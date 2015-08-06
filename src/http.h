#pragma once

#include "util.h"

class document;

class http_request : public std::enable_shared_from_this < http_request >
{
    friend class http;

protected:
    HINTERNET m_hConnection;
    HINTERNET m_hRequest;
    http *m_http;
    critical_section _sync;
    BYTE m_buffer[8192];
    DWORD m_error;
    ULONG64 m_content_length;
    ULONG64 m_downloaded_length;
    DWORD m_status;
    std::wstring m_url;
    std::wstring m_file;
    HANDLE m_hFile;

    typedef std::function<void(const std::wstring&)> Callback;
    Callback _callback;

public:
    http_request(const Callback &callback);
    ~http_request();

    void OnFinish(DWORD dwError, LPCWSTR errMsg);
    void OnData(LPCBYTE data, DWORD len, ULONG64 downloaded, ULONG64 total);
    void OnHeadersReady(HINTERNET hRequest);

    bool create(const std::wstring &url, HINTERNET hSession);
    void cancel();
    DWORD get_status_code() { return m_status; }
    ULONG64 get_content_length() { return m_content_length; }

protected:
    DWORD onSendRequestComplete();
    DWORD onHeadersAvailable();
    DWORD onHandleClosing();
    DWORD onRequestError(DWORD dwError);
    DWORD onReadComplete(DWORD len);
    DWORD readData();
    void set_parent(http* parent);
};

class http
{
    friend class http_request;

    HINTERNET m_hSession;
    std::vector<std::shared_ptr<http_request>> m_requests;
    critical_section _sync;
    DWORD  m_maxConnectionsPerServer;
public:
    http();
    ~http();

    void set_max_connections_per_server(DWORD max_con) { m_maxConnectionsPerServer = max_con; }
    bool open(LPCWSTR pwszUserAgent, DWORD dwAccessType, LPCWSTR pwszProxyName, LPCWSTR pwszProxyBypass);
    bool download_file(const std::wstring &url, const std::shared_ptr<http_request> &request);
    void stop();
    void close();

private:
    static VOID CALLBACK http_callback(HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength);

protected:
    void remove_request(const std::shared_ptr<http_request> &request);
};

