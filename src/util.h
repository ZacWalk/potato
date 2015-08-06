#pragma once

extern HINSTANCE g_hInstance;


class critical_section
{
public:
    critical_section()
    {
        ::InitializeCriticalSection(&_cs);
    }
    ~critical_section()
    {
        ::DeleteCriticalSection(&_cs);
    }

    void Enter()
    {
        ::EnterCriticalSection(&_cs);
    }
    void Leave()
    {
        ::LeaveCriticalSection(&_cs);
    }

private:
    critical_section(const critical_section&);
    critical_section& operator=(const critical_section&);

    CRITICAL_SECTION _cs;
};

class critical_section_lock
{
public:
    critical_section_lock(critical_section& a_section) : m_section(a_section)
    {
        m_section.Enter();
    }
    ~critical_section_lock()
    {
        m_section.Leave();
    }

private:
    critical_section_lock(const critical_section_lock&);
    critical_section_lock& operator=(const critical_section_lock&);

    critical_section& m_section;
};


inline std::string get_file_contents(const std::wstring &file_name)
{
    std::string result;
    std::ifstream in(file_name, std::ios::in | std::ios::binary);

    if (in)
    {
        in.seekg(0, std::ios::end);
        result.resize((int) in.tellg());
        in.seekg(0, std::ios::beg);
        in.read(&result[0], result.size());
        in.close();
    }

    return result;
}

inline std::string load_resource_html(int id)
{
    std::string result;

    auto hInstance = g_hInstance;
    auto hrsrc = ::FindResource(hInstance, MAKEINTRESOURCE(id), RT_HTML);

    if (hrsrc)
    {
        auto hg = ::LoadResource(hInstance, hrsrc);

        if (hg)
        {
            auto szSql = (LPCSTR)::LockResource(hg);
            auto size = SizeofResource(hInstance, hrsrc);
            result = std::string(szSql, size);
            ::FreeResource(hg);
        }
    }

    return result;
}

class ScopeLockedCount 
{
private:
    long volatile &_i;
    long const _current;

public:

    inline ScopeLockedCount(long volatile &i) : _i(i), _current(InterlockedIncrement(&i))
    {
    }

    inline ~ScopeLockedCount()
    {
        InterlockedDecrement(&_i);
    }

    inline long Count() const
    {
        return _current;
    }
};

template<typename T1, typename T2>
inline T1 Min(const T1 &x, const T2 &y)
{
    return x < y ? x : y;
}

template<typename T1, typename T2>
inline T1 Max(const T1 &x, const T2 &y)
{
    return x > y ? x : y;
}

template<typename T1, typename T2, typename T3>
inline T1 Min(const T1 &x, const T2 &y, const T3 &z)
{
    return Min(x, Min(y, z));
}

template<typename T1, typename T2, typename T3>
inline T1 Max(const T1 &x, const T2 &y, const T3 &z)
{
    return Max(x, Max(y, z));
}

inline int round_f(float val)
{
    int int_val = (int) val;
    if (val - int_val >= 0.5)
    {
        int_val++;
    }
    return int_val;
}

inline int round_d(double val)
{
    int int_val = (int) val;
    if (val - int_val >= 0.5)
    {
        int_val++;
    }
    return int_val;
}
