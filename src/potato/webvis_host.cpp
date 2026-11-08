// webvis_host.cpp - Implements the libwebvis host over libdesktop's
// core/platform/render services: fonts, images, files, HTTP and screen data.

#include "pch.h"
#include "webvis_host.h"
#include "core.h"

#include "libdesktop/render/software.h"

#include <atomic>

namespace
{
	std::string file_contents(const core::file_path& path)
	{
		auto data = platform::blob_from_file(path);
		if (data.empty()) return {};
		return {reinterpret_cast<const char*>(data.data()), data.size()};
	}

	std::string svg_attribute(const std::string& tag, const std::string& name)
	{
		size_t pos = 0;
		while ((pos = tag.find(name, pos)) != std::string::npos)
		{
			const bool valid_start = pos == 0 || std::isspace(static_cast<unsigned char>(tag[pos - 1]));
			size_t equals = pos + name.size();
			while (equals < tag.size() && std::isspace(static_cast<unsigned char>(tag[equals]))) ++equals;
			if (!valid_start || equals >= tag.size() || tag[equals] != '=')
			{
				pos += name.size();
				continue;
			}

			++equals;
			while (equals < tag.size() && std::isspace(static_cast<unsigned char>(tag[equals]))) ++equals;
			if (equals >= tag.size()) return {};

			const char quote = tag[equals];
			if (quote == '\'' || quote == '"')
			{
				const auto end = tag.find(quote, equals + 1);
				return end == std::string::npos ? std::string{} : tag.substr(equals + 1, end - equals - 1);
			}

			const auto end = tag.find_first_of(" \t\r\n>", equals);
			return tag.substr(equals, end - equals);
		}
		return {};
	}

	double svg_number(const std::string& value)
	{
		if (value.empty() || value.find('%') != std::string::npos) return 0;
		char* end = nullptr;
		const double result = std::strtod(value.c_str(), &end);
		return end != value.c_str() && result > 0 ? result : 0;
	}

	// SVG is not rendered; a correctly sized frame keeps the surrounding layout
	// honest instead of collapsing the box to nothing.
	core::surface_ptr create_svg_placeholder(const std::string& text)
	{
		std::string lower = text.substr(0, std::min<size_t>(text.size(), 8192));
		for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

		const auto svg_start = lower.find("<svg");
		if (svg_start == std::string::npos) return nullptr;
		const auto svg_end = lower.find('>', svg_start + 4);
		if (svg_end == std::string::npos) return nullptr;

		const auto tag = lower.substr(svg_start, svg_end - svg_start + 1);
		double width = svg_number(svg_attribute(tag, "width"));
		double height = svg_number(svg_attribute(tag, "height"));

		double view_width = 0;
		double view_height = 0;
		const auto view_box = svg_attribute(tag, "viewbox");
		if (!view_box.empty())
		{
			const char* cursor = view_box.c_str();
			for (int part = 0; part < 4; ++part)
			{
				while (*cursor && (std::isspace(static_cast<unsigned char>(*cursor)) || *cursor == ',')) ++cursor;
				char* end = nullptr;
				const double value = std::strtod(cursor, &end);
				if (end == cursor) break;
				if (part == 2) view_width = value;
				if (part == 3) view_height = value;
				cursor = end;
			}
		}

		if (width <= 0) width = view_width;
		if (height <= 0) height = view_height;
		if (width <= 0 && height > 0 && view_width > 0 && view_height > 0)
			width = height * view_width / view_height;
		if (height <= 0 && width > 0 && view_width > 0 && view_height > 0)
			height = width * view_height / view_width;
		if (width <= 0) width = 300;
		if (height <= 0) height = 150;

		const int bitmap_width = std::clamp(static_cast<int>(width + 0.5), 1, 2048);
		const int bitmap_height = std::clamp(static_cast<int>(height + 0.5), 1, 2048);
		auto surface = std::make_shared<core::surface>();
		surface->alloc(bitmap_width, bitmap_height, core::texture_format::ARGB);
		for (int y = 0; y < bitmap_height; ++y)
		{
			auto* pixels = reinterpret_cast<uint32_t*>(surface->pixels_line(y));
			std::fill(pixels, pixels + bitmap_width, core::rgba(238, 238, 238));
			pixels[0] = pixels[bitmap_width - 1] = core::rgba(153, 153, 153);
			const int diagonal = bitmap_height > 1
				                     ? y * (bitmap_width - 1) / (bitmap_height - 1)
				                     : 0;
			pixels[diagonal] = core::rgba(187, 187, 187);
			pixels[bitmap_width - 1 - diagonal] = core::rgba(187, 187, 187);
		}
		if (bitmap_height > 0)
		{
			auto* first = reinterpret_cast<uint32_t*>(surface->pixels_line(0));
			auto* last = reinterpret_cast<uint32_t*>(surface->pixels_line(bitmap_height - 1));
			std::fill(first, first + bitmap_width, core::rgba(153, 153, 153));
			std::fill(last, last + bitmap_width, core::rgba(153, 153, 153));
		}
		return surface;
	}

	webvis::position intersect(const webvis::position& a, const webvis::position& b)
	{
		const int l = std::max(a.left(), b.left());
		const int t = std::max(a.top(), b.top());
		const int r = std::min(a.right(), b.right());
		const int bottom = std::min(a.bottom(), b.bottom());
		return webvis::position(l, t, std::max(0, r - l), std::max(0, bottom - t));
	}

	// The file a resource URL names when it is on this machine: what
	// platform::resolve_url makes of a reference relative to a page opened from disk.
	// Empty for anything that has to be fetched over the network.
	core::file_path local_resource_path(const std::string& url)
	{
		std::string path = url;
		if (path.size() >= 7 && webvis::nicmp(path.c_str(), "file://", 7) == 0)
			path = platform::resolve_url({}, path);

		const bool drive_path = path.size() >= 3 && std::isalpha(static_cast<unsigned char>(path[0])) &&
			path[1] == ':' && core::is_path_sep(path[2]);
		const bool rooted_path = !path.empty() && core::is_path_sep(path[0]);
		if (!drive_path && !rooted_path) return {};

		// A query or fragment is not part of the file name.
		const auto cut = path.find_first_of("?#");
		if (cut != std::string::npos) path.resize(cut);
		return core::file_path(path);
	}

	webvis::resource_result decode_resource(const webvis::resource_kind kind, const core::file_path& path)
	{
		webvis::resource_result result;

		if (kind == webvis::resource_kind::stylesheet)
		{
			result.text = file_contents(path);
			result.ok = !result.text.empty();
		}
		else
		{
			auto surface = platform::load_image_file(path);
			if (!surface) surface = create_svg_placeholder(file_contents(path));

			if (surface)
			{
				result.image = std::make_shared<potato_image>(std::move(surface));
				result.ok = true;
			}
		}

		return result;
	}
}


// ── potato_device_context ────────────────────────────────────────────────────

core::recti potato_device_context::to_rect(const webvis::position& p) const
{
	return {p.x + _origin.x, p.y + _origin.y, p.right() + _origin.x, p.bottom() + _origin.y};
}

core::color potato_device_context::to_color(const webvis::web_color& c) const
{
	return core::color(core::rgba(c.red, c.green, c.blue, c.alpha));
}

void potato_device_context::fill_rect(const webvis::position& pos, const webvis::web_color& color)
{
	_dc.draw_rect(to_rect(pos), to_color(color));
}

void potato_device_context::draw_text(const int x, const int y, const std::string_view text,
                                      const webvis::font_handle font, const webvis::web_color& color)
{
	const auto face = static_cast<render::font_face>(font);
	const auto extent = _dc.measure_text(text, face, render::text_style::single_line, 1000000, 1000);
	const auto top = core::pointi{x + _origin.x, y + _origin.y};
	const core::recti bounds(top.x, top.y, top.x + std::max(1, extent.cx), top.y + std::max(1, extent.cy));
	_dc.draw_text(text, bounds, face, render::text_style::single_line, to_color(color), {});
}

void potato_device_context::draw_image(const webvis::image_ptr& img, const webvis::position& pos)
{
	const auto native = std::static_pointer_cast<potato_image>(img);
	if (!native || !native->surface() || native->surface()->empty()) return;
	render::draw_surface(_dc, native->surface(), to_rect(pos));
}

void potato_device_context::draw_ellipse(const webvis::position& pos, const webvis::web_color& color,
                                         const int line_width)
{
	_dc.draw_ellipse(to_rect(pos), to_color(color), line_width);
}

void potato_device_context::fill_ellipse(const webvis::position& pos, const webvis::web_color& color)
{
	_dc.fill_ellipse(to_rect(pos), to_color(color));
}

void potato_device_context::push_clip(const webvis::position& pos)
{
	const auto clip = _clips.empty() ? pos : intersect(_clips.back(), pos);
	_clips.push_back(clip);
	_dc.clip_bounds(to_rect(clip));
}

void potato_device_context::pop_clip()
{
	if (_clips.empty()) return;
	_clips.pop_back();
	_dc.restore_clip();
}


// ── fonts ────────────────────────────────────────────────────────────────────

std::string windows_face(const std::string_view css_family)
{
	static constexpr std::pair<std::string_view, std::string_view> generics[] = {
		{"monospace", "Consolas"}, {"ui-monospace", "Consolas"},
		{"sans-serif", "Arial"}, {"ui-sans-serif", "Segoe UI"}, {"system-ui", "Segoe UI"},
		{"serif", "Times New Roman"}, {"ui-serif", "Times New Roman"},
		{"cursive", "Comic Sans MS"}, {"fantasy", "Impact"},
	};

	auto name = css_family;
	while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front()))) name.remove_prefix(1);
	while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.remove_suffix(1);

	for (const auto& [generic, face] : generics)
	{
		if (name.size() == generic.size() && webvis::nicmp(name.data(), generic.data(), name.size()) == 0)
			return std::string(face);
	}

	return std::string(css_family);
}


// ── scratch_file ─────────────────────────────────────────────────────────────

scratch_file::scratch_file(core::file_path path, platform::file_ptr file)
	: _path(std::move(path)), _file(std::move(file))
{
}

scratch_file::~scratch_file()
{
	discard();
}

std::shared_ptr<scratch_file> scratch_file::create(const std::string_view /*prefix*/)
{
	auto path = platform::temp_file(".tmp");
	if (path.is_empty()) return nullptr;

	auto file = platform::open_file(path, platform::file_open_mode::create);
	if (!file)
	{
		platform::delete_file(path);
		return nullptr;
	}

	return std::make_shared<scratch_file>(std::move(path), std::move(file));
}

void scratch_file::write(const uint8_t* data, const size_t size)
{
	std::lock_guard lk(_mutex);
	if (_file) _file->write(data, size);
}

void scratch_file::close()
{
	std::lock_guard lk(_mutex);
	_file.reset();
}

void scratch_file::discard()
{
	std::lock_guard lk(_mutex);
	_file.reset();
	if (_deleted) return;
	_deleted = true;
	if (!_path.is_empty()) platform::delete_file(_path);
}


// ── the async HTTP client ────────────────────────────────────────────────────

// Each request downloads to a temp file and then delivers one completion on the
// UI thread. In-flight requests are tracked so they can all be cancelled at
// shutdown. A cancelled request delivers no completion at all, so cancelling
// also abandons its scratch file; it used to be left behind in %TEMP%.
class potato_host::http_client
{
	struct request
	{
		using callback_t = std::function<void(const std::string& file, uint32_t error, uint32_t status)>;

		explicit request(callback_t cb) : callback(std::move(cb))
		{
		}

		void cancel()
		{
			std::function<void()> abandon_now;
			{
				std::lock_guard lk(mutex);
				if (async) async->cancel();
				abandon_now = abandon;
			}
			if (abandon_now) abandon_now();
		}

		void set_async(platform::async_http_request_ptr a)
		{
			std::lock_guard lk(mutex);
			async = std::move(a);
		}

		callback_t callback;
		std::mutex mutex;
		platform::async_http_request_ptr async;
		// set before the request is shared
		std::function<void()> abandon;
	};

	platform::async_http_session_ptr _session;
	std::mutex _mutex;
	std::vector<std::shared_ptr<request>> _requests;

public:
	~http_client() { close(); }

	bool open(const std::string_view user_agent)
	{
		_session = platform::create_async_http_session(user_agent);
		return static_cast<bool>(_session);
	}

	void stop()
	{
		std::vector<std::shared_ptr<request>> snapshot;
		{
			std::lock_guard lk(_mutex);
			snapshot = _requests;
		}
		for (const auto& r : snapshot) r->cancel();
		if (_session) _session->stop();
	}

	void close()
	{
		stop();
		_session.reset();
	}

	bool download(const std::string& url_in, request::callback_t callback)
	{
		if (!_session) return false;

		std::string url = url_in;
		if (!url.starts_with("http://") && !url.starts_with("https://")) url = "https://" + url;

		auto scratch = scratch_file::create("pot");
		if (!scratch) return false;

		struct ctx_t
		{
			std::shared_ptr<request> req;
			// set once, before the request starts
			std::shared_ptr<scratch_file> scratch;
			http_client* parent = nullptr;
			std::atomic<int> status_code{0};
			std::atomic<bool> done{false};
		};

		auto req = std::make_shared<request>(std::move(callback));
		auto ctx = std::make_shared<ctx_t>();
		ctx->req = req;
		ctx->scratch = std::move(scratch);
		ctx->parent = this;

		// Whoever sets `done` first owns the scratch file: a completion that is
		// already under way keeps it until it has been read, otherwise
		// abandoning deletes it at once. Weak, or the request would own itself.
		req->abandon = [weak_ctx = std::weak_ptr<ctx_t>(ctx)]
		{
			if (const auto c = weak_ctx.lock(); c && !c->done.exchange(true)) c->scratch->discard();
		};

		{
			std::lock_guard lk(_mutex);
			_requests.push_back(req);
		}

		platform::async_http_callbacks cb;
		cb.on_headers = [ctx](const int status, std::string, uint64_t)
		{
			ctx->status_code = status;
		};
		cb.on_data = [ctx](const uint8_t* data, const size_t size)
		{
			ctx->scratch->write(data, size);
		};

		auto finish = [ctx](const uint32_t error)
		{
			if (ctx->done.exchange(true)) return;
			ctx->scratch->close();

			auto cb_user = ctx->req->callback;
			const auto status = static_cast<uint32_t>(ctx->status_code.load());
			auto* parent = ctx->parent;
			auto req = ctx->req;

			// Drop the request from the parent synchronously, while we still
			// know the parent is alive: this runs on an HTTP pool thread, which
			// the parent's destructor waits on via stop().
			if (parent)
			{
				std::lock_guard lk(parent->_mutex);
				std::erase(parent->_requests, req);
			}

			// The callback reads the body synchronously. The task holds the
			// scratch file until it has run, or until the UI queue drops it at
			// shutdown, and whichever owner lets go last deletes it.
			webvis::dispatch_to_ui([cb_user = std::move(cb_user), scratch = ctx->scratch, error, status, req]()
			{
				if (cb_user) cb_user(scratch->path().str(), error, status);
			});
		};

		cb.on_complete = [finish]() { finish(0); };
		cb.on_error = [finish](std::string) { finish(1); };

		auto async = _session->get(url, std::move(cb));
		if (!async)
		{
			// platform may already have reported the failure through on_error, in
			// which case `finish` has queued the completion and the caller must
			// not send a second one.
			const bool completion_queued = ctx->done.exchange(true);
			if (!completion_queued) ctx->scratch->discard();
			std::lock_guard lk(_mutex);
			std::erase(_requests, req);
			return completion_queued;
		}

		req->set_async(std::move(async));
		return true;
	}
};


// ── potato_host ──────────────────────────────────────────────────────────────

potato_host::potato_host() : _http(std::make_unique<http_client>()), _measure_surface(std::make_shared<core::surface>())
{
	_measure_surface->alloc(1, 1, core::texture_format::ARGB);
	_measure_context = render::create_surface_context(_measure_surface);
}

potato_host::~potato_host() = default;

bool potato_host::open_network(const std::string_view user_agent)
{
	return _http->open(user_agent);
}

void potato_host::stop_network()
{
	_http->stop();
}

render::measure_context* potato_host::metrics_context() const
{
	if (_layout_metrics) return _layout_metrics;
	return _measure_context.get();
}

webvis::font_handle potato_host::create_font(const webvis::font_desc& desc, webvis::font_metrics* metrics)
{
	render::font_desc def;
	def.family = windows_face(desc.face);
	def.size = std::max(1, desc.size);
	def.weight = desc.weight;
	def.italic = desc.italic;
	def.underline = desc.underline;
	def.strikeout = desc.strikeout;

	const auto face = render::make_font(def);
	if (metrics)
	{
		auto* mc = metrics_context();
		if (mc)
		{
			const auto m = render::metrics(*mc, face);
			metrics->height = m.height;
			metrics->x_height = m.x_height;
			metrics->ascent = m.ascent;
			metrics->descent = m.descent;
			metrics->draw_spaces = true;
		}
	}

	return static_cast<webvis::font_handle>(face);
}

void potato_host::destroy_font(const webvis::font_handle /*font*/)
{
	// render::make_font descriptors are process-owned and have no delete operation.
}

int potato_host::text_width(const webvis::font_handle font, const std::string_view text)
{
	if (!font) return 0;
	auto* mc = metrics_context();
	if (!mc) return 0;
	const auto face = static_cast<render::font_face>(font);

	// DirectWrite drops trailing whitespace from a measured run. A non-space
	// terminator keeps those spaces in the run; subtracting its own advance
	// gives the width GDI measured, which libwebvis lays words out by. Two rounded
	// DirectWrite runs otherwise add one pixel.
	if (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
	{
		std::string measured(text);
		measured.push_back('x');
		const auto with_terminator = mc->measure_text(measured, face, render::text_style::single_line, 1000000, 1000).cx;
		const auto terminator = mc->measure_text("x", face, render::text_style::single_line, 1000000, 1000).cx;
		return std::max(0, with_terminator - terminator - 1);
	}

	return mc->measure_text(text, face, render::text_style::single_line, 1000000, 1000).cx;
}

void potato_host::load_resource(const webvis::resource_kind kind, const std::string& url,
                                std::function<void(webvis::resource_result)> done)
{
	// A page opened from disk names its stylesheets and images by path, which
	// the platform HTTP client cannot fetch. Read them here instead, still completing later on
	// the UI thread as the engine requires.
	if (const auto path = local_resource_path(url); !path.is_empty())
	{
		webvis::dispatch_to_ui([kind, path, done = std::move(done)]
		{
			done(decode_resource(kind, path));
		});
		return;
	}

	const auto started = _http->download(
		url, [kind, done](const std::string& file_name, const uint32_t error, const uint32_t status)
		{
			if (error || status >= 400)
			{
				done(webvis::resource_result{});
				return;
			}

			done(decode_resource(kind, core::file_path(file_name)));
		});

	// A request that never launched still owes the caller a completion, or the
	// document would wait on it forever.
	if (!started) webvis::dispatch_to_ui([done] { done(webvis::resource_result{}); });
}

webvis::size potato_host::screen_size()
{
	const auto sz = platform::primary_screen_size();
	return {sz.cx, sz.cy};
}

int potato_host::screen_dpi()
{
	return platform::primary_screen_dpi();
}

std::string potato_host::resolve_url(const std::string_view base, const std::string_view rel)
{
	return platform::resolve_url(base, rel);
}

bool potato_host::transcode_to_utf8(const std::string_view bytes, const std::string_view charset, std::string& out)
{
	// The engine names these two explicitly when it detects a byte-order mark.
	// platform's charset table covers the labels that appear in markup, which these
	// are not, so they are mapped to their code pages here.
	uint32_t cp = 0;
	if (charset == "utf-16le") cp = 1200;
	else if (charset == "utf-16be") cp = 1201;
	else cp = platform::charset_to_codepage(charset);

	if (!cp) return false;
	out = platform::transcode_to_utf8(bytes, cp);
	return true;
}
