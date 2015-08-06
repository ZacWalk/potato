#pragma once

#include "css.h"

struct table_row
{
    int height;
    int border_top;
    int border_bottom;
    element *el_row;
    int top;
    int bottom;

    table_row()
    {
        top = 0;
        bottom = 0;
        border_bottom = 0;
        border_top = 0;
        height = 0;
        el_row = 0;
    }

    table_row(int h, element *row)
    {
        height = h;
        el_row = row;
        border_bottom = 0;
        border_top = 0;
        top = 0;
        bottom = 0;
    }

    table_row(const table_row& val)
    {
        top = val.top;
        bottom = val.bottom;
        border_bottom = val.border_bottom;
        border_top = val.border_top;
        height = val.height;
        el_row = val.el_row;
    }
};

struct table_column
{
    int min_width;
    int max_width;
    int width;
    css_length css_width;
    int border_left;
    int border_right;
    int left;
    int right;

    table_column()
    {
        left = 0;
        right = 0;
        border_left = 0;
        border_right = 0;
        min_width = 0;
        max_width = 0;
        width = 0;
        css_width.predef(0);
    }

    table_column(int min_w, int max_w)
    {
        left = 0;
        right = 0;
        border_left = 0;
        border_right = 0;
        max_width = max_w;
        min_width = min_w;
        width = 0;
        css_width.predef(0);
    }

    table_column(const table_column& val)
    {
        left = val.left;
        right = val.right;
        border_left = val.border_left;
        border_right = val.border_right;
        max_width = val.max_width;
        min_width = val.min_width;
        width = val.width;
        css_width = val.css_width;
    }
};



class table_column_accessor_max_width
{
public:
    int& get(table_column& col);
};

class table_column_accessor_min_width
{
public:
    int& get(table_column& col);
};

class table_column_accessor_width
{
public:
    int& get(table_column& col);
};



struct table_cell
{
    element *el;
    int colspan;
    int rowspan;
    int min_width;
    int min_height;
    int max_width;
    int max_height;
    int width;
    int height;

    margins borders;

    table_cell()
    {
        min_width = 0;
        min_height = 0;
        max_width = 0;
        max_height = 0;
        width = 0;
        height = 0;
        colspan = 1;
        rowspan = 1;
        el = 0;
    }

    table_cell(const table_cell& val)
    {
        el = val.el;
        colspan = val.colspan;
        rowspan = val.rowspan;
        width = val.width;
        height = val.height;
        min_width = val.min_width;
        min_height = val.min_height;
        max_width = val.max_width;
        max_height = val.max_height;
        borders = val.borders;
    }
};

class table_grid
{
private:

    int m_rows_count = 0;
    int m_cols_count = 0;

    std::vector<std::vector<table_cell>> m_cells;
    std::vector<table_column> m_columns;
    std::vector<table_row> m_rows;
public:

    table_grid()
    {
    }

    void clear();
    void begin_row(element *row);
    void add_cell(element *el);
    bool is_rowspanned(int r, int c);
    void finish();

    table_cell* cell(int t_col, int t_row);
    table_column& column(int c) { return m_columns[c]; }
    table_row& row(int r) { return m_rows[r]; }

    int rows_count() { return m_rows_count; }
    int cols_count() { return m_cols_count; }

    void distribute_max_width(int width, int start, int end);
    void distribute_min_width(int width, int start, int end);
    void distribute_width(int width, int start, int end);
    int calc_table_width(int block_width, bool is_auto);
    void calc_horizontal_positions(margins& table_borders, border_collapse bc, int bdr_space_x);
    void calc_vertical_positions(margins& table_borders, border_collapse bc, int bdr_space_y);

    template<class table_column_accessor>
    void distribute_width(int width, int start, int end, table_column_accessor &acc)
    {
        if (!(start >= 0 && start < m_cols_count && end >= 0 && end < m_cols_count))
        {
            return;
        }

        int cols_width = 0;
        for (int col = start; col <= end; col++)
        {
            cols_width += m_columns[col].max_width;
        }

        int add = width / (end - start + 1);
        int added_width = 0;
        for (int col = start; col <= end; col++)
        {
            if (cols_width)
            {
                add = round_f((float) width * ((float) m_columns[col].max_width / (float) cols_width));
            }
            added_width += add;
            acc.get(m_columns[col]) += add;
        }
        if (added_width < width)
        {
            acc.get(m_columns[start]) += width - added_width;
        }
    }
};
