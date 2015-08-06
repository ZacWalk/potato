#pragma once

#include "types.h"

class element;


template<class t_go_inside, class t_select>
class elements_iterator
{
private:
    struct stack_item
    {
        int idx;
        element *el;
    };

    std::vector<stack_item> m_stack;

    element *m_el;
    int m_idx;

    t_go_inside &m_go_inside;
    t_select &m_select;

public:

    elements_iterator(element *el, t_go_inside &go_inside, t_select &select) :
        m_el(el),
        m_idx(-1),
        m_go_inside(go_inside),
        m_select(select)
    {
    }

    element *next(bool ret_parent = true)
    {
        next_idx();

        while (m_idx < (int) m_el->get_children_count())
        {
            auto el = m_el->get_child(m_idx);

            if (el->get_children_count() && m_go_inside.select(el))
            {
                stack_item si;
                si.idx = m_idx;
                si.el = m_el;
                m_stack.push_back(si);
                m_el = el;
                m_idx = -1;
                if (ret_parent)
                {
                    return el;
                }
                next_idx();
            }
            else if (m_select.select(m_el->get_child(m_idx)))
            {
                return m_el->get_child(m_idx);
            }
            else
            {
                next_idx();
            }
        }

        return 0;
    }

private:

    void next_idx()
    {
        m_idx++;

        while (m_idx >= (int) m_el->get_children_count() && m_stack.size())
        {
            auto si = m_stack.back();
            m_stack.pop_back();

            m_idx = si.idx;
            m_el = si.el;
            m_idx++;
            continue;
        }
    }
};

class go_inside_inline
{
public:
    bool select(element *el)
    {
        return el->get_display() == display_inline || el->get_display() == display_inline_text;
    }
};

class go_inside_table
{
public:
    bool select(element *el)
    {
        return el->get_display() == display_table_row_group ||
            el->get_display() == display_table_header_group ||
            el->get_display() == display_table_footer_group;
    }
};

class table_rows_selector
{
public:
    bool select(element *el)
    {
        return el->get_display() == display_table_row;
    }
};

class table_cells_selector
{
public:
    bool select(element *el)
    {
        return el->get_display() == display_table_cell;
    }
};







