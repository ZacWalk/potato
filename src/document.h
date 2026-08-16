// document.h - Async HTTP client (WinHTTP), HTML scanner/tokenizer, HTML parser
// with implicit tag closing, and the document model that owns the DOM tree and
// manages fonts and stylesheets.

#pragma once
#include "platform.h"
#include "core.h"
#include "element.h"


class document;

// File-callback wrapper. Each request downloads to a temp file and then
// invokes a single completion callback with (file_path, error_code, http_status, url).
// The implementation rides on top of pf::async_http_session.
class http_request : public std::enable_shared_from_this<http_request>
{
public:
	using callback_t = std::function<void(const std::string& file, uint32_t error, uint32_t httpStatus,
	                                      const std::string& url)>;

	explicit http_request(callback_t callback) : m_callback(std::move(callback))
	{
	}

	~http_request() = default;

	void cancel()
	{
		std::lock_guard lk(m_mutex);
		if (m_async) m_async->cancel();
	}

	// Internal — set by http when the request is launched.
	void set_async(pf::async_http_request_ptr a)
	{
		std::lock_guard lk(m_mutex);
		m_async = std::move(a);
	}

	callback_t m_callback;
	std::mutex m_mutex;
	pf::async_http_request_ptr m_async;
};

// Async HTTP — owns a pf::async_http_session and tracks in-flight requests
// so they can be cancelled together at shutdown.
class http
{
	pf::async_http_session_ptr m_session;
	std::mutex m_mutex;
	std::vector<std::shared_ptr<http_request>> m_requests;

public:
	http() = default;
	~http() { close(); }

	http(const http&) = delete;
	http& operator=(const http&) = delete;

	bool open(std::string_view user_agent);
	bool download_file(const std::string& url, const std::shared_ptr<http_request>& request);
	void stop();
	void close();
};


//| 
//| simple and fast XML/HTML scanner/tokenizer
//|
//| (C) Andrew Fedoniouk @ terrainformatica.com
//|


struct html_entities
{
	char szCode[20];
	wchar_t Code;
};

extern html_entities g_html_entities[];

enum token_type
{
	TT_ERROR = -1,
	TT_EOF = 0,

	TT_TAG_START, // <tag ...
	TT_TAG_END, // </tag>
	TT_TAG_END_EMPTY, // <tag ... />
	TT_ATTR, // <tag attr="value" >  
	TT_WORD,
	TT_SPACE,

	TT_DATA, // content of followings:

	TT_COMMENT_START, TT_COMMENT_END, // after "<!--" and "-->"
	TT_CDATA_START, TT_CDATA_END, // after "<![CDATA[" and "]]>"
	TT_PI_START, TT_PI_END, // after "<?" and "?>"
	TT_ENTITY_START, TT_ENTITY_END, // after "<!ENTITY" and ">"
	TT_DOCTYPE_START, TT_DOCTYPE_END, // after "<!DOCTYPE" and ">"
};

// Zero-copy HTML tokenizer. Walks a UTF-8 buffer owned by the caller (the
// document's source text) and returns string_views into it. Values that needed
// entity decoding point into an internal scratch buffer valid until the next
// get_token() call.
class html_scanner
{
public:
	explicit html_scanner(const std::string_view src) : m_src(src)
	{
	}

	token_type get_token() { return (this->*m_scan)(); }

	std::string_view get_value() const { return m_value; }
	std::string_view get_tag_name() const { return m_tag_name; }
	std::string_view get_attr_name() const { return m_attr_name; }

private:
	using scan_function = token_type (html_scanner::*)();

	std::string_view m_src;
	size_t m_pos = 0;

	std::string_view m_value;
	std::string_view m_tag_name;
	std::string_view m_attr_name;

	std::string m_decoded; // scratch for values containing entities
	std::string m_tag_store; // lowercased tag name
	std::string m_attr_store; // lowercased attribute name

	scan_function m_scan = &html_scanner::scan_text;
	std::string_view m_close; // closing delimiter for the current raw region
	token_type m_end_token = TT_EOF; // token emitted once that region ends
	bool m_got_tail = false;

	static bool is_ws(const char c)
	{
		return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
	}

	static char lower(const char c)
	{
		return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
	}

	bool at_end() const { return m_pos >= m_src.size(); }
	char peek() const { return m_pos < m_src.size() ? m_src[m_pos] : '\0'; }

	// Byte length of the UTF-8 sequence starting at `at`.
	size_t seq_len(const size_t at) const
	{
		const auto b = static_cast<uint8_t>(m_src[at]);
		const size_t n = b < 0x80 ? 1 : (b & 0xE0) == 0xC0 ? 2 : (b & 0xF0) == 0xE0 ? 3 : (b & 0xF8) == 0xF0 ? 4 : 1;
		return std::min(n, m_src.size() - at);
	}

	uint32_t codepoint_at(const size_t at) const
	{
		const size_t n = seq_len(at);
		const auto b0 = static_cast<uint8_t>(m_src[at]);
		if (n == 1) return b0;
		uint32_t cp = b0 & (0xFF >> (n + 1));
		for (size_t i = 1; i < n; ++i)
			cp = (cp << 6) | (static_cast<uint8_t>(m_src[at + i]) & 0x3F);
		return cp;
	}

	void skip_ws()
	{
		while (m_pos < m_src.size() && is_ws(m_src[m_pos])) ++m_pos;
	}

	bool starts_ci(size_t at, std::string_view with) const;
	size_t find_ci(std::string_view needle, size_t from) const;

	// m_pos points just past '&' on entry. Appends the replacement to `out`.
	bool decode_entity(std::string& out);
	void scan_attr_value();
	token_type enter_content();

	token_type scan_text();
	token_type scan_tag();
	token_type scan_attributes();
	token_type scan_raw_text();
	token_type scan_delimited();
	token_type scan_markup_decl();
};


class render_win32;
class element;


struct stop_tags_t
{
	const char* tags;
	const char* stop_parent;
};

struct omitted_end_tags_t
{
	const char* tag;
	const char* followed_tags;
};


// A structural summary of a laid-out tree. Everything here is derived from box
// geometry alone, so it can be gathered headlessly and compared between runs.
struct layout_stats
{
	int elements = 0;
	int text_nodes = 0;
	int images = 0;
	int max_depth = 0;

	// Right-most edge any visible box reaches. Should stay within the viewport
	// for a page that lays out correctly at this width.
	int right_edge = 0;

	int overflow_x = 0; // boxes extending past the viewport
	int negative_x = 0; // boxes starting left of the origin
	int zero_area_text = 0; // text that was laid out with no area to draw in
	int unsized_image = 0; // <img> that ended up with no box
	int negative_size = 0; // boxes with a negative width or height
	int hidden_subtrees = 0; // display:none roots, not descended into
};


class document : public std::enable_shared_from_this<document>
{
	view_host& m_view;

	std::shared_ptr<element> m_root;
	std::map<std::string, font_item, ltstr> m_fonts;
	std::mutex m_fonts_mutex;
	css m_styles;
	web_color m_def_color;
	size m_size;
	position m_client_pos;

	position::vector m_fixed_boxes;
	std::vector<std::shared_ptr<media_query_list>> m_media_lists;
	element* m_over_element;

	http m_http;
	std::string m_source; // decoded UTF-8 page text; the DOM points into this
	std::string m_url;
	std::string m_caption;
	std::string m_cursor;
	std::string m_base_path;

	// UI thread only: set while a coalesced restyle is already queued.
	bool m_restyle_pending = false;

	std::map<std::string, pf::bitmap_ptr, ltstr> m_images;

public:
	document(view_host& view);
	~document();

	void clear();
	void load_master_stylesheet(const std::string& str);
	pf::font_handle get_font(const std::string& name, int size, const std::string& weight, const std::string& style,
	                         const std::string& decoration, font_metrics* fm);
	int render(int max_width, render_type rt = render_all);
	void draw(render_win32& renderer, int x, int y, const position* clip);

	web_color get_def_color() { return m_def_color; }

	static int cvt_units(const std::string& str, int fontSize, bool* is_percent = nullptr);
	static int cvt_units(css_length& val, int fontSize, int size = 0);
	static int pt_to_px(int pt);

	int width() const;
	int height() const;
	void diagnose_layout() const;
	layout_stats analyse_layout(std::vector<std::string>* samples = nullptr) const;
	std::vector<std::string> dump_boxes(int max_depth) const;
	std::string dump_layout_json() const;

	bool on_mouse_over(int x, int y, int client_x, int client_y, position::vector& redraw_boxes);
	bool on_lbutton_down(int x, int y, int client_x, int client_y, position::vector& redraw_boxes);
	bool on_lbutton_up(int x, int y, int client_x, int client_y, position::vector& redraw_boxes);
	bool on_mouse_leave(position::vector& redraw_boxes);

	element* root() { return m_root.get(); };
	void add_fixed_box(const position& pos);
	void add_media_list(const std::shared_ptr<media_query_list>& list);
	const std::string& url() const { return m_url; };

	void set_caption(const std::string& caption);
	void set_base_url(const std::string& base_url);
	void link(const element* el);
	void import_css(const std::string& url, const std::string& baseurl, const std::string& media = empty);
	void on_anchor_click(const std::string& url, element* el);
	void set_cursor(const std::string& cursor);
	const std::string& cursor() const { return m_cursor; }

	bool is_image_cached(const std::string& src, const std::string& baseurl);
	void load_image(const std::string& url, const std::string& base);
	pf::bitmap_ptr find_image(const std::string& url);
	pf::bitmap_ptr find_image(const std::string& url, const std::string& base);

	int text_width(std::string_view text, pf::font_handle hFont);


	position client_pos() const { return m_client_pos; };
	void client_pos(const position& pos) { m_client_pos = pos; };
	void get_media_features(media_features& media);

	static int get_default_font_size()
	{
		return 16;
	}

	static const std::string& get_default_font_name()
	{
		static const std::string name = "Times New Roman";
		return name;
	}

	void set_root(std::unique_ptr<element> r);
	void add_stylesheet(const std::string& text, const std::string& baseurl, const std::string& media);

	// Parse `bytes` (raw, any encoding) as the document source. The decoded
	// UTF-8 text is retained for the lifetime of the document so the DOM can
	// reference it directly.
	static std::shared_ptr<document> create_from_bytes(view_host& view, const std::string& url,
	                                                   std::string_view bytes, std::string_view content_type = {});

	friend class html_view;

private:
	pf::font_handle add_font(const std::string& name, int size, const std::string& weight, const std::string& style,
	                         const std::string& decoration, font_metrics* fm);

	bool update_media_lists(const media_features& features);
	void update_styles(element* root_el);
	void apply_stylesheet();
	void request_restyle();
};


struct layout_result
{
	int width = 0;
	int height = 0;
	int64_t parse_style_us = 0;
	int64_t layout_us = 0;
	layout_stats stats;
	std::vector<std::string> anomalies;
	std::vector<std::string> box_dump;
	std::string layout_json;
};

layout_result layout_html_headless(const std::string& html, int width, int height, bool verbose = false,
                                   int dump_depth = 0, bool dump_json = false);

// Lays out a snippet and returns the box of the element with the given id, in
// document coordinates. An empty box means the id was not found.
position layout_html_headless_probe(const std::string& html, int width, const std::string& id);


class parser
{
	document& m_doc;
	std::unique_ptr<element> m_root;
	std::vector<element*> m_parse_stack;

	// Nodes the tree refused. They stay on the parse stack so tag nesting keeps
	// its shape, so they have to outlive the parse.
	std::vector<std::unique_ptr<element>> m_detached;

	static stop_tags_t m_stop_tags[];
	static omitted_end_tags_t m_omitted_end_tags[];

public:
	parser(document& d) : m_doc(d)
	{
		m_root = create_element("html");
		m_parse_stack.push_back(m_root.get());
	}

	bool is_stack_empty() const { return m_parse_stack.empty(); };
	std::unique_ptr<element> release_root() { return std::move(m_root); };

	std::unique_ptr<element> create_element(std::string_view tag_name);

	void parse_tag_start(std::string_view tag_name);
	void parse_tag_end(std::string_view tag_name);
	void parse_attribute(std::string_view attr_name, std::string_view attr_value);
	void parse_word(std::string_view val);
	void parse_space(std::string_view val);
	void parse_comment_start();
	void parse_comment_end();
	void parse_cdata_start();
	void parse_cdata_end();
	void parse_data(std::string_view val);
	void parse_push_element(std::unique_ptr<element> el);
	bool parse_pop_element();
	bool parse_pop_element(std::string_view tag, const char* stop_tags = "");
	void parse_pop_void_element();
	void parse_pop_to_parent(const char* parents, const char* stop_parent);
	void parse_close_omitted_end(std::string_view tag);
	void parse_open_omitted_start(std::string_view tag);
};
