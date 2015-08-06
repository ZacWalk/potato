#include "pch.h"
#include "stylesheet.h"
#include "document.h"
#include "strings.h"

void css_element_selector::parse(const std::wstring& text)
{
    //ATLTRACE(L"Parse element selector: %s\n", text.c_str());

    auto el_end = text.find_first_of(_t(".#[:"));

    m_tag = trim_lower(text.substr(0, el_end));

    while (el_end != std::wstring::npos)
    {
        if (text[el_end] == _t('.'))
        {
            css_attribute_selector attribute;

            auto pos = text.find_first_of(_t(".#[:"), el_end + 1);
            attribute.val = text.substr(el_end + 1, pos - el_end - 1);
            attribute.condition = select_equal;
            attribute.attribute = _t("class");
            m_attrs.push_back(attribute);
            el_end = pos;
        }
        else if (text[el_end] == _t(':'))
        {
            css_attribute_selector attribute;

            if (text[el_end + 1] == _t(':'))
            {
                auto pos = text.find_first_of(_t(".#[:"), el_end + 2);
                attribute.val = trim_lower(text.substr(el_end + 2, pos - el_end - 2));
                attribute.condition = select_pseudo_element;
                attribute.attribute = _t("pseudo-el");
                m_attrs.push_back(attribute);
                el_end = pos;
            }
            else
            {
                auto pos = text.find_first_of(_t(".#[:("), el_end + 1);
                if (pos != std::wstring::npos && text.at(pos) == _t('('))
                {
                    pos = find_close_bracket(text, pos);
                    if (pos != std::wstring::npos)
                    {
                        pos++;
                    }
                    else
                    {
                        int iii = 0;
                        iii++;
                    }
                }
                if (pos != std::wstring::npos)
                {
                    attribute.val = trim_lower(text.substr(el_end + 1, pos - el_end - 1));
                }
                else
                {
                    attribute.val = trim_lower(text.substr(el_end + 1));
                }

                if (attribute.val == _t("after") || attribute.val == _t("before"))
                {
                    attribute.condition = select_pseudo_element;
                }
                else
                {
                    attribute.condition = select_pseudo_class;
                }
                attribute.attribute = _t("pseudo");
                m_attrs.push_back(attribute);
                el_end = pos;
            }
        }
        else if (text[el_end] == _t('#'))
        {
            css_attribute_selector attribute;

            auto pos = text.find_first_of(_t(".#[:"), el_end + 1);
            attribute.val = text.substr(el_end + 1, pos - el_end - 1);
            attribute.condition = select_equal;
            attribute.attribute = _t("id");
            m_attrs.push_back(attribute);
            el_end = pos;
        }
        else if (text[el_end] == _t('['))
        {
            css_attribute_selector attribute;

            auto pos = text.find_first_of(_t("]~=|$*^"), el_end + 1);
            std::wstring attr = trim_lower(text.substr(el_end + 1, pos - el_end - 1));

            if (pos != std::wstring::npos)
            {
                if (text[pos] == _t(']'))
                {
                    attribute.condition = select_exists;
                }
                else if (text[pos] == _t('='))
                {
                    attribute.condition = select_equal;
                    pos++;
                }
                else if (text.substr(pos, 2) == _t("~="))
                {
                    attribute.condition = select_contain_str;
                    pos += 2;
                }
                else if (text.substr(pos, 2) == _t("|="))
                {
                    attribute.condition = select_start_str;
                    pos += 2;
                }
                else if (text.substr(pos, 2) == _t("^="))
                {
                    attribute.condition = select_start_str;
                    pos += 2;
                }
                else if (text.substr(pos, 2) == _t("$="))
                {
                    attribute.condition = select_end_str;
                    pos += 2;
                }
                else if (text.substr(pos, 2) == _t("*="))
                {
                    attribute.condition = select_contain_str;
                    pos += 2;
                }
                else
                {
                    attribute.condition = select_exists;
                    pos += 1;
                }
                pos = text.find_first_not_of(_t(" \t"), pos);
                if (pos != std::wstring::npos)
                {
                    if (text[pos] == _t('"'))
                    {
                        auto pos2 = text.find_first_of(_t("\""), pos + 1);
                        attribute.val = text.substr(pos + 1, pos2 == std::wstring::npos ? pos2 : (pos2 - pos - 2));
                        pos = pos2 == std::wstring::npos ? pos2 : (pos2 + 1);
                    }
                    else if (text[pos] == _t(']'))
                    {
                        pos++;
                    }
                    else
                    {
                        auto pos2 = text.find_first_of(_t("]"), pos + 1);
                        attribute.val = text.substr(pos, pos2 == std::wstring::npos ? pos2 : (pos2 - pos));
                        pos = pos2 == std::wstring::npos ? pos2 : (pos2 + 1);
                    }
                }
            }
            else
            {
                attribute.condition = select_exists;
            }
            attribute.attribute = attr;
            m_attrs.push_back(attribute);
            el_end = pos;
        }
        else
        {
            el_end++;
        }

        el_end = text.find_first_of(_t(".#[:"), el_end);
    }
}


bool css_selector::parse(const std::wstring& text)
{
    //ATLTRACE(L"Parse selector: %s\n", text.c_str());

    if (text.empty())
    {
        return false;
    }

    auto split = text.find_last_of(L" \t>+~");
    //tokenize(text, tokens, _t(""), _t(" \t>+~"), _t("()"));

    if (split == std::wstring::npos)
    {
        m_right.parse(trim_lower(text));
    }
    else
    {
        m_right.parse(trim_lower(text.substr(split + 1)));

        switch (text[split])
        {
        case _t('>'):
            m_combinator = combinator_child;
            break;
        case _t('+'):
            m_combinator = combinator_adjacent_sibling;
            break;
        case _t('~'):
            m_combinator = combinator_general_sibling;
            break;
        default:
            m_combinator = combinator_descendant;
            break;
        }

        if (split > 0)
        {
            auto left = trim_lower(text.substr(0, split));

            if (!left.empty())
            {
                m_left = std::make_shared<css_selector>(nullptr, std::shared_ptr<media_query_list>());

                if (!m_left->parse(left))
                {
                    return false;
                }
            }
        }
    }

    return true;
}

void css_selector::calc_specificity()
{
    if (!m_right.m_tag.empty() && m_right.m_tag != _t("*"))
    {
        m_specificity += 0x0001;
    }

    for (const auto &a : m_right.m_attrs)
    {
        if (a.attribute == _t("id"))
        {
            m_specificity += 0x0100;
        }
        else if (a.attribute == _t("class"))
        {
            auto tokens = split_string(a.val);
            m_specificity += tokens.size() * 0x0010;
        }
        else
        {
            m_specificity += 0x0010;
        }
    }

    if (m_left)
    {
        m_left->calc_specificity();
        m_specificity += m_left->m_specificity;
    }
}

void css_selector::add_media_to_doc(const std::shared_ptr<document> &doc) const
{
    if (m_media_query && doc)
    {
        doc->add_media_list(m_media_query);
    }
}

void css::parse_stylesheet(const std::wstring& str, const std::wstring& baseurl, document &doc, std::shared_ptr<media_query_list>& media)
{
    std::wstring text = str;

    // remove comments
    auto c_start = text.find(_t("/*"));

    while (c_start != std::wstring::npos)
    {
        auto c_end = text.find(_t("*/"), c_start + 2);
        text.erase(c_start, c_end - c_start + 2);
        c_start = text.find(_t("/*"));
    }

    auto pos = text.find_first_not_of(_t(" \n\r\t"));

    while (pos != std::wstring::npos)
    {
        while (pos != std::wstring::npos && text[pos] == _t('@'))
        {
            auto sPos = pos;
            pos = text.find_first_of(L'{', pos);

            if (pos != std::wstring::npos && text[pos] == _t('{'))
            {
                pos = find_close_bracket(text, pos, _t('{'), _t('}'));
            }
            if (pos != std::wstring::npos)
            {
                parse_atrule(text.substr(sPos, pos - sPos + 1), baseurl, doc, media);
            }
            else
            {
                parse_atrule(text.substr(sPos), baseurl, doc, media);
            }

            if (pos != std::wstring::npos)
            {
                pos = text.find_first_not_of(_t(" \n\r\t"), pos + 1);
            }
        }

        if (pos == std::wstring::npos)
        {
            break;
        }

        auto style_start = text.find_first_of(_t("{"), pos);
        auto style_end = text.find_first_of(_t("}"), pos);

        if (style_start != std::wstring::npos && style_end != std::wstring::npos)
        {
            auto selectors = text.substr(pos, style_start - pos);
            auto styles = text.substr(style_start + 1, style_end - style_start - 1);

            auto st = std::make_shared<style>();
            st->add(styles, baseurl);

            parse_selectors(selectors, st, media);

            if (media)
            {
                doc.add_media_list(media);
            }

            pos = style_end + 1;
        }
        else
        {
            pos = std::wstring::npos;
        }

        if (pos != std::wstring::npos)
        {
            pos = text.find_first_not_of(_t(" \n\r\t"), pos);
        }
    }
}

std::wstring css::parse_css_url(const std::wstring& str)
{
    std::wstring result;

    size_t pos1 = str.find_first_of(_t('('));
    size_t pos2 = str.find_first_of(_t(')'));

    if (pos1 != std::wstring::npos && pos2 != std::wstring::npos)
    {
        result = str.substr(pos1 + 1, pos2 - pos1 - 1);
        if (result.length())
        {
            if (result[0] == _t('\'') || result[0] == _t('"'))
            {
                result.erase(0, 1);
            }
        }
        if (result.length())
        {
            if (result[result.length() - 1] == _t('\'') || result[result.length() - 1] == _t('"'))
            {
                result.erase(result.length() - 1, 1);
            }
        }
    }

    return result;
}

void css::parse_selectors(const std::wstring& text, const std::shared_ptr<style> &styles, std::shared_ptr<media_query_list>& media)
{
    auto tokens = split_string(text, L',');

    for (auto tok : tokens)
    {
        auto selector = std::make_shared<css_selector>(styles, media);

        if (selector->parse(tok))
        {
            selector->calc_specificity();
            add_selector(selector);
        }
    }
}

void css::sort_selectors()
{
    sort(m_selectors.begin(), m_selectors.end(), std::less<std::shared_ptr<css_selector>>());
}

void css::parse_atrule(const std::wstring& text, const std::wstring& baseurl, document &doc, std::shared_ptr<media_query_list>& media)
{
    if (text.substr(0, 7) == _t("@import"))
    {
        int sPos = 7;
        std::wstring iStr;
        iStr = text.substr(sPos);
        if (iStr[iStr.length() - 1] == _t(';'))
        {
            iStr.erase(iStr.length() - 1);
        }
        trim(iStr);
        auto tokens = split_string(iStr, _t(" "), _t("(\""));
        //tokenize(iStr, tokens, _t(" "), _t(""), _t("()\""));
        if (!tokens.empty())
        {
            auto url = parse_css_url(tokens.front());

            if (url.empty())
            {
                url = tokens.front();
            }
            tokens.erase(tokens.begin());
            std::wstring css_baseurl;

            if (!baseurl.empty())
            {
                css_baseurl = baseurl;
            }

            doc.import_css(url, css_baseurl);

            /*if (!css_text.empty())
            {
            std::shared_ptr<media_query_list> new_media = media;
            if (!tokens.empty())
            {
            std::wstring media_str;
            for (const auto &tok : tokens)
            {
            if (iter != tokens.begin())
            {
            media_str += _t(" ");
            }
            media_str += (*iter);
            }
            new_media = media_query_list::create_from_string(media_str, doc);
            if (!new_media)
            {
            new_media = media;
            }
            }
            parse_stylesheet(css_text, css_baseurl, doc, new_media);
            }*/
        }
    }
    else if (text.substr(0, 6) == _t("@media"))
    {
        auto b1 = text.find_first_of(L'{');
        auto b2 = text.find_last_of(L'}');

        if (b1 != std::wstring::npos)
        {
            std::wstring media_type = text.substr(6, b1 - 6);
            trim(media_type);
            std::shared_ptr<media_query_list> new_media = media_query_list::create_from_string(media_type);

            std::wstring media_style;
            if (b2 != std::wstring::npos)
            {
                media_style = text.substr(b1 + 1, b2 - b1 - 1);
            }
            else
            {
                media_style = text.substr(b1 + 1);
            }

            parse_stylesheet(media_style, baseurl, doc, new_media);
        }
    }
}
