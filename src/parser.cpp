#include "pch.h"
#include "document.h"
#include "stylesheet.h"
#include "document.h"
#include "render_win32.h"
#include "strings.h"

stop_tags_t parser::m_stop_tags [] =
{
    { _t("body;head"), _t("html") },
    { _t("td;th;tr;tbody;thead;tfoot"), _t("table") },
    { 0, 0 },
};

ommited_end_tags_t parser::m_ommited_end_tags [] =
{
    { _t("li"), _t("li") },
    { _t("dt"), _t("dt;dd") },
    { _t("dd"), _t("dt;dd") },
    { _t("p"), _t("address;article;aside;blockquote;div;dl;fieldset;footer;form;h1;h2;h3;h4;h5;h6;header;hgroup;hr;main;nav;ol;p;pre;section;table;ul") },
    { _t("rb"), _t("rb;rt;rtc;rp") },
    { _t("rt"), _t("rb;rt;rtc;rp") },
    { _t("rtc"), _t("rb;rt;rtc;rp") },
    { _t("rp"), _t("rb;rt;rtc;rp") },
    { _t("optgroup"), _t("optgroup") },
    { _t("option"), _t("optgroup;option") },
    { _t("thead"), _t("tbody;tfoot") },
    { _t("tbody"), _t("tbody;tfoot") },
    { _t("tfoot"), _t("tbody;tfoot") },
    { _t("tr"), _t("tr") },
    { _t("td"), _t("td;th") },
    { _t("th"), _t("td;th") },
    { 0, 0 },
};


element *parser::create_element(const std::wstring &tag_name)
{
    element *newTag = nullptr;

    if (!newTag)
    {
        if (is_equal(tag_name, _t("br")))
        {
            newTag = new element(_doc, el_break);
        }
        else if (is_equal(tag_name, _t("p")))
        {
            newTag = new element(_doc, el_para);
        }
        else if (is_equal(tag_name, _t("img")))
        {
            newTag = new element(_doc, el_image);
        }
        else if (is_equal(tag_name, _t("table")))
        {
            newTag = new element(_doc, el_table);
        }
        else if (is_equal(tag_name, _t("td")) || is_equal(tag_name, _t("th")))
        {
            newTag = new element(_doc, el_td);
        }
        else if (is_equal(tag_name, _t("link")))
        {
            newTag = new element(_doc, el_link);
        }
        else if (is_equal(tag_name, _t("title")))
        {
            newTag = new element(_doc, el_title);
        }
        else if (is_equal(tag_name, _t("a")))
        {
            newTag = new element(_doc, el_anchor);
        }
        else if (is_equal(tag_name, _t("tr")))
        {
            newTag = new element(_doc, el_tr);
        }
        else if (is_equal(tag_name, _t("style")))
        {
            newTag = new element(_doc, el_style);
        }
        else if (is_equal(tag_name, _t("base")))
        {
            newTag = new element(_doc, el_base);
        }
        else if (is_equal(tag_name, _t("body")))
        {
            newTag = new element(_doc, el_body);
        }
        else if (is_equal(tag_name, _t("div")))
        {
            newTag = new element(_doc, el_div);
        }
        else if (is_equal(tag_name, _t("script")))
        {
            newTag = new element(_doc, el_script);
        }
        else if (is_equal(tag_name, _t("font")))
        {
            newTag = new element(_doc, el_font);
        }
        else
        {
            newTag = new element(_doc, el_html);
        }
    }

    if (newTag)
    {
        newTag->set_tagName(tag_name);
    }

    return newTag;
}

void parser::parse_tag_start(const std::wstring &tag_name)
{
    parse_pop_void_element();

    // We add the html(root) element before parsing
    if (is_equal(tag_name, _t("html")))
    {
        return;
    }

    auto el = create_element(tag_name);

    if (el)
    {
        if (m_parse_stack.back()->get_tagName() == _t("html"))
        {
            // if last element is root we have to add head or body
            if (!value_in_list(tag_name, _t("head;body")))
            {
                parse_push_element(create_element(_t("body")));
            }
        }

        parse_close_omitted_end(tag_name);
        parse_open_omitted_start(tag_name);
        parse_push_element(el);
    }
}


void parser::parse_tag_end(const std::wstring &tag_name)
{
    if (!m_parse_stack.empty())
    {
        if (m_parse_stack.back()->get_tagName() == tag_name)
        {
            parse_pop_element();
        }
        else
        {
            const wchar_t *stop_tag = L"";

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


void parser::parse_push_element(element *el)
{
    if (!m_parse_stack.empty())
    {
        m_parse_stack.back()->appendChild(el);
        m_parse_stack.push_back(el);
    }
}

void parser::parse_attribute(const std::wstring &attr_name, const std::wstring &attr_value)
{
    if (!m_parse_stack.empty())
    {
        m_parse_stack.back()->set_attr(attr_name, attr_value);
    }
}

void parser::parse_word(const std::wstring &val)
{
    if (m_parse_stack.back()->get_tagName() == _t("html"))
    {
        parse_push_element(create_element(_t("body")));
    }

    parse_pop_void_element();

    if (!m_parse_stack.empty())
    {
        m_parse_stack.back()->appendText(val);
    }
}

void parser::parse_space(const std::wstring &val)
{
    parse_pop_void_element();
    if (!m_parse_stack.empty())
    {
        m_parse_stack.back()->appendSpace(val);
    }
}

void parser::parse_comment_start()
{
    parse_pop_void_element();
    parse_push_element(new element(_doc, el_comment));
}

void parser::parse_comment_end()
{
    parse_pop_element();
}

void parser::parse_cdata_start()
{
    parse_pop_void_element();
    parse_push_element(new element(_doc, el_cdata));
}

void parser::parse_cdata_end()
{
    parse_pop_element();
}

void parser::parse_data(const std::wstring &val)
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

bool parser::parse_pop_element(const std::wstring &tag, const wchar_t *stop_tags)
{
    bool found = false;
    for (auto iel = m_parse_stack.rbegin(); iel != m_parse_stack.rend(); iel++)
    {
        if ((*iel)->get_tagName() == tag)
        {
            found = true;
            break;
        }
        if (value_in_list((*iel)->get_tagName(), stop_tags)) break;
    }

    if (!found) return false;

    while (found)
    {
        if (m_parse_stack.back()->get_tagName() == tag)
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
        if (value_in_list(m_parse_stack.back()->get_tagName(), void_elements))
        {
            parse_pop_element();
        }
    }
}

void parser::parse_pop_to_parent(const std::wstring &parents, const std::wstring &stop_parent)
{
    std::vector<element*>::size_type parent = 0;
    bool found = false;
    auto p = split_string(parents, L';');

    for (int i = (int) m_parse_stack.size() - 1; i >= 0 && !found; i--)
    {
        if (std::find(p.begin(), p.end(), m_parse_stack[i]->get_tagName()) != p.end())
        {
            found = true;
            parent = i;
        }
        if (m_parse_stack[i]->get_tagName() == stop_parent)
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

void parser::parse_close_omitted_end(const std::wstring &tag)
{
    for (int i = 0; m_ommited_end_tags[i].tag; i++)
    {
        if (m_parse_stack.back()->get_tagName() == m_ommited_end_tags[i].tag)
        {
            if (value_in_list(tag, m_ommited_end_tags[i].followed_tags))
            {
                parse_pop_element();
                break;
            }
        }
    }
}

void parser::parse_open_omitted_start(const std::wstring &tag)
{
    if (is_equal(tag, _t("col")))
    {
        if (m_parse_stack.back()->get_tagName() != _t("colgroup"))
        {
            parse_tag_start(_t("colgroup"));
        }
    }
    else if (is_equal(tag, _t("tr")))
    {
        if (m_parse_stack.back()->get_tagName() != _t("tbody") &&
            m_parse_stack.back()->get_tagName() != _t("thead") &&
            m_parse_stack.back()->get_tagName() != _t("tfoot"))
        {
            parse_tag_start(_t("tbody"));
        }
    }
}

