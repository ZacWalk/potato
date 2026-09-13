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


// ── the async HTTP client ────────────────────────────────────────────────────
//
// Each request downloads to a temp file and then delivers one completion on the
// UI thread. In-flight requests are tracked so they can all be cancelled at
// shutdown.

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
			std::lock_guard lk(mutex);
			if (async) async->cancel();
		}

		void set_async(pf::async_http_request_ptr a)
		{
			std::lock_guard lk(mutex);
			async = std::move(a);
		}

		callback_t callback;
		std::mutex mutex;
		pf::async_http_request_ptr async;
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

		const std::string temp_path = pf::platform_temp_file_path("pot");
		auto file = pf::open_file_for_write(pf::file_path(temp_path));
		if (!file)
		{
			pf::platform_delete_file(pf::file_path(temp_path));
			return false;
		}

		auto req = std::make_shared<request>(std::move(callback));
		{
			std::lock_guard lk(_mutex);
			_requests.push_back(req);
		}

		struct ctx_t
		{
			std::shared_ptr<request> req;
			pf::writable_file_handle_ptr file;
			std::string file_path;
			http_client* parent = nullptr;
			std::atomic<int> status_code{0};
			std::atomic<bool> done{false};
		};

		auto ctx = std::make_shared<ctx_t>();
		ctx->req = req;
		ctx->file = std::move(file);
		ctx->file_path = temp_path;
		ctx->parent = this;

		pf::async_http_callbacks cb;
		cb.on_headers = [ctx](const int status, std::string, uint64_t)
		{
			ctx->status_code = status;
		};
		cb.on_data = [ctx](const uint8_t* data, const size_t size)
		{
			if (ctx->file) ctx->file->write(data, static_cast<uint32_t>(size));
		};

		auto finish = [ctx](const uint32_t error)
		{
			if (ctx->done.exchange(true)) return;
			ctx->file.reset();

			auto cb_user = ctx->req->callback;
			auto file_path = ctx->file_path;
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

			webvis::dispatch_to_ui([cb_user = std::move(cb_user), file_path = std::move(file_path),
					error, status, req]()
				{
					if (cb_user) cb_user(file_path, error, status);
					// The callback reads the body synchronously, so the
					// download's scratch file has no readers left once it
					// returns.
					pf::platform_delete_file(pf::file_path(file_path));
				});
		};

		cb.on_complete = [finish]() { finish(0); };
		cb.on_error = [finish](std::string) { finish(1); };

		auto async = _session->get(url, std::move(cb));
		if (!async)
		{
			ctx->file.reset();
			pf::platform_delete_file(pf::file_path(temp_path));
			std::lock_guard lk(_mutex);
			std::erase(_requests, req);
			return false;
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
	def.face = desc.face;
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
	const auto started = _http->download(
		url, [kind, done](const std::string& file_name, const uint32_t error, const uint32_t status)
		{
			webvis::resource_result result;

			if (error || status >= 400)
			{
				done(result);
				return;
			}

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

			done(result);
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
