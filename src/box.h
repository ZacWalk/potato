#pragma once

class element;

enum box_type
{
    box_block,
    box_line
};

class box
{
protected:
    box_type _type;

    int m_box_top;
    int m_box_left;
    int m_box_right;

    element *m_element;
    std::vector<element*> m_items;
    int m_height;
    int m_width;
    int m_line_height;
    font_metrics m_font_metrics;
    int m_baseline;
    text_align m_text_align;

public:

    box(box_type t, int top, int left, int right, int line_height, font_metrics& fm, text_align align)
    {
        _type = t;
        m_box_top = top;
        m_box_left = left;
        m_box_right = right;
        m_element = nullptr;
        m_height = 0;
        m_width = 0;
        m_font_metrics = fm;
        m_line_height = line_height;
        m_baseline = 0;
        m_text_align = align;
    }

    int bottom() { return m_box_top + height(); }
    int top() { return m_box_top; }
    int right() { return m_box_left + width(); }
    int left() { return m_box_left; }

    box_type get_type() const { return _type; };
    
    int height() const;
    int width() const;

    void add_element(element *el);
    bool can_hold(element *el, white_space ws) const;
    void finish(bool last_box = false);
    bool is_empty() const;
    int baseline() const;
    void get_elements(std::vector<element*>& els);
    int top_margin() const;
    int bottom_margin() const;
    void y_shift(int shift);
    void new_width(int left, int right, std::vector<element*>& els);

private:
    element *get_last_space();
    bool is_break_only() const;
};
