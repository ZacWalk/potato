#include "pch.h"
#include "table.h"
#include "strings.h"
#include "element.h"
#include "util.h"

void table_grid::add_cell(element *el)
{
    table_cell cell;
    cell.el = el;
    cell.colspan = std::stoi(el->get_attr(_t("colspan"), _t("1")));
    cell.rowspan = std::stoi(el->get_attr(_t("rowspan"), _t("1")));
    cell.borders = el->get_borders();

    while (is_rowspanned((int) m_cells.size() - 1, (int) m_cells.back().size()))
    {
        m_cells.back().push_back(table_cell());
    }

    m_cells.back().push_back(cell);
    for (int i = 1; i < cell.colspan; i++)
    {
        table_cell empty_cell;
        m_cells.back().push_back(empty_cell);
    }
}


void table_grid::begin_row(element *row)
{
    std::vector<table_cell> r;
    m_cells.push_back(r);

    m_rows.push_back(table_row(0, row));

}


bool table_grid::is_rowspanned(int r, int c)
{
    for (int row = r - 1; row >= 0; row--)
    {
        if (c < (int) m_cells[row].size())
        {
            if (m_cells[row][c].rowspan > 1)
            {
                if (m_cells[row][c].rowspan >= r - row + 1)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

void table_grid::finish()
{
    m_rows_count = (int) m_cells.size();
    m_cols_count = 0;
    for (int i = 0; i < (int) m_cells.size(); i++)
    {
        m_cols_count = std::max(m_cols_count, (int) m_cells[i].size());
    }
    for (int i = 0; i < (int) m_cells.size(); i++)
    {
        for (int j = (int) m_cells[i].size(); j < m_cols_count; j++)
        {
            table_cell empty_cell;
            m_cells[i].push_back(empty_cell);
        }
    }

    m_columns.clear();
    for (int i = 0; i < m_cols_count; i++)
    {
        m_columns.push_back(table_column(0, 0));
    }

    for (int col = 0; col < m_cols_count; col++)
    {
        for (int row = 0; row < m_rows_count; row++)
        {
            if (cell(col, row)->el)
            {
                // find minimum left border width
                if (m_columns[col].border_left)
                {
                    m_columns[col].border_left = std::min(m_columns[col].border_left, cell(col, row)->borders.left);
                }
                else
                {
                    m_columns[col].border_left = cell(col, row)->borders.left;
                }
                // find minimum right border width
                if (m_columns[col].border_right)
                {
                    m_columns[col].border_right = std::min(m_columns[col].border_right, cell(col, row)->borders.right);
                }
                else
                {
                    m_columns[col].border_right = cell(col, row)->borders.right;
                }
                // find minimum top border width
                if (m_rows[row].border_top)
                {
                    m_rows[row].border_top = std::min(m_rows[row].border_top, cell(col, row)->borders.top);
                }
                else
                {
                    m_rows[row].border_top = cell(col, row)->borders.top;
                }
                // find minimum bottom border width
                if (m_rows[row].border_bottom)
                {
                    m_rows[row].border_bottom = std::min(m_rows[row].border_bottom, cell(col, row)->borders.bottom);
                }
                else
                {
                    m_rows[row].border_bottom = cell(col, row)->borders.bottom;
                }
            }

            if (cell(col, row)->el && cell(col, row)->colspan <= 1)
            {
                if (!cell(col, row)->el->get_css_width().is_predefined())
                {
                    m_columns[col].css_width = cell(col, row)->el->get_css_width();
                    break;
                }
            }
        }
    }

    for (int col = 0; col < m_cols_count; col++)
    {
        for (int row = 0; row < m_rows_count; row++)
        {
            if (cell(col, row)->el)
            {
                cell(col, row)->el->set_css_width(m_columns[col].css_width);
            }
        }
    }
}

table_cell* table_grid::cell(int t_col, int t_row)
{
    if (t_col >= 0 && t_col < m_cols_count && t_row >= 0 && t_row < m_rows_count)
    {
        return &m_cells[t_row][t_col];
    }
    return 0;
}

void table_grid::distribute_max_width(int width, int start, int end)
{
    table_column_accessor_max_width selector;
    distribute_width(width, start, end, selector);
}

void table_grid::distribute_min_width(int width, int start, int end)
{
    table_column_accessor_min_width selector;
    distribute_width(width, start, end, selector);
}



void table_grid::distribute_width(int width, int start, int end)
{
    if (!(start >= 0 && start < m_cols_count && end >= 0 && end < m_cols_count))
    {
        return;
    }

    std::vector<table_column*> distribute_columns;

    for (int step = 0; step < 3; step++)
    {
        distribute_columns.clear();

        switch (step)
        {
        case 0:
        {
            // distribute between the columns with width == auto
            for (int col = start; col <= end; col++)
            {
                if (m_columns[col].css_width.is_predefined())
                {
                    distribute_columns.push_back(&m_columns[col]);
                }
            }
        }
            break;
        case 1:
        {
            // distribute between the columns with percents
            for (int col = start; col <= end; col++)
            {
                if (!m_columns[col].css_width.is_predefined() && m_columns[col].css_width.units() == css_units_percentage)
                {
                    distribute_columns.push_back(&m_columns[col]);
                }
            }
        }
            break;
        case 2:
        {
            //well distribute between all columns
            for (int col = start; col <= end; col++)
            {
                distribute_columns.push_back(&m_columns[col]);
            }
        }
            break;
        }

        int added_width = 0;

        if (!distribute_columns.empty() || step == 2)
        {
            int cols_width = 0;
            for (auto col = distribute_columns.begin(); col != distribute_columns.end(); col++)
            {
                cols_width += (*col)->max_width - (*col)->min_width;
            }

            if (cols_width)
            {
                int add = width / (int) distribute_columns.size();
                for (auto col = distribute_columns.begin(); col != distribute_columns.end(); col++)
                {
                    add = round_f((float) width * ((float) ((*col)->max_width - (*col)->min_width) / (float) cols_width));
                    if ((*col)->width + add >= (*col)->min_width)
                    {
                        (*col)->width += add;
                        added_width += add;
                    }
                    else
                    {
                        added_width += ((*col)->width - (*col)->min_width) * (add / abs(add));
                        (*col)->width = (*col)->min_width;
                    }
                }
                if (added_width < width && step)
                {
                    distribute_columns.front()->width += width - added_width;
                    added_width = width;
                }
            }
            else
            {
                distribute_columns.back()->width += width;
                added_width = width;
            }
        }

        if (added_width == width)
        {
            break;
        }
        else
        {
            width -= added_width;
        }
    }
}

int table_grid::calc_table_width(int block_width, bool is_auto)
{
    //int table_width = 0;

    int min_table_width = 0; // MIN
    int max_table_width = 0; // MAX

    int cur_width = 0;
    int max_w = 0;
    int min_w = 0;

    for (int col = 0; col < m_cols_count; col++)
    {
        min_table_width += m_columns[col].min_width;
        max_table_width += m_columns[col].max_width;

        if (!m_columns[col].css_width.is_predefined())
        {
            m_columns[col].width = m_columns[col].css_width.calc_percent(block_width);
            m_columns[col].width = std::max(m_columns[col].width, m_columns[col].min_width);
        }
        else
        {
            m_columns[col].width = m_columns[col].min_width;
            max_w += m_columns[col].max_width;
            min_w += m_columns[col].min_width;
        }

        cur_width += m_columns[col].width;
    }

    if (cur_width == block_width)
    {
        return cur_width;
    }

    if (cur_width < block_width)
    {
        if (cur_width - min_w + max_w <= block_width)
        {
            cur_width = 0;
            for (int col = 0; col < m_cols_count; col++)
            {
                if (m_columns[col].css_width.is_predefined())
                {
                    m_columns[col].width = m_columns[col].max_width;
                }
                cur_width += m_columns[col].width;
            }
            if (cur_width == block_width || is_auto)
            {
                return cur_width;
            }
        }
        distribute_width(block_width - cur_width, 0, m_cols_count - 1);
        cur_width = 0;
        for (int col = 0; col < m_cols_count; col++)
        {
            cur_width += m_columns[col].width;
        }
    }
    else
    {
        int fixed_width = 0;
        float percent = 0;
        for (int col = 0; col < m_cols_count; col++)
        {
            if (!m_columns[col].css_width.is_predefined() && m_columns[col].css_width.units() == css_units_percentage)
            {
                percent += m_columns[col].css_width.val();
            }
            else
            {
                fixed_width += m_columns[col].width;
            }
        }
        float scale = (float) (100.0 / percent);
        cur_width = 0;
        for (int col = 0; col < m_cols_count; col++)
        {
            if (!m_columns[col].css_width.is_predefined() && m_columns[col].css_width.units() == css_units_percentage)
            {
                css_length w;
                w.set_value(m_columns[col].css_width.val() * scale, css_units_percentage);
                m_columns[col].width = w.calc_percent(block_width - fixed_width);
                if (m_columns[col].width < m_columns[col].min_width)
                {
                    m_columns[col].width = m_columns[col].min_width;
                }
            }
            cur_width += m_columns[col].width;
        }
    }
    return cur_width;
}

void table_grid::clear()
{
    m_rows_count = 0;
    m_cols_count = 0;
    m_cells.clear();
    m_columns.clear();
    m_rows.clear();
}

void table_grid::calc_horizontal_positions(margins& table_borders, border_collapse bc, int bdr_space_x)
{
    if (bc == border_collapse_separate)
    {
        int left = bdr_space_x;
        for (int i = 0; i < m_cols_count; i++)
        {
            m_columns[i].left = left;
            m_columns[i].right = m_columns[i].left + m_columns[i].width;
            left = m_columns[i].right + bdr_space_x;
        }
    }
    else
    {
        int left = 0;
        if (m_cols_count)
        {
            left -= std::min(table_borders.left, m_columns[0].border_left);
        }
        for (int i = 0; i < m_cols_count; i++)
        {
            if (i > 0)
            {
                left -= std::min(m_columns[i - 1].border_right, m_columns[i].border_left);
            }

            m_columns[i].left = left;
            m_columns[i].right = m_columns[i].left + m_columns[i].width;
            left = m_columns[i].right;
        }
    }
}

void table_grid::calc_vertical_positions(margins& table_borders, border_collapse bc, int bdr_space_y)
{
    if (bc == border_collapse_separate)
    {
        int top = bdr_space_y;
        for (int i = 0; i < m_rows_count; i++)
        {
            m_rows[i].top = top;
            m_rows[i].bottom = m_rows[i].top + m_rows[i].height;
            top = m_rows[i].bottom + bdr_space_y;
        }
    }
    else
    {
        int top = 0;
        if (m_rows_count)
        {
            top -= std::min(table_borders.top, m_rows[0].border_top);
        }
        for (int i = 0; i < m_rows_count; i++)
        {
            if (i > 0)
            {
                top -= std::min(m_rows[i - 1].border_bottom, m_rows[i].border_top);
            }

            m_rows[i].top = top;
            m_rows[i].bottom = m_rows[i].top + m_rows[i].height;
            top = m_rows[i].bottom;
        }
    }
}

//////////////////////////////////////////////////////////////////////////

int& table_column_accessor_max_width::get(table_column& col)
{
    return col.max_width;
}

int& table_column_accessor_min_width::get(table_column& col)
{
    return col.min_width;
}

int& table_column_accessor_width::get(table_column& col)
{
    return col.width;
}
