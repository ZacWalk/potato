#pragma once
#include "types.h"
#include "strings.h"

class css_length
{
    union
    {
        float m_value;
        int m_predef;
    };

    css_units m_units;
    bool m_is_predefined;

public:

    inline css_length()
    {
        m_value = 0;
        m_predef = 0;
        m_units = css_units_none;
        m_is_predefined = false;
    }

    inline css_length(const css_length& val)
    {
        if (val.is_predefined())
        {
            m_predef = val.m_predef;
        }
        else
        {
            m_value = val.m_value;
        }
        m_units = val.m_units;
        m_is_predefined = val.m_is_predefined;
    }

    inline css_length& operator=(const css_length& val)
    {
        if (val.is_predefined())
        {
            m_predef = val.m_predef;
        }
        else
        {
            m_value = val.m_value;
        }
        m_units = val.m_units;
        m_is_predefined = val.m_is_predefined;
        return *this;
    }

    inline bool is_predefined() const
    {
        return m_is_predefined;
    }

    inline void predef(int val)
    {
        m_predef = val;
        m_is_predefined = true;
    }

    inline int predef() const
    {
        if (m_is_predefined)
        {
            return m_predef;
        }
        return 0;
    }

    inline void set_value(float val, css_units units)
    {
        m_value = val;
        m_is_predefined = false;
        m_units = units;
    }

    inline float val() const
    {
        if (!m_is_predefined)
        {
            return m_value;
        }
        return 0;
    }

    inline css_units units() const
    {
        return m_units;
    }

    inline int calc_percent(int width) const
    {
        if (!is_predefined())
        {
            if (units() == css_units_percentage)
            {
                return (int) ((double) width * (double) m_value / 100.0);
            }
            else
            {
                return (int) val();
            }
        }
        return 0;
    }
    
    void fromString(const std::wstring& str, const wchar_t *predefs = L"", int defValue = 0)
    {
        // TODO: Make support for calc
        if (str.substr(0, 4) == _t("calc"))
        {
            m_is_predefined = true;
            m_predef = 0;
            return;
        }

        int predef = value_index(str, predefs, -1);

        if (predef >= 0)
        {
            m_is_predefined = true;
            m_predef = predef;
        }
        else
        {
            m_is_predefined = false;

            std::wstring num;
            std::wstring un;
            bool is_unit = false;

            for (auto chr : str)
            {
                if (!is_unit)
                {
                    if (iswdigit(chr) || chr == _t('.') || chr == _t('+') || chr == _t('-'))
                    {
                        num += chr;
                    }
                    else
                    {
                        un += chr;
                        is_unit = true;
                    }
                }
                else 
                {
                    un += chr;
                }
            }

            if (!num.empty())
            {
                m_value = std::stof(num);
                m_units = un.empty() ? css_units_none : (css_units) value_index(un, css_units_strings, css_units_none);
            }
            else
            {
                // not a number so it is predefined
                m_is_predefined = true;
                m_predef = defValue;
            }
        }
    }
};

struct css_margins
{
    css_length left;
    css_length right;
    css_length top;
    css_length bottom;

    css_margins()
    {

    }

    css_margins(const css_margins& val)
    {
        left = val.left;
        right = val.right;
        top = val.top;
        bottom = val.bottom;
    }

    css_margins& operator=(const css_margins& val)
    {
        left = val.left;
        right = val.right;
        top = val.top;
        bottom = val.bottom;
        return *this;
    }
};

struct css_offsets
{
    css_length left;
    css_length top;
    css_length right;
    css_length bottom;

    css_offsets()
    {

    }

    css_offsets(const css_offsets& val)
    {
        left = val.left;
        top = val.top;
        right = val.right;
        bottom = val.bottom;
    }

    css_offsets& operator=(const css_offsets& val)
    {
        left = val.left;
        top = val.top;
        right = val.right;
        bottom = val.bottom;
        return *this;
    }
};

struct css_position
{
    css_length x;
    css_length y;
    css_length width;
    css_length height;

    css_position()
    {

    }

    css_position(const css_position& val)
    {
        x = val.x;
        y = val.y;
        width = val.width;
        height = val.height;
    }

    css_position& operator=(const css_position& val)
    {
        x = val.x;
        y = val.y;
        width = val.width;
        height = val.height;
        return *this;
    }
};
