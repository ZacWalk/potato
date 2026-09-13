// webvis_host.h - The platform-h backed implementations of the libwebvis host
// interfaces. libwebvis knows nothing about Win32, WinHTTP or WIC; everything
// it needs from the machine arrives through the two classes declared here.
//
//   potato_device_context - webvis::device_context over a pf::draw_context
//   potato_host           - webvis::host: fonts, resource loading, environment

#pragma once

#include "platform.h"
#include "webvis.h"

// A decoded image. Wraps the platform bitmap so libwebvis can ask for its
// intrinsic size without knowing what a pf::bitmap is.
class potato_image final : public webvis::image
{
	pf::bitmap_ptr _bitmap;

public:
	explicit potato_image(pf::bitmap_ptr bitmap) : _bitmap(std::move(bitmap))
	{
	}

	webvis::size dimensions() const override
	{
		if (!_bitmap) return {};
		return {_bitmap->width, _bitmap->height};
	}

	const pf::bitmap_ptr& bitmap() const { return _bitmap; }
};

// Stateless drawing surface over a pf::draw_context.
//
// The clip stack is kept here rather than in the platform layer: pf's
// set_clip_rect replaces rather than intersects, so this computes the
// intersection itself and restores the enclosing rect on pop.
class potato_device_context final : public webvis::device_context
{
	pf::draw_context& _dc;
	std::vector<webvis::position> _clips;

public:
	explicit potato_device_context(pf::draw_context& dc) : _dc(dc)
	{
	}

	void fill_rect(const webvis::position& pos, const webvis::web_color& color) override;
	void draw_text(int x, int y, std::string_view text, webvis::font_handle font,
	               const webvis::web_color& color) override;
	void draw_image(const webvis::image_ptr& img, const webvis::position& pos) override;
	void draw_ellipse(const webvis::position& pos, const webvis::web_color& color, int line_width) override;
	void fill_ellipse(const webvis::position& pos, const webvis::web_color& color) override;
	void push_clip(const webvis::position& pos) override;
	void pop_clip() override;
};

// Fonts, resource loading and environment queries.
//
// Resource loading downloads to a temp file on a WinHTTP pool thread and then
// marshals the completion to the UI thread, so the callback libwebvis supplies
// always runs where the DOM lives.
class potato_host final : public webvis::host
{
	class http_client;
	std::unique_ptr<http_client> _http;

public:
	potato_host();
	~potato_host() override;

	potato_host(const potato_host&) = delete;
	potato_host& operator=(const potato_host&) = delete;

	// Starts the HTTP session. Without it, load_resource fails every request,
	// which is exactly what the headless layout modes want.
	bool open_network(std::string_view user_agent);
	void stop_network();

	// -- webvis::host --
	webvis::font_handle create_font(const webvis::font_desc& desc, webvis::font_metrics* metrics) override;
	void destroy_font(webvis::font_handle font) override;
	int text_width(webvis::font_handle font, std::string_view text) override;

	void load_resource(webvis::resource_kind kind, const std::string& url,
	                   std::function<void(webvis::resource_result)> done) override;

	webvis::size screen_size() override;
	int screen_dpi() override;
	std::string resolve_url(std::string_view base, std::string_view rel) override;
	bool transcode_to_utf8(std::string_view bytes, std::string_view charset, std::string& out) override;
};
