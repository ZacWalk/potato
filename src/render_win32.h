#pragma once

#include "background.h"
#include "borders.h"
#include "web_color.h"

struct list_marker
{
    std::wstring image;
    std::wstring baseurl;
    list_style_type marker_type;
    web_color color;
    position pos;
};

class render_win32
{
protected:

    position::vector m_clips;
    HRGN m_hClipRgn;
    HDC _hdc;
    position _client_pos;

public:

    render_win32(HDC hdc, const position &client_pos) : _hdc(hdc), m_hClipRgn(nullptr), _client_pos(client_pos)
    {
    }

    ~render_win32()
    {
        if (m_hClipRgn)
        {
            DeleteObject(m_hClipRgn);
        }
    }

    void draw_image(const std::shared_ptr<Gdiplus::Bitmap> &bm, const position& pos);
    size get_image_size(const std::shared_ptr<Gdiplus::Bitmap> &bm);

    void apply_clip();
    void del_clip();

    void draw_background(render_win32 &renderer, const background_paint& bg);
    void draw_borders(const css_borders& borders, const position& draw_pos, bool root);
    void draw_ellipse(int x, int y, int width, int height, const web_color& color, int line_width);
    void draw_list_marker(const list_marker &marker);
    void draw_text(const wchar_t* text, HFONT hFont, web_color color, const position& pos);
    void fill_ellipse(int x, int y, int width, int height, const web_color& color);
    void fill_rect(const position& pos, const web_color color, const css_border_radius& radius);
    void fill_rect(int x, int y, int width, int height, const web_color& color, const css_border_radius& radius);
    void release_clip();
    void set_clip(const position& pos, bool valid_x, bool valid_y);
    int line_height(HFONT hFont);
};
