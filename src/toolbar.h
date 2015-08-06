// UpdateUI.h: interface for the CommandFrameBase class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "strings.h"
#include "ui.h"




extern HINSTANCE g_hInstance;

struct ICommand
{
    virtual void Invoke() = 0;
    virtual bool IsEnabled() const { return true; };
    virtual bool IsChecked() const { return false; };
};

typedef std::shared_ptr<ICommand> CommandPtr;






class AutoCompleteResult
{
protected:

    Match _match;
    int _weight;

public:

    RectI Bounds;

    AutoCompleteResult() : _weight(0)
    {
    }

    AutoCompleteResult(const Match &t, int weight) : _match(t), _weight(weight)
    {
    }

    AutoCompleteResult(const AutoCompleteResult &other) : _match(other._match), _weight(other._weight), Bounds(other.Bounds)
    {
    }

    AutoCompleteResult &operator=(const AutoCompleteResult &other)
    {
        _match = other._match;
        _weight = other._weight;
        Bounds = other.Bounds;
        return *this;
    }

    const Match &Text() const
    {
        return _match;
    }

    const int Weight() const
    {
        return _weight;
    }

    static bool CompareWeight(const AutoCompleteResult &l, const AutoCompleteResult &r)
    {
        auto diff = r._weight - l._weight;
        return diff == 0 ? l.Text() < r.Text() : diff < 0;
    }

    static bool CompareWeightReverse(const AutoCompleteResult &l, const AutoCompleteResult &r)
    {
        auto diff = l._weight - r._weight;
        return diff == 0 ? l.Text() < r.Text() : diff < 0;
    }
};

typedef std::vector<AutoCompleteResult> AutoCompleteResults;

template<class TCompleteStrategy, class TEdit>
class ListWnd : public CWindowImpl < ListWnd<TCompleteStrategy, TEdit> >
{
private:

    AutoCompleteResults _results;
    std::shared_ptr<TCompleteStrategy> _completes;
    TEdit &_edit;

    AutoCompleteResult *_selectedItem;
    AutoCompleteResult *_hoverItem;

    volatile long _pin;

    SizeI _extent;
    PointI _offset;
    int _yMax;
    int _yTrackingStart;
    int _yTrackingOffsetStart;
    int _height;
    bool _highlightScroll;
    bool _hover;
    bool _tracking;
    bool _trackingScrollbar;
    COLORREF _background;

public:


    static const int RowHeight = 26;

    ListWnd(TEdit &edit, const std::shared_ptr<TCompleteStrategy> &c, int height = 8 + RowHeight * 20, COLORREF bg = 0x888888) :
        _selectedItem(nullptr),
        _hoverItem(nullptr),
        _offset(0, 0),
        _yMax(0),
        _yTrackingStart(0),
        _yTrackingOffsetStart(0),
        _highlightScroll(false),
        _tracking(false),
        _trackingScrollbar(false),
        _completes(c),
        _edit(edit),
        _pin(0),
        _height(height),
        _background(bg)
    {
    }

    int Height() const { return _height; };

    BEGIN_MSG_MAP(AddressComplete)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_MOUSEACTIVATE, OnMouseActivate)
        MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
        MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
        MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnMouseDblClk)
        MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
        MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
        MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)

        MESSAGE_HANDLER(WM_SEARCHING_COMPLETE, OnSearchComplete)
    END_MSG_MAP()

    LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        return 0;
    }

    LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
    {
        _extent.cx = LOWORD(lParam);
        _extent.cy = HIWORD(lParam);
        Layout();
        return 0;
    }

    LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        DestroyWindow();
        PostQuitMessage(0);
        return 0;
    }

    LRESULT OnPaint(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        ATLASSERT(::IsWindow(m_hWnd));

        if (wParam != 0)
        {
            OnPaint((HDC) wParam);
        }
        else
        {
            RectI r;
            GetClientRect(r);

            PAINTSTRUCT ps;
            auto hPaintDc = BeginPaint(&ps);
            auto hdc = CreateCompatibleDC(hPaintDc);
            auto hBitmap = CreateCompatibleBitmap(hPaintDc, r.Width(), r.Height());
            auto hOldBitmap = SelectObject(hdc, hBitmap);

            OnPaint(hdc);
            BitBlt(hPaintDc, 0, 0, r.Width(), r.Height(), hdc, 0, 0, SRCCOPY);

            SelectObject(hdc, hOldBitmap);
            DeleteObject(hBitmap);
            DeleteDC(hdc);
            EndPaint(&ps);
        }

        return 0;
    }

    LRESULT OnEraseBackground(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        return 1;
    }

    LRESULT OnMouseActivate(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
    {
        return MA_NOACTIVATE;
    }

    void Layout()
    {
        win_dc dc(m_hWnd);

        auto hOldFont = dc.SelectFont(_edit.GetFont());
        auto y = 4;
        auto n = 0;

        for (auto &i : _results)
        {
            RectI bounds(0, y, _extent.cx, y + RowHeight);
            i.Bounds = bounds;
            y = bounds.bottom;
        }

        _yMax = y + 4;
        dc.SelectFont(hOldFont);
        Invalidate(FALSE);
    }

    void OnPaint(HDC hdc)
    {
        auto now = time(nullptr);

        RectI r;
        GetClientRect(r);
        FillSolidRect(hdc, r, _background);

        auto hOldFont = (HFONT)::SelectObject(hdc, _edit.GetFont());
        SetBkMode(hdc, TRANSPARENT);

        for (const auto &i : _results)
        {
            auto bounds = i.Bounds.Offset(-_offset);

            if (bounds.Intersects(r))
            {
                if (&i == _hoverItem)
                {
                    FillSolidRect(hdc, bounds, Color::Hover);
                }

                if (&i == _selectedItem)
                {
                    FillSolidRect(hdc, bounds, Color::Highlight);
                }

                SetTextColor(hdc, Color::Text);

                bounds = bounds.Inflate(-8, -4);
                i.Text().Draw(hdc, bounds);
            }
        }

        DrawScroll(hdc, _highlightScroll, _trackingScrollbar);

        ::SelectObject(hdc, hOldFont);
    }

    bool CanScroll() const
    {
        return _yMax > _extent.cy;
    }

    static inline COLORREF HandleColor(bool highlight, bool tracking, COLORREF defaultColor = Color::Hover)
    {
        if (tracking)
        {
            return Color::Highlight;
        }
        else if (highlight)
        {
            return Color::Hover;
        }

        return defaultColor;
    }

    void DrawScroll(HDC hdc, bool highlight, bool tracking)
    {
        if (CanScroll())
        {
            auto y = MulDiv(_offset.y, _extent.cy, _yMax);
            auto cy = MulDiv(_extent.cy, _extent.cy, _yMax);
            auto xPadding = 0;
            auto right = _extent.cx;

            if (highlight || tracking)
            {
                FillSolidRect(hdc, RectI(right - 26, 0, right, _extent.cy), 0x282828u);
                xPadding = 10;
            }

            FillSolidRect(hdc, RectI(right - 12 - xPadding, y, right - 4, y + cy), HandleColor(highlight, tracking));
        }
    }

    AutoCompleteResult *SelectionFromPoint(PointI &pt)
    {
        for (auto &i : _results)
        {
            if (i.Bounds.Contains(pt))
                return &i;
        }

        return nullptr;
    }

    void Hover(AutoCompleteResult *i)
    {
        if (_hoverItem != i)
        {
            _hoverItem = i;
            Invalidate(FALSE);
        }
    }

    void Selected(AutoCompleteResult *i)
    {
        if (_selectedItem != i)
        {
            //_edit
            _selectedItem = i;
            Invalidate(FALSE);

            if (i)
            {
                if (!_completes->AutoSelect)
                {
                    ScopeLockedCount l(_pin);

                    _edit.SetWindowText(i->Text().Text().c_str());
                    _edit.SendMessage(EM_SETSEL, 0, -1);
                    _edit.SendMessage(EM_SCROLLCARET);
                    _edit.SetFocus();
                }

                _completes->Selected(i);
                MakeVisible(i);
            }
        }
    }

    void MakeVisible(AutoCompleteResult *i)
    {
        if (i)
        {
            auto bounds = i->Bounds;

            if (bounds.top < _offset.y)
            {
                ScrollTo(bounds.top);
            }
            else if (bounds.bottom > (_offset.y + _extent.cy))
            {
                ScrollTo(bounds.bottom - _extent.cy);
            }
        }
    }

    AutoCompleteResult *Selected() const
    {
        return _selectedItem;
    }

    LRESULT OnLButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
    {
        PointI point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        Invalidate(false);

        //SetFocus();

        if (IsOverScrollbar(point))
        {
            if (!_tracking)
            {
                _yTrackingStart = point.y;
                _yTrackingOffsetStart = _offset.y;

                _trackingScrollbar = IsOverScrollbar(point);
                _tracking = true;
                SetCapture();
            }
        }
        else
        {
            auto i = SelectionFromPoint(point + _offset);
            Hover(i);
            Selected(i);
            _completes->Click(m_hWnd, i, false);
        }

        return 0;
    }

    LRESULT OnMouseDblClk(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
    {
        PointI point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        auto i = SelectionFromPoint(point + _offset);
        Hover(i);
        Selected(i);
        _completes->Click(m_hWnd, i, true);
        return 0;
    }

    LRESULT OnMouseMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        PointI point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

        if (!_hover)
        {
            TRACKMOUSEEVENT tme = { 0 };
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = m_hWnd;
            _TrackMouseEvent(&tme);
            _hover = true;
        }

        if (_trackingScrollbar)
        {
            auto offset = MulDiv(point.y - _yTrackingStart, _yMax, _extent.cy);
            ScrollTo(_yTrackingOffsetStart + offset);
        }
        else
        {
            UpdateMousePos(point);
        }

        Hover(SelectionFromPoint(point + _offset));
        return 0;
    }

    LRESULT OnLButtonUp(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
    {
        if (_tracking)
        {
            _tracking = false;
            _trackingScrollbar = false;
            _yTrackingStart = 0;
            ReleaseCapture();
            Invalidate();
        }

        return 0;
    }

    LRESULT OnMouseLeave(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
    {
        _hoverItem = nullptr;
        _hover = false;
        _highlightScroll = false;
        Invalidate(FALSE);
        return 0;
    }

    LRESULT OnMouseWheel(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        if (CanScroll())
        {
            ScrollDelta(((short) HIWORD(wParam)) / 2);
        }
        else
        {
            bHandled = false;
        }

        return 0;
    }

    void ScrollDelta(int delta)
    {
        ScrollTo(_offset.y - delta);
    }

    bool IsOverScrollbar(const PointI &point) const
    {
        return (_extent.cx - 32) < point.x;
    }

    void UpdateMousePos(const PointI &point)
    {
        auto h = IsOverScrollbar(point);

        if (_highlightScroll != h)
        {
            _highlightScroll = h;
            Invalidate();
        }
    }

    void ScrollTo(int offset)
    {
        offset = ClampI(offset, 0, _yMax - _extent.cy);

        if (_offset.y != offset)
        {
            _offset.y = offset;
            Layout();
            Invalidate();
        }
    }

    void StepSelection(int i)
    {
        auto size = static_cast<int>(_results.size());

        if (!_results.empty())
        {
            bool found = false;

            if (i > 0)
            {
                for (auto &i : _results)
                {
                    if (found)
                    {
                        Selected(&i);
                        return;
                    }

                    found = _selectedItem == &i;
                }

                Selected(&_results.front());
            }
            else
            {
                for (auto i = _results.rbegin(); i != _results.rend(); i++)
                {
                    if (found)
                    {
                        Selected(&*i);
                        return;
                    }

                    found = _selectedItem == &*i;
                }

                Selected(&_results.back());
            }
        }
    }

    LRESULT OnSearchComplete(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        ShowResults(_completes->Results());
        return 0;
    }

    void Cancel()
    {
        if (m_hWnd)
        {
            _results.clear();
            Selected(nullptr);
            Hover(nullptr);
            ShowWindow(SW_HIDE);
            // TODO RenderWnd().SetFocus();
        }
    }

    void Search(const std::wstring &text)
    {
        if (!_pin)
        {
            _completes->Search(m_hWnd, text);
        }
    }

    void ShowResults(const AutoCompleteResults &results)
    {
        _results = results;
        Hover(nullptr);
        Selected(nullptr);
        Layout();
        _offset.y = 0;

        if (TCompleteStrategy::ResizeToShowResults)
        {
            auto cy = std::min(_height, (10 + RowHeight * static_cast<int>(_results.size())));

            RectI r;
            GetClientRect(r);
            SetWindowPos(nullptr, 0, 0, r.Width(), cy, SWP_NOACTIVATE | SWP_NOMOVE | (_results.empty() ? SWP_HIDEWINDOW : SWP_SHOWWINDOW));
        }

        if (_completes->AutoSelect && !_results.empty())
        {
            Selected(&_results.front());
        }

        Invalidate();
    }
};


inline bool AutoCompleteMatch(const std::wstring &qq, const std::wstring &vv, Match &match)
{
    auto q = qq.c_str();
    auto v = vv.c_str();

    if (q && *q != 0 && v && *v != 0)
    {
        if (q[1] == 0) // Single char?
        {
            if (Normalize(v[0]) == Normalize(q[0]))
            {
                match.Text(vv, Range(0, 1));
                return true;
            }
        }
        else
        {
            auto found = SubString(v, q);

            if (!found.empty())
            {
                match.Text(vv, found);
                return true;
            }
        }
    }

    return false;
}

const static int WM_SEARCHING_COMPLETE = WM_USER + 101;

class AddressComplete : public std::enable_shared_from_this < AddressComplete >
{
    AutoCompleteResults _results;
    std::map<std::wstring, int> _urls;
    CHtmlView &_view;

public:

    static const bool AutoSelect = false;
    static const bool ResizeToShowResults = true;

    AddressComplete(CHtmlView &v) : _view(v)
    {
    }

    void Init(HWND hWnd)
    {
        auto t = shared_from_this();

        _urls[L"163.com"];
        _urls[L"360.cn"];
        _urls[L"Akamaihd.net"];
        _urls[L"Buzzfeed.com"];
        _urls[L"Cntv.cn"];
        _urls[L"Googleadservices.com"];
        _urls[L"Naver.com"];
        _urls[L"Onclickads.net"];
        _urls[L"PayPal.com"];
        _urls[L"adcash.com"];
        _urls[L"adobe.com"];
        _urls[L"alibaba.com"];
        _urls[L"aliexpress.com"];
        _urls[L"alipay.com"];
        _urls[L"amazon.cn"];
        _urls[L"amazon.co.jp"];
        _urls[L"amazon.co.uk"];
        _urls[L"amazon.com"];
        _urls[L"amazon.de"];
        _urls[L"apple.com"];
        _urls[L"ask.com"];
        _urls[L"baidu.com"];
        _urls[L"bbc.co.uk"];
        _urls[L"bestbuy.com"];
        _urls[L"bing.com"];
        _urls[L"blogger.com"];
        _urls[L"blogspot.com"];
        _urls[L"cnn.com"];
        _urls[L"craigslist.org"];
        _urls[L"dailymotion.com"];
        _urls[L"dropbox.com"];
        _urls[L"ebay.co.uk"];
        _urls[L"ebay.com"];
        _urls[L"ebay.de"];
        _urls[L"espn.go.com"];
        _urls[L"facebook.com"];
        _urls[L"fc2.com"];
        _urls[L"flipkart.com"];
        _urls[L"gmw.cn"];
        _urls[L"go.com"];
        _urls[L"google.ca"];
        _urls[L"google.co.in"];
        _urls[L"google.co.jp"];
        _urls[L"google.co.uk"];
        _urls[L"google.com"];
        _urls[L"google.com.au"];
        _urls[L"google.com.br"];
        _urls[L"google.com.hk"];
        _urls[L"google.com.mx"];
        _urls[L"google.com.tr"];
        _urls[L"google.de"];
        _urls[L"google.es"];
        _urls[L"google.fr"];
        _urls[L"google.it"];
        _urls[L"google.pl"];
        _urls[L"google.ru"];
        _urls[L"googleusercontent.com"];
        _urls[L"hao123.com"];
        _urls[L"huffingtonpost.com"];
        _urls[L"imdb.com"];
        _urls[L"imgur.com"];
        _urls[L"instagram.com"];
        _urls[L"kickass.so"];
        _urls[L"linkedin.com"];
        _urls[L"live.com"];
        _urls[L"mail.ru"];
        _urls[L"microsoft.com"];
        _urls[L"msn.com"];
        _urls[L"netflix.com"];
        _urls[L"odnoklassniki.ru"];
        _urls[L"people.com.cn"];
        _urls[L"pinterest.com"];
        _urls[L"pornhub.com"];
        _urls[L"qq.com"];
        _urls[L"rakuten.co.jp"];
        _urls[L"reddit.com"];
        _urls[L"sina.com.cn"];
        _urls[L"sohu.com"];
        _urls[L"soso.com"];
        _urls[L"stackoverflow.com"];
        _urls[L"t.co"];
        _urls[L"taobao.com"];
        _urls[L"thepiratebay.se"];
        _urls[L"tmall.com"];
        _urls[L"tumblr.com"];
        _urls[L"twitter.com"];
        _urls[L"vk.com"];
        _urls[L"walmart.com"];
        _urls[L"weibo.com"];
        _urls[L"wikipedia.org"];
        _urls[L"wordpress.com"];
        _urls[L"xhamster.com"];
        _urls[L"xinhuanet.com"];
        _urls[L"xnxx.com"];
        _urls[L"xvideos.com"];
        _urls[L"yahoo.co.jp"];
        _urls[L"yahoo.com"];
        _urls[L"yandex.ru"];
        _urls[L"youradexchange.com"];
        _urls[L"youtube.com"];
        _urls[L"theguardian.com"];
        _urls[L"bbc.co.uk"];
    }

    void Search(HWND hWnd, const std::wstring &query)
    {
        AutoCompleteResults found;

        if (query.empty())
        {
            for (auto &url : _urls)
            {
                found.push_back(AutoCompleteResult(Match(url.first), url.second));
            }
        }
        else
        {
            for (auto &url : _urls)
            {
                Match m;

                if (AutoCompleteMatch(query, url.first, m))
                {
                    found.push_back(AutoCompleteResult(m, url.second));
                }
            }
        }

        std::sort(found.begin(), found.end(), AutoCompleteResult::CompareWeight);

        unsigned int max = 64;

        if (found.size() > max)
        {
            found.erase(found.begin() + max, found.end());
        }

        std::swap(_results, found);

        ::PostMessage(hWnd, WM_SEARCHING_COMPLETE, 0, 0);
    }

    const AutoCompleteResults &Results()
    {
        return _results;
    }

    void Selected(AutoCompleteResult *i)
    {
    }

    void Click(HWND hwnd, AutoCompleteResult *i, bool isDoubleClick)
    {
        if (i)
        {
            _view.open(i->Text().Text());
        }
    }
};

class AddressEdit : public CWindowImpl < AddressEdit, CWindow >
{
private:

    int _icon;
    CWindow _popup;

public:

    DECLARE_WND_SUPERCLASS(L"potato_edit", _T("EDIT"))

    AddressEdit() : _icon(-1)
    {
    }

    BEGIN_MSG_MAP(AddressEdit)
        MESSAGE_HANDLER(WM_NCCALCSIZE, OnNcCalSizeI)
        MESSAGE_HANDLER(WM_NCPAINT, OnNcPaint)
        //MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
        //MESSAGE_HANDLER(WM_KILLFOCUS, OnKillFocus)  
        //MESSAGE_HANDLER(WM_IME_NOTIFY, OnImeNotify)
    END_MSG_MAP()

    LRESULT OnNcCalSizeI(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
    {
        OnNcCalSizeI((LPRECT) lParam);
        return 0;
    }

    void SetPopup(HWND popup)
    {
        _popup = popup;
    }

    void SetIcon(int i)
    {
        if (_icon != i)
        {
            _icon = i;
            SetWindowPos(HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_DRAWFRAME);
        }
    }

    void OnNcCalSizeI(LPRECT pr)
    {
        assert(pr);

        win_dc dc(m_hWnd);

        RectI r;
        auto hOld = ::SelectObject(dc, GetFont());
        ::DrawText(dc, L"Ky", -1, r, DT_CALCRECT | DT_EDITCONTROL);
        ::SelectObject(dc, hOld);

        auto cyText = r.bottom - r.top + 2;
        auto isMultiLine = (GetStyle() & ES_MULTILINE) != 0;
        auto h = pr->bottom - pr->top;
        auto cy = isMultiLine ? 8 : (h - cyText) / 2;
        auto cx = isMultiLine ? 8 : 8;

        pr->left += cx + (_icon == -1 ? 0 : 22);
        pr->right -= cx;
        pr->top += cy;
        pr->bottom -= cy;
    }

    static void DrawRoundRect(Gdiplus::Graphics& graphics, Gdiplus::Rect rect, int xy, Gdiplus::Brush &brush, Gdiplus::Pen &pen)
    {
        int left = std::min(rect.GetLeft(), rect.GetRight());
        int right = std::max(rect.GetLeft(), rect.GetRight());

        int top = std::min(rect.GetTop(), rect.GetBottom());
        int bottom = std::max(rect.GetTop(), rect.GetBottom());

        Gdiplus::GraphicsPath path;
        path.AddArc(right - xy, top, xy, xy, 270, 90);
        path.AddArc(right - xy, bottom - xy, xy, xy, 0, 90);
        path.AddArc(left, bottom - xy, xy, xy, 90, 90);
        path.AddArc(left, top, xy, xy, 180, 90);
        path.AddLine(left + xy, top, right - xy / 2, top);

        graphics.FillPath(&brush, &path);
        graphics.DrawPath(&pen, &path);
    }

    LRESULT OnNcPaint(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        DefWindowProc();

        HDC dc = ::GetDCEx(m_hWnd, (HRGN) wParam, DCX_WINDOW | DCX_INTERSECTRGN | 0x10000);
        if (dc == nullptr) dc = ::GetWindowDC(m_hWnd);

        if (dc)
        {
            auto clr = IsWindowEnabled() ? 0xFFFFFF : 0X888888;
            auto bg = GetSysColor(COLOR_3DFACE);
            auto edit = GetSysColor(COLOR_WINDOW);


            RectI r;
            GetWindowRect(r);
            RectI outside(2, 2, r.Width() - 2, r.Height() - 2);
            RectI inside = outside;
            OnNcCalSizeI(inside);

            //FillSolidRect(dc, outside, bg);

            Gdiplus::Graphics g(dc);
            g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

            Gdiplus::SolidBrush brush(edit | 0xFF000000);
            Gdiplus::SolidBrush black(0x22333333);
            Gdiplus::Pen pen(&black, 1.5f);

            DrawRoundRect(g, outside, 12, brush, pen);


            if (_icon != -1)
            {
                // TODO
                //ImageList_Draw(Dialog::SmallToolbarImageList(), _icon, dc, 8, (r.Height() - 16) / 2, ILD_TRANSPARENT);
            }

            ReleaseDC(dc);
        }

        return 0;
    }

    LRESULT OnImeNotify(UINT message, WPARAM wparam, LPARAM lparam, BOOL& bHandled)
    {
        LRESULT l = DefWindowProc();

        if (IMN_SETCOMPOSITIONWINDOW == wparam)
        {
            if (_popup && _popup.IsWindow())
            {
                _popup.SetWindowPos(nullptr, PopupRect(), SWP_NOACTIVATE | SWP_NOSIZE);
            }
        }

        return l;
    }

    RectI PopupRect()
    {
        RectI editRect, popupRect;
        GetWindowRect(editRect);

        if (_popup)
        {
            _popup.GetWindowRect(popupRect);
        }

        return RectI(editRect.left, editRect.bottom, editRect.Width(), popupRect.Height());
    }
};


class Toolbar :
    public CWindowImpl < Toolbar >
{

private:

    CHtmlView &_view;

    std::vector<int> _commandIds;

    typedef std::vector<ACCEL> ACCELLIST;
    typedef std::map<int, ACCELLIST> MAPIDTOACCEL;
    typedef std::map<wchar_t, int> MAPSHORTCUTS;

    MAPIDTOACCEL _mapAccel;

    volatile long _showingMenu;
    bool _enablePopupItems;
    std::wstring _tooltip;

    std::set<int> _alwaysEnable;
    std::map<DWORD, CommandPtr> _commands;
    std::map<int, std::wstring> _idToText;

    HWND _navigate1;
    HWND _more;

    AddressEdit _address;

    std::shared_ptr<AddressComplete> _completes;
    ListWnd<AddressComplete, AddressEdit> _popup;

    HACCEL _hAccel;

    //HHOOK _hMenuHook;
    //RectI m_rcButton;

    bool _addressHasFocus;

public:

    DECLARE_WND_CLASS_EX(L"potato_toolbar", CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS, COLOR_3DFACE);

    static const int topControlsHeight = 50;

    Toolbar(CHtmlView &v) : _view(v), _hAccel(nullptr), _showingMenu(0), _addressHasFocus(false), _enablePopupItems(true), _completes(std::make_shared<AddressComplete>(v)), _popup(_address, _completes)
    {
        _alwaysEnable.insert(ID_VIEW_MENU);
    }

    ~Toolbar()
    {
        ATLTRACE("Destroy Toolbar\n");
    }

    bool ShowingMenu() const
    {
        return _showingMenu > 0;
    }

    bool TranslateAccelerator(LPMSG pMsg)
    {
        return _hAccel != nullptr && ::TranslateAccelerator(m_hWnd, _hAccel, pMsg);
    }

    static void StripMenuString(LPWSTR szOut, LPCWSTR szIn)
    {
        // Rest shortcut
        auto pch = szIn;
        auto pch2 = szOut;

        while (*pch != '\0')
        {
            auto ch = *pch++;

            if (ch != '&')
            {
                if (ch == '\t') break;
                *pch2++ = ch;
            }
        }

        *pch2 = 0;
    }

    static void ConstructMenuString(LPWSTR szOut, int szOutSize, LPCWSTR szIn, const ACCELLIST &pal, MAPSHORTCUTS &mapShortcuts)
    {
        bool bInsertedShortcut = false;
        auto pch = szIn;
        auto pch2 = szOut;

        // Rest shortcut
        while (*pch != '\0')
        {
            auto ch = *pch++;

            if (ch != '&')
            {
                if (ch == '\t')
                    break;

                if (!bInsertedShortcut && ch != ' ')
                {
                    auto it = mapShortcuts.find(ch);

                    if (it == mapShortcuts.end())
                    {
                        *pch2++ = '&';
                        mapShortcuts[ch] = 1;
                        bInsertedShortcut = true;
                    }
                }

                *pch2++ = ch;
            }
        }

        *pch2 = 0;

        // Add Accelerator
        bool isFirst = true;

        for (auto &i : pal)
        {
            if (isFirst)
            {
                wcscat_s(szOut, szOutSize, L"\t");
            }
            else
            {
                wcscat_s(szOut, szOutSize, L" OR ");
            }

            if (i.fVirt & FALT)
            {
                wcscat_s(szOut, szOutSize, L"Alt+");
            }
            if (i.fVirt & FCONTROL)
            {
                wcscat_s(szOut, szOutSize, L"Ctrl+");
            }
            if (i.fVirt & FSHIFT)
            {
                wcscat_s(szOut, szOutSize, L"Shift+");
            }

            if (i.key == VK_RETURN)
            {
                wcscat_s(szOut, szOutSize, L"Enter");
            }
            else if (i.fVirt & FVIRTKEY)
            {
                wchar_t keyname[64];
                UINT vkey = MapVirtualKey(i.key, 0) << 16;

                if (0x30 >= i.key && VK_SPACE != i.key && VK_TAB != i.key)
                {
                    vkey |= (1 << 24);
                }

                GetKeyNameTextW(vkey, keyname, 64);
                wcscat_s(szOut, szOutSize, keyname);
            }
            else
            {
                wchar_t szTemp[2] = { (char) i.key, 0 };
                wcscat_s(szOut, szOutSize, szTemp);
            }

            isFirst = false;
        }
    }

    bool InvokeCommand(WORD id)
    {
        //Core::ScopeLockedCount sl1(Core::DontHideCursor);
        //Core::ScopeLockedCount sl2(Core::CommandActive);

        auto pCommand = FindCommand(id);

        if (pCommand)
        {
            pCommand->Invoke();
            return true;
        }

        return false;
    };

    void InitMenu()
    {
        GetSystemSettings();

        _hAccel = ::LoadAccelerators(g_hInstance, MAKEINTRESOURCE(IDC_POTATO));

        if (_hAccel)
        {
            auto n = CopyAcceleratorTable(_hAccel, nullptr, 0);

            if (n > 0)
            {
                auto pAccel = (ACCEL*) alloca(sizeof(ACCEL) * n);
                CopyAcceleratorTable(_hAccel, pAccel, n);

                for (auto i = 0; i < n; i++)
                {
                    auto ac = pAccel[i];
                    _mapAccel[ac.cmd].push_back(ac);
                }
            }
        }

        HMENU menu = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDC_POTATO));
        menu = GetSubMenu(menu, 0);

        MAPSHORTCUTS shortcuts;
        InitMenuText(menu, shortcuts);
    }

    void EnableToolbar(HWND tb)
    {
        SizeI result(0, 0);
        int count = (int)::SendMessage(tb, TB_BUTTONCOUNT, 0, 0L);

        for (int i = 0; i < count; ++i)
        {
            TBBUTTON button;
            if (::SendMessage(tb, TB_GETBUTTON, i, (LPARAM) &button))
            {
                auto id = button.idCommand;

                if (_alwaysEnable.find(id) != _alwaysEnable.end())
                {
                    ::SendMessage(tb, TB_ENABLEBUTTON, id, MAKELPARAM(true, 0));
                }
                else
                {
                    auto c = FindCommand(id);
                    auto enable = c && c->IsEnabled();
                    auto check = (c && c->IsChecked()) ? 1 : 0;

                    ::SendMessage(tb, TB_ENABLEBUTTON, id, MAKELPARAM(enable, 0));
                    ::SendMessage(tb, TB_CHECKBUTTON, id, MAKELPARAM(check, 0));
                }
            }
        }
    }

    void EnableToolbars()
    {
        EnableToolbar(_navigate1);
        EnableToolbar(_more);
    }

    void EnableMenu(HMENU hMenu)
    {
        MENUITEMINFO mii;
        mii.cbSize = sizeof(MENUITEMINFO);
        mii.fMask = MIIM_SUBMENU | MIIM_ID | MIIM_STATE;

        auto cItems = GetMenuItemCount(hMenu);

        for (auto i = 0; i < cItems; i++)
        {
            if (GetMenuItemInfo(hMenu, i, TRUE, &mii))
            {
                if (mii.hSubMenu)
                {
                    EnableMenu(mii.hSubMenu);
                }
                else
                {
                    auto c = FindCommand(mii.wID);
                    auto enable = c && c->IsEnabled();
                    auto check = c && c->IsChecked();

                    mii.fState &= ~(MFS_CHECKED | MFS_DISABLED);
                    if (!enable) mii.fState |= MFS_DISABLED;
                    if (check) mii.fState |= MFS_CHECKED;

                    SetMenuItemInfo(hMenu, i, TRUE, &mii);
                }
            }
        }
    }

    struct IconIndex
    {
        enum
        {
            Down = 0,
            Left = 1,
            Right = 2,
            Up = 3,
            Menu = 4,
            Refresh = 5,
            Settings = 6,
            Stop = 7,
            Home = 6,
        };
    };

    inline CComPtr<IStream> LoadResourceStream(LPCTSTR pType, LPCTSTR pName)
    {

        auto hResource = ::FindResource(g_hInstance, pName, pType);

        if (hResource)
        {
            auto imageSize = ::SizeofResource(g_hInstance, hResource);

            if (imageSize)
            {
                auto pResourceData = ::LockResource(::LoadResource(g_hInstance, hResource));
                if (pResourceData)
                {
                    auto hBuffer = ::GlobalAlloc(GMEM_MOVEABLE, imageSize);

                    if (hBuffer)
                    {
                        auto pBuffer = ::GlobalLock(hBuffer);

                        if (pBuffer)
                        {
                            ::CopyMemory(pBuffer, pResourceData, imageSize);

                            CComPtr<IStream> result;

                            if (::CreateStreamOnHGlobal(hBuffer, TRUE, &result) == S_OK)
                            {
                                return result;
                            }
                        }

                        ::GlobalUnlock(hBuffer);
                        ::GlobalFree(hBuffer);
                    }
                }
            }
        }

        return nullptr;
    }


    LRESULT OnCreate(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        TBBUTTON tbButtonsNav1 [] =
        {
            { IconIndex::Left, ID_BROWSE_BACK, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, 0L, 0 },
            { IconIndex::Right, ID_BROWSE_NEXT, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, 0L, 0 },
            { IconIndex::Refresh, ID_BROWSE_REFRESH, TBSTATE_ENABLED, BTNS_DROPDOWN | BTNS_AUTOSIZE, 0L, 0 },
            { IconIndex::Home, ID_BROWSE_HOME, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, 0L, 0 },
        };

        TBBUTTON tbMore [] =
        {
            { IconIndex::Menu, ID_VIEW_MENU, TBSTATE_ENABLED, BTNS_DROPDOWN | BTNS_AUTOSIZE, 0L, 0 },
        };

        auto xy = 26;
        auto il = ImageList_Create(xy, xy, ILC_COLOR32 | ILC_MASK, 0, 1);

        Gdiplus::Bitmap toolbarBitmap(xy * 8, xy, PixelFormat32bppPARGB);

        {
            Gdiplus::Graphics g(&toolbarBitmap);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

            int resources [] = { IDB_ARROW_DOWN, IDB_ARROW_LEFT, IDB_ARROW_RIGHT, IDB_ARROW_UP, IDB_MENU, IDB_REFRESH, IDB_SETTINGS, IDB_STOP, 0 };

            for (int i = 0; resources[i] != 0; i++)
            {
                Gdiplus::Bitmap arrowDown(LoadResourceStream(L"PNG", MAKEINTRESOURCE(resources[i])));
                g.DrawImage(&arrowDown, xy * i, 0, xy, xy);
            }
        }

        HBITMAP hbm;
        toolbarBitmap.GetHBITMAP(0, &hbm);

        ImageList_Add(il, hbm, nullptr);

        auto toolBarStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | CCS_NODIVIDER | CCS_NORESIZE | CCS_NOPARENTALIGN | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT;

        int nav_id = 1000;
        int more_id = 1000;

        _navigate1 = CreateToolbarEx(m_hWnd, toolBarStyle, nav_id, 0, nullptr, 0, tbButtonsNav1, 4, xy, xy, xy, xy, sizeof(TBBUTTON));
        _more = CreateToolbarEx(m_hWnd, toolBarStyle, more_id, 0, nullptr, 0, tbMore, 1, xy, xy, xy, xy, sizeof(TBBUTTON));

        ::SendMessage(_navigate1, TB_SETIMAGELIST, 0, (LPARAM) il);
        ::SendMessage(_more, TB_SETIMAGELIST, 0, (LPARAM) il);

        auto font = CreateFont(-18, 0, 0, 0, 400, FALSE, FALSE, FALSE, 0, 400, 2, 1, 1, L"Calibri");




        /* _more.Create(m_hWnd, nullptr, nullptr, toolBarStyle, 0, IDC_MORE);
        _more.SetFont(font);
        _more.SetButtonStructSize();
        _more.SetImageList(hLarge);
        _more.AddButtons(countof(tbMore), tbMore);
        _more.AutoSize();*/

        _address.Create(m_hWnd, nullptr, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, IDC_ADDRESS);
        _address.SetFont(font);

        InitMenu();
        Layout();

        return 0;
    }

    BEGIN_MSG_MAP(Toolbar)

        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_COMMAND, OnCommand)
        MESSAGE_HANDLER(WM_INITMENUPOPUP, OnInitMenuPopup)
        //MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
        //MESSAGE_HANDLER(WM_MEASUREITEM, OnMeasureItem)

        //NOTIFY_CODE_HANDLER(NM_CUSTOMDRAW, OnCustomDraw)
        NOTIFY_CODE_HANDLER(TBN_DROPDOWN, OnToolbarDropDown)
        NOTIFY_CODE_HANDLER(TTN_GETDISPINFO, OnToolTipText)

        COMMAND_HANDLER(IDC_ADDRESS, EN_SETFOCUS, OnEditSetFocus)
        COMMAND_HANDLER(IDC_ADDRESS, EN_KILLFOCUS, OnEditKillFocus)
        COMMAND_HANDLER(IDC_ADDRESS, EN_CHANGE, OnEditChange)

        //CHAIN_MSG_MAP(Dialog::ColorWindow)

    END_MSG_MAP()

    bool OnChar(int c)
    {
        switch (c)
        {
        case VK_UP:
            _popup.StepSelection(-1);
            return true;
        case VK_DOWN:
            _popup.StepSelection(1);
            return true;
        case VK_RETURN:
        {
            auto selected = _popup.Selected();
            auto text = selected ? selected->Text().Text(true) : WindowText(_address);
            _view.open(text);            
        }
        return true;
        case VK_ESCAPE:
            _popup.Cancel();
            UpdateAddress();
            return true;
        }

        return false;
    }

    void UpdateAddress()
    {
        if (!_addressHasFocus)
        {
            auto text = _view.url();
            _address.SetWindowText(text.c_str());
            //_address.SetIcon(icon);
        }
    }

    bool PreTranslateMessage(MSG* m)
    {
        if (_addressHasFocus)
        {
            if (_popup)
            {
                switch (m->message)
                {
                case WM_MOUSEWHEEL:
                    if (_popup.CanScroll())
                    {
                        _popup.ScrollDelta(((short) HIWORD(m->wParam)) / 2);
                        return true;
                    }
                    break;
                case WM_KEYDOWN:
                    if (OnChar(m->wParam)) return true;
                    break;
                }
            }

            if (IsDialogMessage(m))
                return true;
        }

        return false;
    }

    LRESULT OnEditSetFocus(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& bHandled)
    {
        if (!_popup)
        {
            RectI r;
            _address.GetWindowRect(r);
            r.top += r.Height();
            r.bottom = 320;

            _popup.Create(_address, r, 0, WS_POPUP | WS_BORDER, WS_EX_NOACTIVATE);
            _popup.Selected(nullptr);
        }

        _completes->Init(_popup);
        _popup.Search(L"");
        _address.PostMessage(EM_SETSEL, 0, -1);
        _address.SetPopup(_popup);
        _addressHasFocus = true;

        return 0;
    }

    LRESULT OnEditKillFocus(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& bHandled)
    {
        if (_popup)
        {
            _popup.ShowWindow(SW_HIDE);
            _popup.DestroyWindow();
            _popup.m_hWnd = nullptr;
        }

        _address.SetPopup(nullptr);
        _addressHasFocus = false;
        return 0;
    }

    LRESULT OnEditChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& bHandled)
    {
        if (_popup)
        {
            _popup.Search(WindowText(_address));
        }

        return 0;
    }

    LRESULT OnInitMenuPopup(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        if (_enablePopupItems)
        {
            auto hMenu = (HMENU) wParam;

            if (hMenu)
            {
                EnableMenu(hMenu);
            }
        }

        return 0;
    }



    void GetSystemSettings()
    {
        // refresh our font
        /* NONCLIENTMETRICS info = { sizeof(NONCLIENTMETRICS), 0 };
        auto result = ::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(info), &info, 0);
        assert(result);

        if (result)
        {
        auto h = ::CreateFontIndirect(&info.lfMenuFont);
        assert(h);

        if (h)
        {
        if (_fontMenu.m_hFont)
        _fontMenu.DeleteObject();
        _fontMenu.Attach(h);
        SetFont(_fontMenu);
        }
        }*/
    }

    LRESULT OnToolTipText(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/)
    {
        LPNMTTDISPINFOW pDispInfo = (LPNMTTDISPINFOW) pnmh;
        pDispInfo->szText[0] = 0;
        pDispInfo->lpszText = 0;
        pDispInfo->hinst = nullptr;

        if (idCtrl != 0)
        {
            try
            {
                _tooltip = GetToolTipText(idCtrl);

                if (!_tooltip.empty())
                {
                    pDispInfo->lpszText = (LPWSTR) _tooltip.c_str();
                }
            }
            catch (std::exception &e)
            {
                //Core::LogException(__FUNCSIG__, e);
            }
        }

        return 0;
    }

    static void AddMenuEntry(HMENU m, int id, LPCWSTR sz, bool current)
    {
        MENUITEMINFO mii = { sizeof(mii), 0 };
        mii.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_STRING | MIIM_ID;
        mii.dwTypeData = (LPWSTR) sz;
        mii.fState = MFS_ENABLED | (current ? MFS_CHECKED : 0);
        mii.fType = MFT_STRING;
        mii.wID = id;

        int n = ::GetMenuItemCount(m);
        ::InsertMenuItem(m, n, TRUE, &mii);
    }

    static void AddMenuItem(HMENU popup, std::wstring name, int &idItem)
    {
        MENUITEMINFO mii = { sizeof(mii), 0 };
        mii.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_STRING | MIIM_ID;
        mii.dwTypeData = (LPWSTR) name.c_str();
        mii.fState = MFS_ENABLED;
        mii.fType = MFT_STRING;
        mii.wID = idItem++;

        int n = ::GetMenuItemCount(popup);
        ::InsertMenuItem(popup, n, TRUE, &mii);
    }

    LRESULT OnToolbarDropDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
    {
        NMTOOLBAR* ptb = (NMTOOLBAR *) pnmh;
        RectI rc;

        auto tbar = pnmh->hwndFrom;
        auto idx = (UINT)::SendMessage(tbar, TB_COMMANDTOINDEX, ptb->iItem, 0L);
        ::SendMessage(tbar, TB_GETITEMRECT, idx, (LPARAM) (LPRECT) rc);
        ::MapWindowPoints(tbar, HWND_DESKTOP, (LPPOINT) (LPRECT) rc, 2);

        auto id = ptb->iItem;

        if (id == ID_VIEW_MENU)
        {
            auto m = ::LoadMenu(g_hInstance, MAKEINTRESOURCE(IDC_POTATO));
            auto popup = GetSubMenu(m, 0);

            auto result = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_RIGHTBUTTON, true, rc.right, rc.bottom, rc);
            InvokeCommand(result);
            Layout();
        }
        else
        {
            bHandled = false;
        }

        return 0;
    }

    LRESULT OnSize(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        Layout();
        return 0;
    }

    LRESULT OnCommand(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        bHandled = InvokeCommand(LOWORD(wParam));
        return 0;
    }

    /*LRESULT OnCustomDraw(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
    {
    auto pCustomDraw = (LPNMCUSTOMDRAW) pnmh;
    auto from = pCustomDraw->hdr.hwndFrom;

    if (_navigate1 == from || _navigate2 == from || _tools == from || _sorting == from || _more == from)
    {
    if (pCustomDraw->dwDrawStage == CDDS_PREPAINT)
    {
    return CDRF_NOTIFYITEMDRAW;
    }
    else if (pCustomDraw->dwDrawStage == CDDS_ITEMPREPAINT)
    {
    Render::DrawButton((LPNMTBCUSTOMDRAW) pnmh);
    return CDRF_SKIPDEFAULT;
    }
    }

    bHandled = false;
    return CDRF_DODEFAULT;
    }

    void Render(CDCHandle dc)
    {
    RectI r;
    GetClientRect(r);
    dc.FillRect(r, Render::Style::Brush(Render::Style::Color::TaskBackground));
    }
    */

    void Address(const std::wstring &url)
    {
        _address.SetWindowText(url.c_str());
        //_address.SetIcon(icon);
    }

    void Layout()
    {
        if (_navigate1)
        {
            RectI rectClient;
            GetClientRect(rectClient);

            auto padding = 2;
            auto sizeNav = MeasureToolbar(_navigate1);
            auto sizeMor = MeasureToolbar(_more);
            auto height = std::max(sizeNav.cy, sizeMor.cy);

            auto left = sizeNav.cx + padding + padding;
            auto right = rectClient.Width() - (sizeMor.cx + 8);

            RectI rNav1(padding, padding, left - padding, height + padding);
            RectI rAddr(left, padding, right, height + padding);
            RectI rMore(right + padding, padding, rectClient.Width() - padding, height + padding);

            ::SetWindowPos(_navigate1, nullptr, rNav1.left, rNav1.top, rNav1.Width(), rNav1.Height(), SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            ::SetWindowPos(_address, nullptr, rAddr.left, rAddr.top, rAddr.Width(), rAddr.Height(), SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            ::SetWindowPos(_more, nullptr, rMore.left, rMore.top, rMore.Width(), rMore.Height(), SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

            if (_popup && _popup.IsWindow() && _popup.IsWindowVisible())
            {
                RectI popupRect;
                _popup.GetWindowRect(popupRect);
                ClientToScreen(rAddr);
                _popup.SetWindowPos(nullptr, rAddr.left, rAddr.bottom, rAddr.Width(), rAddr.Height(), SWP_NOACTIVATE);
            }
        }
    }

    inline void AddCommand(DWORD id, const CommandPtr &c)
    {
        _commands[id] = c;
    }

    CommandPtr FindCommand(DWORD id)
    {
        /*auto c = _view->FindCommand(id);

        if (c == nullptr)*/
        {
            auto it = _commands.find(id);

            if (it != _commands.end())
            {
                return it->second;
            }
        }

        return nullptr;
    }

    /*void View(ViewBase *v)
    {
    if (_view != v)
    {
    _view = v;
    Layout();
    }
    }*/

    void FocusAddress()
    {
        _address.SetFocus();
    }

    std::wstring GetToolTipText(int id)
    {
        /*switch (id)
        {
        case ID_HELP_NEWVERSION:
        return String::Utf8ToUtf16(String::Format("Version %s of Diffractor is availiable.\nYou are currently using version %s.\nDownload from Diffractor.com", Options.AvailableVersion.c_str(), g_szAppVersion));
        case ID_BROWSE_BACK:
        return String::Utf8ToUtf16(_state.History.CanBrowseBack() ? String::Format("Show %s", _state.History.BackPath().first.ToString().c_str()) : "Back");
        case ID_BROWSE_FORWARD:
        return String::Utf8ToUtf16(_state.History.CanBrowseForward() ? String::Format("Show %s", _state.History.ForwardPath().first.ToString().c_str()) : "Forward");
        case ID_BROWSE_LAST:
        return String::Utf8ToUtf16(String::Format("Show %s", _state.NextPath(false).c_str()));
        case ID_BROWSE_NEXT:
        return String::Utf8ToUtf16(String::Format("Show %s", _state.NextPath(true).c_str()));
        case ID_VIEW_SORTITEMS:
        return L"Sort by";
        case ID_VIEW_MENU:
        return L"More...";
        }

        auto text = _idToText.find(id);

        if (text != _idToText.end())
        {
        auto s = text->second;
        for (auto i = 0u; i < s.size(); i++) if (s[i] == '\t') s.replace(i, 1, L" - ");
        return s;
        }*/

        return L"";
    }

    void InitMenuText(HMENU hMenu, MAPSHORTCUTS &mapShortcuts)
    {
        const int len = 128;
        wchar_t sz[len + 1]; // buffer for menu-item text 

        auto cItems = GetMenuItemCount(hMenu);

        for (auto i = 0; i < cItems; i++)
        {
            MENUITEMINFO mii;
            mii.cbSize = sizeof(MENUITEMINFO);
            mii.fMask = MIIM_SUBMENU | MIIM_ID | MIIM_TYPE;

            wchar_t szTypeData[len];
            szTypeData[0] = 0;
            mii.dwTypeData = szTypeData;
            mii.cch = len;

            if (GetMenuItemInfo(hMenu, i, TRUE, &mii))
            {
                ConstructMenuString(sz, len, szTypeData, _mapAccel[mii.wID], mapShortcuts);
                //_idToText[mii.wID] = sz;

                if (mii.hSubMenu)
                {
                    InitMenuText(mii.hSubMenu, mapShortcuts);
                }
            }
        }
    }

    void InitOwnerDrawMenu(HMENU hMenu, MAPSHORTCUTS &mapShortcuts)
    {
        const int len = 128;
        wchar_t sz[len + 1]; // buffer for menu-item text 

        auto cItems = GetMenuItemCount(hMenu);

        for (auto i = 0; i < cItems; i++)
        {
            MENUITEMINFO mii;
            mii.cbSize = sizeof(MENUITEMINFO);
            mii.fMask = MIIM_SUBMENU | MIIM_ID | MIIM_TYPE;

            wchar_t szTypeData[len];
            szTypeData[0] = 0;
            mii.dwTypeData = szTypeData;
            mii.cch = len;

            if (GetMenuItemInfo(hMenu, i, TRUE, &mii))
            {
                mii.fMask = MIIM_TYPE;
                mii.fType = MFT_OWNERDRAW;
                SetMenuItemInfo(hMenu, i, TRUE, &mii);

                ConstructMenuString(sz, len, szTypeData, _mapAccel[mii.wID], mapShortcuts);
                //_idToText[mii.wID] = sz;

                if (mii.hSubMenu)
                {
                    InitOwnerDrawMenu(mii.hSubMenu, mapShortcuts);
                }
            }
        }
    }

    //static LRESULT CALLBACK MyCreateHookProc(int nCode, WPARAM wParam, LPARAM lParam)
    //{
    // LRESULT lRet = 0;
    // wchar_t szClassName[7];

    // auto current = _current;

    // if (nCode == HCBT_CREATEWND)
    // {
    //  HWND hWndMenu = (HWND) wParam;
    //  ::GetClassName(hWndMenu, szClassName, 7);
    //  if (::lstrcmp(L"#32768", szClassName) == 0)
    //  {
    //   _current->_menuStack.push_back(hWndMenu);

    //   // Subclass to a flat-looking menu
    //   auto wnd = new DiffractorMenu(current->m_rcButton.Width());
    //   wnd->SubclassWindow(hWndMenu);
    //   wnd->SetWindowPos(HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_DRAWFRAME);
    //   current->m_rcButton.Clear();
    //  }
    // }
    // else if (nCode == HCBT_DESTROYWND)
    // {
    //  auto hWndMenu = (HWND) wParam;
    //  ::GetClassName(hWndMenu, szClassName, 7);
    //  if (::lstrcmp(L"#32768", szClassName) == 0)
    //  {
    //   ATLASSERT(hWndMenu == _current->_menuStack.back());
    //   _current->_menuStack.pop_back();
    //  }
    // }
    // else if (nCode < 0)
    // {
    //  lRet = ::CallNextHookEx(current->_hMenuHook, nCode, wParam, lParam);
    // }
    // return lRet;
    //}

    int TrackPopupMenu(HMENU hMenu, UINT flags, bool enableItems, int x, int y, const RectI &r)
    {
        /*Core::ScopeLockedCount lockSetting(_showingMenu);
        _enablePopupItems = enableItems;

        MAPSHORTCUTS mapShortcuts;
        InitOwnerDrawMenu(hMenu, mapShortcuts);

        _current = this;
        _hMenuHook = ::SetWindowsHookEx(WH_CBT, MyCreateHookProc, GetModuleInstance(), GetCurrentThreadId());*/
        auto result = ::TrackPopupMenu(hMenu, flags, x, y, 0, m_hWnd, r);
        /*::UnhookWindowsHookEx(_hMenuHook);
        _hMenuHook = nullptr;
        _enablePopupItems = true;*/

        return result;
    }
};



