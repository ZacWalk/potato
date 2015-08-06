#include "pch.h"
#include "element.h"
#include "document.h"
#include "iterators.h"
#include "stylesheet.h"
#include "table.h"
#include "render_win32.h"
#include "strings.h"

element::element(document &doc, enum element_type t, const std::wstring &text) : m_doc(doc), _type(t), m_text(text)
{
    m_box = nullptr;
    m_parent = nullptr;
    m_skip = false;
    m_box_sizing = box_sizing_content_box;
    m_z_index = 0;
    m_overflow = overflow_visible;
    m_text_align = text_align_left;
    m_el_position = element_position_static;
    m_display = display_inline;
    m_vertical_align = va_baseline;
    m_list_style_type = list_style_type_none;
    m_list_style_position = list_style_position_outside;
    m_float = float_none;
    m_clear = clear_none;
    m_font = 0;
    m_font_size = 0;
    m_white_space = white_space_normal;
    m_lh_predefined = false;
    m_line_height = 0;
    m_visibility = visibility_visible;
    _loaded = false;

    m_border_spacing_x = 0;
    m_border_spacing_y = 0;
    m_border_collapse = border_collapse_separate;

    m_text_transform = text_transform_none;
    m_use_transformed = false;
    m_draw_spaces = true;

    if (_type == el_before)
    {
        set_tagName(_t("::before"));
    }
    else if (_type == el_after)
    {
        set_tagName(_t("::after"));
    }
    else if (_type == el_cdata || _type == el_comment)
    {
        m_skip = true;
    }
    else if (_type == el_image)
    {
        m_display = display_inline_block;
    }
}

element::~element()
{
    for (const auto &child : m_children)
    {
        delete child;
    }

    for (const auto &box : m_boxes)
    {
        delete box;
    }

    m_children.clear();
    m_boxes.clear();
}

bool element::is_point_inside(int x, int y)
{
    if (get_display() != display_inline && get_display() != display_table_row)
    {
        position pos = m_pos;
        pos += m_padding;
        pos += m_borders;

        return pos.is_point_inside(x, y);
    }
    else
    {
        position::vector boxes;
        get_inline_boxes(boxes);

        for (auto box = boxes.begin(); box != boxes.end(); box++)
        {
            if (box->is_point_inside(x, y))
            {
                return true;
            }
        }
    }
    return false;
}

web_color element::get_color(const wchar_t* prop_name, bool inherited, const web_color& def_color)
{
    const auto clrstr = get_style_property(prop_name, inherited);

    if (clrstr.empty())
    {
        return def_color;
    }

    return web_color::from_string(clrstr);
}

position element::get_placement() const
{
    position pos = m_pos;
    auto cur_el = parent();

    while (cur_el)
    {
        pos.x += cur_el->m_pos.x;
        pos.y += cur_el->m_pos.y;
        cur_el = cur_el->parent();
    }
    return pos;
}

bool element::is_inline_box() const
{
    style_display d = get_display();

    return d == display_inline ||
        d == display_inline_block ||
        d == display_inline_text;
}

bool element::collapse_top_margin() const
{
    if (!m_borders.top && !m_padding.top && in_normal_flow() && get_float() == float_none && m_margins.top >= 0 && parent())
    {
        return true;
    }
    return false;
}

bool element::collapse_bottom_margin() const
{
    if (!m_borders.bottom && !m_padding.bottom && in_normal_flow() && get_float() == float_none && m_margins.bottom >= 0 && parent())
    {
        return true;
    }
    return false;
}

bool element::get_predefined_height(int& p_height) const
{
    css_length h = get_css_height();
    if (h.is_predefined())
    {
        p_height = m_pos.height;
        return false;
    }
    if (h.units() == css_units_percentage)
    {
        if (!m_parent)
        {
            position client_pos = m_doc.client_pos();
            p_height = h.calc_percent(client_pos.height);
            return true;
        }
        else
        {
            int ph = 0;
            if (m_parent->get_predefined_height(ph))
            {
                p_height = h.calc_percent(ph);
                return true;
            }
            else
            {
                p_height = m_pos.height;
                return false;
            }
        }
    }
    p_height = m_doc.cvt_units(h, get_font_size());
    return true;
}

int element::calc_width(int defVal) const
{
    css_length w = get_css_width();
    if (w.is_predefined())
    {
        return defVal;
    }
    if (w.units() == css_units_percentage)
    {
        if (!m_parent)
        {
            position client_pos = m_doc.client_pos();
            return w.calc_percent(client_pos.width);
        }
        else
        {
            int pw = m_parent->calc_width(defVal);
            return w.calc_percent(pw);
        }
    }
    return m_doc.cvt_units(w, get_font_size());
}

bool element::is_ancestor(element *el)
{
    auto el_parent = parent();

    while (el_parent && el_parent != el)
    {
        el_parent = el_parent->parent();
    }
    if (el_parent)
    {
        return true;
    }
    return false;
}

int element::get_inline_shift_left()
{
    int ret = 0;

    if (m_parent->get_display() == display_inline)
    {
        style_display disp = get_display();

        if (disp == display_inline_text || disp == display_inline_block)
        {
            auto parent = m_parent;
            auto el = this;

            while (parent && parent->get_display() == display_inline)
            {
                if (parent->is_first_child_inline(el))
                {
                    ret += parent->padding_left() + parent->border_left() + parent->margin_left();
                }
                el = parent;
                parent = parent->m_parent;
            }
        }
    }

    return ret;
}

int element::get_inline_shift_right()
{
    int ret = 0;

    if (m_parent->get_display() == display_inline)
    {
        auto disp = get_display();

        if (disp == display_inline_text || disp == display_inline_block)
        {
            auto parent = m_parent;
            auto el = this;

            while (parent && parent->get_display() == display_inline)
            {
                if (parent->is_last_child_inline(el))
                {
                    ret += parent->padding_right() + parent->border_right() + parent->margin_right();
                }

                el = parent;
                parent = parent->m_parent;
            }
        }
    }

    return ret;
}

bool element::appendSpace(const std::wstring &val)
{
    if (_type == el_style || _type == el_script)
    {
        m_text += val;
    }
    else
    {
        appendChild(new element(m_doc, el_space, val));
    }

    return true;
}

bool element::appendText(const std::wstring &val)
{
    if (_type == el_style || _type == el_script)
    {
        m_text += val;
    }
    else
    {
        appendChild(new element(m_doc, el_text, val));
    }

    return true;
}

bool element::appendChild(element *el)
{
    assert(el);

    if (el)
    {
        if (_type == el_table)
        {
            if (el->get_tagName() == _t("tbody") || el->get_tagName() == _t("thead") || el->get_tagName() == _t("tfoot"))
            {
                el->parent(this);
                m_children.push_back(el);
                return true;
            }
        }
        else
        {
            el->parent(this);
            m_children.push_back(el);
            return true;
        }
    }

    return false;
}

void element::set_attr(const std::wstring &k, const std::wstring &val)
{
    if (!k.empty() && !val.empty())
    {
        m_attrs[k] = val;
    }
}

const std::wstring element::get_attr(const std::wstring &name, const std::wstring &def) const
{
    auto attr = m_attrs.find(name);

    if (attr != m_attrs.end())
    {
        return attr->second;
    }
    return def;
}

void element::apply_stylesheet(const css& stylesheet)
{
    if (_type == el_before || _type == el_after || _type == el_text || _type == el_space || _type == el_style || _type == el_script)
    {
        return;
    }

    if (_type == el_anchor)
    {
        if (!get_attr(_t("href")).empty())
        {
            m_pseudo_classes.push_back(_t("link"));
        }
    }

    remove_before_after();
    m_used_styles.clear();

    for (const auto &sel : stylesheet.selectors())
    {
        int apply = select(*sel, false);

        if (apply != select_no_match)
        {
            used_selector us(sel, false);

            if (sel->is_media_valid())
            {
                if (apply & select_match_pseudo_class)
                {
                    if (select(*sel, true))
                    {
                        add_style(sel->m_style);
                        us.m_used = true;
                    }
                }
                else if (apply & select_match_with_after)
                {
                    auto el = get_element_after();

                    if (el)
                    {
                        el->add_style(sel->m_style);
                    }
                }
                else if (apply & select_match_with_before)
                {
                    auto el = get_element_before();

                    if (el)
                    {
                        el->add_style(sel->m_style);
                    }
                }
                else
                {
                    add_style(sel->m_style);
                    us.m_used = true;
                }

                m_used_styles.push_back(us);
            }
        }
    }

    for (const auto &child : m_children)
    {
        //if (child->get_display() != display_inline_text)
        {
            child->apply_stylesheet(stylesheet);
        }
    }
}

void element::get_content_size(render_win32 &renderer, size& sz, int max_width)
{
    if (_type == el_text || _type == el_space)
    {
        sz = m_size;
    }
    else if (_type == el_image)
    {
        sz = renderer.get_image_size(m_doc.find_image(m_src));
    }
    else
    {
        sz.height = 0;

        if (m_display == display_block)
        {
            sz.width = max_width;
        }
        else
        {
            sz.width = 0;
        }
    }
}

void element::draw(render_win32 &renderer, int x, int y, const position* clip)
{
    position pos = m_pos;
    pos.x += x;
    pos.y += y;

    if (_type == el_text || _type == el_space)
    {
        if (is_white_space() && !m_draw_spaces)
        {
            return;
        }

        if (pos.does_intersect(clip))
        {
            auto font = m_parent->get_font();
            auto color = m_parent->get_color(_t("color"), true, m_doc.get_def_color());
            renderer.draw_text(m_use_transformed ? m_transformed_text.c_str() : m_text.c_str(), font, color, pos);
        }
    }
    else if (_type == el_image)
    {
        draw_background(renderer, x, y, clip);

        if (pos.does_intersect(clip))
        {
            background_paint bg;
            bg.image = m_doc.find_image(m_src);
            bg.clip_box = pos;
            bg.origin_box = pos;
            bg.border_box = pos;
            bg.border_box += m_padding;
            bg.border_box += m_borders;
            bg.repeat = background_repeat_no_repeat;
            bg.image_size.width = pos.width;
            bg.image_size.height = pos.height;
            bg.border_radius = m_css_borders.radius;
            bg.position_x = pos.x;
            bg.position_y = pos.y;
            renderer.draw_background(renderer, bg);
        }
    }
    else
    {
        draw_background(renderer, x, y, clip);

        if (m_display == display_list_item && m_list_style_type != list_style_type_none)
        {
            if (m_overflow > overflow_visible)
            {
                renderer.set_clip(pos, true, true);
            }

            draw_list_marker(renderer, pos);

            if (m_overflow > overflow_visible)
            {
                renderer.del_clip();
            }
        }
    }
}

HFONT element::get_font(font_metrics* fm)
{
    if (_type == el_text || _type == el_space)
    {
        return m_parent->get_font(fm);
    }
    else
    {
        if (fm)
        {
            *fm = m_font_metrics;
        }

        return m_font;
    }
}

const std::wstring element::get_style_property(const std::wstring &name, bool inherited, const std::wstring &def) const
{

    if (_type == el_text || _type == el_space)
    {
        if (inherited)
        {
            return m_parent->get_style_property(name, inherited, def);
        }
        return def;
    }
    else
    {
        const auto result = m_style.get_property(name);
        auto pass_parent = false;

        if (m_parent)
        {
            if (!result.empty() && is_equal(result.c_str(), _t("inherit")))
            {
                pass_parent = true;
            }
            else if (result.empty() && inherited)
            {
                pass_parent = true;
            }
        }

        if (pass_parent)
        {
            const auto parent_ret = m_parent->get_style_property(name, inherited, def);

            if (!parent_ret.empty())
            {
                return parent_ret;
            }
        }

        return result.empty() ? def : result;
    }
}

void element::parse_styles(bool is_reparse)
{
    if (_type == el_text || _type == el_space)
    {
        m_text_transform = (text_transform) value_index(get_style_property(_t("text-transform"), true, _t("none")), text_transform_strings, text_transform_none);
        
        if (m_text_transform != text_transform_none)
        {
            m_transformed_text = m_text;
            m_use_transformed = true;
            transform_text(m_transformed_text, m_text_transform);
        }

        if (is_white_space())
        {
            m_transformed_text = _t(" ");
            m_use_transformed = true;
        }
        else if (m_text == _t("\t"))
        {
            m_transformed_text = _t(" ");
            m_use_transformed = true;
        }
        else if (m_text == _t("\n") || m_text == _t("\r"))
        {
            m_transformed_text.clear();
            m_use_transformed = true;
        }

        font_metrics fm;
        auto font = m_parent->get_font(&fm);

        if (is_break())
        {
            m_size.height = 0;
            m_size.width = 0;
        }
        else
        {
            m_size.height = fm.height;
            m_size.width = m_doc.text_width(m_use_transformed ? m_transformed_text.c_str() : m_text.c_str(), font);
        }

        m_draw_spaces = fm.draw_spaces;

        return;
    }

    m_id = get_attr(_t("id"));
    m_class = get_attr(_t("class"));

    const auto &style = get_attr(_t("style"));

    if (!style.empty())
    {
        m_style.add(style, L"");
    }

    init_font();

    m_el_position = (element_position) value_index(get_style_property(_t("position"), false, _t("static")), element_position_strings, element_position_fixed);
    m_text_align = (text_align) value_index(get_style_property(_t("text-align"), true, _t("left")), text_align_strings, text_align_left);
    m_overflow = (overflow) value_index(get_style_property(_t("overflow"), false, _t("visible")), overflow_strings, overflow_visible);
    m_white_space = (white_space) value_index(get_style_property(_t("white-space"), true, _t("normal")), white_space_strings, white_space_normal);
    m_display = (style_display) value_index(get_style_property(_t("display"), false, _t("inline")), style_display_strings, display_inline);
    m_visibility = (visibility) value_index(get_style_property(_t("visibility"), true, _t("visible")), visibility_strings, visibility_visible);
    m_box_sizing = (box_sizing) value_index(get_style_property(_t("box-sizing"), false, _t("content-box")), box_sizing_strings, box_sizing_content_box);

    if (m_el_position != element_position_static)
    {
        const auto val = get_style_property(_t("z-index"), false);

        if (!val.empty())
        {
            m_z_index = std::stoi(val);
        }
    }

    const auto va = get_style_property(_t("vertical-align"), true, _t("baseline"));
    m_vertical_align = (vertical_align) value_index(va, vertical_align_strings, va_baseline);

    const auto fl = get_style_property(_t("float"), false, _t("none"));
    m_float = (element_float) value_index(fl, element_float_strings, float_none);

    m_clear = (element_clear) value_index(get_style_property(_t("clear"), false, _t("none")), element_clear_strings, clear_none);

    if (m_display != display_none)
    {
        if ((m_el_position == element_position_absolute || m_float != float_none))
        {
            m_display = display_block;
        }
        if (m_el_position == element_position_fixed)
        {
            m_display = display_block;
        }
    }

    m_css_text_indent.fromString(get_style_property(_t("text-indent"), true, _t("0")), _t("0"));

    m_css_width.fromString(get_style_property(_t("width"), false, _t("auto")), _t("auto"));
    m_css_height.fromString(get_style_property(_t("height"), false, _t("auto")), _t("auto"));

    m_doc.cvt_units(m_css_width, m_font_size);
    m_doc.cvt_units(m_css_height, m_font_size);

    m_css_min_width.fromString(get_style_property(_t("min-width"), false, _t("0")));
    m_css_min_height.fromString(get_style_property(_t("min-height"), false, _t("0")));

    m_css_max_width.fromString(get_style_property(_t("max-width"), false, _t("none")), _t("none"));
    m_css_max_height.fromString(get_style_property(_t("max-height"), false, _t("none")), _t("none"));

    m_doc.cvt_units(m_css_min_width, m_font_size);
    m_doc.cvt_units(m_css_min_height, m_font_size);

    m_css_offsets.left.fromString(get_style_property(_t("left"), false, _t("auto")), _t("auto"));
    m_css_offsets.right.fromString(get_style_property(_t("right"), false, _t("auto")), _t("auto"));
    m_css_offsets.top.fromString(get_style_property(_t("top"), false, _t("auto")), _t("auto"));
    m_css_offsets.bottom.fromString(get_style_property(_t("bottom"), false, _t("auto")), _t("auto"));

    m_doc.cvt_units(m_css_offsets.left, m_font_size);
    m_doc.cvt_units(m_css_offsets.right, m_font_size);
    m_doc.cvt_units(m_css_offsets.top, m_font_size);
    m_doc.cvt_units(m_css_offsets.bottom, m_font_size);

    m_css_margins.left.fromString(get_style_property(_t("margin-left"), false, _t("0")), _t("auto"));
    m_css_margins.right.fromString(get_style_property(_t("margin-right"), false, _t("0")), _t("auto"));
    m_css_margins.top.fromString(get_style_property(_t("margin-top"), false, _t("0")), _t("auto"));
    m_css_margins.bottom.fromString(get_style_property(_t("margin-bottom"), false, _t("0")), _t("auto"));

    m_css_padding.left.fromString(get_style_property(_t("padding-left"), false, _t("0")));
    m_css_padding.right.fromString(get_style_property(_t("padding-right"), false, _t("0")));
    m_css_padding.top.fromString(get_style_property(_t("padding-top"), false, _t("0")));
    m_css_padding.bottom.fromString(get_style_property(_t("padding-bottom"), false, _t("0")));

    m_css_borders.left.width.fromString(get_style_property(_t("border-left-width"), false, _t("medium")), border_width_strings);
    m_css_borders.right.width.fromString(get_style_property(_t("border-right-width"), false, _t("medium")), border_width_strings);
    m_css_borders.top.width.fromString(get_style_property(_t("border-top-width"), false, _t("medium")), border_width_strings);
    m_css_borders.bottom.width.fromString(get_style_property(_t("border-bottom-width"), false, _t("medium")), border_width_strings);

    m_css_borders.left.color = web_color::from_string(get_style_property(_t("border-left-color"), false));
    m_css_borders.left.style = (border_style) value_index(get_style_property(_t("border-left-style"), false, _t("none")), border_style_strings, border_style_none);

    m_css_borders.right.color = web_color::from_string(get_style_property(_t("border-right-color"), false));
    m_css_borders.right.style = (border_style) value_index(get_style_property(_t("border-right-style"), false, _t("none")), border_style_strings, border_style_none);

    m_css_borders.top.color = web_color::from_string(get_style_property(_t("border-top-color"), false));
    m_css_borders.top.style = (border_style) value_index(get_style_property(_t("border-top-style"), false, _t("none")), border_style_strings, border_style_none);

    m_css_borders.bottom.color = web_color::from_string(get_style_property(_t("border-bottom-color"), false));
    m_css_borders.bottom.style = (border_style) value_index(get_style_property(_t("border-bottom-style"), false, _t("none")), border_style_strings, border_style_none);

    m_css_borders.radius.top_left_x.fromString(get_style_property(_t("border-top-left-radius-x"), false, _t("0")));
    m_css_borders.radius.top_left_y.fromString(get_style_property(_t("border-top-left-radius-y"), false, _t("0")));

    m_css_borders.radius.top_right_x.fromString(get_style_property(_t("border-top-right-radius-x"), false, _t("0")));
    m_css_borders.radius.top_right_y.fromString(get_style_property(_t("border-top-right-radius-y"), false, _t("0")));

    m_css_borders.radius.bottom_right_x.fromString(get_style_property(_t("border-bottom-right-radius-x"), false, _t("0")));
    m_css_borders.radius.bottom_right_y.fromString(get_style_property(_t("border-bottom-right-radius-y"), false, _t("0")));

    m_css_borders.radius.bottom_left_x.fromString(get_style_property(_t("border-bottom-left-radius-x"), false, _t("0")));
    m_css_borders.radius.bottom_left_y.fromString(get_style_property(_t("border-bottom-left-radius-y"), false, _t("0")));

    m_doc.cvt_units(m_css_borders.radius.bottom_left_x, m_font_size);
    m_doc.cvt_units(m_css_borders.radius.bottom_left_y, m_font_size);
    m_doc.cvt_units(m_css_borders.radius.bottom_right_x, m_font_size);
    m_doc.cvt_units(m_css_borders.radius.bottom_right_y, m_font_size);
    m_doc.cvt_units(m_css_borders.radius.top_left_x, m_font_size);
    m_doc.cvt_units(m_css_borders.radius.top_left_y, m_font_size);
    m_doc.cvt_units(m_css_borders.radius.top_right_x, m_font_size);
    m_doc.cvt_units(m_css_borders.radius.top_right_y, m_font_size);

    m_doc.cvt_units(m_css_text_indent, m_font_size);

    m_margins.left = m_doc.cvt_units(m_css_margins.left, m_font_size);
    m_margins.right = m_doc.cvt_units(m_css_margins.right, m_font_size);
    m_margins.top = m_doc.cvt_units(m_css_margins.top, m_font_size);
    m_margins.bottom = m_doc.cvt_units(m_css_margins.bottom, m_font_size);

    m_padding.left = m_doc.cvt_units(m_css_padding.left, m_font_size);
    m_padding.right = m_doc.cvt_units(m_css_padding.right, m_font_size);
    m_padding.top = m_doc.cvt_units(m_css_padding.top, m_font_size);
    m_padding.bottom = m_doc.cvt_units(m_css_padding.bottom, m_font_size);

    m_borders.left = m_doc.cvt_units(m_css_borders.left.width, m_font_size);
    m_borders.right = m_doc.cvt_units(m_css_borders.right.width, m_font_size);
    m_borders.top = m_doc.cvt_units(m_css_borders.top.width, m_font_size);
    m_borders.bottom = m_doc.cvt_units(m_css_borders.bottom.width, m_font_size);

    css_length line_height;
    line_height.fromString(get_style_property(_t("line-height"), true, _t("normal")), _t("normal"));

    if (line_height.is_predefined())
    {
        m_line_height = m_font_metrics.height;
        m_lh_predefined = true;
    }
    else if (line_height.units() == css_units_none)
    {
        m_line_height = (int) (line_height.val() * m_font_size);
        m_lh_predefined = false;
    }
    else
    {
        m_line_height = m_doc.cvt_units(line_height, m_font_size, m_font_size);
        m_lh_predefined = false;
    }

    if (m_display == display_list_item)
    {
        const auto list_type = get_style_property(_t("list-style-type"), true, _t("disc"));
        m_list_style_type = (list_style_type) value_index(list_type, list_style_type_strings, list_style_type_disc);

        const auto list_pos = get_style_property(_t("list-style-position"), true, _t("outside"));
        m_list_style_position = (list_style_position) value_index(list_pos, list_style_position_strings, list_style_position_outside);

        const auto list_image = get_style_property(_t("list-style-image"), true);

        if (!list_image.empty())
        {
            auto url = css::parse_css_url(list_image);
            auto list_image_baseurl = get_style_property(_t("list-style-image-baseurl"), true);

            m_doc.load_image(url, list_image_baseurl);
        }

    }

    parse_background();

    if (!is_reparse)
    {
        for (const auto &child : m_children)
        {
            child->parse_styles();
        }

        init();
    }


    if (_type == el_table)
    {
        m_border_collapse = (border_collapse) value_index(get_style_property(_t("border-collapse"), true, _t("separate")), border_collapse_strings, border_collapse_separate);

        if (m_border_collapse == border_collapse_separate)
        {
            m_css_border_spacing_x.fromString(get_style_property(_t("-potato-border-spacing-x"), true, _t("0px")));
            m_css_border_spacing_y.fromString(get_style_property(_t("-potato-border-spacing-y"), true, _t("0px")));

            int fntsz = get_font_size();
            m_border_spacing_x = m_doc.cvt_units(m_css_border_spacing_x, fntsz);
            m_border_spacing_y = m_doc.cvt_units(m_css_border_spacing_y, fntsz);
        }
        else
        {
            m_border_spacing_x = 0;
            m_border_spacing_y = 0;
            m_padding.bottom = 0;
            m_padding.top = 0;
            m_padding.left = 0;
            m_padding.right = 0;
            m_css_padding.bottom.set_value(0, css_units_px);
            m_css_padding.top.set_value(0, css_units_px);
            m_css_padding.left.set_value(0, css_units_px);
            m_css_padding.right.set_value(0, css_units_px);
        }
    }
    else if (_type == el_image)
    {
        if (!m_src.empty())
        {
            if (!_loaded)
            {
                _loaded = true;
                m_doc.load_image(m_src, empty);
            }
        }
    }
}

int element::render(render_win32 &renderer, int x, int y, int max_width, bool second_pass)
{
    if (_type == el_text || _type == el_space)
    {
        return 0;
    }
    else if (_type == el_table)
    {
        int parent_width = max_width;

        // reset auto margins
        if (m_css_margins.left.is_predefined())
        {
            m_margins.left = 0;
        }
        if (m_css_margins.right.is_predefined())
        {
            m_margins.right = 0;
        }

        m_pos.clear();
        m_pos.move_to(x, y);

        m_pos.x += content_margins_left();
        m_pos.y += content_margins_top();

        def_value<int> block_width(0);

        if (!m_css_width.is_predefined())
        {
            max_width = block_width = calc_width(parent_width - (content_margins_left() + content_margins_right()));
        }
        else
        {
            if (max_width)
            {
                max_width -= content_margins_left() + content_margins_right();
            }
        }

        calc_outlines(parent_width);

        // Calculate table spacing
        int table_width_spacing = 0;
        if (m_border_collapse == border_collapse_separate)
        {
            table_width_spacing = m_border_spacing_x * (m_grid.cols_count() + 1);
        }
        else
        {
            table_width_spacing = 0;

            if (m_grid.cols_count())
            {
                table_width_spacing -= std::min(border_left(), m_grid.column(0).border_left);
                table_width_spacing -= std::min(border_right(), m_grid.column(m_grid.cols_count() - 1).border_right);
            }

            for (int col = 1; col < m_grid.cols_count(); col++)
            {
                table_width_spacing -= std::min(m_grid.column(col).border_left, m_grid.column(col - 1).border_right);
            }
        }


        // Calculate the minimum content width (MCW) of each cell: the formatted content may span any number of lines but may not overflow the cell box. 
        // If the specified 'width' (W) of the cell is greater than MCW, W is the minimum cell width. A value of 'auto' means that MCW is the minimum 
        // cell width.
        // 
        // Also, calculate the "maximum" cell width of each cell: formatting the content without breaking lines other than where explicit line breaks occur.

        if (m_grid.cols_count() == 1 && !block_width.is_default())
        {
            for (int row = 0; row < m_grid.rows_count(); row++)
            {
                table_cell* cell = m_grid.cell(0, row);
                if (cell && cell->el)
                {
                    cell->min_width = cell->max_width = cell->el->render(renderer, 0, 0, max_width - table_width_spacing);
                    cell->el->m_pos.width = cell->min_width - cell->el->content_margins_left() - cell->el->content_margins_right();
                }
            }
        }
        else
        {
            for (int row = 0; row < m_grid.rows_count(); row++)
            {
                for (int col = 0; col < m_grid.cols_count(); col++)
                {
                    table_cell* cell = m_grid.cell(col, row);
                    if (cell && cell->el)
                    {
                        if (!m_grid.column(col).css_width.is_predefined() && m_grid.column(col).css_width.units() != css_units_percentage)
                        {
                            int css_w = m_grid.column(col).css_width.calc_percent(block_width);
                            int el_w = cell->el->render(renderer, 0, 0, css_w);
                            cell->min_width = cell->max_width = std::max(css_w, el_w);
                            cell->el->m_pos.width = cell->min_width - cell->el->content_margins_left() - cell->el->content_margins_right();
                        }
                        else
                        {
                            // calculate minimum content width
                            cell->min_width = cell->el->render(renderer, 0, 0, 1);
                            // calculate maximum content width
                            cell->max_width = cell->el->render(renderer, 0, 0, max_width - table_width_spacing);
                        }
                    }
                }
            }
        }

        // For each column, determine a maximum and minimum column width from the cells that span only that column. 
        // The minimum is that required by the cell with the largest minimum cell width (or the column 'width', whichever is larger). 
        // The maximum is that required by the cell with the largest maximum cell width (or the column 'width', whichever is larger).

        for (int col = 0; col < m_grid.cols_count(); col++)
        {
            m_grid.column(col).max_width = 0;
            m_grid.column(col).min_width = 0;
            for (int row = 0; row < m_grid.rows_count(); row++)
            {
                if (m_grid.cell(col, row)->colspan <= 1)
                {
                    m_grid.column(col).max_width = std::max(m_grid.column(col).max_width, m_grid.cell(col, row)->max_width);
                    m_grid.column(col).min_width = std::max(m_grid.column(col).min_width, m_grid.cell(col, row)->min_width);
                }
            }
        }

        // For each cell that spans more than one column, increase the minimum widths of the columns it spans so that together, 
        // they are at least as wide as the cell. Do the same for the maximum widths. 
        // If possible, widen all spanned columns by approximately the same amount.

        for (int col = 0; col < m_grid.cols_count(); col++)
        {
            for (int row = 0; row < m_grid.rows_count(); row++)
            {
                if (m_grid.cell(col, row)->colspan > 1)
                {
                    int max_total_width = m_grid.column(col).max_width;
                    int min_total_width = m_grid.column(col).min_width;
                    for (int col2 = col + 1; col2 < col + m_grid.cell(col, row)->colspan; col2++)
                    {
                        max_total_width += m_grid.column(col2).max_width;
                        min_total_width += m_grid.column(col2).min_width;
                    }
                    if (min_total_width < m_grid.cell(col, row)->min_width)
                    {
                        m_grid.distribute_min_width(m_grid.cell(col, row)->min_width - min_total_width, col, col + m_grid.cell(col, row)->colspan - 1);
                    }
                    if (max_total_width < m_grid.cell(col, row)->max_width)
                    {
                        m_grid.distribute_max_width(m_grid.cell(col, row)->max_width - max_total_width, col, col + m_grid.cell(col, row)->colspan - 1);
                    }
                }
            }
        }

        // If the 'table' or 'inline-table' element's 'width' property has a computed value (W) other than 'auto', the used width is the 
        // greater of W, CAPMIN, and the minimum width required by all the columns plus cell spacing or borders (MIN). 
        // If the used width is greater than MIN, the extra width should be distributed over the columns.
        //
        // If the 'table' or 'inline-table' element has 'width: auto', the used width is the greater of the table's containing block width, 
        // CAPMIN, and MIN. However, if either CAPMIN or the maximum width required by the columns plus cell spacing or borders (MAX) is 
        // less than that of the containing block, use max(MAX, CAPMIN).


        int table_width = 0;

        if (!block_width.is_default())
        {
            table_width = m_grid.calc_table_width(block_width - table_width_spacing, false);
        }
        else
        {
            table_width = m_grid.calc_table_width(max_width - table_width_spacing, true);
        }

        table_width += table_width_spacing;
        m_grid.calc_horizontal_positions(m_borders, m_border_collapse, m_border_spacing_x);

        bool row_span_found = false;

        // render cells with computed width
        for (int row = 0; row < m_grid.rows_count(); row++)
        {
            m_grid.row(row).height = 0;
            for (int col = 0; col < m_grid.cols_count(); col++)
            {
                table_cell* cell = m_grid.cell(col, row);
                if (cell->el)
                {
                    int span_col = col + cell->colspan - 1;
                    if (span_col >= m_grid.cols_count())
                    {
                        span_col = m_grid.cols_count() - 1;
                    }
                    int cell_width = m_grid.column(span_col).right - m_grid.column(col).left;

                    if (cell->el->m_pos.width != cell_width - cell->el->content_margins_left() - cell->el->content_margins_right())
                    {
                        cell->el->render(renderer, m_grid.column(col).left, 0, cell_width);
                        cell->el->m_pos.width = cell_width - cell->el->content_margins_left() - cell->el->content_margins_right();
                    }
                    else
                    {
                        cell->el->m_pos.x = m_grid.column(col).left + cell->el->content_margins_left();
                    }

                    if (cell->rowspan <= 1)
                    {
                        m_grid.row(row).height = std::max(m_grid.row(row).height, cell->el->height());
                    }
                    else
                    {
                        row_span_found = true;
                    }

                }
            }
        }

        if (row_span_found)
        {
            for (int col = 0; col < m_grid.cols_count(); col++)
            {
                for (int row = 0; row < m_grid.rows_count(); row++)
                {
                    table_cell* cell = m_grid.cell(col, row);
                    if (cell->el)
                    {
                        int span_row = row + cell->rowspan - 1;
                        if (span_row >= m_grid.rows_count())
                        {
                            span_row = m_grid.rows_count() - 1;
                        }
                        if (span_row != row)
                        {
                            int h = 0;
                            for (int i = row; i <= span_row; i++)
                            {
                                h += m_grid.row(i).height;
                            }
                            if (h < cell->el->height())
                            {
                                m_grid.row(span_row).height += cell->el->height() - h;
                            }
                        }
                    }
                }
            }
        }

        m_grid.calc_vertical_positions(m_borders, m_border_collapse, m_border_spacing_y);

        int table_height = 0;
        // place cells vertically
        for (int col = 0; col < m_grid.cols_count(); col++)
        {
            for (int row = 0; row < m_grid.rows_count(); row++)
            {
                table_cell* cell = m_grid.cell(col, row);
                if (cell->el)
                {
                    int span_row = row + cell->rowspan - 1;
                    if (span_row >= m_grid.rows_count())
                    {
                        span_row = m_grid.rows_count() - 1;
                    }
                    cell->el->m_pos.y = m_grid.row(row).top + cell->el->content_margins_top();
                    cell->el->m_pos.height = m_grid.row(span_row).bottom - m_grid.row(row).top - cell->el->content_margins_top() - cell->el->content_margins_bottom();
                    table_height = std::max(table_height, m_grid.row(span_row).bottom);
                    cell->el->apply_vertical_align();
                }
            }
        }

        if (m_border_collapse == border_collapse_collapse)
        {
            if (m_grid.rows_count())
            {
                table_height -= std::min(border_bottom(), m_grid.row(m_grid.rows_count() - 1).border_bottom);
            }
        }
        else
        {
            table_height += m_border_spacing_y;
        }

        m_pos.width = table_width;

        calc_outlines(parent_width);

        m_pos.move_to(x, y);
        m_pos.x += content_margins_left();
        m_pos.y += content_margins_top();
        m_pos.width = table_width;
        m_pos.height = table_height;

        return table_width;
    }
    else if (_type == el_image)
    {
        int parent_width = max_width;

        // restore margins after collapse
        m_margins.top = m_doc.cvt_units(m_css_margins.top, m_font_size);
        m_margins.bottom = m_doc.cvt_units(m_css_margins.bottom, m_font_size);

        m_pos.move_to(x, y);

        auto sz = renderer.get_image_size(m_doc.find_image(m_src));

        m_pos.width = sz.width;
        m_pos.height = sz.height;

        if (m_css_height.is_predefined() && m_css_width.is_predefined())
        {
            m_pos.height = sz.height;
            m_pos.width = sz.width;

            // check for max-height
            if (!m_css_max_width.is_predefined())
            {
                int max_width = m_doc.cvt_units(m_css_max_width, m_font_size, parent_width);
                if (m_pos.width > max_width)
                {
                    m_pos.width = max_width;
                }
                if (sz.width)
                {
                    m_pos.height = (int) ((float) m_pos.width * (float) sz.height / (float) sz.width);
                }
                else
                {
                    m_pos.height = sz.height;
                }
            }

            // check for max-height
            if (!m_css_max_height.is_predefined())
            {
                int max_height = m_doc.cvt_units(m_css_max_height, m_font_size);
                if (m_pos.height > max_height)
                {
                    m_pos.height = max_height;
                }
                if (sz.height)
                {
                    m_pos.width = (int) (m_pos.height * (float) sz.width / (float) sz.height);
                }
                else
                {
                    m_pos.width = sz.width;
                }
            }
        }
        else if (!m_css_height.is_predefined() && m_css_width.is_predefined())
        {
            m_pos.height = (int) m_css_height.val();

            // check for max-height
            if (!m_css_max_height.is_predefined())
            {
                int max_height = m_doc.cvt_units(m_css_max_height, m_font_size);
                if (m_pos.height > max_height)
                {
                    m_pos.height = max_height;
                }
            }

            if (sz.height)
            {
                m_pos.width = (int) (m_pos.height * (float) sz.width / (float) sz.height);
            }
            else
            {
                m_pos.width = sz.width;
            }
        }
        else if (m_css_height.is_predefined() && !m_css_width.is_predefined())
        {
            m_pos.width = (int) m_css_width.calc_percent(parent_width);

            // check for max-width
            if (!m_css_max_width.is_predefined())
            {
                int max_width = m_doc.cvt_units(m_css_max_width, m_font_size, parent_width);
                if (m_pos.width > max_width)
                {
                    m_pos.width = max_width;
                }
            }

            if (sz.width)
            {
                m_pos.height = (int) ((float) m_pos.width * (float) sz.height / (float) sz.width);
            }
            else
            {
                m_pos.height = sz.height;
            }
        }
        else
        {
            m_pos.width = (int) m_css_width.calc_percent(parent_width);
            m_pos.height = (int) m_css_height.val();

            // check for max-height
            if (!m_css_max_height.is_predefined())
            {
                int max_height = m_doc.cvt_units(m_css_max_height, m_font_size);
                if (m_pos.height > max_height)
                {
                    m_pos.height = max_height;
                }
            }

            // check for max-height
            if (!m_css_max_width.is_predefined())
            {
                int max_width = m_doc.cvt_units(m_css_max_width, m_font_size, parent_width);
                if (m_pos.width > max_width)
                {
                    m_pos.width = max_width;
                }
            }
        }

        calc_outlines(parent_width);

        m_pos.x += content_margins_left();
        m_pos.y += content_margins_top();

        return m_pos.width + content_margins_left() + content_margins_right();
    }
    else
    {

        int parent_width = max_width;

        // restore margins after collapse
        m_margins.top = m_doc.cvt_units(m_css_margins.top, m_font_size, max_width);
        m_margins.bottom = m_doc.cvt_units(m_css_margins.bottom, m_font_size, max_width);

        // reset auto margins
        if (m_css_margins.left.is_predefined())
        {
            m_margins.left = 0;
        }
        if (m_css_margins.right.is_predefined())
        {
            m_margins.right = 0;
        }

        m_pos.clear();
        m_pos.move_to(x, y);

        m_pos.x += content_margins_left();
        m_pos.y += content_margins_top();

        int ret_width = 0;

        def_value<int> block_width(0);

        if (m_display != display_table_cell && !m_css_width.is_predefined())
        {
            int w = calc_width(parent_width);
            if (m_box_sizing == box_sizing_border_box)
            {
                w -= m_padding.left + m_borders.left + m_padding.right + m_borders.right;
            }
            ret_width = max_width = block_width = w;
        }
        else
        {
            if (max_width)
            {
                max_width -= content_margins_left() + content_margins_right();
            }
        }

        // check for max-width
        if (!m_css_max_width.is_predefined())
        {
            int mw = m_doc.cvt_units(m_css_max_width, m_font_size, parent_width);
            if (m_box_sizing == box_sizing_border_box)
            {
                mw -= m_padding.left + m_borders.left + m_padding.right + m_borders.right;
            }
            if (max_width > mw)
            {
                max_width = mw;
            }
        }

        m_floats_left.clear();
        m_floats_right.clear();
        m_boxes.clear();
        m_cahe_line_left.invalidate();
        m_cahe_line_right.invalidate();

        calc_outlines(parent_width);


        for (const auto &el : m_children)
        {
            auto el_position = el->get_element_position();
            if ((el_position == element_position_absolute || el_position == element_position_fixed) && second_pass) continue;

            int rw = place_element(renderer, el, max_width);
            if (rw > ret_width)
            {
                ret_width = rw;
            }
        }

        m_pos.height = 0;

        finish_last_box(true);

        if (block_width.is_default() && is_inline_box())
        {
            m_pos.width = ret_width;
        }
        else
        {
            m_pos.width = max_width;
        }
        calc_outlines(parent_width);

        if (!m_boxes.empty())
        {
            if (collapse_top_margin())
            {
                int old_top = m_margins.top;
                m_margins.top = std::max(m_boxes.front()->top_margin(), m_margins.top);
                if (m_margins.top != old_top)
                {
                    update_floats(m_margins.top - old_top, this);
                }
            }
            if (collapse_bottom_margin())
            {
                m_margins.bottom = std::max(m_boxes.back()->bottom_margin(), m_margins.bottom);
                m_pos.height = m_boxes.back()->bottom() - m_boxes.back()->bottom_margin();
            }
            else
            {
                m_pos.height = m_boxes.back()->bottom();
            }
        }

        // add the floats height to the block height
        if (is_floats_holder())
        {
            int floats_height = get_floats_height();
            if (floats_height > m_pos.height)
            {
                m_pos.height = floats_height;
            }
        }

        // calculate the final position

        m_pos.move_to(x, y);
        m_pos.x += content_margins_left();
        m_pos.y += content_margins_top();

        int block_height = 0;
        if (get_predefined_height(block_height))
        {
            m_pos.height = block_height;
        }

        int min_height = 0;
        if (!m_css_min_height.is_predefined() && m_css_min_height.units() == css_units_percentage)
        {
            if (m_parent)
            {
                if (m_parent->get_predefined_height(block_height))
                {
                    min_height = m_css_min_height.calc_percent(block_height);
                }
            }
        }
        else
        {
            min_height = (int) m_css_min_height.val();
        }
        if (min_height != 0 && m_box_sizing == box_sizing_border_box)
        {
            min_height -= m_padding.top + m_borders.top + m_padding.bottom + m_borders.bottom;
            if (min_height < 0) min_height = 0;
        }

        if (m_display == display_list_item)
        {
            auto list_image = get_style_property(_t("list-style-image"), true);

            if (!list_image.empty())
            {
                auto url = css::parse_css_url(list_image);
                auto list_image_baseurl = get_style_property(_t("list-style-image-baseurl"), true);
                auto sz = renderer.get_image_size(m_doc.find_image(url, list_image_baseurl));

                if (min_height < sz.height)
                {
                    min_height = sz.height;
                }
            }

        }

        if (min_height > m_pos.height)
        {
            m_pos.height = min_height;
        }

        int min_width = m_css_min_width.calc_percent(parent_width);

        if (min_width != 0 && m_box_sizing == box_sizing_border_box)
        {
            min_width -= m_padding.left + m_borders.left + m_padding.right + m_borders.right;
            if (min_width < 0) min_width = 0;
        }

        if (min_width != 0)
        {
            if (min_width > m_pos.width)
            {
                m_pos.width = min_width;
            }
            if (min_width > ret_width)
            {
                ret_width = min_width;
            }
        }

        ret_width += content_margins_left() + content_margins_right();

        // re-render with new width
        if (ret_width < max_width && !second_pass && m_parent)
        {
            if (m_display == display_inline_block ||
                m_css_width.is_predefined() &&
                (m_float != float_none ||
                m_display == display_table ||
                m_el_position == element_position_absolute ||
                m_el_position == element_position_fixed
                )
                )
            {
                render(renderer, x, y, ret_width, true);
                m_pos.width = ret_width - (content_margins_left() + content_margins_right());
            }
        }

        return ret_width;
    }
}

bool element::is_white_space()
{
    if (_type == el_space)
    {
        auto ws = get_white_space();

        return ws == white_space_normal ||
            ws == white_space_nowrap ||
            ws == white_space_pre_line;
    }

    return false;
}

int element::get_font_size() const
{
    return m_font_size;
}

int element::get_base_line() const
{
    if (_type == el_text || _type == el_space)
    {
        return m_parent->get_base_line();
    }

    if (is_replaced())
    {
        return 0;
    }

    int bl = 0;
    if (!m_boxes.empty())
    {
        bl = m_boxes.back()->baseline() + content_margins_bottom();
    }

    return bl;
}

void element::init()
{
    if (_type == el_table)
    {
        m_grid.clear();

        go_inside_table table_selector;
        table_rows_selector row_selector;
        table_cells_selector cell_selector;

        elements_iterator<go_inside_table, table_rows_selector> row_iter(this, table_selector, row_selector);

        auto row = row_iter.next(false);
        while (row)
        {
            m_grid.begin_row(row);

            elements_iterator<go_inside_table, table_cells_selector> cell_iter(row, table_selector, cell_selector);
            auto cell = cell_iter.next();

            while (cell)
            {
                m_grid.add_cell(cell);

                cell = cell_iter.next(false);
            }
            row = row_iter.next(false);
        }

        m_grid.finish();
    }
    else
    {
        //remove duplicate white spaces
        auto i = m_children.begin();

        while (i != m_children.end())
        {
            if ((*i)->is_white_space())
            {
                i++;

                while (i != m_children.end() && (*i)->is_white_space())
                {
                    i = m_children.erase(i);
                }
            }
            else
            {
                i++;
            }
        }
    }
}

int element::select(const css_selector& selector, bool apply_pseudo)
{
    if (_type == el_text || _type == el_space)
    {
        return select_no_match;
    }

    int right_res = select(selector.m_right, apply_pseudo);

    if (right_res == select_no_match)
    {
        return select_no_match;
    }

    if (selector.m_left)
    {
        if (!m_parent)
        {
            return select_no_match;
        }

        switch (selector.m_combinator)
        {
        case combinator_descendant:
        {
            bool is_pseudo = false;
            auto res = find_ancestor(*selector.m_left, apply_pseudo, &is_pseudo);

            if (!res)
            {
                return select_no_match;
            }
            else
            {
                if (is_pseudo)
                {
                    right_res |= select_match_pseudo_class;
                }
            }
        }
            break;

        case combinator_child:
        {
            int res = m_parent->select(*selector.m_left, apply_pseudo);
            if (res == select_no_match)
            {
                return select_no_match;
            }
            else
            {
                if (right_res != select_match_pseudo_class)
                {
                    right_res |= res;
                }
            }
        }
            break;

        case combinator_adjacent_sibling:
        {
            bool is_pseudo = false;
            auto res = m_parent->find_adjacent_sibling(this, *selector.m_left, apply_pseudo, &is_pseudo);

            if (!res)
            {
                return select_no_match;
            }
            else
            {
                if (is_pseudo)
                {
                    right_res |= select_match_pseudo_class;
                }
            }
        }
            break;

        case combinator_general_sibling:
        {
            bool is_pseudo = false;
            auto res = m_parent->find_sibling(this, *selector.m_left, apply_pseudo, &is_pseudo);
            if (!res)
            {
                return select_no_match;
            }
            else
            {
                if (is_pseudo)
                {
                    right_res |= select_match_pseudo_class;
                }
            }
        }
            break;

        default:
            right_res = select_no_match;
        }
    }

    return right_res;
}

int element::select(const css_element_selector& selector, bool apply_pseudo)
{
    if (_type == el_text || _type == el_space)
    {
        return select_no_match;
    }

    if (!selector.m_tag.empty() && selector.m_tag != _t("*"))
    {
        if (m_tag != selector.m_tag)
        {
            return select_no_match;
        }
    }

    int res = select_match;

    for (const auto &sa : selector.m_attrs)
    {
        const auto attr_value = get_attr(sa.attribute);

        switch (sa.condition)
        {
        case select_exists:
            if (attr_value.empty())
            {
                return select_no_match;
            }
            break;
        case select_equal:
            if (attr_value.empty())
            {
                return select_no_match;
            }
            else
            {
                if (sa.attribute == _t("class"))
                {
                    auto tokens1 = split_string(attr_value);
                    auto tokens2 = split_string(sa.val);
                    bool found = true;

                    for (auto str1 = tokens2.begin(); str1 != tokens2.end() && found; str1++)
                    {
                        bool f = false;
                        for (auto str2 = tokens1.begin(); str2 != tokens1.end() && !f; str2++)
                        {
                            if (is_equal(str1->c_str(), str2->c_str()))
                            {
                                f = true;
                            }
                        }
                        if (!f)
                        {
                            found = false;
                        }
                    }

                    if (!found)
                    {
                        return select_no_match;
                    }
                }
                else
                {
                    if (!is_equal(sa.val, attr_value))
                    {
                        return select_no_match;
                    }
                }
            }
            break;
        case select_contain_str:
            if (attr_value.empty())
            {
                return select_no_match;
            }
            else if (!wcsstr(attr_value.c_str(), sa.val.c_str()))
            {
                return select_no_match;
            }
            break;
        case select_start_str:
            if (attr_value.empty())
            {
                return select_no_match;
            }
            else if (_wcsnicmp(attr_value.c_str(), sa.val.c_str(), sa.val.length()))
            {
                return select_no_match;
            }
            break;
        case select_end_str:
            if (attr_value.empty())
            {
                return select_no_match;
            }
            else if (_wcsnicmp(attr_value.c_str(), sa.val.c_str(), sa.val.length()))
            {
                const auto &s = attr_value.c_str() + attr_value.length() - sa.val.length() - 1;

                if (s < attr_value)
                {
                    return select_no_match;
                }
                if (sa.val != s)
                {
                    return select_no_match;
                }
            }
            break;
        case select_pseudo_element:
            if (sa.val == _t("after"))
            {
                res |= select_match_with_after;
            }
            else if (sa.val == _t("before"))
            {
                res |= select_match_with_before;
            }
            else
            {
                return select_no_match;
            }
            break;
        case select_pseudo_class:
            if (apply_pseudo)
            {
                if (!m_parent) return select_no_match;

                std::wstring selector_param;
                std::wstring selector_name;

                auto begin = sa.val.find_first_of(L'(');
                auto end = (begin == std::wstring::npos) ? std::wstring::npos : find_close_bracket(sa.val, begin);

                if (begin != std::wstring::npos && end != std::wstring::npos)
                {
                    selector_param = sa.val.substr(begin + 1, end - begin - 1);
                }

                if (begin != std::wstring::npos)
                {
                    selector_name = sa.val.substr(0, begin);
                    trim(selector_name);
                }
                else
                {
                    selector_name = sa.val;
                }

                int selector = value_index(selector_name, pseudo_class_strings);

                switch (selector)
                {
                case pseudo_class_only_child:
                    if (!m_parent->is_only_child(this, false))
                    {
                        return select_no_match;
                    }
                    break;
                case pseudo_class_only_of_type:
                    if (!m_parent->is_only_child(this, true))
                    {
                        return select_no_match;
                    }
                    break;
                case pseudo_class_first_child:
                    if (!m_parent->is_nth_child(this, 0, 1, false))
                    {
                        return select_no_match;
                    }
                    break;
                case pseudo_class_first_of_type:
                    if (!m_parent->is_nth_child(this, 0, 1, true))
                    {
                        return select_no_match;
                    }
                    break;
                case pseudo_class_last_child:
                    if (!m_parent->is_nth_last_child(this, 0, 1, false))
                    {
                        return select_no_match;
                    }
                    break;
                case pseudo_class_last_of_type:
                    if (!m_parent->is_nth_last_child(this, 0, 1, true))
                    {
                        return select_no_match;
                    }
                    break;
                case pseudo_class_nth_child:
                case pseudo_class_nth_of_type:
                case pseudo_class_nth_last_child:
                case pseudo_class_nth_last_of_type:
                {
                    if (selector_param.empty()) return select_no_match;

                    int num = 0;
                    int off = 0;

                    parse_nth_child_params(selector_param, num, off);
                    if (!num && !off) return select_no_match;
                    switch (selector)
                    {
                    case pseudo_class_nth_child:
                        if (!m_parent->is_nth_child(this, num, off, false))
                        {
                            return select_no_match;
                        }
                        break;
                    case pseudo_class_nth_of_type:
                        if (!m_parent->is_nth_child(this, num, off, true))
                        {
                            return select_no_match;
                        }
                        break;
                    case pseudo_class_nth_last_child:
                        if (!m_parent->is_nth_last_child(this, num, off, false))
                        {
                            return select_no_match;
                        }
                        break;
                    case pseudo_class_nth_last_of_type:
                        if (!m_parent->is_nth_last_child(this, num, off, true))
                        {
                            return select_no_match;
                        }
                        break;
                    }

                }
                    break;
                case pseudo_class_not:
                {
                    css_element_selector sel;
                    sel.parse(selector_param);
                    if (select(sel, apply_pseudo))
                    {
                        return select_no_match;
                    }
                }
                    break;
                default:
                    if (std::find(m_pseudo_classes.begin(), m_pseudo_classes.end(), sa.val) == m_pseudo_classes.end())
                    {
                        return select_no_match;
                    }
                    break;
                }
            }
            else
            {
                res |= select_match_pseudo_class;
            }
            break;
        }
    }
    return res;
}

element *element::find_ancestor(const css_selector& selector, bool apply_pseudo, bool* is_pseudo)
{
    if (!m_parent)
    {
        return nullptr;
    }
    int res = m_parent->select(selector, apply_pseudo);
    if (res != select_no_match)
    {
        if (is_pseudo)
        {
            *is_pseudo = (res & select_match_pseudo_class) != 0;
        }
        return m_parent;
    }
    return m_parent->find_ancestor(selector, apply_pseudo, is_pseudo);
}

int element::get_floats_height(element_float el_float) const
{
    if (is_floats_holder())
    {
        int h = 0;

        bool process = false;

        for (auto i = m_floats_left.begin(); i != m_floats_left.end(); i++)
        {
            process = false;
            switch (el_float)
            {
            case float_none:
                process = true;
                break;
            case float_left:
                if (i->clear_floats == clear_left || i->clear_floats == clear_both)
                {
                    process = true;
                }
                break;
            case float_right:
                if (i->clear_floats == clear_right || i->clear_floats == clear_both)
                {
                    process = true;
                }
                break;
            }
            if (process)
            {
                if (el_float == float_none)
                {
                    h = std::max(h, i->pos.bottom());
                }
                else
                {
                    h = std::max(h, i->pos.top());
                }
            }
        }


        for (auto i = m_floats_right.begin(); i != m_floats_right.end(); i++)
        {
            process = false;
            switch (el_float)
            {
            case float_none:
                process = true;
                break;
            case float_left:
                if (i->clear_floats == clear_left || i->clear_floats == clear_both)
                {
                    process = true;
                }
                break;
            case float_right:
                if (i->clear_floats == clear_right || i->clear_floats == clear_both)
                {
                    process = true;
                }
                break;
            }
            if (process)
            {
                if (el_float == float_none)
                {
                    h = std::max(h, i->pos.bottom());
                }
                else
                {
                    h = std::max(h, i->pos.top());
                }
            }
        }

        return h;
    }
    int h = m_parent->get_floats_height(el_float);
    return h - m_pos.y;
}

int element::get_left_floats_height() const
{
    if (is_floats_holder())
    {
        int h = 0;
        if (!m_floats_left.empty())
        {
            for (auto el = m_floats_left.begin(); el != m_floats_left.end(); el++)
            {
                h = std::max(h, el->pos.bottom());
            }
        }
        return h;
    }
    int h = m_parent->get_left_floats_height();
    return h - m_pos.y;
}

int element::get_right_floats_height() const
{
    if (is_floats_holder())
    {
        int h = 0;
        if (!m_floats_right.empty())
        {
            for (auto el = m_floats_right.begin(); el != m_floats_right.end(); el++)
            {
                h = std::max(h, el->pos.bottom());
            }
        }
        return h;
    }
    int h = m_parent->get_right_floats_height();
    return h - m_pos.y;
}

int element::get_line_left(int y)
{
    if (is_floats_holder())
    {
        if (m_cahe_line_left.is_valid && m_cahe_line_left.hash == y)
        {
            return m_cahe_line_left.val;
        }

        int w = 0;
        for (auto el = m_floats_left.begin(); el != m_floats_left.end(); el++)
        {
            if (y >= el->pos.top() && y < el->pos.bottom())
            {
                w = std::max(w, el->pos.right());
                if (w < el->pos.right())
                {
                    break;
                }
            }
        }
        m_cahe_line_left.set_value(y, w);
        return w;
    }
    int w = m_parent->get_line_left(y + m_pos.y);
    if (w < 0)
    {
        w = 0;
    }
    return w - (w ? m_pos.x : 0);
}

int element::get_line_right(int y, int def_right)
{
    if (_type == el_text || _type == el_space)
    {
        return def_right;
    }

    if (is_floats_holder())
    {
        if (m_cahe_line_right.is_valid && m_cahe_line_right.hash == y)
        {
            if (m_cahe_line_right.is_default)
            {
                return def_right;
            }
            else
            {
                return std::min(m_cahe_line_right.val, def_right);
            }
        }

        int w = def_right;
        m_cahe_line_right.is_default = true;
        for (auto el = m_floats_right.begin(); el != m_floats_right.end(); el++)
        {
            if (y >= el->pos.top() && y < el->pos.bottom())
            {
                w = std::min(w, el->pos.left());
                m_cahe_line_right.is_default = false;
                if (w > el->pos.left())
                {
                    break;
                }
            }
        }
        m_cahe_line_right.set_value(y, w);
        return w;
    }
    int w = m_parent->get_line_right(y + m_pos.y, def_right + m_pos.x);
    return w - m_pos.x;
}


void element::get_line_left_right(int y, int def_right, int& ln_left, int& ln_right)
{
    if (is_floats_holder())
    {
        ln_left = get_line_left(y);
        ln_right = get_line_right(y, def_right);
    }
    else
    {
        m_parent->get_line_left_right(y + m_pos.y, def_right + m_pos.x, ln_left, ln_right);
        ln_right -= m_pos.x;

        if (ln_left < 0)
        {
            ln_left = 0;
        }
        else if (ln_left)
        {
            ln_left -= m_pos.x;
        }
    }
}

int element::fix_line_width(render_win32 &renderer, int max_width, element_float flt)
{
    int ret_width = 0;
    if (!m_boxes.empty())
    {
        std::vector<element*> els;
        m_boxes.back()->get_elements(els);
        bool was_cleared = false;
        if (!els.empty() && els.front()->get_clear() != clear_none)
        {
            if (els.front()->get_clear() == clear_both)
            {
                was_cleared = true;
            }
            else
            {
                if ((flt == float_left && els.front()->get_clear() == clear_left) ||
                    (flt == float_right && els.front()->get_clear() == clear_right))
                {
                    was_cleared = true;
                }
            }
        }

        if (!was_cleared)
        {
            m_boxes.pop_back();

            for (auto i = els.begin(); i != els.end(); i++)
            {
                int rw = place_element(renderer, (*i), max_width);
                if (rw > ret_width)
                {
                    ret_width = rw;
                }
            }
        }
        else
        {
            int line_top = 0;
            if (m_boxes.back()->get_type() == box_line)
            {
                line_top = m_boxes.back()->top();
            }
            else
            {
                line_top = m_boxes.back()->bottom();
            }

            int line_left = 0;
            int line_right = max_width;
            get_line_left_right(line_top, max_width, line_left, line_right);

            if (m_boxes.back()->get_type() == box_line)
            {
                if (m_boxes.size() == 1 && m_list_style_type != list_style_type_none && m_list_style_position == list_style_position_inside)
                {
                    int sz_font = get_font_size();
                    line_left += sz_font;
                }

                if (m_css_text_indent.val() != 0)
                {
                    bool line_box_found = false;
                    for (auto iter = m_boxes.begin(); iter < m_boxes.end(); iter++)
                    {
                        if ((*iter)->get_type() == box_line)
                        {
                            line_box_found = true;
                            break;
                        }
                    }
                    if (!line_box_found)
                    {
                        line_left += m_css_text_indent.calc_percent(max_width);
                    }
                }

            }

            std::vector<element*> els;
            m_boxes.back()->new_width(line_left, line_right, els);
            for (auto i = els.begin(); i != els.end(); i++)
            {
                int rw = place_element(renderer, (*i), max_width);
                if (rw > ret_width)
                {
                    ret_width = rw;
                }
            }
        }
    }

    return ret_width;
}

void element::add_float(element *el, int x, int y)
{
    if (is_floats_holder())
    {
        floated_box fb;
        fb.pos.x = el->left() + x;
        fb.pos.y = el->top() + y;
        fb.pos.width = el->width();
        fb.pos.height = el->height();
        fb.float_side = el->get_float();
        fb.clear_floats = el->get_clear();
        fb.el = el;

        if (fb.float_side == float_left)
        {
            if (m_floats_left.empty())
            {
                m_floats_left.push_back(fb);
            }
            else
            {
                bool inserted = false;
                for (auto i = m_floats_left.begin(); i != m_floats_left.end(); i++)
                {
                    if (fb.pos.right() > i->pos.right())
                    {
                        m_floats_left.insert(i, fb);
                        inserted = true;
                        break;
                    }
                }
                if (!inserted)
                {
                    m_floats_left.push_back(fb);
                }
            }
            m_cahe_line_left.invalidate();
        }
        else if (fb.float_side == float_right)
        {
            if (m_floats_right.empty())
            {
                m_floats_right.push_back(fb);
            }
            else
            {
                bool inserted = false;
                for (auto i = m_floats_right.begin(); i != m_floats_right.end(); i++)
                {
                    if (fb.pos.left() < i->pos.left())
                    {
                        m_floats_right.insert(i, fb);
                        inserted = true;
                        break;
                    }
                }
                if (!inserted)
                {
                    m_floats_right.push_back(fb);
                }
            }
            m_cahe_line_right.invalidate();
        }
    }
    else
    {
        m_parent->add_float(el, x + m_pos.x, y + m_pos.y);
    }
}

int element::find_next_line_top(int top, int width, int def_right)
{
    if (is_floats_holder())
    {
        int new_top = top;
        std::vector<int> points;

        for (auto el = m_floats_left.begin(); el != m_floats_left.end(); el++)
        {
            if (el->pos.top() >= top)
            {
                if (find(points.begin(), points.end(), el->pos.top()) == points.end())
                {
                    points.push_back(el->pos.top());
                }
            }
            if (el->pos.bottom() >= top)
            {
                if (find(points.begin(), points.end(), el->pos.bottom()) == points.end())
                {
                    points.push_back(el->pos.bottom());
                }
            }
        }

        for (auto el = m_floats_right.begin(); el != m_floats_right.end(); el++)
        {
            if (el->pos.top() >= top)
            {
                if (find(points.begin(), points.end(), el->pos.top()) == points.end())
                {
                    points.push_back(el->pos.top());
                }
            }
            if (el->pos.bottom() >= top)
            {
                if (find(points.begin(), points.end(), el->pos.bottom()) == points.end())
                {
                    points.push_back(el->pos.bottom());
                }
            }
        }

        if (!points.empty())
        {
            sort(points.begin(), points.end(), std::less<int>());
            new_top = points.back();

            for (auto i = points.begin(); i != points.end(); i++)
            {
                int pos_left = 0;
                int pos_right = def_right;
                get_line_left_right((*i), def_right, pos_left, pos_right);

                if (pos_right - pos_left >= width)
                {
                    new_top = (*i);
                    break;
                }
            }
        }
        return new_top;
    }
    int new_top = m_parent->find_next_line_top(top + m_pos.y, width, def_right + m_pos.x);
    return new_top - m_pos.y;
}

void element::parse_background()
{
    // parse background-color
    m_bg.m_color = get_color(_t("background-color"), false, web_color(0, 0, 0, 0));

    // parse background-position
    auto str = get_style_property(_t("background-position"), false, _t("0% 0%"));

    if (!str.empty())
    {
        auto res = split_string(str, _t(" \t"));

        if (res.size() > 0)
        {
            if (res.size() == 1)
            {
                if (value_in_list(res[0], _t("left;right;center")))
                {
                    m_bg.m_position.x.fromString(res[0], _t("left;right;center"));
                    m_bg.m_position.y.set_value(50, css_units_percentage);
                }
                else if (value_in_list(res[0], _t("top;bottom;center")))
                {
                    m_bg.m_position.y.fromString(res[0], _t("top;bottom;center"));
                    m_bg.m_position.x.set_value(50, css_units_percentage);
                }
                else
                {
                    m_bg.m_position.x.fromString(res[0], _t("left;right;center"));
                    m_bg.m_position.y.set_value(50, css_units_percentage);
                }
            }
            else
            {
                if (value_in_list(res[0], _t("left;right")))
                {
                    m_bg.m_position.x.fromString(res[0], _t("left;right;center"));
                    m_bg.m_position.y.fromString(res[1], _t("top;bottom;center"));
                }
                else if (value_in_list(res[0], _t("top;bottom")))
                {
                    m_bg.m_position.x.fromString(res[1], _t("left;right;center"));
                    m_bg.m_position.y.fromString(res[0], _t("top;bottom;center"));
                }
                else if (value_in_list(res[1], _t("left;right")))
                {
                    m_bg.m_position.x.fromString(res[1], _t("left;right;center"));
                    m_bg.m_position.y.fromString(res[0], _t("top;bottom;center"));
                }
                else if (value_in_list(res[1], _t("top;bottom")))
                {
                    m_bg.m_position.x.fromString(res[0], _t("left;right;center"));
                    m_bg.m_position.y.fromString(res[1], _t("top;bottom;center"));
                }
                else
                {
                    m_bg.m_position.x.fromString(res[0], _t("left;right;center"));
                    m_bg.m_position.y.fromString(res[1], _t("top;bottom;center"));
                }
            }

            if (m_bg.m_position.x.is_predefined())
            {
                switch (m_bg.m_position.x.predef())
                {
                case 0:
                    m_bg.m_position.x.set_value(0, css_units_percentage);
                    break;
                case 1:
                    m_bg.m_position.x.set_value(100, css_units_percentage);
                    break;
                case 2:
                    m_bg.m_position.x.set_value(50, css_units_percentage);
                    break;
                }
            }
            if (m_bg.m_position.y.is_predefined())
            {
                switch (m_bg.m_position.y.predef())
                {
                case 0:
                    m_bg.m_position.y.set_value(0, css_units_percentage);
                    break;
                case 1:
                    m_bg.m_position.y.set_value(100, css_units_percentage);
                    break;
                case 2:
                    m_bg.m_position.y.set_value(50, css_units_percentage);
                    break;
                }
            }
        }
        else
        {
            m_bg.m_position.x.set_value(0, css_units_percentage);
            m_bg.m_position.y.set_value(0, css_units_percentage);
        }
    }
    else
    {
        m_bg.m_position.y.set_value(0, css_units_percentage);
        m_bg.m_position.x.set_value(0, css_units_percentage);
    }

    str = get_style_property(_t("background-size"), false, _t("auto"));

    if (!str.empty())
    {
        auto res = split_string(str, _t(" \t"));

        if (!res.empty())
        {
            m_bg.m_position.width.fromString(res[0], background_size_strings);
            if (res.size() > 1)
            {
                m_bg.m_position.height.fromString(res[1], background_size_strings);
            }
            else
            {
                m_bg.m_position.height.predef(background_size_auto);
            }
        }
        else
        {
            m_bg.m_position.width.predef(background_size_auto);
            m_bg.m_position.height.predef(background_size_auto);
        }
    }

    m_doc.cvt_units(m_bg.m_position.x, m_font_size);
    m_doc.cvt_units(m_bg.m_position.y, m_font_size);
    m_doc.cvt_units(m_bg.m_position.width, m_font_size);
    m_doc.cvt_units(m_bg.m_position.height, m_font_size);

    // parse background_attachment
    m_bg.m_attachment = (background_attachment) value_index(
        get_style_property(_t("background-attachment"), false, _t("scroll")),
        background_attachment_strings,
        background_attachment_scroll);

    // parse background_attachment
    m_bg.m_repeat = (background_repeat) value_index(
        get_style_property(_t("background-repeat"), false, _t("repeat")),
        background_repeat_strings,
        background_repeat_repeat);

    // parse background_clip
    m_bg.m_clip = (background_box) value_index(
        get_style_property(_t("background-clip"), false, _t("border-box")),
        background_box_strings,
        background_box_border);

    // parse background_origin
    m_bg.m_origin = (background_box) value_index(
        get_style_property(_t("background-origin"), false, _t("padding-box")),
        background_box_strings,
        background_box_content);

    // parse background-image
    m_bg.m_image = css::parse_css_url(get_style_property(_t("background-image"), false));
    m_bg.m_baseurl = get_style_property(_t("background-image-baseurl"), false);

    if (!m_bg.m_image.empty())
    {
        m_doc.load_image(m_bg.m_image, m_bg.m_baseurl.empty() ? L"" : m_bg.m_baseurl);
    }
}

void element::add_positioned(element *el)
{
    if (m_el_position != element_position_static || (!m_parent))
    {
        m_positioned.push_back(el);
    }
    else
    {
        m_parent->add_positioned(el);
    }
}

void element::calc_outlines(int parent_width)
{
    m_padding.left = m_css_padding.left.calc_percent(parent_width);
    m_padding.right = m_css_padding.right.calc_percent(parent_width);

    m_borders.left = m_css_borders.left.width.calc_percent(parent_width);
    m_borders.right = m_css_borders.right.width.calc_percent(parent_width);

    m_margins.left = m_css_margins.left.calc_percent(parent_width);
    m_margins.right = m_css_margins.right.calc_percent(parent_width);

    m_margins.top = m_css_margins.top.calc_percent(parent_width);
    m_margins.bottom = m_css_margins.bottom.calc_percent(parent_width);

    m_padding.top = m_css_padding.top.calc_percent(parent_width);
    m_padding.bottom = m_css_padding.bottom.calc_percent(parent_width);

    if (m_display == display_block || m_display == display_table)
    {
        if (m_css_margins.left.is_predefined() && m_css_margins.right.is_predefined())
        {
            int el_width = m_pos.width + m_borders.left + m_borders.right + m_padding.left + m_padding.right;
            if (el_width <= parent_width)
            {
                m_margins.left = (parent_width - el_width) / 2;
                m_margins.right = (parent_width - el_width) - m_margins.left;
            }
            else
            {
                m_margins.left = 0;
                m_margins.right = 0;
            }
        }
        else if (m_css_margins.left.is_predefined() && !m_css_margins.right.is_predefined())
        {
            int el_width = m_pos.width + m_borders.left + m_borders.right + m_padding.left + m_padding.right + m_margins.right;
            m_margins.left = parent_width - el_width;
            if (m_margins.left < 0) m_margins.left = 0;
        }
        else if (!m_css_margins.left.is_predefined() && m_css_margins.right.is_predefined())
        {
            int el_width = m_pos.width + m_borders.left + m_borders.right + m_padding.left + m_padding.right + m_margins.left;
            m_margins.right = parent_width - el_width;
            if (m_margins.right < 0) m_margins.right = 0;
        }
    }
}

void element::parse_attributes()
{
    if (_type == el_tr)
    {
        auto str = get_attr(_t("align"));

        if (!str.empty())
        {
            m_style.add_property(_t("text-align"), str, L"", false);
        }

        str = get_attr(_t("valign"));

        if (!str.empty())
        {
            m_style.add_property(_t("vertical-align"), str, L"", false);
        }
    }
    else if (_type == el_title)
    {
        m_doc.set_caption(get_text());
    }
    else if (_type == el_td)
    {
        auto str = get_attr(_t("width"));

        if (!str.empty())
        {
            m_style.add_property(_t("width"), str, L"", false);
        }

        str = get_attr(_t("background"));

        if (!str.empty())
        {
            std::wstring url = _t("url('");
            url += str;
            url += _t("')");
            m_style.add_property(_t("background-image"), url, L"", false);
        }

        str = get_attr(_t("align"));

        if (!str.empty())
        {
            m_style.add_property(_t("text-align"), str, L"", false);
        }

        str = get_attr(_t("valign"));

        if (!str.empty())
        {
            m_style.add_property(_t("vertical-align"), str, L"", false);
        }
    }
    else if (_type == el_table)
    {
        auto str = get_attr(_t("width"));

        if (!str.empty())
        {
            m_style.add_property(_t("width"), str, L"", false);
        }

        str = get_attr(_t("align"));

        if (!str.empty())
        {
            int align = value_index(str, _t("left;center;right"));

            switch (align)
            {
            case 1:
                m_style.add_property(_t("margin-left"), _t("auto"), L"", false);
                m_style.add_property(_t("margin-right"), _t("auto"), L"", false);
                break;
            case 2:
                m_style.add_property(_t("margin-left"), _t("auto"), L"", false);
                m_style.add_property(_t("margin-right"), _t("0"), L"", false);
                break;
            }
        }

        str = get_attr(_t("cellspacing"));

        if (!str.empty())
        {
            std::wstring val = str;
            val += _t(" ");
            val += str;
            m_style.add_property(_t("border-spacing"), val, L"", false);
        }

        str = get_attr(_t("border"));

        if (!str.empty())
        {
            m_style.add_property(_t("border-width"), str, L"", false);
        }
    }
    else if (_type == el_style)
    {
        m_doc.add_stylesheet(m_text, L"", get_attr(_t("media"), L""));
    }
    else if (_type == el_para)
    {
        const auto &str = get_attr(_t("align"));

        if (!str.empty())
        {
            m_style.add_property(_t("text-align"), str, L"", false);
        }
    }
    if (_type == el_link)
    {
        if (!_loaded)
        {
            _loaded = true;

            const auto &rel = get_attr(_t("rel"));

            if (!rel.empty() && rel == _t("stylesheet"))
            {
                const auto &media = get_attr(_t("media"));
                const auto &href = get_attr(_t("href"));

                if (!href.empty())
                {
                    m_doc.import_css(href, empty);
                }
            }
            else
            {
                m_doc.link(this);
            }
        }
    }
    else if (_type == el_image)
    {
        m_src = get_attr(_t("src"));

        const auto &attr_height = get_attr(_t("height"));

        if (!attr_height.empty())
        {
            m_style.add_property(_t("height"), attr_height, empty, false);
        }

        const auto &attr_width = get_attr(_t("width"));

        if (!attr_width.empty())
        {
            m_style.add_property(_t("width"), attr_width, empty, false);
        }
    }
    else if (_type == el_font)
    {
        auto str = get_attr(_t("color"));

        if (!str.empty())
        {
            m_style.add_property(_t("color"), str, empty, false);
        }

        str = get_attr(_t("face"));

        if (!str.empty())
        {
            m_style.add_property(_t("font-face"), str, empty, false);
        }

        str = get_attr(_t("size"));

        if (!str.empty())
        {
            int sz = std::stoi(str);
            if (sz <= 1)
            {
                m_style.add_property(_t("font-size"), _t("x-small"), empty, false);
            }
            else if (sz >= 6)
            {
                m_style.add_property(_t("font-size"), _t("xx-large"), empty, false);
            }
            else
            {
                switch (sz)
                {
                case 2:
                    m_style.add_property(_t("font-size"), _t("small"), empty, false);
                    break;
                case 3:
                    m_style.add_property(_t("font-size"), _t("medium"), empty, false);
                    break;
                case 4:
                    m_style.add_property(_t("font-size"), _t("large"), empty, false);
                    break;
                case 5:
                    m_style.add_property(_t("font-size"), _t("x-large"), empty, false);
                    break;
                }
            }
        }
    }
    else if (_type == el_div)
    {
        const auto &str = get_attr(_t("align"));

        if (!str.empty())
        {
            m_style.add_property(_t("text-align"), str, empty, false);
        }
    }
    else if (_type == el_break)
    {
        const auto &attr_clear = get_attr(_t("clear"));

        if (!attr_clear.empty())
        {
            m_style.add_property(_t("clear"), attr_clear, empty, false);
        }
    }
    else if (_type == el_base)
    {
        m_doc.set_base_url(get_attr(_t("href")));
        return; //?
    }

    for (const auto &child : m_children)
    {
        child->parse_attributes();
    }
}

const std::wstring element::get_text() const
{
    if (_type == el_cdata || _type == el_comment || _type == el_text || _type == el_style || _type == el_space)
    {
        return m_text;
    }

    std::wstring result;

    for (const auto &child : m_children)
    {
        result += child->get_text();
    }

    return result;
}

void element::set_data(const std::wstring &data)
{
    if (_type == el_cdata || _type == el_comment)
    {
        m_text += data;
    }
}

void element::get_inline_boxes(position::vector& boxes)
{
    if (_type == el_tr)
    {
        for (const auto &el : m_children)
        {
            if (el->get_display() == display_table_cell)
            {
                position pos;

                pos.x = el->left() + el->margin_left();
                pos.y = el->top() - m_padding.top - m_borders.top;
                pos.width = el->right() - pos.x - el->margin_right() - el->margin_left();
                pos.height = el->height() + m_padding.top + m_padding.bottom + m_borders.top + m_borders.bottom;

                boxes.push_back(pos);
            }
        }
    }
    else
    {
        box* old_box = 0;
        position pos;
        for (const auto &el : m_children)
        {
            if (!el->skip())
            {
                if (el->m_box)
                {
                    if (el->m_box != old_box)
                    {
                        if (old_box)
                        {
                            if (boxes.empty())
                            {
                                pos.x -= m_padding.left + m_borders.left;
                                pos.width += m_padding.left + m_borders.left;
                            }
                            boxes.push_back(pos);
                        }
                        old_box = el->m_box;
                        pos.x = el->left() + el->margin_left();
                        pos.y = el->top() - m_padding.top - m_borders.top;
                        pos.width = 0;
                        pos.height = 0;
                    }
                    pos.width = el->right() - pos.x - el->margin_right() - el->margin_left();
                    pos.height = std::max(pos.height, el->height() + m_padding.top + m_padding.bottom + m_borders.top + m_borders.bottom);
                }
                else if (el->get_display() == display_inline)
                {
                    position::vector sub_boxes;
                    el->get_inline_boxes(sub_boxes);
                    if (!sub_boxes.empty())
                    {
                        sub_boxes.rbegin()->width += el->margin_right();
                        if (boxes.empty())
                        {
                            if (m_padding.left + m_borders.left > 0)
                            {
                                position padding_box = (*sub_boxes.begin());
                                padding_box.x -= m_padding.left + m_borders.left + el->margin_left();
                                padding_box.width = m_padding.left + m_borders.left + el->margin_left();
                                boxes.push_back(padding_box);
                            }
                        }

                        sub_boxes.rbegin()->width += el->margin_right();

                        boxes.insert(boxes.end(), sub_boxes.begin(), sub_boxes.end());
                    }
                }
            }
        }
        if (pos.width || pos.height)
        {
            if (boxes.empty())
            {
                pos.x -= m_padding.left + m_borders.left;
                pos.width += m_padding.left + m_borders.left;
            }
            boxes.push_back(pos);
        }
        if (!boxes.empty())
        {
            if (m_padding.right + m_borders.right > 0)
            {
                position padding_box = (*boxes.rbegin());
                padding_box.x += padding_box.width;
                padding_box.width = m_padding.right + m_borders.right;
                boxes.push_back(padding_box);
            }
        }
    }
}

bool element::on_mouse_over()
{
    bool ret = false;
    element *el = this;

    while (el)
    {
        if (el->set_pseudo_class(_t("hover"), true))
        {
            ret = true;
        }
        el = el->parent();
    }

    return ret;
}

bool element::find_styles_changes(position::vector& redraw_boxes, int x, int y)
{
    if (m_display == display_inline_text)
    {
        return false;
    }

    bool ret = false;
    bool apply = false;
    for (auto iter = m_used_styles.begin(); iter != m_used_styles.end() && !apply; iter++)
    {
        if (iter->m_selector->is_media_valid())
        {
            int res = select(*(iter->m_selector), true);
            if ((res == select_no_match && iter->m_used) || (res == select_match && !iter->m_used))
            {
                apply = true;
            }
        }
    }

    if (apply)
    {
        if (m_display == display_inline || m_display == display_table_row)
        {
            position::vector boxes;
            get_inline_boxes(boxes);
            for (auto pos = boxes.begin(); pos != boxes.end(); pos++)
            {
                pos->x += x;
                pos->y += y;
                redraw_boxes.push_back(*pos);
            }
        }
        else
        {
            position pos = m_pos;
            if (m_el_position != element_position_fixed)
            {
                pos.x += x;
                pos.y += y;
            }
            pos += m_padding;
            pos += m_borders;
            redraw_boxes.push_back(pos);
        }

        ret = true;
        refresh_styles();
        parse_styles();
    }
    for (const auto &child : m_children)
    {
        if (!child->skip())
        {
            if (m_el_position != element_position_fixed)
            {
                if (child->find_styles_changes(redraw_boxes, x + m_pos.x, y + m_pos.y))
                {
                    ret = true;
                }
            }
            else
            {
                if (child->find_styles_changes(redraw_boxes, m_pos.x, m_pos.y))
                {
                    ret = true;
                }
            }
        }
    }
    return ret;
}

bool element::on_mouse_leave()
{
    bool ret = false;
    element *el = this;

    while (el)
    {
        if (el->set_pseudo_class(_t("hover"), false))
        {
            ret = true;
        }
        if (el->set_pseudo_class(_t("active"), false))
        {
            ret = true;
        }
        el = el->parent();
    }

    return ret;
}

bool element::on_lbutton_down()
{
    return set_pseudo_class(_t("active"), true);
}

bool element::on_lbutton_up()
{
    bool ret = false;

    if (set_pseudo_class(_t("active"), false))
    {
        ret = true;
        on_click();
    }

    return ret;
}

void element::on_click()
{
    if (_type == el_anchor)
    {
        const auto &href = get_attr(_t("href"));

        if (!href.empty())
        {
            m_doc.on_anchor_click(href, this);
        }
    }
    else if (parent())
    {
        parent()->on_click();
    }
}

const std::wstring element::get_cursor() const
{
    return get_style_property(_t("cursor"), true);
}

static const int font_size_table[8][7] =
{
    { 9, 9, 9, 9, 11, 14, 18 },
    { 9, 9, 9, 10, 12, 15, 20 },
    { 9, 9, 9, 11, 13, 17, 22 },
    { 9, 9, 10, 12, 14, 18, 24 },
    { 9, 9, 10, 13, 16, 20, 26 },
    { 9, 9, 11, 14, 17, 21, 28 },
    { 9, 10, 12, 15, 17, 23, 30 },
    { 9, 10, 13, 16, 18, 24, 32 }
};


void element::init_font()
{
    // initialize font size
    const auto str = get_style_property(_t("font-size"), false);

    int parent_sz = 0;
    int doc_font_size = m_doc.get_default_font_size();
    if (m_parent)
    {
        parent_sz = m_parent->get_font_size();
    }
    else
    {
        parent_sz = doc_font_size;
    }


    if (str.empty())
    {
        m_font_size = parent_sz;
    }
    else
    {
        m_font_size = parent_sz;

        css_length sz;
        sz.fromString(str, font_size_strings);
        if (sz.is_predefined())
        {
            int idx_in_table = doc_font_size - 9;
            if (idx_in_table >= 0 && idx_in_table <= 7)
            {
                if (sz.predef() >= fontSize_xx_small && sz.predef() <= fontSize_xx_large)
                {
                    m_font_size = font_size_table[idx_in_table][sz.predef()];
                }
                else
                {
                    m_font_size = doc_font_size;
                }
            }
            else
            {
                switch (sz.predef())
                {
                case fontSize_xx_small:
                    m_font_size = doc_font_size * 3 / 5;
                    break;
                case fontSize_x_small:
                    m_font_size = doc_font_size * 3 / 4;
                    break;
                case fontSize_small:
                    m_font_size = doc_font_size * 8 / 9;
                    break;
                case fontSize_large:
                    m_font_size = doc_font_size * 6 / 5;
                    break;
                case fontSize_x_large:
                    m_font_size = doc_font_size * 3 / 2;
                    break;
                case fontSize_xx_large:
                    m_font_size = doc_font_size * 2;
                    break;
                default:
                    m_font_size = doc_font_size;
                    break;
                }
            }
        }
        else
        {
            if (sz.units() == css_units_percentage)
            {
                m_font_size = sz.calc_percent(parent_sz);
            }
            else if (sz.units() == css_units_none)
            {
                m_font_size = parent_sz;
            }
            else
            {
                m_font_size = m_doc.cvt_units(sz, parent_sz);
            }
        }
    }

    // initialize font
    const auto name = get_style_property(_t("font-family"), true, _t("inherit"));
    const auto weight = get_style_property(_t("font-weight"), true, _t("normal"));
    const auto style = get_style_property(_t("font-style"), true, _t("normal"));
    const auto decoration = get_style_property(_t("text-decoration"), true, _t("none"));

    //ATLTRACE(L"font-weight = %s\n", weight.c_str());

    m_font = m_doc.get_font(name, m_font_size, weight, style, decoration, &m_font_metrics);
}

bool element::is_break() const
{
    if (_type == el_space)
    {
        auto ws = get_white_space();

        if (ws == white_space_pre ||
            ws == white_space_pre_line ||
            ws == white_space_pre_wrap)
        {
            if (m_text == _t("\n"))
            {
                return true;
            }
        }

        return false;
    }

    return _type == el_break;
}

void element::set_tagName(const std::wstring &name)
{
    m_tag = name;
}

void element::draw_background(render_win32 &renderer, int x, int y, const position* clip)
{
    position pos = m_pos;
    pos.x += x;
    pos.y += y;

    position el_pos = pos;
    el_pos += m_padding;
    el_pos += m_borders;

    if (m_display != display_inline && m_display != display_table_row)
    {
        if (el_pos.does_intersect(clip))
        {
            background* bg = get_background();
            if (bg)
            {
                background_paint bg_paint;
                init_background_paint(renderer, pos, bg_paint, bg);

                renderer.draw_background(renderer, bg_paint);
            }
            position border_box = pos;
            border_box += m_padding;
            border_box += m_borders;
            renderer.draw_borders(m_css_borders, border_box, parent() ? false : true);
        }
    }
    else
    {
        background* bg = get_background();

        position::vector boxes;
        get_inline_boxes(boxes);

        background_paint bg_paint;
        position content_box;

        for (auto box = boxes.begin(); box != boxes.end(); box++)
        {
            box->x += x;
            box->y += y;

            if (box->does_intersect(clip))
            {
                content_box = *box;
                content_box -= m_borders;
                content_box -= m_padding;

                if (bg)
                {
                    init_background_paint(renderer, content_box, bg_paint, bg);
                }

                css_borders bdr;

                // set left borders radius for the first box
                if (box == boxes.begin())
                {
                    bdr.radius.bottom_left_x = m_css_borders.radius.bottom_left_x;
                    bdr.radius.bottom_left_y = m_css_borders.radius.bottom_left_y;
                    bdr.radius.top_left_x = m_css_borders.radius.top_left_x;
                    bdr.radius.top_left_y = m_css_borders.radius.top_left_y;
                }

                // set right borders radius for the last box
                if (box == boxes.end() - 1)
                {
                    bdr.radius.bottom_right_x = m_css_borders.radius.bottom_right_x;
                    bdr.radius.bottom_right_y = m_css_borders.radius.bottom_right_y;
                    bdr.radius.top_right_x = m_css_borders.radius.top_right_x;
                    bdr.radius.top_right_y = m_css_borders.radius.top_right_y;
                }


                bdr.top = m_css_borders.top;
                bdr.bottom = m_css_borders.bottom;
                if (box == boxes.begin())
                {
                    bdr.left = m_css_borders.left;
                }
                if (box == boxes.end() - 1)
                {
                    bdr.right = m_css_borders.right;
                }


                if (bg)
                {
                    bg_paint.border_radius = bdr.radius;
                    renderer.draw_background(renderer, bg_paint);
                }

                renderer.draw_borders(bdr, *box, false);
            }
        }
    }
}

int element::render_inline(render_win32 &renderer, element *container, int max_width)
{
    if (_type == el_text || _type == el_space)
    {
        return 0;
    }

    int ret_width = 0;
    int rw = 0;
    for (const auto &child : m_children)
    {
        rw = container->place_element(renderer, child, max_width);

        if (rw > ret_width)
        {
            ret_width = rw;
        }
    }
    return ret_width;
}

int element::place_element(render_win32 &renderer, element *el, int max_width)
{
    if (el->get_display() == display_none) return 0;

    if (el->get_display() == display_inline)
    {
        return el->render_inline(renderer, this, max_width);
    }

    auto el_position = el->get_element_position();

    if (el_position == element_position_absolute || el_position == element_position_fixed)
    {
        int line_top = 0;
        if (!m_boxes.empty())
        {
            if (m_boxes.back()->get_type() == box_line)
            {
                line_top = m_boxes.back()->top();
                if (!m_boxes.back()->is_empty())
                {
                    line_top += line_height();
                }
            }
            else
            {
                line_top = m_boxes.back()->bottom();
            }
        }

        el->render(renderer, 0, line_top, max_width);
        el->m_pos.x += el->content_margins_left();
        el->m_pos.y += el->content_margins_top();

        return 0;
    }

    int ret_width = 0;

    switch (el->get_float())
    {
    case float_left:
    {
        int line_top = 0;
        if (!m_boxes.empty())
        {
            if (m_boxes.back()->get_type() == box_line)
            {
                line_top = m_boxes.back()->top();
            }
            else
            {
                line_top = m_boxes.back()->bottom();
            }
        }
        line_top = get_cleared_top(el, line_top);
        int line_left = 0;
        int line_right = max_width;
        get_line_left_right(line_top, max_width, line_left, line_right);

        el->render(renderer, line_left, line_top, line_right);
        if (el->right() > line_right)
        {
            int new_top = find_next_line_top(el->top(), el->width(), max_width);
            el->m_pos.x = get_line_left(new_top) + el->content_margins_left();
            el->m_pos.y = new_top + el->content_margins_top();
        }
        add_float(el, 0, 0);
        ret_width = fix_line_width(renderer, max_width, float_left);
        if (!ret_width)
        {
            ret_width = el->right();
        }
    }
        break;
    case float_right:
    {
        int line_top = 0;
        if (!m_boxes.empty())
        {
            if (m_boxes.back()->get_type() == box_line)
            {
                line_top = m_boxes.back()->top();
            }
            else
            {
                line_top = m_boxes.back()->bottom();
            }
        }
        line_top = get_cleared_top(el, line_top);
        int line_left = 0;
        int line_right = max_width;
        get_line_left_right(line_top, max_width, line_left, line_right);

        el->render(renderer, 0, line_top, line_right);

        if (line_left + el->width() > line_right)
        {
            int new_top = find_next_line_top(el->top(), el->width(), max_width);
            el->m_pos.x = get_line_right(new_top, max_width) - el->width() + el->content_margins_left();
            el->m_pos.y = new_top + el->content_margins_top();
        }
        else
        {
            el->m_pos.x = line_right - el->width() + el->content_margins_left();
        }
        add_float(el, 0, 0);
        ret_width = fix_line_width(renderer, max_width, float_right);

        if (!ret_width)
        {
            line_left = 0;
            line_right = max_width;
            get_line_left_right(line_top, max_width, line_left, line_right);

            ret_width = ret_width + (max_width - line_right);
        }
    }
        break;
    default:
    {
        int line_top = 0;
        if (!m_boxes.empty())
        {
            line_top = m_boxes.back()->top();
        }
        int line_left = 0;
        int line_right = max_width;
        get_line_left_right(line_top, max_width, line_left, line_right);

        switch (el->get_display())
        {
        case display_inline_block:
            ret_width = el->render(renderer, line_left, line_top, line_right);
            break;
        case display_block:
            if (el->is_replaced() || el->is_floats_holder())
            {
                el->m_pos.width = el->get_css_width().calc_percent(line_right - line_left);
                el->m_pos.height = el->get_css_height().calc_percent(0);
                if (el->m_pos.width || el->m_pos.height)
                {
                    el->calc_outlines(line_right - line_left);
                }
            }
            break;
        case display_inline_text:
        {
            size sz;
            el->get_content_size(renderer, sz, line_right);
            el->m_pos = sz;
        }
            break;
        default:
            ret_width = 0;
            break;
        }

        bool add_box = true;
        if (!m_boxes.empty())
        {
            if (m_boxes.back()->can_hold(el, m_white_space))
            {
                add_box = false;
            }
        }
        if (add_box)
        {
            line_top = new_box(el, max_width);
        }
        else if (!m_boxes.empty())
        {
            line_top = m_boxes.back()->top();
        }

        line_left = 0;
        line_right = max_width;
        get_line_left_right(line_top, max_width, line_left, line_right);

        if (!el->is_inline_box())
        {
            if (m_boxes.size() == 1)
            {
                if (collapse_top_margin())
                {
                    int shift = el->margin_top();
                    if (shift >= 0)
                    {
                        line_top -= shift;
                        m_boxes.back()->y_shift(-shift);
                    }
                }
            }
            else
            {
                int shift = 0;
                int prev_margin = m_boxes[m_boxes.size() - 2]->bottom_margin();

                if (prev_margin > el->margin_top())
                {
                    shift = el->margin_top();
                }
                else
                {
                    shift = prev_margin;
                }
                if (shift >= 0)
                {
                    line_top -= shift;
                    m_boxes.back()->y_shift(-shift);
                }
            }
        }

        switch (el->get_display())
        {
        case display_table:
        case display_list_item:
            ret_width = el->render(renderer, line_left, line_top, line_right - line_left);
            break;
        case display_block:
        case display_table_cell:
        case display_table_caption:
        case display_table_row:
            if (el->is_replaced() || el->is_floats_holder())
            {
                ret_width = el->render(renderer, line_left, line_top, line_right - line_left) + line_left + (max_width - line_right);
            }
            else
            {
                ret_width = el->render(renderer, 0, line_top, max_width);
            }
            break;
        default:
            ret_width = 0;
            break;
        }

        m_boxes.back()->add_element(el);

        if (el->is_inline_box() && !el->skip())
        {
            ret_width = el->right() + (max_width - line_right);
        }
    }
        break;
    }

    return ret_width;
}

bool element::set_pseudo_class(const std::wstring &pclass, bool add)
{
    bool ret = false;
    if (add)
    {
        if (std::find(m_pseudo_classes.begin(), m_pseudo_classes.end(), pclass) == m_pseudo_classes.end())
        {
            m_pseudo_classes.push_back(pclass);
            ret = true;
        }
    }
    else
    {
        auto pi = std::find(m_pseudo_classes.begin(), m_pseudo_classes.end(), pclass);
        if (pi != m_pseudo_classes.end())
        {
            m_pseudo_classes.erase(pi);
            ret = true;
        }
    }
    return ret;
}

int element::line_height() const
{
    if (_type == el_text || _type == el_space)
    {
        return m_parent->line_height();
    }
    else if (_type == el_image)
    {
        return height();
    }

    return m_line_height;
}

bool element::is_replaced() const
{
    return _type == el_image;
}

int element::finish_last_box(bool end_of_render)
{
    int line_top = 0;

    if (!m_boxes.empty())
    {
        m_boxes.back()->finish(end_of_render);

        if (m_boxes.back()->is_empty())
        {
            line_top = m_boxes.back()->top();
            m_boxes.pop_back();
        }

        if (!m_boxes.empty())
        {
            line_top = m_boxes.back()->bottom();
        }
    }
    return line_top;
}

int element::new_box(element *el, int max_width)
{
    int line_top = get_cleared_top(el, finish_last_box());

    int line_left = 0;
    int line_right = max_width;
    get_line_left_right(line_top, max_width, line_left, line_right);

    if (el->is_inline_box() || el->is_floats_holder())
    {
        if (el->width() > line_right - line_left)
        {
            line_top = find_next_line_top(line_top, el->width(), max_width);
            line_left = 0;
            line_right = max_width;
            get_line_left_right(line_top, max_width, line_left, line_right);
        }
    }

    int first_line_margin = 0;
    if (m_boxes.empty() && m_list_style_type != list_style_type_none && m_list_style_position == list_style_position_inside)
    {
        int sz_font = get_font_size();
        first_line_margin = sz_font;
    }

    if (el->is_inline_box())
    {
        int text_indent = 0;
        if (m_css_text_indent.val() != 0)
        {
            bool line_box_found = false;
            for (auto iter = m_boxes.begin(); iter != m_boxes.end(); iter++)
            {
                if ((*iter)->get_type() == box_line)
                {
                    line_box_found = true;
                    break;
                }
            }
            if (!line_box_found)
            {
                text_indent = m_css_text_indent.calc_percent(max_width);
            }
        }

        font_metrics fm;
        get_font(&fm);

        m_boxes.push_back(new box(box_line, line_top, line_left + first_line_margin + text_indent, line_right, line_height(), fm, m_text_align));
    }
    else
    {
        m_boxes.push_back(new box(box_block, line_top, line_left, line_right, line_height(), m_font_metrics, m_text_align));
    }

    return line_top;
}

int element::get_cleared_top(element *el, int line_top)
{
    switch (el->get_clear())
    {
    case clear_left:
    {
        int fh = get_left_floats_height();
        if (fh && fh > line_top)
        {
            line_top = fh;
        }
    }
        break;
    case clear_right:
    {
        int fh = get_right_floats_height();
        if (fh && fh > line_top)
        {
            line_top = fh;
        }
    }
        break;
    case clear_both:
    {
        int fh = get_floats_height();
        if (fh && fh > line_top)
        {
            line_top = fh;
        }
    }
        break;
    default:
        if (el->get_float() != float_none)
        {
            int fh = get_floats_height(el->get_float());
            if (fh && fh > line_top)
            {
                line_top = fh;
            }
        }
        break;
    }
    return line_top;
}

style_display element::get_display() const
{
    if (_type == el_text || _type == el_space)
    {
        return display_inline_text;
    }

    return m_display;
}

element_float element::get_float() const
{
    return m_float;
}

bool element::is_floats_holder() const
{
    if (_type == el_text || _type == el_space)
    {
        return false;
    }

    return m_display == display_inline_block ||
        m_display == display_table_cell ||
        !m_parent ||
        is_body() ||
        m_float != float_none ||
        m_el_position == element_position_absolute ||
        m_el_position == element_position_fixed ||
        m_overflow > overflow_visible;
}

bool element::is_first_child_inline(element *el)
{
    for (const auto &child : m_children)
    {
        if (!child->is_white_space())
        {
            if (el == child)
            {
                return true;
            }
            if (child->get_display() == display_inline)
            {
                if (child->have_inline_child())
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
    }
    return false;
}

bool element::is_last_child_inline(element *el)
{
    for (std::vector<element*>::reverse_iterator iter = m_children.rbegin(); iter != m_children.rend(); iter++)
    {
        if (!(*iter)->is_white_space())
        {
            if (el == (*iter))
            {
                return true;
            }
            if ((*iter)->get_display() == display_inline)
            {
                if ((*iter)->have_inline_child())
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
    }
    return false;
}

white_space element::get_white_space() const
{
    if (_type == el_text || _type == el_space)
    {
        if (m_parent) return m_parent->get_white_space();
        return white_space_normal;
    }

    return m_white_space;
}

vertical_align element::get_vertical_align() const
{
    return m_vertical_align;
}

css_length element::get_css_left() const
{
    if (_type == el_text || _type == el_space)
    {
        return css_length();
    }

    return m_css_offsets.left;
}

css_length element::get_css_right() const
{
    if (_type == el_text || _type == el_space)
    {
        return css_length();
    }

    return m_css_offsets.right;
}

css_length element::get_css_top() const
{
    if (_type == el_text || _type == el_space)
    {
        return css_length();
    }

    return m_css_offsets.top;
}

css_length element::get_css_bottom() const
{
    if (_type == el_text || _type == el_space)
    {
        return css_length();
    }

    return m_css_offsets.bottom;
}


css_offsets element::get_css_offsets() const
{
    if (_type == el_text || _type == el_space)
    {
        auto p = parent();

        while (p && p->get_display() == display_inline)
        {
            if (p->get_element_position() == element_position_relative)
            {
                return p->get_css_offsets();
            }
            p = p->parent();
        }
    }

    return m_css_offsets;
}

element_clear element::get_clear() const
{
    if (_type == el_text || _type == el_space)
    {
        return clear_none;
    }   

    return m_clear;
}

css_length element::get_css_width() const
{
    if (_type == el_text || _type == el_space)
    {
        return css_length();
    }

    return m_css_width;
}

css_length element::get_css_height() const
{
    if (_type == el_text || _type == el_space)
    {
        return css_length();
    }

    return m_css_height;
}

size_t element::get_children_count() const
{
    return m_children.size();
}

element *element::get_child(int idx) const
{
    return m_children[idx];
}

void element::set_css_width(css_length& w)
{
    m_css_width = w;
}

void element::apply_vertical_align()
{
    if (!m_boxes.empty())
    {
        int add = 0;
        int content_height = m_boxes.back()->bottom();

        if (m_pos.height > content_height)
        {
            switch (m_vertical_align)
            {
            case va_middle:
                add = (m_pos.height - content_height) / 2;
                break;
            case va_bottom:
                add = m_pos.height - content_height;
                break;
            default:
                add = 0;
                break;
            }
        }

        if (add)
        {
            for (size_t i = 0; i < m_boxes.size(); i++)
            {
                m_boxes[i]->y_shift(add);
            }
        }
    }
}

element_position element::get_element_position(css_offsets* offsets) const
{

    if (_type == el_text || _type == el_space)
    {
        auto p = parent();
        while (p && p->get_display() == display_inline)
        {
            if (p->get_element_position() == element_position_relative)
            {
                if (offsets)
                {
                    *offsets = p->get_css_offsets();
                }
                return element_position_relative;
            }
            p = p->parent();
        }
        return element_position_static;
    }

    if (offsets && m_el_position != element_position_static)
    {
        *offsets = m_css_offsets;
    }
    return m_el_position;
}

void element::init_background_paint(render_win32 &renderer, position pos, background_paint &bg_paint, background* bg)
{
    if (!bg) return;


    bg_paint.attachment = bg->m_attachment;
    bg_paint.image = m_doc.find_image(bg->m_image, bg->m_baseurl);
    bg_paint.repeat = bg->m_repeat;
    bg_paint.color = bg->m_color;

    position content_box = pos;
    position padding_box = pos;
    padding_box += m_padding;
    position border_box = padding_box;
    border_box += m_borders;

    switch (bg->m_clip)
    {
    case background_box_padding:
        bg_paint.clip_box = padding_box;
        break;
    case background_box_content:
        bg_paint.clip_box = content_box;
        break;
    default:
        bg_paint.clip_box = border_box;
        break;
    }

    switch (bg->m_origin)
    {
    case background_box_border:
        bg_paint.origin_box = border_box;
        break;
    case background_box_content:
        bg_paint.origin_box = content_box;
        break;
    default:
        bg_paint.origin_box = padding_box;
        break;
    }

    if (bg_paint.image)
    {
        bg_paint.image_size = renderer.get_image_size(bg_paint.image);

        if (bg_paint.image_size.width && bg_paint.image_size.height)
        {
            size img_new_sz = bg_paint.image_size;
            double img_ar_width = (double) bg_paint.image_size.width / (double) bg_paint.image_size.height;
            double img_ar_height = (double) bg_paint.image_size.height / (double) bg_paint.image_size.width;


            if (bg->m_position.width.is_predefined())
            {
                switch (bg->m_position.width.predef())
                {
                case background_size_contain:
                    if ((int) ((double) bg_paint.origin_box.width * img_ar_height) <= bg_paint.origin_box.height)
                    {
                        img_new_sz.width = bg_paint.origin_box.width;
                        img_new_sz.height = (int) ((double) bg_paint.origin_box.width * img_ar_height);
                    }
                    else
                    {
                        img_new_sz.height = bg_paint.origin_box.height;
                        img_new_sz.width = (int) ((double) bg_paint.origin_box.height * img_ar_width);
                    }
                    break;
                case background_size_cover:
                    if ((int) ((double) bg_paint.origin_box.width * img_ar_height) >= bg_paint.origin_box.height)
                    {
                        img_new_sz.width = bg_paint.origin_box.width;
                        img_new_sz.height = (int) ((double) bg_paint.origin_box.width * img_ar_height);
                    }
                    else
                    {
                        img_new_sz.height = bg_paint.origin_box.height;
                        img_new_sz.width = (int) ((double) bg_paint.origin_box.height * img_ar_width);
                    }
                    break;
                    break;
                case background_size_auto:
                    if (!bg->m_position.height.is_predefined())
                    {
                        img_new_sz.height = bg->m_position.height.calc_percent(bg_paint.origin_box.height);
                        img_new_sz.width = (int) ((double) img_new_sz.height * img_ar_width);
                    }
                    break;
                }
            }
            else
            {
                img_new_sz.width = bg->m_position.width.calc_percent(bg_paint.origin_box.width);
                if (bg->m_position.height.is_predefined())
                {
                    img_new_sz.height = (int) ((double) img_new_sz.width * img_ar_height);
                }
                else
                {
                    img_new_sz.height = bg->m_position.height.calc_percent(bg_paint.origin_box.height);
                }
            }

            bg_paint.image_size = img_new_sz;
            bg_paint.position_x = bg_paint.origin_box.x + (int) bg->m_position.x.calc_percent(bg_paint.origin_box.width - bg_paint.image_size.width);
            bg_paint.position_y = bg_paint.origin_box.y + (int) bg->m_position.y.calc_percent(bg_paint.origin_box.height - bg_paint.image_size.height);
        }

    }
    bg_paint.border_radius = m_css_borders.radius;
    bg_paint.border_box = border_box;
    bg_paint.is_root = parent() ? false : true;
}

visibility element::get_visibility() const
{
    return m_visibility;
}

void element::draw_list_marker(render_win32 &renderer, const position &pos)
{
    list_marker lm;

    auto list_image = get_style_property(_t("list-style-image"), true);
    size img_size;

    if (!list_image.empty())
    {
        lm.image = css::parse_css_url(list_image);
        lm.baseurl = get_style_property(_t("list-style-image-baseurl"), true);
        img_size = renderer.get_image_size(m_doc.find_image(lm.image, lm.baseurl));
    }

    int ln_height = line_height();
    int sz_font = get_font_size();
    lm.pos.x = pos.x;
    lm.pos.width = sz_font - sz_font * 2 / 3;
    lm.pos.height = sz_font - sz_font * 2 / 3;
    lm.pos.y = pos.y + ln_height / 2 - lm.pos.height / 2;

    if (img_size.width && img_size.height)
    {
        if (lm.pos.y + img_size.height > pos.y + pos.height)
        {
            lm.pos.y = pos.y + pos.height - img_size.height;
        }
        if (img_size.width > lm.pos.width)
        {
            lm.pos.x -= img_size.width - lm.pos.width;
        }

        lm.pos.width = img_size.width;
        lm.pos.height = img_size.height;
    }
    if (m_list_style_position == list_style_position_outside)
    {
        lm.pos.x -= sz_font;
    }

    lm.color = get_color(_t("color"), true, web_color(0, 0, 0));
    lm.marker_type = m_list_style_type;
    renderer.draw_list_marker(lm);
}

void element::draw_children(render_win32 &renderer, int x, int y, const position* clip, draw_flag flag, int zindex)
{
    position pos = m_pos;
    pos.x += x;
    pos.y += y;

    if (_type == el_table)
    {
        for (int row = 0; row < m_grid.rows_count(); row++)
        {
            if (flag == draw_block)
            {
                m_grid.row(row).el_row->draw_background(renderer, pos.x, pos.y, clip);
            }
            for (int col = 0; col < m_grid.cols_count(); col++)
            {
                table_cell* cell = m_grid.cell(col, row);
                if (cell->el)
                {
                    if (flag == draw_block)
                    {
                        cell->el->draw(renderer, pos.x, pos.y, clip);
                    }
                    cell->el->draw_children(renderer, pos.x, pos.y, clip, flag, zindex);
                }
            }
        }
    }
    else
    {
        if (m_overflow > overflow_visible)
        {
            renderer.set_clip(pos, true, true);
        }

        auto browser_wnd = m_doc.client_pos();

        for (auto el : m_children)
        {
            if (el->is_visible())
            {
                switch (flag)
                {
                case draw_positioned:
                    if (el->is_positioned() && el->get_zindex() == zindex)
                    {
                        if (el->get_element_position() == element_position_fixed)
                        {
                            el->draw(renderer, browser_wnd.x, browser_wnd.y, clip);
                            el->draw_stacking_context(renderer, browser_wnd.x, browser_wnd.y, clip, true);
                        }
                        else
                        {
                            el->draw(renderer, pos.x, pos.y, clip);
                            el->draw_stacking_context(renderer, pos.x, pos.y, clip, true);
                        }
                        el = nullptr;
                    }
                    break;
                case draw_block:
                    if (!el->is_inline_box() && el->get_float() == float_none && !el->is_positioned())
                    {
                        el->draw(renderer, pos.x, pos.y, clip);
                    }
                    break;
                case draw_floats:
                    if (el->get_float() != float_none && !el->is_positioned())
                    {
                        el->draw(renderer, pos.x, pos.y, clip);
                        el->draw_stacking_context(renderer, pos.x, pos.y, clip, false);
                        el = nullptr;
                    }
                    break;
                case draw_inlines:
                    if (el->is_inline_box() && el->get_float() == float_none && !el->is_positioned())
                    {
                        el->draw(renderer, pos.x, pos.y, clip);
                        if (el->get_display() == display_inline_block)
                        {
                            el->draw_stacking_context(renderer, pos.x, pos.y, clip, false);
                            el = nullptr;
                        }
                    }
                    break;
                default:
                    break;
                }

                if (el)
                {
                    if (flag == draw_positioned)
                    {
                        if (!el->is_positioned())
                        {
                            el->draw_children(renderer, pos.x, pos.y, clip, flag, zindex);
                        }
                    }
                    else
                    {
                        if (el->get_float() == float_none &&
                            el->get_display() != display_inline_block &&
                            !el->is_positioned())
                        {
                            el->draw_children(renderer, pos.x, pos.y, clip, flag, zindex);
                        }
                    }
                }
            }
        }

        if (m_overflow > overflow_visible)
        {
            renderer.del_clip();
        }
    }
}

bool element::fetch_positioned()
{
    bool ret = false;

    m_positioned.clear();

    for (const auto &el : m_children)
    {
        if (el->get_element_position() != element_position_static)
        {
            add_positioned(el);
        }
        if (!ret && (el->get_element_position() == element_position_absolute || el->get_element_position() == element_position_fixed))
        {
            ret = true;
        }
        if (el->fetch_positioned())
        {
            ret = true;
        }
    }
    return ret;
}

int element::get_zindex() const
{
    return m_z_index;
}

void element::render_positioned(render_win32 &renderer, render_type rt)
{
    if (_type == el_text || _type == el_space)
    {
        return;
    }

    position wnd_position = m_doc.client_pos();
    element_position el_position;
    bool process;

    for (const auto &el : m_positioned)
    {
        el_position = el->get_element_position();

        process = false;
        if (el->get_display() != display_none)
        {
            if (el_position == element_position_absolute)
            {
                if (rt != render_fixed_only)
                {
                    process = true;
                }
            }
            else if (el_position == element_position_fixed)
            {
                if (rt != render_no_fixed)
                {
                    process = true;
                }
            }
        }

        if (process)
        {
            int parent_height = 0;
            int parent_width = 0;
            int client_x = 0;
            int client_y = 0;
            if (el_position == element_position_fixed)
            {
                parent_height = wnd_position.height;
                parent_width = wnd_position.width;
                client_x = wnd_position.left();
                client_y = wnd_position.top();
            }
            else
            {
                if (el->parent())
                {
                    parent_height = el->parent()->height();
                    parent_width = el->parent()->width();
                }
            }

            css_length css_left = el->get_css_left();
            css_length css_right = el->get_css_right();
            css_length css_top = el->get_css_top();
            css_length css_bottom = el->get_css_bottom();

            bool need_render = false;

            css_length el_w = el->get_css_width();
            css_length el_h = el->get_css_height();
            if (el_w.units() == css_units_percentage && parent_width)
            {
                int w = el_w.calc_percent(parent_width);
                if (el->m_pos.width != w)
                {
                    need_render = true;
                    el->m_pos.width = w;
                }
            }

            if (el_h.units() == css_units_percentage && parent_height)
            {
                int h = el_h.calc_percent(parent_height);
                if (el->m_pos.height != h)
                {
                    need_render = true;
                    el->m_pos.height = h;
                }
            }

            bool cvt_x = false;
            bool cvt_y = false;

            if (el_position == element_position_fixed)
            {
                if (!css_left.is_predefined() || !css_right.is_predefined())
                {
                    if (!css_left.is_predefined() && css_right.is_predefined())
                    {
                        el->m_pos.x = css_left.calc_percent(parent_width) + el->content_margins_left();
                    }
                    else if (css_left.is_predefined() && !css_right.is_predefined())
                    {
                        el->m_pos.x = parent_width - css_right.calc_percent(parent_width) - el->m_pos.width - el->content_margins_right();
                    }
                    else
                    {
                        el->m_pos.x = css_left.calc_percent(parent_width) + el->content_margins_left();
                        el->m_pos.width = parent_width - css_left.calc_percent(parent_width) - css_right.calc_percent(parent_width) - (el->content_margins_left() + el->content_margins_right());
                        need_render = true;
                    }
                }

                if (!css_top.is_predefined() || !css_bottom.is_predefined())
                {
                    if (!css_top.is_predefined() && css_bottom.is_predefined())
                    {
                        el->m_pos.y = css_top.calc_percent(parent_height) + el->content_margins_top();
                    }
                    else if (css_top.is_predefined() && !css_bottom.is_predefined())
                    {
                        el->m_pos.y = parent_height - css_bottom.calc_percent(parent_height) - el->m_pos.height - el->content_margins_bottom();
                    }
                    else
                    {
                        el->m_pos.y = css_top.calc_percent(parent_height) + el->content_margins_top();
                        el->m_pos.height = parent_height - css_top.calc_percent(parent_height) - css_bottom.calc_percent(parent_height) - (el->content_margins_top() + el->content_margins_bottom());
                        need_render = true;
                    }
                }
            }
            else
            {
                if (!css_left.is_predefined() || !css_right.is_predefined())
                {
                    if (!css_left.is_predefined() && css_right.is_predefined())
                    {
                        el->m_pos.x = css_left.calc_percent(parent_width) + el->content_margins_left() - m_padding.left;
                    }
                    else if (css_left.is_predefined() && !css_right.is_predefined())
                    {
                        el->m_pos.x = m_pos.width + m_padding.right - css_right.calc_percent(parent_width) - el->m_pos.width - el->content_margins_right();
                    }
                    else
                    {
                        el->m_pos.x = css_left.calc_percent(parent_width) + el->content_margins_left() - m_padding.left;
                        el->m_pos.width = m_pos.width + m_padding.left + m_padding.right - css_left.calc_percent(parent_width) - css_right.calc_percent(parent_width) - (el->content_margins_left() + el->content_margins_right());
                        need_render = true;
                    }
                    cvt_x = true;
                }

                if (!css_top.is_predefined() || !css_bottom.is_predefined())
                {
                    if (!css_top.is_predefined() && css_bottom.is_predefined())
                    {
                        el->m_pos.y = css_top.calc_percent(parent_height) + el->content_margins_top() - m_padding.top;
                    }
                    else if (css_top.is_predefined() && !css_bottom.is_predefined())
                    {
                        el->m_pos.y = m_pos.height + m_padding.bottom - css_bottom.calc_percent(parent_height) - el->m_pos.height - el->content_margins_bottom();
                    }
                    else
                    {
                        el->m_pos.y = css_top.calc_percent(parent_height) + el->content_margins_top() - m_padding.top;
                        el->m_pos.height = m_pos.height + m_padding.top + m_padding.bottom - css_top.calc_percent(parent_height) - css_bottom.calc_percent(parent_height) - (el->content_margins_top() + el->content_margins_bottom());
                        need_render = true;
                    }
                    cvt_y = true;
                }
            }

            if (cvt_x || cvt_y)
            {
                int offset_x = 0;
                int offset_y = 0;
                auto cur_el = el->parent();
                auto pThis = this;

                while (cur_el && cur_el != pThis)
                {
                    offset_x += cur_el->m_pos.x;
                    offset_y += cur_el->m_pos.y;
                    cur_el = cur_el->parent();
                }
                if (cvt_x) el->m_pos.x -= offset_x;
                if (cvt_y) el->m_pos.y -= offset_y;
            }

            if (need_render)
            {
                position pos = el->m_pos;
                el->render(renderer, el->left(), el->top(), el->width(), true);
                el->m_pos = pos;
            }

            if (el_position == element_position_fixed)
            {
                position fixed_pos;
                el->get_redraw_box(fixed_pos);
                m_doc.add_fixed_box(fixed_pos);
            }
        }

        el->render_positioned(renderer);
    }

    if (!m_positioned.empty())
    {
        std::stable_sort(m_positioned.begin(), m_positioned.end(), element_zindex_sort());
    }
}

void element::draw_stacking_context(render_win32 &renderer, int x, int y, const position* clip, bool with_positioned)
{
    if (is_visible())
    {
        std::set<int> zindexes;

        if (with_positioned)
        {
            for (const auto &el : m_positioned)
            {
                zindexes.insert(el->get_zindex());
            }

            for (const auto &idx : zindexes)
            {
                if (idx < 0)
                {
                    draw_children(renderer, x, y, clip, draw_positioned, idx);
                }
            }
        }

        draw_children(renderer, x, y, clip, draw_block, 0);
        draw_children(renderer, x, y, clip, draw_floats, 0);
        draw_children(renderer, x, y, clip, draw_inlines, 0);

        if (with_positioned)
        {
            for (const auto &idx : zindexes)
            {
                if (idx == 0)
                {
                    draw_children(renderer, x, y, clip, draw_positioned, idx);
                }
            }

            for (const auto &idx : zindexes)
            {
                if (idx > 0)
                {
                    draw_children(renderer, x, y, clip, draw_positioned, idx);
                }
            }
        }
    }
}

overflow element::get_overflow() const
{
    return m_overflow;
}

bool element::is_nth_child(element *el, int num, int off, bool of_type)
{
    int idx = 1;

    for (const auto &child : m_children)
    {
        if (child->get_display() != display_inline_text)
        {
            if ((!of_type) || (of_type && el->get_tagName() == child->get_tagName()))
            {
                if (el == child)
                {
                    if (num != 0)
                    {
                        if ((idx - off) >= 0 && (idx - off) % num == 0)
                        {
                            return true;
                        }

                    }
                    else if (idx == off)
                    {
                        return true;
                    }
                    return false;
                }
                idx++;
            }
            if (el == child) break;
        }
    }
    return false;
}

bool element::is_nth_last_child(element *el, int num, int off, bool of_type)
{
    int idx = 1;
    for (std::vector<element*>::reverse_iterator child = m_children.rbegin(); child != m_children.rend(); child++)
    {
        if ((*child)->get_display() != display_inline_text)
        {
            if (!of_type || (of_type && el->get_tagName() == (*child)->get_tagName()))
            {
                if (el == (*child))
                {
                    if (num != 0)
                    {
                        if ((idx - off) >= 0 && (idx - off) % num == 0)
                        {
                            return true;
                        }

                    }
                    else if (idx == off)
                    {
                        return true;
                    }
                    return false;
                }
                idx++;
            }
            if (el == (*child)) break;
        }
    }
    return false;
}

void element::parse_nth_child_params(const std::wstring &param, int &num, int &off)
{
    if (param == _t("odd"))
    {
        num = 2;
        off = 1;
    }
    else if (param == _t("even"))
    {
        num = 2;
        off = 0;
    }
    else
    {
        // TODO
        assert(0);
        auto tokens = split_string(param, _t(" n"), _t("n"));

        std::wstring s_num;
        std::wstring s_off;
        std::wstring s_int;

        for (const auto &tok : tokens)
        {
            if (tok == _t("n"))
            {
                s_num = s_int;
                s_int.clear();
            }
            else
            {
                s_int += tok;
            }
        }

        s_off = s_int;

        num = std::stoi(s_num);
        off = std::stoi(s_off);
    }
}

void element::calc_document_size(size& sz, int x /*= 0*/, int y /*= 0*/)
{
    if (is_visible() && m_el_position != element_position_fixed)
    {
        sz.width = std::max(sz.width, x + right());
        sz.height = std::max(sz.height, y + bottom());

        if (m_overflow == overflow_visible)
        {
            for (const auto &el : m_children)
            {
                el->calc_document_size(sz, x + m_pos.x, y + m_pos.y);
            }
        }

        // root element (<html>) must to cover entire window
        if (!parent())
        {
            position client_pos = m_doc.client_pos();
            m_pos.height = std::max(sz.height, client_pos.height) - content_margins_top() - content_margins_bottom();
            m_pos.width = std::max(sz.width, client_pos.width) - content_margins_left() - content_margins_right();
        }
    }
}


void element::get_redraw_box(position& pos, int x /*= 0*/, int y /*= 0*/)
{
    if (is_visible())
    {
        int p_left = std::min(pos.left(), x + m_pos.left() - m_padding.left - m_borders.left);
        int p_right = std::max(pos.right(), x + m_pos.right() + m_padding.left + m_borders.left);
        int p_top = std::min(pos.top(), y + m_pos.top() - m_padding.top - m_borders.top);
        int p_bottom = std::max(pos.bottom(), y + m_pos.bottom() + m_padding.bottom + m_borders.bottom);

        pos.x = p_left;
        pos.y = p_top;
        pos.width = p_right - p_left;
        pos.height = p_bottom - p_top;

        if (m_overflow == overflow_visible)
        {
            for (const auto &el : m_children)
            {
                if (el->get_element_position() != element_position_fixed)
                {
                    el->get_redraw_box(pos, x + m_pos.x, y + m_pos.y);
                }
            }
        }
    }
}

element *element::find_adjacent_sibling(element *el, const css_selector& selector, bool apply_pseudo /*= true*/, bool* is_pseudo /*= 0*/)
{
    element *ret = nullptr;

    for (const auto &e : m_children)
    {
        if (e->get_display() != display_inline_text)
        {
            if (e == el)
            {
                if (ret)
                {
                    int res = ret->select(selector, apply_pseudo);
                    if (res != select_no_match)
                    {
                        if (is_pseudo)
                        {
                            *is_pseudo = (res & select_match_pseudo_class) != 0;
                        }
                        return ret;
                    }
                }
                return nullptr;
            }
            else
            {
                ret = e;
            }
        }
    }
    return nullptr;
}

element *element::find_sibling(element *el, const css_selector& selector, bool apply_pseudo /*= true*/, bool* is_pseudo /*= 0*/)
{
    element *ret = nullptr;

    for (const auto &e : m_children)
    {
        if (e->get_display() != display_inline_text)
        {
            if (e == el)
            {
                return ret;
            }
            else if (!ret)
            {
                int res = e->select(selector, apply_pseudo);
                if (res != select_no_match)
                {
                    if (is_pseudo)
                    {
                        *is_pseudo = (res & select_match_pseudo_class) != 0;
                    }
                    ret = e;
                }
            }
        }
    }
    return nullptr;
}

bool element::is_only_child(element *el, bool of_type)
{
    int child_count = 0;
    for (const auto &child : m_children)
    {
        if (child->get_display() != display_inline_text)
        {
            if (!of_type || (of_type && el->get_tagName() == child->get_tagName()))
            {
                child_count++;
            }
            if (child_count > 1) break;
        }
    }
    if (child_count > 1)
    {
        return false;
    }
    return true;
}

void element::update_floats(int dy, element *parent)
{
    if (is_floats_holder())
    {
        bool reset_cache = false;
        for (auto fb = m_floats_left.rbegin(); fb != m_floats_left.rend(); fb++)
        {
            if (fb->el->is_ancestor(parent))
            {
                reset_cache = true;
                fb->pos.y += dy;
            }
        }
        if (reset_cache)
        {
            m_cahe_line_left.invalidate();
        }
        reset_cache = false;
        for (auto fb = m_floats_right.rbegin(); fb != m_floats_right.rend(); fb++)
        {
            if (fb->el->is_ancestor(parent))
            {
                reset_cache = true;
                fb->pos.y += dy;
            }
        }
        if (reset_cache)
        {
            m_cahe_line_right.invalidate();
        }
    }
    else
    {
        m_parent->update_floats(dy, parent);
    }
}

void element::remove_before_after()
{
    if (!m_children.empty())
    {
        if (m_children.front()->get_tagName() == _t("::before"))
        {
            m_children.erase(m_children.begin());
        }
    }
    if (!m_children.empty())
    {
        if (m_children.back()->get_tagName() == _t("::after"))
        {
            m_children.erase(m_children.end() - 1);
        }
    }
}

element *element::get_element_before()
{
    if (!m_children.empty())
    {
        if (m_children.front()->get_tagName() == _t("::before"))
        {
            return m_children.front();
        }
    }

    auto el = new element(m_doc, el_before);
    el->parent(this);
    m_children.insert(m_children.begin(), el);
    return el;
}

element *element::get_element_after()
{
    if (!m_children.empty())
    {
        if (m_children.back()->get_tagName() == _t("::after"))
        {
            return m_children.back();
        }
    }
    auto el = new element(m_doc, el_after);
    el->parent(this);
    m_children.push_back(el);
    return el;
}

void element::add_style(const std::shared_ptr<style> &st)
{
    if (_type == el_text || _type == el_space)
    {
        return;
    }

    m_style.combine(*st);

    if (_type == el_before || _type == el_after)
    {
        const auto content = get_style_property(_t("content"), false);

        if (!content.empty())
        {
            int idx = value_index(content, content_property_string);
            if (idx < 0)
            {
                std::wstring fnc;
                std::wstring::size_type i = 0;
                while (i < content.length() && i != std::wstring::npos)
                {
                    if (content.at(i) == L'\"')
                    {
                        fnc.clear();
                        i++;

                        auto pos = content.find_first_of(L'\"', i);
                        std::wstring txt;

                        if (pos == std::wstring::npos)
                        {
                            txt = content.substr(i);
                            i = std::wstring::npos;
                        }
                        else
                        {
                            txt = content.substr(i, pos - i);
                            i = pos + 1;
                        }
                        add_text(txt);
                    }
                    else if (content.at(i) == _t('('))
                    {
                        i++;

                        auto pos = content.find_first_of(_t(')'), i);
                        std::wstring params;

                        if (pos == std::wstring::npos)
                        {
                            params = content.substr(i);
                            i = std::wstring::npos;
                        }
                        else
                        {
                            params = content.substr(i, pos - i);
                            i = pos + 1;
                        }

                        add_function(trim_lower(fnc), params);
                        fnc.clear();
                    }
                    else
                    {
                        fnc += content.at(i);
                        i++;
                    }
                }
            }
        }
    }
}

bool element::have_inline_child()
{
    for (const auto &child : m_children)
    {
        if (!child->is_white_space())
        {
            return true;
        }
    }

    return false;
}

void element::refresh_styles()
{
    remove_before_after();

    for (const auto &child : m_children)
    {
        if (child->get_display() != display_inline_text)
        {
            child->refresh_styles();
        }
    }

    m_style.clear();

    for (auto &usel : m_used_styles)
    {
        usel.m_used = false;

        if (usel.m_selector->is_media_valid())
        {
            int apply = select(*usel.m_selector, false);

            if (apply != select_no_match)
            {
                if (apply & select_match_pseudo_class)
                {
                    if (select(*usel.m_selector, true))
                    {
                        add_style(usel.m_selector->m_style);
                        usel.m_used = true;
                    }
                }
                else if (apply & select_match_with_after)
                {
                    element *el = get_element_after();
                    if (el)
                    {
                        el->add_style(usel.m_selector->m_style);
                    }
                }
                else if (apply & select_match_with_before)
                {
                    element *el = get_element_before();
                    if (el)
                    {
                        el->add_style(usel.m_selector->m_style);
                    }
                }
                else
                {
                    add_style(usel.m_selector->m_style);
                    usel.m_used = true;
                }
            }
        }
    }
}

element *element::get_child_by_point(int x, int y, int client_x, int client_y, draw_flag flag, int zindex)
{
    element *ret = nullptr;

    if (m_overflow > overflow_visible)
    {
        if (!m_pos.is_point_inside(x, y))
        {
            return ret;
        }
    }

    position pos = m_pos;
    pos.x = x - pos.x;
    pos.y = y - pos.y;

    for (auto i = m_children.rbegin(); i != m_children.rend() && !ret; i++)
    {
        auto el = (*i);

        if (el->is_visible() && el->get_display() != display_inline_text)
        {
            switch (flag)
            {
            case draw_positioned:
                if (el->is_positioned() && el->get_zindex() == zindex)
                {
                    if (el->get_element_position() == element_position_fixed)
                    {
                        ret = el->get_element_by_point(client_x, client_y, client_x, client_y);
                        if (!ret && (*i)->is_point_inside(client_x, client_y))
                        {
                            ret = (*i);
                        }
                    }
                    else
                    {
                        ret = el->get_element_by_point(pos.x, pos.y, client_x, client_y);
                        if (!ret && (*i)->is_point_inside(pos.x, pos.y))
                        {
                            ret = (*i);
                        }
                    }
                    el = 0;
                }
                break;
            case draw_block:
                if (!el->is_inline_box() && el->get_float() == float_none && !el->is_positioned())
                {
                    if (el->is_point_inside(pos.x, pos.y))
                    {
                        ret = el;
                    }
                }
                break;
            case draw_floats:
                if (el->get_float() != float_none && !el->is_positioned())
                {
                    ret = el->get_element_by_point(pos.x, pos.y, client_x, client_y);

                    if (!ret && (*i)->is_point_inside(pos.x, pos.y))
                    {
                        ret = (*i);
                    }
                    el = 0;
                }
                break;
            case draw_inlines:
                if (el->is_inline_box() && el->get_float() == float_none && !el->is_positioned())
                {
                    if (el->get_display() == display_inline_block)
                    {
                        ret = el->get_element_by_point(pos.x, pos.y, client_x, client_y);
                        el = 0;
                    }
                    if (!ret && (*i)->is_point_inside(pos.x, pos.y))
                    {
                        ret = (*i);
                    }
                }
                break;
            default:
                break;
            }

            if (el && !el->is_positioned())
            {
                if (flag == draw_positioned)
                {
                    auto child = el->get_child_by_point(pos.x, pos.y, client_x, client_y, flag, zindex);

                    if (child)
                    {
                        ret = child;
                    }
                }
                else
                {
                    if (el->get_float() == float_none &&
                        el->get_display() != display_inline_block)
                    {
                        auto child = el->get_child_by_point(pos.x, pos.y, client_x, client_y, flag, zindex);

                        if (child)
                        {
                            ret = child;
                        }
                    }
                }
            }
        }
    }

    return ret;
}

element *element::get_element_by_point(int x, int y, int client_x, int client_y)
{
    element *ret = nullptr;

    if (is_visible())
    {        
        std::set<int> zindexes;

        for (const auto &i : m_positioned)
        {
            zindexes.insert(i->get_zindex());
        }

        for (const auto &idx : zindexes)
        {
            if (idx > 0)
            {
                ret = get_child_by_point(x, y, client_x, client_y, draw_positioned, idx);
                if (ret) return ret;
            }
        }

        for (const auto &idx : zindexes)
        {
            if (idx == 0)
            {
                ret = get_child_by_point(x, y, client_x, client_y, draw_positioned, idx);
                if (ret) return ret;
            }
        }

        ret = get_child_by_point(x, y, client_x, client_y, draw_inlines, 0);
        if (ret) return ret;

        ret = get_child_by_point(x, y, client_x, client_y, draw_floats, 0);
        if (ret) return ret;

        ret = get_child_by_point(x, y, client_x, client_y, draw_block, 0);
        if (ret) return ret;

        for (const auto &idx : zindexes)
        {
            if (idx < 0)
            {
                ret = get_child_by_point(x, y, client_x, client_y, draw_positioned, idx);
                if (ret) return ret;
            }
        }
        
        if (m_el_position == element_position_fixed)
        {
            if (is_point_inside(client_x, client_y))
            {
                ret = this;
            }
        }
        else if (is_point_inside(x, y))
        {
            ret = this;
        }
    }

    return ret;
}

background* element::get_background(bool own_only)
{
    if (own_only)
    {
        // return own background with check for empty one
        if (m_bg.m_image.empty() && !m_bg.m_color.alpha)
        {
            return nullptr;
        }
        return &m_bg;
    }

    if (m_bg.m_image.empty() && !m_bg.m_color.alpha)
    {
        // if this is root element (<html>) try to get background from body
        if (!parent())
        {
            for (const auto &child : m_children)
            {
                if (child->is_body())
                {
                    // return own body background
                    return child->get_background(true);
                }
            }
        }
        return nullptr;
    }

    if (is_body())
    {
        if (!m_parent->get_background(true))
        {
            // parent of body will draw background for body
            return nullptr;
        }
    }

    return &m_bg;
}


void element::add_text(const std::wstring& txt)
{
    std::wstring word;
    std::wstring esc;

    for (auto i = 0; i < txt.length(); i++)
    {
        if ((txt.at(i) == _t(' ')) || (txt.at(i) == _t('\t')) || (txt.at(i) == _t('\\') && !esc.empty()))
        {
            if (esc.empty())
            {
                if (!word.empty())
                {
                    auto el = new element(m_doc, el_text, word);
                    appendChild(el);
                    word.clear();
                }

                auto el = new element(m_doc, el_text, txt.substr(i, 1));
                appendChild(el);
            }
            else
            {
                word += convert_escape(esc.c_str() + 1);
                esc.clear();
                if (txt.at(i) == _t('\\'))
                {
                    esc += txt.at(i);
                }
            }
        }
        else
        {
            if (!esc.empty() || txt.at(i) == _t('\\'))
            {
                esc += txt.at(i);
            }
            else
            {
                word += txt.at(i);
            }
        }
    }

    if (!esc.empty())
    {
        word += convert_escape(esc.c_str() + 1);
    }
    if (!word.empty())
    {
        auto el = new element(m_doc, el_text, word);
        appendChild(el);
        word.clear();
    }
}

void element::add_function(const std::wstring& fnc, const std::wstring& params)
{
    int idx = value_index(fnc, _t("attr;counter;url"));

    switch (idx)
    {
        // attr
    case 0:
    {
        const auto &attr_value = m_parent->get_attr(trim_lower(params));

        if (!attr_value.empty())
        {
            add_text(attr_value);
        }
    }
        break;
        // counter
    case 1:
        break;
        // url
    case 2:
    {
        std::wstring p_url = params;
        trim(p_url);
        if (!p_url.empty())
        {
            if (p_url.at(0) == _t('\'') || p_url.at(0) == _t('\"'))
            {
                p_url.erase(0, 1);
            }
        }
        if (!p_url.empty())
        {
            if (p_url.at(p_url.length() - 1) == _t('\'') || p_url.at(p_url.length() - 1) == _t('\"'))
            {
                p_url.erase(p_url.length() - 1, 1);
            }
        }
        if (!p_url.empty())
        {
            auto el = new element(m_doc, el_image);
            el->set_attr(_t("src"), p_url);
            el->set_attr(_t("style"), _t("display:inline-block"));
            el->set_tagName(_t("img"));
            appendChild(el);
            el->parse_attributes();
        }
    }
        break;
    }
}

wchar_t element::convert_escape(const wchar_t* txt)
{
    return (wchar_t) std::stol(txt, 0, 16);
}
