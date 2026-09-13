// webvis.h - Public API of libwebvis, a standalone HTML/CSS layout and
// rendering engine.
//
// The library owns parsing, the cascade, layout and the decision of what to
// paint where. It owns no window, no socket and no drawing surface. Everything
// platform-specific reaches it through two interfaces the host implements:
//
//   device_context - a stateless drawing surface. Every call carries its own
//                    coordinates and colours; the context holds no pen, brush
//                    or transform that the engine is expected to set up first.
//                    Only painting goes through here, so layout runs headless.
//
//   host           - text measurement, resource loading and environment
//                    queries. Text measurement lives here rather than on the
//                    device context precisely so that layout needs no surface.
//
// Nothing in this header includes a platform header, and no type here is
// platform-specific.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace webvis
{
	using byte = unsigned char;

	struct margins
	{
		int left = 0;
		int right = 0;
		int top = 0;
		int bottom = 0;

		int width() const { return left + right; }
		int height() const { return top + bottom; }
	};

	struct size
	{
		int width = 0;
		int height = 0;
	};

	struct position
	{
		using vector = std::vector<position>;

		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;

		position() = default;

		position(const int x, const int y, const int width, const int height)
			: x(x), y(y), width(width), height(height)
		{
		}

		int right() const { return x + width; }
		int bottom() const { return y + height; }
		int left() const { return x; }
		int top() const { return y; }

		void operator+=(const margins& mg)
		{
			x -= mg.left;
			y -= mg.top;
			width += mg.left + mg.right;
			height += mg.top + mg.bottom;
		}

		void operator-=(const margins& mg)
		{
			x += mg.left;
			y += mg.top;
			width -= mg.left + mg.right;
			height -= mg.top + mg.bottom;
		}

		void clear()
		{
			x = y = width = height = 0;
		}

		void operator=(const size& sz)
		{
			width = sz.width;
			height = sz.height;
		}

		void move_to(const int x, const int y)
		{
			this->x = x;
			this->y = y;
		}

		bool does_intersect(const position* val) const
		{
			if (!val) return false;

			return
				left() <= val->right() &&
				right() >= val->left() &&
				bottom() >= val->top() &&
				top() <= val->bottom();
		}

		bool empty() const
		{
			return width == 0 && height == 0;
		}

		bool is_point_inside(const int x, const int y) const
		{
			return x >= left() && x <= right() && y >= top() && y <= bottom();
		}
	};

	struct web_color
	{
		byte blue;
		byte green;
		byte red;
		byte alpha;

		web_color(const byte r, const byte g, const byte b, const byte a = 255)
		{
			blue = b;
			green = g;
			red = r;
			alpha = a;
		}

		web_color()
		{
			blue = 0;
			green = 0;
			red = 0;
			alpha = 0xFF;
		}

		web_color(const web_color& val) = default;
		web_color& operator=(const web_color& val) = default;

		static web_color from_string(const char* str);
		static web_color from_string(const std::string& str) { return from_string(str.c_str()); }

		static bool is_color(const char* str);
		static bool is_color(const std::string& str) { return is_color(str.c_str()); }
	};

	struct font_metrics
	{
		int height = 0;
		int ascent = 0;
		int descent = 0;
		int x_height = 0;
		bool draw_spaces = true;

		int base_line() const { return descent; }
	};

	// An opaque, host-owned font. Zero is the null handle. The engine never
	// interprets the value; it only passes it back to the host for measurement
	// and to the device context for drawing.
	using font_handle = std::uintptr_t;

	struct font_desc
	{
		std::string face;
		int size = 0;
		int weight = 400;
		bool italic = false;
		bool underline = false;
		bool strikeout = false;
	};

	// A decoded image. The host owns the pixels and the decoding; the engine
	// only needs the intrinsic size in order to lay the image out, and the
	// handle itself in order to ask the device context to draw it.
	struct image
	{
		virtual ~image() = default;
		virtual webvis::size dimensions() const = 0;
	};

	using image_ptr = std::shared_ptr<image>;

	// A stateless drawing surface.
	//
	// Every operation is fully described by its arguments. There is no
	// "current" colour, font or origin to set beforehand, so the engine can
	// emit paint calls in any order and the host can back this with an
	// immediate-mode or a retained renderer as it prefers. Coordinates are in
	// device pixels, already translated into the surface's own space by the
	// caller.
	//
	// push_clip/pop_clip are the one piece of state, and they nest strictly.
	struct device_context
	{
		virtual ~device_context() = default;

		virtual void fill_rect(const position& pos, const web_color& color) = 0;

		// Draws with a transparent background; fill the background separately.
		virtual void draw_text(int x, int y, std::string_view text, font_handle font,
		                       const web_color& color) = 0;

		// Scales the image into `pos`.
		virtual void draw_image(const image_ptr& img, const position& pos) = 0;

		virtual void draw_ellipse(const position& pos, const web_color& color, int line_width) = 0;
		virtual void fill_ellipse(const position& pos, const web_color& color) = 0;

		// Intersects the current clip with `pos`. Strictly nested with pop_clip.
		virtual void push_clip(const position& pos) = 0;
		virtual void pop_clip() = 0;
	};

	enum class resource_kind
	{
		stylesheet,
		image,
	};

	struct resource_result
	{
		bool ok = false;
		std::string text; // populated for resource_kind::stylesheet
		image_ptr image; // populated for resource_kind::image
	};

	// Text measurement, resource loading and environment queries.
	//
	// Text measurement lives here rather than on device_context so that layout
	// can run without any drawing surface at all.
	struct host
	{
		virtual ~host() = default;

		// -- fonts and text measurement --
		virtual font_handle create_font(const font_desc& desc, font_metrics* metrics) = 0;
		virtual void destroy_font(font_handle font) = 0;
		virtual int text_width(font_handle font, std::string_view text) = 0;

		// -- resource loading --
		// Asynchronous. `done` must be invoked on the thread that drives the
		// document (the UI thread), never inline from another thread. A host
		// that cannot service the request calls `done` with ok == false.
		virtual void load_resource(resource_kind kind, const std::string& url,
		                           std::function<void(resource_result)> done) = 0;

		// The built-in user-agent stylesheet is compiled into the library, so
		// there is no hook for it here.

		// -- environment --
		virtual webvis::size screen_size() = 0;
		virtual int screen_dpi() = 0;

		// Resolves `rel` against `base`, returning an absolute URL or path.
		virtual std::string resolve_url(std::string_view base, std::string_view rel) = 0;

		// Decodes `bytes` from the named charset into UTF-8, writing the result
		// to `out`. Returns false when the label is not recognised, leaving
		// `out` untouched so the caller can fall back to its own heuristic.
		// The labels "utf-16le", "utf-16be" and "windows-1252" must be
		// supported, since the engine names them explicitly when it detects a
		// byte-order mark or needs the de-facto legacy default.
		virtual bool transcode_to_utf8(std::string_view bytes, std::string_view charset,
		                               std::string& out) = 0;
	};

	// The engine reaches its host through this global, mirroring the way the
	// platform functions it replaced were themselves global. It must be set
	// before any document is created and must outlive every document.
	void set_host(host* h);
	host* current_host();

	// Queues work onto the thread that drives the document (the UI thread).
	// Provided by the host application; the engine uses it to marshal
	// completions back from whatever thread they arrive on. It must queue,
	// never run the task inline and never wait for it.
	void dispatch_to_ui(std::function<void()> fn);

	// The user-agent stylesheet compiled into the library.
	std::string_view master_stylesheet();

	// Notifications from a document back to whatever is presenting it.
	// Implemented by the host's view; the defaults ignore everything optional.
	class view_host
	{
	public:
		virtual ~view_host() = default;
		virtual void layout() = 0;
		virtual void invalidate() = 0;
		virtual void open(const std::string& url) = 0;

		virtual void diagnostic(const std::string&)
		{
		}

		virtual void resource_started(const std::string&, const std::string&)
		{
		}

		virtual void resource_finished(const std::string&, const std::string&, bool)
		{
		}
	};
}
