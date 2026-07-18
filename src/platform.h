// platform.h — Platform-independent types, constants, window/draw abstractions, API declarations.
// Must NOT include OS-specific headers. See platform_win.cpp for Win32 implementation.

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <bit>
#include <charconv>
#include <stdexcept>
#include <utility>
#include <cwctype>
#include <vector>

namespace pf
{
	constexpr uint32_t LEAD_SURROGATE_MIN = 0xd800u;
	constexpr uint32_t LEAD_SURROGATE_MAX = 0xdbffu;
	constexpr uint32_t TRAIL_SURROGATE_MIN = 0xdc00u;
	constexpr uint32_t TRAIL_SURROGATE_MAX = 0xdfffu;
	constexpr uint32_t LEAD_OFFSET = LEAD_SURROGATE_MIN - (0x10000 >> 10);
	constexpr uint32_t SURROGATE_OFFSET = 0xfca02400u; //  0x10000u - (LEAD_SURROGATE_MIN << 10) - TRAIL_SURROGATE_MIN;

	inline bool is_lead_surrogate(const uint32_t cp)
	{
		return cp >= LEAD_SURROGATE_MIN && cp <= LEAD_SURROGATE_MAX;
	}

	inline bool is_trail_surrogate(const uint32_t cp)
	{
		return cp >= TRAIL_SURROGATE_MIN && cp <= TRAIL_SURROGATE_MAX;
	}

	inline uint16_t mask16(const uint32_t oc)
	{
		return static_cast<uint16_t>(0xffff & oc);
	}

	constexpr bool is_utf8_continuation(const char b)
	{
		return (static_cast<uint8_t>(b) & 0xC0) == 0x80;
	}

	constexpr int utf8_codepoint_count(const std::string_view s)
	{
		int count = 0;
		for (const auto b : s)
		{
			if (!is_utf8_continuation(b))
				count++;
		}
		return count;
	}

	constexpr size_t utf8_truncate(const std::string_view s, const int max_codepoints)
	{
		int cps = 0;
		size_t i = 0;
		while (i < s.size())
		{
			if (!is_utf8_continuation(s[i]))
			{
				if (cps >= max_codepoints)
					return i;
				cps++;
			}
			i++;
		}
		return i;
	}

	constexpr int utf8_next(const std::string_view s, int pos)
	{
		if (pos >= static_cast<int>(s.size())) return pos;
		pos++;
		while (pos < static_cast<int>(s.size()) && is_utf8_continuation(s[pos]))
			pos++;
		return pos;
	}

	constexpr int utf8_prev(const std::string_view s, int pos)
	{
		if (pos <= 0) return 0;
		pos--;
		while (pos > 0 && is_utf8_continuation(s[pos]))
			pos--;
		return pos;
	}

	std::string utf16_to_utf8(std::wstring_view wstr);
	std::string u32_to_utf8(std::u32string_view str);
	std::u32string utf8_to_u32(std::string_view str);
	std::wstring u32_to_wstr(std::u32string_view str);
	std::u32string wstr_to_u32(std::wstring_view str);

	constexpr uint32_t to_lower(const uint32_t c)
	{
		if (c < 128) return c >= U'A' && c <= U'Z' ? c - U'A' + U'a' : c;
		if (c > USHRT_MAX) return c;
		return towlower(c);
	}

	constexpr uint32_t to_upper(const uint32_t c)
	{
		if (c < 128) return c >= U'a' && c <= U'z' ? c - U'a' + U'A' : c;
		if (c > USHRT_MAX) return c;
		return towupper(c);
	}

	constexpr uint32_t pop_utf8_char(std::string_view::const_iterator& in_ptr,
	                                 const std::string_view::const_iterator& end)
	{
		const auto c1 = static_cast<uint8_t>(*in_ptr++);

		if (c1 < 0x80)
		{
			return c1;
		}
		if (c1 >> 5 == 0x6)
		{
			if (std::distance(in_ptr, end) < 1)
			{
				in_ptr = end;
				return 0;
			}

			uint32_t c = (c1 & 0x1F) << 6;
			c |= (*in_ptr++ & 0x3F) << 0;
			return c;
		}
		if (c1 >> 4 == 0xe)
		{
			if (std::distance(in_ptr, end) < 2)
			{
				in_ptr = end;
				return 0;
			}

			uint32_t c = (c1 & 0x0F) << 12;
			c |= (*in_ptr++ & 0x3F) << 6;
			c |= (*in_ptr++ & 0x3F) << 0;
			return c;
		}
		if (c1 >> 3 == 0x1e)
		{
			if (std::distance(in_ptr, end) < 3)
			{
				in_ptr = end;
				return 0;
			}

			uint32_t c = (c1 & 0x07) << 18;
			c |= (*in_ptr++ & 0x3F) << 12;
			c |= (*in_ptr++ & 0x3F) << 6;
			c |= (*in_ptr++ & 0x3F) << 0;
			return c;
		}

		return '?';
	}

	constexpr uint32_t peek_utf8_char(std::string_view::const_iterator in_ptr,
	                                  const std::string_view::const_iterator& end)
	{
		return pop_utf8_char(in_ptr, end);
	}

	inline std::string_view utf8_cast(const std::string_view val)
	{
		return {std::bit_cast<const char*>(val.data()), val.size()};
	}

	inline std::wstring utf8_to_utf16(const std::string_view s)
	{
		std::wstring result;
		result.reserve(s.size());
		auto i = s.begin();
		while (i < s.end())
		{
			const auto cp = pop_utf8_char(i, s.end());

			if (cp > 0xffff)
			{
				result += static_cast<uint16_t>((cp >> 10) + LEAD_OFFSET);
				result += static_cast<uint16_t>((cp & 0x3ff) + TRAIL_SURROGATE_MIN);
			}
			else
			{
				result += static_cast<uint16_t>(cp);
			}
		}
		return result;
	}

	template <class output_it>
	void char32_to_utf8(output_it&& inserter, const uint32_t ch)
	{
		if (ch < 0x80)
		{
			*inserter++ = static_cast<uint8_t>(ch);
		}
		else if (ch < 0x800)
		{
			*inserter++ = static_cast<uint8_t>(0xC0 | ch >> 6);
			*inserter++ = static_cast<uint8_t>(0x80 | ch >> 0 & 0x3F);
		}
		else if (ch < 0x10000)
		{
			*inserter++ = static_cast<uint8_t>(0xE0 | ch >> 12);
			*inserter++ = static_cast<uint8_t>(0x80 | ch >> 6 & 0x3F);
			*inserter++ = static_cast<uint8_t>(0x80 | ch >> 0 & 0x3F);
		}
		else
		{
			*inserter++ = static_cast<uint8_t>(ch >> 18 | 0xf0);
			*inserter++ = static_cast<uint8_t>(ch >> 12 & 0x3f | 0x80);
			*inserter++ = static_cast<uint8_t>(ch >> 6 & 0x3f | 0x80);
			*inserter++ = static_cast<uint8_t>(ch & 0x3f | 0x80);
		}
	}

	inline void utf16_to_utf8(const std::wstring_view s, std::string& result)
	{
		result.clear();
		result.reserve(std::max(result.capacity(), s.size()));
		auto inserter = std::back_inserter(result);

		auto start = s.begin();
		const auto end = s.end();

		while (start != end)
		{
			uint32_t cp = mask16(*start++);

			if (is_lead_surrogate(cp))
			{
				if (start != end)
				{
					const uint32_t trail_surrogate = mask16(*start++);

					if (is_trail_surrogate(trail_surrogate))
					{
						cp = (cp << 10) + trail_surrogate + SURROGATE_OFFSET;
					}
					else
					{
						throw std::invalid_argument("Invalid input string");
					}
				}
				else
				{
					throw std::invalid_argument("Invalid input string");
				}
			}
			else if (is_trail_surrogate(cp))
			{
				throw std::invalid_argument("Invalid input string");
			}

			char32_to_utf8(inserter, cp);
		}
	}

	inline std::string utf16_to_utf8(const std::wstring_view s)
	{
		std::string result;
		utf16_to_utf8(s, result);
		return result;
	};

	inline std::string to_lower(const std::string_view s)
	{
		std::string result;
		result.reserve(s.size());
		auto inserter = std::back_inserter(result);

		auto i = s.begin();
		while (i < s.end())
		{
			const auto cp = pop_utf8_char(i, s.end());
			char32_to_utf8(inserter, to_lower(cp));
		}

		return result;
	}

	inline std::string to_upper(const std::string_view s)
	{
		std::string result;
		result.reserve(s.size());
		auto inserter = std::back_inserter(result);

		auto i = s.begin();
		while (i < s.end())
		{
			const auto cp = pop_utf8_char(i, s.end());
			char32_to_utf8(inserter, to_upper(cp));
		}

		return result;
	}

	constexpr int icmp(const std::string_view ll, const std::string_view rr)
	{
		if (ll.data() == rr.data() || (ll.empty() && rr.empty())) return 0;
		if (ll.empty()) return -1;
		if (rr.empty()) return 1;

		auto cl = 0;
		auto cr = 0;

		auto il = ll.begin();
		auto ir = rr.begin();
		const auto el = ll.end();
		const auto er = rr.end();

		while (il < el && ir < er)
		{
			cl = to_lower(pop_utf8_char(il, el));
			cr = to_lower(pop_utf8_char(ir, er));
			if (cl < cr) return -1;
			if (cl > cr) return 1;
		}

		if (il == el) cl = 0;
		if (ir == er) cr = 0;
		return cl - cr;
	}

	[[nodiscard]] constexpr std::string_view unquote(const std::string_view text)
	{
		if (text.size() > 1 && text.front() == '"' && text.back() == '"')
		{
			return text.substr(1, text.length() - 2);
		}
		if (text.size() > 1 && text.front() == '\'' && text.back() == '\'')
		{
			return text.substr(1, text.length() - 2);
		}

		return text;
	}


	std::string url_encode(std::string_view input);

	class ipoint
	{
	public:
		int32_t x = 0, y = 0;

		constexpr ipoint(const int xx = 0, const int yy = 0) : x(xx), y(yy)
		{
		}

		bool operator==(const ipoint& other) const = default;

		constexpr ipoint operator -() const
		{
			return ipoint(-x, -y);
		}

		constexpr ipoint operator +(const ipoint& point) const
		{
			return ipoint(x + point.x, y + point.y);
		}
	};

	class isize
	{
	public:
		int32_t cx = 0, cy = 0;

		constexpr isize(const int xx = 0, const int yy = 0) : cx(xx), cy(yy)
		{
		}

		bool operator==(const isize& other) const = default;
	};

	class irect
	{
	public:
		int32_t left = 0, top = 0, right = 0, bottom = 0;

		irect(const int l = 0, const int t = 0, const int r = 0, const int b = 0)
			: left(l), top(t), right(r), bottom(b)
		{
		}

		[[nodiscard]] int width() const
		{
			return right - left;
		}

		[[nodiscard]] int height() const
		{
			return bottom - top;
		}

		[[nodiscard]] irect offset(const ipoint& pt) const
		{
			return irect(left + pt.x, top + pt.y, right + pt.x, bottom + pt.y);
		}

		[[nodiscard]] irect offset(const int x, const int y) const
		{
			return irect(left + x, top + y, right + x, bottom + y);
		}

		[[nodiscard]] bool intersects(const irect& other) const
		{
			return left < other.right &&
				top < other.bottom &&
				right > other.left &&
				bottom > other.top;
		}

		[[nodiscard]] irect inflate(const int xy) const
		{
			return irect(left - xy, top - xy, right + xy, bottom + xy);
		}

		[[nodiscard]] irect inflate(const int x, const int y) const
		{
			return irect(left - x, top - y, right + x, bottom + y);
		}

		[[nodiscard]] irect inflate(const isize& s) const
		{
			return irect(left - s.cx, top - s.cy, right + s.cx, bottom + s.cy);
		}

		[[nodiscard]] bool contains(const ipoint& point) const
		{
			return left <= point.x && right >= point.x && top <= point.y && bottom >= point.y;
		}
	};

	[[nodiscard]] constexpr int clamp(const int v, const int lo, const int hi)
	{
		return std::clamp(v, lo, std::max(lo, hi));
	}


	struct color_t
	{
		uint8_t r = 0;
		uint8_t g = 0;
		uint8_t b = 0;

		constexpr color_t() = default;

		constexpr color_t(const uint8_t r, const uint8_t g, const uint8_t b) : r(r), g(g), b(b)
		{
		}

		bool operator==(const color_t&) const = default;
		auto operator<=>(const color_t&) const = default;

		[[nodiscard]] constexpr uint32_t rgb() const
		{
			return (r & 0xff) | (g & 0xff) << 8 | (b & 0xff) << 16;
		}

		static constexpr uint8_t clamp_byte(const int n)
		{
			return static_cast<uint8_t>(n > 255 ? 255 : n < 0 ? 0 : n);
		}

		[[nodiscard]] constexpr color_t lighten(const int n = 32) const
		{
			return {clamp_byte(r + n), clamp_byte(g + n), clamp_byte(b + n)};
		}

		[[nodiscard]] constexpr color_t darken(const int n = 32) const
		{
			return lighten(-n);
		}

		[[nodiscard]] constexpr color_t emphasize(const int n = 48) const
		{
			const bool is_light = r > 0x80 || g > 0x80 || b > 0x80;
			return lighten(is_light ? -n : n);
		}
	};

	uint32_t fnv1a_i(std::string_view sv);
	uint64_t fnv1a_i_64(std::string_view sv);

	class file_path
	{
		std::string _path;

	public:
		file_path(const std::string_view path) : _path(path)
		{
			for (auto& c : _path)
			{
				if (c == u8'/') c = u8'\\';
			}
			while (_path.size() > 1 && _path.back() == u8'\\')
			{
				if (_path.size() == 3 && _path[1] == u8':')
					break;
				_path.pop_back();
			}
		}

		file_path() = default;

		[[nodiscard]] const char* c_str() const
		{
			return _path.c_str();
		}

		[[nodiscard]] std::string_view view() const
		{
			return _path;
		}

		bool operator==(const file_path& other) const
		{
			return icmp(_path, other._path) == 0;
		}

		static constexpr std::string_view::size_type find_ext(const std::string_view path)
		{
			const auto last = path.find_last_of("./\\");
			if (last == std::string_view::npos || path[last] != '.') return path.size();
			return last;
		}

		static constexpr std::string_view::size_type find_last_slash(const std::string_view path)
		{
			const auto last = path.find_last_of("/\\");
			if (last == std::string_view::npos) return 0;
			return last + 1;
		}

		[[nodiscard]] std::string without_extension() const
		{
			return _path.substr(0, find_ext(_path));
		}

		[[nodiscard]] std::string extension() const
		{
			return _path.substr(find_ext(_path));
		}

		file_path combine(const std::string& name, const std::string& extension) const
		{
			const auto with_name = combine(name);
			auto result = std::string{with_name.without_extension()};

			if (!extension.empty())
			{
				if (extension[0] != L'.') result += L'.';
				result += extension;
			}
			return file_path{result};
		}

		static constexpr bool is_path_sep(const char c)
		{
			return c == '\\' || c == '/';
		}

		[[nodiscard]] file_path combine(const std::string_view part) const
		{
			auto result = _path;

			if (!part.empty())
			{
				if (!is_path_sep(result.back()) && !is_path_sep(part[0])) result += '\\';
				result += part;
			}
			return file_path{result};
		}

		[[nodiscard]] bool exists() const;

		[[nodiscard]] bool is_save_path() const
		{
			return _path.find_first_of("/\\") != std::string::npos;
		}

		[[nodiscard]] bool empty() const
		{
			return _path.empty();
		}

		[[nodiscard]] std::string name() const
		{
			return _path.substr(find_last_slash(_path));
		}

		[[nodiscard]] file_path folder() const
		{
			return file_path{_path.substr(0, find_last_slash(_path))};
		}

		static file_path module_folder();
	};

	struct ihash
	{
		size_t operator()(const file_path& path) const
		{
			return fnv1a_i(path.view());
		}

		size_t operator()(const std::string_view s) const
		{
			return fnv1a_i(s);
		}
	};

	struct iless
	{
		bool operator()(const file_path& l, const file_path& r) const
		{
			return icmp(l.view(), r.view()) < 0;
		}

		bool operator()(const std::string_view l, const std::string_view r) const
		{
			return icmp(l, r) < 0;
		}
	};

	struct ieq
	{
		bool operator()(const file_path& l, const file_path& r) const
		{
			return icmp(l.view(), r.view()) == 0;
		}

		bool operator()(const std::string_view l, const std::string_view r) const
		{
			return icmp(l, r) == 0;
		}
	};


	[[nodiscard]] constexpr bool is_empty(const char* sz)
	{
		return sz == nullptr || sz[0] == 0;
	}

	[[nodiscard]] constexpr bool is_empty(const wchar_t* sz)
	{
		return sz == nullptr || sz[0] == 0;
	}

	inline int32_t stoi(const std::string_view u8_string)
	{
		const auto sv = std::string_view(u8_string.data(), u8_string.size());
		int32_t result = 0;
		std::from_chars(sv.data(), sv.data() + sv.size(), result);
		return result;
	}

	inline double stod(const std::string_view u8_string)
	{
		const auto sv = std::string_view(u8_string.data(), u8_string.size());
		double result = 0.0;
		std::from_chars(sv.data(), sv.data() + sv.size(), result);
		return result;
	}


	// Key codes (match Windows VK_ values)
	//
	namespace platform_key
	{
		constexpr unsigned int LButton = 0x01;
		constexpr unsigned int RButton = 0x02;
		constexpr unsigned int Back = 0x08;
		constexpr unsigned int Tab = 0x09;
		constexpr unsigned int Return = 0x0D;
		constexpr unsigned int Shift = 0x10;
		constexpr unsigned int Control = 0x11;
		constexpr unsigned int Escape = 0x1B;
		constexpr unsigned int Alt = 0x12;
		constexpr unsigned int Space = 0x20;
		constexpr unsigned int Prior = 0x21;
		constexpr unsigned int Next = 0x22;
		constexpr unsigned int End = 0x23;
		constexpr unsigned int Home = 0x24;
		constexpr unsigned int Left = 0x25;
		constexpr unsigned int Up = 0x26;
		constexpr unsigned int Right = 0x27;
		constexpr unsigned int Down = 0x28;
		constexpr unsigned int Insert = 0x2D;
		constexpr unsigned int Delete = 0x2E;
		constexpr unsigned int F1 = 0x70;
		constexpr unsigned int F2 = 0x71;
		constexpr unsigned int F3 = 0x72;
		constexpr unsigned int F5 = 0x74;
		constexpr unsigned int F6 = 0x75;
		constexpr unsigned int F7 = 0x76;
		constexpr unsigned int F8 = 0x77;
		constexpr unsigned int F9 = 0x78;
		constexpr unsigned int F10 = 0x79;
	}

	// Key modifier flags for accelerator bindings
	//
	namespace key_mod
	{
		constexpr uint8_t none = 0;
		constexpr uint8_t ctrl = 1;
		constexpr uint8_t shift = 2;
		constexpr uint8_t alt = 4;
	}

	// Keyboard accelerator binding (key + modifiers)
	//
	struct key_binding
	{
		unsigned int key = 0;
		uint8_t modifiers = key_mod::none;

		[[nodiscard]] bool empty() const { return key == 0; }
	};

	// Format a key binding as human-readable text (e.g. "Ctrl+S", "Ctrl+Shift+F")
	std::string format_key_binding(const key_binding& kb);

	// Menu definitions
	//
	struct menu_command
	{
		std::string text;
		int id = 0;
		std::function<void()> action;
		std::function<bool()> is_enabled;
		std::function<bool()> is_checked;
		std::vector<menu_command> children;
		key_binding accel;

		menu_command() = default;

		// Leaf item with action + optional enabled/checked + optional key binding
		menu_command(std::string t, const int cmd_id,
		             std::function<void()> act,
		             std::function<bool()> en = nullptr,
		             std::function<bool()> chk = nullptr,
		             const key_binding kb = {})
			: text(std::move(t)), id(cmd_id), action(std::move(act)),
			  is_enabled(std::move(en)), is_checked(std::move(chk)),
			  accel(kb)
		{
		}

		// Submenu item
		menu_command(std::string t, const int cmd_id,
		             std::function<void()> act,
		             std::function<bool()> en,
		             std::function<bool()> chk,
		             std::vector<menu_command> ch)
			: text(std::move(t)), id(cmd_id), action(std::move(act)),
			  is_enabled(std::move(en)), is_checked(std::move(chk)),
			  children(std::move(ch))
		{
		}
	};

	// Cursor shapes
	//
	enum class cursor_shape { arrow, ibeam, size_we, size_ns, hand, wait };

	// Window style flags
	//
	namespace window_style
	{
		constexpr uint32_t child = 1 << 0;
		constexpr uint32_t visible = 1 << 1;
		constexpr uint32_t clip_children = 1 << 4;

		constexpr uint32_t composited = 1 << 16;
	}


	// Message types for frame_reactor (non-mouse, non-keyboard messages)
	//
	enum class message_type : unsigned int
	{
		create,
		destroy,
		set_focus,
		kill_focus,
		erase_background,
		timer,
		sys_color_change,
		command,
		close,
		dpi_changed,
		init_dialog,
		drop_files,
	};

	// Mouse message types — dispatched via handle_mouse
	//
	enum class mouse_message_type : unsigned int
	{
		left_button_dbl_clk,
		left_button_down,
		right_button_down,
		left_button_up,
		mouse_move,
		mouse_wheel,
		mouse_leave,
		mouse_activate,
		context_menu,
		set_cursor,
	};

	// Bundled mouse parameters
	struct mouse_params
	{
		ipoint point;
		bool left_button = false;
		bool control = false;
		bool shift = false;
		int16_t wheel_delta = 0;
		uint32_t hit_test = 0; // for set_cursor: HTCLIENT etc.
	};

	// Keyboard message types — dispatched via handle_keyboard
	//
	enum class keyboard_message_type : unsigned int
	{
		key_down,
		char_input,
	};

	// Bundled keyboard parameters
	struct keyboard_params
	{
		unsigned int vk = 0; // virtual key code (for key_down)
		char ch = 0; // character (for char_input)
	};

	// Extract signed mouse coordinates from packed lParam (handles negative values on multi-monitor)
	inline ipoint point_from_lparam(const intptr_t lParam)
	{
		return ipoint(static_cast<int16_t>(lParam & 0xFFFF),
		              static_cast<int16_t>(lParam >> 16 & 0xFFFF));
	}

	// Font types
	//
	enum class font_name
	{
		consolas,
		arial,
		calibri,
		segoe_icons, // Segoe Fluent Icons / Segoe MDL2 Assets — for icon glyphs
	};

	// Standard icon glyph code points for use with font_name::segoe_icons.
	// Values match Segoe Fluent Icons / Segoe MDL2 Assets so the same
	// codepoints work on both fonts.
	namespace icon_glyph
	{
		constexpr uint32_t back = 0xE72B;
		constexpr uint32_t forward = 0xE72A;
		constexpr uint32_t refresh = 0xE72C;
		constexpr uint32_t stop = 0xE711;
		constexpr uint32_t home = 0xE80F;
		constexpr uint32_t menu = 0xE700;
		constexpr uint32_t search = 0xE721;
		constexpr uint32_t settings = 0xE713;
		constexpr uint32_t cancel = 0xE10A;
	}

	struct font
	{
		int size = 12; // in points
		font_name name;
	};

	// Opaque platform font handle. On Win32 this stores an HFONT cast to
	// uintptr_t. Created via create_font_handle, freed via delete_font_handle.
	using font_handle = uintptr_t;

	// Font metrics returned alongside a created font.
	struct font_metrics_data
	{
		int height = 0;
		int ascent = 0;
		int descent = 0;
		int x_height = 0;
		int avg_char_width = 0;
	};

	struct font_def
	{
		std::string face;
		int size = 12; // in points
		int weight = 400; // 100..900
		bool italic = false;
		bool underline = false;
		bool strikeout = false;
	};

	// Create / destroy a platform font. Created handles are NOT cached by
	// the platform; the caller is responsible for caching and lifetime.
	font_handle create_font_handle(const font_def& def, font_metrics_data* out_metrics = nullptr);
	void delete_font_handle(font_handle h);

	// Measure text or font line height using a previously created handle.
	isize measure_text_with_font(font_handle h, std::string_view text);
	int line_height_for_font(font_handle h);

	// Resolve a (possibly relative) URL against an absolute base URL.
	// Mirrors the previous shlwapi-based behaviour.
	std::string resolve_url(std::string_view base, std::string_view rel);


	// Measure / Draw contexts
	//
	struct measure_context
	{
		virtual ~measure_context() = default;
		virtual isize measure_text(std::string_view text, const font& f) const = 0;
		virtual isize measure_char(const font& f) const = 0;
	};

	struct draw_context
	{
		virtual ~draw_context() = default;

		// Clip region — the dirty rectangle that needs repainting
		virtual irect clip_rect() const = 0;

		// Fill operations
		virtual void fill_solid_rect(const irect& rc, color_t color) = 0;
		virtual void fill_solid_rect(int x, int y, int cx, int cy, color_t color) = 0;

		// Text output
		virtual void draw_text(int x, int y, const irect& clip, std::string_view text,
		                       const font& f, color_t text_color, color_t bg_color) = 0;
		virtual isize measure_text(std::string_view text, const font& f) const = 0;

		// Text output using a cached platform font handle (see create_font_handle).
		// Always draws with a transparent background; fill the background separately
		// with fill_solid_rect if needed.
		virtual void draw_text_h(int x, int y, std::string_view text,
		                         font_handle handle, color_t text_color) = 0;
		virtual isize measure_text_h(std::string_view text, font_handle handle) const = 0;

		// Line drawing
		virtual void draw_lines(std::span<const ipoint> points, color_t color) = 0;

		// Solid-color line segment (logical pixel coords). Width is integer pixels.
		virtual void draw_solid_line(ipoint a, ipoint b, color_t color, int width = 1) = 0;

		// Outlined / filled ellipse fitting the bounding rect (x,y,w,h).
		virtual void draw_ellipse(int x, int y, int w, int h, color_t color, int line_width = 1) = 0;
		virtual void fill_ellipse(int x, int y, int w, int h, color_t color) = 0;

		// Push a clip rectangle. Subsequent draws will be clipped to this rect
		// in addition to clip_rect(). Pair with clear_clip_rect.
		virtual void set_clip_rect(const irect& rc) = 0;
		virtual void clear_clip_rect() = 0;

		// Bitmap blit (top-left at x,y, no scaling). The bitmap is drawn via the
		// platform's fast path (StretchDIBits/SetDIBitsToDevice on Win32).
		virtual void draw_bitmap(int x, int y, const struct bitmap& bmp) = 0;

		// Bitmap blit with scaling into dest rectangle.
		virtual void draw_bitmap(const irect& dest, const struct bitmap& bmp) = 0;
	};

	// Wrap a native device context (HDC on Win32) into a transient draw_context.
	// The returned context does NOT own the native DC. Use this in window-paint
	// handlers that already have an HDC from BeginPaint.
	std::unique_ptr<draw_context> wrap_native_dc(uintptr_t native_dc, const irect& clip);

	// Wrap a native device context for measuring only.
	std::unique_ptr<measure_context> wrap_native_dc_measure(uintptr_t native_dc);

	struct frame_reactor;
	struct window_frame;
	struct toolbar_frame;
	struct address_bar_config;

	using window_frame_ptr = std::shared_ptr<window_frame>;
	using frame_reactor_ptr = std::shared_ptr<frame_reactor>;
	using toolbar_frame_ptr = std::shared_ptr<toolbar_frame>;

	// window_frame — Platform-independent window abstraction
	struct window_frame
	{
		virtual ~window_frame() = default;

		// Reactor binding
		virtual void set_reactor(frame_reactor_ptr reactor) = 0;
		virtual void notify_size() = 0;

		virtual irect get_client_rect() const = 0;

		virtual void invalidate() = 0;
		virtual void invalidate_rect(const irect& rect) = 0;

		// Focus & capture
		virtual void set_focus() = 0;
		virtual bool has_focus() const = 0;
		virtual void set_capture() = 0;
		virtual void release_capture() = 0;
		// Timers
		virtual uint32_t set_timer(uint32_t id, uint32_t ms) = 0;
		virtual void kill_timer(uint32_t id) = 0;
		// Coordinate mapping
		virtual ipoint screen_to_client(ipoint pt) const = 0;
		// Cursor
		virtual void set_cursor_shape(cursor_shape shape) = 0;
		// Window management
		virtual void move_window(const irect& bounds) = 0;
		virtual void show(bool visible) = 0;
		virtual bool is_visible() const = 0;
		virtual void set_text(std::string_view text) = 0;
		// Clipboard
		virtual std::string text_from_clipboard() = 0;
		virtual bool text_to_clipboard(std::string_view text) = 0;

		// Window placement
		struct placement
		{
			irect normal_bounds;
			bool maximized = false;
		};

		virtual placement get_placement() const = 0;
		virtual void set_placement(const placement& p) = 0;
		// Mouse tracking
		virtual void track_mouse_leave() = 0;
		// Key state
		virtual bool is_key_down(unsigned int vk) const = 0;
		virtual bool is_key_down_async(unsigned int vk) const = 0;
		// Child windows
		virtual window_frame_ptr create_child(std::string_view class_name, uint32_t style,
		                                      color_t background) const & = 0;
		virtual void close() = 0;
		virtual int message_box(std::string_view text, std::string_view title, uint32_t style) = 0;
		// Menu
		virtual void set_menu(std::vector<menu_command> menu_def) = 0;
		// Measure context
		virtual std::unique_ptr<measure_context> create_measure_context() const = 0;
		// Popup menu
		virtual void show_popup_menu(const std::vector<menu_command>& items, const ipoint& screen_pt) = 0;
		// DPI
		virtual double get_dpi_scale() const = 0;
		// Drag and drop
		virtual void accept_drop_files(bool accept) = 0;

		// Create a platform-rendered toolbar / address bar child window.
		// The returned toolbar_frame is itself a window_frame (see toolbar_frame::frame())
		// so its bounds can be positioned via move_window().
		virtual toolbar_frame_ptr create_address_bar(const address_bar_config& cfg) = 0;
	};

	// frame_reactor — Event handler for window_frame
	struct frame_reactor
	{
		virtual ~frame_reactor() = default;
		virtual uint32_t handle_message(window_frame_ptr window, message_type message, uintptr_t wParam,
		                                intptr_t lParam) = 0;

		virtual uint32_t handle_mouse(window_frame_ptr window, mouse_message_type message,
		                              const mouse_params& params)
		{
			return 0;
		}

		virtual uint32_t handle_keyboard(window_frame_ptr window, keyboard_message_type message,
		                                 const keyboard_params& params)
		{
			return 0;
		}

		virtual void handle_paint(window_frame_ptr& window, draw_context& draw) = 0;
		virtual void handle_size(window_frame_ptr& window, isize extent, measure_context& measure) = 0;
	};

	// Cursor position (global, not window-specific)
	ipoint platform_cursor_pos();

	// Primary screen size in pixels
	isize platform_screen_size();

	// Primary screen DPI (logical pixels per inch). Returns 96 if unknown.
	int platform_screen_dpi();

	// Load an embedded text resource (e.g. master.css). Returns empty on failure.
	std::string platform_load_text_resource(int id);

	// Dialog / Message box constants
	namespace dialog_id
	{
		constexpr int ok = 1;
		constexpr int cancel = 2;
	}

	namespace msg_box_style
	{
		constexpr uint32_t ok = 0x0000;
		constexpr uint32_t yes_no = 0x0004;
		constexpr uint32_t yes_no_cancel = 0x0003;
		constexpr uint32_t icon_warning = 0x0030;
		constexpr uint32_t icon_question = 0x0020;
	}

	namespace msg_box_result
	{
		constexpr int yes = 6;
		constexpr int no = 7;
		constexpr int cancel = 2;
	}

	inline int mul_div(const int a, const int b, const int c)
	{
		return static_cast<int>(static_cast<int64_t>(a) * b / c);
	}

	// File system
	//
	bool is_directory(const file_path& path);
	file_path current_directory();

	// File dialog
	file_path open_file_path(std::string_view title, std::string_view filters);
	file_path save_file_path(std::string_view title, const file_path& default_path, std::string_view filters);

	// File iteration
	struct file_attributes_t
	{
		bool is_readonly = false;
		bool is_offline = false;
		bool is_hidden = false;
		uint64_t modified = 0;
		uint64_t created = 0;
		uint64_t size = 0;
	};

	struct file_info
	{
		file_path path;
		file_attributes_t attributes;
	};

	struct folder_info
	{
		file_path path;
		file_attributes_t attributes;
	};

	struct folder_contents
	{
		std::vector<folder_info> folders;
		std::vector<file_info> files;
	};

	folder_contents iterate_file_items(const file_path& folder, bool show_hidden);

	uint64_t file_modified_time(const file_path& path);

	bool platform_events();
	void platform_set_menu(std::vector<menu_command> menuDef);

	// Platform message loop (returns process exit code)
	int platform_run();

	// Timer
	double platform_get_time();
	void platform_sleep(int milliseconds);

	// Resource loading
	void* platform_load_resource(std::string_view name, std::string_view type);

	void platform_show_error(std::string_view message, std::string_view title);

	// Platform locale
	std::string platform_language();

	// Spell checking
	struct spell_checker
	{
		virtual ~spell_checker() = default;
		virtual bool available() const = 0;
		virtual std::string diagnostics() const = 0;
		virtual bool is_word_valid(std::string_view word) = 0;
		virtual std::vector<std::string> suggest(std::string_view word) = 0;
		virtual void add_word(std::string_view word) = 0;
	};

	std::unique_ptr<spell_checker> create_spell_checker();

	// File I/O
	struct file_handle
	{
		virtual ~file_handle() = default;
		virtual bool read(uint8_t* buffer, uint32_t bytesToRead, uint32_t* bytesRead) = 0;
		virtual uint32_t size() const = 0;
	};

	using file_handle_ptr = std::shared_ptr<file_handle>;

	file_handle_ptr open_for_read(const file_path& path);

	// Writable file handle
	struct writable_file_handle
	{
		virtual ~writable_file_handle() = default;
		virtual uint32_t write(const uint8_t* buffer, uint32_t bytes) = 0;
	};

	using writable_file_handle_ptr = std::shared_ptr<writable_file_handle>;

	writable_file_handle_ptr open_file_for_write(const file_path& path);

	// File operations
	bool platform_move_file_replace(const char* source, const char* dest);
	std::string platform_temp_file_path(const char* prefix);
	std::string platform_last_error_message();
	bool platform_recycle_file(const file_path& path);
	bool platform_rename_file(const file_path& old_path, const file_path& new_path);
	bool platform_create_directory(const file_path& path);
	bool platform_copy_file(const file_path& source, const file_path& dest, bool fail_if_exists);

	// Drag and drop
	std::vector<file_path> dropped_file_paths(uintptr_t drop_handle);

	// Clipboard
	bool platform_clipboard_has_text();
	std::string platform_text_from_clipboard();
	bool platform_text_to_clipboard(std::string_view text);

	// Bitmap resource loading
	struct bitmap_data
	{
		int width;
		int height;
		std::vector<uint32_t> pixels;
	};

	std::optional<bitmap_data> platform_load_bitmap_resource(std::string_view resName);


	void debug_trace(const std::string& msg);
	void write_stdout(std::string_view text);

	// Configuration (INI file)
	std::string config_read(std::string_view section, std::string_view key,
	                        std::string_view default_value = {});
	void config_write(std::string_view section, std::string_view key, std::string_view value);

	// background tasks
	void run_async(std::function<void()> task);
	void run_ui(std::function<void()> task);

	// network
	bool is_online();

	using web_params = std::vector<std::pair<std::string, std::string>>;

	enum class web_request_verb
	{
		POST,
		GET
	};

	struct web_request
	{
		std::string command;
		std::string path;
		std::string body;

		web_params query;
		web_params headers;
		web_params form_data;

		std::string file_form_data_name;
		std::string file_name;
		file_path upload_file_path;

		file_path download_file_path;

		web_request_verb verb = web_request_verb::GET;
	};

	struct web_response
	{
		std::string headers;
		std::string body;
		std::string content_type;
		int status_code = 0;
	};

	struct web_host;
	using web_host_ptr = std::shared_ptr<web_host>;

	web_host_ptr connect_to_host(std::string_view host, bool secure = true, int port = 0,
	                             std::string_view user_agent = {});
	web_response send_request(const web_host_ptr& host, const web_request& req);

	// ── Bitmap (WIC-backed) ──────────────────────────────────────────────────
	//
	// A device-independent 32-bit BGRA bitmap, top-down. Loaded via WIC on
	// Windows so any image format installed on the system (PNG, JPEG, GIF,
	// BMP, TIFF, ICO, etc.) is supported. Drawn via SetDIBitsToDevice /
	// StretchDIBits in draw_context::draw_bitmap.
	struct bitmap : bitmap_data
	{
		bitmap() = default;

		bitmap(int w, int h, std::vector<uint32_t> px)
		{
			width = w;
			height = h;
			pixels = std::move(px);
		}

		[[nodiscard]] bool empty() const { return pixels.empty(); }
	};

	using bitmap_ptr = std::shared_ptr<bitmap>;

	// Load a bitmap from a file on disk.
	bitmap_ptr load_bitmap_file(const file_path& path);

	// Load a bitmap from in-memory encoded image bytes.
	bitmap_ptr load_bitmap_memory(const uint8_t* data, size_t size);

	// Load an encoded image from an embedded resource (RT_RCDATA by default).
	bitmap_ptr load_bitmap_named_resource(std::string_view name, std::string_view type = "RCDATA");

	// ── Async HTTP ────────────────────────────────────────────────────────────
	//
	// A platform async HTTP client. Callbacks are dispatched from a background
	// thread; marshal to the UI thread with run_ui() if necessary. Cancel a
	// request by calling cancel() on the returned handle; a cancelled request
	// will not invoke any further callbacks.
	struct async_http_callbacks
	{
		// Called once after headers are received. status_code is HTTP status.
		std::function<void(int status_code, std::string content_type, uint64_t content_length)> on_headers;
		// Called for each chunk of body data.
		std::function<void(const uint8_t* data, size_t size)> on_data;
		// Called once after the body has been fully received.
		std::function<void()> on_complete;
		// Called once on error. on_complete will not be called.
		std::function<void(std::string error)> on_error;
	};

	struct async_http_request
	{
		virtual ~async_http_request() = default;
		// Cancel the request. Callbacks will no longer fire after cancel returns.
		virtual void cancel() = 0;
	};

	using async_http_request_ptr = std::shared_ptr<async_http_request>;

	struct async_http_session
	{
		virtual ~async_http_session() = default;
		// Issue a GET for the given absolute URL (http or https).
		virtual async_http_request_ptr get(std::string_view url, async_http_callbacks cb) = 0;
		// Cancel all in-flight requests issued from this session.
		virtual void stop() = 0;
	};

	using async_http_session_ptr = std::shared_ptr<async_http_session>;

	// Create a new async HTTP session. user_agent is sent with every request.
	async_http_session_ptr create_async_http_session(std::string_view user_agent = {});

	// ── Toolbar / Address bar ─────────────────────────────────────────────────
	//
	// Platform-rendered toolbar widget. Two styles are supported:
	//   - menu        : a row of icon-font buttons (no edit field)
	//   - address_bar : icon-font buttons on either side with an editable URL
	//                   field in the middle (browser-style)
	//
	// Buttons are described declaratively. The icon is a code point in
	// font_name::segoe_icons (see icon_glyph for the standard set).
	enum class toolbar_style
	{
		menu,
		address_bar,
	};

	struct toolbar_button
	{
		uint32_t glyph = 0; // codepoint in icon font
		int id = 0; // app-defined identifier (passed to set_button_enabled)
		std::string tooltip;
		std::function<void()> action;
		std::function<bool()> is_enabled;
	};

	struct address_bar_config
	{
		toolbar_style style = toolbar_style::address_bar;

		std::vector<toolbar_button> left_buttons;
		std::vector<toolbar_button> right_buttons;

		// Fired when the user activates the address (presses Enter).
		std::function<void(std::string url)> on_navigate;

		// Optional. Called as the user types, on the UI thread. Returns
		// suggestions to show in the dropdown.
		std::function<std::vector<std::string>(std::string_view text)> on_suggest;

		// Optional initial URL.
		std::string initial_text;

		// Visual styling. Defaults are reasonable.
		color_t background = color_t(255, 255, 255);
		color_t edit_background = color_t(245, 245, 245);
		color_t text_color = color_t(0, 0, 0);
		color_t button_color = color_t(64, 64, 64);
		color_t button_hover = color_t(220, 220, 220);
		color_t border_color = color_t(200, 200, 200);

		int height = 54; // dialog units (scaled by DPI on use)
		int button_width = 54;
	};

	struct toolbar_frame
	{
		virtual ~toolbar_frame() = default;

		// Underlying window — use this for layout (move_window, etc.).
		virtual window_frame_ptr frame() = 0;

		// Preferred height in pixels.
		virtual int preferred_height() const = 0;

		// Address-bar text accessors (no-op for toolbar_style::menu).
		virtual void set_address_text(std::string_view text) = 0;
		[[nodiscard]] virtual std::string address_text() const = 0;
		virtual void focus_address() = 0;
		virtual void select_all_address() = 0;

		// Enable/disable a button by its declared id.
		virtual void set_button_enabled(int id, bool enabled) = 0;

		// Replace a button's glyph (e.g. swap refresh ⇄ stop while loading).
		virtual void set_button_glyph(int id, uint32_t glyph) = 0;

		// Attach a popup menu to a button (e.g. a "burger" / hamburger
		// menu). When the button is clicked, the menu is shown beneath it
		// and the button's own action callback is NOT invoked. Pass an
		// empty vector to detach the menu and restore action behaviour.
		virtual void set_menu(int button_id, std::vector<menu_command> items) = 0;
	};

	// Create an address-bar / toolbar widget as a child of an existing native
	// window (HWND on Win32, passed as uintptr_t). Use this when the parent
	// window is not yet a pf::window_frame. Returns a toolbar_frame whose
	// frame()->m_hWnd (Win32) lives inside the given parent.
	toolbar_frame_ptr create_address_bar_for_hwnd(uintptr_t parent_native_handle,
	                                              const address_bar_config& cfg);
}

struct app_init_result
{
	bool start_gui = true;
	int exit_code = 0;
};


// App callbacks implemented by the application layer
app_init_result app_init(const pf::window_frame_ptr& main_frame, std::span<const std::string_view> params);
void app_idle();
void app_destroy();
