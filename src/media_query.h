#pragma once

struct media_query_expression
{
    media_feature feature;
    int val;
    int val2;
    bool check_as_bool;

    media_query_expression()
    {
        check_as_bool = false;
        feature = media_feature_none;
        val = 0;
        val2 = 0;
    }

    bool check(const media_features& features) const;
};

class media_query
{
private:
    std::vector<media_query_expression> m_expressions;
    bool  m_not;
    media_type  m_media_type;
public:
    media_query(const media_query& val);

    static std::shared_ptr<media_query> create_from_string(const std::wstring& str);
    bool check(const media_features& features) const;
    //private:
    media_query();
};

class media_query_list
{
private:
    std::vector<std::shared_ptr<media_query>> m_queries;
    bool m_is_used;

public:

    inline media_query_list()
    {
        m_is_used = false;
    }

    inline media_query_list(const media_query_list& val)
    {
        m_is_used = val.m_is_used;
        m_queries = val.m_queries;
    }

    static std::shared_ptr<media_query_list> create_from_string(const std::wstring& str);
    bool is_used() const { return m_is_used; }

    bool apply_media_features(const media_features& features); // returns true if the m_is_used changed
};



