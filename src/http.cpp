#include "pch.h"
#include "http.h"
#include "document.h"
#include "switch.h"
#include "strings.h"

#pragma comment(lib, "winhttp.lib")

http::http()
{
    m_hSession = nullptr;
    m_maxConnectionsPerServer = 5;
}

http::~http()
{
    stop();

    if (m_hSession)
    {
        WinHttpCloseHandle(m_hSession);
    }
}

bool http::open(LPCWSTR pwszUserAgent, DWORD dwAccessType, LPCWSTR pwszProxyName, LPCWSTR pwszProxyBypass)
{
    m_hSession = WinHttpOpen(pwszUserAgent, dwAccessType, pwszProxyName, pwszProxyBypass, WINHTTP_FLAG_ASYNC);
    if (m_hSession)
    {
        WinHttpSetOption(m_hSession, WINHTTP_OPTION_MAX_CONNS_PER_SERVER, &m_maxConnectionsPerServer, sizeof(m_maxConnectionsPerServer));

        if (WinHttpSetStatusCallback(m_hSession, (WINHTTP_STATUS_CALLBACK) http::http_callback, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0) != WINHTTP_INVALID_STATUS_CALLBACK)
        {
            return TRUE;
        }
    }
    if (m_hSession)
    {
        WinHttpCloseHandle(m_hSession);
    }
    return FALSE;
}

void http::close()
{
    if (m_hSession)
    {
        WinHttpCloseHandle(m_hSession);
        m_hSession = nullptr;
    }
}

VOID CALLBACK http::http_callback(HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength)
{
    DWORD dwError = ERROR_SUCCESS;
    http_request* request = (http_request*) dwContext;

    if (request)
    {
        switch (dwInternetStatus)
        {

        case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
            dwError = request->onSendRequestComplete();
            break;

        case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
            dwError = request->onHeadersAvailable();
            break;

        case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
            dwError = request->onReadComplete(dwStatusInformationLength);
            break;

        case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
            dwError = request->onHandleClosing();
            break;

        case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
            /*dwError = */request->onRequestError(((WINHTTP_ASYNC_RESULT*) lpvStatusInformation)->dwError);
            break;

        }

        if (dwError != ERROR_SUCCESS)
        {
            request->cancel();
        }
    }
}

bool http::download_file(const std::wstring &url, const std::shared_ptr<http_request> &request)
{
    ATLTRACE(L"Download %s\n", url.c_str());

    if (request)
    {
        request->set_parent(this);
        if (request->create(url, m_hSession))
        {
            critical_section_lock lock(_sync);
            m_requests.push_back(request);
            return true;
        }
    }

    return false;
}

void http::remove_request(const std::shared_ptr<http_request> &request)
{
    critical_section_lock lock(_sync);
    for (auto i = m_requests.begin(); i != m_requests.end(); i++)
    {
        if ((*i) == request)
        {
            m_requests.erase(i);
            break;
        }
    }

}

void http::stop()
{
    critical_section_lock lock(_sync);

    for (auto i = m_requests.begin(); i != m_requests.end(); i++)
    {
        (*i)->cancel();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

http_request::http_request(const Callback &callback) : _callback(callback)
{
    m_status = 0;
    m_error = 0;
    m_downloaded_length = 0;
    m_content_length = 0;
    m_hConnection = nullptr;
    m_hRequest = nullptr;
    m_http = nullptr;

    WCHAR folder[MAX_PATH];
    WCHAR path[MAX_PATH];

    GetTempPath(MAX_PATH, folder);
    GetTempFileName(folder, L"lbr", 0, path);

    m_file = path;
    m_hFile = CreateFile(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

http_request::~http_request()
{
    cancel();

    if (m_hFile)
    {
        CloseHandle(m_hFile);
        m_hFile = nullptr;
    }
}

bool http_request::create(const std::wstring &url_in, HINTERNET hSession)
{
    m_url = url_in;
    m_error = ERROR_SUCCESS;

    if (!starts(m_url, L"http://") && !starts(m_url, L"https://"))
    {
        m_url = L"http://" + m_url;
    }

    URL_COMPONENTS urlComp;

    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);

    urlComp.dwSchemeLength = -1;
    urlComp.dwHostNameLength = -1;
    urlComp.dwUrlPathLength = -1;
    urlComp.dwExtraInfoLength = -1;

    if (!WinHttpCrackUrl(m_url.c_str(), m_url.size(), 0, &urlComp))
    {
        return FALSE;
    }

    std::wstring host;
    std::wstring path;
    std::wstring extra;

    host.insert(0, urlComp.lpszHostName, urlComp.dwHostNameLength);
    path.insert(0, urlComp.lpszUrlPath, urlComp.dwUrlPathLength);

    if (urlComp.dwExtraInfoLength)
    {
        extra.insert(0, urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
    }

    DWORD flags = 0;
    if (urlComp.nScheme == INTERNET_SCHEME_HTTPS)
    {
        flags = WINHTTP_FLAG_SECURE;
    }

    m_hConnection = WinHttpConnect(hSession, host.c_str(), urlComp.nPort, 0);

    PCWSTR pwszAcceptTypes [] = { L"*/*", nullptr };

    path += extra;

    m_hRequest = WinHttpOpenRequest(m_hConnection, L"GET", path.c_str(), nullptr, nullptr, pwszAcceptTypes, flags);

    critical_section_lock lock(_sync);
    if (!m_hRequest)
    {
        WinHttpCloseHandle(m_hConnection);
        m_hConnection = nullptr;

        return FALSE;
    }

    if (!WinHttpSendRequest(m_hRequest, nullptr, 0, nullptr, 0, 0, (DWORD_PTR) this))
    {
        WinHttpCloseHandle(m_hRequest);
        m_hRequest = nullptr;
        WinHttpCloseHandle(m_hConnection);
        m_hConnection = nullptr;

        return FALSE;
    }


    return TRUE;
}

void http_request::cancel()
{
    critical_section_lock lock(_sync);
    if (m_hRequest)
    {
        WinHttpCloseHandle(m_hRequest);
        m_hRequest = nullptr;
    }
    if (m_hConnection)
    {
        WinHttpCloseHandle(m_hConnection);
        m_hConnection = nullptr;
    }

}

DWORD http_request::onSendRequestComplete()
{
    critical_section_lock lock(_sync);
    DWORD dwError = ERROR_SUCCESS;

    if (!WinHttpReceiveResponse(m_hRequest, nullptr))
    {
        dwError = GetLastError();
    }



    return dwError;
}

DWORD http_request::onHeadersAvailable()
{
    critical_section_lock lock(_sync);

    DWORD dwError = ERROR_SUCCESS;
    m_status = 0;
    DWORD StatusCodeLength = sizeof(m_status);

    OnHeadersReady(m_hRequest);

    if (!WinHttpQueryHeaders(m_hRequest, WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE, nullptr, &m_status, &StatusCodeLength, nullptr))
    {
        dwError = GetLastError();
    }
    else
    {
        WCHAR buf[255];
        DWORD len = sizeof(buf);
        if (WinHttpQueryHeaders(m_hRequest, WINHTTP_QUERY_CONTENT_LENGTH, nullptr, buf, &len, nullptr))
        {
            m_content_length = _wtoi64(buf);
        }
        else
        {
            m_content_length = 0;
        }
        m_downloaded_length = 0;

        dwError = readData();
    }



    return dwError;
}

DWORD http_request::onHandleClosing()
{
    WCHAR errMsg[255];
    errMsg[0] = 0;

    if (m_error)
    {
        FormatMessage(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM, GetModuleHandle(L"winhttp.dll"), m_error, 0, errMsg, 255, nullptr);
    }

    OnFinish(m_error, errMsg);
    m_http->remove_request(shared_from_this());

    return ERROR_SUCCESS;
}

DWORD http_request::onRequestError(DWORD dwError)
{
    m_error = dwError;
    return m_error;
}

DWORD http_request::readData()
{
    DWORD dwError = ERROR_SUCCESS;

    if (!WinHttpReadData(m_hRequest, m_buffer, sizeof(m_buffer), nullptr))
    {
        dwError = GetLastError();
    }

    return dwError;
}

DWORD http_request::onReadComplete(DWORD len)
{
    DWORD dwError = ERROR_SUCCESS;

    if (len != 0)
    {
        critical_section_lock lock(_sync);
        m_downloaded_length += len;
        OnData(m_buffer, len, m_downloaded_length, m_content_length);
        dwError = readData();

    }
    else
    {
        cancel();
    }

    return dwError;
}

void http_request::set_parent(http* parent)
{
    m_http = parent;
}

void http_request::OnHeadersReady(HINTERNET hRequest)
{

}


void http_request::OnFinish(DWORD dwError, LPCWSTR errMsg)
{
    if (m_hFile)
    {
        CloseHandle(m_hFile);
        m_hFile = nullptr;
    }

    if (dwError)
    {
    }
    else
    {
        auto cb = _callback;
        auto f = m_file;

        Switch([cb, f]() { cb(f); });
    }
}

void http_request::OnData(LPCBYTE data, DWORD len, ULONG64 downloaded, ULONG64 total)
{
    if (m_hFile)
    {
        DWORD cbWritten = 0;
        WriteFile(m_hFile, data, len, &cbWritten, nullptr);
    }
}

