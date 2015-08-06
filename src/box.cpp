#include "pch.h"
#include "box.h"
#include "element.h"

int box::height() const
{
    return _type == box_block ? m_element->height() : m_height;
}

int box::width() const
{
    return _type == box_block ? m_element->width() : m_width;
}

void box::add_element(element *el)
{
    if (_type == box_block)
    {
        m_element = el;
        el->m_box = this;
    }
    else
    {
        el->m_skip = false;
        el->m_box = nullptr;

        bool add = true;
        if ((m_items.empty() && el->is_white_space()) || el->is_break())
        {
            el->m_skip = true;
        }
        else if (el->is_white_space())
        {
            element *ws = get_last_space();
            if (ws)
            {
                add = false;
                el->m_skip = true;
            }
        }

        if (add)
        {
            el->m_box = this;
            m_items.push_back(el);

            if (!el->m_skip)
            {
                int el_shift_left = el->get_inline_shift_left();
                int el_shift_right = el->get_inline_shift_right();

                el->m_pos.x = m_box_left + m_width + el_shift_left + el->content_margins_left();
                el->m_pos.y = m_box_top + el->content_margins_top();
                m_width += el->width() + el_shift_left + el_shift_right;
            }
        }
    }
}

void box::finish(bool last_box)
{
    if (_type == box_block)
    {
        if (!m_element) return;

        css_offsets offsets;
        if (m_element->get_element_position(&offsets) == element_position_relative)
        {
            if (!offsets.left.is_predefined())
            {
                m_element->m_pos.x += offsets.left.calc_percent(m_box_right - m_box_left);
            }
            else if (!offsets.right.is_predefined())
            {
                m_element->m_pos.x -= offsets.right.calc_percent(m_box_right - m_box_left);
            }
            if (!offsets.top.is_predefined())
            {
                int h = 0;
                if (offsets.top.units() == css_units_percentage)
                {
                    if (m_element->parent())
                    {
                        m_element->parent()->get_predefined_height(h);
                    }
                }
                m_element->m_pos.y += offsets.top.calc_percent(h);
            }
            else if (!offsets.bottom.is_predefined())
            {
                int h = 0;
                if (offsets.bottom.units() == css_units_percentage)
                {
                    if (m_element->parent())
                    {
                        m_element->parent()->get_predefined_height(h);
                    }
                }
                m_element->m_pos.y -= offsets.bottom.calc_percent(h);
            }
        }
    }
    else
    {
        if (is_empty() || (!is_empty() && last_box && is_break_only()))
        {
            m_height = 0;
            return;
        }

        for (auto i = m_items.rbegin(); i != m_items.rend(); i++)
        {
            if ((*i)->is_white_space() || (*i)->is_break())
            {
                if (!(*i)->m_skip)
                {
                    (*i)->m_skip = true;
                    m_width -= (*i)->width();
                }
            }
            else
            {
                break;
            }
        }

        int base_line = m_font_metrics.base_line();
        int line_height = m_line_height;

        int add_x = 0;
        switch (m_text_align)
        {
        case text_align_right:
            if (m_width < (m_box_right - m_box_left))
            {
                add_x = (m_box_right - m_box_left) - m_width;
            }
            break;
        case text_align_center:
            if (m_width < (m_box_right - m_box_left))
            {
                add_x = ((m_box_right - m_box_left) - m_width) / 2;
            }
            break;
        default:
            add_x = 0;
        }

        m_height = 0;

        // find line box baseline and line-height
        for (auto &i : m_items)
        {
            if (i->get_display() == display_inline_text)
            {
                font_metrics fm;
                i->get_font(&fm);
                base_line = std::max(base_line, fm.base_line());
                line_height = std::max(line_height, i->line_height());
                m_height = std::max(m_height, fm.height);
            }
            i->m_pos.x += add_x;
        }

        if (m_height)
        {
            base_line += (line_height - m_height) / 2;
        }

        m_height = line_height;

        int y1 = 0;
        int y2 = m_height;

        for (auto &i : m_items)
        {
            if (i->get_display() == display_inline_text)
            {
                font_metrics fm;
                i->get_font(&fm);
                i->m_pos.y = m_height - base_line - fm.ascent;
            }
            else
            {
                switch (i->get_vertical_align())
                {
                case va_super:
                case va_sub:
                case va_baseline:
                    i->m_pos.y = m_height - base_line - i->height() + i->get_base_line() + i->content_margins_top();
                    break;
                case va_top:
                    i->m_pos.y = y1 + i->content_margins_top();
                    break;
                case va_text_top:
                    i->m_pos.y = m_height - base_line - m_font_metrics.ascent + i->content_margins_top();
                    break;
                case va_middle:
                    i->m_pos.y = m_height - base_line - m_font_metrics.x_height / 2 - i->height() / 2 + i->content_margins_top();
                    break;
                case va_bottom:
                    i->m_pos.y = y2 - i->height() + i->content_margins_top();
                    break;
                case va_text_bottom:
                    i->m_pos.y = m_height - base_line + m_font_metrics.descent - i->height() + i->content_margins_top();
                    break;
                }
                y1 = std::min(y1, i->top());
                y2 = std::max(y2, i->bottom());
            }
        }

        css_offsets offsets;

        for (auto &i : m_items)
        {
            i->m_pos.y -= y1;
            i->m_pos.y += m_box_top;

            if (i->get_display() != display_inline_text)
            {
                switch (i->get_vertical_align())
                {
                case va_top:
                    i->m_pos.y = m_box_top + i->content_margins_top();
                    break;
                case va_bottom:
                    i->m_pos.y = m_box_top + (y2 - y1) - i->height() + i->content_margins_top();
                    break;
                case va_baseline:
                    //TODO: process vertical align "baseline"
                    break;
                case va_middle:
                    //TODO: process vertical align "middle"
                    break;
                case va_sub:
                    //TODO: process vertical align "sub"
                    break;
                case va_super:
                    //TODO: process vertical align "super"
                    break;
                case va_text_bottom:
                    //TODO: process vertical align "text-bottom"
                    break;
                case va_text_top:
                    //TODO: process vertical align "text-top"
                    break;
                }
            }

            // update position for relative positioned elements

            if (i->get_element_position(&offsets) == element_position_relative)
            {
                if (!offsets.left.is_predefined())
                {
                    i->m_pos.x += offsets.left.calc_percent(m_box_right - m_box_left);
                }
                else if (!offsets.right.is_predefined())
                {
                    i->m_pos.x -= offsets.right.calc_percent(m_box_right - m_box_left);
                }
                if (!offsets.top.is_predefined())
                {
                    // TODO: m_line_height is not correct here
                    i->m_pos.y += offsets.top.calc_percent(m_line_height);
                }
                else if (!offsets.bottom.is_predefined())
                {
                    // TODO: m_line_height is not correct here
                    i->m_pos.y -= offsets.bottom.calc_percent(m_line_height);
                }
            }
        }
        m_height = y2 - y1;
        m_baseline = (base_line - y1) - (m_height - line_height);
    }
}

bool box::can_hold(element *el, white_space ws) const
{
    if (_type == box_block)
    {
        if (m_element || el->is_inline_box())
        {
            return false;
        }
    }
    else
    {
        if (!el->is_inline_box()) return false;

        if (el->is_break())
        {
            return false;
        }

        if (ws == white_space_nowrap || ws == white_space_pre)
        {
            return true;
        }

        if (m_box_left + m_width + el->width() + el->get_inline_shift_left() + el->get_inline_shift_right() > m_box_right)
        {
            return false;
        }

    }

    return true;
}

bool box::is_empty() const
{
    if (_type == box_block)
    {
        if (m_element)
        {
            return false;
        }
    }
    else
    {
        if (m_items.empty()) return true;

        for (auto i = m_items.rbegin(); i != m_items.rend(); i++)
        {
            if (!(*i)->m_skip || (*i)->is_break())
            {
                return false;
            }
        }
    }

    return true;
}

int box::baseline() const
{
    if (_type == box_block)
    {
        if (m_element)
        {
            return m_element->get_base_line();
        }
    }
    else
    {
        return m_baseline;
    }

    return 0;
}

void box::get_elements(std::vector<element*>& els)
{
    if (_type == box_block)
    {
        els.push_back(m_element);
    }
    else
    {
        els.insert(els.begin(), m_items.begin(), m_items.end());
    }
}

int box::top_margin() const
{
    if (_type == box_block)
    {
        if (m_element && m_element->collapse_top_margin())
        {
            return m_element->m_margins.top;
        }
    }

    return 0;
}

int box::bottom_margin() const
{
    if (_type == box_block)
    {
        if (m_element && m_element->collapse_bottom_margin())
        {
            return m_element->m_margins.bottom;
        }
    }

    return 0;
}

void box::y_shift(int shift)
{
    if (_type == box_block)
    {
        m_box_top += shift;
        if (m_element)
        {
            m_element->m_pos.y += shift;
        }
    }
    else
    {
        m_box_top += shift;
        for (auto i = m_items.begin(); i != m_items.end(); i++)
        {
            (*i)->m_pos.y += shift;
        }
    }
}

void box::new_width(int left, int right, std::vector<element*>& els)
{
    if (_type == box_block)
    {
    }
    else
    {
        int add = left - m_box_left;
        if (add)
        {
            m_box_left = left;
            m_box_right = right;
            m_width = 0;
            auto remove_begin = m_items.end();

            for (auto i = m_items.begin() + 1; i != m_items.end(); i++)
            {
                element *el = (*i);

                if (!el->m_skip)
                {
                    if (m_box_left + m_width + el->width() + el->get_inline_shift_right() + el->get_inline_shift_left() > m_box_right)
                    {
                        remove_begin = i;
                        break;
                    }
                    else
                    {
                        el->m_pos.x += add;
                        m_width += el->width() + el->get_inline_shift_right() + el->get_inline_shift_left();
                    }
                }
            }
            if (remove_begin != m_items.end())
            {
                els.insert(els.begin(), remove_begin, m_items.end());
                m_items.erase(remove_begin, m_items.end());

                for (auto i = els.begin(); i != els.end(); i++)
                {
                    (*i)->m_box = nullptr;
                }
            }
        }
    }
}



element *box::get_last_space()
{
    element *ret = nullptr;

    for (auto i = m_items.rbegin(); i != m_items.rend() && !ret; i++)
    {
        if ((*i)->is_white_space() || (*i)->is_break())
        {
            ret = (*i);
        }
        else
        {
            break;
        }
    }
    return ret;
}


bool box::is_break_only() const
{
    if (m_items.empty()) return true;

    if (m_items.front()->is_break())
    {
        for (auto i = m_items.begin() + 1; i != m_items.end(); i++)
        {
            if (!(*i)->m_skip)
            {
                return false;
            }
        }
        return true;
    }
    return false;
}


