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

template <class tstream>
class scanner
{
public:
	scanner(tstream& is) :
		m_token(TT_EOF),
		m_input(is),
		m_input_char(0),
		m_got_tail(false)
	{
		m_scan = &scanner::scan_body;
	}

	// get next token
	token_type get_token() { return (this->*m_scan)(); }

	// get value of TT_WORD, TT_SPACE, TT_ATTR and TT_DATA
	const std::string& get_value() const
	{
		return m_value;
	}

	const std::string& get_attr_name() const
	{
		return m_attr_name;
	}

	const std::string& get_tag_name() const
	{
		return m_tag_name;
	}

private: /* methods */

	using scan_function = token_type(scanner::*)();
	scan_function m_scan; // current 'reader'

	token_type scan_body()
	{
		wchar_t c = get_char();

		m_value.clear();

		bool ws = false;

		if (c == 0) return TT_EOF;
		if (c == '<') return scan_tag();
		if (c == '&')
			c = scan_entity();
		else
			ws = is_whitespace(c);

		if (!ws)
		{
			while (true)
			{
				append_value(c);

				// CJK character range
				if (c >= 0x4E00 && c <= 0x9FCC)
				{
					break;
				}

				c = get_char();
				if (c == 0)
				{
					push_back(c);
					break;
				}
				if (c == '<')
				{
					push_back(c);
					break;
				}
				if (c == '&')
				{
					push_back(c);
					break;
				}

				if (is_whitespace(c) != ws)
				{
					push_back(c);
					break;
				}
			}
		}
		else
		{
			while (ws)
			{
				append_value(c);
				c = get_char();
				ws = is_whitespace(c);
			}

			push_back(c);
			ws = true;
		}

		return ws ? TT_SPACE : TT_WORD;
	}

	token_type scan_head()
	{
		wchar_t c = skip_whitespace();

		if (c == '>')
		{
			if (is_equal(m_tag_name, "script"))
			{
				m_scan = &scanner::scan_raw_body;
				return scan_raw_body();
			}
			m_scan = &scanner::scan_body;
			return scan_body();
		}

		if (c == '/')
		{
			const wchar_t t = get_char();
			if (t == '>')
			{
				m_scan = &scanner::scan_body;
				return TT_TAG_END_EMPTY;
			}
			push_back(t);
			return TT_ERROR;
		}

		m_attr_name.clear();
		m_value.clear();

		// attribute name...
		while (c != '=')
		{
			if (c == 0) return TT_EOF;
			if (c == '>')
			{
				push_back(c);
				return TT_ATTR;
			} // attribute without value (HTML style)
			if (is_whitespace(c))
			{
				c = skip_whitespace();
				if (c != '=')
				{
					push_back(c);
					return TT_ATTR;
				} // attribute without value (HTML style)
				break;
			}
			if (c == '<') return TT_ERROR;
			append_attr_name(c);
			c = get_char();
		}

		c = skip_whitespace();
		// attribute value...

		if (c == '\"')
			while ((c = get_char()))
			{
				if (c == '\"') return TT_ATTR;
				if (c == '&') c = scan_entity();
				append_value(c);
			}
		else if (c == '\'') // allowed in html
			while ((c = get_char()))
			{
				if (c == '\'') return TT_ATTR;
				if (c == '&') c = scan_entity();
				append_value(c);
			}
		else // scan token, allowed in html: e.g. align=center
			do
			{
				if (is_whitespace(c)) return TT_ATTR;
				/* these two removed in favour of better html support:
				if( c == '/' || c == '>' ) { push_back(c); return TT_ATTR; }
				if( c == '&' ) c = scan_entity();*/
				if (c == '>')
				{
					push_back(c);
					return TT_ATTR;
				}
				append_value(c);
			}
			while ((c = get_char()));

		return TT_ERROR;
	}

	// caller already consumed '<'
	// scan header start or tag tail
	token_type scan_tag()
	{
		m_tag_name.clear();

		wchar_t c = get_char();

		const bool is_tail = c == '/';
		if (is_tail) c = get_char();

		while (c)
		{
			if (is_whitespace(c))
			{
				c = skip_whitespace();
				break;
			}
			if (c == '/' || c == '>') break;
			append_tag_name(c);

			switch (m_tag_name.length())
			{
			case 3:
				if (starts(m_tag_name, "!--"))
				{
					m_scan = &scanner::scan_comment;
					return TT_COMMENT_START;
				}
				break;
			case 8:
				if (starts(m_tag_name, "![cdata["))
				{
					m_scan = &scanner::scan_cdata;
					return TT_CDATA_START;
				}
				if (starts(m_tag_name, "!doctype"))
				{
					m_scan = &scanner::scan_entity_decl;
					return TT_DOCTYPE_START;
				}
				break;
			case 7:
				if (starts(m_tag_name, "!entity"))
				{
					m_scan = &scanner::scan_entity_decl;
					return TT_ENTITY_START;
				}
				break;
			}

			c = get_char();
		}

		if (c == 0) return TT_ERROR;

		if (is_tail)
		{
			if (c == '>') return TT_TAG_END;
			return TT_ERROR;
		}
		push_back(c);

		m_scan = &scanner::scan_head;
		return TT_TAG_START;
	}

	// skip whitespaces.
	// returns first non-whitespace char
	wchar_t skip_whitespace()
	{
		while (const wchar_t c = get_char())
		{
			if (!is_whitespace(c)) return c;
		}
		return 0;
	}

	void push_back(const wchar_t c)
	{
		m_input_char = c;
	}

	wchar_t get_char()
	{
		if (m_input_char)
		{
			const wchar_t t = m_input_char;
			m_input_char = 0;
			return t;
		}
		return m_input.get_char();
	}


	// caller consumed '&'
	wchar_t scan_entity()
	{
		std::string buf_ch;
		char buf[32];
		int i = 0;
		wchar_t t = 0;

		for (; i < 31; ++i)
		{
			t = get_char();

			if (t == ';')
				break;

			if (t == 0) return TT_EOF;
			if (!isalnum(t) && t != '#')
			{
				push_back(t);
				t = 0;
				break; // appears a erroneous entity token.
				// but we try to use it.
			}
			buf[i] = static_cast<char>(t);
			m_input.wchar_to_chars(t, buf_ch);
		}

		buf[i] = 0;

		if (is_equal(buf_ch, "gt")) return '>';
		if (is_equal(buf_ch, "lt")) return '<';
		if (is_equal(buf_ch, "amp")) return '&';
		if (is_equal(buf_ch, "apos")) return '\'';
		if (is_equal(buf_ch, "quot")) return '\"';

		const wchar_t entity = resolve_entity(buf_ch.c_str(), static_cast<int>(buf_ch.size()));

		if (entity)
		{
			return entity;
		}
		// no luck ...
		append_value('&');
		for (int n = 0; n < i; ++n)
			append_value(buf[n]);
		if (t) return t;
		return get_char();
	}

	bool is_whitespace(const wchar_t c)
	{
		return c <= ' '
			&& (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f');
	}

	void append_value(wchar_t c)
	{
		m_input.wchar_to_chars(c, m_value);
	}

	void append_attr_name(wchar_t c)
	{
		m_input.wchar_to_chars(c, m_attr_name);
	}

	void append_tag_name(wchar_t c)
	{
		m_input.wchar_to_chars(c, m_tag_name);
	}

	token_type scan_comment()
	{
		if (m_got_tail)
		{
			m_scan = &scanner::scan_body;
			m_got_tail = false;
			return TT_COMMENT_END;
		}
		m_value.clear();
		for (;;)
		{
			const wchar_t c = get_char();
			if (c == 0) return TT_EOF;
			append_value(c);

			if (m_value.length() >= 3
				&& m_value[m_value.length() - 1] == '>'
				&& m_value[m_value.length() - 2] == '-'
				&& m_value[m_value.length() - 3] == '-')
			{
				m_got_tail = true;
				m_value.erase(m_value.length() - 3);
				break;
			}
		}
		return TT_DATA;
	}

	token_type scan_cdata()
	{
		if (m_got_tail)
		{
			m_scan = &scanner::scan_body;
			m_got_tail = false;
			return TT_CDATA_END;
		}
		m_value.clear();
		for (;;)
		{
			const wchar_t c = get_char();
			if (c == 0) return TT_EOF;
			append_value(c);

			if (m_value.length() >= 3
				&& m_value[m_value.length() - 1] == '>'
				&& m_value[m_value.length() - 2] == ']'
				&& m_value[m_value.length() - 3] == ']')
			{
				m_got_tail = true;
				m_value.erase(m_value.length() - 3);
				break;
			}
		}
		return TT_DATA;
	}

	token_type scan_pi()
	{
		if (m_got_tail)
		{
			m_scan = &scanner::scan_body;
			m_got_tail = false;
			return TT_PI_END;
		}
		m_value.clear();
		for (;;)
		{
			const wchar_t c = get_char();
			if (c == 0) return TT_EOF;
			append_value(c);

			if (m_value.length() >= 2
				&& m_value[m_value.length() - 1] == '>'
				&& m_value[m_value.length() - 2] == '?')
			{
				m_got_tail = true;
				m_value.erase(m_value.length() - 2);
				break;
			}
		}
		return TT_DATA;
	}

	token_type scan_entity_decl()
	{
		if (m_got_tail)
		{
			m_scan = &scanner::scan_body;
			m_got_tail = false;
			return TT_ENTITY_END;
		}
		unsigned int tc = 0;
		m_value.clear();
		for (;;)
		{
			const wchar_t t = get_char();
			if (t == 0) return TT_EOF;
			append_value(t);
			if (t == '\"') tc++;
			else if (t == '>' && (tc & 1) == 0)
			{
				m_got_tail = true;
				break;
			}
		}
		return TT_DATA;
	}

	token_type scan_doctype_decl()
	{
		if (m_got_tail)
		{
			m_scan = &scanner::scan_body;
			m_got_tail = false;
			return TT_DOCTYPE_END;
		}
		unsigned int tc = 0;
		m_value.clear();
		for (;;)
		{
			const wchar_t t = get_char();
			if (t == 0) return TT_EOF;
			append_value(t);
			if (t == '\"') tc++;
			else if (t == '>' && (tc & 1) == 0)
			{
				m_got_tail = true;
				break;
			}
		}
		return TT_DATA;
	}

	wchar_t resolve_entity(const char* buf, const int buf_size)
	{
		wchar_t wres = 0;
		if (buf[0] == '#')
		{
			if (buf[1] == 'x' || buf[1] == 'X')
			{
				wres = static_cast<wchar_t>(safe_stol(buf + 2, 16));
			}
			else
			{
				wres = static_cast<wchar_t>(safe_stoi(buf + 1));
			}
		}
		else
		{
			std::string str = "&";
			str.append(buf, buf_size);
			str += ";";

			for (int i = 0; g_html_entities[i].szCode[0]; i++)
			{
				if (is_equal(str, g_html_entities[i].szCode))
				{
					wres = g_html_entities[i].Code;
					break;
				}
			}
		}
		return wres;
	}

	token_type scan_raw_body()
	{
		if (m_got_tail)
		{
			m_scan = &scanner::scan_body;
			m_got_tail = false;
			return TT_TAG_END;
		}
		m_value.clear();
		for (;;)
		{
			const wchar_t c = get_char();
			if (c == 0) return TT_EOF;
			append_value(c);

			if (m_value.length() >= 9 && !_strnicmp(m_value.c_str() + m_value.length() - 9, "</script>", 9))
			{
				m_got_tail = true;
				m_value.erase(m_value.length() - 9);
				break;
			}
		}
		return TT_DATA;
	}

	token_type m_token;

	std::string m_value;
	std::string m_tag_name;
	std::string m_attr_name;

	tstream& m_input;
	wchar_t m_input_char;

	bool m_got_tail; // aux flag used in scan_comment, etc. 
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
	std::string m_url;
	std::string m_caption;
	std::string m_cursor;
	std::string m_base_path;

	std::map<std::string, pf::bitmap_ptr, ltstr> m_images;

public:
	std::unique_ptr<element> m_parsed_root;

	document(view_host& view);
	~document();

	void clear();
	void load_master_stylesheet(const std::string& str);
	pf::font_handle get_font(const std::string& name, int size, const std::string& weight, const std::string& style,
	                         const std::string& decoration, font_metrics* fm);
	int render(render_win32& renderer, int max_width, render_type rt = render_all);
	void draw(render_win32& renderer, int x, int y, const position* clip);

	web_color get_def_color() { return m_def_color; }

	static int cvt_units(const std::string& str, int fontSize, bool* is_percent = nullptr);
	static int cvt_units(css_length& val, int fontSize, int size = 0);
	static int pt_to_px(int pt);

	int width() const;
	int height() const;

	bool on_mouse_over(int x, int y, int client_x, int client_y, position::vector& redraw_boxes);
	bool on_lbutton_down(int x, int y, int client_x, int client_y, position::vector& redraw_boxes);
	bool on_lbutton_up(int x, int y, int client_x, int client_y, position::vector& redraw_boxes);
	bool on_mouse_leave(position::vector& redraw_boxes);

	element* root() { return m_root.get(); };
	const position::vector& get_fixed_boxes() const { return m_fixed_boxes; }
	void add_fixed_box(const position& pos);
	void add_media_list(const std::shared_ptr<media_query_list>& list);
	bool media_changed();
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

	int text_width(const std::string& text, pf::font_handle hFont);
	void delete_font(pf::font_handle hFont);


	position client_pos() const { return m_client_pos; };
	void client_pos(const position& pos) { m_client_pos = pos; };
	void get_media_features(media_features& media);

	static int get_default_font_size()
	{
		return 16;
	}

	static const std::string get_default_font_name()
	{
		return "Times New Roman";
	}

	void set_root(std::unique_ptr<element> r);
	void resolve_styles();
	void finalize();
	void add_stylesheet(const std::string& text, const std::string& baseurl, const std::string& media);

	static std::shared_ptr<document> create_from_utf16(view_host& view, const std::string& url,
	                                                   const std::string& str);
	static std::shared_ptr<document> create_from_utf8(view_host& view, const std::string& url, const std::string& str);

	static std::shared_ptr<document>
	parse_from_utf16(view_host& view, const std::string& url, const std::string& str);
	static std::shared_ptr<document> parse_from_utf8(view_host& view, const std::string& url, const std::string& str);

	friend class html_view;

private:
	element* add_root();
	element* add_body();

	pf::font_handle add_font(const std::string& name, int size, const std::string& weight, const std::string& style,
	                         const std::string& decoration, font_metrics* fm);

	bool update_media_lists(const media_features& features);
	void update_styles(element* root_el);
	void apply_stylesheet();
};

class parser
{
	document& m_doc;
	std::unique_ptr<element> m_root;
	std::vector<element*> m_parse_stack;

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

	std::unique_ptr<element> create_element(const std::string& tag_name);

	void parse_tag_start(const std::string& tag_name);
	void parse_tag_end(const std::string& tag_name);
	void parse_attribute(const std::string& attr_name, const std::string& attr_value);
	void parse_word(const std::string& val);
	void parse_space(const std::string& val);
	void parse_comment_start();
	void parse_comment_end();
	void parse_cdata_start();
	void parse_cdata_end();
	void parse_data(const std::string& val);
	void parse_push_element(std::unique_ptr<element> el);
	bool parse_pop_element();
	bool parse_pop_element(const std::string& tag, const char* stop_tags = "");
	void parse_pop_void_element();
	void parse_pop_to_parent(const std::string& parents, const std::string& stop_parent);
	void parse_close_omitted_end(const std::string& tag);
	void parse_open_omitted_start(const std::string& tag);
};
