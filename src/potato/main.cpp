// main.cpp - The potato browser application. The frame is a libdesktop UI
// application: drawn controls form the chrome and one element presents the
// platform-free libwebvis document below them.

#include "pch.h"
#include "webvis_host.h"
#include "document.h"
#include "style.h"

#include "libdesktop/render/software.h"
#include "libdesktop/ui/controls.h"
#include "libdesktop/ui/elements.h"
#include "libdesktop/ui/menu_bar.h"

#include <condition_variable>
#include <limits>

// The engine lives in namespace webvis; the browser is its only consumer here,
// so it is pulled in wholesale rather than qualified at hundreds of sites.
using namespace webvis;

enum class icon_index
{
	none = 0,
	back = 0xE72B,
	forward = 0xE72A,
	refresh = 0xE72C,
	home = 0xE80F,
	menu = 0xE700,
	check = 0xE73E,
	checkbox_off = 0xE739,
	checkbox_on = 0xE73A,
	radio_off = 0xECCA,
	radio_on = 0xECCB,
	close = 0xE74D,
	maximize = 0xE922,
	minimize = 0xE921,
	restore = 0xE923,
};

enum class command_group
{
	none = 0,
};

enum class commands
{
	none = 0,
	open,
	exit,
	back,
	forward,
	reload,
	home,
	test_page,
};

namespace
{
	// The one host instance, handed to libwebvis before any document exists.
	potato_host g_host;

	struct browser_options
	{
		bool console = false;
		bool network_test = true;
		std::string layout_path;
		int layout_width = 1902;
		int layout_repeats = 1;
		bool layout_verbose = false;
		int layout_dump = 0;
		bool layout_dump_json = false;
		std::string eval_url;
		std::string startup_url;
	};

	std::vector<std::string> command_line_tokens(const std::string_view command_line)
	{
		std::vector<std::string> result;
		for (const auto token : core::split(command_line, true))
			result.emplace_back(core::unquote(token));
		return result;
	}

	std::string read_file_string(const core::file_path& path)
	{
		auto data = platform::blob_from_file(path);
		if (data.empty()) return {};
		return {reinterpret_cast<const char*>(data.data()), data.size()};
	}

	// "caf%C3%A9" -> "café". Links percent-encode a fragment, but ids and names
	// are written out literally.
	std::string percent_decode(const std::string_view text)
	{
		const auto hex = [](const char c) -> int
		{
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			return -1;
		};

		std::string result;
		result.reserve(text.size());
		for (size_t i = 0; i < text.size(); ++i)
		{
			if (text[i] == '%' && i + 2 < text.size())
			{
				const int hi = hex(text[i + 1]);
				const int lo = hex(text[i + 2]);
				if (hi >= 0 && lo >= 0)
				{
					result.push_back(static_cast<char>(hi * 16 + lo));
					i += 2;
					continue;
				}
			}
			result.push_back(text[i]);
		}
		return result;
	}

	// Splits "page#name" into the document and the fragment that points into it.
	std::pair<std::string, std::string> split_fragment(const std::string& url)
	{
		const auto hash = url.find('#');
		if (hash == std::string::npos) return {url, {}};
		return {url.substr(0, hash), url.substr(hash + 1)};
	}

	bool is_web_url(const std::string_view url)
	{
		return (url.size() >= 7 && nicmp(url.data(), "http://", 7) == 0) ||
			(url.size() >= 8 && nicmp(url.data(), "https://", 8) == 0);
	}

	// True when two URLs name the same document, whatever their fragments. A
	// local path is compared as a path: its separators and case depend on how
	// it was typed or resolved.
	bool same_document(std::string_view a, std::string_view b)
	{
		a = a.substr(0, a.find('#'));
		b = b.substr(0, b.find('#'));
		if (is_web_url(a) || is_web_url(b)) return a == b;
		if (a.size() != b.size()) return false;

		const auto fold = [](const char c)
		{
			return c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		};
		for (size_t i = 0; i < a.size(); ++i)
			if (fold(a[i]) != fold(b[i])) return false;
		return true;
	}

	// RFC 3986 scheme followed by ':'. "c:" is a drive letter and "host:8080" a
	// port, neither a scheme.
	bool has_scheme(const std::string_view text)
	{
		const auto colon = text.find(':');
		if (colon == std::string_view::npos || colon < 2) return false;
		if (!std::isalpha(static_cast<unsigned char>(text[0]))) return false;

		for (size_t i = 1; i < colon; ++i)
		{
			const auto c = static_cast<unsigned char>(text[i]);
			if (!std::isalnum(c) && c != '+' && c != '-' && c != '.') return false;
		}

		const auto rest = text.substr(colon + 1);
		const auto port = rest.substr(0, rest.find('/'));
		const bool is_port = !port.empty() &&
			std::all_of(port.begin(), port.end(), [](const char c) { return c >= '0' && c <= '9'; });
		return !is_port;
	}

	std::string html_escape(const std::string_view text)
	{
		std::string result;
		result.reserve(text.size());
		for (const auto c : text)
		{
			switch (c)
			{
			case '&': result += "&amp;"; break;
			case '<': result += "&lt;"; break;
			case '>': result += "&gt;"; break;
			case '"': result += "&quot;"; break;
			default: result.push_back(c); break;
			}
		}
		return result;
	}

	// "text/plain; charset=utf-8" names plain text, whatever its parameters.
	bool is_plain_text(const std::string_view content_type)
	{
		auto media = content_type.substr(0, content_type.find(';'));
		while (!media.empty() && media.back() == ' ') media.remove_suffix(1);
		while (!media.empty() && media.front() == ' ') media.remove_prefix(1);
		return is_equal(media, "text/plain");
	}

	// A text file is shown as it is: its markup characters are escaped rather
	// than parsed, and its line breaks kept.
	std::string plain_text_page(const std::string_view bytes, const std::string_view content_type)
	{
		return "<html><body><pre style=\"white-space:pre-wrap\">" +
			html_escape(decode_to_utf8(bytes, content_type)) + "</pre></body></html>";
	}

	ui::command_ptr make_command(std::string_view text, std::function<void()> invoke = {})
	{
		auto result = std::make_shared<ui::command>();
		result->text = text;
		result->toolbar_text = text;
		result->tooltip_text = text;
		result->invoke = std::move(invoke);
		return result;
	}
	// ── Page element ──────────────────────────────────────────────────────
	// The document remains a normal libwebvis view_host. It is an element only
	// for frame layout and presentation; its input is handled by the frame host.
	class page_element final : public ui::element, public view_host
	{
		std::shared_ptr<document> _doc;
		std::function<void(const std::string&)> _on_open;
		std::function<void()> _request_invalidate;
		std::function<void(platform::cursor)> _set_cursor;
		std::function<void(const std::string&)> _on_diagnostic;
		std::function<void(const std::string&, const std::string&)> _on_resource_started;
		std::function<void(const std::string&, const std::string&, bool)> _on_resource_finished;

		// width the document was laid out at; 0 forces a new layout
		int _last_layout_width = 0;
		// viewport width that layout was made for
		int _last_viewport_w = 0;
		int _layout_pass = 0;
		// Fragment of the URL on screen. It is re-applied after every layout, so
		// a late stylesheet or image cannot push its target out of view, and is
		// dropped as soon as the reader scrolls for themselves.
		std::string _anchor;
		// Scrolling state.
		// current scroll offset (document pixels)
		int _scroll_y = 0;
		// total laid-out document height
		int _content_height = 0;
		// last-known viewport height
		int _viewport_h = 0;
		bool _dragging_thumb = false;
		// mouse-y minus thumb-top at drag start
		int _drag_offset = 0;

		static constexpr int k_scrollbar_w = 14;
		static constexpr int k_wheel_step = 60;
		static constexpr int k_line_step = 40;

		void invalidate_now() const
		{
			if (_request_invalidate) _request_invalidate();
		}

		// Width of the document layout area (viewport minus scrollbar gutter
		// when one is needed). Pass 0 if unknown.
		int doc_width(const int viewport_w) const
		{
			return viewport_w > k_scrollbar_w ? viewport_w - k_scrollbar_w : viewport_w;
		}

		bool needs_scrollbar() const
		{
			return _content_height > _viewport_h && _viewport_h > 0;
		}

		core::recti scrollbar_track_rect(const core::recti& client) const
		{
			return {client.right - k_scrollbar_w, client.top, client.right, client.bottom};
		}

		core::recti scrollbar_thumb_rect(const core::recti& client) const
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
			return {track.left, thumb_y, track.right, thumb_y + thumb_h};
		}

		static int mul_div(const int a, const int b, const int c)
		{
			return c == 0 ? 0 : static_cast<int>(static_cast<int64_t>(a) * b / c);
		}

		void clamp_scroll()
		{
			const int max_scroll = std::max(0, _content_height - _viewport_h);
			_scroll_y = std::clamp(_scroll_y, 0, max_scroll);
		}

		void scroll_to(const int y)
		{
			const int old = _scroll_y;
			_scroll_y = y;
			clamp_scroll();
			if (_scroll_y != old) invalidate_now();
		}

		// Scrolling the reader asked for: the fragment stops holding the view.
		void user_scroll_to(const int y)
		{
			_anchor.clear();
			scroll_to(y);
		}

		// Positions are only known once the document has been laid out, so this
		// is a no-op until then and do_layout applies it.
		void apply_anchor()
		{
			if (!_doc || _anchor.empty() || _last_layout_width == 0) return;
			int y = _doc->anchor_top(_anchor);
			if (y < 0)
			{
				// Links percent-encode a fragment; ids are written out literally.
				const auto decoded = percent_decode(_anchor);
				if (decoded != _anchor) y = _doc->anchor_top(decoded);
			}
			if (y < 0 && icmp(_anchor, "top") == 0) y = 0;
			if (y >= 0) scroll_to(y);
		}

		// Map a CSS cursor name to a platform cursor shape.
		static platform::cursor map_cursor(const std::string& css)
		{
			if (css == "pointer") return platform::cursor::link;
			if (css == "text") return platform::cursor::text_select;
			if (css == "ew-resize" || css == "col-resize" || css == "w-resize" || css == "e-resize")
				return platform::cursor::left_right;
			if (css == "ns-resize" || css == "row-resize" || css == "n-resize" || css == "s-resize")
				return platform::cursor::up_down;
			return platform::cursor::normal;
		}

		// Layout needs no draw context, so it can run without a window.
		void do_layout(const int avail_w, const int avail_h)
		{
			if (!_doc) return;
			_viewport_h = avail_h;
			// A page taller than the viewport is laid out narrower than it, to
			// leave room for the scrollbar, so the layout is cached against the
			// viewport width it was made for rather than its own width; keyed on
			// its own width, every paint laid the whole document out twice.
			if (_last_layout_width != 0 && _last_viewport_w == avail_w) return;

			int layout_w = avail_w;
			_doc->client_pos(position(0, 0, layout_w, _viewport_h));
			_doc->render(layout_w);
			_content_height = _doc->height();

			if (_content_height > _viewport_h)
			{
				layout_w = doc_width(avail_w);
				_doc->client_pos(position(0, 0, layout_w, _viewport_h));
				_doc->render(layout_w);
				_content_height = _doc->height();
			}

			_last_layout_width = layout_w;
			_last_viewport_w = avail_w;
			clamp_scroll();
			++_layout_pass;
			diagnostic(std::format("Layout {}: viewport={}x{}, document={}x{}", _layout_pass, avail_w,
			                       _viewport_h, _doc->width(), _doc->height()));
			_doc->diagnose_layout();
			apply_anchor();
		}

		void report_redraw(const position::vector& boxes)
		{
			if (!boxes.empty() || _doc) invalidate_now();
		}

	public:
		page_element() : ui::element(ui::flex_item::grow | ui::flex_item::stretch)
		{
			padding(0);
		}

		void set_callbacks(std::function<void()> invalidate_callback,
		                  std::function<void(platform::cursor)> cursor_callback)
		{
			_request_invalidate = std::move(invalidate_callback);
			_set_cursor = std::move(cursor_callback);
		}

		void set_on_open(std::function<void(const std::string&)> callback) { _on_open = std::move(callback); }
		void set_on_diagnostic(std::function<void(const std::string&)> callback) { _on_diagnostic = std::move(callback); }
		void set_on_resource_started(std::function<void(const std::string&, const std::string&)> callback)
		{
			_on_resource_started = std::move(callback);
		}
		void set_on_resource_finished(std::function<void(const std::string&, const std::string&, bool)> callback)
		{
			_on_resource_finished = std::move(callback);
		}

		void load_html(const std::string& url, const std::string& html, const std::string& content_type = {},
		               const std::string& anchor = {})
		{
			_last_layout_width = 0;
			_scroll_y = 0;
			_content_height = 0;
			_anchor = anchor;
			_doc.reset();
			if (!html.empty()) _doc = document::create_from_bytes(*this, url, html, content_type);
			invalidate_now();
		}

		bool has_document() const { return _doc != nullptr; }

		void scroll_to_anchor(const std::string& anchor)
		{
			_anchor = anchor;
			if (_anchor.empty()) scroll_to(0);
			else apply_anchor();
		}

		bool wheel_at(const core::pointi loc, const int delta)
		{
			if (!_doc || !bounds.contains(loc)) return false;
			// delta is multiples of 120; positive = up.
			user_scroll_to(_scroll_y - (delta * k_wheel_step) / 120);
			return true;
		}

		bool key_down(const char32_t key, const platform::key_state keys)
		{
			// The page takes the focus when clicked, so it answers the usual
			// scrolling keys.
			if (!_doc) return false;
			const int page = std::max(40, _viewport_h - 40);
			const bool shift = keys.shift;
			if (key == platform::keys::UP) { user_scroll_to(_scroll_y - k_line_step); return true; }
			if (key == platform::keys::DOWN) { user_scroll_to(_scroll_y + k_line_step); return true; }
			if (key == platform::keys::PRIOR) { user_scroll_to(_scroll_y - page); return true; }
			if (key == platform::keys::NEXT) { user_scroll_to(_scroll_y + page); return true; }
			if (key == platform::keys::SPACE) { user_scroll_to(_scroll_y + (shift ? -page : page)); return true; }
			if (key == platform::keys::HOME) { user_scroll_to(0); return true; }
			if (key == platform::keys::END) { user_scroll_to(_content_height); return true; }
			return false;
		}

		void pointer_move(const core::pointi loc)
		{
			const auto local = loc - bounds.top_left();
			const auto client = core::recti(bounds.extent());
			const auto track = scrollbar_track_rect(client);
			const bool has_sb = needs_scrollbar();
			if (_dragging_thumb && has_sb)
			{
				const auto thumb = scrollbar_thumb_rect(client);
				const int max_scroll = std::max(0, _content_height - _viewport_h);
				const int span = std::max(1, track.height() - thumb.height());
				user_scroll_to(mul_div(local.y - track.top - _drag_offset, max_scroll, span));
				if (_set_cursor) _set_cursor(platform::cursor::normal);
				return;
			}
			if (has_sb && track.contains(local))
			{
				if (_set_cursor) _set_cursor(platform::cursor::normal);
				return;
			}
			if (!_doc) return;
			position::vector redraw_boxes;
			const bool changed = _doc->on_mouse_over(local.x, local.y + _scroll_y, local.x, local.y, redraw_boxes);
			if (_set_cursor) _set_cursor(map_cursor(_doc->cursor()));
			if (changed) report_redraw(redraw_boxes);
		}

		void pointer_down(const core::pointi loc, const platform::key_state keys)
		{
			const auto local = loc - bounds.top_left();
			const auto client = core::recti(bounds.extent());
			const auto track = scrollbar_track_rect(client);
			const auto thumb = scrollbar_thumb_rect(client);
			if (_doc && needs_scrollbar() && track.contains(local))
			{
				_anchor.clear();
				if (thumb.contains(local))
				{
					_dragging_thumb = true;
					_drag_offset = local.y - thumb.top;
				}
				else
				{
					// Page up / page down on track click.
					const int delta = std::max(40, _viewport_h - 20);
					user_scroll_to(local.y < thumb.top ? _scroll_y - delta : _scroll_y + delta);
				}
				return;
			}
			if (!_doc) return;
			position::vector redraw_boxes;
			if (_doc->on_lbutton_down(local.x, local.y + _scroll_y, local.x, local.y, redraw_boxes))
				report_redraw(redraw_boxes);
			(void)keys;
		}

		void pointer_up(const core::pointi loc, const platform::key_state keys)
		{
			if (_dragging_thumb)
			{
				_dragging_thumb = false;
				return;
			}
			if (!_doc) return;
			const auto local = loc - bounds.top_left();
			position::vector redraw_boxes;
			if (_doc->on_lbutton_up(local.x, local.y + _scroll_y, local.x, local.y, redraw_boxes))
				report_redraw(redraw_boxes);
			(void)keys;
		}

		void pointer_leave()
		{
			_dragging_thumb = false;
			if (_doc)
			{
				position::vector redraw_boxes;
				if (_doc->on_mouse_leave(redraw_boxes)) report_redraw(redraw_boxes);
			}
			if (_set_cursor) _set_cursor(platform::cursor::normal);
		}
		// ── ui::element ──
		core::sizei measure(render::measure_context&, int width_limit) const override
		{
			return {width_limit, 1};
		}

		void layout(render::measure_context&, const core::recti bounds_in) override
		{
			bounds = bounds_in;
			do_layout(bounds.width(), bounds.height());
		}

		void render(render::draw_context& dc, const core::pointi element_offset) const override
		{
			const auto page = bounds.offset(element_offset);
			dc.draw_rect(page, core::color(core::rgb(255, 255, 255)));
			if (!_doc) return;

			auto* self = const_cast<page_element*>(this);
			self->do_layout(page.width(), page.height());
			const int layout_w = self->_last_layout_width;
			const position client_pos(0, 0, layout_w, self->_viewport_h);
			_doc->client_pos(client_pos);
			potato_device_context device(dc, page.top_left());
			renderer renderer(device, client_pos);
			// Draw document translated by -scroll_y. The clip is in the same
			// (translated) space as the positions element::draw tests against.
			const position clip(0, 0, layout_w, self->_viewport_h);
			{
				render::scoped_clip scoped(dc, page);
				_doc->draw(renderer, 0, -self->_scroll_y, &clip);
			}

			// Scrollbar overlay.
			if (needs_scrollbar())
			{
				const auto track = scrollbar_track_rect(page);
				const auto thumb = scrollbar_thumb_rect(page);
				dc.draw_rect(track, core::color(core::rgb(240, 240, 240)));
				dc.draw_rect(thumb, core::color(core::rgb(180, 180, 180)));
			}
		}

		// ── view_host ──
		void layout() override
		{
			_last_layout_width = 0;
			invalidate_now();
		}

		void open(const std::string& url) override
		{
			if (_on_open) _on_open(url);
		}

		void invalidate() override { invalidate_now(); }

		void diagnostic(const std::string& message) override
		{
			if (_on_diagnostic) _on_diagnostic(message);
		}

		void resource_started(const std::string& type, const std::string& url) override
		{
			if (_on_resource_started) _on_resource_started(type, url);
		}

		void resource_finished(const std::string& type, const std::string& url, const bool success) override
		{
			if (_on_resource_finished) _on_resource_finished(type, url, success);
		}
	};

	int run_layout(const std::string& path, int width, int height, int repeats, bool verbose, int dump_depth, bool dump_json);
	int run_self_test(bool network);

	// ── Browser application ──────────────────────────────────────────────
	class potato_app final : public ui::app, public ui::frame_host,
	                         public std::enable_shared_from_this<potato_app>
	{
		browser_options _options;
		platform::setting_file_ptr _settings;
		ui::frame_ptr _frame;
		std::shared_ptr<ui::elements> _root;
		std::shared_ptr<ui::elements> _chrome;
		std::shared_ptr<page_element> _page;
		ui::toolbar_ptr _navigation;
		ui::edit_ptr _address;
		ui::control_ptr _menu_bar;
		std::vector<ui::command_ptr> _menus;
		ui::command_ptr _back_command;
		ui::command_ptr _forward_command;
		ui::command_ptr _refresh_command;
		ui::command_ptr _home_command;
		ui::command_ptr _menu_command;

		// Async HTTP session and the URL of the most recently loaded page.
		platform::async_http_session_ptr _http;
		platform::async_http_request_ptr _pending;
		std::string _current_url;
		uint64_t _load_token = 0;
		// Simple in-memory navigation history.
		std::vector<std::string> _history;
		size_t _history_pos = 0;

		std::string _eval_url;
		bool _eval_page_loaded = false;
		int _eval_pending_resources = 0;
		int _eval_failed_resources = 0;
		std::chrono::steady_clock::time_point _eval_started = std::chrono::steady_clock::now();
		std::chrono::steady_clock::time_point _eval_last_activity = _eval_started;

		bool can_go_back() const { return _history_pos > 0; }
		bool can_go_forward() const { return _history_pos + 1 < _history.size(); }

		void eval_log(const std::string& message)
		{
			if (_eval_url.empty()) return;
			_eval_last_activity = std::chrono::steady_clock::now();
			const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
				_eval_last_activity - _eval_started).count();
			platform::write_stdout(std::format("[{:6} ms] {}\n", elapsed, message));
		}

		void update_buttons()
		{
			if (!_navigation) return;
			_back_command->enable = can_go_back();
			_forward_command->enable = can_go_forward();
			_navigation->update_button_state(false, false);
		}

		void focus_address()
		{
			if (!_address) return;
			_address->auto_completes(suggest(_address->window_text()));
			_address->focus();
			_address->select_all();
		}

		core::file_path absolute_file_path(const std::string_view value) const
		{
			std::string path(value);
			if (path.starts_with("file://")) path = platform::resolve_url({}, path);
			const bool drive_path = path.size() >= 3 && std::isalpha(static_cast<unsigned char>(path[0])) &&
				path[1] == ':' && core::is_path_sep(path[2]);
			const bool rooted_path = !path.empty() && core::is_path_sep(path[0]);
			if (!drive_path && !rooted_path)
			{
				auto base = std::string(platform::current_directory().text());
				if (!base.empty() && !core::is_path_sep(base.back())) base.push_back(core::preferred_path_sep);
				path = base + path;
			}
			return core::file_path(path);
		}

		// What a person types into the address bar is not a reference relative
		// to the page on screen: "example.com" names a site, not a sibling of the
		// current document. Keep anything with a scheme or that names a local
		// file, and treat the rest as a host name.
		static std::string typed_url(const std::string_view text, const potato_app& app)
		{
			const auto first = text.find_first_not_of(" \t\r\n");
			if (first == std::string_view::npos) return {};
			const auto last = text.find_last_not_of(" \t\r\n");
			std::string url(text.substr(first, last - first + 1));

			if (has_scheme(url))
			{
				const auto colon = url.find(':');
				for (size_t i = 0; i < colon; ++i)
					url[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(url[i])));
				return url;
			}

			const bool drive_path = url.size() >= 2 && std::isalpha(static_cast<unsigned char>(url[0])) && url[1] == ':';
			if (drive_path || core::is_path_sep(url[0])) return url;

			// A path relative to the working directory, as on a command line.
			if (const auto local = app.absolute_file_path(url); platform::exists(local)) return local.str();
			return "https://" + url;
		}

		void open_local_file(const core::file_path& path)
		{
			if (!path.is_empty() && platform::exists(path)) navigate(path.str());
		}

		void choose_local_file()
		{
			std::vector<core::file_path> paths;
			const std::vector<platform::file_dialog_filter> filters = {
				{"Web pages (*.htm, *.html)", "*.htm;*.html;*.xhtml"},
				{"Text files (*.txt)", "*.txt"},
				{"All files (*.*)", "*.*"},
			};
			if (platform::prompt_for_open_paths(nullptr, paths, filters, false, "Open HTML file") && !paths.empty())
				open_local_file(paths.front());
		}

		std::vector<std::string> suggest(const std::string_view text) const
		{
			// Common bookmarks shown by default (no text) and folded into
			// the filtered results when the user starts typing.
			static constexpr std::string_view bookmarks[] = {
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
			std::set<std::string> seen;
			const auto add = [&](const std::string_view value)
			{
				if (result.size() < max_results && seen.insert(std::string(value)).second)
					result.emplace_back(value);
			};

			// First focus / empty edit: show recent history first, then
			// fill with bookmarks.
			if (text.empty())
			{
				for (auto it = _history.rbegin(); it != _history.rend(); ++it) add(*it);
				for (const auto bookmark : bookmarks) add(bookmark);
				return result;
			}

			std::string needle(text);
			for (auto& c : needle) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			const auto matches = [&](const std::string_view candidate)
			{
				for (size_t i = 0; i + needle.size() <= candidate.size(); ++i)
				{
					bool equal = true;
					for (size_t j = 0; j < needle.size(); ++j)
						if (std::tolower(static_cast<unsigned char>(candidate[i + j])) !=
							std::tolower(static_cast<unsigned char>(needle[j]))) equal = false;
					if (equal) return true;
				}
				return false;
			};

			for (auto it = _history.rbegin(); it != _history.rend(); ++it)
				if (matches(*it)) add(*it);
			for (const auto bookmark : bookmarks)
				if (matches(bookmark)) add(bookmark);
			return result;
		}
		// Browser-utility items only (no curated bookmarks).
		std::vector<ui::command_ptr> make_utility_menu()
		{
			std::vector<ui::command_ptr> result;
			result.push_back(make_command("&Back", [this] { go_back(); }));
			result.back()->enable = can_go_back();
			result.push_back(make_command("&Forward", [this] { go_forward(); }));
			result.back()->enable = can_go_forward();
			result.push_back(make_command("&Reload", [this] { reload(); }));
			// separator
			result.push_back(nullptr);
			result.push_back(make_command("&Home", [this] { navigate("about:blank", false); }));
			result.push_back(make_command("&Test page", [this] { navigate("res://test.htm", false); }));
			result.push_back(make_command("&Open file...", [this] { choose_local_file(); }));
			// separator
			result.push_back(nullptr);
			result.push_back(make_command("E&xit", [this] { if (_frame) _frame->close(); }));
			return result;
		}

		std::vector<ui::command_ptr> make_file_menu()
		{
			auto open = make_command("&Open...", [this] { choose_local_file(); });
			open->kba.push_back({U'O', platform::keyboard_accelerator_t::control});
			open->keyboard_accelerator_text = platform::format_keyboard_accelerator(open->kba);
			return {open, nullptr, make_command("E&xit", [this] { if (_frame) _frame->close(); })};
		}
		void build_ui()
		{
			// ── Toolbar ──
			_back_command = make_command("Back", [this] { go_back(); });
			_back_command->icon = icon_index::back;
			_back_command->toolbar_text.clear();
			_forward_command = make_command("Forward", [this] { go_forward(); });
			_forward_command->icon = icon_index::forward;
			_forward_command->toolbar_text.clear();
			_refresh_command = make_command("Refresh", [this] { reload(); });
			_refresh_command->icon = icon_index::refresh;
			_refresh_command->toolbar_text.clear();
			_home_command = make_command("Home", [this] { navigate("about:blank", false); });
			_home_command->icon = icon_index::home;
			_home_command->toolbar_text.clear();
			// Attach a hamburger menu to the right-hand "menu" button.
			_menu_command = make_command("Menu");
			_menu_command->icon = icon_index::menu;
			_menu_command->toolbar_text.clear();
			_menu_command->menu = [this] { return make_utility_menu(); };

			ui::toolbar_styles toolbar_style;
			toolbar_style.button_extent = {32, 30};
			_navigation = ui::make_toolbar(toolbar_style,
			                              {_back_command, _forward_command, _refresh_command, _home_command, _menu_command});

			ui::edit_styles address_style;
			address_style.rounded_corners = true;
			address_style.select_all_on_focus = true;
			address_style.cue = "Address";
			address_style.capture_key_down = [this](const int key, const platform::key_state keys)
			{
				const auto c = static_cast<char32_t>(key);
				if (c == platform::keys::RETURN)
				{
					navigate_typed(_address->window_text());
					if (_frame) _frame->focus();
					return true;
				}
				if (c == platform::keys::ESCAPE)
				{
					_address->window_text(_current_url.empty() && !_history.empty() ? _history[_history_pos] : _current_url);
					if (_frame) _frame->focus();
					return true;
				}
				if (keys.control && (c == U'O' || c == U'o'))
				{
					choose_local_file();
					return true;
				}
				return false;
			};
			_address = ui::make_edit(address_style, "about:blank", [this](const std::string& text)
			{
				if (_address) _address->auto_completes(suggest(text));
			});

			_navigation->flex = ui::flex_item::stretch;
			_address->flex = ui::flex_item::grow | ui::flex_item::stretch;

			auto file_menu = make_command("&File");
			file_menu->menu = [this] { return make_file_menu(); };
			_menus = {file_menu};
			_menu_bar = ui::make_menu_bar(_menus);
			_menu_bar->flex = ui::flex_item::stretch;

			_chrome = std::make_shared<ui::elements>();
			_chrome->padding(0);
			_chrome->flex_container.direction = ui::flex_direction::row;
			_chrome->flex_container.wrap = ui::flex_wrap::no_wrap;
			_chrome->flex_container.align_items = ui::flex_align::stretch;
			_chrome->add(_navigation);
			_chrome->add(_address);
			_chrome->add(_menu_bar);

			// ── Content ──
			_page = std::make_shared<page_element>();
			_page->set_on_open([this](const std::string& url) { navigate(url); });
			_page->set_on_diagnostic([this](const std::string& message) { eval_log(message); });
			_page->set_on_resource_started([this](const std::string& type, const std::string& url)
			{
				++_eval_pending_resources;
				eval_log(std::format("{} request: {}", type, url));
			});
			_page->set_on_resource_finished([this](const std::string& type, const std::string& url, const bool success)
			{
				_eval_pending_resources = std::max(0, _eval_pending_resources - 1);
				if (!success) ++_eval_failed_resources;
				eval_log(std::format("{} {}: {}", type, success ? "loaded" : "failed", url));
			});

			_root = std::make_shared<ui::elements>();
			_root->padding(0);
			_root->flex_container.direction = ui::flex_direction::column;
			_root->flex_container.wrap = ui::flex_wrap::no_wrap;
			_root->flex_container.align_items = ui::flex_align::stretch;
			_root->add(_chrome);
			_root->add(_page);
		}

		// Shows why `url` could not be displayed, so the address bar never names
		// something other than what the content area shows. The error page is
		// what the navigation ends in, so --eval stops after its quiet period
		// instead of waiting out the 60-second cap and reporting a timeout.
		void show_error(const std::string& url, const std::string_view title, const std::string_view detail)
		{
			_current_url = url;
			std::string html = "<html><head><title>" + html_escape(title) + "</title></head><body><h2>" +
				html_escape(title) + "</h2><p>" + html_escape(url) + "</p>";
			if (!detail.empty()) html += "<pre>" + html_escape(detail) + "</pre>";
			html += "</body></html>";
			_page->load_html(url, html);
			_eval_page_loaded = true;
		}

		void load(const std::string& url)
		{
			if (!_page) return;
			eval_log(std::format("Navigation started: {}", url));
			// Cancel any in-flight request for the previous navigation.
			if (_pending)
			{
				_pending->cancel();
				_pending.reset();
			}
			++_load_token;

			const auto [doc_url, fragment] = split_fragment(url);
			if (doc_url == "res://test.htm" || doc_url == "about:blank")
			{
				_current_url = url;
				_page->load_html(doc_url, std::string(platform::embedded_resource_text("test.htm")), {}, fragment);
				// Loaded synchronously, like a local file; without this --eval of the
				// built-in page waited out its 60-second cap.
				_eval_page_loaded = true;
				return;
			}

			if (!is_web_url(doc_url))
			{
				// A folder may itself contain '#', so the whole string is tried as a
				// path before its tail is read as a fragment.
				const std::pair<std::string, std::string> candidates[] = {{url, {}}, {doc_url, fragment}};
				for (const auto& [candidate, anchor] : candidates)
				{
					if (candidate.empty()) continue;
					const auto local_path = absolute_file_path(candidate);
					if (platform::is_directory(local_path))
					{
						show_error(url, "Cannot open a folder", {});
						return;
					}
					if (!platform::exists(local_path)) continue;

					const auto path = local_path.str();
					_current_url = url;
					auto bytes = read_file_string(local_path);
					if (path.size() >= 4 && is_equal(std::string_view(path).substr(path.size() - 4), ".txt"))
						_page->load_html(path, plain_text_page(bytes, {}), "text/html; charset=utf-8", anchor);
					else _page->load_html(path, bytes, "text/html", anchor);
					_eval_page_loaded = true;
					return;
				}
			}

			if (is_web_url(doc_url))
			{
				if (!_http) _http = platform::create_async_http_session("potato/1.0");
				if (!_http)
				{
					show_error(url, "Failed to load", "The request could not be started.");
					return;
				}

				const uint64_t token = _load_token;
				std::weak_ptr<potato_app> weak_self = shared_from_this();
				auto body = std::make_shared<std::string>();
				auto type = std::make_shared<std::string>();
				platform::async_http_callbacks cb;
				cb.on_headers = [weak_self, type, url](const int status, std::string content_type, const uint64_t content_length)
				{
					platform::run_ui([weak_self, type, url, status, content_type = std::move(content_type), content_length]
					{
						*type = content_type;
						if (const auto self = weak_self.lock())
							self->eval_log(std::format("Navigation headers: HTTP {}, type={}, length={}: {}", status,
							                          content_type, content_length, url));
					});
				};
				cb.on_data = [body](const uint8_t* data, const size_t size)
				{
					body->append(reinterpret_cast<const char*>(data), size);
				};
				cb.on_complete = [weak_self, body, type, url, token]
				{
					platform::run_ui([weak_self, body, type, url, token]
					{
						if (const auto self = weak_self.lock()) self->on_download_complete(token, url, *body, *type, {});
					});
				};
				cb.on_error = [weak_self, url, token](std::string error)
				{
					platform::run_ui([weak_self, url, token, error = std::move(error)]
					{
						if (const auto self = weak_self.lock()) self->on_download_complete(token, url, {}, {}, error);
					});
				};

				// The fragment is for the browser, never the server.
				_pending = _http->get(doc_url, std::move(cb));
				if (!_pending) show_error(url, "Failed to load", "The request could not be started.");
				return;
			}

			if (has_scheme(url)) show_error(url, "Cannot open this address", "Potato opens http, https and local files.");
			else show_error(url, "File not found", {});
		}

		void on_download_complete(const uint64_t token, const std::string& url, const std::string& body,
		                          const std::string& content_type, const std::string& error)
		{
			// Stale response (newer navigation issued).
			if (token != _load_token) return;
			_pending.reset();
			if (!_page) return;
			if (!error.empty() || body.empty())
			{
				eval_log(std::format("Navigation failed: {}: {}", url, error.empty() ? "empty response" : error));
				show_error(url, "Failed to load", error.empty() ? std::string("(empty response)") : error);
				return;
			}

			_current_url = url;
			eval_log(std::format("Navigation downloaded: {} bytes: {}", body.size(), url));
			const auto [doc_url, fragment] = split_fragment(url);
			if (is_plain_text(content_type))
				_page->load_html(doc_url, plain_text_page(body, content_type), "text/html; charset=utf-8", fragment);
			else _page->load_html(doc_url, body, content_type, fragment);
			_eval_page_loaded = true;
		}
		// Shows a history entry. A fragment of the document already on screen
		// only scrolls; anything else loads. Moving through history between
		// entries of one document scrolls too, the way following its links did.
		void show(const std::string& url, const bool traversal)
		{
			if (_address) _address->window_text(url);
			const bool in_page = _page && _page->has_document() && !_pending &&
				(traversal || url.find('#') != std::string::npos) && same_document(url, _current_url);
			if (in_page)
			{
				_current_url = url;
				_page->scroll_to_anchor(split_fragment(url).second);
			}
			else load(url);
			update_buttons();
		}

		// Follows a link or opens a URL the browser itself chose. A link from a
		// document arrives already resolved; resolving again is harmless.
		void navigate(const std::string& url, const bool relative_to_page = true)
		{
			std::string target = url;
			if (relative_to_page && !_current_url.empty())
			{
				auto resolved = platform::resolve_url(_current_url, url);
				if (!resolved.empty()) target = std::move(resolved);
			}
			// Truncate forward history when navigating from the middle.
			if (_history_pos + 1 < _history.size()) _history.resize(_history_pos + 1);
			if (_history.empty() || _history.back() != target)
			{
				_history.push_back(target);
				_history_pos = _history.size() - 1;
			}
			show(target, false);
		}

		// What the address bar hands over on Enter.
		void navigate_typed(const std::string& text)
		{
			const auto url = typed_url(text, *this);
			if (url.empty())
			{
				if (_address && !_history.empty()) _address->window_text(_history[_history_pos]);
				return;
			}
			navigate(url, false);
		}

		// Loads the current entry again without touching the history around it.
		void reload()
		{
			if (_history.empty()) return;
			const auto url = _history[_history_pos];
			if (_address) _address->window_text(url);
			load(url);
		}

		void go_back()
		{
			if (!can_go_back()) return;
			--_history_pos;
			show(_history[_history_pos], true);
		}

		void go_forward()
		{
			if (!can_go_forward()) return;
			++_history_pos;
			show(_history[_history_pos], true);
		}

		void open_dropped_files(platform::clipboard_data& data)
		{
			if (data.has_drop_files())
			{
				const auto path = data.first_path();
				if (!path.is_empty()) open_local_file(path);
			}
		}

		void parse_options(const std::string_view command_line)
		{
			_options = {};
			const auto tokens = command_line_tokens(command_line);
			for (size_t i = 0; i < tokens.size(); ++i)
			{
				const auto& p = tokens[i];
				if (p == "/test" || p == "--test" || p == "-test")
				{
					_options.console = true;
					_options.network_test = true;
				}
				else if (p == "/test-offline" || p == "--test-offline" || p == "-test-offline")
				{
					_options.console = true;
					_options.network_test = false;
				}
				else if (p.starts_with("/layout:") || p.starts_with("--layout:"))
				{
					_options.console = true;
					_options.layout_path = p.substr(p.find(':') + 1);
				}
				else if (p == "--layout" && i + 1 < tokens.size())
				{
					_options.console = true;
					_options.layout_path = tokens[++i];
				}
				else if (p == "--verbose" || p == "-v") _options.layout_verbose = true;
				else if (p.starts_with("--width:")) _options.layout_width = safe_stoi(p.substr(p.find(':') + 1), 1902);
				else if (p.starts_with("--repeat:")) _options.layout_repeats = safe_stoi(p.substr(p.find(':') + 1), 1);
				else if (p == "--dump") _options.layout_dump = 64;
				else if (p.starts_with("--dump:")) _options.layout_dump = safe_stoi(p.substr(p.find(':') + 1), 64);
				else if (p == "--dump-json") _options.layout_dump_json = true;
				else if (p.starts_with("/eval:") || p.starts_with("--eval:"))
				{
					_options.eval_url = p.substr(p.find(':') + 1);
				}
				else if (!p.starts_with('-') && !p.starts_with('/') && _options.startup_url.empty())
					_options.startup_url = p;
			}
			if (!_options.layout_path.empty()) _options.console = true;
		}

		void walk_elements(const ui::element_ptr& element, const core::pointi offset,
		                   const std::function<void(const ui::element_ptr&, core::pointi)>& visit) const
		{
			if (!element) return;
			visit(element, offset);
			const auto child_offset = offset + element->children_offset();
			element->visit_children([&](const ui::element_ptr& child) { walk_elements(child, child_offset, visit); });
		}

	public:
		potato_app() = default;
		~potato_app() override = default;

		bool pre_init() override
		{
			// libwebvis reaches the machine through this host, so it has to be in
			// place before anything creates a document — including the CLI modes.
			webvis::set_host(&g_host);
			return true;
		}

		bool parse_command_line(const std::string_view command_line) override
		{
			parse_options(command_line);
			return _options.console;
		}

		int run_console_mode() override
		{
			if (!_options.layout_path.empty())
				return run_layout(_options.layout_path, _options.layout_width, 896, _options.layout_repeats,
			                          _options.layout_verbose, _options.layout_dump, _options.layout_dump_json);
			return run_self_test(_options.network_test);
		}

		bool load_settings(const platform::setting_file_ptr& store) override
		{
			const auto running = platform::running_app_path().folder();
			_settings = platform::create_ini_file_settings(running);
			if (!_settings) _settings = store;
			return _settings != nullptr;
		}

		bool wants_gpu() const override { return true; }

		bool key_down(const char32_t key, const platform::key_state keys) override
		{
			if (key == platform::keys::F3)
			{
				focus_address();
				return true;
			}
			if (_address && _address->has_focus()) return false;
			if (ui::dispatch_accelerator(_menus, key, keys)) return true;
			for (const auto& menu : _menus)
				if (menu->menu && ui::dispatch_accelerator(menu->menu(), key, keys)) return true;
			if (key == platform::keys::BROWSER_BACK) { go_back(); return true; }
			if (key == platform::keys::BROWSER_FORWARD) { go_forward(); return true; }
			if (key == platform::keys::BROWSER_REFRESH) { reload(); return true; }
			if (key == platform::keys::BROWSER_HOME) { navigate("about:blank", false); return true; }
			if (key == platform::keys::BROWSER_STOP || key == platform::keys::MEDIA_STOP)
			{
				if (_pending) { _pending->cancel(); _pending.reset(); }
				return true;
			}
			return _page && _page->key_down(key, keys);
		}

		bool text_input(std::string_view) override { return false; }

		ui::focus_mode focus_mode() const override
		{
			return _address && _address->has_focus() ? ui::focus_mode::text_edit : ui::focus_mode::view;
		}

		void track_menu(const ui::frame_ptr& parent, const core::recti bounds,
		                const std::vector<ui::command_ptr>& commands) override
		{
			if (parent) parent->track_menu(bounds, commands);
		}

		bool can_exit() override { return true; }
		bool init(const std::string_view command_line) override
		{
			parse_options(command_line);
			webvis::set_host(&g_host);
			g_host.open_network("potato/1.0");
			_http = platform::create_async_http_session("potato/1.0");
			_eval_url = _options.eval_url;
			_eval_started = std::chrono::steady_clock::now();
			_eval_last_activity = _eval_started;

			build_ui();
			ui::frame_style style;
			style.drawn_caption = false;
			style.hardware_accelerated = true;
			style.client_size = {1200, 800};
			style.minimum_client = {480, 240};
			// Evaluation runs are automated, so keep their window off the desktop.
			style.offscreen = !_eval_url.empty();
			style.can_drop = true;
			style.timer_milliseconds = _eval_url.empty() ? 0 : 500;
			style.colors = {core::rgb(240, 240, 240), core::rgb(32, 32, 32), core::rgb(220, 230, 245)};

			const auto self = shared_from_this();
			_frame = ui::create_app_frame(std::static_pointer_cast<ui::app>(self), _settings,
			                              std::static_pointer_cast<ui::frame_host>(self), style);
			if (!_frame) return false;
			_frame->window_text("Potato");
			_page->set_callbacks([this] { if (_frame) _frame->invalidate(); },
			                     [this](platform::cursor c) { if (_frame) _frame->set_cursor(c); });
			_frame->layout();

			if (!_options.eval_url.empty())
			{
				eval_log(std::format("Evaluation started: {}", _options.eval_url));
				navigate_typed(_options.eval_url);
			}
			// A URL on the command line is read the way the address bar reads it.
			else if (!_options.startup_url.empty()) navigate_typed(_options.startup_url);
			else navigate("res://test.htm", false);
			update_buttons();
			return true;
		}

		void exit() override
		{
			if (_pending) { _pending->cancel(); _pending.reset(); }
			if (_http) _http->stop();
		}

		void final_exit() override { g_host.stop_network(); }

		void tick() override
		{
			if (_eval_url.empty() || !_frame) return;
			const auto now = std::chrono::steady_clock::now();
			const auto elapsed = now - _eval_started;
			const auto quiet = now - _eval_last_activity;
			if ((_eval_page_loaded && _eval_pending_resources == 0 && quiet >= std::chrono::seconds(5)) ||
				elapsed >= std::chrono::seconds(60))
			{
				eval_log(std::format("Evaluation complete: pending={}, failed={}, timed_out={}", _eval_pending_resources,
				                     _eval_failed_resources, elapsed >= std::chrono::seconds(60)));
				_frame->close();
			}
		}

		// ── frame_host ──
		void on_window_layout(render::measure_context& mc, const core::sizei extent, bool) override
		{
			g_host.set_measure_context(&mc);
			if (_root) _root->layout(mc, core::recti(extent));
			g_host.set_measure_context(nullptr);
		}

		void on_window_paint(render::draw_context& dc) override
		{
			dc.clear(core::color(core::rgb(240, 240, 240)));
			if (_root) _root->render(dc, {});
		}

		void visit_elements(const std::function<void(const ui::element_ptr&, core::pointi)>& visit) override
		{
			walk_elements(_root, {}, visit);
		}

		void focus_changed(const bool has_focus, const ui::control_ptr& child) override
		{
			if (!has_focus && child == _address && _address)
				_address->window_text(_current_url.empty() && !_history.empty() ? _history[_history_pos] : _current_url);
		}
		void on_mouse_move(const core::pointi loc, bool) override { if (_page) _page->pointer_move(loc); }
		void on_mouse_left_button_down(const core::pointi loc, const platform::key_state keys) override
		{
			if (_frame) _frame->focus();
			if (_page) _page->pointer_down(loc, keys);
		}
		void on_mouse_left_button_up(const core::pointi loc, const platform::key_state keys) override
		{
			if (_page) _page->pointer_up(loc, keys);
		}
		void on_mouse_leave(const core::pointi) override { if (_page) _page->pointer_leave(); }
		void on_mouse_wheel(const core::pointi loc, const int delta, const platform::key_state, bool& was_handled) override
		{
			if (_page) was_handled = _page->wheel_at(loc, delta);
		}
		bool key_down(const int key, const platform::key_state keys) override
		{
			return _page && _page->key_down(static_cast<char32_t>(key), keys);
		}
		platform::drop_effect drag_over(const platform::clipboard_data& data, const platform::key_state,
		                                const core::pointi) override
		{
			return data.has_drop_files() ? platform::drop_effect::copy : platform::drop_effect::none;
		}
		platform::drop_effect drag_drop(platform::clipboard_data& data, const platform::key_state,
	                                const core::pointi) override
		{
			open_dropped_files(data);
			return data.has_drop_files() ? platform::drop_effect::copy : platform::drop_effect::none;
		}
		void drag_leave() override {}
	};

	void register_chrome_icons()
	{
		render::set_icon_font_family("Segoe Fluent Icons");
		ui::set_chrome_icons({
			.check = icon_index::check,
			.checkbox_off = icon_index::checkbox_off,
			.checkbox_on = icon_index::checkbox_on,
			.radio_off = icon_index::radio_off,
			.radio_on = icon_index::radio_on,
			.close = icon_index::close,
			.maximize = icon_index::maximize,
			.minimize = icon_index::minimize,
			.restore = icon_index::restore,
		});
	}

	// The browser's own cases, run in the same table as the engine's
	// (webvis::run_tests) by the self-test.
	void register_shell_tests(webvis::tests& t)
	{
		t.register_test("Shell: typed text with a scheme, a drive or a port", []
		{
			should::equal(true, has_scheme("https://example.com/"), "https");
			should::equal(true, has_scheme("about:blank"), "about");
			should::equal(true, has_scheme("res://test.htm"), "res");
			should::equal(false, has_scheme("c:\\pages\\a.html"), "drive letter");
			should::equal(false, has_scheme("localhost:8080/a"), "port");
			should::equal(false, has_scheme("example.com"), "bare host");
		});

		t.register_test("Shell: same document whatever the fragment", []
		{
			should::equal(true, same_document("https://a.test/p#x", "https://a.test/p#y"), "fragments");
			should::equal(false, same_document("https://a.test/p", "https://a.test/q"), "other page");
			should::equal(false, same_document("https://a.test/P", "https://a.test/p"), "web paths keep case");
			should::equal(true, same_document("C:\\Dir\\Page.html#a", "c:/dir/page.html"), "local path");
		});

		t.register_test("Shell: fragments are percent-decoded", []
		{
			should::equal("caf\xC3\xA9", percent_decode("caf%C3%A9"), "UTF-8");
			should::equal("%zz and %4", percent_decode("%zz and %4"), "not an escape");
		});

		t.register_test("Shell: CSS generic font families name installed faces", []
		{
			should::equal("Consolas", windows_face("monospace"), "monospace");
			should::equal("Consolas", windows_face(" Monospace "), "case and spaces");
			should::equal("Arial", windows_face("sans-serif"), "sans-serif");
			should::equal("Times New Roman", windows_face("serif"), "serif");
			should::equal("Segoe UI", windows_face("Segoe UI"), "a face is kept");
		});

		t.register_test("Shell: command-line paths preserve backslashes", []
		{
			const auto tokens = command_line_tokens(R"("--layout:C:\code\apps\potato\src\potato\res\test.htm" "C:\My Pages\a.html")");
			should::EqualTrue(tokens.size() == 2, "two quoted arguments");
			should::equal(std::string(R"(--layout:C:\code\apps\potato\src\potato\res\test.htm)"), tokens[0], "switch path");
			should::equal(std::string(R"(C:\My Pages\a.html)"), tokens[1], "file path");
		});
		t.register_test("Shell: text measurement counts spaces", []
		{
			webvis::font_desc desc;
			desc.face = "Arial";
			desc.size = 16;
			const auto font = g_host.create_font(desc, nullptr);
			should::EqualTrue(g_host.text_width(font, " ") > 0, "a space has width");
			g_host.destroy_font(font);
		});
		// text/plain used to be parsed as HTML: <b> turned bold and lines ran together.
		t.register_test("Shell: plain text is shown, not parsed", []
		{
			should::equal(true, is_plain_text("text/plain; charset=utf-8"), "with a charset");
			should::equal(true, is_plain_text(" Text/Plain "), "case and spaces");
			should::equal(false, is_plain_text("text/html"), "html");
			should::equal(false, is_plain_text(""), "no type");
			const auto page = plain_text_page("a <b>c</b>\n\xE9", "text/plain; charset=iso-8859-1");
			should::EqualTrue(page.find("a &lt;b&gt;c&lt;/b&gt;\n\xC3\xA9") != std::string::npos,
			                  "escaped, decoded, line break kept");
		});

		// Downloads land in %TEMP%; every way a download can end must delete
		// its file. A download cancelled at exit used to leave it behind.
		t.register_test("Shell: a scratch file lives until its last owner lets go", []
		{
			auto scratch = scratch_file::create("pts");
			should::EqualTrue(scratch != nullptr, "created");
			const std::string body = "body";
			scratch->write(reinterpret_cast<const uint8_t*>(body.data()), body.size());
			scratch->close();
			const auto path = scratch->path();
			// the completion's copy
			auto reader = scratch;
			scratch.reset();
			should::EqualTrue(platform::exists(path), "kept while the completion holds it");
			reader.reset();
			should::EqualTrue(!platform::exists(path), "deleted by the last owner");
		});

		t.register_test("Shell: an abandoned scratch file goes at once, and only once", []
		{
			auto scratch = scratch_file::create("pts");
			should::EqualTrue(scratch != nullptr, "created");
			const auto path = scratch->path();
			scratch->discard();
			should::EqualTrue(!platform::exists(path), "discard deletes");
			const std::string late = "late";
			scratch->write(reinterpret_cast<const uint8_t*>(late.data()), late.size());
			should::EqualTrue(!platform::exists(path), "a late write is dropped");
			// The system may hand the freed name to the next download.
			should::EqualTrue(platform::open_file(path, platform::file_open_mode::create) != nullptr, "name reused");
			scratch.reset();
			const bool survived = platform::exists(path);
			platform::delete_file(path);
			should::EqualTrue(survived, "the old owner leaves the reused name alone");
		});
	}

	// Headless layout of a local HTML file. No window and no message loop, so
	// no async resource can ever land and the result is repeatable. Prints
	// "<file>: <w>x<h>" plus stage timings, structure and layout anomalies.
	int run_layout(const std::string& path, const int width, const int height, const int repeats,
	               const bool verbose, const int dump_depth, const bool dump_json)
	{
		const auto html = get_file_contents(path);
		if (html.empty())
		{
			platform::write_stdout(std::format("Layout: cannot read {}\n", path));
			return 11;
		}

		layout_result r;
		for (auto i = 0; i < std::max(1, repeats); ++i)
		{
			r = layout_html_headless(html, width, height, dump_json ? false : verbose, dump_depth, dump_json);
			if (!dump_json)
				platform::write_stdout(std::format("{}: {}x{} (parse+style {} us, layout {} us)\n", path, r.width, r.height,
				                                   r.parse_style_us, r.layout_us));
		}

		if (dump_json)
		{
			platform::write_stdout(r.layout_json + "\n");
			return r.height > 0 ? 0 : 12;
		}
		const auto& s = r.stats;
		platform::write_stdout(std::format("  nodes: {} elements, {} text, {} images, depth {}; right edge {}, {} hidden subtrees\n",
		                                   s.elements, s.text_nodes, s.images, s.max_depth, s.right_edge, s.hidden_subtrees));
		const auto anomalies = s.overflow_x + s.negative_x + s.zero_area_text + s.unsized_image + s.negative_size;
		if (anomalies)
		{
			platform::write_stdout(std::format(
				"  anomalies: {} overflow-x, {} negative-x, {} zero-height-text, {} unsized-image, {} negative-size\n",
				s.overflow_x, s.negative_x, s.zero_area_text, s.unsized_image, s.negative_size));
			for (const auto& a : r.anomalies) platform::write_stdout("    " + a + "\n");
		}
		else platform::write_stdout("  anomalies: none\n");
		for (const auto& line : r.box_dump) platform::write_stdout(line + "\n");
		return r.height > 0 ? 0 : 12;
	}
	// Combined self-test:
	//   1. Runs the in-process unit tests (run_tests() from core.cpp, plus the
	//      browser's own from register_shell_tests) and
	//      writes the HTML report to a temp file.
	//   2. Unless offline, synchronously fetches a handful of live pages and
	//      verifies each returns a successful HTTP response with a non-empty body.
	// Prints progress to stdout. Returns 0 on success, non-zero on failure.
	int run_self_test(const bool network)
	{
		int result = 0;
		// 1. Unit tests
		platform::write_stdout("Potato self-test: running unit tests ...\n");
		const auto report = run_tests(register_shell_tests);
		const auto report_path = platform::temp_file(".tmp");
		if (const auto out = platform::open_file(report_path, platform::file_open_mode::create))
		{
			out->write(reinterpret_cast<const uint8_t*>(report.data()), report.size());
			out->flush();
		}
		const bool unit_failed = report.find("FAILED") != std::string::npos;
		platform::write_stdout(std::format("Unit tests: {} (report: {})\n", unit_failed ? "FAIL" : "PASS", report_path.str()));
		if (unit_failed) result = 10;

		const auto fetch = [](const std::string_view url) -> int
		{
			platform::write_stdout(std::format("Potato self-test: fetching {} ...\n", url));
			const auto session = platform::create_async_http_session("Potato/1.0-test");
			if (!session)
			{
				platform::write_stdout("FAIL: create_async_http_session returned null\n");
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
			platform::async_http_callbacks cb;
			cb.on_headers = [&](const int sc, std::string ct, uint64_t)
			{
				std::lock_guard lk(mtx);
				status_code = sc;
				content_type = std::move(ct);
			};
			cb.on_data = [&](const uint8_t* data, const size_t size)
			{
				std::lock_guard lk(mtx);
				body_bytes += size;
				if (body_preview.size() < 256)
				{
					const auto take = std::min<size_t>(size, 256 - body_preview.size());
					body_preview.append(reinterpret_cast<const char*>(data), take);
				}
			};
			cb.on_complete = [&]
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
				platform::write_stdout("FAIL: session->get returned null\n");
				return 2;
			}
			std::unique_lock lk(mtx);
			if (!cv.wait_for(lk, std::chrono::seconds(30), [&] { return done; }))
			{
				platform::write_stdout("FAIL: request timed out after 30 seconds\n");
				req->cancel();
				return 3;
			}
			platform::write_stdout(std::format("  HTTP status: {}, content-type: {}, body: {} bytes\n", status_code, content_type,
			                                   body_bytes));
			if (!body_preview.empty())
			{
				// Replace control chars for stdout safety; show full preview.
				std::string preview;
				for (const auto c : body_preview) preview.push_back((c == '\r' || c == '\n' || c == '\t') ? ' ' : c);
				platform::write_stdout(std::format("  body preview: {}\n", preview));
			}
			if (!error.empty()) { platform::write_stdout(std::format("FAIL: {}\n", error)); return 4; }
			if (status_code < 200 || status_code >= 400) { platform::write_stdout("FAIL: unexpected HTTP status code\n"); return 5; }
			if (body_bytes == 0) { platform::write_stdout("FAIL: empty response body\n"); return 6; }
			platform::write_stdout("  PASS\n");
			return 0;
		};

		if (!network)
		{
			platform::write_stdout("Network checks skipped (offline self-test).\n");
			return result;
		}
		for (const auto url : {"http://example.com/", "https://example.com/", "https://www.google.com/",
		                       "https://en.wikipedia.org/wiki/Main_Page"})
		{
			const auto rc = fetch(url);
			if (rc != 0 && result == 0) result = rc;
		}
		return result;
	}
}

// ── App entry points ──────────────────────────────────────────────────────
int main(int argc, char** argv)
{
	core::set_app_name("Potato");
	register_chrome_icons();
	webvis::set_host(&g_host);
	auto app = std::make_shared<potato_app>();
	return platform::run(app, argc, argv);
}

// Portable UI dispatch entry point declared in webvis.h. Forwards to the
// platform layer's task queue.
void webvis::dispatch_to_ui(std::function<void()> fn)
{
	platform::run_ui(std::move(fn));
}
