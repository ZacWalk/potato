#pragma once

extern std::wstring empty;

inline bool is_empty(const wchar_t *sz)
{
    return sz == nullptr || sz[0] == 0;
}

inline const wchar_t* wcsichr(const wchar_t* text, wchar_t cc)
{
    auto c = iswupper(cc) ? towlower(cc) : cc;

    while (*text)
    {
        auto t = iswupper(*text) ? towlower(*text) : *text;
        if (t == c) return text;
        text++;
    }

    return nullptr;
}

inline std::string ToUtf8(const std::wstring &wstr)
{
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int) wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int) wstr.size(), &result[0], size_needed, nullptr, nullptr);
    return result;
}

inline std::wstring ToUtf16(const char *sz)
{
    int len = strlen(sz);
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, sz, len, nullptr, 0);
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, sz, len, &result[0], size_needed);
    return result;
}

inline std::wstring ToUtf16(const std::string &str)
{
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int) str.size(), nullptr, 0);
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int) str.size(), &result[0], size_needed);
    return result;
}

inline int Clamp(int v, int l, int r)
{
    if (v < l) return l;
    if (v > r) return r;
    return v;
}


inline static bool is_quote(wchar_t c)
{
    return c == L'\"' || c == L'\'';
}

static inline int IsSpaceChar(__in int ch)
{
    return iswspace(ch) || ch == 0;
}

inline static std::wstring Trim(__in const std::wstring &ss)
{
    auto s = ss;
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(IsSpaceChar))));
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(IsSpaceChar))).base(), s.end());
    return s;
}

inline void trim(std::wstring &s)
{
    auto pos = s.find_first_not_of(_t(" \n\r\t"));
    if (pos != std::wstring::npos)
    {
        s.erase(s.begin(), s.begin() + pos);
    }
    pos = s.find_last_not_of(_t(" \n\r\t"));
    if (pos != std::wstring::npos)
    {
        s.erase(s.begin() + pos + 1, s.end());
    }
}

inline std::wstring trim_lower(const std::wstring &source)
{
    auto p = source.c_str();

    std::wstring result;
    result.reserve(source.size());

    while (iswspace(*p)) p++;
    while (*p) result += towlower(*p++);
    result.erase(std::find_if(result.rbegin(), result.rend(), std::not1(std::ptr_fun<int, int>(IsSpaceChar))).base(), result.end());

    return result;
}


inline bool starts(const wchar_t *text, const wchar_t *with)
{
    while (*with != 0)
    {
        if (towlower(*text) != towlower(*with))
            return false;

        ++text;
        ++with;
    }

    return true;
}

inline bool starts(const std::wstring &text, const wchar_t *with)
{
    return starts(text.c_str(), with);
}

inline std::wstring make_url(const std::wstring &url, const std::wstring &basepath)
{
    std::wstring result;

    if (PathIsRelative(url.c_str()) && !PathIsURL(url.c_str()))
    {
        WCHAR abs_url[512];
        DWORD dl = 512;
        UrlCombine(basepath.c_str(), url.c_str(), abs_url, &dl, 0);
        result = abs_url;
    }
    else
    {
        if (PathIsURL(url.c_str()))
        {
            result = url;
        }
        else
        {
            WCHAR abs_url[512];
            DWORD dl = 512;
            UrlCreateFromPath(url.c_str(), abs_url, &dl, 0);
            result = abs_url;
        }
    }
    if (result.substr(0, 8) == L"file:///")
    {
        result.erase(5, 1);
    }
    if (result.substr(0, 7) == L"file://")
    {
        result.erase(0, 7);
    }
    return result;
}

inline void transform_text(std::wstring& text, text_transform tt)
{
    if (text.empty()) return;

    switch (tt)
    {
    case text_transform_capitalize:
        if (!text.empty())
        {
            text[0] = toupper(text[0]);
        }
        break;
    case text_transform_uppercase:
        for (size_t i = 0; i < text.length(); i++)
        {
            text[i] = toupper(text[i]);
        }
        break;
    case text_transform_lowercase:
        for (size_t i = 0; i < text.length(); i++)
        {
            text[i] = toupper(text[i]);
        }
        break;
    }
}

wchar_t Normalize(__in wchar_t c);

inline bool NeedsQuoting(__in_z const wchar_t *s)
{
    return wcspbrk(s, L": \t\'\"") != 0;
}

inline std::wstring QuoteIfNeeded(__in const std::wstring &s)
{
    if (NeedsQuoting(s.c_str()))
    {
        if (s.find_first_of(L'\"') == std::wstring::npos)
        {
            return L'"' + s + L'"';
        }
        else
        {
            return L'\'' + s + L'\'';
        }
    }

    return s;
}

inline int icmp(__in_z const wchar_t *l, __in_z const wchar_t *r)
{
    if (l == r) return 0;
    if (l == nullptr) return 1;
    if (r == nullptr) return -1;
    return _wcsicmp(l, r);
}

inline int icmp(const std::wstring &l, __in_z const wchar_t *r)
{
    return icmp(l.c_str(), r);
}

inline int icmp(const std::wstring &l, const std::wstring &r)
{
    return icmp(l.c_str(), r.c_str());
}

inline bool is_equal(__in_z const wchar_t *l, __in_z const wchar_t *r)
{
    if (l == r) return true;
    if (l == nullptr) return false;
    if (r == nullptr) return false;
    return _wcsicmp(l, r) == 0;
}

inline bool is_equal(const std::wstring &l, __in_z const wchar_t *r)
{
    return is_equal(l.c_str(), r);
}

inline bool is_equal(const std::wstring &l, const std::wstring &r)
{
    return is_equal(l.c_str(), r.c_str());
}

struct Range
{
    size_t begin;
    size_t end;

    Range() : begin(0), end(0) {}
    Range(const wchar_t *sz) : begin(0), end(sz ? wcslen(sz) : 0) {}
    Range(size_t b, size_t e) : begin(b), end(e) {}

    void clear()
    {
        begin = end = 0;
    }

    bool empty() const
    {
        return begin == end;
    }

    bool operator==(const Range &other) const
    {
        return begin == other.begin && end == other.end;
    }

    size_t size() const
    {
        return end - begin;
    }
};

inline Range SubString(const std::wstring &string, const std::wstring &query)
{
    auto ps = string.c_str();
    auto pq = query.c_str();
    auto start = ps;

    auto s = Normalize(*ps++);
    auto q = Normalize(*pq++);

    while (s)
    {
        if (s == q) // Is matching?
        {
            auto match_ps = ps;
            auto match_pq = pq;
            auto match_s = Normalize(*match_ps++);
            auto match_q = Normalize(*match_pq++);

            while (match_s == match_q)
            {
                if (match_q == 0)
                    return Range(start - string.c_str(), match_ps - 1 - string.c_str());

                match_s = Normalize(*match_ps++);
                match_q = Normalize(*match_pq++);
            }

            if (match_q == 0)
                return Range(start - string.c_str(), match_ps - 1 - string.c_str());
        }

        start = ps;
        s = Normalize(*ps++);
    }

    return Range();
}



class Match
{
private:
    std::wstring _text;
    std::wstring _prefix;

    Range _selection;

public:

    Match() { }
    explicit Match(const std::wstring &s) : _text(s) {  }
    Match(const Match &other) : _text(other._text), _prefix(other._prefix), _selection(other._selection) { }

    void operator=(const Match &other)
    {
        _text = other._text;
        _prefix = other._prefix;
        _selection = other._selection;
    }

    void operator=(const std::wstring &s)
    {
        _prefix = nullptr;
        _text = s;
        _selection.clear();
    }

    void Draw(HDC hdc, const RectI &rr) const;

    void Text(const std::wstring &s, const Range selection)
    {
        assert(s.size() >= selection.begin && s.size() >= selection.end);
        assert(selection.size() <= s.size());

        _text = s;
        _selection = selection;
    };

    std::wstring Text(bool quoteIfNeeded = false) const
    {
        std::wstring result;

        if (!_prefix.empty())
        {
            result += _prefix;
            result += L":";
        }

        result += quoteIfNeeded ? QuoteIfNeeded(_text) : _text;
        return result;
    };

    void Prefix(std::wstring sz)
    {
        _prefix = sz;
    }

    bool operator<(const Match &other) const
    {
        auto pref = icmp(_prefix, other._prefix);
        if (pref != 0) return pref < 0;
        return icmp(_text, other._text) < 0;
    }
};

static std::wstring Format(const wchar_t *format, ...)
{
    va_list argList;
    va_start(argList, format);

    auto length = _vscwprintf(format, argList);
    auto sz = (wchar_t*) _alloca((length + 1) * sizeof(wchar_t));
    if (sz == nullptr) return L"";
    vswprintf_s(sz, length + 1, format, argList);
    va_end(argList);
    sz[length] = 0;
    return sz;
}

static const wchar_t *From(bool val) { return val ? L"true" : L"false"; };

// Case insensitive string equivalence test for collections
struct ltstr
{
    bool operator()(const char* s1, const char* s2) const
    {
        return _stricmp(s1, s2) < 0;
    }

    bool operator()(const std::string& s1, const std::string& s2) const
    {
        return _stricmp(s1.c_str(), s2.c_str()) < 0;
    }

    bool operator()(const std::string& s1, const char * s2) const
    {
        return _stricmp(s1.c_str(), s2) < 0;
    }

    bool operator()(const char * s1, const std::string& s2) const
    {
        return _stricmp(s1, s2.c_str()) >= 0;
    }

    bool operator()(const wchar_t* s1, const wchar_t* s2) const
    {
        return _wcsicmp(s1, s2) < 0;
    }

    bool operator()(const std::wstring& s1, const std::wstring& s2) const
    {
        return _wcsicmp(s1.c_str(), s2.c_str()) < 0;
    }

    bool operator()(const std::wstring& s1, const wchar_t * s2) const
    {
        return _wcsicmp(s1.c_str(), s2) < 0;
    }

    bool operator()(const wchar_t * s1, const std::wstring& s2) const
    {
        return _wcsicmp(s1, s2.c_str()) >= 0;
    }
};

int value_index(const std::wstring& val, const wchar_t *strings, int defValue = -1, wchar_t delim = _t(';'));
bool value_in_list(const std::wstring& val, const wchar_t *strings, wchar_t delim = _t(';'));

std::wstring::size_type find_close_bracket(const std::wstring &s, std::wstring::size_type off, wchar_t open_b = _t('('), wchar_t close_b = _t(')'));


std::vector<std::wstring> split_string(const std::wstring& str, const wchar_t delim = L' ');
std::vector<std::wstring> split_string(const std::wstring& str, const wchar_t *delims, const wchar_t *quote = _t("\""));
