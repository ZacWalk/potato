// main.cpp - Minimal portable application entry. Builds a basic browser
// frame: a toolbar with back/forward/refresh/home buttons + an address
// bar across the top, and a blank content area below. The full document
// rendering pipeline will be wired in subsequent passes.

#include "pch.h"
#include "platform.h"
#include "resource.h"
#include "document.h"
#include "element.h"
#include "style.h"

namespace
{
	// ── Content area ──────────────────────────────────────────────────────
	// Reactor for the blank content child window. Paints a white background
	// and centres a placeholder label.
	class content_reactor final : public pf::frame_reactor, public view_host
	{
		pf::window_frame_ptr _frame;
		std::shared_ptr<document> _doc;
		std::function<void(const std::string&)> _on_open;
		std::function<void()> _on_focus_address;
		int _last_layout_width = 0;

		// Scrolling state.
		int _scroll_y = 0; // current scroll offset (document pixels)
		int _content_height = 0; // total laid-out document height
		int _viewport_h = 0; // last-known viewport height
		bool _dragging_thumb = false;
		int _drag_offset = 0; // mouse-y minus thumb-top at drag start

		static constexpr int k_scrollbar_w = 14;
		static constexpr int k_wheel_step = 60;

	public:
		void set_frame(pf::window_frame_ptr f) { _frame = std::move(f); }

		void set_on_open(std::function<void(const std::string&)> f)
		{
			_on_open = std::move(f);
		}

		void set_on_focus_address(std::function<void()> f)
		{
			_on_focus_address = std::move(f);
		}

		void load_html(const std::string& url, const std::string& html)
		{
			_last_layout_width = 0;
			_scroll_y = 0;
			_content_height = 0;
			_doc.reset();
			if (!html.empty())
				_doc = document::create_from_utf8(*this, url, html);
			if (_frame) _frame->invalidate();
		}

		// ── view_host ──
		void layout() override
		{
			_last_layout_width = 0;
			if (_frame) _frame->invalidate();
		}

		void invalidate() override
		{
			if (_frame) _frame->invalidate();
		}

		void open(const std::string& url) override
		{
			if (_on_open) _on_open(url);
		}

	private:
		// Width of the document layout area (viewport minus scrollbar gutter
		// when one is needed). Pass 0 if unknown.
		int doc_width(int viewport_w) const
		{
			return viewport_w > k_scrollbar_w ? viewport_w - k_scrollbar_w : viewport_w;
		}

		bool needs_scrollbar() const
		{
			return _content_height > _viewport_h && _viewport_h > 0;
		}

		pf::irect scrollbar_track_rect(const pf::irect& client) const
		{
			return pf::irect(client.right - k_scrollbar_w, client.top,
			                 client.right, client.bottom);
		}

		pf::irect scrollbar_thumb_rect(const pf::irect& client) const
		{
			const auto track = scrollbar_track_rect(client);
			if (_content_height <= 0 || _viewport_h <= 0) return track;
			const int track_h = track.height();
			int thumb_h = std::max(20, mul_div(track_h, _viewport_h, _content_height));
			thumb_h = std::min(thumb_h, track_h);
			const int max_scroll = std::max(0, _content_height - _viewport_h);
			const int thumb_y = max_scroll == 0
				                    ? track.top
				                    : track.top + mul_div(track_h - thumb_h, _scroll_y, max_scroll);
			return pf::irect(track.left, thumb_y, track.right, thumb_y + thumb_h);
		}

		static int mul_div(int a, int b, int c)
		{
			if (c == 0) return 0;
			return static_cast<int>(static_cast<int64_t>(a) * b / c);
		}

		void clamp_scroll()
		{
			const int max_scroll = std::max(0, _content_height - _viewport_h);
			if (_scroll_y < 0) _scroll_y = 0;
			if (_scroll_y > max_scroll) _scroll_y = max_scroll;
		}

		void scroll_to(int y)
		{
			const int old = _scroll_y;
			_scroll_y = y;
			clamp_scroll();
			if (_scroll_y != old && _frame) _frame->invalidate();
		}

	public:
		// ── pf::frame_reactor ──
		uint32_t handle_message(pf::window_frame_ptr, pf::message_type m,
		                        uintptr_t, intptr_t) override
		{
			if (m == pf::message_type::erase_background) return 1;
			return 0;
		}

		uint32_t handle_mouse(pf::window_frame_ptr, pf::mouse_message_type m,
		                      const pf::mouse_params& p) override
		{
			if (!_frame) return 0;

			const auto client = _frame->get_client_rect();
			const bool has_sb = needs_scrollbar();
			const auto thumb = scrollbar_thumb_rect(client);
			const auto track = scrollbar_track_rect(client);

			// ── Scrollbar interaction (takes precedence over the document) ──
			if (m == pf::mouse_message_type::mouse_wheel)
			{
				if (has_sb)
				{
					// wheel_delta is multiples of 120; positive = up.
					scroll_to(_scroll_y - (p.wheel_delta * k_wheel_step) / 120);
					return 1;
				}
				return 0;
			}

			if (_dragging_thumb)
			{
				if (m == pf::mouse_message_type::mouse_move && has_sb)
				{
					const int track_h = track.height();
					const int thumb_h = thumb.height();
					const int max_scroll = std::max(0, _content_height - _viewport_h);
					const int rel = p.point.y - track.top - _drag_offset;
					const int span = std::max(1, track_h - thumb_h);
					scroll_to(mul_div(rel, max_scroll, span));
					return 1;
				}
				if (m == pf::mouse_message_type::left_button_up)
				{
					_dragging_thumb = false;
					_frame->release_capture();
					return 1;
				}
				if (m == pf::mouse_message_type::set_cursor)
				{
					_frame->set_cursor_shape(pf::cursor_shape::arrow);
					return 1;
				}
			}

			if (m == pf::mouse_message_type::left_button_down && has_sb &&
				track.contains(p.point))
			{
				if (thumb.contains(p.point))
				{
					_dragging_thumb = true;
					_drag_offset = p.point.y - thumb.top;
					_frame->set_capture();
				}
				else
				{
					// Page up / page down on track click.
					const int delta = std::max(40, _viewport_h - 20);
					if (p.point.y < thumb.top) scroll_to(_scroll_y - delta);
					else scroll_to(_scroll_y + delta);
				}
				return 1;
			}

			if (m == pf::mouse_message_type::set_cursor && has_sb &&
				track.contains(p.point))
			{
				_frame->set_cursor_shape(pf::cursor_shape::arrow);
				return 1;
			}

			if (!_doc) return 0;

			// Map a CSS cursor name to a platform cursor shape.
			auto map_css_cursor = [](const std::string& css) -> pf::cursor_shape
			{
				if (css == "pointer") return pf::cursor_shape::hand;
				if (css == "text") return pf::cursor_shape::ibeam;
				if (css == "wait" || css == "progress") return pf::cursor_shape::wait;
				if (css == "ew-resize" || css == "col-resize" ||
					css == "w-resize" || css == "e-resize")
					return pf::cursor_shape::size_we;
				if (css == "ns-resize" || css == "row-resize" ||
					css == "n-resize" || css == "s-resize")
					return pf::cursor_shape::size_ns;
				return pf::cursor_shape::arrow;
			};

			// Win32 sends WM_SETCURSOR before WM_MOUSEMOVE; consume it so the
			// class-default arrow cursor isn't restored after we set ours.
			if (m == pf::mouse_message_type::set_cursor)
			{
				_frame->set_cursor_shape(map_css_cursor(_doc->cursor()));
				return 1;
			}

			position::vector redraw_boxes;
			bool changed = false;
			const int x = p.point.x;
			const int y = p.point.y + _scroll_y; // doc coords
			const int cx = p.point.x;
			const int cy = p.point.y; // viewport-relative

			switch (m)
			{
			case pf::mouse_message_type::mouse_move:
				changed = _doc->on_mouse_over(x, y, cx, cy, redraw_boxes);
				_frame->set_cursor_shape(map_css_cursor(_doc->cursor()));
				_frame->track_mouse_leave();
				break;
			case pf::mouse_message_type::left_button_down:
				_frame->set_focus();
				changed = _doc->on_lbutton_down(x, y, cx, cy, redraw_boxes);
				break;
			case pf::mouse_message_type::left_button_up:
				changed = _doc->on_lbutton_up(x, y, cx, cy, redraw_boxes);
				break;
			case pf::mouse_message_type::mouse_leave:
				changed = _doc->on_mouse_leave(redraw_boxes);
				_frame->set_cursor_shape(pf::cursor_shape::arrow);
				break;
			default:
				break;
			}

			if (changed)
			{
				if (redraw_boxes.empty())
				{
					_frame->invalidate();
				}
				else
				{
					for (const auto& b : redraw_boxes)
						_frame->invalidate_rect(pf::irect(b.x, b.y - _scroll_y,
						                                  b.x + b.width, b.y - _scroll_y + b.height));
				}
			}
			return 0;
		}

		uint32_t handle_keyboard(pf::window_frame_ptr, pf::keyboard_message_type t,
		                         const pf::keyboard_params& p) override
		{
			if (t == pf::keyboard_message_type::key_down && p.vk == pf::platform_key::F3)
			{
				if (_on_focus_address) _on_focus_address();
				return 1;
			}
			return 0;
		}

		void handle_paint(pf::window_frame_ptr& frame, pf::draw_context& dc) override
		{
			if (!frame) return;
			const auto rc = frame->get_client_rect();
			dc.fill_solid_rect(rc, pf::color_t(255, 255, 255));

			if (!_doc)
			{
				_content_height = 0;
				_viewport_h = rc.height();
				return;
			}

			// Layout against the visible content width (excluding the scroll-
			// bar gutter once one is needed). We do an initial layout at the
			// full width, then re-layout if a scrollbar is required.
			_viewport_h = rc.height();
			int layout_w = rc.width();

			const position client_pos(0, 0, layout_w, _viewport_h);
			_doc->client_pos(client_pos);

			render_win32 renderer(dc, client_pos);

			if (_last_layout_width != layout_w)
			{
				_doc->render(renderer, layout_w);
				_content_height = _doc->height();

				if (_content_height > _viewport_h)
				{
					layout_w = doc_width(rc.width());
					const position cp2(0, 0, layout_w, _viewport_h);
					_doc->client_pos(cp2);
					_doc->render(renderer, layout_w);
					_content_height = _doc->height();
				}

				_last_layout_width = layout_w;
				clamp_scroll();
			}

			// Draw document translated by -scroll_y; clip to visible area.
			const position clip(0, _scroll_y, layout_w, _viewport_h);
			_doc->draw(renderer, 0, -_scroll_y, &clip);

			// Scrollbar overlay.
			if (needs_scrollbar())
			{
				const auto track = scrollbar_track_rect(rc);
				const auto thumb = scrollbar_thumb_rect(rc);
				dc.fill_solid_rect(track, pf::color_t(240, 240, 240));
				dc.fill_solid_rect(thumb, pf::color_t(180, 180, 180));
			}
		}

		void handle_size(pf::window_frame_ptr&, pf::isize, pf::measure_context&) override
		{
			_last_layout_width = 0;
		}
	};

	// ── Main frame ────────────────────────────────────────────────────────
	// Owns the toolbar (address bar + nav buttons) and the content child.
	// Lays them out vertically on every WM_SIZE.
	class main_frame_reactor final : public pf::frame_reactor,
	                                 public std::enable_shared_from_this<main_frame_reactor>
	{
		pf::toolbar_frame_ptr _toolbar;
		pf::window_frame_ptr _content;
		std::shared_ptr<content_reactor> _content_reactor;

		// Async HTTP session and the URL of the most recently loaded page.
		pf::async_http_session_ptr _http;
		pf::async_http_request_ptr _pending;
		std::string _current_url;
		uint64_t _load_token = 0;

		// Simple in-memory navigation history.
		std::vector<std::string> _history;
		size_t _history_pos = 0;

		bool can_go_back() const { return _history_pos > 0; }
		bool can_go_forward() const { return _history_pos + 1 < _history.size(); }

		void update_buttons() const
		{
			if (!_toolbar) return;
			_toolbar->set_button_enabled(1, can_go_back());
			_toolbar->set_button_enabled(2, can_go_forward());
		}

		void focus_address()
		{
			if (!_toolbar) return;
			_toolbar->focus_address();
			_toolbar->select_all_address();
		}

		std::vector<std::string> suggest(std::string_view text) const
		{
			// Common bookmarks shown by default (no text) and folded into
			// the filtered results when the user starts typing.
			static const std::vector<std::string> bookmarks = {
				"https://www.google.com/",
				"https://en.wikipedia.org/wiki/Main_Page",
				"https://www.bbc.com/news",
				"https://news.ycombinator.com/",
				"https://github.com/",
				"https://stackoverflow.com/",
				"https://www.reddit.com/",
				"https://diffractor.com/",
			};

			constexpr size_t max_results = 8;
			std::vector<std::string> result;
			std::set<std::string, ltstr> seen;

			const auto add = [&](const std::string& s)
			{
				if (result.size() >= max_results) return;
				if (seen.insert(s).second) result.push_back(s);
			};

			if (text.empty())
			{
				// First focus / empty edit: show recent history first, then
				// fill with bookmarks.
				for (auto it = _history.rbegin(); it != _history.rend(); ++it) add(*it);
				for (const auto& b : bookmarks) add(b);
				return result;
			}

			std::string needle(text);
			for (auto& c : needle) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

			const auto matches = [&](const std::string& candidate)
			{
				std::string hay = candidate;
				for (auto& c : hay) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				return hay.find(needle) != std::string::npos;
			};

			for (auto it = _history.rbegin(); it != _history.rend(); ++it)
				if (matches(*it)) add(*it);
			for (const auto& b : bookmarks)
				if (matches(b)) add(b);

			return result;
		}

		void navigate(const std::string& url)
		{
			// Resolve relative URLs against the currently loaded page.
			std::string target = url;
			if (!_current_url.empty())
				target = pf::resolve_url(_current_url, url);

			// Truncate forward history when navigating from the middle.
			if (_history_pos + 1 < _history.size())
				_history.resize(_history_pos + 1);

			if (_history.empty() || _history.back() != target)
			{
				_history.push_back(target);
				_history_pos = _history.size() - 1;
			}

			if (_toolbar) _toolbar->set_address_text(target);
			load(target);
			update_buttons();
		}

		void load(const std::string& url)
		{
			if (!_content_reactor) return;

			// Cancel any in-flight request for the previous navigation.
			if (_pending)
			{
				_pending->cancel();
				_pending.reset();
			}
			++_load_token;

			if (url == "res://test.htm" || url == "about:blank")
			{
				_current_url = url;
				_content_reactor->load_html(url, pf::platform_load_text_resource(IDR_HTML_TEST));
				return;
			}

			if (url.starts_with("http://") || url.starts_with("https://"))
			{
				if (!_http)
					_http = pf::create_async_http_session("Potato/1.0");

				const uint64_t token = _load_token;
				std::weak_ptr<main_frame_reactor> weak_self = shared_from_this();
				auto body = std::make_shared<std::string>();

				pf::async_http_callbacks cb;
				cb.on_data = [body](const uint8_t* data, size_t size)
				{
					body->append(reinterpret_cast<const char*>(data), size);
				};
				cb.on_complete = [weak_self, body, url, token]
				{
					pf::run_ui([weak_self, body, url, token]
					{
						if (auto self = weak_self.lock())
							self->on_download_complete(token, url, *body, {});
					});
				};
				cb.on_error = [weak_self, url, token](std::string err)
				{
					pf::run_ui([weak_self, url, token, err = std::move(err)]
					{
						if (auto self = weak_self.lock())
							self->on_download_complete(token, url, {}, err);
					});
				};

				_pending = _http->get(url, std::move(cb));
			}

			// Unknown scheme: keep the current page rather than blanking it.
		}

		void on_download_complete(uint64_t token, const std::string& url,
		                          const std::string& body, const std::string& error)
		{
			// Stale response (newer navigation issued).
			if (token != _load_token) return;
			_pending.reset();

			if (!_content_reactor) return;

			if (!error.empty() || body.empty())
			{
				const std::string msg = error.empty() ? std::string("(empty response)") : error;
				const std::string html =
					"<html><body><h2>Failed to load</h2><p>" + url + "</p><pre>" + msg + "</pre></body></html>";
				_current_url = url;
				_content_reactor->load_html(url, html);
				return;
			}

			_current_url = url;
			_content_reactor->load_html(url, body);
		}

		void go_back()
		{
			if (!can_go_back()) return;
			--_history_pos;
			if (_toolbar) _toolbar->set_address_text(_history[_history_pos]);
			load(_history[_history_pos]);
			update_buttons();
		}

		void go_forward()
		{
			if (!can_go_forward()) return;
			++_history_pos;
			if (_toolbar) _toolbar->set_address_text(_history[_history_pos]);
			load(_history[_history_pos]);
			update_buttons();
		}

		void initialise(const pf::window_frame_ptr& main_frame)
		{
			if (_toolbar) return; // already built

			// ── Toolbar ──
			pf::address_bar_config cfg;
			cfg.style = pf::toolbar_style::address_bar;
			cfg.initial_text = "about:blank";

			cfg.left_buttons.push_back({
				pf::icon_glyph::back, 1, "Back",
				[this] { go_back(); },
				[this] { return can_go_back(); }
			});
			cfg.left_buttons.push_back({
				pf::icon_glyph::forward, 2, "Forward",
				[this] { go_forward(); },
				[this] { return can_go_forward(); }
			});
			cfg.left_buttons.push_back({
				pf::icon_glyph::refresh, 3, "Refresh",
				[this]
				{
					if (_toolbar)
						navigate(_toolbar->address_text());
				},
				{}
			});
			cfg.left_buttons.push_back({
				pf::icon_glyph::home, 4, "Home",
				[this] { navigate("about:blank"); },
				{}
			});

			cfg.right_buttons.push_back({
				pf::icon_glyph::menu, 5, "Menu", {}, {}
			});

			cfg.on_navigate = [this](std::string url) { navigate(url); };
			cfg.on_suggest = [this](std::string_view text) { return suggest(text); };

			_toolbar = main_frame->create_address_bar(cfg);

			// Attach a hamburger menu to the right-hand "menu" button.
			// Browser-utility items only (no curated bookmarks).
			std::vector<pf::menu_command> menu_items;
			menu_items.emplace_back("Back", 0, [this] { go_back(); });
			menu_items.emplace_back("Forward", 0, [this] { go_forward(); });
			menu_items.emplace_back("Reload", 0, [this]
			{
				if (_toolbar) navigate(_toolbar->address_text());
			});
			menu_items.emplace_back(); // separator (empty text)
			menu_items.emplace_back("Home", 0, [this] { navigate("about:blank"); });
			menu_items.emplace_back("Test page", 0, [this] { navigate("res://test.htm"); });
			menu_items.emplace_back(); // separator
			menu_items.emplace_back("Exit", 0, [main_frame] { main_frame->close(); });
			_toolbar->set_menu(5, std::move(menu_items));

			// ── Content ──
			_content = main_frame->create_child("PotatoContent",
			                                    pf::window_style::child |
			                                    pf::window_style::visible |
			                                    pf::window_style::clip_children,
			                                    pf::color_t(255, 255, 255));
			_content_reactor = std::make_shared<content_reactor>();
			_content_reactor->set_frame(_content);
			_content_reactor->set_on_open([this](const std::string& url) { navigate(url); });
			_content_reactor->set_on_focus_address([this] { focus_address(); });
			_content->set_reactor(_content_reactor);

			// Load the embedded sample page.
			navigate("res://test.htm");

			update_buttons();
		}

	public:
		// Called from app_init — stash the frame so we can build children
		// once it actually has an HWND.
		void attach(pf::window_frame_ptr main_frame) { _main = std::move(main_frame); }

		// ── pf::frame_reactor ─────────────────────────────────────────────
		uint32_t handle_message(pf::window_frame_ptr frame, pf::message_type m,
		                        uintptr_t, intptr_t) override
		{
			if (m == pf::message_type::create)
			{
				// HWND now exists — build the toolbar and content child.
				initialise(frame ? frame : _main);
				return 0;
			}
			if (m == pf::message_type::close)
			{
				if (frame) frame->close();
				return 0;
			}
			if (m == pf::message_type::erase_background) return 1;
			return 0;
		}

	private:
		pf::window_frame_ptr _main;

		uint32_t handle_mouse(pf::window_frame_ptr, pf::mouse_message_type,
		                      const pf::mouse_params&) override { return 0; }

		uint32_t handle_keyboard(pf::window_frame_ptr, pf::keyboard_message_type t,
		                         const pf::keyboard_params& p) override
		{
			if (t == pf::keyboard_message_type::key_down && p.vk == pf::platform_key::F3)
			{
				focus_address();
				return 1;
			}
			return 0;
		}

		void handle_paint(pf::window_frame_ptr& frame, pf::draw_context& dc) override
		{
			// The toolbar and content children paint themselves; just clear
			// any uncovered area as a backdrop.
			if (frame)
				dc.fill_solid_rect(frame->get_client_rect(), pf::color_t(240, 240, 240));
		}

		void handle_size(pf::window_frame_ptr& frame, pf::isize extent,
		                 pf::measure_context&) override
		{
			if (!frame) return;
			const int toolbar_h = _toolbar ? _toolbar->preferred_height() : 0;

			if (_toolbar && _toolbar->frame())
				_toolbar->frame()->move_window(pf::irect(0, 0, extent.cx, toolbar_h));

			if (_content)
				_content->move_window(pf::irect(0, toolbar_h, extent.cx, extent.cy));
		}
	};
}

// ── App entry points ──────────────────────────────────────────────────────

// Defined in core.cpp — runs the registered in-process unit tests and
// returns an HTML report. Failed cases contain the substring "FAILED".
extern std::string run_tests();

namespace
{
	// Combined self-test:
	//   1. Runs the in-process unit tests (run_tests() from core.cpp) and
	//      writes the HTML report to a temp file.
	//   2. Synchronously fetches https://www.google.com and verifies a
	//      successful HTTP response with non-empty body.
	// Prints progress to stdout. Returns 0 on success, non-zero on failure.
	int run_self_test()
	{
		int result = 0;

		// 1. Unit tests
		pf::write_stdout("Potato self-test: running unit tests ...\n");
		const auto report = run_tests();
		const auto report_path = pf::platform_temp_file_path("potato_tests_");
		if (const auto out = pf::open_file_for_write(pf::file_path{report_path}))
		{
			out->write(reinterpret_cast<const uint8_t*>(report.data()),
			           static_cast<uint32_t>(report.size()));
		}
		const bool unit_failed = report.find("FAILED") != std::string::npos;
		pf::write_stdout(std::format(
			"Unit tests: {} (report: {})\n",
			unit_failed ? "FAIL" : "PASS", report_path));
		if (unit_failed) result = 10;

		// 2. Network fetch — exercise the same async HTTP path the browser
		//    uses to load pages, stylesheets and images. We block the calling
		//    thread on a condition variable; callbacks fire on a background
		//    thread (see win_async_http_request::s_callback) so this is safe
		//    without a UI message pump.
		const auto fetch = [](const std::string_view url) -> int
		{
			pf::write_stdout(std::format("Potato self-test: fetching {} ...\n", url));

			const auto session = pf::create_async_http_session("Potato/1.0-test");
			if (!session)
			{
				pf::write_stdout("FAIL: create_async_http_session returned null\n");
				return 1;
			}

			std::mutex mtx;
			std::condition_variable cv;
			bool done = false;
			int status_code = 0;
			std::string content_type;
			std::string error;
			size_t body_bytes = 0;
			std::string body_preview;

			pf::async_http_callbacks cb;
			cb.on_headers = [&](int sc, std::string ct, uint64_t /*len*/)
			{
				std::lock_guard lk(mtx);
				status_code = sc;
				content_type = std::move(ct);
			};
			cb.on_data = [&](const uint8_t* data, size_t size)
			{
				std::lock_guard lk(mtx);
				body_bytes += size;
				if (body_preview.size() < 256)
				{
					const auto take = std::min<size_t>(size, 256 - body_preview.size());
					body_preview.append(reinterpret_cast<const char*>(data), take);
				}
			};
			cb.on_complete = [&]()
			{
				std::lock_guard lk(mtx);
				done = true;
				cv.notify_all();
			};
			cb.on_error = [&](std::string e)
			{
				std::lock_guard lk(mtx);
				error = std::move(e);
				done = true;
				cv.notify_all();
			};

			const auto req = session->get(url, cb);
			if (!req)
			{
				pf::write_stdout("FAIL: session->get returned null\n");
				return 2;
			}

			std::unique_lock lk(mtx);
			if (!cv.wait_for(lk, std::chrono::seconds(30), [&] { return done; }))
			{
				pf::write_stdout("FAIL: request timed out after 30 seconds\n");
				req->cancel();
				return 3;
			}

			pf::write_stdout(std::format(
				"  HTTP status: {}, content-type: {}, body: {} bytes\n",
				status_code, content_type, body_bytes));

			if (!body_preview.empty())
			{
				// Replace control chars for stdout safety; show full preview.
				std::string preview;
				preview.reserve(body_preview.size());
				for (const auto c : body_preview)
					preview.push_back((c == '\r' || c == '\n' || c == '\t') ? ' ' : c);
				pf::write_stdout(std::format("  body preview: {}\n", preview));
			}

			if (!error.empty())
			{
				pf::write_stdout(std::format("FAIL: {}\n", error));
				return 4;
			}
			if (status_code < 200 || status_code >= 400)
			{
				pf::write_stdout("FAIL: unexpected HTTP status code\n");
				return 5;
			}
			if (body_bytes == 0)
			{
				pf::write_stdout("FAIL: empty response body\n");
				return 6;
			}

			pf::write_stdout("  PASS\n");
			return 0;
		};

		for (const auto url : {
			     "http://example.com/",
			     "https://example.com/",
			     "https://www.google.com/",
			     "https://en.wikipedia.org/wiki/Main_Page",
		     })
		{
			const auto rc = fetch(url);
			if (rc != 0 && result == 0) result = rc;
		}

		return result;
	}
}

app_init_result app_init(const pf::window_frame_ptr& main_frame,
                         std::span<const std::string_view> params)
{
	app_init_result r;
	r.start_gui = true;
	r.exit_code = 0;

	for (const auto& p : params)
	{
		if (p == "/test" || p == "--test" || p == "-test")
		{
			r.start_gui = false;
			r.exit_code = run_self_test();
			return r;
		}
	}

	if (main_frame)
	{
		main_frame->set_text("Potato");
		const auto reactor = std::make_shared<main_frame_reactor>();
		reactor->attach(main_frame);
		main_frame->set_reactor(reactor);
	}

	return r;
}

void app_idle()
{
}

void app_destroy()
{
}

// Portable UI/async dispatch entry points declared in core.h. Forward to the
// platform layer's task queues.
void dispatch_to_ui(std::function<void()> fn)
{
	pf::run_ui(std::move(fn));
}

void dispatch_async(std::function<void()> fn)
{
	pf::run_async(std::move(fn));
}
