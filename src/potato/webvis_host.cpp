// webvis_host.cpp - Implements the libwebvis host interfaces over platform-h:
// GDI fonts, WIC image decoding, WinHTTP downloads and the screen metrics.
//
// The async HTTP client and the SVG placeholder generator used to live in the
// engine. They belong here: libwebvis asks for a resource by URL and is handed
// back either text or a decoded image, and never learns how that happened.

#include "pch.h"
#include "webvis_host.h"

#include <atomic>

namespace
{
	std::string file_contents(const std::string& file_name)
	{
		std::string result;
		std::ifstream in(file_name, std::ios::in | std::ios::binary);

		if (in)
		{
			in.seekg(0, std::ios::end);
			result.resize(in.tellg());
			in.seekg(0, std::ios::beg);
			in.read(result.data(), result.size());
		}

		return result;
	}

	std::string svg_attribute(const std::string& tag, const std::string& name)
	{
		size_t pos = 0;
		while ((pos = tag.find(name, pos)) != std::string::npos)
		{
			const bool valid_start = pos == 0 || isspace(static_cast<unsigned char>(tag[pos - 1]));
			size_t equals = pos + name.size();
			while (equals < tag.size() && isspace(static_cast<unsigned char>(tag[equals]))) ++equals;
			if (!valid_start || equals >= tag.size() || tag[equals] != '=')
			{
				pos += name.size();
				continue;
			}

			++equals;
			while (equals < tag.size() && isspace(static_cast<unsigned char>(tag[equals]))) ++equals;
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
	pf::bitmap_ptr create_svg_placeholder(const std::string& text)
	{
		std::string lower = pf::to_lower(text.substr(0, std::min<size_t>(text.size(), 8192)));
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
				while (*cursor && (isspace(static_cast<unsigned char>(*cursor)) || *cursor == ',')) ++cursor;
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
		std::vector<uint32_t> pixels(static_cast<size_t>(bitmap_width) * bitmap_height, 0xffeeeeee);
		const auto set_pixel = [&](const int x, const int y, const uint32_t color)
		{
			pixels[static_cast<size_t>(y) * bitmap_width + x] = color;
		};

		for (int x = 0; x < bitmap_width; ++x)
		{
			set_pixel(x, 0, 0xff999999);
			set_pixel(x, bitmap_height - 1, 0xff999999);
		}
		for (int y = 0; y < bitmap_height; ++y)
		{
			set_pixel(0, y, 0xff999999);
			set_pixel(bitmap_width - 1, y, 0xff999999);
			const int diagonal = bitmap_height > 1
				                     ? y * (bitmap_width - 1) / (bitmap_height - 1)
				                     : 0;
			set_pixel(diagonal, y, 0xffbbbbbb);
			set_pixel(bitmap_width - 1 - diagonal, y, 0xffbbbbbb);
		}

		return std::make_shared<pf::bitmap>(bitmap_width, bitmap_height, std::move(pixels));
	}

	pf::color_t to_pf_color(const webvis::web_color& c)
	{
		return pf::color_t(c.red, c.green, c.blue);
	}

	pf::irect to_pf_rect(const webvis::position& p)
	{
		return pf::irect(p.left(), p.top(), p.right(), p.bottom());
	}

	webvis::position intersect(const webvis::position& a, const webvis::position& b)
	{
		const int l = std::max(a.left(), b.left());
		const int t = std::max(a.top(), b.top());
		const int r = std::min(a.right(), b.right());
		const int bm = std::min(a.bottom(), b.bottom());
		return webvis::position(l, t, std::max(0, r - l), std::max(0, bm - t));
	}

	// The file a resource URL names when it is on this machine: what
	// pf::resolve_url makes of a reference relative to a page opened from disk.
	// Empty for anything that has to be fetched over the network.
	std::string local_resource_path(const std::string& url)
	{
		std::string path = url;
		if (path.size() >= 7 && _strnicmp(path.c_str(), "file://", 7) == 0) path = pf::resolve_url({}, path);

		const bool drive_path = path.size() >= 3 && std::isalpha(static_cast<unsigned char>(path[0])) &&
			path[1] == ':' && pf::file_path::is_path_sep(path[2]);
		if (!drive_path) return {};

		// A query or fragment is not part of the file name.
		const auto cut = path.find_first_of("?#");
		if (cut != std::string::npos) path.resize(cut);
		return path;
	}

	webvis::resource_result decode_resource(const webvis::resource_kind kind, const std::string& file_name)
	{
		webvis::resource_result result;

		if (kind == webvis::resource_kind::stylesheet)
		{
			result.text = file_contents(file_name);
			result.ok = !result.text.empty();
		}
		else
		{
			auto bitmap = pf::load_bitmap_file(pf::file_path(file_name));
			if (!bitmap) bitmap = create_svg_placeholder(file_contents(file_name));

			if (bitmap)
			{
				result.image = std::make_shared<potato_image>(std::move(bitmap));
				result.ok = true;
			}
		}

		return result;
	}
}


// ── potato_device_context ────────────────────────────────────────────────────

void potato_device_context::fill_rect(const webvis::position& pos, const webvis::web_color& color)
{
	_dc.fill_solid_rect(pos.x, pos.y, pos.width, pos.height, to_pf_color(color));
}

void potato_device_context::draw_text(const int x, const int y, const std::string_view text,
                                      const webvis::font_handle font, const webvis::web_color& color)
{
	_dc.draw_text_h(x, y, text, static_cast<pf::font_handle>(font), to_pf_color(color));
}

void potato_device_context::draw_image(const webvis::image_ptr& img, const webvis::position& pos)
{
	const auto native = std::static_pointer_cast<potato_image>(img);
	if (!native) return;

	const auto& bmp = native->bitmap();
	if (bmp && !bmp->empty())
	{
		_dc.draw_bitmap(to_pf_rect(pos), *bmp);
	}
}

void potato_device_context::draw_ellipse(const webvis::position& pos, const webvis::web_color& color,
                                         const int line_width)
{
	_dc.draw_ellipse(pos.x, pos.y, pos.width, pos.height, to_pf_color(color), line_width);
}

void potato_device_context::fill_ellipse(const webvis::position& pos, const webvis::web_color& color)
{
	_dc.fill_ellipse(pos.x, pos.y, pos.width, pos.height, to_pf_color(color));
}

void potato_device_context::push_clip(const webvis::position& pos)
{
	const auto clip = _clips.empty() ? pos : intersect(_clips.back(), pos);
	_clips.push_back(clip);
	_dc.set_clip_rect(to_pf_rect(clip));
}

void potato_device_context::pop_clip()
{
	if (_clips.empty()) return;

	_clips.pop_back();

	if (_clips.empty())
		_dc.clear_clip_rect();
	else
		_dc.set_clip_rect(to_pf_rect(_clips.back()));
}


// ── fonts ────────────────────────────────────────────────────────────────────

std::string windows_face(const std::string_view css_family)
{
	// The CSS generic families name no installed face, so GDI used to put its
	// own default in their place and <pre> and <code> came out proportional.
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
		if (name.size() == generic.size() && _strnicmp(name.data(), generic.data(), name.size()) == 0)
			return std::string(face);
	}

	return std::string(css_family);
}


// ── scratch_file ─────────────────────────────────────────────────────────────

scratch_file::scratch_file(std::string path, pf::writable_file_handle_ptr file)
	: _path(std::move(path)), _file(std::move(file))
{
}

scratch_file::~scratch_file()
{
	discard();
}

std::shared_ptr<scratch_file> scratch_file::create(const std::string_view prefix)
{
	auto path = pf::platform_temp_file_path(std::string(prefix).c_str());
	if (path.empty()) return nullptr;

	auto file = pf::open_file_for_write(pf::file_path(path));
	if (!file)
	{
		pf::platform_delete_file(pf::file_path(path));
		return nullptr;
	}

	return std::make_shared<scratch_file>(std::move(path), std::move(file));
}

void scratch_file::write(const uint8_t* data, const size_t size)
{
	std::lock_guard lk(_mutex);
	if (_file) _file->write(data, static_cast<uint32_t>(size));
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
	pf::platform_delete_file(pf::file_path(_path));
}


// ── the async HTTP client ────────────────────────────────────────────────────
//
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

		void set_async(pf::async_http_request_ptr a)
		{
			std::lock_guard lk(mutex);
			async = std::move(a);
		}

		callback_t callback;
		std::mutex mutex;
		pf::async_http_request_ptr async;
		std::function<void()> abandon; // set before the request is shared
	};

	pf::async_http_session_ptr _session;
	std::mutex _mutex;
	std::vector<std::shared_ptr<request>> _requests;

public:
	~http_client() { close(); }

	bool open(const std::string_view user_agent)
	{
		_session = pf::create_async_http_session(user_agent);
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
		if (!url.starts_with("http://") && !url.starts_with("https://"))
		{
			url = "https://" + url;
		}

		auto scratch = scratch_file::create("pot");
		if (!scratch) return false;

		struct ctx_t
		{
			std::shared_ptr<request> req;
			std::shared_ptr<scratch_file> scratch; // set once, before the request starts
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

		pf::async_http_callbacks cb;
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
				if (cb_user) cb_user(scratch->path(), error, status);
			});
		};

		cb.on_complete = [finish]() { finish(0); };
		cb.on_error = [finish](std::string) { finish(1); };

		auto async = _session->get(url, std::move(cb));
		if (!async)
		{
			// pf may already have reported the failure through on_error, in
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

potato_host::potato_host() : _http(std::make_unique<http_client>())
{
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

webvis::font_handle potato_host::create_font(const webvis::font_desc& desc, webvis::font_metrics* metrics)
{
	pf::font_def def;
	def.face = windows_face(desc.face);
	def.size = desc.size;
	def.weight = desc.weight;
	def.italic = desc.italic;
	def.underline = desc.underline;
	def.strikeout = desc.strikeout;

	pf::font_metrics_data m{};
	const auto handle = pf::create_font_handle(def, &m);

	if (metrics)
	{
		metrics->height = m.height;
		metrics->x_height = m.x_height;
		metrics->ascent = m.ascent;
		metrics->descent = m.descent;
		metrics->draw_spaces = true;
	}

	return static_cast<webvis::font_handle>(handle);
}

void potato_host::destroy_font(const webvis::font_handle font)
{
	if (font) pf::delete_font_handle(static_cast<pf::font_handle>(font));
}

int potato_host::text_width(const webvis::font_handle font, const std::string_view text)
{
	return pf::measure_text_with_font(static_cast<pf::font_handle>(font), text).cx;
}

void potato_host::load_resource(const webvis::resource_kind kind, const std::string& url,
                                std::function<void(webvis::resource_result)> done)
{
	// A page opened from disk names its stylesheets and images by path, which
	// WinHTTP cannot fetch. Read them here instead, still completing later on
	// the UI thread as the engine requires.
	if (auto path = local_resource_path(url); !path.empty())
	{
		webvis::dispatch_to_ui([kind, path = std::move(path), done = std::move(done)]
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

			done(decode_resource(kind, file_name));
		});

	// A request that never launched still owes the caller a completion, or the
	// document would wait on it forever.
	if (!started)
	{
		webvis::dispatch_to_ui([done] { done(webvis::resource_result{}); });
	}
}

webvis::size potato_host::screen_size()
{
	const auto sz = pf::platform_screen_size();
	return {sz.cx, sz.cy};
}

int potato_host::screen_dpi()
{
	return pf::platform_screen_dpi();
}

std::string potato_host::resolve_url(const std::string_view base, const std::string_view rel)
{
	return pf::resolve_url(base, rel);
}

bool potato_host::transcode_to_utf8(const std::string_view bytes, const std::string_view charset, std::string& out)
{
	// The engine names these two explicitly when it detects a byte-order mark.
	// pf's charset table covers the labels that appear in markup, which these
	// are not, so they are mapped to their code pages here.
	uint32_t cp = 0;
	if (charset == "utf-16le") cp = 1200;
	else if (charset == "utf-16be") cp = 1201;
	else cp = pf::charset_to_codepage(charset);

	if (!cp) return false;

	out = pf::transcode_to_utf8(bytes, cp);
	return true;
}
