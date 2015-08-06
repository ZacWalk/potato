#pragma once

#include "attributes.h"
#include "strings.h"

class property_value
{
public:
    std::wstring m_value;
    bool m_important;

    property_value() : m_important(false)
    {
    }
    property_value(const std::wstring& val, bool imp) : m_important(imp), m_value(val)
    {
    }
    property_value(const property_value& other) : m_value(other.m_value), m_important(other.m_important)
    {
    }
    property_value(property_value && other) : m_important(other.m_important)
    {
        std::swap(m_value, other.m_value);
    }
    property_value& operator=(const property_value& other)
    {
        m_value = other.m_value;
        m_important = other.m_important;
        return *this;
    }
};


class style
{
private:
    std::map<std::wstring, property_value, ltstr> m_properties;

public:
    style()
    {
    }
    style(const style& other) : m_properties(other.m_properties)
    {
    }

    style(style && other) : m_properties(other.m_properties)
    {
        std::swap(m_properties, other.m_properties);
    }

    void operator=(const style& val)
    {
        m_properties = val.m_properties;
    }

    void add(const std::wstring& txt, const std::wstring& baseurl)
    {
        parse(txt, baseurl);
    }

    void add_property(const std::wstring& name, const std::wstring& val, const std::wstring& baseurl, bool important);

    const std::wstring get_property(const std::wstring& name) const
    {
        auto f = m_properties.find(name);

        if (f != m_properties.end())
        {
            return f->second.m_value;
        }

        return empty;
    }

    void combine(const style& src);

    void clear()
    {
        m_properties.clear();
    }

private:
    void parse_property(const std::wstring& txt, const std::wstring& baseurl);
    void parse_property(const std::wstring& name, const std::wstring& val, const std::wstring& baseurl);
    void parse(const std::wstring& txt, const std::wstring& baseurl);
    void parse_short_border(const std::wstring& key, const std::wstring& val, bool important);
    void parse_border_style(const wchar_t *style, const std::wstring& val, bool important);
    void parse_short_background(const std::wstring& val, const std::wstring& baseurl, bool important);
    void parse_short_font(const std::wstring& val, bool important);

    void add_parsed_property(const std::wstring& name, const std::wstring& val, bool important);
    void remove_property(const std::wstring& name, bool important);
};