// main.cpp - Application entry point (wWinMain), main frame window, html_view
// implementation, web_history, toolbar command wiring, and the message loop.

#include "pch.h"
#include "ui.h"
#include "style.h"

std::string run_tests();


html_view::html_view(toolbar& tb) : m_toolbar(tb)
{
	// Many sites (including Wikipedia) reject minimal user-agents. Use a Mozilla-compatible UA.
	m_http.open(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) potato/1.0",
	            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS);
}

html_view::~html_view()
{
	m_http.close();
}

void html_view::on_paint(const HDC hdc, const LPRECT rcDraw)
{
	const auto page = m_page;

	if (page)
	{
		render_win32 renderer(hdc, client_pos());

		const position clip(rcDraw->left, rcDraw->top, rcDraw->right - rcDraw->left, rcDraw->bottom - rcDraw->top);
		page->draw(renderer, -m_left, -m_top, &clip);
	}
}

void html_view::on_size(int width, int height)
{
	const auto page = m_page;

	if (page)
	{
		page->client_pos(client_pos());
		layout();

		if (page->media_changed())
		{
			layout();
		}
	}
}

void html_view::open(const std::string& url)
{
	// about: URLs are served from compiled-in resources rather than the network.
	// This lets the back/forward stack round-trip to the start page (about:home),
	// trace output (about:trace), and the test runner (about:tests).
	if (starts(url, "about:"))
	{
		const auto what = url.substr(6);
		if (what == "home" || what.empty())
		{
			open_text(url, load_resource_html(IDR_HTML_TEST));
			return;
		}
		if (what == "tests")
		{
			open_text(url, run_tests());
			return;
		}
		// Unknown about: URL -- fall through to network, which will surface an error.
	}

	m_trace_lines.clear();
	m_loading_url = url;
	trace("[LOAD] Starting download: %s\n", url.c_str());
	auto pThis = this;
	const bool started = m_http.download_file(url, std::make_shared<http_request>(
		                                          [pThis, url](const std::string& file_name, const DWORD error,
		                                                       const DWORD httpStatus, const std::string& reqUrl)
		                                          {
			                                          try
			                                          {
				                                          if (error || httpStatus >= 400)
				                                          {
					                                          pThis->trace_error(url, error, httpStatus);
				                                          }
				                                          else
				                                          {
					                                          pThis->open_file(url, file_name);
				                                          }
			                                          }
			                                          catch (const std::exception& e)
			                                          {
				                                          pThis->trace("[ERROR] Exception: %s\n",
				                                                       e.what());
			                                          }
		                                          }));

	if (!started)
	{
		trace_error(url, GetLastError(), 0);
	}

	set_focus();
}

void html_view::open_file(const std::string& url, const std::string& file_name)
{
	OutputDebugStringA(std::format("[LOAD] File downloaded: {} -> {}\n", url, file_name).c_str());

	auto pThis = this;

	run_async([pThis, url, file_name]()
	{
		auto text = get_file_contents(file_name);
		auto doc = document::parse_from_utf8(*pThis, url, text);
		doc->resolve_styles();

		run_on_ui([pThis, doc, url]()
		{
			OutputDebugStringA(std::format("[LOAD] UI thread: finalizing document for {}\n", url).c_str());

			if (pThis->m_page)
			{
				pThis->m_page->clear();
			}

			pThis->m_page = doc;
			pThis->m_loading_url.clear();
			doc->finalize();

			pThis->m_top = 0;
			pThis->m_left = 0;

			pThis->set_caption();
			pThis->update_history();
			pThis->m_toolbar.address(url);

			OutputDebugStringA(std::format("[LOAD] Page load complete: {}\n", url).c_str());
			pThis->invalidate();
		});
	});

	set_focus();
}

void html_view::trace(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char buffer[1024];
	_vsnprintf_s(buffer, _countof(buffer), _TRUNCATE, format, args);
	va_end(args);

	const std::string message(buffer);
	OutputDebugStringA(message.c_str());

	const bool is_error = message.find("[ERROR]") != std::string::npos;

	// Cheap path: while a page is loading and there is nothing to display, only the
	// initial "Starting download" + any errors get rendered. Otherwise this function
	// would rebuild a full document on every progress message and starve the UI thread
	// when loading link-heavy pages (e.g. Wikipedia pulls many stylesheets/images).
	if (!is_error && !m_trace_lines.empty()) return;

	m_trace_lines.push_back(message);

	std::string html =
		"<html><body style='font-family:Consolas,monospace;padding:20px;background:#1e1e1e;color:#ccc;font-size:13px;'>";
	html += "<h3 style='color:#569cd6;margin-top:0;'>Loading...</h3>";

	for (const auto& line : m_trace_lines)
	{
		if (line.find("[ERROR]") != std::string::npos)
			html += "<div style='color:#f44;white-space:pre;'>";
		else
			html += "<div style='white-space:pre;'>";
		html += line;
		html += "</div>";
	}

	html += "</body></html>";

	if (m_page)
	{
		m_page->clear();
	}

	m_page = document::create_from_utf8(*this, "about:trace", html);
	layout();
	m_top = 0;
	m_left = 0;
	invalidate();
	update_window();
}

void html_view::trace_error(const std::string& url, const DWORD error, const DWORD httpStatus)
{
	trace("[ERROR] Failed to load: %s\n", url.c_str());

	if (httpStatus)
	{
		trace("[ERROR] HTTP Status: %lu\n", httpStatus);
	}

	if (error)
	{
		trace("[ERROR] Error code: %lu\n", error);

		WCHAR errMsg[512] = {};
		FormatMessageW(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM,
		              GetModuleHandleA("winhttp.dll"), error, 0, errMsg, 512, nullptr);

		if (errMsg[0])
		{
			trace("[ERROR] Details: %s\n", to_utf8(errMsg).c_str());
		}
	}
}


void html_view::open_text(const std::string& url, const std::string& text)
{
	if (m_page)
	{
		m_page->clear();
	}

	win_dc hdc(m_hWnd);
	render_win32 renderer(hdc, client_pos());
	m_page = document::create_from_utf8(*this, url, text);

	layout();
	m_top = 0;
	m_left = 0;

	set_caption();
	update_history();

	m_toolbar.address(url);

	invalidate();
}

void html_view::layout()
{
	if (m_hWnd && !m_in_layout)
	{
		const auto page = m_page;

		if (page)
		{
			m_in_layout = true;
			RECT rcClient;
			get_client_rect(&rcClient);

			win_dc hdc(m_hWnd);
			render_win32 renderer(hdc, client_pos());

			const int width = rcClient.right - rcClient.left;
			const int height = rcClient.bottom - rcClient.top;

			page->render(renderer, width);

			m_max_top = page->height() - height;
			if (m_max_top < 0) m_max_top = 0;

			m_max_left = page->width() - width;
			if (m_max_left < 0) m_max_left = 0;

			update_scroll();
			m_in_layout = false;
			invalidate();
		}
	}
}

void html_view::update_scroll()
{
	if (!m_page)
	{
		show_scroll_bar(SB_BOTH, FALSE);
		return;
	}

	if (m_max_top > 0)
	{
		show_scroll_bar(SB_VERT, TRUE);

		RECT rcClient;
		get_client_rect(&rcClient);

		SCROLLINFO si;
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_ALL;
		si.nMin = 0;
		si.nMax = m_max_top + (rcClient.bottom - rcClient.top);
		si.nPos = m_top;
		si.nPage = rcClient.bottom - rcClient.top;
		set_scroll_info(SB_VERT, &si, TRUE);
	}
	else
	{
		show_scroll_bar(SB_VERT, FALSE);
	}

	if (m_max_left > 0)
	{
		show_scroll_bar(SB_HORZ, TRUE);

		RECT rcClient;
		get_client_rect(&rcClient);

		SCROLLINFO si;
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_ALL;
		si.nMin = 0;
		si.nMax = m_max_left + (rcClient.right - rcClient.left);
		si.nPos = m_left;
		si.nPage = rcClient.right - rcClient.left;
		set_scroll_info(SB_HORZ, &si, TRUE);
	}
	else
	{
		show_scroll_bar(SB_HORZ, FALSE);
	}
}

void html_view::on_v_scroll(const int pos, const int flags)
{
	RECT rcClient;
	get_client_rect(&rcClient);

	constexpr int lineHeight = 16;
	const int pageHeight = rcClient.bottom - rcClient.top - lineHeight;

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

void html_view::on_h_scroll(const int pos, const int flags)
{
	RECT rcClient;
	get_client_rect(&rcClient);

	constexpr int lineWidth = 16;
	const int pageWidth = rcClient.right - rcClient.left - lineWidth;

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

void html_view::on_mouse_wheel(const int delta)
{
	constexpr int lineHeight = 16;

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

void html_view::on_key_down(const UINT vKey)
{
	switch (vKey)
	{
	case VK_F5:
		refresh();
		break;
	}
}

void html_view::refresh()
{
	const auto page = m_page;

	if (page)
	{
		open(page->m_url);
	}
}

void html_view::set_caption()
{
	const auto page = m_page;
	const auto parent = get_parent();

	if (!page)
	{
		::SetWindowTextW(parent, L"potato");
	}
	else
	{
		::SetWindowTextW(parent, to_utf16(page->m_caption).c_str());
	}
}

void html_view::on_mouse_move(const int x, const int y)
{
	const auto page = m_page;

	if (page)
	{
		win_dc hdc(m_hWnd);
		render_win32 renderer(hdc, client_pos());
		position::vector redraw_boxes;

		if (page->on_mouse_over(x + m_left, y + m_top, x, y, redraw_boxes))
		{
			for (auto box = redraw_boxes.begin(); box != redraw_boxes.end(); ++box)
			{
				box->x -= m_left;
				box->y -= m_top;
				RECT rcRedraw;
				rcRedraw.left = box->left();
				rcRedraw.right = box->right();
				rcRedraw.top = box->top();
				rcRedraw.bottom = box->bottom();
			}

			invalidate();
			update_cursor();
		}
	}
}

void html_view::on_mouse_leave()
{
	auto page = m_page;

	if (page)
	{
		win_dc hdc(m_hWnd);
		render_win32 renderer(hdc, client_pos());
		position::vector redraw_boxes;

		if (page->on_mouse_leave(redraw_boxes))
		{
			for (auto box = redraw_boxes.begin(); box != redraw_boxes.end(); ++box)
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

		invalidate();
		page.reset();
	}
}

void html_view::on_l_button_down(const int x, const int y)
{
	set_focus();

	const auto page = m_page;

	if (page)
	{
		win_dc hdc(m_hWnd);
		render_win32 renderer(hdc, client_pos());
		position::vector redraw_boxes;

		if (page->on_lbutton_down(x + m_left, y + m_top, x, y, redraw_boxes))
		{
			for (auto box = redraw_boxes.begin(); box != redraw_boxes.end(); ++box)
			{
				box->x -= m_left;
				box->y -= m_top;
				RECT rcRedraw;
				rcRedraw.left = box->left();
				rcRedraw.right = box->right();
				rcRedraw.top = box->top();
				rcRedraw.bottom = box->bottom();
			}

			invalidate();
		}
	}
}

void html_view::on_l_button_up(const int x, const int y)
{
	const auto page = m_page;

	if (page)
	{
		position::vector redraw_boxes;
		win_dc hdc(m_hWnd);
		render_win32 renderer(hdc, client_pos());

		if (page->on_lbutton_up(x + m_left, y + m_top, x, y, redraw_boxes))
		{
			for (auto box = redraw_boxes.begin(); box != redraw_boxes.end(); ++box)
			{
				box->x -= m_left;
				box->y -= m_top;
				RECT rcRedraw;
				rcRedraw.left = box->left();
				rcRedraw.right = box->right();
				rcRedraw.top = box->top();
				rcRedraw.bottom = box->bottom();
			}

			invalidate();
		}
	}
}

void html_view::back()
{
	std::string url;
	if (m_history.back(url))
	{
		open(url);
	}
}

void html_view::forward()
{
	std::string url;
	if (m_history.forward(url))
	{
		open(url);
	}
}

void html_view::update_cursor()
{
	//LPCWSTR defArrow = m_page_next ? IDC_APPSTARTING : IDC_ARROW;
	const LPCWSTR defArrow = IDC_ARROW;

	const auto page = m_page;

	if (!page)
	{
		SetCursor(LoadCursor(nullptr, defArrow));
	}
	else
	{
		if (page->m_cursor == "pointer")
		{
			SetCursor(LoadCursor(nullptr, IDC_HAND));
		}
		else
		{
			SetCursor(LoadCursor(nullptr, defArrow));
		}
	}
}

position html_view::client_pos()
{
	RECT rcClient;
	get_client_rect(&rcClient);

	position client;
	client.x = rcClient.left;
	client.y = rcClient.top;
	client.width = rcClient.right - rcClient.left;
	client.height = rcClient.bottom - rcClient.top;

	return client;
}

void html_view::update_history()
{
	const auto page = m_page;

	if (page)
	{
		m_history.url_opened(page->url());
	}
}

void html_view::scroll_to(const int new_left, const int new_top)
{
	auto client = client_pos();

	if (new_top != m_top)
	{
		m_top = new_top;
		SetScrollPos(m_hWnd, SB_VERT, m_top, TRUE);
		invalidate();
	}

	if (new_left != m_left)
	{
		m_left = new_left;
		SetScrollPos(m_hWnd, SB_HORZ, m_left, TRUE);
		invalidate();
	}
}


web_history::web_history()
{
	m_current_item = 0;
}

web_history::~web_history()
{
}

void web_history::url_opened(const std::string& url)
{
	if (!m_items.empty())
	{
		if (m_current_item != m_items.size() - 1)
		{
			if (m_current_item > 0 && m_items[m_current_item - 1] == url)
			{
				m_current_item--;
			}
			else if (m_current_item < m_items.size() - 1 && m_items[m_current_item + 1] == url)
			{
				m_current_item++;
			}
			else
			{
				m_items.erase(m_items.begin() + m_current_item + 1, m_items.end());
				m_items.push_back(url);
				m_current_item = m_items.size() - 1;
			}
		}
		else
		{
			if (m_current_item > 0 && m_items[m_current_item - 1] == url)
			{
				m_current_item--;
			}
			else
			{
				m_items.push_back(url);
				m_current_item = m_items.size() - 1;
			}
		}
	}
	else
	{
		m_items.push_back(url);
		m_current_item = m_items.size() - 1;
	}
}

bool web_history::back(std::string& url)
{
	if (m_items.empty()) return false;

	if (m_current_item > 0)
	{
		url = m_items[m_current_item - 1];
		return true;
	}
	return false;
}

bool web_history::forward(std::string& url)
{
	if (m_items.empty()) return false;

	if (m_current_item < m_items.size() - 1)
	{
		url = m_items[m_current_item + 1];
		return true;
	}
	return false;
}


#pragma comment(lib, "shlwapi")
#pragma comment(lib, "gdiplus")
#pragma comment(lib, "Comctl32")

#if defined(_M_IX86)
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined(_M_X64)
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#error "Unsupported Processor architecture"
#endif

HINSTANCE g_hInstance;

tasks run_on_ui;
tasks run_async;

std::string run_tests();

sys_event event_exit(true, false);

auto title = L"Potato Browser";

class link_command final : public i_command
{
	html_view& _view;
	std::string _url;

public:
	link_command(html_view& v, std::string url) : _view(v), _url(std::move(url))
	{
	}

	void invoke() override { _view.open(_url); }
};

class func_command final : public i_command
{
	std::function<void()> _f;

public:
	func_command(std::function<void()>&& f) : _f(std::move(f))
	{
	}

	void invoke() override { _f(); }
};

class main_frame final : public base_window<main_frame>
{
public:
	toolbar _toolbar;
	html_view _view;

	static constexpr LPCWSTR class_name() { return L"potato_main"; }

	void ensure_class_registered()
	{
		register_wnd_class(class_name(), CS_HREDRAW | CS_VREDRAW, COLOR_WINDOW + 1,
		                   LoadCursor(nullptr, IDC_ARROW));
	}

	main_frame() : _toolbar(_view), _view(_toolbar)
	{
	}

	static bool is_pre_translate_message(const int message)
	{
		return message != WM_TIMER && message != WM_PAINT && message != WM_ERASEBKGND;
	}

	bool pre_translate_message(MSG* pMsg)
	{
		if (is_pre_translate_message(pMsg->message))
		{
			if (!_toolbar.showing_menu())
			{
				if (_toolbar.pre_translate_message(pMsg))
					return true;

				if (pMsg->message == WM_KEYDOWN)
				{
					switch (pMsg->wParam)
					{
					case VK_BROWSER_BACK: _view.back();
						return true;
					case VK_BROWSER_FORWARD: _view.forward();
						return true;
					case VK_BROWSER_REFRESH: _view.refresh();
						return true;
					}
				}
			}

			if (_toolbar.TranslateAccelerator(pMsg))
				return true;
		}

		return false;
	}

	LRESULT handle_message(const UINT msg, const WPARAM wParam, const LPARAM lParam) override
	{
		switch (msg)
		{
		case WM_CREATE:
			{
				_toolbar.create(m_hWnd, nullptr, nullptr, WS_CHILD | WS_VISIBLE, WS_EX_COMPOSITED);
				_view.create(m_hWnd, nullptr, nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
				             WS_EX_COMPOSITED);

			_view.open("about:home");

				_toolbar.add_command(ID_FILE_BBC, std::make_shared<link_command>(_view, "https://www.bbc.com/news"));
				_toolbar.add_command(
					ID_FILE_WIKIPEDIA,
					std::make_shared<link_command>(_view, "https://en.wikipedia.org/wiki/Main_Page"));
				_toolbar.add_command(ID_FILE_GOOGLE, std::make_shared<link_command>(_view, "http://www.google.com/"));

				_toolbar.add_command(ID_TEST, std::make_shared<func_command>([&]()
				{
					_view.open("about:tests");
				}));
				_toolbar.add_command(IDM_EXIT, std::make_shared<func_command>([&]() { post_message(WM_CLOSE); }));
				_toolbar.add_command(ID_BROWSE_BACK, std::make_shared<func_command>([&]() { _view.back(); }));
				_toolbar.add_command(ID_BROWSE_NEXT, std::make_shared<func_command>([&]() { _view.forward(); }));
				_toolbar.add_command(ID_BROWSE_REFRESH, std::make_shared<func_command>([&]() { _view.refresh(); }));
			}
			return 0;

		case WM_SIZE:
			{
				recti rcClient;
				get_client_rect(&rcClient);
				constexpr auto height = 36;

				auto rcToolbar = rcClient;
				auto rcView = rcClient;
				rcToolbar.bottom = rcToolbar.top + height;
				rcView.top += height;

				_toolbar.set_window_pos(nullptr, rcToolbar, SWP_NOZORDER);
				_view.set_window_pos(nullptr, rcView, SWP_NOZORDER);
			}
			return 0;

		case WM_ACTIVATE:
			{
				def_window_proc();
				if (_view.is_window() && (wParam == WA_ACTIVE || wParam == WA_CLICKACTIVE))
					_view.set_focus();
			}
			return 0;

		case WM_CLOSE:
			destroy_window();
			PostQuitMessage(0);
			return 0;
		}

		return def_window_proc(msg, wParam, lParam);
	}
};

static DWORD WINAPI async_thread_proc(LPVOID)
{
	for (;;)
	{
		const HANDLE h[] = {run_async, event_exit};

		switch (WaitForMultipleObjects(2, h, FALSE, INFINITE))
		{
		case WAIT_OBJECT_0:
			run_async.run();
			break;
		case WAIT_OBJECT_0 + 1:
			return 0;
		default:
			return 1;
		}
	}
}

static int run(main_frame& frame)
{
	const HANDLE hAsyncThread = CreateThread(nullptr, 0, async_thread_proc, nullptr, 0, nullptr);

	MSG msg;

	for (;;)
	{
		const HANDLE h[] = {run_on_ui, event_exit};
		constexpr auto n = 2;

		switch (MsgWaitForMultipleObjects(n, h, FALSE, INFINITE, QS_ALLINPUT))
		{
		case WAIT_OBJECT_0:
			run_on_ui.run();
			break;
		case WAIT_OBJECT_0 + 1:
			if (hAsyncThread)
			{
				WaitForSingleObject(hAsyncThread, 5000);
				CloseHandle(hAsyncThread);
			}
			return 0;
		case WAIT_OBJECT_0 + 2:
			break;
		default:
			if (hAsyncThread) CloseHandle(hAsyncThread);
			return FALSE;
		}

		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				event_exit.set();
				if (hAsyncThread)
				{
					WaitForSingleObject(hAsyncThread, 5000);
					CloseHandle(hAsyncThread);
				}
				return static_cast<int>(msg.wParam);
			}

			if (!frame.pre_translate_message(&msg))
			{
				TranslateMessage(&msg);
				::DispatchMessage(&msg);
			}
		}
	}
}

int APIENTRY wWinMain(_In_ const HINSTANCE hInstance,
                      _In_opt_ HINSTANCE,
                      _In_ LPWSTR,
                      _In_ const int nCmdShow)
{
	g_hInstance = hInstance;

	int result = 1;

	if (SUCCEEDED(CoInitialize(nullptr)))
	{
		const Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		ULONG_PTR gdiplusToken;
		Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

		constexpr INITCOMMONCONTROLSEX iccx = {sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES};
		InitCommonControlsEx(&iccx);

		{
			main_frame frame;
			frame.create(nullptr, nullptr, title, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);

			const auto icon = LoadIcon(hInstance, MAKEINTRESOURCE(IDC_POTATO));
			frame.set_icon(icon, TRUE);
			frame.set_icon(icon, FALSE);
			frame.show_window(nCmdShow);

			result = run(frame);
		}

		Gdiplus::GdiplusShutdown(gdiplusToken);
		CoUninitialize();
	}

	return result;
}
