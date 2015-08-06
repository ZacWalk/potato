// Potato.cpp : Defines the entry point for the application.
//

#include "pch.h"
#include "Resource.h"
#include "Util.h"
#include "html_view.h"
#include "switch.h"
#include "toolbar.h"

#pragma comment(lib, "ws2_32") 
#pragma comment( lib, "shlwapi" )
#pragma comment (lib, "gdiplus")
#pragma comment (lib, "Comctl32")

#if defined(_M_IX86)
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined(_M_X64)
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#error "Unsupported Processor architecture"
#endif

#define MAX_BUFFER_LEN 256
#define WM_SOCKET WM_USER + 19

HINSTANCE g_hInstance;

Tasks Switch;
Tasks Async;

std::wstring RunTests();

CEvent eventExit(TRUE, FALSE);

std::wstring empty;
const wchar_t *Title = L"Potato Browser";

class link_command : public ICommand
{
private:
    html_view &_view;
    std::wstring _url;

public:

    link_command(html_view &v, const std::wstring &url) : _view(v), _url(url)
    {
    }

    void Invoke()
    {
        _view.open(_url);
    }
};

class FuncCommand : public ICommand
{
private:
    std::function<void()> _f;

public:

    FuncCommand(std::function<void()> && f) : _f(f) 
    {
    }

    void Invoke()
    {
        _f();
    }
};

class main_frame : public CWindowImpl < main_frame >
{
public:

    //HtmlView _view;

    toolbar _toolbar;
    html_view _view;

    BEGIN_MSG_MAP(main_frame)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_ACTIVATE, OnActivate)
        //MESSAGE_HANDLER(WM_SETFOCUS, OnFocus)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_INITMENUPOPUP, OnInitMenuPopup)
        MESSAGE_HANDLER(WM_SOCKET, OnSocket)
    END_MSG_MAP()

    main_frame() : _toolbar(_view), _view(_toolbar)
    {
    }

    static bool IsPreTranslateMessageMessage(const int message)
    {
        return message != WM_TIMER &&
            message != WM_PAINT &&
            message != WM_ERASEBKGND;
    }

    bool main_frame::PreTranslateMessage(MSG* pMsg)
    {
        if (IsPreTranslateMessageMessage(pMsg->message))
        {
            if (!_toolbar.ShowingMenu())
            {
                if (_toolbar.PreTranslateMessage(pMsg))
                {
                    return TRUE;
                }

                if (pMsg->message == WM_KEYDOWN)
                {
                    auto c = pMsg->wParam;
                    auto flags = static_cast<int>(pMsg->lParam);

                    switch (c)
                    {
                    case VK_BROWSER_BACK:
                        _view.back();
                        return TRUE;
                    case VK_BROWSER_FORWARD:
                        _view.forward();
                        return TRUE;
                    case VK_BROWSER_REFRESH:
                        _view.refresh();
                        return TRUE;
                    case VK_ESCAPE:
                        //if (Escape()) return true;
                        break;
                    }
                }


            }

            if (_toolbar.TranslateAccelerator(pMsg))
                return TRUE;
        }

        return FALSE;
    }

    LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        _toolbar.Create(m_hWnd, nullptr, nullptr, WS_CHILD | WS_VISIBLE, WS_EX_COMPOSITED);
        _view.Create(m_hWnd, nullptr, nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL, WS_EX_COMPOSITED);

        //_view.open(L"http://www.w3.org/Style/CSS/Test/CSS1/current/test5526c.htm"); 
        //_view.open(L"http://www.dmoz.org/");
        _view.open_text(L"test", load_resource_html(IDR_HTML_TEST));

        //auto job = std::make_shared<Load>(_view, m_hWnd, "en.wikipedia.org", "/wiki/World_Wide_Web");
        //_jobs[job->Start()] = job;

        _toolbar.AddCommand(ID_FILE_BBC, std::make_shared<link_command>(_view, L"http://www.bbc.com/"));
        _toolbar.AddCommand(ID_FILE_WIKIPEDIA, std::make_shared<link_command>(_view, L"http://www.wikipedia.org/"));
        _toolbar.AddCommand(ID_FILE_GOOGLE, std::make_shared<link_command>(_view, L"http://www.google.com/"));

        // IDM_ABOUT
        _toolbar.AddCommand(ID_TEST, std::make_shared<FuncCommand>([&](){ _view.open_text(L"test", RunTests()); }));
        _toolbar.AddCommand(IDM_EXIT, std::make_shared<FuncCommand>([&](){ PostMessage(WM_CLOSE); }));
        _toolbar.AddCommand(ID_BROWSE_BACK, std::make_shared<FuncCommand>([&](){ _view.back(); }));
        _toolbar.AddCommand(ID_BROWSE_NEXT, std::make_shared<FuncCommand>([&](){ _view.forward(); }));
        _toolbar.AddCommand(ID_BROWSE_REFRESH, std::make_shared<FuncCommand>([&](){ _view.refresh(); }));

        return 0;
    }

    LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        RectI rcClient;
        GetClientRect(&rcClient);
        //_view.MoveWindow(&rc);

        auto rcToolbar = rcClient;
        auto rcView = rcClient;
        auto height = 36;

        rcToolbar.bottom = rcToolbar.top + height;
        rcView.top += height;

        _toolbar.SetWindowPos(nullptr, rcToolbar, SWP_NOZORDER);
        _view.SetWindowPos(nullptr, rcView, SWP_NOZORDER);

        

        return 0;
    }

    LRESULT OnActivate(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
    {
        LPARAM r = DefWindowProc();

        if (_view.IsWindow() && (wParam == WA_ACTIVE || wParam == WA_CLICKACTIVE))
        {
            _view.SetFocus();
        }

        return r;
    }

    LRESULT OnFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        //_view.SetFocus();
        return 0;
    }

    LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        DestroyWindow();
        PostQuitMessage(0);
        return 0;
    }

    LRESULT OnInitMenuPopup(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
    {
        //_view.EnableMenuItems((HMENU) wParam);
        return 0;
    }

    LRESULT OnSocket(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
    {
        //_jobs[wParam]->Proc(_view, lParam);
        return 0;
    }
};

static int Run(main_frame &frame)
{
    MSG msg;

    for (;;)
    {
        HANDLE h [] = { Switch, eventExit };
        auto n = countof(h);

        switch (MsgWaitForMultipleObjects(n, h, FALSE, INFINITE, QS_ALLINPUT))
        {
        case WAIT_OBJECT_0:
            Switch.Run();
            break;
        case WAIT_OBJECT_0 + 1:
            return 0;
        case WAIT_OBJECT_0 + 2:
            break; // Mindows message
        default:
            return FALSE; // unexpected failure
        }

        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                return (int) msg.wParam;

            if (!frame.PreTranslateMessage(&msg))
            {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
            }
        }
    }

    return (int) msg.wParam;
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPTSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    g_hInstance = hInstance;

    //Initialize Winsock
    WSADATA wsaData;
    int result = 1;

    if (NO_ERROR == WSAStartup(MAKEWORD(2, 2), &wsaData))
    {
        if (SUCCEEDED(CoInitialize(nullptr)))
        {
            Gdiplus::GdiplusStartupInput gdiplusStartupInput;
            ULONG_PTR gdiplusToken;
            Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

            INITCOMMONCONTROLSEX iccx = { sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES };
            ::InitCommonControlsEx(&iccx);

            {
                main_frame frame;
                frame.Create(nullptr, nullptr, Title, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);

                auto icon = LoadIcon(hInstance, MAKEINTRESOURCE(IDC_POTATO));
                frame.SetIcon(icon, TRUE);
                frame.SetIcon(icon, FALSE);
                frame.ShowWindow(SW_SHOW);

                result = Run(frame);
            }

            Gdiplus::GdiplusShutdown(gdiplusToken);
            CoUninitialize();
        }

        WSACleanup();
    }

    return result;
}

