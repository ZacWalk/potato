#pragma once

class utf16_instream
{
private:
    const std::wstring &_s;
    std::wstring::const_iterator _i;

public:
    utf16_instream(const std::wstring &s) : _s(s), _i(_s.begin())
    {
    }

    inline wchar_t get_char()
    {
        return _i == _s.end() ? 0 : *_i++;
    }

    void wchar_to_chars(wchar_t wch, std::wstring& dst)
    {
        dst += (wchar_t) wch;
    }
};


class utf8_instream
{
private:
    const std::string &_s;
    std::string::const_iterator _i;

public:
    utf8_instream(const std::string &s) : _s(s), _i(_s.begin())
    {
    }

    inline wchar_t getb()
    {
        return _i == _s.end() ? 0 : (wchar_t) *_i++;
    }
    inline wchar_t get_next_utf8(wchar_t val)
    {
        return (val & 0x3f);
    }

    inline wchar_t get_char()
    {
        wchar_t b1 = getb();

        if (!b1)
        {
            return 0;
        }

        // Determine whether we are dealing
        // with a one-, two-, three-, or four-
        // byte sequence.
        if ((b1 & 0x80) == 0)
        {
            // 1-byte sequence: 000000000xxxxxxx = 0xxxxxxx
            return b1;
        }
        else if ((b1 & 0xe0) == 0xc0)
        {
            // 2-byte sequence: 00000yyyyyxxxxxx = 110yyyyy 10xxxxxx
            wchar_t r = (b1 & 0x1f) << 6;
            r |= get_next_utf8(getb());
            return r;
        }
        else if ((b1 & 0xf0) == 0xe0)
        {
            // 3-byte sequence: zzzzyyyyyyxxxxxx = 1110zzzz 10yyyyyy 10xxxxxx
            wchar_t r = (b1 & 0x0f) << 12;
            r |= get_next_utf8(getb()) << 6;
            r |= get_next_utf8(getb());
            return r;
        }
        else if ((b1 & 0xf8) == 0xf0)
        {
            // 4-byte sequence: 11101110wwwwzzzzyy + 110111yyyyxxxxxx
            //  = 11110uuu 10uuzzzz 10yyyyyy 10xxxxxx
            // (uuuuu = wwww + 1)
            int b2 = get_next_utf8(getb());
            int b3 = get_next_utf8(getb());
            int b4 = get_next_utf8(getb());
            return ((b1 & 7) << 18) | ((b2 & 0x3f) << 12) |
                ((b3 & 0x3f) << 6) | (b4 & 0x3f);
        }

        //bad start for UTF-8 multi-byte sequence
        return '?';
    }


    void utf8_instream::wchar_to_chars(wchar_t code, std::wstring& dst)
    {
        dst += (wchar_t) code;
    }

};