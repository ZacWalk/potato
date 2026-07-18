// document.cpp - WinHTTP async downloads, HTML entity table, parser with
// implicit tag closing, document rendering, font caching via GDI+, and
// stylesheet application.

#include "pch.h"
#include "document.h"
#include "ui.h"
#include "resource.h"


#pragma comment(lib, "winhttp.lib")

http::~http()
{
	stop();
	close();
}

bool http::open(const LPCWSTR pwszUserAgent, const DWORD dwAccessType, const LPCWSTR pwszProxyName,
                const LPCWSTR pwszProxyBypass)
{
	m_hSession = WinHttpOpen(pwszUserAgent, dwAccessType, pwszProxyName, pwszProxyBypass, WINHTTP_FLAG_ASYNC);
	if (!m_hSession) return false;

	WinHttpSetOption(m_hSession, WINHTTP_OPTION_MAX_CONNS_PER_SERVER,
	                 &m_max_connections_per_server, sizeof(m_max_connections_per_server));

	// Enable automatic gzip/deflate decoding so we can talk to modern sites (e.g. Wikipedia).
	DWORD decompress = WINHTTP_DECOMPRESSION_FLAG_ALL;
	WinHttpSetOption(m_hSession, WINHTTP_OPTION_DECOMPRESSION, &decompress, sizeof(decompress));

	// Allow redirects across http<->https; default policy disallows scheme downgrade only.
	DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
	WinHttpSetOption(m_hSession, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));

	// Modern TLS only.
	DWORD secure_protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
	WinHttpSetOption(m_hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &secure_protocols, sizeof(secure_protocols));

	if (WinHttpSetStatusCallback(m_hSession, http_callback,
	                             WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0) == WINHTTP_INVALID_STATUS_CALLBACK)
	{
		WinHttpCloseHandle(m_hSession);
		m_hSession = nullptr;
		return false;
	}

	return true;
}

void http::close()
{
	if (m_hSession)
	{
		WinHttpCloseHandle(m_hSession);
		m_hSession = nullptr;
	}
}

VOID CALLBACK http::http_callback(HINTERNET, const DWORD_PTR dwContext, const DWORD dwInternetStatus,
                                  const LPVOID lpvStatusInformation, const DWORD dwStatusInformationLength)
{
	const auto request = reinterpret_cast<http_request*>(dwContext);
	if (!request) return;

	DWORD dwError = ERROR_SUCCESS;

	switch (dwInternetStatus)
	{
	case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
		dwError = request->on_send_request_complete();
		break;
	case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
		dwError = request->on_headers_available();
		break;
	case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
		dwError = request->on_read_complete(dwStatusInformationLength);
		break;
	case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
		request->on_handle_closing();
		return; // request may be destroyed; do not touch further
	case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
		dwError = request->on_request_error(static_cast<WINHTTP_ASYNC_RESULT*>(lpvStatusInformation)->dwError);
		break;
	default:
		break;
	}

	if (dwError != ERROR_SUCCESS)
	{
		request->cancel();
	}
}

bool http::download_file(const std::string& url, const std::shared_ptr<http_request>& request)
{
	OutputDebugStringA(std::format("Download {}\n", url).c_str());

	if (!request || !m_hSession) return false;

	request->set_parent(this);

	// Register BEFORE issuing the request so callbacks (which can fire on another
	// thread the moment WinHttpSendRequest returns) always find a live shared_ptr.
	{
		std::lock_guard lock(m_mutex);
		m_requests.push_back(request);
	}

	if (!request->create(url, m_hSession))
	{
		// create() failed before any callback could fire; un-register.
		std::lock_guard lock(m_mutex);
		std::erase(m_requests, request);
		return false;
	}
	return true;
}

void http::remove_request(const std::shared_ptr<http_request>& request)
{
	std::lock_guard lock(m_mutex);
	std::erase(m_requests, request);
}

void http::stop()
{
	// Snapshot under lock, then cancel without holding it -- cancel() takes the
	// per-request mutex and we must not hold http::m_mutex across that.
	std::vector<std::shared_ptr<http_request>> snapshot;
	{
		std::lock_guard lock(m_mutex);
		snapshot = m_requests;
	}
	for (const auto& r : snapshot)
	{
		r->cancel();
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

http_request::http_request(callback_t callback) : m_callback(std::move(callback))
{
	WCHAR folder[MAX_PATH];
	WCHAR path[MAX_PATH];

	GetTempPathW(MAX_PATH, folder);
	GetTempFileNameW(folder, L"pot", 0, path);

	m_file = to_utf8(path);
	m_hFile = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
	                      CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
}

http_request::~http_request()
{
	cancel();

	if (m_hFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;
	}
}

bool http_request::create(const std::string& url_in, const HINTERNET hSession)
{
	m_url = url_in;
	m_error = ERROR_SUCCESS;

	if (!starts(m_url, "http://") && !starts(m_url, "https://"))
	{
		m_url = "https://" + m_url;
	}

	const auto wurl = to_utf16(m_url);

	URL_COMPONENTS urlComp{};
	urlComp.dwStructSize = sizeof(urlComp);
	urlComp.dwSchemeLength = static_cast<DWORD>(-1);
	urlComp.dwHostNameLength = static_cast<DWORD>(-1);
	urlComp.dwUrlPathLength = static_cast<DWORD>(-1);
	urlComp.dwExtraInfoLength = static_cast<DWORD>(-1);

	if (!WinHttpCrackUrl(wurl.c_str(), static_cast<DWORD>(wurl.size()), 0, &urlComp))
	{
		return false;
	}

	const std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
	std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
	if (urlComp.dwExtraInfoLength)
	{
		path.append(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
	}

	const DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;

	HINTERNET hConn = WinHttpConnect(hSession, host.c_str(), urlComp.nPort, 0);
	if (!hConn) return false;

	PCWSTR acceptTypes[] = {L"*/*", nullptr};
	HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
	                                    acceptTypes, flags);
	if (!hReq)
	{
		WinHttpCloseHandle(hConn);
		return false;
	}

	// Tell the server we accept compressed responses (the session-level decompression
	// option will transparently decode them for us).
	const std::wstring extra_headers = L"Accept-Encoding: gzip, deflate\r\n";
	WinHttpAddRequestHeaders(hReq, extra_headers.c_str(), static_cast<DWORD>(extra_headers.size()),
	                         WINHTTP_ADDREQ_FLAG_ADD);

	// Publish handles, then send. The send must come AFTER assigning to members so
	// that a callback firing on another thread can safely use them via on_*.
	{
		std::lock_guard lock(m_handle_mutex);
		m_hConnection = hConn;
		m_hRequest = hReq;
	}

	if (!WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0,
	                        reinterpret_cast<DWORD_PTR>(this)))
	{
		cancel();
		return false;
	}

	return true;
}

void http_request::cancel()
{
	HINTERNET hReq = nullptr;
	HINTERNET hConn = nullptr;
	{
		std::lock_guard lock(m_handle_mutex);
		hReq = m_hRequest;
		hConn = m_hConnection;
		m_hRequest = nullptr;
		m_hConnection = nullptr;
	}
	// Close OUTSIDE the lock; closing a handle can synchronously invoke the
	// HANDLE_CLOSING callback which would otherwise reenter via on_handle_closing.
	if (hReq) WinHttpCloseHandle(hReq);
	if (hConn) WinHttpCloseHandle(hConn);
}

DWORD http_request::on_send_request_complete()
{
	HINTERNET h = nullptr;
	{
		std::lock_guard lock(m_handle_mutex);
		h = m_hRequest;
	}
	if (!h) return ERROR_OPERATION_ABORTED;
	return WinHttpReceiveResponse(h, nullptr) ? ERROR_SUCCESS : GetLastError();
}

DWORD http_request::on_headers_available()
{
	HINTERNET h = nullptr;
	{
		std::lock_guard lock(m_handle_mutex);
		h = m_hRequest;
	}
	if (!h) return ERROR_OPERATION_ABORTED;

	on_headers_ready(h);

	m_status = 0;
	DWORD statusLen = sizeof(m_status);
	if (!WinHttpQueryHeaders(h, WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE, nullptr, &m_status,
	                         &statusLen, nullptr))
	{
		return GetLastError();
	}

	WCHAR buf[64];
	DWORD len = sizeof(buf);
	m_content_length = WinHttpQueryHeaders(h, WINHTTP_QUERY_CONTENT_LENGTH, nullptr, buf, &len, nullptr)
		                   ? _wtoi64(buf)
		                   : 0;
	m_downloaded_length = 0;

	return read_data();
}

DWORD http_request::on_handle_closing()
{
	if (m_finished.exchange(true)) return ERROR_SUCCESS;

	WCHAR errMsg[256] = {};
	if (m_error)
	{
		FormatMessage(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM,
		              GetModuleHandleA("winhttp.dll"), m_error, 0, errMsg, _countof(errMsg), nullptr);
	}

	on_finish(m_error, errMsg);

	// Keep ourselves alive across the un-registration: once removed from
	// http::m_requests this could be the last reference.
	if (m_http)
	{
		auto self = shared_from_this();
		m_http->remove_request(self);
	}
	return ERROR_SUCCESS;
}

DWORD http_request::on_request_error(const DWORD dwError)
{
	m_error = dwError;
	return dwError;
}

DWORD http_request::read_data()
{
	HINTERNET h = nullptr;
	{
		std::lock_guard lock(m_handle_mutex);
		h = m_hRequest;
	}
	if (!h) return ERROR_OPERATION_ABORTED;
	return WinHttpReadData(h, m_buffer, sizeof(m_buffer), nullptr) ? ERROR_SUCCESS : GetLastError();
}

DWORD http_request::on_read_complete(const DWORD len)
{
	if (len == 0)
	{
		cancel();
		return ERROR_SUCCESS;
	}

	m_downloaded_length += len;
	on_data(m_buffer, len, m_downloaded_length, m_content_length);
	return read_data();
}

void http_request::on_headers_ready(HINTERNET)
{
}

void http_request::on_finish(const DWORD dwError, LPCWSTR /*errMsg*/)
{
	if (m_hFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;
	}

	auto cb = m_callback;
	auto f = m_file;
	auto err = dwError;
	auto status = m_status;
	auto url = m_url;

	run_on_ui([cb = std::move(cb), f = std::move(f), err, status, url = std::move(url)]()
	{
		if (cb) cb(f, err, status, url);
	});
}

void http_request::on_data(const LPCBYTE data, const DWORD len, ULONG64 /*downloaded*/, ULONG64 /*total*/)
{
	if (m_hFile != INVALID_HANDLE_VALUE)
	{
		DWORD written = 0;
		WriteFile(m_hFile, data, len, &written, nullptr);
	}
}


html_entities g_html_entities[] =
{
	{"&quot;", 0x0022},
	{"&amp;", 0x0026},
	{"&lt;", 0x003C},
	{"&gt;", 0x003E},
	{"&nbsp;", 0x00A0},
	{"&iexcl;", 0x00A1},
	{"&cent;", 0x00A2},
	{"&pound;", 0x00A3},
	{"&curren;", 0x00A4},
	{"&yen;", 0x00A5},
	{"&brvbar;", 0x00A6},
	{"&sect;", 0x00A7},
	{"&uml;", 0x00A8},
	{"&copy;", 0x00A9},
	{"&ordf;", 0x00AA},
	{"&laquo;", 0x00AB},
	{"&lsaquo;", 0x00AB},
	{"&not;", 0x00AC},
	{"&shy;", 0x00AD},
	{"&reg;", 0x00AE},
	{"&macr;", 0x00AF},
	{"&deg;", 0x00B0},
	{"&plusmn;", 0x00B1},
	{"&sup2;", 0x00B2},
	{"&sup3;", 0x00B3},
	{"&acute;", 0x00B4},
	{"&micro;", 0x00B5},
	{"&para;", 0x00B6},
	{"&middot;", 0x00B7},
	{"&cedil;", 0x00B8},
	{"&sup1;", 0x00B9},
	{"&ordm;", 0x00BA},
	{"&raquo;", 0x00BB},
	{"&rsaquo;", 0x00BB},
	{"&frac14;", 0x00BC},
	{"&frac12;", 0x00BD},
	{"&frac34;", 0x00BE},
	{"&iquest;", 0x00BF},
	{"&Agrave;", 0x00C0},
	{"&Aacute;", 0x00C1},
	{"&Acirc;", 0x00C2},
	{"&Atilde;", 0x00C3},
	{"&Auml;", 0x00C4},
	{"&Aring;", 0x00C5},
	{"&AElig;", 0x00C6},
	{"&Ccedil;", 0x00C7},
	{"&Egrave;", 0x00C8},
	{"&Eacute;", 0x00C9},
	{"&Ecirc;", 0x00CA},
	{"&Euml;", 0x00CB},
	{"&Igrave;", 0x00CC},
	{"&Iacute;", 0x00CD},
	{"&Icirc;", 0x00CE},
	{"&Iuml;", 0x00CF},
	{"&ETH;", 0x00D0},
	{"&Ntilde;", 0x00D1},
	{"&Ograve;", 0x00D2},
	{"&Oacute;", 0x00D3},
	{"&Ocirc;", 0x00D4},
	{"&Otilde;", 0x00D5},
	{"&Ouml;", 0x00D6},
	{"&times;", 0x00D7},
	{"&Oslash;", 0x00D8},
	{"&Ugrave;", 0x00D9},
	{"&Uacute;", 0x00DA},
	{"&Ucirc;", 0x00DB},
	{"&Uuml;", 0x00DC},
	{"&Yacute;", 0x00DD},
	{"&THORN;", 0x00DE},
	{"&szlig;", 0x00DF},
	{"&agrave;", 0x00E0},
	{"&aacute;", 0x00E1},
	{"&acirc;", 0x00E2},
	{"&atilde;", 0x00E3},
	{"&auml;", 0x00E4},
	{"&aring;", 0x00E5},
	{"&aelig;", 0x00E6},
	{"&ccedil;", 0x00E7},
	{"&egrave;", 0x00E8},
	{"&eacute;", 0x00E9},
	{"&ecirc;", 0x00EA},
	{"&euml;", 0x00EB},
	{"&igrave;", 0x00EC},
	{"&iacute;", 0x00ED},
	{"&icirc;", 0x00EE},
	{"&iuml;", 0x00EF},
	{"&eth;", 0x00F0},
	{"&ntilde;", 0x00F1},
	{"&ograve;", 0x00F2},
	{"&oacute;", 0x00F3},
	{"&ocirc;", 0x00F4},
	{"&otilde;", 0x00F5},
	{"&ouml;", 0x00F6},
	{"&divide;", 0x00F7},
	{"&oslash;", 0x00F8},
	{"&ugrave;", 0x00F9},
	{"&uacute;", 0x00FA},
	{"&ucirc;", 0x00FB},
	{"&uuml;", 0x00FC},
	{"&yacute;", 0x00FD},
	{"&thorn;", 0x00FE},
	{"&yuml;", 0x00FF},
	{"&OElig;", 0x0152},
	{"&oelig;", 0x0153},
	{"&Scaron;", 0x0160},
	{"&scaron;", 0x0161},
	{"&Yuml;", 0x0178},
	{"&fnof;", 0x0192},
	{"&circ;", 0x02C6},
	{"&tilde;", 0x02DC},
	{"&Alpha;", 0x0391},
	{"&Beta;", 0x0392},
	{"&Gamma;", 0x0393},
	{"&Delta;", 0x0394},
	{"&Epsilon;", 0x0395},
	{"&Zeta;", 0x0396},
	{"&Eta;", 0x0397},
	{"&Theta;", 0x0398},
	{"&Iota;", 0x0399},
	{"&Kappa;", 0x039A},
	{"&Lambda;", 0x039B},
	{"&Mu;", 0x039C},
	{"&Nu;", 0x039D},
	{"&Xi;", 0x039E},
	{"&Omicron;", 0x039F},
	{"&Pi;", 0x03A0},
	{"&Rho;", 0x03A1},
	{"&Sigma;", 0x03A3},
	{"&Tau;", 0x03A4},
	{"&Upsilon;", 0x03A5},
	{"&Phi;", 0x03A6},
	{"&Chi;", 0x03A7},
	{"&Psi;", 0x03A8},
	{"&Omega;", 0x03A9},
	{"&alpha;", 0x03B1},
	{"&beta;", 0x03B2},
	{"&gamma;", 0x03B3},
	{"&delta;", 0x03B4},
	{"&epsilon;", 0x03B5},
	{"&zeta;", 0x03B6},
	{"&eta;", 0x03B7},
	{"&theta;", 0x03B8},
	{"&iota;", 0x03B9},
	{"&kappa;", 0x03BA},
	{"&lambda;", 0x03BB},
	{"&mu;", 0x03BC},
	{"&nu;", 0x03BD},
	{"&xi;", 0x03BE},
	{"&omicron;", 0x03BF},
	{"&pi;", 0x03C0},
	{"&rho;", 0x03C1},
	{"&sigmaf;", 0x03C2},
	{"&sigma;", 0x03C3},
	{"&tau;", 0x03C4},
	{"&upsilon;", 0x03C5},
	{"&phi;", 0x03C6},
	{"&chi;", 0x03C7},
	{"&psi;", 0x03C8},
	{"&omega;", 0x03C9},
	{"&thetasym;", 0x03D1},
	{"&upsih;", 0x03D2},
	{"&piv;", 0x03D6},
	{"&ensp;", 0x2002},
	{"&emsp;", 0x2003},
	{"&thinsp;", 0x2009},
	{"&zwnj;", 0x200C},
	{"&zwj;", 0x200D},
	{"&lrm;", 0x200E},
	{"&rlm;", 0x200F},
	{"&ndash;", 0x2013},
	{"&mdash;", 0x2014},
	{"&lsquo;", 0x2018},
	{"&rsquo;", 0x2019},
	{"&sbquo;", 0x201A},
	{"&ldquo;", 0x201C},
	{"&rdquo;", 0x201D},
	{"&bdquo;", 0x201E},
	{"&dagger;", 0x2020},
	{"&Dagger;", 0x2021},
	{"&bull;", 0x2022},
	{"&hellip;", 0x2026},
	{"&permil;", 0x2030},
	{"&prime;", 0x2032},
	{"&Prime;", 0x2033},
	{"&lsaquo;", 0x2039},
	{"&rsaquo;", 0x203A},
	{"&oline;", 0x203E},
	{"&frasl;", 0x2044},
	{"&euro;", 0x20AC},
	{"&image;", 0x2111},
	{"&weierp;", 0x2118},
	{"&real;", 0x211C},
	{"&trade;", 0x2122},
	{"&alefsym;", 0x2135},
	{"&larr;", 0x2190},
	{"&uarr;", 0x2191},
	{"&rarr;", 0x2192},
	{"&darr;", 0x2193},
	{"&harr;", 0x2194},
	{"&crarr;", 0x21B5},
	{"&lArr;", 0x21D0},
	{"&uArr;", 0x21D1},
	{"&rArr;", 0x21D2},
	{"&dArr;", 0x21D3},
	{"&hArr;", 0x21D4},
	{"&forall;", 0x2200},
	{"&part;", 0x2202},
	{"&exist;", 0x2203},
	{"&empty;", 0x2205},
	{"&nabla;", 0x2207},
	{"&isin;", 0x2208},
	{"&notin;", 0x2209},
	{"&ni;", 0x220B},
	{"&prod;", 0x220F},
	{"&sum;", 0x2211},
	{"&minus;", 0x2212},
	{"&lowast;", 0x2217},
	{"&radic;", 0x221A},
	{"&prop;", 0x221D},
	{"&infin;", 0x221E},
	{"&ang;", 0x2220},
	{"&and;", 0x2227},
	{"&or;", 0x2228},
	{"&cap;", 0x2229},
	{"&cup;", 0x222A},
	{"&int;", 0x222B},
	{"&there4;", 0x2234},
	{"&sim;", 0x223C},
	{"&cong;", 0x2245},
	{"&asymp;", 0x2248},
	{"&ne;", 0x2260},
	{"&equiv;", 0x2261},
	{"&le;", 0x2264},
	{"&ge;", 0x2265},
	{"&sub;", 0x2282},
	{"&sup;", 0x2283},
	{"&nsub;", 0x2284},
	{"&sube;", 0x2286},
	{"&supe;", 0x2287},
	{"&oplus;", 0x2295},
	{"&otimes;", 0x2297},
	{"&perp;", 0x22A5},
	{"&sdot;", 0x22C5},
	{"&lceil;", 0x2308},
	{"&rceil;", 0x2309},
	{"&lfloor;", 0x230A},
	{"&rfloor;", 0x230B},
	{"&lang;", 0x2329},
	{"&rang;", 0x232A},
	{"&loz;", 0x25CA},
	{"&spades;", 0x2660},
	{"&clubs;", 0x2663},
	{"&hearts;", 0x2665},
	{"&diams;", 0x2666},

	{"", 0}
};


stop_tags_t parser::m_stop_tags[] =
{
	{"body;head", "html"},
	{"td;th;tr;tbody;thead;tfoot", "table"},
	{nullptr, nullptr},
};

omitted_end_tags_t parser::m_omitted_end_tags[] =
{
	{"li", "li"},
	{"dt", "dt;dd"},
	{"dd", "dt;dd"},
	{
		"p", "address;article;aside;blockquote;div;dl;fieldset;footer;form;h1;h2;h3;h4;h5;h6;header;hgroup;hr;main;nav;ol;p;pre;section;table;ul"
	},
	{"rb", "rb;rt;rtc;rp"},
	{"rt", "rb;rt;rtc;rp"},
	{"rtc", "rb;rt;rtc;rp"},
	{"rp", "rb;rt;rtc;rp"},
	{"optgroup", "optgroup"},
	{"option", "optgroup;option"},
	{"thead", "tbody;tfoot"},
	{"tbody", "tbody;tfoot"},
	{"tfoot", "tbody;tfoot"},
	{"tr", "tr"},
	{"td", "td;th"},
	{"th", "td;th"},
	{nullptr, nullptr},
};


std::unique_ptr<element> parser::create_element(const std::string& tag_name)
{
	std::unique_ptr<element> newTag;

	if (!newTag)
	{
		if (is_equal(tag_name, "br"))
		{
			newTag = std::make_unique<element>(m_doc, el_break);
		}
		else if (is_equal(tag_name, "p"))
		{
			newTag = std::make_unique<element>(m_doc, el_para);
		}
		else if (is_equal(tag_name, "img"))
		{
			newTag = std::make_unique<element>(m_doc, el_image);
		}
		else if (is_equal(tag_name, "table"))
		{
			newTag = std::make_unique<element>(m_doc, el_table);
		}
		else if (is_equal(tag_name, "td") || is_equal(tag_name, "th"))
		{
			newTag = std::make_unique<element>(m_doc, el_td);
		}
		else if (is_equal(tag_name, "link"))
		{
			newTag = std::make_unique<element>(m_doc, el_link);
		}
		else if (is_equal(tag_name, "title"))
		{
			newTag = std::make_unique<element>(m_doc, el_title);
		}
		else if (is_equal(tag_name, "a"))
		{
			newTag = std::make_unique<element>(m_doc, el_anchor);
		}
		else if (is_equal(tag_name, "tr"))
		{
			newTag = std::make_unique<element>(m_doc, el_tr);
		}
		else if (is_equal(tag_name, "style"))
		{
			newTag = std::make_unique<element>(m_doc, el_style);
		}
		else if (is_equal(tag_name, "base"))
		{
			newTag = std::make_unique<element>(m_doc, el_base);
		}
		else if (is_equal(tag_name, "body"))
		{
			newTag = std::make_unique<element>(m_doc, el_body);
		}
		else if (is_equal(tag_name, "div"))
		{
			newTag = std::make_unique<element>(m_doc, el_div);
		}
		else if (is_equal(tag_name, "script"))
		{
			newTag = std::make_unique<element>(m_doc, el_script);
		}
		else if (is_equal(tag_name, "font"))
		{
			newTag = std::make_unique<element>(m_doc, el_font);
		}
		else if (is_equal(tag_name, "svg"))
		{
			newTag = std::make_unique<element>(m_doc, el_svg);
		}
		else
		{
			newTag = std::make_unique<element>(m_doc, el_html);
		}
	}

	if (newTag)
	{
		newTag->set_tag_name(tag_name);
	}

	return newTag;
}

void parser::parse_tag_start(const std::string& tag_name)
{
	parse_pop_void_element();

	// We add the html(root) element before parsing
	if (is_equal(tag_name, "html"))
	{
		return;
	}

	auto el = create_element(tag_name);

	if (el)
	{
		if (m_parse_stack.back()->get_tag_name() == "html")
		{
			// if last element is root we have to add head or body
			if (!value_in_list(tag_name, "head;body"))
			{
				parse_push_element(create_element("body"));
			}
		}

		parse_close_omitted_end(tag_name);
		parse_open_omitted_start(tag_name);
		parse_push_element(std::move(el));
	}
}


void parser::parse_tag_end(const std::string& tag_name)
{
	if (!m_parse_stack.empty())
	{
		if (m_parse_stack.back()->get_tag_name() == tag_name)
		{
			parse_pop_element();
		}
		else
		{
			auto stop_tag = "";

			for (int i = 0; m_stop_tags[i].tags; i++)
			{
				if (value_in_list(tag_name, m_stop_tags[i].tags))
				{
					stop_tag = m_stop_tags[i].stop_parent;
					break;
				}
			}
			parse_pop_element(tag_name, stop_tag);
		}
	}
}


void parser::parse_push_element(std::unique_ptr<element> el)
{
	if (!m_parse_stack.empty())
	{
		auto raw = el.get();
		m_parse_stack.back()->append_child(std::move(el));
		m_parse_stack.push_back(raw);
	}
}

void parser::parse_attribute(const std::string& attr_name, const std::string& attr_value)
{
	if (!m_parse_stack.empty())
	{
		m_parse_stack.back()->set_attr(attr_name, attr_value);
	}
}

void parser::parse_word(const std::string& val)
{
	if (m_parse_stack.back()->get_tag_name() == "html")
	{
		parse_push_element(create_element("body"));
	}

	parse_pop_void_element();

	if (!m_parse_stack.empty())
	{
		m_parse_stack.back()->append_text(val);
	}
}

void parser::parse_space(const std::string& val)
{
	parse_pop_void_element();
	if (!m_parse_stack.empty())
	{
		m_parse_stack.back()->append_space(val);
	}
}

void parser::parse_comment_start()
{
	parse_pop_void_element();
	parse_push_element(std::make_unique<element>(m_doc, el_comment));
}

void parser::parse_comment_end()
{
	parse_pop_element();
}

void parser::parse_cdata_start()
{
	parse_pop_void_element();
	parse_push_element(std::make_unique<element>(m_doc, el_cdata));
}

void parser::parse_cdata_end()
{
	parse_pop_element();
}

void parser::parse_data(const std::string& val)
{
	if (!m_parse_stack.empty())
	{
		m_parse_stack.back()->set_data(val);
	}
}

bool parser::parse_pop_element()
{
	if (!m_parse_stack.empty())
	{
		m_parse_stack.pop_back();
		return true;
	}
	return false;
}

bool parser::parse_pop_element(const std::string& tag, const char* stop_tags)
{
	bool found = false;
	for (auto iel = m_parse_stack.rbegin(); iel != m_parse_stack.rend(); ++iel)
	{
		if ((*iel)->get_tag_name() == tag)
		{
			found = true;
			break;
		}
		if (value_in_list((*iel)->get_tag_name(), stop_tags)) break;
	}

	if (!found) return false;

	while (found)
	{
		if (m_parse_stack.back()->get_tag_name() == tag)
		{
			found = false;
		}
		parse_pop_element();
	}
	return true;
}

void parser::parse_pop_void_element()
{
	if (!m_parse_stack.empty())
	{
		if (value_in_list(m_parse_stack.back()->get_tag_name(), void_elements))
		{
			parse_pop_element();
		}
	}
}

void parser::parse_pop_to_parent(const std::string& parents, const std::string& stop_parent)
{
	std::vector<element*>::size_type parent = 0;
	bool found = false;
	auto p = split_string(parents, ';');

	for (int i = static_cast<int>(m_parse_stack.size()) - 1; i >= 0 && !found; i--)
	{
		if (std::find(p.begin(), p.end(), m_parse_stack[i]->get_tag_name()) != p.end())
		{
			found = true;
			parent = i;
		}
		if (m_parse_stack[i]->get_tag_name() == stop_parent)
		{
			break;
		}
	}
	if (found)
	{
		m_parse_stack.erase(m_parse_stack.begin() + parent + 1, m_parse_stack.end());
	}
	else
	{
		parse_tag_start(p.front());
	}
}

void parser::parse_close_omitted_end(const std::string& tag)
{
	for (int i = 0; m_omitted_end_tags[i].tag; i++)
	{
		if (m_parse_stack.back()->get_tag_name() == m_omitted_end_tags[i].tag)
		{
			if (value_in_list(tag, m_omitted_end_tags[i].followed_tags))
			{
				parse_pop_element();
				break;
			}
		}
	}
}

void parser::parse_open_omitted_start(const std::string& tag)
{
	if (is_equal(tag, "col"))
	{
		if (m_parse_stack.back()->get_tag_name() != "colgroup")
		{
			parse_tag_start("colgroup");
		}
	}
	else if (is_equal(tag, "tr"))
	{
		if (m_parse_stack.back()->get_tag_name() != "tbody" &&
			m_parse_stack.back()->get_tag_name() != "thead" &&
			m_parse_stack.back()->get_tag_name() != "tfoot")
		{
			parse_tag_start("tbody");
		}
	}
}


document::document(html_view& v) : m_view(v), m_over_element(nullptr)
{
	m_http.open(L"potato/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr);
}

document::~document()
{
	m_http.stop();
	clear();

	std::lock_guard lock(m_fonts_mutex);
	for (auto& [key, fi] : m_fonts)
	{
		if (fi.font)
		{
			DeleteObject(fi.font);
			fi.font = nullptr;
		}
	}
}

void document::clear()
{
	m_parsed_root.reset();
	m_root.reset();
	m_over_element = nullptr;

	m_styles.clear();
	m_fixed_boxes.clear();
	m_media_lists.clear();
	m_images.clear();
}

void document::load_master_stylesheet(const std::string& text)
{
	auto media_list = media_query_list::create_from_string("screen");
	m_styles.parse_stylesheet(text, empty, *this, media_list);
}

template <class tstream>
static void parse_stream(std::shared_ptr<document> doc, tstream& str, parser& par)
{
	scanner<tstream> sc(str);

	int t = 0;
	int token_count = 0;

	while ((t = sc.get_token()) != TT_EOF && !par.is_stack_empty())
	{
		token_count++;

		switch (t)
		{
		case TT_CDATA_START:
			par.parse_cdata_start();
			break;
		case TT_CDATA_END:
			par.parse_cdata_end();
			break;
		case TT_COMMENT_START:
			par.parse_comment_start();
			break;
		case TT_COMMENT_END:
			par.parse_comment_end();
			break;
		case TT_DATA:
			par.parse_data(sc.get_value());
			break;
		case TT_TAG_START:
			{
				std::string tmp_str = sc.get_tag_name();
				if (!tmp_str.empty() && tmp_str[0] != '!')
				{
					par.parse_tag_start(trim_lower(tmp_str));
				}
			}
			break;
		case TT_TAG_END_EMPTY:
		case TT_TAG_END:
			{
				par.parse_tag_end(trim_lower(sc.get_tag_name()));
			}
			break;
		case TT_ATTR:
			{
				par.parse_attribute(trim_lower(sc.get_attr_name()), sc.get_value());
			}
			break;
		case TT_WORD:
			par.parse_word(sc.get_value());
			break;
		case TT_SPACE:
			par.parse_space(sc.get_value());
			break;
		}
	}
}

template <class tstream>
static std::shared_ptr<document> create_from_stream(html_view& view, const std::string& url, tstream& str)
{
	auto doc = std::make_shared<document>(view);

	doc->set_base_url(url);
	parser par(*doc);

	parse_stream(doc, str, par);

	doc->load_master_stylesheet(load_resource_html(IDR_CSS_MASTER));
	doc->set_root(par.release_root());

	return doc;
}

template <class tstream>
static std::shared_ptr<document> parse_from_stream(html_view& view, const std::string& url, tstream& str)
{
	auto doc = std::make_shared<document>(view);

	doc->set_base_url(url);
	parser par(*doc);

	parse_stream(doc, str, par);

	doc->load_master_stylesheet(load_resource_html(IDR_CSS_MASTER));
	doc->m_parsed_root = par.release_root();

	return doc;
}

void document::set_root(std::unique_ptr<element> r)
{
	m_root = std::move(r);
	apply_stylesheet();
}

void document::update_styles(element* root_el)
{
	if (root_el)
	{
		root_el->parse_attributes();
	}
	m_styles.sort_selectors();

	if (!m_media_lists.empty())
	{
		media_features features;
		get_media_features(features);
		update_media_lists(features);
	}

	if (root_el)
	{
		root_el->apply_stylesheet(m_styles);
		root_el->parse_styles();
	}
}

void document::resolve_styles()
{
	update_styles(m_parsed_root.get());
}

void document::finalize()
{
	m_root = std::move(m_parsed_root);

	m_view.layout();
}

std::shared_ptr<document> document::create_from_utf16(html_view& view, const std::string& url, const std::string& str)
{
	default_instream si(str);
	return create_from_stream(view, url, si);
}

std::shared_ptr<document> document::create_from_utf8(html_view& view, const std::string& url, const std::string& str)
{
	utf8_instream si(str);
	return create_from_stream(view, url, si);
}

std::shared_ptr<document> document::parse_from_utf16(html_view& view, const std::string& url, const std::string& str)
{
	default_instream si(str);
	return parse_from_stream(view, url, si);
}

std::shared_ptr<document> document::parse_from_utf8(html_view& view, const std::string& url, const std::string& str)
{
	utf8_instream si(str);
	return parse_from_stream(view, url, si);
}

void document::add_stylesheet(const std::string& text, const std::string& baseurl, const std::string& media)
{
	auto media_list = media_query_list::create_from_string(media);
	m_styles.parse_stylesheet(text, baseurl, *this, media_list);
}

void document::apply_stylesheet()
{
	update_styles(m_root.get());
	auto pThis = shared_from_this();
	run_on_ui([pThis]() { pThis->m_view.layout(); });
}

HFONT document::add_font(const std::string& name_in, int size, const std::string& weight, const std::string& style,
                         const std::string& decoration, font_metrics* fm)
{
	HFONT ret = nullptr;
	auto name = name_in;

	if (name.empty() || is_equal(name.c_str(), "inherit"))
	{
		name = get_default_font_name();
	}

	if (!size)
	{
		size = get_default_font_size();
	}

	auto key = std::format("{}:{}:{}:{}:{}", name, size, weight, style, decoration);

	{
		std::lock_guard lock(m_fonts_mutex);
		const auto it = m_fonts.find(key);
		if (it != m_fonts.end())
		{
			if (fm) *fm = it->second.metrics;
			return it->second.font;
		}
	}

	{
		auto fs = static_cast<font_style>(value_index(style, font_style_strings, font_style_normal));

		int fw = value_index(weight, font_weight_strings, -1);
		if (fw >= 0)
		{
			switch (fw)
			{
			case font_weight_bold:
				fw = 700;
				break;
			case font_weight_bolder:
				fw = 600;
				break;
			case font_weight_lighter:
				fw = 300;
				break;
			default:
				fw = 400;
				break;
			}
		}
		else
		{
			fw = safe_stoi(weight, 400);
			if (fw < 100)
			{
				fw = 400;
			}
		}

		unsigned int decor = 0;

		if (!decoration.empty())
		{
			auto tokens = split_string(decoration);

			for (const auto& tok : tokens)
			{
				if (is_equal(tok.c_str(), "underline"))
				{
					decor |= font_decoration_underline;
				}
				else if (is_equal(tok.c_str(), "line-through"))
				{
					decor |= font_decoration_linethrough;
				}
				else if (is_equal(tok.c_str(), "overline"))
				{
					decor |= font_decoration_overline;
				}
			}
		}

		font_item fi = {nullptr};

		auto fonts = split_string(name, ',');
		trim(fonts[0]);

		LOGFONT lf;
		ZeroMemory(&lf, sizeof(lf));
		const auto wfont = to_utf16(fonts[0]);
		wcscpy_s(lf.lfFaceName, LF_FACESIZE, wfont.c_str());

		lf.lfHeight = -size;
		lf.lfWeight = fw;
		lf.lfItalic = fs == font_style_italic ? TRUE : FALSE;
		lf.lfCharSet = DEFAULT_CHARSET;
		lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
		lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
		lf.lfQuality = CLEARTYPE_QUALITY;
		lf.lfStrikeOut = decor & font_decoration_linethrough ? TRUE : FALSE;
		lf.lfUnderline = decor & font_decoration_underline ? TRUE : FALSE;

		fi.font = CreateFontIndirect(&lf);

		win_dc hdc(nullptr);
		auto oldFont = static_cast<HFONT>(SelectObject(hdc, fi.font));
		TEXTMETRIC tm;
		GetTextMetrics(hdc, &tm);
		SelectObject(hdc, oldFont);

		fi.metrics.height = tm.tmHeight;
		fi.metrics.x_height = tm.tmHeight;
		fi.metrics.ascent = tm.tmAscent;
		fi.metrics.descent = tm.tmDescent;
		fi.metrics.draw_spaces = true;

		{
			std::lock_guard lock(m_fonts_mutex);
			m_fonts[key] = fi;
		}
		ret = fi.font;

		if (fm)
		{
			*fm = fi.metrics;
		}
	}
	return ret;
}

HFONT document::get_font(const std::string& name_in, int size, const std::string& weight, const std::string& style,
                         const std::string& decoration, font_metrics* fm)
{
	auto name = name_in;

	if (name.empty() || is_equal(name, "inherit"))
	{
		name = get_default_font_name();
	}

	if (!size)
	{
		size = get_default_font_size();
	}

	const auto key = std::format("{}:{}:{}:{}:{}", name, size, weight, style, decoration);

	{
		std::lock_guard lock(m_fonts_mutex);
		const auto el = m_fonts.find(key);
		if (el != m_fonts.end())
		{
			if (fm) *fm = el->second.metrics;
			return el->second.font;
		}
	}

	return add_font(name, size, weight, style, decoration, fm);
}

element* document::add_root()
{
	if (!m_root)
	{
		m_root = std::make_unique<element>(*this, el_html);
		m_root->set_tag_name("html");
	}
	return m_root.get();
}

element* document::add_body()
{
	if (!m_root)
	{
		add_root();
	}
	auto el = std::make_unique<element>(*this, el_body);
	el->set_tag_name("body");
	auto raw = el.get();
	m_root->append_child(std::move(el));
	return raw;
}

int document::render(render_win32& renderer, const int max_width, const render_type rt)
{
	int ret = 0;
	if (m_root)
	{
		if (rt == render_fixed_only)
		{
			m_fixed_boxes.clear();
			m_root->render_positioned(renderer, rt);
		}
		else
		{
			ret = m_root->render(renderer, 0, 0, max_width);
			if (m_root->fetch_positioned())
			{
				m_fixed_boxes.clear();
				m_root->render_positioned(renderer, rt);
			}
			m_size.width = 0;
			m_size.height = 0;
			m_root->calc_document_size(m_size);
		}
	}
	return ret;
}

void document::draw(render_win32& renderer, const int x, const int y, const position* clip)
{
	if (m_root)
	{
		m_root->draw(renderer, x, y, clip);
		m_root->draw_stacking_context(renderer, x, y, clip, true);
	}
}

int document::cvt_units(const std::string& str, const int fontSize, bool* is_percent/*= 0*/)
{
	if (str.empty()) return 0;

	css_length val;
	val.fromString(str);

	if (is_percent && val.units() == css_units_percentage && !val.is_predefined())
	{
		*is_percent = true;
	}
	return cvt_units(val, fontSize);
}

int document::cvt_units(css_length& val, const int fontSize, const int size)
{
	if (val.is_predefined())
	{
		return 0;
	}
	int ret = 0;
	switch (val.units())
	{
	case css_units_percentage:
		ret = val.calc_percent(size);
		break;
	case css_units_em:
		ret = round_f(val.val() * fontSize);
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	case css_units_rem:
		ret = round_f(val.val() * get_default_font_size());
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	case css_units_vw:
		ret = round_f(val.val() * GetSystemMetrics(SM_CXSCREEN) / 100.0f);
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	case css_units_vh:
		ret = round_f(val.val() * GetSystemMetrics(SM_CYSCREEN) / 100.0f);
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	case css_units_vmin:
		ret = round_f(val.val() * std::min(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)) / 100.0f);
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	case css_units_vmax:
		ret = round_f(val.val() * std::max(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)) / 100.0f);
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	case css_units_pt:
		ret = pt_to_px(static_cast<int>(val.val()));
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	case css_units_in:
		ret = pt_to_px(static_cast<int>(val.val() * 72));
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	case css_units_cm:
		ret = pt_to_px(static_cast<int>(val.val() * 0.3937 * 72));
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	case css_units_mm:
		ret = pt_to_px(static_cast<int>(val.val() * 0.3937 * 72) / 10);
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	default:
		ret = static_cast<int>(val.val());
		break;
	}
	return ret;
}

int document::width() const
{
	return m_size.width;
}

int document::height() const
{
	return m_size.height;
}

bool document::on_mouse_over(const int x, const int y, const int client_x, const int client_y,
                             position::vector& redraw_boxes)
{
	if (!m_root)
	{
		return false;
	}

	element* over_el = m_root->get_element_by_point(x, y, client_x, client_y);

	bool state_was_changed = false;

	if (over_el != m_over_element)
	{
		if (m_over_element)
		{
			if (m_over_element->on_mouse_leave())
			{
				state_was_changed = true;
			}
		}
		m_over_element = over_el;
		if (m_over_element)
		{
			if (m_over_element->on_mouse_over())
			{
				state_was_changed = true;
			}
		}
	}

	std::string cursor = "auto";

	if (m_over_element)
	{
		cursor = m_over_element->get_cursor();
	}

	set_cursor(cursor);

	if (state_was_changed)
	{
		return m_root->find_styles_changes(redraw_boxes, 0, 0);
	}

	return false;
}

bool document::on_mouse_leave(position::vector& redraw_boxes)
{
	if (!m_root)
	{
		return false;
	}
	if (m_over_element)
	{
		if (m_over_element->on_mouse_leave())
		{
			return m_root->find_styles_changes(redraw_boxes, 0, 0);
		}
	}
	return false;
}

bool document::on_lbutton_down(const int x, const int y, const int client_x, const int client_y,
                               position::vector& redraw_boxes)
{
	if (!m_root)
	{
		return false;
	}

	element* over_el = m_root->get_element_by_point(x, y, client_x, client_y);

	bool state_was_changed = false;

	if (over_el != m_over_element)
	{
		if (m_over_element)
		{
			if (m_over_element->on_mouse_leave())
			{
				state_was_changed = true;
			}
		}
		m_over_element = over_el;
		if (m_over_element)
		{
			if (m_over_element->on_mouse_over())
			{
				state_was_changed = true;
			}
		}
	}

	std::string cursor = "auto";

	if (m_over_element)
	{
		if (m_over_element->on_lbutton_down())
		{
			state_was_changed = true;
		}
		cursor = m_over_element->get_cursor();
	}

	set_cursor(cursor);

	if (state_was_changed)
	{
		return m_root->find_styles_changes(redraw_boxes, 0, 0);
	}

	return false;
}

bool document::on_lbutton_up(int x, int y, int client_x, int client_y, position::vector& redraw_boxes)
{
	auto root = m_root;
	if (!root)
	{
		return false;
	}
	if (m_over_element)
	{
		if (m_over_element->on_lbutton_up())
		{
			return root->find_styles_changes(redraw_boxes, 0, 0);
		}
	}
	return false;
}

void document::add_fixed_box(const position& pos)
{
	m_fixed_boxes.push_back(pos);
}

bool document::media_changed()
{
	if (!m_media_lists.empty())
	{
		media_features features;
		get_media_features(features);

		if (update_media_lists(features))
		{
			m_root->refresh_styles();
			m_root->parse_styles();
			return true;
		}
	}
	return false;
}

bool document::update_media_lists(const media_features& features)
{
	bool update_styles = false;

	for (const auto& ml : m_media_lists)
	{
		if (ml->apply_media_features(features))
		{
			update_styles = true;
		}
	}

	return update_styles;
}

void document::add_media_list(const std::shared_ptr<media_query_list>& list)
{
	if (list)
	{
		if (std::find(m_media_lists.begin(), m_media_lists.end(), list) == m_media_lists.end())
		{
			m_media_lists.push_back(list);
		}
	}
}


void document::set_caption(const std::string& caption)
{
	m_caption = caption;
}

void document::set_base_url(const std::string& base_url)
{
	if (!base_url.empty())
	{
		if (PathIsRelativeA(base_url.c_str()) && !PathIsURLA(base_url.c_str()))
		{
			m_base_path = make_url(base_url, m_url);
		}
		else
		{
			m_base_path = base_url;
		}
	}
	else
	{
		//m_base_path = m_url;
	}
}

void document::link(const element* el)
{
	const auto& rel = el->get_attr("rel");

	if (rel == "stylesheet")
	{
		const auto& href = el->get_attr("href");
		if (href.empty()) return;

		const auto media = el->get_attr("media", "all");
		// Trigger an asynchronous CSS download. import_css already handles base path
		// resolution, parsing, and applying the rules to the live DOM.
		import_css(href, m_base_path, media);
	}
}

void document::import_css(const std::string& url, const std::string& baseurl, const std::string& media)
{
	auto base_path = baseurl;

	if (base_path.empty())
	{
		base_path = m_base_path;
	}

	auto css_url = make_url(url, base_path);
	auto pThis = shared_from_this();

	m_http.download_file(css_url, std::make_shared<http_request>(
		                     [pThis, css_url, media](const std::string& file_name, const DWORD error,
		                                             const DWORD httpStatus,
		                                             const std::string& /*reqUrl*/)
		                     {
			                     if (error || httpStatus >= 400) return;
			                     const auto css_text = get_file_contents(file_name);
			                     if (css_text.empty()) return;

			                     // Stylesheet parsing and selector sort can be slow; do them on the
			                     // background thread, then apply on the UI thread once.
			                     run_async([pThis, css_url, css_text, media]()
			                     {
				                     pThis->add_stylesheet(css_text, css_url, media);
				                     pThis->m_styles.sort_selectors();

				                     run_on_ui([pThis]()
				                     {
					                     if (pThis->m_root)
					                     {
						                     pThis->m_root->apply_stylesheet(pThis->m_styles);
						                     pThis->m_root->parse_styles();
					                     }
					                     // Defer layout: just invalidate. The next paint or
					                     // window event will trigger layout if needed.
					                     pThis->m_view.invalidate();
				                     });
			                     });
		                     }));
}

void document::on_anchor_click(const std::string& url, element* el)
{
	auto full = make_url(url, m_base_path);
	html_view* view = &m_view;
	run_on_ui([view, full] { view->open(full); });
}

void document::set_cursor(const std::string& cursor)
{
	m_cursor = cursor;
}

void document::load_image(const std::string& url, const std::string& base)
{
	auto image_url = make_url(url, base);
	auto pThis = shared_from_this();

	if (!m_images.contains(image_url))
	{
		m_images[image_url] = nullptr; // Indicate loading

		m_http.download_file(image_url, std::make_shared<http_request>(
			                     [pThis, image_url](const std::string& file_name, const DWORD error,
			                                        const DWORD httpStatus, const std::string& reqUrl)
			                     {
				                     if (error || httpStatus >= 400)
				                     {
					                     return;
				                     }
				                     pThis->m_images[image_url] = std::make_shared<Gdiplus::Bitmap>(to_utf16(file_name).c_str());
				                     // Re-layout only if the image differs in size from the placeholder; for
				                     // simplicity request layout once per page-load tick by invalidating. Many
				                     // pages (e.g. Wikipedia) have hundreds of images and a full re-layout per
				                     // image effectively starves the UI thread.
				                     pThis->m_view.invalidate();
			                     }));
	}
}

std::shared_ptr<Gdiplus::Bitmap> document::find_image(const std::string& url)
{
	const auto found = m_images.find(url);
	return found != m_images.end() ? found->second : nullptr;
}

std::shared_ptr<Gdiplus::Bitmap> document::find_image(const std::string& url, const std::string& base)
{
	return find_image(make_url(url, base));
}

bool document::is_image_cached(const std::string& src, const std::string& baseurl)
{
	const auto url = make_url(src, baseurl);
	return m_images.contains(url);
}


void document::delete_font(const HFONT hFont)
{
	DeleteObject(hFont);
}

int document::text_width(const std::string& text, const HFONT hFont)
{
	win_dc hdc(nullptr);
	const auto oldFont = static_cast<HFONT>(SelectObject(hdc, hFont));

	SIZE sz = {0, 0};
	const auto wtext = to_utf16(text);
	GetTextExtentPoint32W(hdc, wtext.c_str(), static_cast<int>(wtext.size()), &sz);
	SelectObject(hdc, oldFont);

	return static_cast<int>(sz.cx);
}

int document::pt_to_px(const int pt)
{
	win_dc hdc(nullptr);
	return MulDiv(pt, GetDeviceCaps(hdc, LOGPIXELSY), 72);
}


void document::get_media_features(media_features& media)
{
	win_dc hdc(nullptr);

	media.type = media_type_screen;
	media.width = m_client_pos.width;
	media.height = m_client_pos.height;
	media.color = 8;
	media.monochrome = 0;
	media.color_index = 256;
	media.resolution = GetDeviceCaps(hdc, LOGPIXELSX);
	media.device_width = GetDeviceCaps(hdc, HORZRES);
	media.device_height = GetDeviceCaps(hdc, VERTRES);
}
