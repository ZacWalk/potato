#pragma once

class web_history
{
    std::vector<std::wstring> m_items;
    std::vector<std::wstring>::size_type m_current_item;
public:
    web_history();
    virtual ~web_history();

    void url_opened(const std::wstring& url);
    bool back(std::wstring& url);
    bool forward(std::wstring& url);
};