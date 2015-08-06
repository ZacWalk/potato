#pragma once

namespace Color
{
    const unsigned Highlight = 0x00CC6611;
    const unsigned Hover = 0x00999999;
    const unsigned Text = 0x00ffffff;
    const unsigned TaskBackground = 0x00777777;
}

inline COLORREF RGBA(const unsigned r, const unsigned g, const unsigned b, const unsigned a = 255)
{
    return r | (g << 8) | (b << 16) | (a << 24);
};

inline unsigned ByteClamp(const int n)
{
    return (n > 255) ? 255u : (n < 0) ? 0u : static_cast<unsigned>(n);
}

inline COLORREF SaturateRGBA(const int r, const int g, const int b, const int a)
{
    return ByteClamp(r) | (ByteClamp(g) << 8) | (ByteClamp(b) << 16) | (ByteClamp(a) << 24);
};

inline unsigned GetA(const COLORREF c) { return 0xffu & (c >> 24); };
inline unsigned GetR(const COLORREF c) { return 0xffu & c; };
inline unsigned GetG(const COLORREF c) { return 0xffu & (c >> 8); };
inline unsigned GetB(const COLORREF c) { return 0xffu & (c >> 16); };

inline COLORREF Lighten(const COLORREF  c, const int n = 32)
{
    return SaturateRGBA(GetR(c) + n, GetG(c) + n, GetB(c) + n, GetA(c));
}

inline COLORREF Darken(const COLORREF  c, const int n = 32)
{
    return Lighten(c, -n);
}

inline COLORREF Emphasize(const COLORREF  c, const int n = 48)
{
    const bool isLight = GetB(c) > 0x80 || GetG(c) > 0x80 || GetR(c) > 0x80;

    return Lighten(c, isLight ? -n : n);
}

inline SizeI MeasureToolbar(HWND tb)
{
    SizeI result(0, 0);
    auto count = (int)::SendMessage(tb, TB_BUTTONCOUNT, 0, 0L);

    for (int i = 0; i < count; ++i)
    {
        RectI r;
        if ((BOOL)::SendMessage(tb, TB_GETITEMRECT, i, (LPARAM) (LPRECT) r))
        {
            result.cx = std::max(r.right, result.cx);
            result.cy = std::max(r.bottom, result.cy);
        }
    }

    return result;
}


class win_dc
{
public:
    HDC _hdc;
    HWND _hwnd;

    win_dc(HWND hwnd) : _hwnd(hwnd), _hdc(GetDC(hwnd))
    {
    }

    ~win_dc()
    {
        ReleaseDC(_hwnd, _hdc);
    }

    operator HDC()
    {
        return _hdc;
    }

    HFONT SelectFont(HFONT hFont)
    {
        return (HFONT)::SelectObject(_hdc, hFont);
    }
};

inline std::wstring WindowText(HWND h)
{
    auto len = ::GetWindowTextLength(h);
    auto result = (wchar_t*) alloca((len + 1) * sizeof(wchar_t));
    GetWindowText(h, result, len + 1);
    return result;
}

inline void FillSolidRect(HDC hDC, int x, int y, int cx, int cy, COLORREF clr)
{
    RECT rect = { x, y, x + cx, y + cy };
    COLORREF crOldBkColor = ::SetBkColor(hDC, clr);
    ::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rect, nullptr, 0, nullptr);
    ::SetBkColor(hDC, crOldBkColor);
}


inline void FillSolidRect(HDC hDC, const RECT* pRC, COLORREF crColor)
{
    FillSolidRect(hDC, pRC->left, pRC->top, pRC->right - pRC->left, pRC->bottom - pRC->top, crColor);
}