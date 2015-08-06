#include "pch.h"
#include "render_win32.h"
#include "util.h"

static size get_img_size(const std::shared_ptr<Gdiplus::Bitmap> &bmp)
{
    size result;

    if (bmp)
    {
        result.width = bmp->GetWidth();
        result.height = bmp->GetHeight();
    }

    return result;
}


static void draw_img(HDC hdc, const std::shared_ptr<Gdiplus::Bitmap> &bmp, const position& pos)
{
    if (bmp)
    {
        Gdiplus::Graphics graphics(hdc);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        graphics.DrawImage(bmp.get(), pos.x, pos.y, pos.width, pos.height);
    }
}

static void draw_img_bg(HDC hdc, const std::shared_ptr<Gdiplus::Bitmap> &bgbmp, const position& draw_pos, const position& pos, background_repeat repeat, background_attachment attachment)
{
    int img_width = bgbmp->GetWidth();
    int img_height = bgbmp->GetHeight();

    Gdiplus::Graphics graphics(hdc);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    Gdiplus::Region reg(Gdiplus::Rect(draw_pos.left(), draw_pos.top(), draw_pos.width, draw_pos.height));
    graphics.SetClip(&reg);

    switch (repeat)
    {
    case background_repeat_no_repeat:
    {
        graphics.DrawImage(bgbmp.get(), pos.x, pos.y, bgbmp->GetWidth(), bgbmp->GetHeight());
    }
        break;
    case background_repeat_repeat_x:
    {
        Gdiplus::CachedBitmap bmp(bgbmp.get(), &graphics);
        for (int x = pos.left(); x < pos.right(); x += bgbmp->GetWidth())
        {
            graphics.DrawCachedBitmap(&bmp, x, pos.top());
        }

        for (int x = pos.left() - bgbmp->GetWidth(); x + (int) bgbmp->GetWidth() > draw_pos.left(); x -= bgbmp->GetWidth())
        {
            graphics.DrawCachedBitmap(&bmp, x, pos.top());
        }
    }
        break;
    case background_repeat_repeat_y:
    {
        Gdiplus::CachedBitmap bmp(bgbmp.get(), &graphics);
        for (int y = pos.top(); y < pos.bottom(); y += bgbmp->GetHeight())
        {
            graphics.DrawCachedBitmap(&bmp, pos.left(), y);
        }

        for (int y = pos.top() - bgbmp->GetHeight(); y + (int) bgbmp->GetHeight() > draw_pos.top(); y -= bgbmp->GetHeight())
        {
            graphics.DrawCachedBitmap(&bmp, pos.left(), y);
        }
    }
        break;
    case background_repeat_repeat:
    {
        Gdiplus::CachedBitmap bmp(bgbmp.get(), &graphics);
        if (bgbmp->GetHeight() >= 0)
        {
            for (int x = pos.left(); x < pos.right(); x += bgbmp->GetWidth())
            {
                for (int y = pos.top(); y < pos.bottom(); y += bgbmp->GetHeight())
                {
                    graphics.DrawCachedBitmap(&bmp, x, y);
                }
            }
        }
    }
        break;
    }
}



int render_win32::line_height(HFONT hFont)
{
    HFONT oldFont = (HFONT) SelectObject(_hdc, (HFONT) hFont);
    TEXTMETRIC tm;
    GetTextMetrics(_hdc, &tm);
    SelectObject(_hdc, oldFont);
    return (int) tm.tmHeight;
}



void render_win32::draw_text(const wchar_t* text, HFONT hFont, web_color color, const position& pos)
{
    apply_clip();

    HFONT oldFont = (HFONT) SelectObject(_hdc, (HFONT) hFont);

    SetBkMode(_hdc, TRANSPARENT);

    SetTextColor(_hdc, RGB(color.red, color.green, color.blue));

    RECT rcText = { pos.left(), pos.top(), pos.right(), pos.bottom() };
    //DrawText(_hdc, text, -1, &rcText, DT_SINGLELINE | DT_NOPREFIX | DT_BOTTOM | DT_NOCLIP);
    ExtTextOut(_hdc, pos.left(), pos.top(), 0, nullptr, text, wcslen(text), nullptr);

    SelectObject(_hdc, oldFont);

    release_clip();
}

void render_win32::fill_rect(const position& pos, const web_color color, const css_border_radius& radius)
{
    apply_clip();
    fill_rect(pos.x, pos.y, pos.width, pos.height, color, radius);
    release_clip();
}



void render_win32::draw_list_marker(const list_marker &marker)
{
    apply_clip();

    int top_margin = marker.pos.height / 3;

    int draw_x = marker.pos.x;
    int draw_y = marker.pos.y + top_margin;
    int draw_width = marker.pos.height - top_margin * 2;
    int draw_height = marker.pos.height - top_margin * 2;

    switch (marker.marker_type)
    {
    case list_style_type_circle:
    {
        draw_ellipse(draw_x, draw_y, draw_width, draw_height, marker.color, 1);
    }
        break;
    case list_style_type_disc:
    {
        fill_ellipse(draw_x, draw_y, draw_width, draw_height, marker.color);
    }
        break;
    case list_style_type_square:
    {
        fill_rect(draw_x, draw_y, draw_width, draw_height, marker.color, css_border_radius());
    }
        break;
    }
    release_clip();
}

void render_win32::draw_image(const std::shared_ptr<Gdiplus::Bitmap> &bm, const position& pos)
{
    draw_img(_hdc, bm, pos);
}

size render_win32::get_image_size(const std::shared_ptr<Gdiplus::Bitmap> &bm)
{
    return get_img_size(bm);
}


void render_win32::draw_background(render_win32 &renderer, const background_paint& bg)
{
    apply_clip();

    if (bg.color.alpha > 0)
    {
        fill_rect(bg.border_box, bg.color, bg.border_radius);
    }

    auto img = bg.image;

    if (img)
    {
        auto img_sz = get_img_size(img);
        position pos(bg.position_x, bg.position_y, bg.image_size.width, bg.image_size.height);
        auto draw_pos = pos;

        /*if (bg_pos.x.units() != css_units_percentage)
        {
        pos.x += (int) bg_pos.x.val();
        }
        else
        {
        pos.x += (int) ((float) (draw_pos.width - img_sz.width) * bg_pos.x.val() / 100.0);
        }

        if (bg_pos.y.units() != css_units_percentage)
        {
        pos.y += (int) bg_pos.y.val();
        }
        else
        {
        pos.y += (int) ((float) (draw_pos.height - img_sz.height) * bg_pos.y.val() / 100.0);
        }*/

        draw_img_bg(_hdc, img, draw_pos, pos, bg.repeat, bg.attachment);
    }

    release_clip();
}

void render_win32::set_clip(const position& pos, bool valid_x, bool valid_y)
{
    position clip_pos = pos;

    if (!valid_x)
    {
        clip_pos.x = _client_pos.x;
        clip_pos.width = _client_pos.width;
    }
    if (!valid_y)
    {
        clip_pos.y = _client_pos.y;
        clip_pos.height = _client_pos.height;
    }
    m_clips.push_back(clip_pos);
}

void render_win32::del_clip()
{
    if (!m_clips.empty())
    {
        m_clips.pop_back();
        if (!m_clips.empty())
        {
            position clip_pos = m_clips.back();
        }
    }
}

void render_win32::apply_clip()
{
    if (m_hClipRgn)
    {
        DeleteObject(m_hClipRgn);
        m_hClipRgn = nullptr;
    }

    if (!m_clips.empty())
    {
        POINT ptView = { 0, 0 };
        GetWindowOrgEx(_hdc, &ptView);

        position clip_pos = m_clips.back();
        m_hClipRgn = CreateRectRgn(clip_pos.left() - ptView.x, clip_pos.top() - ptView.y, clip_pos.right() - ptView.x, clip_pos.bottom() - ptView.y);
        SelectClipRgn(_hdc, m_hClipRgn);
    }
}

void render_win32::release_clip()
{
    SelectClipRgn(_hdc, nullptr);

    if (m_hClipRgn)
    {
        DeleteObject(m_hClipRgn);
        m_hClipRgn = nullptr;
    }
}

void render_win32::draw_ellipse(int x, int y, int width, int height, const web_color& color, int line_width)
{
    Gdiplus::Graphics graphics(_hdc);
    Gdiplus::LinearGradientBrush* brush = nullptr;

    graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    Gdiplus::Pen pen(Gdiplus::Color(color.alpha, color.red, color.green, color.blue));
    graphics.DrawEllipse(&pen, x, y, width, height);
}

void render_win32::fill_ellipse(int x, int y, int width, int height, const web_color& color)
{
    Gdiplus::Graphics graphics(_hdc);

    graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    Gdiplus::SolidBrush brush(Gdiplus::Color(color.alpha, color.red, color.green, color.blue));
    graphics.FillEllipse(&brush, x, y, width, height);
}

void render_win32::fill_rect(int x, int y, int width, int height, const web_color& color, const css_border_radius& radius)
{
    Gdiplus::Graphics graphics(_hdc);

    Gdiplus::SolidBrush brush(Gdiplus::Color(color.alpha, color.red, color.green, color.blue));
    graphics.FillRectangle(&brush, x, y, width, height);
}


void render_win32::draw_borders(const css_borders& borders, const position& draw_pos, bool root)
{
    apply_clip();

    // draw left border
    if (borders.left.width.val() != 0 && borders.left.style > border_style_hidden)
    {
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(borders.left.color.red, borders.left.color.green, borders.left.color.blue));
        HPEN oldPen = (HPEN) SelectObject(_hdc, pen);
        for (int x = 0; x < borders.left.width.val(); x++)
        {
            MoveToEx(_hdc, draw_pos.left() + x, draw_pos.top(), nullptr);
            LineTo(_hdc, draw_pos.left() + x, draw_pos.bottom());
        }
        SelectObject(_hdc, oldPen);
        DeleteObject(pen);
    }
    // draw right border
    if (borders.right.width.val() != 0 && borders.right.style > border_style_hidden)
    {
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(borders.right.color.red, borders.right.color.green, borders.right.color.blue));
        HPEN oldPen = (HPEN) SelectObject(_hdc, pen);
        for (int x = 0; x < borders.right.width.val(); x++)
        {
            MoveToEx(_hdc, draw_pos.right() - x - 1, draw_pos.top(), nullptr);
            LineTo(_hdc, draw_pos.right() - x - 1, draw_pos.bottom());
        }
        SelectObject(_hdc, oldPen);
        DeleteObject(pen);
    }
    // draw top border
    if (borders.top.width.val() != 0 && borders.top.style > border_style_hidden)
    {
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(borders.top.color.red, borders.top.color.green, borders.top.color.blue));
        HPEN oldPen = (HPEN) SelectObject(_hdc, pen);
        for (int y = 0; y < borders.top.width.val(); y++)
        {
            MoveToEx(_hdc, draw_pos.left(), draw_pos.top() + y, nullptr);
            LineTo(_hdc, draw_pos.right(), draw_pos.top() + y);
        }
        SelectObject(_hdc, oldPen);
        DeleteObject(pen);
    }
    // draw bottom border
    if (borders.bottom.width.val() != 0 && borders.bottom.style > border_style_hidden)
    {
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(borders.bottom.color.red, borders.bottom.color.green, borders.bottom.color.blue));
        HPEN oldPen = (HPEN) SelectObject(_hdc, pen);
        for (int y = 0; y < borders.bottom.width.val(); y++)
        {
            MoveToEx(_hdc, draw_pos.left(), draw_pos.bottom() - y - 1, nullptr);
            LineTo(_hdc, draw_pos.right(), draw_pos.bottom() - y - 1);
        }
        SelectObject(_hdc, oldPen);
        DeleteObject(pen);
    }

    release_clip();
}


