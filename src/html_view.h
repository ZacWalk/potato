#pragma once
#include "document.h"
#include "web_history.h"
#include "render_win32.h"
#include "util.h"

class toolbar;

class html_view : public CWindowImpl < html_view >
{
    typedef html_view thisClass;

private:

    toolbar &_toolbar;

    int m_top;
    int m_left;
    int m_max_top;
    int m_max_left;
    web_history m_history;
    std::shared_ptr<document> m_page;
    http m_http;

public:

    html_view(toolbar &tb);
    ~html_view(void);

    void open(const std::wstring &url);
    void open_file(const std::wstring &url, const std::wstring &filename);
    void open_text(const std::wstring &url, const std::string &text);
    void open_text(const std::wstring &url, const std::wstring &text);
    void refresh();
    void back();
    void forward();
    void set_caption();

    void layout();

    std::wstring url() const { return m_page ? m_page->url() : empty; };

    position client_pos();
    void update_history();

protected:
    void OnPaint(HDC hdc, LPRECT rcDraw);
    void OnSize(int width, int height);
    void OnVScroll(int pos, int flags);
    void OnHScroll(int pos, int flags);
    void OnMouseWheel(int delta);
    void OnKeyDown(UINT vKey);
    void OnMouseMove(int x, int y);
    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnMouseLeave();

    void update_scroll();
    void update_cursor();
    void scroll_to(int new_left, int new_top);


private:

    BEGIN_MSG_MAP(thisClass)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_SETCURSOR, OnSetCursor)
        //MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_VSCROLL, OnVScroll)
        MESSAGE_HANDLER(WM_HSCROLL, OnHScroll)
        MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
        MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
        MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
        MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
        MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
        MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)

    END_MSG_MAP()

    LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        return 0;
    }

    LRESULT OnSetCursor(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        update_cursor();
        return 0;
    }

    LRESULT OnEraseBackground(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        return 0;
    }

    LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(&ps);
        OnPaint(hdc, &ps.rcPaint);
        EndPaint(&ps);
        return 0;
    }

    LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
    {
        OnSize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    }

    LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        return 0;
    }

    LRESULT OnVScroll(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        OnVScroll(HIWORD(wParam), LOWORD(wParam));
        return 0;
    }

    LRESULT OnHScroll(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        OnHScroll(HIWORD(wParam), LOWORD(wParam));
        return 0;
    }

    LRESULT OnMouseWheel(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;
    }

    LRESULT OnKeyDown(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        OnKeyDown((UINT) wParam);
        return 0;
    }

    LRESULT OnMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
    {
        TRACKMOUSEEVENT tme;
        ZeroMemory(&tme, sizeof(TRACKMOUSEEVENT));
        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_QUERY;
        tme.hwndTrack = m_hWnd;
        TrackMouseEvent(&tme);
        if (!(tme.dwFlags & TME_LEAVE))
        {
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = m_hWnd;
            TrackMouseEvent(&tme);
        }
        OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    }

    LRESULT OnMouseLeave(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        OnMouseLeave();
        return 0;
    }

    LRESULT OnLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
    {
        OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    }

    LRESULT OnLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
    {
        OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    }

};