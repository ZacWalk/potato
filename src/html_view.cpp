#include "pch.h"
#include "resource.h"
#include "html_view.h"
#include "toolbar.h"



CHtmlView::CHtmlView(Toolbar &tb) : _toolbar(tb)
{
    m_top = 0;
    m_left = 0;
    m_max_top = 0;
    m_max_left = 0;

    m_http.open(L"potato/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS);
}

CHtmlView::~CHtmlView(void)
{
    m_http.close();
}

void CHtmlView::OnPaint(HDC hdc, LPRECT rcDraw)
{
    auto page = m_page;

    if (page)
    {
        render_win32 renderer(hdc, client_pos());

        position clip(rcDraw->left, rcDraw->top, rcDraw->right - rcDraw->left, rcDraw->bottom - rcDraw->top);
        page->draw(renderer, -m_left, -m_top, &clip);
    }
}

void CHtmlView::OnSize(int width, int height)
{
    auto page = m_page;

    if (page)
    {
        page->client_pos(client_pos());
        layout();
        page->media_changed();
    }
}

void CHtmlView::open(const std::wstring &url)
{
    auto pThis = this;
    m_http.download_file(url, std::make_shared<http_request>([pThis, url](const std::wstring &file_name) { pThis->open_file(url, file_name); }));
    SetFocus();
}

void CHtmlView::open_file(const std::wstring &url, const std::wstring &file_name)
{
    open_text(url, get_file_contents(file_name));
    SetFocus();
}


void CHtmlView::open_text(const std::wstring &url, const std::string &text)
{
    open_text(url, ToUtf16(text));
}

void CHtmlView::open_text(const std::wstring &url, const std::wstring &text)
{
    if (m_page)
    {
        m_page->clear();
    }

    win_dc hdc(m_hWnd);
    render_win32 renderer(hdc, client_pos());
    m_page = document::createFromUTF16(*this, url, text);

    layout();
    m_top = 0;
    m_left = 0;

    set_caption();
    update_history();

    _toolbar.Address(url);

    Invalidate();
}

void CHtmlView::layout()
{
    if (m_hWnd)
    {
        auto page = m_page;

        if (page)
        {
            RECT rcClient;
            GetClientRect(&rcClient);

            win_dc hdc(m_hWnd);
            render_win32 renderer(hdc, client_pos());

            int width = rcClient.right - rcClient.left;
            int height = rcClient.bottom - rcClient.top;

            page->render(renderer, width);

            m_max_top = page->height() - height;
            if (m_max_top < 0) m_max_top = 0;

            m_max_left = page->width() - width;
            if (m_max_left < 0) m_max_left = 0;

            update_scroll();
            Invalidate();
        }
    }
}

void CHtmlView::update_scroll()
{
    if (!m_page)
    {
        ShowScrollBar(SB_BOTH, FALSE);
        return;
    }

    if (m_max_top > 0)
    {
        ShowScrollBar(SB_VERT, TRUE);

        RECT rcClient;
        GetClientRect(&rcClient);

        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_ALL;
        si.nMin = 0;
        si.nMax = m_max_top + (rcClient.bottom - rcClient.top);
        si.nPos = m_top;
        si.nPage = rcClient.bottom - rcClient.top;
        SetScrollInfo(SB_VERT, &si, TRUE);
    }
    else
    {
        ShowScrollBar(SB_VERT, FALSE);
    }

    if (m_max_left > 0)
    {
        ShowScrollBar(SB_HORZ, TRUE);

        RECT rcClient;
        GetClientRect(&rcClient);

        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_ALL;
        si.nMin = 0;
        si.nMax = m_max_left + (rcClient.right - rcClient.left);
        si.nPos = m_left;
        si.nPage = rcClient.right - rcClient.left;
        SetScrollInfo(SB_HORZ, &si, TRUE);
    }
    else
    {
        ShowScrollBar(SB_HORZ, FALSE);
    }
}

void CHtmlView::OnVScroll(int pos, int flags)
{
    RECT rcClient;
    GetClientRect(&rcClient);

    int lineHeight = 16;
    int pageHeight = rcClient.bottom - rcClient.top - lineHeight;

    int newTop = m_top;

    switch (flags)
    {
    case SB_LINEDOWN:
        newTop = m_top + lineHeight;
        if (newTop > m_max_top)
        {
            newTop = m_max_top;
        }
        break;
    case SB_PAGEDOWN:
        newTop = m_top + pageHeight;
        if (newTop > m_max_top)
        {
            newTop = m_max_top;
        }
        break;
    case SB_LINEUP:
        newTop = m_top - lineHeight;
        if (newTop < 0)
        {
            newTop = 0;
        }
        break;
    case SB_PAGEUP:
        newTop = m_top - pageHeight;
        if (newTop < 0)
        {
            newTop = 0;
        }
        break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
        newTop = pos;
        if (newTop < 0)
        {
            newTop = 0;
        }
        if (newTop > m_max_top)
        {
            newTop = m_max_top;
        }
        break;
    }

    scroll_to(m_left, newTop);
}

void CHtmlView::OnHScroll(int pos, int flags)
{
    RECT rcClient;
    GetClientRect(&rcClient);

    int lineWidth = 16;
    int pageWidth = rcClient.right - rcClient.left - lineWidth;

    int newLeft = m_left;

    switch (flags)
    {
    case SB_LINERIGHT:
        newLeft = m_left + lineWidth;
        if (newLeft > m_max_left)
        {
            newLeft = m_max_left;
        }
        break;
    case SB_PAGERIGHT:
        newLeft = m_left + pageWidth;
        if (newLeft > m_max_left)
        {
            newLeft = m_max_left;
        }
        break;
    case SB_LINELEFT:
        newLeft = m_left - lineWidth;
        if (newLeft < 0)
        {
            newLeft = 0;
        }
        break;
    case SB_PAGELEFT:
        newLeft = m_left - pageWidth;
        if (newLeft < 0)
        {
            newLeft = 0;
        }
        break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
        newLeft = pos;
        if (newLeft < 0)
        {
            newLeft = 0;
        }
        if (newLeft > m_max_left)
        {
            newLeft = m_max_left;
        }
        break;
    }

    scroll_to(newLeft, m_top);
}

void CHtmlView::OnMouseWheel(int delta)
{
    int lineHeight = 16;

    int newTop = m_top - delta / WHEEL_DELTA * lineHeight * 3;

    if (newTop < 0)
    {
        newTop = 0;
    }
    if (newTop > m_max_top)
    {
        newTop = m_max_top;
    }

    if (newTop != m_top)
    {
        scroll_to(m_left, newTop);
    }
}

void CHtmlView::OnKeyDown(UINT vKey)
{
    switch (vKey)
    {
    case VK_F5:
        refresh();
        break;
    }
}

void CHtmlView::refresh()
{
    auto page = m_page;

    if (page)
    {
        open(page->m_url);
    }
}

void CHtmlView::set_caption()
{
    auto page = m_page;

    if (!page)
    {
        GetParent().SetWindowText(L"potato");
    }
    else
    {
        GetParent().SetWindowText(page->m_caption.c_str());
    }
}

void CHtmlView::OnMouseMove(int x, int y)
{
    auto page = m_page;

    if (page)
    {
        win_dc hdc(m_hWnd);
        render_win32 renderer(hdc, client_pos());
        position::vector redraw_boxes;

        if (page->on_mouse_over(x + m_left, y + m_top, x, y, redraw_boxes))
        {
            for (auto box = redraw_boxes.begin(); box != redraw_boxes.end(); box++)
            {
                box->x -= m_left;
                box->y -= m_top;
                RECT rcRedraw;
                rcRedraw.left = box->left();
                rcRedraw.right = box->right();
                rcRedraw.top = box->top();
                rcRedraw.bottom = box->bottom();
            }

            Invalidate();
            update_cursor();
        }
    }
}

void CHtmlView::OnMouseLeave()
{
    auto page = m_page;

    if (page)
    {
        win_dc hdc(m_hWnd);
        render_win32 renderer(hdc, client_pos());
        position::vector redraw_boxes;

        if (page->on_mouse_leave(redraw_boxes))
        {
            for (auto box = redraw_boxes.begin(); box != redraw_boxes.end(); box++)
            {
                box->x -= m_left;
                box->y -= m_top;
                RECT rcRedraw;
                rcRedraw.left = box->left();
                rcRedraw.right = box->right();
                rcRedraw.top = box->top();
                rcRedraw.bottom = box->bottom();
            }
        }

        Invalidate();
        page.reset();
    }
}

void CHtmlView::OnLButtonDown(int x, int y)
{
    SetFocus();

    auto page = m_page;

    if (page)
    {
        win_dc hdc(m_hWnd);
        render_win32 renderer(hdc, client_pos());
        position::vector redraw_boxes;

        if (page->on_lbutton_down(x + m_left, y + m_top, x, y, redraw_boxes))
        {
            for (auto box = redraw_boxes.begin(); box != redraw_boxes.end(); box++)
            {
                box->x -= m_left;
                box->y -= m_top;
                RECT rcRedraw;
                rcRedraw.left = box->left();
                rcRedraw.right = box->right();
                rcRedraw.top = box->top();
                rcRedraw.bottom = box->bottom();
            }

            Invalidate();
        }
    }
}

void CHtmlView::OnLButtonUp(int x, int y)
{
    auto page = m_page;

    if (page)
    {
        position::vector redraw_boxes;
        win_dc hdc(m_hWnd);
        render_win32 renderer(hdc, client_pos());

        if (page->on_lbutton_up(x + m_left, y + m_top, x, y, redraw_boxes))
        {
            for (auto box = redraw_boxes.begin(); box != redraw_boxes.end(); box++)
            {
                box->x -= m_left;
                box->y -= m_top;
                RECT rcRedraw;
                rcRedraw.left = box->left();
                rcRedraw.right = box->right();
                rcRedraw.top = box->top();
                rcRedraw.bottom = box->bottom();
            }

            Invalidate();
        }
    }
}

void CHtmlView::back()
{
    std::wstring url;
    if (m_history.back(url))
    {
        open(url);
    }
}

void CHtmlView::forward()
{
    std::wstring url;
    if (m_history.forward(url))
    {
        open(url);
    }
}

void CHtmlView::update_cursor()
{
    //LPCWSTR defArrow = m_page_next ? IDC_APPSTARTING : IDC_ARROW;
    LPCWSTR defArrow = IDC_ARROW;

    auto page = m_page;

    if (!page)
    {
        SetCursor(LoadCursor(nullptr, defArrow));
    }
    else
    {
        if (page->m_cursor == L"pointer")
        {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
        }
        else
        {
            SetCursor(LoadCursor(nullptr, defArrow));
        }
    }
}

position CHtmlView::client_pos()
{
    RECT rcClient;
    GetClientRect(&rcClient);

    position client;
    client.x = rcClient.left;
    client.y = rcClient.top;
    client.width = rcClient.right - rcClient.left;
    client.height = rcClient.bottom - rcClient.top;

    return client;
}

void CHtmlView::update_history()
{
    auto page = m_page;

    if (page)
    {
        m_history.url_opened(page->url());
    }
}

void CHtmlView::scroll_to(int new_left, int new_top)
{
    auto client = client_pos();

    if (new_top != m_top)
    {
        m_top = new_top;
        SetScrollPos(SB_VERT, m_top, TRUE);
        Invalidate();
    }

    if (new_left != m_left)
    {
        m_left = new_left;
        SetScrollPos(SB_HORZ, m_left, TRUE);
        Invalidate();
    }
}
