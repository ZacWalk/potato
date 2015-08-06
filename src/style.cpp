#include "pch.h"
#include "style.h"
#include "strings.h"
#include "web_color.h"

void style::parse(const std::wstring& txt, const std::wstring& baseurl)
{
    auto properties = split_string(txt, L":;");

    for (size_t i = 0; (i + 1) < properties.size(); i += 2)
    {
        parse_property(properties[i], properties[i + 1], baseurl);
    }
}

void style::parse_property(const std::wstring& txt, const std::wstring& baseurl)
{
    auto pos = txt.find_first_of(L':');

    if (pos != std::wstring::npos)
    {
        auto name = txt.substr(0, pos);
        auto val = txt.substr(pos + 1);

        trim(val);

        parse_property(trim_lower(name), val, baseurl);
    }
}

void style::parse_property(const std::wstring& name, const std::wstring& val, const std::wstring& baseurl)
{
    if (!name.empty() && !val.empty())
    {
        auto pos = val.find_first_of(L'!');

        if (pos == std::wstring::npos)
        {
            add_property(name, val, baseurl, false);
        }
        else 
        {
            auto left = val.substr(0, pos);
            auto right = val.substr(pos + 1);

            trim(left);

            add_property(name, left, baseurl, is_equal(right, L"important"));
        }
    }
}

void style::combine(const style& src)
{
    for (const auto &prop : src.m_properties)
    {
        add_parsed_property(prop.first, prop.second.m_value, prop.second.m_important);
    }
}

void style::add_property(const std::wstring& name, const std::wstring& val, const std::wstring& baseurl, bool important)
{
    if (name.empty() || val.empty())
    {
        return;
    }

    // Add baseurl for background image 
    if (name == _t("background-image"))
    {
        add_parsed_property(name, val, important);

        if (!baseurl.empty())
        {
            add_parsed_property(_t("background-image-baseurl"), baseurl, important);
        }
    }
    // Parse border spacing properties 
    else if (name == _t("border-spacing"))
    {
        auto tokens = split_string(val);

        if (tokens.size() == 1)
        {
            add_property(_t("-potato-border-spacing-x"), tokens[0], baseurl, important);
            add_property(_t("-potato-border-spacing-y"), tokens[0], baseurl, important);
        }
        else if (tokens.size() == 2)
        {
            add_property(_t("-potato-border-spacing-x"), tokens[0], baseurl, important);
            add_property(_t("-potato-border-spacing-y"), tokens[1], baseurl, important);
        }
    }
    // Parse borders shorthand properties 
    else if (name == _t("border"))
    {
        auto tokens = split_string(val, _t(" "), _t("("));

        for (const auto &tok : tokens)
        {
            auto idx = value_index(tok, border_style_strings, -1);

            if (idx >= 0)
            {
                add_property(_t("border-left-style"), tok, baseurl, important);
                add_property(_t("border-right-style"), tok, baseurl, important);
                add_property(_t("border-top-style"), tok, baseurl, important);
                add_property(_t("border-bottom-style"), tok, baseurl, important);
            }
            else
            {
                if (web_color::is_color(tok))
                {
                    add_property(_t("border-left-color"), tok, baseurl, important);
                    add_property(_t("border-right-color"), tok, baseurl, important);
                    add_property(_t("border-top-color"), tok, baseurl, important);
                    add_property(_t("border-bottom-color"), tok, baseurl, important);
                }
                else
                {
                    add_property(_t("border-left-width"), tok, baseurl, important);
                    add_property(_t("border-right-width"), tok, baseurl, important);
                    add_property(_t("border-top-width"), tok, baseurl, important);
                    add_property(_t("border-bottom-width"), tok, baseurl, important);
                }
            }
        }
    }
    else if (name == _t("border-left") ||
        name == _t("border-right") ||
        name == _t("border-top") ||
        name == _t("border-bottom"))
    {
        auto tokens = split_string(val, _t(" "), _t("("));

        for (const auto &tok : tokens)
        {
            auto idx = value_index(tok, border_style_strings, -1);

            if (idx >= 0)
            {
                auto k = name + _t("-style");
                add_property(k, tok, baseurl, important);
            }
            else
            {
                if (web_color::is_color(tok))
                {
                    auto k = name + _t("-color");
                    add_property(k, tok, baseurl, important);
                }
                else
                {
                    auto k = name + _t("-width");
                    add_property(k, tok, baseurl, important);
                }
            }
        }
    }
    // Parse border radius shorthand properties 
    else if (name == _t("border-bottom-left-radius"))
    {
        auto tokens = split_string(val);

        if (tokens.size() >= 2)
        {
            add_property(_t("border-bottom-left-radius-x"), tokens[0], baseurl, important);
            add_property(_t("border-bottom-left-radius-y"), tokens[1], baseurl, important);
        }
        else if (tokens.size() == 1)
        {
            add_property(_t("border-bottom-left-radius-x"), tokens[0], baseurl, important);
            add_property(_t("border-bottom-left-radius-y"), tokens[0], baseurl, important);
        }

    }
    else if (name == _t("border-bottom-right-radius"))
    {
        auto tokens = split_string(val);
        if (tokens.size() >= 2)
        {
            add_property(_t("border-bottom-right-radius-x"), tokens[0], baseurl, important);
            add_property(_t("border-bottom-right-radius-y"), tokens[1], baseurl, important);
        }
        else if (tokens.size() == 1)
        {
            add_property(_t("border-bottom-right-radius-x"), tokens[0], baseurl, important);
            add_property(_t("border-bottom-right-radius-y"), tokens[0], baseurl, important);
        }

    }
    else if (name == _t("border-top-right-radius"))
    {
        auto tokens = split_string(val);
        if (tokens.size() >= 2)
        {
            add_property(_t("border-top-right-radius-x"), tokens[0], baseurl, important);
            add_property(_t("border-top-right-radius-y"), tokens[1], baseurl, important);
        }
        else if (tokens.size() == 1)
        {
            add_property(_t("border-top-right-radius-x"), tokens[0], baseurl, important);
            add_property(_t("border-top-right-radius-y"), tokens[0], baseurl, important);
        }

    }
    else if (name == _t("border-top-left-radius"))
    {
        auto tokens = split_string(val);
        if (tokens.size() >= 2)
        {
            add_property(_t("border-top-left-radius-x"), tokens[0], baseurl, important);
            add_property(_t("border-top-left-radius-y"), tokens[1], baseurl, important);
        }
        else if (tokens.size() == 1)
        {
            add_property(_t("border-top-left-radius-x"), tokens[0], baseurl, important);
            add_property(_t("border-top-left-radius-y"), tokens[0], baseurl, important);
        }

    }
    // Parse border-radius shorthand properties 
    else if (name == _t("border-radius"))
    {
        auto tokens = split_string(val, L'/');
        if (tokens.size() == 1)
        {
            add_property(_t("border-radius-x"), tokens[0], baseurl, important);
            add_property(_t("border-radius-y"), tokens[0], baseurl, important);
        }
        else if (tokens.size() >= 2)
        {
            add_property(_t("border-radius-x"), tokens[0], baseurl, important);
            add_property(_t("border-radius-y"), tokens[1], baseurl, important);
        }
    }
    else if (name == _t("border-radius-x"))
    {
        auto tokens = split_string(val);
        if (tokens.size() == 1)
        {
            add_property(_t("border-top-left-radius-x"), tokens[0], baseurl, important);
            add_property(_t("border-top-right-radius-x"), tokens[0], baseurl, important);
            add_property(_t("border-bottom-right-radius-x"), tokens[0], baseurl, important);
            add_property(_t("border-bottom-left-radius-x"), tokens[0], baseurl, important);
        }
        else if (tokens.size() == 2)
        {
            add_property(_t("border-top-left-radius-x"), tokens[0], baseurl, important);
            add_property(_t("border-top-right-radius-x"), tokens[1], baseurl, important);
            add_property(_t("border-bottom-right-radius-x"), tokens[0], baseurl, important);
            add_property(_t("border-bottom-left-radius-x"), tokens[1], baseurl, important);
        }
        else if (tokens.size() == 3)
        {
            add_property(_t("border-top-left-radius-x"), tokens[0], baseurl, important);
            add_property(_t("border-top-right-radius-x"), tokens[1], baseurl, important);
            add_property(_t("border-bottom-right-radius-x"), tokens[2], baseurl, important);
            add_property(_t("border-bottom-left-radius-x"), tokens[1], baseurl, important);
        }
        else if (tokens.size() == 4)
        {
            add_property(_t("border-top-left-radius-x"), tokens[0], baseurl, important);
            add_property(_t("border-top-right-radius-x"), tokens[1], baseurl, important);
            add_property(_t("border-bottom-right-radius-x"), tokens[2], baseurl, important);
            add_property(_t("border-bottom-left-radius-x"), tokens[3], baseurl, important);
        }
    }
    else if (name == _t("border-radius-y"))
    {
        auto tokens = split_string(val);
        if (tokens.size() == 1)
        {
            add_property(_t("border-top-left-radius-y"), tokens[0], baseurl, important);
            add_property(_t("border-top-right-radius-y"), tokens[0], baseurl, important);
            add_property(_t("border-bottom-right-radius-y"), tokens[0], baseurl, important);
            add_property(_t("border-bottom-left-radius-y"), tokens[0], baseurl, important);
        }
        else if (tokens.size() == 2)
        {
            add_property(_t("border-top-left-radius-y"), tokens[0], baseurl, important);
            add_property(_t("border-top-right-radius-y"), tokens[1], baseurl, important);
            add_property(_t("border-bottom-right-radius-y"), tokens[0], baseurl, important);
            add_property(_t("border-bottom-left-radius-y"), tokens[1], baseurl, important);
        }
        else if (tokens.size() == 3)
        {
            add_property(_t("border-top-left-radius-y"), tokens[0], baseurl, important);
            add_property(_t("border-top-right-radius-y"), tokens[1], baseurl, important);
            add_property(_t("border-bottom-right-radius-y"), tokens[2], baseurl, important);
            add_property(_t("border-bottom-left-radius-y"), tokens[1], baseurl, important);
        }
        else if (tokens.size() == 4)
        {
            add_property(_t("border-top-left-radius-y"), tokens[0], baseurl, important);
            add_property(_t("border-top-right-radius-y"), tokens[1], baseurl, important);
            add_property(_t("border-bottom-right-radius-y"), tokens[2], baseurl, important);
            add_property(_t("border-bottom-left-radius-y"), tokens[3], baseurl, important);
        }
    }
    // Parse list-style shorthand properties 
    else if (name == _t("list-style"))
    {
        add_parsed_property(_t("list-style-type"), _t("disc"), important);
        add_parsed_property(_t("list-style-position"), _t("outside"), important);
        add_parsed_property(_t("list-style-image"), empty, important);
        add_parsed_property(_t("list-style-image-baseurl"), empty, important);

        auto tokens = split_string(val, _t(" "), _t("("));

        for (const auto &tok : tokens)
        {
            int idx = value_index(tok, list_style_type_strings, -1);
            if (idx >= 0)
            {
                add_parsed_property(_t("list-style-type"), tok, important);
            }
            else
            {
                idx = value_index(tok, list_style_position_strings, -1);
                if (idx >= 0)
                {
                    add_parsed_property(_t("list-style-position"), tok, important);
                }
                else if (starts(val, _t("url")))
                {
                    add_parsed_property(_t("list-style-image"), tok, important);
                    if (!baseurl.empty())
                    {
                        add_parsed_property(_t("list-style-image-baseurl"), baseurl, important);
                    }
                }
            }
        }
    }
    // Add baseurl for background image 
    else if (name == _t("list-style-image"))
    {
        add_parsed_property(name, val, important);
        if (!baseurl.empty())
        {
            add_parsed_property(_t("list-style-image-baseurl"), baseurl, important);
        }
    }
    // Parse background shorthand properties 
    else if (name == _t("background"))
    {
        parse_short_background(val, baseurl, important);

    }
    // Parse margin and padding shorthand properties 
    else if (name == _t("margin") || name == _t("padding"))
    {
        auto tokens = split_string(val);
        if (tokens.size() >= 4)
        {
            add_parsed_property(name + _t("-top"), tokens[0], important);
            add_parsed_property(name + _t("-right"), tokens[1], important);
            add_parsed_property(name + _t("-bottom"), tokens[2], important);
            add_parsed_property(name + _t("-left"), tokens[3], important);
        }
        else if (tokens.size() == 3)
        {
            add_parsed_property(name + _t("-top"), tokens[0], important);
            add_parsed_property(name + _t("-right"), tokens[1], important);
            add_parsed_property(name + _t("-left"), tokens[1], important);
            add_parsed_property(name + _t("-bottom"), tokens[2], important);
        }
        else if (tokens.size() == 2)
        {
            add_parsed_property(name + _t("-top"), tokens[0], important);
            add_parsed_property(name + _t("-bottom"), tokens[0], important);
            add_parsed_property(name + _t("-right"), tokens[1], important);
            add_parsed_property(name + _t("-left"), tokens[1], important);
        }
        else if (tokens.size() == 1)
        {
            add_parsed_property(name + _t("-top"), tokens[0], important);
            add_parsed_property(name + _t("-bottom"), tokens[0], important);
            add_parsed_property(name + _t("-right"), tokens[0], important);
            add_parsed_property(name + _t("-left"), tokens[0], important);
        }
    }
    // Parse border-* shorthand properties 
    else if (name == _t("border-left") ||
        name == _t("border-right") ||
        name == _t("border-top") ||
        name == _t("border-bottom"))
    {
        parse_short_border(name, val, important);
    }
    // Parse border-width/style/color shorthand properties 
    else if (name == _t("border-width"))
    {
        parse_border_style(L"width", val, important);
    }
    else if (name == _t("border-style"))
    {
        parse_border_style(L"style", val, important);
    }
    else if (name == _t("border-color"))
    {
        parse_border_style(L"color", val, important);
    }
    // Parse font shorthand properties 
    else if (name == _t("font"))
    {
        parse_short_font(val, important);
    }
    else
    {
        add_parsed_property(name, val, important);
    }
}

void style::parse_short_border(const std::wstring& key, const std::wstring& val, bool important)
{
    auto tokens = split_string(val, _t(" "), _t("("));

    if (tokens.size() >= 3)
    {
        add_parsed_property(key + _t("-width"), tokens[0], important);
        add_parsed_property(key + _t("-style"), tokens[1], important);
        add_parsed_property(key + _t("-color"), tokens[2], important);
    }
    else if (tokens.size() == 2)
    {
        if (iswdigit(tokens[0][0]) || value_index(val, border_width_strings) >= 0)
        {
            add_parsed_property(key + _t("-width"), tokens[0], important);
            add_parsed_property(key + _t("-style"), tokens[1], important);
        }
        else
        {
            add_parsed_property(key + _t("-style"), tokens[0], important);
            add_parsed_property(key + _t("-color"), tokens[1], important);
        }
    }
}

void style::parse_border_style(const wchar_t *style, const std::wstring& val, bool important)
{
    std::wstring key(L"border");

    auto tokens = split_string(val);

    if (tokens.size() >= 4)
    {
        add_parsed_property(key + _t("-top-") + style, tokens[0], important);
        add_parsed_property(key + _t("-right-") + style, tokens[1], important);
        add_parsed_property(key + _t("-bottom-") + style, tokens[2], important);
        add_parsed_property(key + _t("-left-") + style, tokens[3], important);
    }
    else if (tokens.size() == 3)
    {
        add_parsed_property(key + _t("-top-") + style, tokens[0], important);
        add_parsed_property(key + _t("-right-") + style, tokens[1], important);
        add_parsed_property(key + _t("-left-") + style, tokens[1], important);
        add_parsed_property(key + _t("-bottom-") + style, tokens[2], important);
    }
    else if (tokens.size() == 2)
    {
        add_parsed_property(key + _t("-top-") + style, tokens[0], important);
        add_parsed_property(key + _t("-bottom-") + style, tokens[0], important);
        add_parsed_property(key + _t("-right-") + style, tokens[1], important);
        add_parsed_property(key + _t("-left-") + style, tokens[1], important);
    }
    else if (tokens.size() == 1)
    {
        add_parsed_property(key + _t("-top-") + style, tokens[0], important);
        add_parsed_property(key + _t("-bottom-") + style, tokens[0], important);
        add_parsed_property(key + _t("-right-") + style, tokens[0], important);
        add_parsed_property(key + _t("-left-") + style, tokens[0], important);
    }
}

void style::parse_short_background(const std::wstring& val, const std::wstring& baseurl, bool important)
{
    add_parsed_property(_t("background-color"), _t("transparent"), important);
    add_parsed_property(_t("background-image"), empty, important);
    add_parsed_property(_t("background-image-baseurl"), empty, important);
    add_parsed_property(_t("background-repeat"), _t("repeat"), important);
    add_parsed_property(_t("background-origin"), _t("padding-box"), important);
    add_parsed_property(_t("background-clip"), _t("border-box"), important);
    add_parsed_property(_t("background-attachment"), _t("scroll"), important);

    if (val == _t("none"))
    {
        return;
    }

    auto tokens = split_string(val, _t(" "), _t("("));
    auto origin_found = false;

    for (const auto &tok : tokens)
    {
        if (web_color::is_color(tok))
        {
            add_parsed_property(_t("background-color"), tok, important);
        }
        else if (tok.substr(0, 3) == _t("url"))
        {
            add_parsed_property(_t("background-image"), tok, important);
            if (!baseurl.empty())
            {
                add_parsed_property(_t("background-image-baseurl"), baseurl, important);
            }

        }
        else if (value_in_list(tok, background_repeat_strings))
        {
            add_parsed_property(_t("background-repeat"), tok, important);
        }
        else if (value_in_list(tok, background_attachment_strings))
        {
            add_parsed_property(_t("background-attachment"), tok, important);
        }
        else if (value_in_list(tok, background_box_strings))
        {
            if (!origin_found)
            {
                add_parsed_property(_t("background-origin"), tok, important);
                origin_found = true;
            }
            else
            {
                add_parsed_property(_t("background-clip"), tok, important);
            }
        }
        else if (value_in_list(tok, _t("left;right;top;bottom;center")) ||
            iswdigit((tok)[0]) ||
            tok[0] == _t('-') ||
            tok[0] == _t('.') ||
            tok[0] == _t('+'))
        {
            if (m_properties.find(_t("background-position")) != m_properties.end())
            {
                m_properties[_t("background-position")].m_value = m_properties[_t("background-position")].m_value + _t(" ") + tok;
            }
            else
            {
                add_parsed_property(_t("background-position"), tok, important);
            }
        }
    }
}

void style::parse_short_font(const std::wstring& val, bool important)
{
    add_parsed_property(_t("font-style"), _t("normal"), important);
    add_parsed_property(_t("font-variant"), _t("normal"), important);
    add_parsed_property(_t("font-weight"), _t("normal"), important);
    add_parsed_property(_t("font-size"), _t("medium"), important);
    add_parsed_property(_t("line-height"), _t("normal"), important);

    auto tokens = split_string(val, _t(" "), _t("\""));

    int idx = 0;
    bool was_normal = false;
    bool is_family = false;
    std::wstring font_family;

    for (const auto &tok : tokens)
    {
        idx = value_index(tok, font_style_strings);

        if (!is_family)
        {
            if (idx >= 0)
            {
                if (idx == 0 && !was_normal)
                {
                    add_parsed_property(_t("font-weight"), tok, important);
                    add_parsed_property(_t("font-variant"), tok, important);
                    add_parsed_property(_t("font-style"), tok, important);
                }
                else
                {
                    add_parsed_property(_t("font-style"), tok, important);
                }
            }
            else
            {
                if (value_in_list(tok, font_weight_strings))
                {
                    add_parsed_property(_t("font-weight"), tok, important);
                }
                else
                {
                    if (value_in_list(tok, font_variant_strings))
                    {
                        add_parsed_property(_t("font-variant"), tok, important);
                    }
                    else if (iswdigit(tok[0]))
                    {
                        auto szlh = split_string(tok, L'/');

                        if (szlh.size() == 1)
                        {
                            add_parsed_property(_t("font-size"), szlh[0], important);
                        }
                        else if (szlh.size() >= 2)
                        {
                            add_parsed_property(_t("font-size"), szlh[0], important);
                            add_parsed_property(_t("line-height"), szlh[1], important);
                        }
                    }
                    else
                    {
                        is_family = true;
                        font_family += tok;
                    }
                }
            }
        }
        else
        {
            font_family += tok;
        }
    }

    add_parsed_property(_t("font-family"), font_family, important);
}

void style::add_parsed_property(const std::wstring& name, const std::wstring& val_in, bool important)
{
    auto val = val_in;

    // unquot 
    if (val.size() > 1 && (val.front() == val.back()) && (val.front() == '"'))
    {
        val.erase(0, 1);
        val.pop_back();
    }

    auto found = m_properties.find(name);

    if (found != m_properties.end())
    {
        if (!found->second.m_important || (important && found->second.m_important))
        {
            found->second.m_value = val;
            found->second.m_important = important;
        }
    }
    else
    {
        m_properties[name] = property_value(val, important);

        //ATLTRACE(L"m_properties[%s] = %s\n", name.str().c_str(), val.c_str());
    }
}

void style::remove_property(const std::wstring& name, bool important)
{
    auto found = m_properties.find(name);

    if (found != m_properties.end())
    {
        if (!found->second.m_important || (important && found->second.m_important))
        {
            m_properties.erase(found);
        }
    }
}
