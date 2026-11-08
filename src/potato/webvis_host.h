// webvis_host.h - The libdesktop-backed implementations of libwebvis's host
// interfaces. libwebvis knows nothing about windows, networking or pixels; the
// browser supplies those services here.

#pragma once

#include "libdesktop/ui.h"
#include "webvis.h"


// The installed face for a CSS font family: a generic family such as
// "monospace" becomes a Windows font; any other name is returned as it is.
std::string windows_face(std::string_view css_family);

// The %TEMP% file a download is written to. It is deleted exactly once: when
// its last owner lets go - the completion that reads it, or that completion
// being dropped because the UI queue has shut down - or earlier by discard(),
// when a download is abandoned. Writes arrive on a platform HTTP thread, so they are
// serialised with discard().
class scratch_file
{
	std::mutex _mutex;
	core::file_path _path;
	platform::file_ptr _file;
	bool _deleted = false;

public:
	scratch_file(core::file_path path, platform::file_ptr file);
	~scratch_file();

	scratch_file(const scratch_file&) = delete;
	scratch_file& operator=(const scratch_file&) = delete;

	static std::shared_ptr<scratch_file> create(std::string_view prefix);

	const core::file_path& path() const { return _path; }
	void write(const uint8_t* data, size_t size);
	// No more data is coming; the file stays for whoever reads it.
	void close();
	// Closes and deletes the file now. Later writes are dropped, and the
	// destructor does not delete the name again: by then the system may have
	// handed it to another download.
	void discard();
};

// A decoded image whose pixels stay in the platform-neutral core surface.
class potato_image final : public webvis::image
{
	core::surface_ptr _surface;

public:
	explicit potato_image(core::surface_ptr surface) : _surface(std::move(surface))
	{
	}

	webvis::size dimensions() const override
	{
		if (!_surface) return {};
		return {static_cast<int>(_surface->width()), static_cast<int>(_surface->height())};
	}

	const core::surface_ptr& surface() const { return _surface; }
};

// Stateless drawing surface over a libdesktop render context. The origin keeps
// the web document's local coordinates separate from the frame's coordinates.
class potato_device_context final : public webvis::device_context
{
	render::draw_context& _dc;
	core::pointi _origin;
	std::vector<webvis::position> _clips;

	core::recti to_rect(const webvis::position& p) const;
	core::recti to_rect(const core::recti& r) const { return r.offset(_origin); }
	core::color to_color(const webvis::web_color& c) const;

public:
	explicit potato_device_context(render::draw_context& dc, core::pointi origin = {}) : _dc(dc), _origin(origin)
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

// Fonts, resource loading and environment queries. Layout has no draw context,
// so a tiny software surface supplies a persistent measure context for the
// headless path; a live frame replaces it while it lays out.
// Resource loading downloads to a temp file on a platform HTTP pool thread and then
// marshals the completion to the UI thread, so the callback libwebvis supplies
// always runs where the DOM lives. A resource of a page opened from disk is a
// local path, read directly but still completed on the UI thread.
class potato_host final : public webvis::host
{
	class http_client;
	std::unique_ptr<http_client> _http;
	core::surface_ptr _measure_surface;
	render::draw_context_device_ptr _measure_context;
	render::measure_context* _layout_metrics = nullptr;

public:
	potato_host();
	~potato_host() override;

	potato_host(const potato_host&) = delete;
	potato_host& operator=(const potato_host&) = delete;

	// Starts the HTTP session. Without it, load_resource fails every request,
	// which is exactly what the headless layout modes want.
	bool open_network(std::string_view user_agent);
	void stop_network();
	void set_measure_context(render::measure_context* context) { _layout_metrics = context; }

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

private:
	render::measure_context* metrics_context() const;
};