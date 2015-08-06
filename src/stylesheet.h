#pragma once
#include "style.h"
#include "media_query.h"

class document;



//////////////////////////////////////////////////////////////////////////

enum attr_select_condition
{
    select_exists,
    select_equal,
    select_contain_str,
    select_start_str,
    select_end_str,
    select_pseudo_class,
    select_pseudo_element,
};

//////////////////////////////////////////////////////////////////////////

struct css_attribute_selector
{
    std::wstring attribute;
    std::wstring val;
    attr_select_condition condition = select_exists;
};

//////////////////////////////////////////////////////////////////////////

class css_element_selector
{
public:
    std::wstring m_tag;
    std::vector<css_attribute_selector> m_attrs;

public:
    

    void parse(const std::wstring& txt);
};

//////////////////////////////////////////////////////////////////////////

enum css_combinator
{
    combinator_descendant,
    combinator_child,
    combinator_adjacent_sibling,
    combinator_general_sibling
};

//////////////////////////////////////////////////////////////////////////

class css_selector
{
public:

    int m_specificity = 0;
    css_element_selector m_right;
    std::shared_ptr<css_selector> m_left;
    css_combinator m_combinator = combinator_descendant;
    std::shared_ptr<style> m_style;
    int  m_order = 0;
    std::shared_ptr<media_query_list> m_media_query;

public:

    css_selector(const std::shared_ptr<style> &s, const std::shared_ptr<media_query_list> &media) : m_style(s), m_media_query(media)
    {
    }

    ~css_selector()
    {
    }

    bool parse(const std::wstring& text);
    void calc_specificity();

    inline bool is_media_valid() const
    {
        if (!m_media_query)
        {
            return true;
        }

        return m_media_query->is_used();
    }

    void add_media_to_doc(const std::shared_ptr<document> &doc) const;
};


//////////////////////////////////////////////////////////////////////////

inline bool operator > (const css_selector& v1, const css_selector& v2)
{
    if (v1.m_specificity == v2.m_specificity)
    {
        return (v1.m_order > v2.m_order);
    }
    return (v1.m_specificity > v2.m_specificity);
}

inline bool operator < (const css_selector& v1, const css_selector& v2)
{
    if (v1.m_specificity == v2.m_specificity)
    {
        return (v1.m_order < v2.m_order);
    }
    return (v1.m_specificity < v2.m_specificity);
}

inline bool operator >(std::shared_ptr<css_selector> v1, std::shared_ptr<css_selector> v2)
{
    return (*v1 > *v2);
}

inline bool operator < (std::shared_ptr<css_selector> v1, std::shared_ptr<css_selector> v2)
{
    return (*v1 < *v2);
}

//////////////////////////////////////////////////////////////////////////

class used_selector
{
public:

    std::shared_ptr<css_selector> m_selector;
    bool m_used = false;

    used_selector(const std::shared_ptr<css_selector> &s, bool used) : m_selector(s), m_used(used)
    {
    }

    used_selector(const used_selector &other) : m_selector(other.m_selector), m_used(other.m_used)
    {
    }
};




class css
{
    std::vector<std::shared_ptr<css_selector>> m_selectors;

public:
    css()
    {

    }

    ~css()
    {

    }

    const std::vector<std::shared_ptr<css_selector>>& selectors() const
    {
        return m_selectors;
    }

    void clear()
    {
        m_selectors.clear();
    }

    void parse_stylesheet(const std::wstring& str, const std::wstring& baseurl, document &doc, std::shared_ptr<media_query_list>& media);
    void sort_selectors();

    static std::wstring parse_css_url(const std::wstring& str);

private:
    void parse_atrule(const std::wstring& text, const std::wstring& baseurl, document &doc, std::shared_ptr<media_query_list>& media);
    void parse_selectors(const std::wstring& txt, const std::shared_ptr<style> &styles, std::shared_ptr<media_query_list>& media);

    inline void add_selector(const std::shared_ptr<css_selector> &selector)
    {
        selector->m_order = (int) m_selectors.size();
        m_selectors.push_back(selector);
    }


};


