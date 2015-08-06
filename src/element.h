#pragma once
#include "stylesheet.h"
#include "css.h"
#include "web_color.h"
#include "style.h"
#include "background.h"
#include "borders.h"
#include "stylesheet.h"
#include "box.h"
#include "table.h"

class box;
class background;
class render_win32;

enum element_type
{
    el_html,
    el_anchor,
    el_base,
    el_before,
    el_after,
    el_body,
    el_break,
    el_cdata,
    el_comment,
    el_div,
    el_font,
    el_image,
    el_link,
    el_para,
    el_script,
    el_space,
    el_style,
    el_table,
    el_td,
    el_text,
    el_title,
    el_tr
};



class element
{
    friend class box;
    friend class el_table;
    friend class element;
    friend class table_grid;

protected:

    enum element_type _type;
    element *m_parent;
    document &m_doc;
    
    position m_pos;
    margins m_margins;
    margins m_padding;
    margins m_borders;
    bool m_skip;
    bool _loaded;
    std::vector<element*> m_children;

    std::wstring m_id;
    std::wstring m_class;
    std::wstring m_text;
    std::wstring m_transformed_text;
    size m_size;
    text_transform m_text_transform;
    bool m_use_transformed;
    bool m_draw_spaces;
    std::wstring m_src;
    std::wstring m_tag;
    style m_style;
    std::map<std::wstring, std::wstring, ltstr> m_attrs;
    vertical_align m_vertical_align;
    text_align m_text_align;
    style_display m_display;
    list_style_type m_list_style_type;
    list_style_position m_list_style_position;
    white_space m_white_space;
    element_float m_float;
    element_clear m_clear;
    std::vector<floated_box> m_floats_left;
    std::vector<floated_box> m_floats_right;
    std::vector<element*> m_positioned;
    background m_bg;
    element_position m_el_position;
    int m_line_height;
    bool m_lh_predefined;
    std::vector<std::wstring> m_pseudo_classes;
    std::vector<used_selector> m_used_styles;

    box* m_box;
    std::vector<box*> m_boxes;

    HFONT m_font;
    int m_font_size;
    font_metrics m_font_metrics;

    css_margins m_css_margins;
    css_margins m_css_padding;
    css_borders m_css_borders;
    css_length m_css_width;
    css_length m_css_height;
    css_length m_css_min_width;
    css_length m_css_min_height;
    css_length m_css_max_width;
    css_length m_css_max_height;
    css_offsets m_css_offsets;
    css_length m_css_text_indent;

    overflow m_overflow;
    visibility m_visibility;
    int m_z_index;
    box_sizing m_box_sizing;

    int_int_cache m_cahe_line_left;
    int_int_cache m_cahe_line_right;

    // table
    table_grid m_grid;
    css_length m_css_border_spacing_x;
    css_length m_css_border_spacing_y;
    int m_border_spacing_x;
    int m_border_spacing_y;
    border_collapse m_border_collapse;

public:

    element(document &doc, enum element_type, const std::wstring &text = empty);
    ~element();

    bool collapse_bottom_margin() const;
    bool collapse_top_margin() const;
    bool in_normal_flow() const;
    bool is_inline_box() const;
    bool is_positioned() const;
    bool is_visible() const;
    bool skip();
    element *parent() const { return m_parent; }
    std::vector<element*>& children();
    int border_bottom() const;
    int border_left() const;
    int border_right() const;
    int border_top() const;
    int bottom() const;
    int calc_width(int defVal) const;
    int content_margins_bottom() const;
    int content_margins_left() const;
    int content_margins_right() const;
    int content_margins_top() const;
    int finish_last_box(bool end_of_render = false);
    int get_cleared_top(element *el, int line_top);
    int get_inline_shift_left();
    int get_inline_shift_right();
    int height() const;
    int left() const;
    int margin_bottom() const;
    int margin_left() const;
    int margin_right() const;
    int margin_top() const;
    int new_box(element *el, int max_width);
    int padding_bottom() const;
    int padding_left() const;
    int padding_right() const;
    int padding_top() const;
    int right() const;
    int top() const;
    int width() const;
    margins get_borders() const;
    margins get_margins() const;
    margins get_paddings() const;
    position get_placement() const;
    HFONT get_font(font_metrics* fm = 0);
    background* get_background(bool own_only = false);

    bool appendChild(element *el);
    bool appendSpace(const std::wstring &val);
    bool appendText(const std::wstring &val);

    bool fetch_positioned();
    bool find_styles_changes(position::vector& redraw_boxes, int x, int y);
    bool get_predefined_height(int& p_height) const;
    bool have_inline_child();
    bool is_ancestor(element *el);
    bool is_body() const { return _type == el_body; };
    bool is_break() const;
    bool is_first_child_inline(element *el);
    bool is_floats_holder() const;
    bool is_last_child_inline(element *el);
    bool is_nth_child(element *el, int num, int off, bool of_type);
    bool is_nth_last_child(element *el, int num, int off, bool of_type);
    bool is_only_child(element *el, bool of_type);
    bool is_point_inside(int x, int y);
    bool is_replaced() const;
    bool is_white_space();
    bool on_lbutton_down();
    bool on_lbutton_up();
    bool on_mouse_leave();
    bool on_mouse_over();
    bool set_pseudo_class(const std::wstring &pclass, bool add);
    const std::wstring &get_tagName() const { return m_tag; }
    const std::wstring get_attr(const std::wstring &name, const std::wstring &def = empty) const;
    const std::wstring get_cursor() const;
    const std::wstring get_style_property(const std::wstring &name, bool inherited, const std::wstring &def = empty) const;
    const std::wstring get_text() const;
    css_length get_css_bottom() const;
    css_length get_css_height() const;
    css_length get_css_left() const;
    css_length get_css_right() const;
    css_length get_css_top() const;
    css_length get_css_width() const;
    css_offsets get_css_offsets() const;
    element_clear get_clear() const;
    element_float get_float() const;
    element_position get_element_position(css_offsets* offsets = nullptr) const;
    int find_next_line_top(int top, int width, int def_right);
    int get_base_line() const;
    int get_floats_height(element_float el_float = float_none) const;
    int get_font_size() const;
    int get_left_floats_height() const;
    int get_line_left(int y);
    int get_line_right(int y, int def_right);
    int get_right_floats_height() const;
    int get_zindex() const;
    int line_height() const;
    int place_element(render_win32 &renderer, element *el, int max_width);
    int render(render_win32 &renderer, int x, int y, int max_width, bool second_pass = false);
    int render_inline(render_win32 &renderer, element *container, int max_width);
    int select(const css_element_selector& selector, bool apply_pseudo = true);
    int select(const css_selector& selector, bool apply_pseudo = true);
    overflow get_overflow() const;
    size_t get_children_count() const;
    element *find_adjacent_sibling(element *el, const css_selector& selector, bool apply_pseudo = true, bool* is_pseudo = 0);
    element *find_ancestor(const css_selector& selector, bool apply_pseudo = true, bool* is_pseudo = 0);
    element *find_sibling(element *el, const css_selector& selector, bool apply_pseudo = true, bool* is_pseudo = 0);
    element *get_child(int idx) const;
    element *get_child_by_point(int x, int y, int client_x, int client_y, draw_flag flag, int zindex);
    element *get_element_by_point(int x, int y, int client_x, int client_y);
    style_display get_display() const;
    vertical_align get_vertical_align() const;
    visibility get_visibility() const;
    void add_float(element *el, int x, int y);
    void add_positioned(element *el);
    void add_style(const std::shared_ptr<style> &st);
    void apply_stylesheet(const css& stylesheet);
    void apply_vertical_align();
    void calc_document_size(size& sz, int x = 0, int y = 0);
    void calc_outlines(int parent_width);
    void draw(render_win32 &renderer, int x, int y, const position* clip);
    void draw_background(render_win32 &renderer, int x, int y, const position* clip);
    void draw_children(render_win32 &renderer, int x, int y, const position* clip, draw_flag flag, int zindex);
    void draw_stacking_context(render_win32 &renderer, int x, int y, const position* clip, bool with_positioned);
    void get_content_size(render_win32 &renderer, size& sz, int max_width);
    void get_inline_boxes(position::vector& boxes);
    void get_line_left_right(int y, int def_right, int& ln_left, int& ln_right);
    void get_redraw_box(position& pos, int x = 0, int y = 0);
    void init();
    void init_font();
    void on_click();
    void parse_attributes();
    void parse_styles(bool is_reparse = false);
    void refresh_styles();
    void render_positioned(render_win32 &renderer, render_type rt = render_all);
    void set_attr(const std::wstring &name, const std::wstring &val);
    void set_css_width(css_length& w);
    void set_data(const std::wstring &data);
    void set_tagName(const std::wstring &tag);
    void update_floats(int dy, element *parent);
    white_space get_white_space() const;
    void parent(element *par);
    void skip(bool val);
    web_color get_color(const wchar_t* prop_name, bool inherited, const web_color& def_color = web_color());

protected:

    int fix_line_width(render_win32 &renderer, int max_width, element_float flt);
    void parse_background();
    void init_background_paint(render_win32 &renderer, position pos, background_paint &bg_paint, background* bg);
    void draw_list_marker(render_win32 &renderer, const position &pos);
    void parse_nth_child_params(const std::wstring &param, int &num, int &off);
    void remove_before_after();
    void add_text(const std::wstring& txt);
    void add_function(const std::wstring& fnc, const std::wstring& params);

    wchar_t convert_escape(const wchar_t* txt);

    element *get_element_before();
    element *get_element_after();
};



inline std::vector<element*>& element::children()
{
    return m_children;
}

class element_zindex_sort
{
public:
    bool operator()(element *_Left, element *_Right) const
    {
        return (_Left->get_zindex() < _Right->get_zindex());
    }
};

inline int element::right() const
{
    return left() + width();
}

inline int element::left() const
{
    return m_pos.left() - margin_left() - m_padding.left - m_borders.left;
}

inline int element::top() const
{
    return m_pos.top() - margin_top() - m_padding.top - m_borders.top;
}

inline int element::bottom() const
{
    return top() + height();
}

inline int element::height() const
{
    return m_pos.height + margin_top() + margin_bottom() + m_padding.height() + m_borders.height();
}

inline int element::width() const
{
    return m_pos.width + margin_left() + margin_right() + m_padding.width() + m_borders.width();
}

inline int element::content_margins_top() const
{
    return margin_top() + m_padding.top + m_borders.top;
}

inline int element::content_margins_bottom() const
{
    return margin_bottom() + m_padding.bottom + m_borders.bottom;
}

inline int element::content_margins_left() const
{
    return margin_left() + m_padding.left + m_borders.left;
}

inline int element::content_margins_right() const
{
    return margin_right() + m_padding.right + m_borders.right;
}

inline margins element::get_paddings() const
{
    return m_padding;
}

inline margins element::get_borders() const
{
    return m_borders;
}

inline int element::padding_top() const
{
    return m_padding.top;
}

inline int element::padding_bottom() const
{
    return m_padding.bottom;
}

inline int element::padding_left() const
{
    return m_padding.left;
}

inline int element::padding_right() const
{
    return m_padding.right;
}

inline bool element::in_normal_flow() const
{
    if (get_element_position() != element_position_absolute && get_display() != display_none)
    {
        return true;
    }
    return false;
}

inline int element::border_top() const
{
    return m_borders.top;
}

inline int element::border_bottom() const
{
    return m_borders.bottom;
}

inline int element::border_left() const
{
    return m_borders.left;
}

inline int element::border_right() const
{
    return m_borders.right;
}

inline bool element::skip()
{
    return m_skip;
}

inline void element::skip(bool val)
{
    m_skip = val;
}

inline void element::parent(element *par)
{
    m_parent = par;
}

inline int element::margin_top() const
{
    return m_margins.top;
}

inline int element::margin_bottom() const
{
    return m_margins.bottom;
}

inline int element::margin_left() const
{
    return m_margins.left;
}

inline int element::margin_right() const
{
    return m_margins.right;
}

inline margins element::get_margins() const
{
    margins ret;
    ret.left = margin_left();
    ret.right = margin_right();
    ret.top = margin_top();
    ret.bottom = margin_bottom();

    return ret;
}

inline bool element::is_positioned() const
{
    return (get_element_position() > element_position_static);
}

inline bool element::is_visible() const
{
    return !(m_skip || get_display() == display_none || get_visibility() != visibility_visible);
}
