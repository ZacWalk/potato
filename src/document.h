#pragma once
#include "style.h"
#include "types.h"
#include "xh_scanner.h"
#include "http.h"
#include "element.h"

class render_win32;
class html_view;
class element;



struct stop_tags_t
{
    const wchar_t* tags;
    const wchar_t* stop_parent;
};

struct ommited_end_tags_t
{
    const wchar_t* tag;
    const wchar_t* followed_tags;
};


class document : public std::enable_shared_from_this < document >
{
private:

    html_view &_view;

    element *m_root;
    std::map<std::wstring, font_item, ltstr> m_fonts;
    css m_styles;
    web_color m_def_color;
    size m_size;
    position _client_pos;

    position::vector m_fixed_boxes;
    std::vector<std::shared_ptr<media_query_list>> m_media_lists;
    element *m_over_element;

    http m_http;
    std::wstring m_url;
    std::wstring m_caption;
    std::wstring m_cursor;
    std::wstring m_base_path;

    std::map<std::wstring, std::shared_ptr<Gdiplus::Bitmap>, ltstr> m_images;

public:
    document(html_view &view);
    ~document();

    void clear();
    void load_master_stylesheet(const std::wstring &str);
    HFONT get_font(const std::wstring &name, int size, const std::wstring &weight, const std::wstring &style, const std::wstring &decoration, font_metrics* fm);
    int render(render_win32 &renderer, int max_width, render_type rt = render_all);
    void draw(render_win32 &renderer, int x, int y, const position* clip);

    web_color get_def_color() { return m_def_color; }

    static int cvt_units(const std::wstring &str, int fontSize, bool* is_percent = 0);
    static int cvt_units(css_length& val, int fontSize, int size = 0);
    static int pt_to_px(int pt);

    int width() const;
    int height() const;

    bool on_mouse_over(int x, int y, int client_x, int client_y, position::vector& redraw_boxes);
    bool on_lbutton_down(int x, int y, int client_x, int client_y, position::vector& redraw_boxes);
    bool on_lbutton_up(int x, int y, int client_x, int client_y, position::vector& redraw_boxes);
    bool on_mouse_leave(position::vector& redraw_boxes);

    element *root() { return m_root; };
    const position::vector& get_fixed_boxes() const { return m_fixed_boxes; }
    void add_fixed_box(const position& pos);
    void add_media_list(std::shared_ptr<media_query_list> list);
    bool media_changed();
    const std::wstring& url() const { return m_url; };

    void set_caption(const std::wstring &caption);
    void set_base_url(const std::wstring &base_url);
    void link(element *el);
    void import_css(const std::wstring& url, const std::wstring& baseurl);
    void on_anchor_click(const std::wstring &url, element *el);
    void set_cursor(const std::wstring &cursor);

    bool is_image_cached(const std::wstring &src, const std::wstring &baseurl);
    void load_image(const std::wstring &url, const std::wstring &base);
    std::shared_ptr<Gdiplus::Bitmap> find_image(const std::wstring &url);
    std::shared_ptr<Gdiplus::Bitmap> find_image(const std::wstring &url, const std::wstring &base);

    int text_width(const std::wstring &text, HFONT hFont);
    void delete_font(HFONT hFont);


    position client_pos() const { return _client_pos; };
    void client_pos(const position &pos) { _client_pos = pos; };
    void get_media_features(media_features& media);

    static int get_default_font_size()
    {
        return 16;
    }

    static const std::wstring get_default_font_name()
    {
        return _t("Times New Roman");
    }

    void set_root(element *r);
    void add_stylesheet(const std::wstring &text, const std::wstring &baseurl, const std::wstring &media);

    static std::shared_ptr<document> createFromUTF16(html_view &view, const std::wstring &url, const std::wstring &str);
    static std::shared_ptr<document> createFromUTF8(html_view &view, const std::wstring &url, const std::string &str);

    friend class html_view;

private:

    element *add_root();
    element *add_body();

    HFONT add_font(const std::wstring &name, int size, const std::wstring &weight, const std::wstring &style, const std::wstring &decoration, font_metrics* fm);

    bool update_media_lists(const media_features& features);
    void apply_stylesheet();
};

class parser
{
private:

    document &_doc;
    element *m_root;
    std::vector<element*> m_parse_stack;

    static stop_tags_t m_stop_tags [];
    static ommited_end_tags_t m_ommited_end_tags [];

public:

    parser(document &d) : _doc(d)
    {
        m_root = create_element(_t("html"));
        m_parse_stack.push_back(m_root);
    }

    bool is_stack_empty() const { return m_parse_stack.empty(); };
    element *root() { return m_root; };

    element *create_element(const std::wstring &tag_name);

    void parse_tag_start(const std::wstring &tag_name);
    void parse_tag_end(const std::wstring &tag_name);
    void parse_attribute(const std::wstring &attr_name, const std::wstring &attr_value);
    void parse_word(const std::wstring &val);
    void parse_space(const std::wstring &val);
    void parse_comment_start();
    void parse_comment_end();
    void parse_cdata_start();
    void parse_cdata_end();
    void parse_data(const std::wstring &val);
    void parse_push_element(element *el);
    bool parse_pop_element();
    bool parse_pop_element(const std::wstring &tag, const wchar_t *stop_tags = L"");
    void parse_pop_void_element();
    void parse_pop_to_parent(const std::wstring &parents, const std::wstring &stop_parent);
    void parse_close_omitted_end(const std::wstring &tag);
    void parse_open_omitted_start(const std::wstring &tag);
};

