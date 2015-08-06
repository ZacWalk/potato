//| 
//| simple and fast XML/HTML scanner/tokenizer
//|
//| (C) Andrew Fedoniouk @ terrainformatica.com
//|

#pragma once

struct html_entities
{
    wchar_t szCode[20];
    wchar_t Code;
};

html_entities m_HTMLCodes [];

enum token_type
{
    TT_ERROR = -1,
    TT_EOF = 0,

    TT_TAG_START, // <tag ...
    TT_TAG_END,  // </tag>
    TT_TAG_END_EMPTY, // <tag ... />
    TT_ATTR,  // <tag attr="value" >  
    TT_WORD,
    TT_SPACE,

    TT_DATA,  // content of followings:

    TT_COMMENT_START, TT_COMMENT_END, // after "<!--" and "-->"
    TT_CDATA_START, TT_CDATA_END,  // after "<![CDATA[" and "]]>"
    TT_PI_START, TT_PI_END,   // after "<?" and "?>"
    TT_ENTITY_START, TT_ENTITY_END, // after "<!ENTITY" and ">"
    TT_DOCTYPE_START, TT_DOCTYPE_END, // after "<!DOCTYPE" and ">"
};

template<class tstream>
class scanner
{

public:

    scanner(tstream& is) :
        token(TT_EOF),
        input(is),
        input_char(0),
        got_tail(false) {
        c_scan = &scanner::scan_body;
    }

    // get next token
    token_type get_token() { return (this->*c_scan)(); }

    // get value of TT_WORD, TT_SPACE, TT_ATTR and TT_DATA
    const std::wstring &get_value() const
    {
        return m_value;
    }

    const std::wstring &get_attr_name() const
    {
        return m_attr_name;
    }

    const std::wstring &get_tag_name() const
    {
        return m_tag_name;
    }


private: /* methods */

    typedef token_type(scanner::*scan)();
    scan c_scan; // current 'reader'

    token_type scan_body()
    {
        wchar_t c = get_char();

        m_value.clear();

        bool ws = false;

        if (c == 0) return TT_EOF;
        else if (c == '<') return scan_tag();
        else if (c == '&')
            c = scan_entity();
        else
            ws = is_whitespace(c);

        if (!ws)
        {
            while (true)
            {
                append_value(c);

                // CJK character range
                if (c >= 0x4E00 && c <= 0x9FCC)
                {
                    break;
                }

                c = get_char();
                if (c == 0) { push_back(c); break; }
                if (c == '<') { push_back(c); break; }
                if (c == '&') { push_back(c); break; }

                if (is_whitespace(c) != ws)
                {
                    push_back(c);
                    break;
                }

            }
        }
        else
        {
            while (ws)
            {
                append_value(c);
                c = get_char();
                ws = is_whitespace(c);
            }

            push_back(c);
            ws = true; 
        }

        return ws ? TT_SPACE : TT_WORD;
    }

    token_type scan_head()
    {
        wchar_t c = skip_whitespace();

        if (c == '>')
        {
            if (is_equal(m_tag_name, _t("script")))
            {
                c_scan = &scanner::scan_raw_body;
                return scan_raw_body();
            }
            else
            {
                c_scan = &scanner::scan_body;
                return scan_body();
            }
        }

        if (c == '/')
        {
            wchar_t t = get_char();
            if (t == '>') { c_scan = &scanner::scan_body; return TT_TAG_END_EMPTY; }
            else { push_back(t); return TT_ERROR; } // erroneous situtation - standalone '/'
        }

        m_attr_name.clear();
        m_value.clear();

        // attribute name...
        while (c != '=')
        {
            if (c == 0) return TT_EOF;
            if (c == '>') { push_back(c); return TT_ATTR; } // attribute without value (HTML style)
            if (is_whitespace(c))
            {
                c = skip_whitespace();
                if (c != '=') { push_back(c); return TT_ATTR; } // attribute without value (HTML style)
                else break;
            }
            if (c == '<') return TT_ERROR;
            append_attr_name(c);
            c = get_char();
        }

        c = skip_whitespace();
        // attribute value...

        if (c == '\"')
            while ((c = get_char()))
            {
            if (c == '\"') return TT_ATTR;
            if (c == '&') c = scan_entity();
            append_value(c);
            }
        else if (c == '\'') // allowed in html
            while ((c = get_char()))
            {
            if (c == '\'') return TT_ATTR;
            if (c == '&') c = scan_entity();
            append_value(c);
            }
        else // scan token, allowed in html: e.g. align=center
            do
            {
            if (is_whitespace(c)) return TT_ATTR;
            /* these two removed in favour of better html support:
            if( c == '/' || c == '>' ) { push_back(c); return TT_ATTR; }
            if( c == '&' ) c = scan_entity();*/
            if (c == '>') { push_back(c); return TT_ATTR; }
            append_value(c);
            } while ((c = get_char()));

        return TT_ERROR;
    }

    // caller already consumed '<'
    // scan header start or tag tail
    token_type scan_tag()
    {
        m_tag_name.clear();

        wchar_t c = get_char();

        bool is_tail = c == '/';
        if (is_tail) c = get_char();

        while (c)
        {
            if (is_whitespace(c)) { c = skip_whitespace(); break; }
            if (c == '/' || c == '>') break;
            append_tag_name(c);

            switch (m_tag_name.length())
            {
            case 3:
                if (starts(m_tag_name, _t("!--"))) { c_scan = &scanner::scan_comment; return TT_COMMENT_START; }
                break;
            case 8:
                if (starts(m_tag_name, _t("![cdata["))) { c_scan = &scanner::scan_cdata; return TT_CDATA_START; }
                if (starts(m_tag_name, _t("!doctype"))) { c_scan = &scanner::scan_entity_decl; return TT_DOCTYPE_START; }
                break;
            case 7:
                if (starts(m_tag_name, _t("!entity"))) { c_scan = &scanner::scan_entity_decl; return TT_ENTITY_START; }
                break;
            }

            c = get_char();
        }

        if (c == 0) return TT_ERROR;

        if (is_tail)
        {
            if (c == '>') return TT_TAG_END;
            return TT_ERROR;
        }
        else
            push_back(c);

        c_scan = &scanner::scan_head;
        return TT_TAG_START;
    }

    // skip whitespaces.
    // returns first non-whitespace char
    wchar_t skip_whitespace()
    {
        while (wchar_t c = get_char())
        {
            if (!is_whitespace(c)) return c;
        }
        return 0;
    }

    void push_back(wchar_t c)
    {
        input_char = c;
    }

    wchar_t get_char()
    {
        if (input_char)
        {
            wchar_t t = input_char;
            input_char = 0;
            return t;
        }
        return input.get_char();
    }


    // caller consumed '&'
    wchar_t scan_entity()
    {
        std::wstring buf_ch;
        wchar_t buf[32];
        int i = 0;
        wchar_t t = 0;

        for (; i < 31; ++i)
        {
            t = get_char();

            if (t == ';')
                break;

            if (t == 0) return TT_EOF;
            if (!isalnum(t) && t != '#')
            {
                push_back(t);
                t = 0;
                break; // appears a erroneous entity token.
                // but we try to use it.
            }
            buf[i] = t;
            input.wchar_to_chars(t, buf_ch);
        }

        buf[i] = 0;

        if (is_equal(buf_ch, _t("gt"))) return '>';
        if (is_equal(buf_ch, _t("lt"))) return '<';
        if (is_equal(buf_ch, _t("amp"))) return '&';
        if (is_equal(buf_ch, _t("apos"))) return '\'';
        if (is_equal(buf_ch, _t("quot"))) return '\"';

        wchar_t entity = resolve_entity(buf_ch.c_str(), i);

        if (entity)
        {
            return entity;
        }
        // no luck ...
        append_value('&');
        for (int n = 0; n < i; ++n)
            append_value(buf[n]);
        if (t) return t;
        return get_char();
    }

    inline bool is_whitespace(wchar_t c)
    {
        return c <= ' '
            && (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f');
    }

    void append_value(wchar_t c)
    {
        input.wchar_to_chars(c, m_value);
    }

    void append_attr_name(wchar_t c)
    {
        input.wchar_to_chars(c, m_attr_name);
    }

    void append_tag_name(wchar_t c)
    {
        input.wchar_to_chars(c, m_tag_name);
    }

    token_type scan_comment()
    {
        if (got_tail)
        {
            c_scan = &scanner::scan_body;
            got_tail = false;
            return TT_COMMENT_END;
        }
        m_value.clear();
        for (;;)
        {
            wchar_t c = get_char();
            if (c == 0) return TT_EOF;
            append_value(c);

            if (m_value.length() >= 3
                && m_value[m_value.length() - 1] == '>'
                && m_value[m_value.length() - 2] == '-'
                && m_value[m_value.length() - 3] == '-')
            {
                got_tail = true;
                m_value.erase(m_value.length() - 3);
                break;
            }
        }
        return TT_DATA;
    }

    token_type scan_cdata()
    {
        if (got_tail)
        {
            c_scan = &scanner::scan_body;
            got_tail = false;
            return TT_CDATA_END;
        }
        m_value.clear();
        for (;;)
        {
            wchar_t c = get_char();
            if (c == 0) return TT_EOF;
            append_value(c);

            if (m_value.length() >= 3
                && m_value[m_value.length() - 1] == '>'
                && m_value[m_value.length() - 2] == ']'
                && m_value[m_value.length() - 3] == ']')
            {
                got_tail = true;
                m_value.erase(m_value.length() - 3);
                break;
            }
        }
        return TT_DATA;
    }

    token_type scan_pi()
    {
        if (got_tail)
        {
            c_scan = &scanner::scan_body;
            got_tail = false;
            return TT_PI_END;
        }
        m_value.clear();
        for (;;)
        {
            wchar_t c = get_char();
            if (c == 0) return TT_EOF;
            append_value(c);

            if (m_value.length() >= 2
                && m_value[m_value.length() - 1] == '>'
                && m_value[m_value.length() - 2] == '?')
            {
                got_tail = true;
                m_value.erase(m_value.length() - 2);
                break;
            }
        }
        return TT_DATA;
    }

    token_type scan_entity_decl()
    {
        if (got_tail)
        {
            c_scan = &scanner::scan_body;
            got_tail = false;
            return TT_ENTITY_END;
        }
        wchar_t t;
        unsigned int tc = 0;
        m_value.clear();
        for (;;)
        {
            t = get_char();
            if (t == 0) return TT_EOF;
            append_value(t);
            if (t == '\"') tc++;
            else if (t == '>' && (tc & 1) == 0)
            {
                got_tail = true;
                break;
            }
        }
        return TT_DATA;
    }

    token_type scan_doctype_decl()
    {
        if (got_tail)
        {
            c_scan = &scanner::scan_body;
            got_tail = false;
            return TT_DOCTYPE_END;
        }
        wchar_t t;
        unsigned int tc = 0;
        m_value.clear();
        for (;;)
        {
            t = get_char();
            if (t == 0) return TT_EOF;
            append_value(t);
            if (t == '\"') tc++;
            else if (t == '>' && (tc & 1) == 0)
            {
                got_tail = true;
                break;
            }
        }
        return TT_DATA;
    }

    wchar_t resolve_entity(const wchar_t* buf, int buf_size)
    {
        wchar_t wres = 0;
        if (buf[0] == '#')
        {
            if (buf[1] == 'x' || buf[1] == 'X')
            {
                wres = (wchar_t) std::stol(buf + 2, 0, 16);
            }
            else
            {
                wres = (wchar_t) std::stoi(buf + 1);
            }
        }
        else
        {
            std::wstring str = _t("&");
            str.append(buf, buf_size);
            str += _t(";");

            for (int i = 0; m_HTMLCodes[i].szCode[0]; i++)
            {
                if (is_equal(str, m_HTMLCodes[i].szCode))
                {
                    wres = m_HTMLCodes[i].Code;
                    break;
                }
            }
        }
        return wres;
    }

    token_type scan_raw_body()
    {
        if (got_tail)
        {
            c_scan = &scanner::scan_body;
            got_tail = false;
            return TT_TAG_END;
        }
        m_value.clear();
        for (;;)
        {
            wchar_t c = get_char();
            if (c == 0) return TT_EOF;
            append_value(c);

            if (m_value.length() >= 9 && !_wcsnicmp(m_value.c_str() + m_value.length() - 9, _t("</script>"), 9))
            {
                got_tail = true;
                m_value.erase(m_value.length() - 9);
                break;
            }
        }
        return TT_DATA;
    }

private:

    token_type token;

    std::wstring m_value;
    std::wstring m_tag_name;
    std::wstring m_attr_name;

    tstream& input;
    wchar_t input_char;

    bool  got_tail; // aux flag used in scan_comment, etc. 

};


