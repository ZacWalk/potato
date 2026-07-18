// core.cpp - String normalization, CSS color parsing (hex/rgb/hsl/named),
// value_index lookups into semicolon-delimited tables, split_string, and
// autocomplete text_match rendering.

#include "pch.h"
#include "ui.h"


std::string empty;

char normalize(const char c)
{
	const auto uc = static_cast<unsigned char>(c);
	switch (uc)
	{
	case 0xE1: // á (in Latin-1/ISO-8859-1)
	case 0xE4: // ä
	case 0xE0: // à
	case 0xE2: // â
	case 0xE3: // ã
	case 0xE5: // å
		return 'a';
	case 0xE9: // é
	case 0xEB: // ë
	case 0xE8: // è
	case 0xEA: // ê
		return 'e';
	case 0xED: // í
	case 0xEF: // ï
	case 0xEC: // ì
	case 0xEE: // î
		return 'i';
	case 0xF3: // ó
	case 0xF6: // ö
	case 0xF2: // ò
	case 0xF4: // ô
	case 0xF5: // õ
		return 'o';
	case 0xFA: // ú
	case 0xFC: // ü
	case 0xF9: // ù
	case 0xFB: // û
		return 'u';
	default:
		return static_cast<char>(tolower(uc));
	}
}


void text_match::draw(const HDC hdc, const recti& rr) const
{
	auto r = rr;

	constexpr auto normalText = color::text;
	const auto highlightBk = lighten(color::task_background, 64);
	const auto propertyText = emphasize(normalText);

	const auto clrOld = SetTextColor(hdc, normalText);

	if (!_prefix.empty())
	{
		const auto wpfx = to_utf16(_prefix);
		size_i size;

		if (GetTextExtentPointW(hdc, wpfx.c_str(), static_cast<int>(wpfx.size()), &size))
		{
			const auto y = r.top + (r.height() - size.cy) / 2;

			SetTextColor(hdc, propertyText);
			ExtTextOutW(hdc, r.left, y, 0, nullptr, wpfx.c_str(), static_cast<UINT>(wpfx.size()), nullptr);
			r.left += size.cx + 2;
			ExtTextOutW(hdc, r.left, y, 0, nullptr, L":", 1, nullptr);
			r.left += 8;
			SetTextColor(hdc, normalText);
		}
	}

	const auto wtext = to_utf16(_text);

	if (_selection.empty())
	{
		size_i size;

		if (GetTextExtentPointW(hdc, wtext.c_str(), static_cast<int>(wtext.size()), &size))
		{
			ExtTextOutW(hdc, r.left, r.top + (r.height() - size.cy) / 2, 0, nullptr, wtext.c_str(),
			           static_cast<UINT>(wtext.size()),
			           nullptr);
		}
	}
	else
	{
		const auto lenLeft = static_cast<int>(_selection.begin);
		const auto lenMid = static_cast<int>(_selection.end - _selection.begin);
		const auto lenRight = static_cast<int>(wtext.size() - _selection.end);

		const auto textLeft = wtext.c_str();
		const auto textMid = wtext.c_str() + _selection.begin;
		const auto textRight = wtext.c_str() + _selection.end;

		size_i extentLeft;
		size_i extentMid;
		size_i extentRight;

		GetTextExtentPointW(hdc, textLeft, lenLeft, &extentLeft);
		GetTextExtentPointW(hdc, textMid, lenMid, &extentMid);
		GetTextExtentPointW(hdc, textRight, lenRight, &extentRight);

		const auto hight = Max(extentLeft.cy, extentMid.cy, extentRight.cy);
		const auto y = r.top + (r.height() - hight) / 2;

		if (lenMid > 0)
		{
			const auto oldBkCol = SetBkColor(hdc, highlightBk);
			const auto oldBkMode = SetBkMode(hdc, OPAQUE);

			ExtTextOutW(hdc, r.left + extentLeft.cx, y, 0, nullptr, textMid, static_cast<UINT>(lenMid), nullptr);

			SetBkMode(hdc, oldBkMode);
			SetBkColor(hdc, oldBkCol);
		}

		if (lenLeft > 0)
		{
			ExtTextOutW(hdc, r.left, y, 0, nullptr, textLeft, static_cast<UINT>(lenLeft), nullptr);
		}

		if (lenRight > 0)
		{
			ExtTextOutW(hdc, r.left + extentLeft.cx + extentMid.cx, y, 0, nullptr, textRight,
			           static_cast<UINT>(lenRight), nullptr);
		}
	}

	SetTextColor(hdc, clrOld);
}

std::string::size_type find_close_bracket(const std::string& s, const std::string::size_type off,
                                           const char open_b, const char close_b)
{
	int cnt = 0;

	for (auto i = off; i < s.length(); i++)
	{
		if (s[i] == open_b)
		{
			cnt++;
		}
		else if (s[i] == close_b)
		{
			cnt--;
			if (!cnt)
			{
				return i;
			}
		}
	}
	return std::string::npos;
}

static inline const char* next_delim(const char* text, const char d)
{
	while (*text)
	{
		if (*text == d) return text;
		text++;
	}

	return text;
}

static inline bool one_of(const char c, const char* chars)
{
	while (*chars)
	{
		if (*chars == c) return true;
		chars++;
	}

	return false;
}

static inline const char* next_delim(const char* text, const char* delims)
{
	while (*text)
	{
		if (one_of(*text, delims)) return text;
		text++;
	}

	return text;
}

int value_index(const std::string& val, const char* strings, const int defValue, const char delim)
{
	assert(!val.empty());
	assert(delim);

	auto idx = 0;
	auto delim_start = strings;
	auto delim_end = next_delim(strings, delim);
	const auto strings_end = strings + strlen(strings);

	while (delim_start < strings_end)
	{
		const auto delimLen = delim_end - delim_start;

		if (delimLen == val.size() &&
			_strnicmp(delim_start, val.c_str(), delimLen) == 0)
		{
			return idx;
		}

		idx++;

		delim_start = delim_end + 1;
		delim_end = next_delim(delim_start, delim);
	}

	return defValue;
}

bool value_in_list(const std::string& val, const char* strings, const char delim)
{
	return value_index(val, strings, -1, delim) >= 0;
}


std::vector<std::string> split_string(const std::string& strings, const char delim)
{
	char delims[2] = { delim, 0 };
	return split_string(strings, delims, "\"'");
}

std::vector<std::string> split_string(const std::string& strings, const char* delims, const char* quote)
{
	std::vector<std::string> results;

	if (strings.find_first_not_of(delims) == std::string::npos)
	{
		if (!strings.empty()) results.push_back(strings);
		return results;
	}

	if (!strings.empty())
	{
		bool inQuotes = false;
		char quoteChar = 0;
		auto p = strings.c_str();

		std::string current;

		while (*p != 0)
		{
			if (one_of(*p, quote) && (quoteChar == 0 || quoteChar == *p))
			{
				current += *p;
				inQuotes = !inQuotes;

				if (inQuotes)
				{
					quoteChar = *p;
					if (quoteChar == '(') quoteChar = ')';
					if (quoteChar == '[') quoteChar = ']';
				}
				else
				{
					quoteChar = 0;
				}
			}
			else if (inQuotes || !one_of(*p, delims))
			{
				current += *p;
			}
			else
			{
				current = trimmed(current);
				if (!current.empty()) results.push_back(current);
				current.clear();
			}

			p++;
		}

		current = trimmed(current);
		if (!current.empty()) results.push_back(current);
	}

	return results;
}

struct def_color
{
	const char* name;
	const char* rgb;
};

static def_color g_def_colors[] =
{
	{"transparent", "rgba(0, 0, 0, 0)"},
	{"AliceBlue", "#F0F8FF"},
	{"AntiqueWhite", "#FAEBD7"},
	{"Aqua", "#00FFFF"},
	{"Aquamarine", "#7FFFD4"},
	{"Azure", "#F0FFFF"},
	{"Beige", "#F5F5DC"},
	{"Bisque", "#FFE4C4"},
	{"Black", "#000000"},
	{"BlanchedAlmond", "#FFEBCD"},
	{"Blue", "#0000FF"},
	{"BlueViolet", "#8A2BE2"},
	{"Brown", "#A52A2A"},
	{"BurlyWood", "#DEB887"},
	{"CadetBlue", "#5F9EA0"},
	{"Chartreuse", "#7FFF00"},
	{"Chocolate", "#D2691E"},
	{"Coral", "#FF7F50"},
	{"CornflowerBlue", "#6495ED"},
	{"Cornsilk", "#FFF8DC"},
	{"Crimson", "#DC143C"},
	{"Cyan", "#00FFFF"},
	{"DarkBlue", "#00008B"},
	{"DarkCyan", "#008B8B"},
	{"DarkGoldenRod", "#B8860B"},
	{"DarkGray", "#A9A9A9"},
	{"DarkGrey", "#A9A9A9"},
	{"DarkGreen", "#006400"},
	{"DarkKhaki", "#BDB76B"},
	{"DarkMagenta", "#8B008B"},
	{"DarkOliveGreen", "#556B2F"},
	{"Darkorange", "#FF8C00"},
	{"DarkOrchid", "#9932CC"},
	{"DarkRed", "#8B0000"},
	{"DarkSalmon", "#E9967A"},
	{"DarkSeaGreen", "#8FBC8F"},
	{"DarkSlateBlue", "#483D8B"},
	{"DarkSlateGray", "#2F4F4F"},
	{"DarkSlateGrey", "#2F4F4F"},
	{"DarkTurquoise", "#00CED1"},
	{"DarkViolet", "#9400D3"},
	{"DeepPink", "#FF1493"},
	{"DeepSkyBlue", "#00BFFF"},
	{"DimGray", "#696969"},
	{"DimGrey", "#696969"},
	{"DodgerBlue", "#1E90FF"},
	{"FireBrick", "#B22222"},
	{"FloralWhite", "#FFFAF0"},
	{"ForestGreen", "#228B22"},
	{"Fuchsia", "#FF00FF"},
	{"Gainsboro", "#DCDCDC"},
	{"GhostWhite", "#F8F8FF"},
	{"Gold", "#FFD700"},
	{"GoldenRod", "#DAA520"},
	{"Gray", "#808080"},
	{"Grey", "#808080"},
	{"Green", "#008000"},
	{"GreenYellow", "#ADFF2F"},
	{"HoneyDew", "#F0FFF0"},
	{"HotPink", "#FF69B4"},
	{"Ivory", "#FFFFF0"},
	{"Khaki", "#F0E68C"},
	{"Lavender", "#E6E6FA"},
	{"LavenderBlush", "#FFF0F5"},
	{"LawnGreen", "#7CFC00"},
	{"LemonChiffon", "#FFFACD"},
	{"LightBlue", "#ADD8E6"},
	{"LightCoral", "#F08080"},
	{"LightCyan", "#E0FFFF"},
	{"LightGoldenRodYellow", "#FAFAD2"},
	{"LightGray", "#D3D3D3"},
	{"LightGrey", "#D3D3D3"},
	{"LightGreen", "#90EE90"},
	{"LightPink", "#FFB6C1"},
	{"LightSalmon", "#FFA07A"},
	{"LightSeaGreen", "#20B2AA"},
	{"LightSkyBlue", "#87CEFA"},
	{"LightSlateGray", "#778899"},
	{"LightSlateGrey", "#778899"},
	{"LightSteelBlue", "#B0C4DE"},
	{"LightYellow", "#FFFFE0"},
	{"Lime", "#00FF00"},
	{"LimeGreen", "#32CD32"},
	{"Linen", "#FAF0E6"},
	{"Magenta", "#FF00FF"},
	{"Maroon", "#800000"},
	{"MediumAquaMarine", "#66CDAA"},
	{"MediumBlue", "#0000CD"},
	{"MediumOrchid", "#BA55D3"},
	{"MediumPurple", "#9370D8"},
	{"MediumSeaGreen", "#3CB371"},
	{"MediumSlateBlue", "#7B68EE"},
	{"MediumSpringGreen", "#00FA9A"},
	{"MediumTurquoise", "#48D1CC"},
	{"MediumVioletRed", "#C71585"},
	{"MidnightBlue", "#191970"},
	{"MintCream", "#F5FFFA"},
	{"MistyRose", "#FFE4E1"},
	{"Moccasin", "#FFE4B5"},
	{"NavajoWhite", "#FFDEAD"},
	{"Navy", "#000080"},
	{"OldLace", "#FDF5E6"},
	{"Olive", "#808000"},
	{"OliveDrab", "#6B8E23"},
	{"Orange", "#FFA500"},
	{"OrangeRed", "#FF4500"},
	{"Orchid", "#DA70D6"},
	{"PaleGoldenRod", "#EEE8AA"},
	{"PaleGreen", "#98FB98"},
	{"PaleTurquoise", "#AFEEEE"},
	{"PaleVioletRed", "#D87093"},
	{"PapayaWhip", "#FFEFD5"},
	{"PeachPuff", "#FFDAB9"},
	{"Peru", "#CD853F"},
	{"Pink", "#FFC0CB"},
	{"Plum", "#DDA0DD"},
	{"PowderBlue", "#B0E0E6"},
	{"Purple", "#800080"},
	{"Red", "#FF0000"},
	{"RosyBrown", "#BC8F8F"},
	{"RoyalBlue", "#4169E1"},
	{"SaddleBrown", "#8B4513"},
	{"Salmon", "#FA8072"},
	{"SandyBrown", "#F4A460"},
	{"SeaGreen", "#2E8B57"},
	{"SeaShell", "#FFF5EE"},
	{"Sienna", "#A0522D"},
	{"Silver", "#C0C0C0"},
	{"SkyBlue", "#87CEEB"},
	{"SlateBlue", "#6A5ACD"},
	{"SlateGray", "#708090"},
	{"SlateGrey", "#708090"},
	{"Snow", "#FFFAFA"},
	{"SpringGreen", "#00FF7F"},
	{"SteelBlue", "#4682B4"},
	{"Tan", "#D2B48C"},
	{"Teal", "#008080"},
	{"Thistle", "#D8BFD8"},
	{"Tomato", "#FF6347"},
	{"Turquoise", "#40E0D0"},
	{"Violet", "#EE82EE"},
	{"Wheat", "#F5DEB3"},
	{"White", "#FFFFFF"},
	{"WhiteSmoke", "#F5F5F5"},
	{"Yellow", "#FFFF00"},
	{"YellowGreen", "#9ACD32"},
	{nullptr, nullptr}
};

static std::map<const char*, web_color, ltstr> colorMap;

static bool can_parse(const char* str)
{
	return str != nullptr && (str[0] == '#' || _strnicmp(str, "rgb", 3) == 0 || _strnicmp(str, "hsl", 3) ==
		0);
}

static web_color parse_rgb(const char* str)
{
	web_color result;

	if (str[0] == '#')
	{
		char red[3] = {0};
		char green[3] = {0};
		char blue[3] = {0};
		char alpha[3] = {0};

		const auto len = strlen(str + 1);

		if (len == 3)
		{
			red[0] = str[1];
			red[1] = str[1];
			green[0] = str[2];
			green[1] = str[2];
			blue[0] = str[3];
			blue[1] = str[3];
		}
		else if (len == 4)
		{
			red[0] = str[1];
			red[1] = str[1];
			green[0] = str[2];
			green[1] = str[2];
			blue[0] = str[3];
			blue[1] = str[3];
			alpha[0] = str[4];
			alpha[1] = str[4];
		}
		else if (len == 6)
		{
			red[0] = str[1];
			red[1] = str[2];
			green[0] = str[3];
			green[1] = str[4];
			blue[0] = str[5];
			blue[1] = str[6];
		}
		else if (len == 8)
		{
			red[0] = str[1];
			red[1] = str[2];
			green[0] = str[3];
			green[1] = str[4];
			blue[0] = str[5];
			blue[1] = str[6];
			alpha[0] = str[7];
			alpha[1] = str[8];
		}

		result.red = static_cast<byte>(safe_stol(red, 16));
		result.green = static_cast<byte>(safe_stol(green, 16));
		result.blue = static_cast<byte>(safe_stol(blue, 16));

		if (alpha[0])
		{
			result.alpha = static_cast<byte>(safe_stol(alpha, 16));
		}
	}
	else if (!_strnicmp(str, "rgb", 3))
	{
		std::string s = str;
		auto pos = s.find('(');

		if (pos != std::string::npos)
		{
			s.erase(s.begin(), s.begin() + pos + 1);
		}

		pos = s.find_last_of(")");

		if (pos != std::string::npos)
		{
			s.erase(s.begin() + pos, s.end());
		}

		const auto tokens = split_string(s, ", \t/");

		auto parse_channel = [](const std::string& tok) -> byte
		{
			if (!tok.empty() && tok.back() == '%')
			{
				char* end = nullptr;
				const auto pct = strtof(tok.c_str(), &end);
				return static_cast<byte>(clamp(static_cast<int>(pct * 255.0f / 100.0f), 0, 255));
			}
			return static_cast<byte>(clamp(safe_stoi(tok), 0, 255));
		};

		if (tokens.size() >= 1) result.red = parse_channel(tokens[0]);
		if (tokens.size() >= 2) result.green = parse_channel(tokens[1]);
		if (tokens.size() >= 3) result.blue = parse_channel(tokens[2]);
		if (tokens.size() >= 4)
		{
			char* end = nullptr;
			const auto a = strtof(tokens[3].c_str(), &end);
			result.alpha = static_cast<byte>(clamp(static_cast<int>(a * 255.0f), 0, 255));
		}
	}
	else if (!_strnicmp(str, "hsl", 3))
	{
		std::string s = str;
		auto pos = s.find('(');

		if (pos != std::string::npos)
		{
			s.erase(s.begin(), s.begin() + pos + 1);
		}

		pos = s.find_last_of(")");

		if (pos != std::string::npos)
		{
			s.erase(s.begin() + pos, s.end());
		}

		const auto tokens = split_string(s, ", \t");

		if (tokens.size() >= 3)
		{
			char* end = nullptr;
			float h = strtof(tokens[0].c_str(), &end);
			float sat = strtof(tokens[1].c_str(), &end) / 100.0f;
			float lit = strtof(tokens[2].c_str(), &end) / 100.0f;

			h = fmodf(h, 360.0f);
			if (h < 0) h += 360.0f;
			sat = std::max(0.0f, std::min(1.0f, sat));
			lit = std::max(0.0f, std::min(1.0f, lit));

			auto hue2rgb = [](const float p, const float q, float t) -> float
			{
				if (t < 0.0f) t += 1.0f;
				if (t > 1.0f) t -= 1.0f;
				if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
				if (t < 1.0f / 2.0f) return q;
				if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
				return p;
			};

			if (sat == 0.0f)
			{
				result.red = result.green = result.blue = static_cast<byte>(lit * 255.0f);
			}
			else
			{
				const float q = lit < 0.5f ? lit * (1.0f + sat) : lit + sat - lit * sat;
				const float p = 2.0f * lit - q;
				const float hNorm = h / 360.0f;
				result.red = static_cast<byte>(hue2rgb(p, q, hNorm + 1.0f / 3.0f) * 255.0f);
				result.green = static_cast<byte>(hue2rgb(p, q, hNorm) * 255.0f);
				result.blue = static_cast<byte>(hue2rgb(p, q, hNorm - 1.0f / 3.0f) * 255.0f);
			}

			if (tokens.size() >= 4)
			{
				const auto a = strtof(tokens[3].c_str(), &end);
				result.alpha = static_cast<byte>(clamp(static_cast<int>(a * 255.0f), 0, 255));
			}
		}
	}

	return result;
}

static void load_names()
{
	for (int i = 0; g_def_colors[i].name; i++)
	{
		colorMap[g_def_colors[i].name] = parse_rgb(g_def_colors[i].rgb);
	}
}

web_color web_color::from_string(const char* str)
{
	web_color result;

	if (str)
	{
		if (can_parse(str))
		{
			result = parse_rgb(str);
		}
		else
		{
			if (colorMap.empty())
			{
				load_names();
			}

			const auto found = colorMap.find(str);

			if (found != colorMap.end())
			{
				result = found->second;
			}
		}
	}

	return result;
}

bool web_color::is_color(const char* str)
{
	if (can_parse(str))
	{
		return true;
	}

	if (colorMap.empty())
	{
		load_names();
	}

	return colorMap.contains(str);
}


static void should_find_value_index()
{
	const auto index = value_index("table-column", style_display_strings, display_inline);
	should::equal(8, index);
}

static void should_pass_css_size()
{
	css_length sz;
	sz.fromString("2em", font_size_strings);

	should::equal(2, static_cast<int>(sz.val()));
	should::equal(css_units_em, sz.units());
}


std::string run_tests()
{
	tests tests;

	tests.register_test("Should find value index", should_find_value_index);
	tests.register_test("Should pass css size", should_pass_css_size);

	std::stringstream output;
	tests.run_tests(output);
	return output.str();
}
