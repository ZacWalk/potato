// ui.h - Win32 window framework (base_window CRTP, subclassed_window), task
// queue for UI thread marshaling, web_history for back/forward navigation,
// html_view rendering host, toolbar with address bar, and autocomplete UI.

#pragma once
#include "document.h"
#include "resource.h"


extern HINSTANCE g_hInstance;

// Simple RAII wrapper for Win32 events (replaces ATL CEvent)
class sys_event
{
	HANDLE m_handle;

public:
	sys_event(const bool manualReset = false, const bool initialState = false)
		: m_handle(CreateEventW(nullptr, manualReset ? TRUE : FALSE, initialState ? TRUE : FALSE, nullptr))
	{
	}

	~sys_event()
	{
		if (m_handle) CloseHandle(m_handle);
	}

	sys_event(const sys_event&) = delete;
	sys_event& operator=(const sys_event&) = delete;

	void set() { SetEvent(m_handle); }
	void reset() { ResetEvent(m_handle); }
	operator HANDLE() const { return m_handle; }
};

// CRTP base class for Win32 windows
template <typename Derived>
class base_window
{
public:
	HWND m_hWnd = nullptr;

	operator HWND() const { return m_hWnd; }
	explicit operator bool() const { return m_hWnd != nullptr; }

	virtual ~base_window() = default;

	static bool register_wnd_class(
		const LPCWSTR className,
		const UINT style = CS_HREDRAW | CS_VREDRAW,
		const int bgColor = COLOR_WINDOW + 1,
		const HCURSOR cursor = LoadCursor(nullptr, IDC_ARROW),
		const LPCWSTR menuName = nullptr)
	{
		WNDCLASSEXW wc = {sizeof(wc)};
		if (GetClassInfoExW(g_hInstance, className, &wc)) return true;
		wc.cbSize = sizeof(wc);
		wc.style = style;
		wc.lpfnWndProc = static_wnd_proc;
		wc.hInstance = g_hInstance;
		wc.hCursor = cursor;
		wc.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(bgColor));
		wc.lpszClassName = className;
		wc.lpszMenuName = menuName;
		return RegisterClassExW(&wc) != 0;
	}

	HWND create(HWND parent, const RECT* rect = nullptr, LPCWSTR title = nullptr,
	            DWORD style = WS_OVERLAPPEDWINDOW, DWORD exStyle = 0)
	{
		auto* derived = static_cast<Derived*>(this);
		derived->ensure_class_registered();

		int x = CW_USEDEFAULT, y = CW_USEDEFAULT, w = CW_USEDEFAULT, h = CW_USEDEFAULT;
		if (rect)
		{
			x = rect->left;
			y = rect->top;
			w = rect->right - rect->left;
			h = rect->bottom - rect->top;
		}

		m_hWnd = CreateWindowExW(exStyle, Derived::class_name(), title, style,
		                         x, y, w, h, parent, nullptr, g_hInstance, this);
		return m_hWnd;
	}

	// Common Win32 forwarding methods
	BOOL destroy_window() { return DestroyWindow(m_hWnd); }
	BOOL show_window(const int cmd) { return ShowWindow(m_hWnd, cmd); }
	BOOL update_window() { return UpdateWindow(m_hWnd); }
	BOOL invalidate_rect(const RECT* r = nullptr, const BOOL erase = TRUE) { return InvalidateRect(m_hWnd, r, erase); }
	void invalidate(const BOOL erase = TRUE) { InvalidateRect(m_hWnd, nullptr, erase); }
	BOOL get_client_rect(const LPRECT r) { return GetClientRect(m_hWnd, r); }

	BOOL set_window_pos(const HWND after, const int x, const int y, const int cx, const int cy, const UINT flags)
	{
		return SetWindowPos(m_hWnd, after, x, y, cx, cy, flags);
	}

	BOOL set_window_pos(const HWND after, const RECT& r, const UINT flags)
	{
		return SetWindowPos(m_hWnd, after, r.left, r.top, r.right - r.left, r.bottom - r.top, flags);
	}

	HWND set_focus() { return SetFocus(m_hWnd); }
	BOOL is_window() const { return m_hWnd && IsWindow(m_hWnd); }

	LRESULT send_message(const UINT msg, const WPARAM wp = 0, const LPARAM lp = 0)
	{
		return ::SendMessage(m_hWnd, msg, wp, lp);
	}

	BOOL post_message(const UINT msg, const WPARAM wp = 0, const LPARAM lp = 0)
	{
		return ::PostMessage(m_hWnd, msg, wp, lp);
	}

	HWND get_parent() { return GetParent(m_hWnd); }
	LRESULT def_window_proc() { return ::DefWindowProc(m_hWnd, m_currentMsg, m_currentWP, m_currentLP); }

	LRESULT def_window_proc(const UINT msg, const WPARAM wp, const LPARAM lp)
	{
		return ::DefWindowProc(m_hWnd, msg, wp, lp);
	}

	HDC begin_paint(PAINTSTRUCT* ps) { return BeginPaint(m_hWnd, ps); }
	BOOL end_paint(const PAINTSTRUCT* ps) { return EndPaint(m_hWnd, ps); }
	BOOL show_scroll_bar(const int bar, const BOOL show) { return ShowScrollBar(m_hWnd, bar, show); }

	int set_scroll_info(const int bar, const SCROLLINFO* si, const BOOL redraw)
	{
		return SetScrollInfo(m_hWnd, bar, si, redraw);
	}

	HICON set_icon(HICON icon, const BOOL big)
	{
		return reinterpret_cast<HICON>(::SendMessage(m_hWnd, WM_SETICON, big ? ICON_BIG : ICON_SMALL,
		                                             reinterpret_cast<LPARAM>(icon)));
	}

	BOOL set_window_text(const LPCWSTR text) { return ::SetWindowText(m_hWnd, text); }
	DWORD get_style() const { return static_cast<DWORD>(GetWindowLongPtr(m_hWnd, GWL_STYLE)); }
	HFONT get_font() const { return reinterpret_cast<HFONT>(::SendMessage(m_hWnd, WM_GETFONT, 0, 0)); }

	void set_font(HFONT font, const BOOL redraw = TRUE)
	{
		::SendMessage(m_hWnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), MAKELPARAM(redraw, 0));
	}

	BOOL is_dialog_message(MSG* msg) { return ::IsDialogMessage(m_hWnd, msg); }
	HWND set_capture() { return SetCapture(m_hWnd); }
	BOOL get_window_rect(const LPRECT r) { return GetWindowRect(m_hWnd, r); }
	BOOL is_window_enabled() { return IsWindowEnabled(m_hWnd); }

protected:
	// Current message state for DefWindowProc
	UINT m_currentMsg = 0;
	WPARAM m_currentWP = 0;
	LPARAM m_currentLP = 0;

	// Override in derived class to handle messages
	virtual LRESULT handle_message(const UINT msg, const WPARAM wParam, const LPARAM lParam)
	{
		return ::DefWindowProc(m_hWnd, msg, wParam, lParam);
	}

private:
	static LRESULT CALLBACK static_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
	{
		base_window* self = nullptr;

		if (msg == WM_NCCREATE)
		{
			const auto cs = reinterpret_cast<CREATESTRUCT*>(lp);
			self = static_cast<base_window*>(cs->lpCreateParams);
			self->m_hWnd = hwnd;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		}
		else
		{
			self = reinterpret_cast<base_window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		}

		if (self)
		{
			self->m_currentMsg = msg;
			self->m_currentWP = wp;
			self->m_currentLP = lp;
			return self->handle_message(msg, wp, lp);
		}

		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
};

// Subclass wrapper for existing window classes (e.g., EDIT controls)
class subclassed_window
{
public:
	HWND m_hWnd = nullptr;

	operator HWND() const { return m_hWnd; }
	explicit operator bool() const { return m_hWnd != nullptr; }

	virtual ~subclassed_window()
	{
		if (m_hWnd && m_origProc)
		{
			SetWindowLongPtr(m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_origProc));
		}
	}

	HWND create(const LPCWSTR className, const HWND parent, const RECT* rect = nullptr, const LPCWSTR title = L"",
	            const DWORD style = WS_CHILD | WS_VISIBLE, const DWORD exStyle = 0, const int id = 0)
	{
		int x = 0, y = 0, w = 100, h = 24;
		if (rect)
		{
			x = rect->left;
			y = rect->top;
			w = rect->right - rect->left;
			h = rect->bottom - rect->top;
		}

		m_hWnd = CreateWindowExW(exStyle, className, title, style,
		                         x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_hInstance,
		                         nullptr);

		if (m_hWnd)
		{
			SetWindowLongPtr(m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
			m_origProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(m_hWnd, GWLP_WNDPROC,
			                                                        reinterpret_cast<LONG_PTR>(static_subclass_proc)));
		}

		return m_hWnd;
	}

	// Common Win32 forwarding methods
	LRESULT send_message(const UINT msg, const WPARAM wp = 0, const LPARAM lp = 0)
	{
		return ::SendMessage(m_hWnd, msg, wp, lp);
	}

	BOOL post_message(const UINT msg, const WPARAM wp = 0, const LPARAM lp = 0)
	{
		return ::PostMessage(m_hWnd, msg, wp, lp);
	}

	BOOL set_window_pos(const HWND after, const int x, const int y, const int cx, const int cy, const UINT flags)
	{
		return SetWindowPos(m_hWnd, after, x, y, cx, cy, flags);
	}

	BOOL set_window_text(const LPCWSTR text) { return ::SetWindowText(m_hWnd, text); }
	HFONT get_font() const { return reinterpret_cast<HFONT>(::SendMessage(m_hWnd, WM_GETFONT, 0, 0)); }

	void set_font(HFONT font, const BOOL redraw = TRUE)
	{
		::SendMessage(m_hWnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), MAKELPARAM(redraw, 0));
	}

	DWORD get_style() const { return static_cast<DWORD>(GetWindowLongPtr(m_hWnd, GWL_STYLE)); }
	BOOL is_window_enabled() { return IsWindowEnabled(m_hWnd); }
	BOOL get_window_rect(const LPRECT r) { return GetWindowRect(m_hWnd, r); }
	void set_popup(const HWND popup) { m_popup = popup; }
	HWND set_focus() { return SetFocus(m_hWnd); }

protected:
	WNDPROC m_origProc = nullptr;
	HWND m_popup = nullptr;

	virtual LRESULT handle_message(const UINT msg, const WPARAM wParam, const LPARAM lParam)
	{
		return CallWindowProc(m_origProc, m_hWnd, msg, wParam, lParam);
	}

	LRESULT call_default(const UINT msg, const WPARAM wp, const LPARAM lp)
	{
		return CallWindowProc(m_origProc, m_hWnd, msg, wp, lp);
	}

private:
	static LRESULT CALLBACK static_subclass_proc(const HWND hwnd, const UINT msg, const WPARAM wp, const LPARAM lp)
	{
		const auto self = reinterpret_cast<subclassed_window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		if (self) return self->handle_message(msg, wp, lp);
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
};


class tasks
{
	sys_event m_event;
	std::mutex m_mutex;

	using Task = std::function<void()>;
	std::deque<Task> m_q;
	Task m_single;

public:
	~tasks()
	{
		std::lock_guard lock(m_mutex);
		m_q.clear();
	}

	bool pop(Task& result)
	{
		std::lock_guard lock(m_mutex);

		if (m_single)
		{
			result = std::move(m_single);
			m_single = nullptr;
			return true;
		}

		if (m_q.empty()) return false;
		result = std::move(m_q.front());
		m_q.pop_front();
		return true;
	}

	void single(const Task& f)
	{
		std::lock_guard lock(m_mutex);
		m_single = f;
		m_event.set();
	}

	void operator()(const Task& f)
	{
		std::lock_guard lock(m_mutex);
		m_q.push_back(f);
		m_event.set();
	}

	void run()
	{
		Task task;

		while (pop(task))
		{
			try
			{
				task();
			}
			catch (const std::exception& e)
			{
				OutputDebugStringA("run_on_ui task exception: ");
				OutputDebugStringA(e.what());
				OutputDebugStringA("\n");
			}

			task = nullptr;
		}
	}

	operator HANDLE() const
	{
		return m_event;
	}
};

extern tasks run_on_ui;
extern tasks run_async;

extern sys_event event_exit;


namespace color
{
	constexpr unsigned highlight = 0x00CC6611;
	constexpr unsigned hover = 0x00999999;
	constexpr unsigned text = 0x00ffffff;
	constexpr unsigned task_background = 0x00777777;
}

inline COLORREF make_rgba(const unsigned r, const unsigned g, const unsigned b, const unsigned a = 255)
{
	return r | g << 8 | b << 16 | a << 24;
};

inline unsigned byte_clamp(const int n)
{
	return n > 255 ? 255u : n < 0 ? 0u : static_cast<unsigned>(n);
}

inline COLORREF saturate_rgba(const int r, const int g, const int b, const int a)
{
	return byte_clamp(r) | byte_clamp(g) << 8 | byte_clamp(b) << 16 | byte_clamp(a) << 24;
};

inline unsigned get_a(const COLORREF c) { return 0xffu & c >> 24; };
inline unsigned get_r(const COLORREF c) { return 0xffu & c; };
inline unsigned get_g(const COLORREF c) { return 0xffu & c >> 8; };
inline unsigned get_b(const COLORREF c) { return 0xffu & c >> 16; };

inline COLORREF lighten(const COLORREF c, const int n = 32)
{
	return saturate_rgba(get_r(c) + n, get_g(c) + n, get_b(c) + n, get_a(c));
}

inline COLORREF darken(const COLORREF c, const int n = 32)
{
	return lighten(c, -n);
}

inline COLORREF emphasize(const COLORREF c, const int n = 48)
{
	const bool isLight = get_b(c) > 0x80 || get_g(c) > 0x80 || get_r(c) > 0x80;

	return lighten(c, isLight ? -n : n);
}

inline size_i measure_toolbar(const HWND tb)
{
	size_i result(0, 0);
	const auto count = static_cast<int>(::SendMessage(tb, TB_BUTTONCOUNT, 0, 0L));

	for (int i = 0; i < count; ++i)
	{
		recti r;
		if (static_cast<BOOL>(::SendMessage(tb, TB_GETITEMRECT, i, (LPARAM)static_cast<LPRECT>(r))))
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
	HDC m_hdc;
	HWND m_hwnd;

	win_dc(const HWND hwnd) : m_hdc(GetDC(hwnd)), m_hwnd(hwnd)
	{
	}

	~win_dc()
	{
		ReleaseDC(m_hwnd, m_hdc);
	}

	operator HDC() const
	{
		return m_hdc;
	}

	HFONT set_font(const HFONT hFont)
	{
		return static_cast<HFONT>(SelectObject(m_hdc, hFont));
	}
};

inline std::string window_text(const HWND h)
{
	const auto len = ::GetWindowTextLengthW(h);
	const auto result = static_cast<wchar_t*>(alloca((len + 1) * sizeof(wchar_t)));
	GetWindowTextW(h, result, len + 1);
	return to_utf8(result);
}

inline void fill_solid_rect(const HDC hDC, const int x, const int y, const int cx, const int cy, const COLORREF clr)
{
	const RECT rect = {x, y, x + cx, y + cy};
	const COLORREF crOldBkColor = SetBkColor(hDC, clr);
	::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rect, nullptr, 0, nullptr);
	SetBkColor(hDC, crOldBkColor);
}


inline void fill_solid_rect(const HDC hDC, const RECT* pRC, const COLORREF crColor)
{
	fill_solid_rect(hDC, pRC->left, pRC->top, pRC->right - pRC->left, pRC->bottom - pRC->top, crColor);
}


class web_history final
{
	std::vector<std::string> m_items;
	std::vector<std::string>::size_type m_current_item;

public:
	web_history();
	virtual ~web_history();

	void url_opened(const std::string& url);
	bool back(std::string& url);
	bool forward(std::string& url);
};


class toolbar;

class html_view final : public base_window<html_view>
{
	toolbar& m_toolbar;

	int m_top = 0;
	int m_left = 0;
	int m_max_top = 0;
	int m_max_left = 0;
	bool m_in_layout = false;
	web_history m_history;
	std::shared_ptr<document> m_page;
	http m_http;
	std::vector<std::string> m_trace_lines;
	std::string m_loading_url;

public:
	html_view(toolbar& tb);
	~html_view() override;

	static constexpr LPCWSTR class_name() { return L"potato_html_view"; }
	void ensure_class_registered() { register_wnd_class(class_name()); }

	void open(const std::string& url);
	void open_file(const std::string& url, const std::string& filename);
	void trace(const char* format, ...);
	void trace_error(const std::string& url, DWORD error, DWORD httpStatus);
	void open_text(const std::string& url, const std::string& text);
	void refresh();
	void back();
	void forward();
	void set_caption();

	void layout();

	std::string url() const { return m_page ? m_page->url() : std::string{}; }

	position client_pos();
	void update_history();

protected:
	void on_paint(HDC hdc, LPRECT rcDraw);
	void on_size(int width, int height);
	void on_v_scroll(int pos, int flags);
	void on_h_scroll(int pos, int flags);
	void on_mouse_wheel(int delta);
	void on_key_down(UINT vKey);
	void on_mouse_move(int x, int y);
	void on_l_button_down(int x, int y);
	void on_l_button_up(int x, int y);
	void on_mouse_leave();

	void update_scroll();
	void update_cursor();
	void scroll_to(int new_left, int new_top);
	void invalidate_boxes(const position::vector& boxes);

	LRESULT handle_message(const UINT msg, const WPARAM wParam, const LPARAM lParam) override
	{
		switch (msg)
		{
		case WM_SETCURSOR:
			update_cursor();
			return 0;
		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				const HDC hdc = begin_paint(&ps);
				on_paint(hdc, &ps.rcPaint);
				end_paint(&ps);
			}
			return 0;
		case WM_SIZE:
			on_size(LOWORD(lParam), HIWORD(lParam));
			return 0;
		case WM_VSCROLL:
			on_v_scroll(HIWORD(wParam), LOWORD(wParam));
			return 0;
		case WM_HSCROLL:
			on_h_scroll(HIWORD(wParam), LOWORD(wParam));
			return 0;
		case WM_MOUSEWHEEL:
			on_mouse_wheel(GET_WHEEL_DELTA_WPARAM(wParam));
			return 0;
		case WM_KEYDOWN:
			on_key_down(static_cast<UINT>(wParam));
			return 0;
		case WM_MOUSEMOVE:
			{
				TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, m_hWnd, 0};
				TrackMouseEvent(&tme);
				on_mouse_move(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			}
			return 0;
		case WM_MOUSELEAVE:
			on_mouse_leave();
			return 0;
		case WM_LBUTTONDOWN:
			on_l_button_down(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			return 0;
		case WM_LBUTTONUP:
			on_l_button_up(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			return 0;
		}
		return def_window_proc(msg, wParam, lParam);
	}
};


extern HINSTANCE g_hInstance;

struct i_command
{
	virtual ~i_command() = default;
	virtual void invoke() = 0;
	virtual bool is_enabled() const { return true; }
	virtual bool is_checked() const { return false; }
};

using command_ptr = std::shared_ptr<i_command>;


class auto_complete_result
{
	text_match m_match;
	int m_weight = 0;

public:
	recti bounds;

	auto_complete_result() = default;

	auto_complete_result(const text_match& t, const int weight) : m_match(t), m_weight(weight)
	{
	}

	const text_match& text() const { return m_match; }
	int weight() const { return m_weight; }

	static bool compare_weight(const auto_complete_result& l, const auto_complete_result& r)
	{
		const auto diff = r.m_weight - l.m_weight;
		return diff == 0 ? l.text() < r.text() : diff < 0;
	}
};

using auto_complete_results = std::vector<auto_complete_result>;

static constexpr int WM_SEARCHING_COMPLETE = WM_USER + 101;

template <class complete_strategy, class TEdit>
class list_wnd final : public base_window<list_wnd<complete_strategy, TEdit>>
{
	using Base = base_window<list_wnd<complete_strategy, TEdit>>;

public:
	using Base::m_hWnd;

	static constexpr LPCWSTR class_name() { return L"potato_list"; }

	void ensure_class_registered()
	{
		Base::register_wnd_class(class_name(), CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS);
	}

private:
	auto_complete_results m_results;
	std::shared_ptr<complete_strategy> m_completes;
	TEdit& m_edit;

	auto_complete_result* m_selected_item = nullptr;
	auto_complete_result* m_hover_item = nullptr;

	std::atomic<long> m_pin{0};

	size_i m_extent;
	point_i m_offset{0, 0};
	int m_y_max = 0;
	int m_y_tracking_start = 0;
	int m_y_tracking_offset_start = 0;
	int m_height;
	bool m_highlight_scroll = false;
	bool m_hover = false;
	bool m_tracking = false;
	bool m_tracking_scroll = false;
	COLORREF m_background;

public:
	static constexpr int row_height = 26;

	list_wnd(TEdit& edit, const std::shared_ptr<complete_strategy>& c, const int height = 8 + row_height * 20,
	         const COLORREF bg = 0x888888)
		: m_completes(c), m_edit(edit), m_height(height), m_background(bg)
	{
	}

	int height() const { return m_height; }

	void layout()
	{
		win_dc dc(m_hWnd);
		auto hOldFont = dc.set_font(m_edit.get_font());
		auto y = 4;

		for (auto& i : m_results)
		{
			i.bounds = recti(0, y, m_extent.cx, y + row_height);
			y = i.bounds.bottom;
		}

		m_y_max = y + 4;
		dc.set_font(hOldFont);
		this->invalidate(FALSE);
	}

	void on_paint(const HDC hdc)
	{
		recti r;
		this->get_client_rect(&r);
		fill_solid_rect(hdc, &r, m_background);

		const auto hOldFont = static_cast<HFONT>(::SelectObject(hdc, m_edit.get_font()));
		SetBkMode(hdc, TRANSPARENT);

		for (const auto& i : m_results)
		{
			auto bounds = i.bounds.offset(-m_offset);

			if (bounds.intersects(r))
			{
				if (&i == m_hover_item)
					fill_solid_rect(hdc, &bounds, color::hover);
				if (&i == m_selected_item)
					fill_solid_rect(hdc, &bounds, color::highlight);

				SetTextColor(hdc, color::text);
				bounds = bounds.inflate(-8, -4);
				i.text().draw(hdc, bounds);
			}
		}

		draw_scroll(hdc, m_highlight_scroll, m_tracking_scroll);
		SelectObject(hdc, hOldFont);
	}

	bool can_scroll() const { return m_y_max > m_extent.cy; }

	static COLORREF handle_color(const bool highlight, const bool tracking, const COLORREF defaultColor = color::hover)
	{
		if (tracking) return color::highlight;
		if (highlight) return color::hover;
		return defaultColor;
	}

	void draw_scroll(const HDC hdc, const bool highlight, const bool tracking)
	{
		if (can_scroll())
		{
			const auto y = MulDiv(m_offset.y, m_extent.cy, m_y_max);
			const auto cy = MulDiv(m_extent.cy, m_extent.cy, m_y_max);
			const auto x_padding = (highlight || tracking) ? 10 : 0;
			const auto right = m_extent.cx;

			if (highlight || tracking)
				fill_solid_rect(hdc, recti(right - 26, 0, right, m_extent.cy), 0x282828u);

			fill_solid_rect(hdc, recti(right - 12 - x_padding, y, right - 4, y + cy), handle_color(highlight, tracking));
		}
	}

	auto_complete_result* selection_from_point(const point_i& pt)
	{
		for (auto& i : m_results)
			if (i.bounds.contains(pt)) return &i;
		return nullptr;
	}

	void hover(auto_complete_result* i)
	{
		if (m_hover_item != i)
		{
			m_hover_item = i;
			this->invalidate(FALSE);
		}
	}

	void selected(auto_complete_result* i)
	{
		if (m_selected_item != i)
		{
			m_selected_item = i;
			this->invalidate(FALSE);

			if (i)
			{
				if (!m_completes->auto_select)
				{
					scope_locked_count l(m_pin);
					m_edit.set_window_text(to_utf16(i->text().text()).c_str());
					m_edit.send_message(EM_SETSEL, 0, -1);
					m_edit.send_message(EM_SCROLLCARET);
					m_edit.set_focus();
				}
				m_completes->selected(i);
				make_visible(i);
			}
		}
	}

	void make_visible(const auto_complete_result* i)
	{
		if (i)
		{
			if (i->bounds.top < m_offset.y)
				scroll_to(i->bounds.top);
			else if (i->bounds.bottom > m_offset.y + m_extent.cy)
				scroll_to(i->bounds.bottom - m_extent.cy);
		}
	}

	auto_complete_result* selected() const { return m_selected_item; }
	bool is_over_scrollbar(const point_i& point) const { return m_extent.cx - 32 < point.x; }
	void scroll_delta(const int delta) { scroll_to(m_offset.y - delta); }

	void scroll_to(int offset)
	{
		offset = clamp_i(offset, 0, m_y_max - m_extent.cy);
		if (m_offset.y != offset)
		{
			m_offset.y = offset;
			layout();
			this->invalidate();
		}
	}

	void step_selection(const int dir)
	{
		if (m_results.empty()) return;

		bool found = false;
		if (dir > 0)
		{
			for (auto& i : m_results)
			{
				if (found)
				{
					selected(&i);
					return;
				}
				found = m_selected_item == &i;
			}
			selected(&m_results.front());
		}
		else
		{
			for (auto i = m_results.rbegin(); i != m_results.rend(); ++i)
			{
				if (found)
				{
					selected(&*i);
					return;
				}
				found = m_selected_item == &*i;
			}
			selected(&m_results.back());
		}
	}

	void cancel()
	{
		if (m_hWnd)
		{
			m_results.clear();
			selected(nullptr);
			hover(nullptr);
			this->show_window(SW_HIDE);
		}
	}

	void search(const std::string& text)
	{
		if (m_pin == 0) m_completes->search(m_hWnd, text);
	}

	void show_results(const auto_complete_results& results)
	{
		m_results = results;
		hover(nullptr);
		selected(nullptr);
		layout();
		m_offset.y = 0;

		if (complete_strategy::ResizeToShowResults)
		{
			auto cy = std::min(m_height, 10 + row_height * static_cast<int>(m_results.size()));
			recti r;
			this->get_client_rect(&r);
			this->set_window_pos(nullptr, 0, 0, r.width(), cy,
			                     SWP_NOACTIVATE | SWP_NOMOVE | (m_results.empty() ? SWP_HIDEWINDOW : SWP_SHOWWINDOW));
		}

		if (m_completes->auto_select && !m_results.empty())
			selected(&m_results.front());

		this->invalidate();
	}

	LRESULT handle_message(UINT msg, WPARAM wParam, LPARAM lParam) override
	{
		switch (msg)
		{
		case WM_SIZE:
			m_extent.cx = LOWORD(lParam);
			m_extent.cy = HIWORD(lParam);
			layout();
			return 0;
		case WM_ERASEBKGND:
			return 1;
		case WM_PAINT:
			{
				recti r;
				this->get_client_rect(&r);
				PAINTSTRUCT ps;
				auto hPaintDc = this->begin_paint(&ps);
				auto hdc = CreateCompatibleDC(hPaintDc);
				auto hBitmap = CreateCompatibleBitmap(hPaintDc, r.width(), r.height());
				auto hOldBitmap = SelectObject(hdc, hBitmap);
				on_paint(hdc);
				BitBlt(hPaintDc, 0, 0, r.width(), r.height(), hdc, 0, 0, SRCCOPY);
				SelectObject(hdc, hOldBitmap);
				DeleteObject(hBitmap);
				DeleteDC(hdc);
				this->end_paint(&ps);
			}
			return 0;
		case WM_MOUSEACTIVATE:
			return MA_NOACTIVATE;
		case WM_LBUTTONDOWN:
			{
				const point_i point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				this->invalidate(false);
				if (is_over_scrollbar(point))
				{
					if (!m_tracking)
					{
						m_y_tracking_start = point.y;
						m_y_tracking_offset_start = m_offset.y;
						m_tracking_scroll = true;
						m_tracking = true;
						this->set_capture();
					}
				}
				else
				{
					auto i = selection_from_point(point + m_offset);
					hover(i);
					selected(i);
					m_completes->click(m_hWnd, i, false);
				}
			}
			return 0;
		case WM_LBUTTONDBLCLK:
			{
				const point_i point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				auto i = selection_from_point(point + m_offset);
				hover(i);
				selected(i);
				m_completes->click(m_hWnd, i, true);
			}
			return 0;
		case WM_MOUSEMOVE:
			{
				const point_i point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				if (!m_hover)
				{
					TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, m_hWnd, 0};
					TrackMouseEvent(&tme);
					m_hover = true;
				}
				if (m_tracking_scroll)
				{
					const auto offset = MulDiv(point.y - m_y_tracking_start, m_y_max, m_extent.cy);
					scroll_to(m_y_tracking_offset_start + offset);
				}
				else
				{
					const auto h = is_over_scrollbar(point);
					if (m_highlight_scroll != h)
					{
						m_highlight_scroll = h;
						this->invalidate();
					}
				}
				hover(selection_from_point(point + m_offset));
			}
			return 0;
		case WM_LBUTTONUP:
			if (m_tracking)
			{
				m_tracking = false;
				m_tracking_scroll = false;
				m_y_tracking_start = 0;
				ReleaseCapture();
				this->invalidate();
			}
			return 0;
		case WM_MOUSELEAVE:
			m_hover_item = nullptr;
			m_hover = false;
			m_highlight_scroll = false;
			this->invalidate(FALSE);
			return 0;
		case WM_MOUSEWHEEL:
			if (can_scroll())
				scroll_delta(static_cast<short>(HIWORD(wParam)) / 2);
			return 0;
		case WM_SEARCHING_COMPLETE:
			show_results(m_completes->results());
			return 0;
		}
		return this->def_window_proc(msg, wParam, lParam);
	}
};


inline bool auto_complete_match(const std::string& qq, const std::string& vv, text_match& match)
{
	const auto q = qq.c_str();
	const auto v = vv.c_str();

	if (q && *q != 0 && v && *v != 0)
	{
		if (q[1] == 0)
		{
			if (normalize(v[0]) == normalize(q[0]))
			{
				match.text(vv, text_range(0, 1));
				return true;
			}
		}
		else
		{
			const auto found = sub_string(v, q);
			if (!found.empty())
			{
				match.text(vv, found);
				return true;
			}
		}
	}
	return false;
}

class html_view;

class address_complete : public std::enable_shared_from_this<address_complete>
{
	auto_complete_results _results;
	std::map<std::string, int> _urls;
	html_view& _view;

public:
	static constexpr bool auto_select = false;
	static constexpr bool ResizeToShowResults = true;

	address_complete(html_view& v) : _view(v)
	{
	}

	void init(HWND)
	{
		if (_urls.empty())
		{
			const char* defaults[] = {
				"amazon.com", "bbc.co.uk", "bing.com", "cnn.com", "facebook.com",
				"github.com", "google.com", "instagram.com", "linkedin.com",
				"microsoft.com", "netflix.com", "reddit.com", "stackoverflow.com",
				"twitter.com", "wikipedia.org", "youtube.com"
			};
			for (const auto u : defaults) _urls[u] = 0;
		}
	}

	void search(const HWND hWnd, const std::string& query)
	{
		auto_complete_results found;

		for (auto& [url, weight] : _urls)
		{
			if (query.empty())
			{
				found.emplace_back(text_match(url), weight);
			}
			else
			{
				text_match m;
				if (auto_complete_match(query, url, m))
					found.emplace_back(m, weight);
			}
		}

		std::sort(found.begin(), found.end(), auto_complete_result::compare_weight);
		if (found.size() > 64) found.resize(64);

		_results = std::move(found);
		::PostMessage(hWnd, WM_SEARCHING_COMPLETE, 0, 0);
	}

	const auto_complete_results& results() { return _results; }

	void selected(auto_complete_result*)
	{
	}

	void click(HWND hwnd, const auto_complete_result* i, bool isDoubleClick);
};


class address_edit final : public subclassed_window
{
	int _icon = -1;

public:
	HWND create(const HWND parent, const LPCWSTR title, const DWORD style, const DWORD exStyle, const int id)
	{
		return subclassed_window::create(L"EDIT", parent, nullptr, title, style, exStyle, id);
	}

	void set_icon(const int i)
	{
		if (_icon != i)
		{
			_icon = i;
			SetWindowPos(m_hWnd, HWND_TOP, 0, 0, 0, 0,
			             SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_DRAWFRAME);
		}
	}

	void on_nc_calc_size(const LPRECT pr)
	{
		if (!pr) return;
		win_dc dc(m_hWnd);
		recti r;
		const auto hOld = SelectObject(dc, get_font());
		::DrawTextW(dc, L"Ky", -1, r, DT_CALCRECT | DT_EDITCONTROL);
		SelectObject(dc, hOld);

		const auto cyText = r.bottom - r.top + 2;
		const auto isMultiLine = (get_style() & ES_MULTILINE) != 0;
		const auto h = pr->bottom - pr->top;
		const auto cy = isMultiLine ? 8 : (h - cyText) / 2;
		constexpr auto cx = 8;

		pr->left += cx + (_icon == -1 ? 0 : 22);
		pr->right -= cx;
		pr->top += cy;
		pr->bottom -= cy;
	}

	static void draw_round_rect(Gdiplus::Graphics& g, const Gdiplus::Rect rect, const int xy,
	                            const Gdiplus::Brush& brush, const Gdiplus::Pen& pen)
	{
		const int left = std::min(rect.GetLeft(), rect.GetRight());
		const int right = std::max(rect.GetLeft(), rect.GetRight());
		const int top = std::min(rect.GetTop(), rect.GetBottom());
		const int bottom = std::max(rect.GetTop(), rect.GetBottom());

		Gdiplus::GraphicsPath path;
		path.AddArc(right - xy, top, xy, xy, 270, 90);
		path.AddArc(right - xy, bottom - xy, xy, xy, 0, 90);
		path.AddArc(left, bottom - xy, xy, xy, 90, 90);
		path.AddArc(left, top, xy, xy, 180, 90);
		path.AddLine(left + xy, top, right - xy / 2, top);
		g.FillPath(&brush, &path);
		g.DrawPath(&pen, &path);
	}

protected:
	LRESULT handle_message(const UINT msg, const WPARAM wParam, const LPARAM lParam) override
	{
		switch (msg)
		{
		case WM_NCCALCSIZE:
			on_nc_calc_size(reinterpret_cast<LPRECT>(lParam));
			return 0;
		case WM_NCPAINT:
			{
				call_default(msg, wParam, lParam);
				HDC dc = GetDCEx(m_hWnd, reinterpret_cast<HRGN>(wParam), DCX_WINDOW | DCX_INTERSECTRGN | 0x10000);
				if (!dc) dc = GetWindowDC(m_hWnd);
				if (dc)
				{
					const auto edit = GetSysColor(COLOR_WINDOW);
					recti r;
					get_window_rect(&r);
					const recti outside(2, 2, r.width(), r.height() - 2);

					Gdiplus::Graphics g(dc);
					g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
					const Gdiplus::SolidBrush brush(edit | 0xFF000000);
					const Gdiplus::SolidBrush black(0x22333333);
					const Gdiplus::Pen pen(&black, 1.5f);
					draw_round_rect(g, outside, 12, brush, pen);
					ReleaseDC(m_hWnd, dc);
				}
			}
			return 0;
		}
		return call_default(msg, wParam, lParam);
	}
};


class toolbar final : public base_window<toolbar>
{
	html_view& _view;

	using ACCELLIST = std::vector<ACCEL>;
	using MAPIDTOACCEL = std::map<int, ACCELLIST>;

	MAPIDTOACCEL _mapAccel;
	volatile long _showingMenu = 0;
	bool _enablePopupItems = true;

	std::map<DWORD, command_ptr> _commands;

	HWND _navigate1 = nullptr;
	HWND _more = nullptr;

	address_edit _address;

	std::shared_ptr<address_complete> _completes;
	list_wnd<address_complete, address_edit> _popup;

	HACCEL _hAccel = nullptr;
	bool _addressHasFocus = false;

public:
	static constexpr LPCWSTR class_name() { return L"potato_toolbar"; }

	void ensure_class_registered()
	{
		register_wnd_class(class_name(), CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS, COLOR_3DFACE + 1);
	}

	toolbar(html_view& v) : _view(v),
	                        _completes(std::make_shared<address_complete>(v)), _popup(_address, _completes)
	{
	}

	bool showing_menu() const { return _showingMenu > 0; }

	bool TranslateAccelerator(const LPMSG pMsg)
	{
		return _hAccel && ::TranslateAccelerator(m_hWnd, _hAccel, pMsg);
	}

	bool on_char(const int c)
	{
		switch (c)
		{
		case VK_UP: _popup.step_selection(-1);
			return true;
		case VK_DOWN: _popup.step_selection(1);
			return true;
		case VK_RETURN:
			{
				const auto selected = _popup.selected();
				const auto text = selected ? selected->text().text(true) : window_text(_address);
				_view.open(text);
			}
			return true;
		case VK_ESCAPE:
			_popup.cancel();
			update_address();
			return true;
		}
		return false;
	}

	void update_address()
	{
		if (!_addressHasFocus)
			_address.set_window_text(to_utf16(_view.url()).c_str());
	}

	bool pre_translate_message(MSG* m)
	{
		if (_addressHasFocus)
		{
			if (_popup)
			{
				switch (m->message)
				{
				case WM_MOUSEWHEEL:
					if (_popup.can_scroll())
					{
						_popup.scroll_delta(static_cast<short>(HIWORD(m->wParam)) / 2);
						return true;
					}
					break;
				case WM_KEYDOWN:
					if (on_char(static_cast<int>(m->wParam))) return true;
					break;
				}
			}
			if (is_dialog_message(m)) return true;
		}
		return false;
	}

	void add_command(const DWORD id, const command_ptr& c) { _commands[id] = c; }

	command_ptr find_command(const DWORD id)
	{
		const auto it = _commands.find(id);
		return it != _commands.end() ? it->second : nullptr;
	}

	bool invoke_command(const WORD id)
	{
		const auto cmd = find_command(id);
		if (cmd)
		{
			cmd->invoke();
			return true;
		}
		return false;
	}

	void address(const std::string& url)
	{
		_address.set_window_text(to_utf16(url).c_str());
	}

	void layout()
	{
		recti rcClient;
		get_client_rect(&rcClient);

		const auto navSize = measure_toolbar(_navigate1);
		const auto moreSize = measure_toolbar(_more);
		constexpr auto pad = 4;

		SetWindowPos(_navigate1, nullptr, pad, pad, navSize.cx, navSize.cy, SWP_NOZORDER);

		const auto addressLeft = pad + navSize.cx + pad;
		const auto addressRight = rcClient.right - moreSize.cx - pad * 2;
		SetWindowPos(_address, nullptr, addressLeft, pad, addressRight - addressLeft, navSize.cy, SWP_NOZORDER);
		SetWindowPos(_more, nullptr, addressRight + pad, pad, moreSize.cx, moreSize.cy, SWP_NOZORDER);

		if (_popup && _popup.is_window())
		{
			recti rAddr;
			_address.get_window_rect(&rAddr);
			recti popupRect;
			_popup.get_window_rect(&popupRect);
			SetWindowPos(_popup, nullptr, rAddr.left, rAddr.bottom, rAddr.width(), popupRect.height(),
			             SWP_NOACTIVATE);
		}
	}

	void enable_toolbar(const HWND tb)
	{
		const int count = static_cast<int>(::SendMessage(tb, TB_BUTTONCOUNT, 0, 0));
		for (int i = 0; i < count; ++i)
		{
			TBBUTTON button;
			if (::SendMessage(tb, TB_GETBUTTON, i, reinterpret_cast<LPARAM>(&button)))
			{
				const auto c = find_command(button.idCommand);
				const auto enable = c && c->is_enabled();
				const auto check = c && c->is_checked() ? 1 : 0;
				::SendMessage(tb, TB_ENABLEBUTTON, button.idCommand, MAKELPARAM(enable, 0));
				::SendMessage(tb, TB_CHECKBUTTON, button.idCommand, MAKELPARAM(check, 0));
			}
		}
	}

	void enable_menu(const HMENU hMenu)
	{
		MENUITEMINFO mii = {sizeof(mii)};
		mii.fMask = MIIM_SUBMENU | MIIM_ID | MIIM_STATE;

		const auto cItems = GetMenuItemCount(hMenu);
		for (auto i = 0; i < cItems; i++)
		{
			if (GetMenuItemInfo(hMenu, i, TRUE, &mii))
			{
				if (mii.hSubMenu)
				{
					enable_menu(mii.hSubMenu);
				}
				else
				{
					const auto c = find_command(mii.wID);
					mii.fState &= ~(MFS_CHECKED | MFS_DISABLED);
					if (!c || !c->is_enabled()) mii.fState |= MFS_DISABLED;
					if (c && c->is_checked()) mii.fState |= MFS_CHECKED;
					SetMenuItemInfo(hMenu, i, TRUE, &mii);
				}
			}
		}
	}

	void init_menu()
	{
		_hAccel = ::LoadAccelerators(g_hInstance, MAKEINTRESOURCE(IDC_POTATO));

		if (_hAccel)
		{
			const auto n = CopyAcceleratorTable(_hAccel, nullptr, 0);
			if (n > 0)
			{
				const auto pAccel = std::make_unique<ACCEL[]>(n);
				CopyAcceleratorTable(_hAccel, pAccel.get(), n);
				for (int i = 0; i < n; i++)
					_mapAccel[pAccel[i].cmd].push_back(pAccel[i]);
			}
		}
	}

	IStream* load_resource_stream(const LPCWSTR pType, const LPCWSTR pName)
	{
		const auto hrsrc = ::FindResource(g_hInstance, pName, pType);
		if (!hrsrc) return nullptr;

		const auto imageSize = SizeofResource(g_hInstance, hrsrc);
		if (!imageSize) return nullptr;

		const auto pData = LockResource(LoadResource(g_hInstance, hrsrc));
		if (!pData) return nullptr;

		const auto hBuf = GlobalAlloc(GMEM_MOVEABLE, imageSize);
		if (!hBuf) return nullptr;

		const auto pBuf = GlobalLock(hBuf);
		if (!pBuf)
		{
			GlobalFree(hBuf);
			return nullptr;
		}

		memcpy(pBuf, pData, imageSize);
		GlobalUnlock(hBuf);

		IStream* stream = nullptr;
		if (CreateStreamOnHGlobal(hBuf, TRUE, &stream) != S_OK)
		{
			GlobalFree(hBuf);
			return nullptr;
		}
		return stream;
	}

	LRESULT handle_message(const UINT msg, const WPARAM wParam, const LPARAM lParam) override
	{
		switch (msg)
		{
		case WM_CREATE:
			{
				constexpr TBBUTTON tbButtonsNav1[] = {
					{1, ID_BROWSE_BACK, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {}, 0},
					{2, ID_BROWSE_NEXT, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {}, 0},
					{5, ID_BROWSE_REFRESH, TBSTATE_ENABLED, BTNS_DROPDOWN | BTNS_AUTOSIZE, {}, 0},
					{6, ID_BROWSE_HOME, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {}, 0},
				};
				constexpr TBBUTTON tbMore[] = {
					{4, ID_VIEW_MENU, TBSTATE_ENABLED, BTNS_DROPDOWN | BTNS_AUTOSIZE, {}, 0},
				};

				constexpr auto xy = 26;
				auto il = ImageList_Create(xy, xy, ILC_COLOR32 | ILC_MASK, 0, 1);

				Gdiplus::Bitmap toolbarBitmap(xy * 8, xy, PixelFormat32bppPARGB);
				{
					Gdiplus::Graphics g(&toolbarBitmap);
					g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

					const int resources[] = {
						IDB_ARROW_DOWN, IDB_ARROW_LEFT, IDB_ARROW_RIGHT, IDB_ARROW_UP,
						IDB_MENU, IDB_REFRESH, IDB_SETTINGS, IDB_STOP, 0
					};

					for (int i = 0; resources[i] != 0; i++)
					{
						const auto stream = load_resource_stream(L"PNG", MAKEINTRESOURCE(resources[i]));
						if (stream)
						{
							Gdiplus::Bitmap bmp(stream);
							g.DrawImage(&bmp, xy * i, 0, xy, xy);
							stream->Release();
						}
					}
				}

				HBITMAP hbm;
				toolbarBitmap.GetHBITMAP(0, &hbm);
				ImageList_Add(il, hbm, nullptr);
				DeleteObject(hbm);

				constexpr auto toolBarStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | CCS_NODIVIDER | CCS_NORESIZE |
					CCS_NOPARENTALIGN | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT;

				_navigate1 = CreateToolbarEx(m_hWnd, toolBarStyle, 1000, 0, nullptr, 0, tbButtonsNav1, 4, xy, xy, xy,
				                             xy, sizeof(TBBUTTON));
				_more = CreateToolbarEx(m_hWnd, toolBarStyle, 1001, 0, nullptr, 0, tbMore, 1, xy, xy, xy, xy,
				                        sizeof(TBBUTTON));

				::SendMessage(_navigate1, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(il));
				::SendMessage(_more, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(il));

				const auto font = CreateFontW(-18, 0, 0, 0, 400, FALSE, FALSE, FALSE, 0, 400, 2, 1, 1, L"Calibri");

				_address.create(m_hWnd, L"",
				                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
				                0, IDC_ADDRESS);
				_address.set_font(font);

				init_menu();
				layout();
			}
			return 0;

		case WM_SIZE:
			layout();
			return 0;

		case WM_COMMAND:
			{
				const auto code = HIWORD(wParam);
				const auto id = LOWORD(wParam);
				const auto hwndCtl = reinterpret_cast<HWND>(lParam);

				if (hwndCtl == _address.m_hWnd)
				{
					if (code == EN_SETFOCUS)
					{
						if (!_popup)
						{
							recti r;
							_address.get_window_rect(&r);
							r.top += r.height();
							r.bottom = 320;
							_popup.create(_address, &r, nullptr, WS_POPUP | WS_BORDER, WS_EX_NOACTIVATE);
							_popup.selected(nullptr);
						}
						_completes->init(_popup);
						_popup.search("");
						_address.post_message(EM_SETSEL, 0, -1);
						_address.set_popup(_popup);
						_addressHasFocus = true;
						return 0;
					}
					if (code == EN_KILLFOCUS)
					{
						if (_popup)
						{
							ShowWindow(_popup, SW_HIDE);
							DestroyWindow(_popup);
							_popup.m_hWnd = nullptr;
						}
						_address.set_popup(nullptr);
						_addressHasFocus = false;
						return 0;
					}
					if (code == EN_CHANGE)
					{
						if (_popup) _popup.search(window_text(_address));
						return 0;
					}
				}

				invoke_command(id);
			}
			return 0;

		case WM_INITMENUPOPUP:
			if (_enablePopupItems)
			{
				const auto hMenu = reinterpret_cast<HMENU>(wParam);
				if (hMenu) enable_menu(hMenu);
			}
			return 0;

		case WM_NOTIFY:
			{
				const auto pnmh = reinterpret_cast<LPNMHDR>(lParam);

				if (pnmh->code == TBN_DROPDOWN)
				{
					const auto ptb = reinterpret_cast<NMTOOLBAR*>(lParam);
					recti rc;
					const auto tbar = pnmh->hwndFrom;
					const auto idx = static_cast<UINT>(::SendMessage(tbar, TB_COMMANDTOINDEX, ptb->iItem, 0));
					::SendMessage(tbar, TB_GETITEMRECT, idx, reinterpret_cast<LPARAM>(static_cast<LPRECT>(rc)));
					MapWindowPoints(tbar, HWND_DESKTOP, reinterpret_cast<LPPOINT>(static_cast<LPRECT>(rc)), 2);

					if (ptb->iItem == ID_VIEW_MENU)
					{
						const auto m = ::LoadMenu(g_hInstance, MAKEINTRESOURCE(IDC_POTATO));
						const auto popup = GetSubMenu(m, 0);
						const auto result = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_RIGHTBUTTON,
						                                   rc.right,
						                                   rc.bottom, 0, m_hWnd, &rc);
						invoke_command(static_cast<WORD>(result));
						DestroyMenu(m);
						layout();
					}
					return TBDDRET_DEFAULT;
				}

				if (pnmh->code == TTN_GETDISPINFO)
				{
					const auto pdi = reinterpret_cast<LPNMTTDISPINFOW>(lParam);
					pdi->szText[0] = 0;
					pdi->lpszText = nullptr;
					pdi->hinst = nullptr;
				}
			}
			return 0;
		}
		return def_window_proc(msg, wParam, lParam);
	}
};

// Defined after toolbar and html_view are both visible
inline void address_complete::click(HWND, const auto_complete_result* i, bool)
{
	if (i) _view.open(i->text().text());
}
