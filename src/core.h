// core.h - Foundational types and utilities: geometry (position, recti, size_i),
// CSS enums, css_length with calc() support, web_color, string helpers, and
// font metrics. Included by all other headers.

#pragma once



using LPCBYTE = const BYTE*;

namespace sizing
{
	template <typename TDataType, size_t t_nElements>
	char (& lengthof_impl(const volatile TDataType (&)[t_nElements]))[t_nElements];

	template <typename TDataType, size_t t_nElements>
	char (& string_lengthof_impl(const volatile TDataType (&)[t_nElements]))[t_nElements - 1];

#define lengthof(dt) sizeof(::sizing::lengthof_impl(dt))
#define stringlengthof(dt) sizeof(::sizing::string_lengthof_impl(dt))
#define countof(dt) sizeof(::sizing::lengthof_impl(dt))
}


class document;
class element;

constexpr unsigned int font_decoration_none = 0x00;
constexpr unsigned int font_decoration_underline = 0x01;
constexpr unsigned int font_decoration_linethrough = 0x02;
constexpr unsigned int font_decoration_overline = 0x04;

using byte = unsigned char;

struct margins
{
	int left = 0;
	int right = 0;
	int top = 0;
	int bottom = 0;

	int width() const { return left + right; }
	int height() const { return top + bottom; }
};

struct size
{
	int width = 0;
	int height = 0;
};

struct position
{
	using vector = std::vector<position>;

	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;

	position() = default;

	position(const int x, const int y, const int width, const int height)
		: x(x), y(y), width(width), height(height)
	{
	}

	int right() const { return x + width; }
	int bottom() const { return y + height; }
	int left() const { return x; }
	int top() const { return y; }

	void operator+=(const margins& mg)
	{
		x -= mg.left;
		y -= mg.top;
		width += mg.left + mg.right;
		height += mg.top + mg.bottom;
	}

	void operator-=(const margins& mg)
	{
		x += mg.left;
		y += mg.top;
		width -= mg.left + mg.right;
		height -= mg.top + mg.bottom;
	}

	void clear()
	{
		x = y = width = height = 0;
	}

	void operator=(const size& sz)
	{
		width = sz.width;
		height = sz.height;
	}

	void move_to(const int x, const int y)
	{
		this->x = x;
		this->y = y;
	}

	bool does_intersect(const position* val) const
	{
		if (!val) return true;

		return (
				left() <= val->right() &&
				right() >= val->left() &&
				bottom() >= val->top() &&
				top() <= val->bottom())
			|| (
				val->left() <= right() &&
				val->right() >= left() &&
				val->bottom() >= top() &&
				val->top() <= bottom());
	}

	bool empty() const
	{
		return width == 0 && height == 0;
	}

	bool is_point_inside(const int x, const int y) const
	{
		return x >= left() && x <= right() && y >= top() && y <= bottom();
	}
};

struct font_metrics
{
	int height = 0;
	int ascent = 0;
	int descent = 0;
	int x_height = 0;
	bool draw_spaces = true;

	int base_line() const { return descent; }
};

struct font_item
{
	HFONT font;
	font_metrics metrics;
};

enum draw_flag
{
	draw_root,
	draw_block,
	draw_floats,
	draw_inlines,
	draw_positioned,
};

#define style_display_strings "none;block;inline;inline-block;list-item;table;table-caption;table-cell;table-column;table-column-group;table-footer-group;table-header-group;table-row;table-row-group;flex;inline-flex"

enum style_display
{
	display_none,
	display_block,
	display_inline,
	display_inline_block,
	display_list_item,
	display_table,
	display_table_caption,
	display_table_cell,
	display_table_column,
	display_table_column_group,
	display_table_footer_group,
	display_table_header_group,
	display_table_row,
	display_table_row_group,
	display_flex,
	display_inline_flex,
	display_inline_text,
};

#define flex_direction_strings "row;row-reverse;column;column-reverse"

enum flex_direction
{
	flex_direction_row,
	flex_direction_row_reverse,
	flex_direction_column,
	flex_direction_column_reverse,
};

#define flex_wrap_strings "nowrap;wrap;wrap-reverse"

enum flex_wrap
{
	flex_wrap_nowrap,
	flex_wrap_wrap,
	flex_wrap_wrap_reverse,
};

#define flex_justify_content_strings "flex-start;flex-end;center;space-between;space-around;space-evenly"

enum flex_justify_content
{
	flex_justify_content_flex_start,
	flex_justify_content_flex_end,
	flex_justify_content_center,
	flex_justify_content_space_between,
	flex_justify_content_space_around,
	flex_justify_content_space_evenly,
};

#define flex_align_items_strings "stretch;flex-start;flex-end;center;baseline"

enum flex_align_items
{
	flex_align_items_stretch,
	flex_align_items_flex_start,
	flex_align_items_flex_end,
	flex_align_items_center,
	flex_align_items_baseline,
};

enum style_border
{
	border_nope,
	border_none,
	border_hidden,
	border_dotted,
	border_dashed,
	border_solid,
	border_double
};

#define font_size_strings "xx-small;x-small;small;medium;large;x-large;xx-large;smaller;larger"

enum font_size
{
	font_size_xx_small,
	font_size_x_small,
	font_size_small,
	font_size_medium,
	font_size_large,
	font_size_x_large,
	font_size_xx_large,
	font_size_smaller,
	font_size_larger,
};

#define font_style_strings "normal;italic"

enum font_style
{
	font_style_normal,
	font_style_italic
};

#define font_variant_strings "normal;small-caps"

enum font_variant
{
	font_variant_normal,
	font_variant_italic
};

#define font_weight_strings "normal;bold;bolder;lighter;100;200;300;400;500;600;700"

enum font_weight
{
	font_weight_normal,
	font_weight_bold,
	font_weight_bolder,
	font_weight_lighter,
	font_weight_100,
	font_weight_200,
	font_weight_300,
	font_weight_400,
	font_weight_500,
	font_weight_600,
	font_weight_700
};

#define list_style_type_strings "none;circle;disc;square;armenian;cjk-ideographic;decimal;decimal-leading-zero;georgian;hebrew;hiragana;hiragana-iroha;katakana;katakana-iroha;lower-alpha;lower-greek;lower-latin;lower-roman;upper-alpha;upper-latin;upper-roman"

enum list_style_type
{
	list_style_type_none,
	list_style_type_circle,
	list_style_type_disc,
	list_style_type_square,
	list_style_type_armenian,
	list_style_type_cjk_ideographic,
	list_style_type_decimal,
	list_style_type_decimal_leading_zero,
	list_style_type_georgian,
	list_style_type_hebrew,
	list_style_type_hiragana,
	list_style_type_hiragana_iroha,
	list_style_type_katakana,
	list_style_type_katakana_iroha,
	list_style_type_lower_alpha,
	list_style_type_lower_greek,
	list_style_type_lower_latin,
	list_style_type_lower_roman,
	list_style_type_upper_alpha,
	list_style_type_upper_latin,
	list_style_type_upper_roman,
};

#define list_style_position_strings "inside;outside"

enum list_style_position
{
	list_style_position_inside,
	list_style_position_outside
};

#define vertical_align_strings "baseline;sub;super;top;text-top;middle;bottom;text-bottom"

enum vertical_align
{
	va_baseline,
	va_sub,
	va_super,
	va_top,
	va_text_top,
	va_middle,
	va_bottom,
	va_text_bottom
};

#define border_width_strings "thin;medium;thick"

enum border_width
{
	border_width_thin,
	border_width_medium,
	border_width_thick
};

#define border_style_strings "none;hidden;dotted;dashed;solid;double;groove;ridge;inset;outset"

enum border_style
{
	border_style_none,
	border_style_hidden,
	border_style_dotted,
	border_style_dashed,
	border_style_solid,
	border_style_double,
	border_style_groove,
	border_style_ridge,
	border_style_inset,
	border_style_outset
};

#define element_float_strings "none;left;right"

enum element_float
{
	float_none,
	float_left,
	float_right
};

#define element_clear_strings "none;left;right;both"

enum element_clear
{
	clear_none,
	clear_left,
	clear_right,
	clear_both
};

#define css_units_strings "none;%;in;cm;mm;em;ex;pt;pc;px;dpi;dpcm;rem;vw;vh;vmin;vmax"

enum css_units
{
	css_units_none,
	css_units_percentage,
	css_units_in,
	css_units_cm,
	css_units_mm,
	css_units_em,
	css_units_ex,
	css_units_pt,
	css_units_pc,
	css_units_px,
	css_units_dpi,
	css_units_dpcm,
	css_units_rem,
	css_units_vw,
	css_units_vh,
	css_units_vmin,
	css_units_vmax,
};

#define background_attachment_strings "scroll;fixed"

enum background_attachment
{
	background_attachment_scroll,
	background_attachment_fixed
};

#define background_repeat_strings "repeat;repeat-x;repeat-y;no-repeat"

enum background_repeat
{
	background_repeat_repeat,
	background_repeat_repeat_x,
	background_repeat_repeat_y,
	background_repeat_no_repeat
};

#define background_box_strings "border-box;padding-box;content-box"

enum background_box
{
	background_box_border,
	background_box_padding,
	background_box_content
};

#define element_position_strings "static;relative;absolute;fixed"

enum element_position
{
	element_position_static,
	element_position_relative,
	element_position_absolute,
	element_position_fixed,
};

#define text_align_strings "left;right;center;justify"

enum text_align
{
	text_align_left,
	text_align_right,
	text_align_center,
	text_align_justify
};

#define text_transform_strings "none;capitalize;uppercase;lowercase"

enum text_transform
{
	text_transform_none,
	text_transform_capitalize,
	text_transform_uppercase,
	text_transform_lowercase
};

#define white_space_strings "normal;nowrap;pre;pre-line;pre-wrap"

enum white_space
{
	white_space_normal,
	white_space_nowrap,
	white_space_pre,
	white_space_pre_line,
	white_space_pre_wrap
};

#define overflow_strings "visible;hidden;scroll;auto;no-display;no-content"

enum overflow
{
	overflow_visible,
	overflow_hidden,
	overflow_scroll,
	overflow_auto,
	overflow_no_display,
	overflow_no_content
};

#define background_size_strings "auto;cover;contain"

enum background_size
{
	background_size_auto,
	background_size_cover,
	background_size_contain,
};

#define visibility_strings "visible;hidden;collapse"

enum visibility
{
	visibility_visible,
	visibility_hidden,
	visibility_collapse,
};

#define border_collapse_strings "collapse;separate"

enum border_collapse
{
	border_collapse_collapse,
	border_collapse_separate,
};


#define pseudo_class_strings "only-child;only-of-type;first-child;first-of-type;last-child;last-of-type;nth-child;nth-of-type;nth-last-child;nth-last-of-type;not;root"

enum pseudo_class
{
	pseudo_class_only_child,
	pseudo_class_only_of_type,
	pseudo_class_first_child,
	pseudo_class_first_of_type,
	pseudo_class_last_child,
	pseudo_class_last_of_type,
	pseudo_class_nth_child,
	pseudo_class_nth_of_type,
	pseudo_class_nth_last_child,
	pseudo_class_nth_last_of_type,
	pseudo_class_not,
	pseudo_class_root,
};

#define content_property_string "none;normal;open-quote;close-quote;no-open-quote;no-close-quote"

enum content_property
{
	content_property_none,
	content_property_normal,
	content_property_open_quote,
	content_property_close_quote,
	content_property_no_open_quote,
	content_property_no_close_quote,
};


struct floated_box
{
	position pos;
	element_float float_side = float_none;
	element_clear clear_floats = clear_none;
	element* el = nullptr;
};

struct int_int_cache
{
	int hash = 0;
	int val = 0;
	bool is_valid = false;
	bool is_default = false;

	void invalidate()
	{
		is_valid = false;
		is_default = false;
	}

	void set_value(const int vHash, const int vVal)
	{
		hash = vHash;
		val = vVal;
		is_valid = true;
	}
};

enum select_result
{
	select_no_match = 0x00,
	select_match = 0x01,
	select_match_pseudo_class = 0x02,
	select_match_with_before = 0x10,
	select_match_with_after = 0x20,
};

template <class T>
class def_value
{
	T m_val;
	bool m_is_default;

public:
	def_value(T def_val)
	{
		m_is_default = true;
		m_val = def_val;
	}

	void reset(T def_val)
	{
		m_is_default = true;
		m_val = def_val;
	}

	bool is_default()
	{
		return m_is_default;
	}

	T operator=(T new_val)
	{
		m_val = new_val;
		m_is_default = false;
		return m_val;
	}

	operator T()
	{
		return m_val;
	}
};


#define media_orientation_strings "portrait;landscape"

enum media_orientation
{
	media_orientation_portrait,
	media_orientation_landscape,
};

#define media_feature_strings "none;width;min-width;max-width;height;min-height;max-height;device-width;min-device-width;max-device-width;device-height;min-device-height;max-device-height;orientation;aspect-ratio;min-aspect-ratio;max-aspect-ratio;device-aspect-ratio;min-device-aspect-ratio;max-device-aspect-ratio;color;min-color;max-color;color-index;min-color-index;max-color-index;monochrome;min-monochrome;max-monochrome;resolution;min-resolution;max-resolution"

enum media_feature
{
	media_feature_none,

	media_feature_width,
	media_feature_min_width,
	media_feature_max_width,

	media_feature_height,
	media_feature_min_height,
	media_feature_max_height,

	media_feature_device_width,
	media_feature_min_device_width,
	media_feature_max_device_width,

	media_feature_device_height,
	media_feature_min_device_height,
	media_feature_max_device_height,

	media_feature_orientation,

	media_feature_aspect_ratio,
	media_feature_min_aspect_ratio,
	media_feature_max_aspect_ratio,

	media_feature_device_aspect_ratio,
	media_feature_min_device_aspect_ratio,
	media_feature_max_device_aspect_ratio,

	media_feature_color,
	media_feature_min_color,
	media_feature_max_color,

	media_feature_color_index,
	media_feature_min_color_index,
	media_feature_max_color_index,

	media_feature_monochrome,
	media_feature_min_monochrome,
	media_feature_max_monochrome,

	media_feature_resolution,
	media_feature_min_resolution,
	media_feature_max_resolution,
};

#define box_sizing_strings "content-box;border-box"

enum box_sizing
{
	box_sizing_content_box,
	box_sizing_border_box,
};


#define media_type_strings "none;all;screen;print;braille;embossed;handheld;projection;speech;tty;tv"

enum media_type
{
	media_type_none,
	media_type_all,
	media_type_screen,
	media_type_print,
	media_type_braille,
	media_type_embossed,
	media_type_handheld,
	media_type_projection,
	media_type_speech,
	media_type_tty,
	media_type_tv,
};

struct media_features
{
	media_type type;
	int width;
	// (pixels) For continuous media, this is the width of the viewport including the size of a rendered scroll bar (if any). For paged media, this is the width of the page box.
	int height;
	// (pixels) The height of the targeted display area of the output device. For continuous media, this is the height of the viewport including the size of a rendered scroll bar (if any). For paged media, this is the height of the page box.
	int device_width;
	// (pixels) The width of the rendering surface of the output device. For continuous media, this is the width of the screen. For paged media, this is the width of the page sheet size.
	int device_height;
	// (pixels) The height of the rendering surface of the output device. For continuous media, this is the height of the screen. For paged media, this is the height of the page sheet size.
	int color;
	// The number of bits per color component of the output device. If the device is not a color device, the value is zero.
	int color_index;
	// The number of entries in the color lookup table of the output device. If the device does not use a color lookup table, the value is zero.
	int monochrome;
	// The number of bits per pixel in a monochrome frame buffer. If the device is not a monochrome device, the output device value will be 0.
	int resolution; // The resolution of the output device (in DPI)
};

enum render_type
{
	render_all,
	render_no_fixed,
	render_fixed_only,
};

// List of the Void Elements (can't have any contents)
const auto void_elements = "area;base;br;col;command;embed;hr;img;input;keygen;link;meta;param;source;track;wbr";


class size_i;
class point_i;
class recti;

static constexpr double d_pi = 3.14159265358979323846;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


inline double to_radian(const double theta)
{
	return theta * (d_pi / 180.0);
}

inline double to_degrees(const double theta)
{
	return theta / (d_pi / 180.0);
}

inline int clamp_i(const int& v, const int& l, const int& h)
{
	if (h <= l) return l;
	return v < l ? l : v > h ? h : v;
}

class size_i : public SIZE
{
public:
	size_i()
	{
		cx = 0;
		cy = 0;
	}

	size_i(const int initCX, const int initCY)
	{
		cx = initCX;
		cy = initCY;
	}

	size_i(const SIZE& initSize)
	{
		*static_cast<SIZE*>(this) = initSize;
	}

	size_i(const POINT& initPt)
	{
		*(POINT*)this = initPt;
	}

	void set(const int initCX, const int initCY)
	{
		cx = initCX;
		cy = initCY;
	}

	bool operator ==(const SIZE& size) const
	{
		return cx == size.cx && cy == size.cy;
	}

	bool operator !=(const SIZE& size) const
	{
		return cx != size.cx || cy != size.cy;
	}

	bool is_empty() const
	{
		return cx == 0 && cy == 0;
	}

	size_i operator +(const SIZE& size) const
	{
		return size_i(cx + size.cx, cy + size.cy);
	}

	size_i operator -(const SIZE& size) const
	{
		return size_i(cx - size.cx, cy - size.cy);
	}

	size_i operator -() const
	{
		return size_i(-cx, -cy);
	}

	point_i operator +(const POINT& point) const;
	point_i operator -(const POINT& point) const;
};

class point_i : public POINT
{
public:
	point_i()
	{
		x = 0;
		y = 0;
	}

	point_i(const int initX, const int initY)
	{
		x = initX;
		y = initY;
	}

	point_i(const POINT& initPt)
	{
		*static_cast<POINT*>(this) = initPt;
	}

	point_i(const SIZE& initSize)
	{
		*(SIZE*)this = initSize;
	}

	bool operator ==(const POINT& point) const
	{
		return x == point.x && y == point.y;
	}

	bool operator !=(const POINT& point) const
	{
		return x != point.x || y != point.y;
	}

	point_i operator +(const SIZE& size) const
	{
		return point_i(x + size.cx, y + size.cy);
	}

	point_i operator -(const SIZE& size) const
	{
		return point_i(x - size.cx, y - size.cy);
	}

	point_i operator -() const
	{
		return point_i(-x, -y);
	}

	point_i operator +(const POINT& point) const
	{
		return point_i(x + point.x, y + point.y);
	}

	size_i operator -(const POINT& point) const
	{
		return size_i(x - point.x, y - point.y);
	}

	point_i clamp(const RECT& limit) const
	{
		return point_i(clamp_i(x, limit.left, limit.right), clamp_i(y, limit.top, limit.bottom));
	}
};

class recti : public RECT
{
public:
	// Constructors
	recti()
	{
		left = 0;
		top = 0;
		right = 0;
		bottom = 0;
	}

	recti(const int l, const int t, const int r, const int b)
	{
		left = l;
		top = t;
		right = r;
		bottom = b;
	}

	recti(const RECT& other)
	{
		copy(&other);
	}

	recti(const LPCRECT other)
	{
		copy(other);
	}

	recti(const POINT& point, const SIZE& size)
	{
		right = (left = point.x) + size.cx;
		bottom = (top = point.y) + size.cy;
	}

	recti(const POINT& topLeft, const POINT& bottomRight)
	{
		left = topLeft.x;
		top = topLeft.y;
		right = bottomRight.x;
		bottom = bottomRight.y;
	}

	void copy(const LPCRECT other)
	{
		left = other->left;
		top = other->top;
		right = other->right;
		bottom = other->bottom;
	}

	int width() const
	{
		return right - left;
	}

	int height() const
	{
		return bottom - top;
	}

	size_i size() const
	{
		return size_i(right - left, bottom - top);
	}

	point_i top_left() const
	{
		return point_i(left, top);
	}

	point_i bottom_right() const
	{
		return point_i(right, bottom);
	}

	point_i center() const
	{
		return point_i((left + right) / 2, (top + bottom) / 2);
	}

	operator LPRECT()
	{
		return this;
	}

	operator LPCRECT() const
	{
		return this;
	}

	bool is_empty() const
	{
		return left >= right || top >= bottom;
	}

	bool is_null() const
	{
		return left == 0 && right == 0 && top == 0 && bottom == 0;
	}

	bool contains(const POINT& point) const
	{
		return left <= point.x && right >= point.x && top <= point.y && bottom >= point.y;
	}

	void clear()
	{
		left = right = top = bottom = 0;
	}

	void set(const int x1, const int y1, const int x2, const int y2)
	{
		left = x1;
		top = y1;
		right = x2;
		bottom = y2;
	}

	void set(const POINT& topLeft, const POINT& bottomRight)
	{
		set(topLeft.x, topLeft.y, bottomRight.x, bottomRight.y);
	}

	recti inflate(const int xy) const
	{
		return recti(left - xy, top - xy, right + xy, bottom + xy);
	}

	recti inflate(const int x, const int y) const
	{
		return recti(left - x, top - y, right + x, bottom + y);
	}

	recti inflate(const size_i& s) const
	{
		return recti(left - s.cx, top - s.cy, right + s.cx, bottom + s.cy);
	}

	bool intersects(const recti& other) const
	{
		return left < other.right &&
			top < other.bottom &&
			right > other.left &&
			bottom > other.top;
	}

	recti intersection(const recti& other) const
	{
		if (!intersects(other)) return recti();
		return recti(std::max(left, other.left), std::max(top, other.top), std::min(right, other.right),
		             std::min(bottom, other.bottom));
	}

	recti union_with(const recti& other) const
	{
		if (is_empty()) return other;
		if (other.is_empty()) return this;
		return recti(std::min(left, other.left), std::min(top, other.top), std::max(right, other.right),
		             std::max(bottom, other.bottom));
	}

	recti clamp(const recti& limit) const
	{
		size_i off(0, 0);

		if (top < limit.top)
			off.cy = limit.top - top;

		if (left < limit.left)
			off.cx = limit.left - left;

		if (bottom > limit.bottom)
			off.cy = limit.bottom - bottom;

		if (right > limit.right)
			off.cx = limit.right - right;

		return offset(off);
	}

	recti crop(const recti& limit) const
	{
		recti result(this);

		if (top < limit.top) result.top = limit.top;
		if (left < limit.left) result.left = limit.left;
		if (bottom > limit.bottom) result.bottom = limit.bottom;
		if (right > limit.right) result.right = limit.right;

		return result;
	}

	recti offset(const point_i& pt) const
	{
		return recti(left + pt.x, top + pt.y, right + pt.x, bottom + pt.y);
	}

	recti offset(const int x, const int y) const
	{
		return recti(left + x, top + y, right + x, bottom + y);
	}

	void operator =(const RECT& other)
	{
		copy(&other);
	}

	bool operator ==(const RECT& other) const
	{
		return left == other.left && top == other.top && right == other.right && bottom == other.bottom;
	}

	bool operator !=(const RECT& other) const
	{
		return left != other.left || top != other.top || right != other.right || bottom != other.bottom;
	}

	operator Gdiplus::Rect() const { return Gdiplus::Rect(left, top, width(), height()); };
};


inline point_i size_i::operator +(const POINT& point) const
{
	return point_i(cx + point.x, cy + point.y);
}

inline point_i size_i::operator -(const POINT& point) const
{
	return point_i(cx - point.x, cy - point.y);
}

inline recti center_rect(const size_i& s, const int xx, const int yy)
{
	const auto x = xx - s.cx / 2;
	const auto y = yy - s.cy / 2;
	return recti(x, y, x + s.cx, y + s.cy);
}

inline recti center_rect(const size_i& s, const recti& limit)
{
	const auto center = limit.center();
	return center_rect(s, center.x, center.y);
}

inline recti center_rect(const size_i& s, const size_i& limit)
{
	return center_rect(s, limit.cx / 2, limit.cy / 2);
}

inline recti center_rect(const size_i& s, const point_i& limit)
{
	return center_rect(s, limit.x, limit.y);
}

inline recti center_rect(const recti& r, const recti& limit)
{
	return center_rect(r.size(), limit);
}


extern std::string empty;

inline bool is_empty(const char* sz)
{
	return sz == nullptr || sz[0] == 0;
}

inline const char* strichr(const char* text, const char cc)
{
	const auto c = isupper(cc) ? tolower(cc) : cc;

	while (*text)
	{
		const auto t = isupper(*text) ? tolower(*text) : *text;
		if (t == c) return text;
		text++;
	}

	return nullptr;
}

inline std::wstring to_utf16(const char* sz)
{
	if (!sz || !*sz) return std::wstring();
	const auto len = static_cast<int>(strlen(sz));
	const int size_needed = MultiByteToWideChar(CP_UTF8, 0, sz, len, nullptr, 0);
	std::wstring result(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, sz, len, &result[0], size_needed);
	return result;
}

inline std::wstring to_utf16(const std::string& str)
{
	if (str.empty()) return std::wstring();
	const int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), nullptr, 0);
	std::wstring result(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), &result[0], size_needed);
	return result;
}

inline std::string to_utf8(const std::wstring& wstr)
{
	if (wstr.empty()) return std::string();
	const int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), nullptr, 0,
	                                            nullptr, nullptr);
	std::string result(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), &result[0], size_needed, nullptr, nullptr);
	return result;
}

inline int clamp(const int v, const int l, const int r)
{
	return std::clamp(v, l, r);
}

inline int safe_stoi(const std::string& str, const int def = 0)
{
	char* end = nullptr;
	const auto val = strtol(str.c_str(), &end, 10);
	return (end != str.c_str()) ? static_cast<int>(val) : def;
}

inline int safe_stoi(const char* str, const int def = 0)
{
	char* end = nullptr;
	const auto val = strtol(str, &end, 10);
	return (end != str) ? static_cast<int>(val) : def;
}

inline long safe_stol(const std::string& str, const int base = 10, const long def = 0)
{
	char* end = nullptr;
	const auto val = strtol(str.c_str(), &end, base);
	return (end != str.c_str()) ? val : def;
}

inline long safe_stol(const char* str, const int base = 10, const long def = 0)
{
	char* end = nullptr;
	const auto val = strtol(str, &end, base);
	return (end != str) ? val : def;
}

inline float safe_stof(const std::string& str, const float def = 0.0f)
{
	char* end = nullptr;
	const auto val = strtof(str.c_str(), &end);
	return (end != str.c_str()) ? val : def;
}

inline float safe_stof(const char* str, const float def = 0.0f)
{
	char* end = nullptr;
	const auto val = strtof(str, &end);
	return (end != str) ? val : def;
}


inline bool is_quote(const char c)
{
	return c == '\"' || c == '\'';
}

inline int is_space_char(const int ch)
{
	return isspace(static_cast<unsigned char>(ch)) || ch == 0;
}

inline void trim(std::string& s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](const int ch) { return !is_space_char(ch); }));
	s.erase(std::find_if(s.rbegin(), s.rend(), [](const int ch) { return !is_space_char(ch); }).base(), s.end());
}

inline std::string trimmed(__in const std::string& ss)
{
	auto s = ss;
	trim(s);
	return s;
}

inline std::string trim_lower(const std::string& source)
{
	auto p = source.c_str();

	std::string result;
	result.reserve(source.size());

	while (isspace(*p)) p++;
	while (*p) result += tolower(*p++);
	result.erase(std::find_if(result.rbegin(), result.rend(), [](const int ch) { return !is_space_char(ch); }).base(),
	             result.end());

	return result;
}


inline bool starts(const char* text, const char* with)
{
	while (*with != 0)
	{
		if (tolower(static_cast<unsigned char>(*text)) != tolower(static_cast<unsigned char>(*with)))
			return false;

		++text;
		++with;
	}

	return true;
}

inline bool starts(const std::string& text, const char* with)
{
	return starts(text.c_str(), with);
}

inline std::string make_url(const std::string& url, const std::string& basepath)
{
	const auto wurl = to_utf16(url);
	const auto wbase = to_utf16(basepath);
	std::wstring wresult;

	if (PathIsRelativeW(wurl.c_str()) && !PathIsURLW(wurl.c_str()))
	{
		WCHAR abs_url[512];
		DWORD dl = 512;
		UrlCombineW(wbase.c_str(), wurl.c_str(), abs_url, &dl, 0);
		wresult = abs_url;
	}
	else
	{
		if (PathIsURLW(wurl.c_str()))
		{
			wresult = wurl;
		}
		else
		{
			WCHAR abs_url[512];
			DWORD dl = 512;
			UrlCreateFromPathW(wurl.c_str(), abs_url, &dl, 0);
			wresult = abs_url;
		}
	}
	auto result = to_utf8(wresult);
	if (result.substr(0, 8) == "file:///")
	{
		result.erase(5, 1);
	}
	if (result.substr(0, 7) == "file://")
	{
		result.erase(0, 7);
	}
	return result;
}

inline void transform_text(std::string& text, const text_transform tt)
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
		for (auto& ch : text)
			ch = toupper(ch);
		break;
	case text_transform_lowercase:
		for (auto& ch : text)
			ch = tolower(ch);
		break;
	}
}

char normalize(char c);

inline bool needs_quoting(const char* s)
{
	return strpbrk(s, ": \t\'\"") != nullptr;
}

inline std::string quote_if_needed(__in const std::string& s)
{
	if (needs_quoting(s.c_str()))
	{
		if (s.find_first_of('\"') == std::string::npos)
		{
			return '"' + s + '"';
		}
		return '\'' + s + '\'';
	}

	return s;
}

inline int icmp(const char* l, const char* r)
{
	if (l == r) return 0;
	if (l == nullptr) return 1;
	if (r == nullptr) return -1;
	return _stricmp(l, r);
}

inline int icmp(const std::string& l, const char* r)
{
	return icmp(l.c_str(), r);
}

inline int icmp(const std::string& l, const std::string& r)
{
	return icmp(l.c_str(), r.c_str());
}

inline bool is_equal(const char* l, const char* r)
{
	if (l == r) return true;
	if (l == nullptr) return false;
	if (r == nullptr) return false;
	return _stricmp(l, r) == 0;
}

inline bool is_equal(const std::string& l, const char* r)
{
	return is_equal(l.c_str(), r);
}

inline bool is_equal(const std::string& l, const std::string& r)
{
	return is_equal(l.c_str(), r.c_str());
}

struct text_range
{
	size_t begin;
	size_t end;

	text_range() : begin(0), end(0)
	{
	}

	text_range(const char* sz) : begin(0), end(sz ? strlen(sz) : 0)
	{
	}

	text_range(const size_t b, const size_t e) : begin(b), end(e)
	{
	}

	void clear()
	{
		begin = end = 0;
	}

	bool empty() const
	{
		return begin == end;
	}

	bool operator==(const text_range& other) const
	{
		return begin == other.begin && end == other.end;
	}

	size_t size() const
	{
		return end - begin;
	}
};

inline text_range sub_string(const std::string& string, const std::string& query)
{
	auto ps = string.c_str();
	auto pq = query.c_str();
	auto start = ps;

	auto s = normalize(*ps++);
	const auto q = normalize(*pq++);

	while (s)
	{
		if (s == q) // Is matching?
		{
			auto match_ps = ps;
			auto match_pq = pq;
			auto match_s = normalize(*match_ps++);
			auto match_q = normalize(*match_pq++);

			while (match_s == match_q)
			{
				if (match_q == 0)
					return text_range(start - string.c_str(), match_ps - 1 - string.c_str());

				match_s = normalize(*match_ps++);
				match_q = normalize(*match_pq++);
			}

			if (match_q == 0)
				return text_range(start - string.c_str(), match_ps - 1 - string.c_str());
		}

		start = ps;
		s = normalize(*ps++);
	}

	return text_range();
}


class text_match
{
	std::string _text;
	std::string _prefix;

	text_range _selection;

public:
	text_match()
	{
	}

	explicit text_match(std::string s) : _text(std::move(s))
	{
	}

	text_match(const text_match& other) : _text(other._text), _prefix(other._prefix), _selection(other._selection)
	{
	}

	void operator=(const text_match& other)
	{
		_text = other._text;
		_prefix = other._prefix;
		_selection = other._selection;
	}

	void operator=(const std::string& s)
	{
		_prefix.clear();
		_text = s;
		_selection.clear();
	}

	void draw(HDC hdc, const recti& rr) const;

	void text(const std::string& s, const text_range selection)
	{
		assert(s.size() >= selection.begin && s.size() >= selection.end);
		assert(selection.size() <= s.size());

		_text = s;
		_selection = selection;
	};

	std::string text(const bool quoteIfNeeded = false) const
	{
		std::string result;

		if (!_prefix.empty())
		{
			result += _prefix;
			result += ":";
		}

		result += quoteIfNeeded ? quote_if_needed(_text) : _text;
		return result;
	};

	void prefix(const std::string& sz)
	{
		_prefix = sz;
	}

	bool operator<(const text_match& other) const
	{
		const auto pref = icmp(_prefix, other._prefix);
		if (pref != 0) return pref < 0;
		return icmp(_text, other._text) < 0;
	}
};

static std::string format(const char* fmt, ...)
{
	va_list argList;
	va_start(argList, fmt);

	const auto length = _vscprintf(fmt, argList);
	const auto sz = static_cast<char*>(_alloca(length + 1));
	if (sz == nullptr) return "";
	vsprintf_s(sz, length + 1, fmt, argList);
	va_end(argList);
	sz[length] = 0;
	return sz;
}

static const char* from(const bool val) { return val ? "true" : "false"; };

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

	bool operator()(const std::string& s1, const char* s2) const
	{
		return _stricmp(s1.c_str(), s2) < 0;
	}

	bool operator()(const char* s1, const std::string& s2) const
	{
		return _stricmp(s1, s2.c_str()) < 0;
	}
};

int value_index(const std::string& val, const char* strings, int defValue = -1, char delim = ';');
bool value_in_list(const std::string& val, const char* strings, char delim = ';');

std::string::size_type find_close_bracket(const std::string& s, std::string::size_type off, char open_b = '(',
                                           char close_b = ')');


std::vector<std::string> split_string(const std::string& str, char delim = ' ');
std::vector<std::string> split_string(const std::string& str, const char* delims, const char* quote = "\"");


struct web_color
{
	byte blue;
	byte green;
	byte red;
	byte alpha;

	web_color(const byte r, const byte g, const byte b, const byte a = 255)
	{
		blue = b;
		green = g;
		red = r;
		alpha = a;
	}

	web_color()
	{
		blue = 0;
		green = 0;
		red = 0;
		alpha = 0xFF;
	}

	web_color(const web_color& val)
	{
		blue = val.blue;
		green = val.green;
		red = val.red;
		alpha = val.alpha;
	}

	web_color& operator=(const web_color& val)
	{
		blue = val.blue;
		green = val.green;
		red = val.red;
		alpha = val.alpha;
		return *this;
	}

	static web_color from_string(const char* str);
	static web_color from_string(const std::string& str) { return from_string(str.c_str()); };

	static bool is_color(const char* str);
	static bool is_color(const std::string& str) { return is_color(str.c_str()); };
};


class document;

struct calc_term
{
	float value;
	css_units units;
};

class css_length
{
	union
	{
		float m_value = 0;
		int m_predef;
	};

	css_units m_units = css_units_none;
	bool m_is_predefined = false;
	bool m_is_calc = false;
	float m_calc_percent = 0;
	float m_calc_fixed = 0;

public:
	css_length() = default;
	css_length(const css_length&) = default;
	css_length& operator=(const css_length&) = default;

	bool is_predefined() const
	{
		return m_is_predefined && !m_is_calc;
	}

	void predef(const int val)
	{
		m_predef = val;
		m_is_predefined = true;
		m_is_calc = false;
	}

	int predef() const
	{
		if (m_is_predefined)
		{
			return m_predef;
		}
		return 0;
	}

	void set_value(const float val, const css_units units)
	{
		m_value = val;
		m_is_predefined = false;
		m_is_calc = false;
		m_units = units;
	}

	float val() const
	{
		if (!m_is_predefined)
		{
			return m_value;
		}
		return 0;
	}

	css_units units() const
	{
		return m_units;
	}

	int calc_percent(const int width) const
	{
		if (m_is_calc)
		{
			return static_cast<int>(m_calc_percent / 100.0 * width + m_calc_fixed);
		}
		if (!is_predefined())
		{
			if (units() == css_units_percentage)
			{
				return static_cast<int>(static_cast<double>(width) * static_cast<double>(m_value) / 100.0);
			}
			return static_cast<int>(val());
		}
		return 0;
	}

	static float resolve_term_to_px(const float value, const css_units units)
	{
		switch (units)
		{
		case css_units_em:
			return value * 16.0f;
		case css_units_rem:
			return value * 16.0f;
		case css_units_pt:
			return value * 96.0f / 72.0f;
		case css_units_in:
			return value * 96.0f;
		case css_units_cm:
			return value * 96.0f / 2.54f;
		case css_units_mm:
			return value * 96.0f / 25.4f;
		case css_units_vw:
			return value * GetSystemMetrics(SM_CXSCREEN) / 100.0f;
		case css_units_vh:
			return value * GetSystemMetrics(SM_CYSCREEN) / 100.0f;
		default:
			return value;
		}
	}

	void fromString(const std::string& str, const char* predefs = "", const int defValue = 0)
	{
		if (str.size() > 5 && str.substr(0, 4) == "calc")
		{
			parse_calc(str);
			return;
		}

		const int predef = value_index(str, predefs, -1);

		if (predef >= 0)
		{
			m_is_predefined = true;
			m_predef = predef;
		}
		else
		{
			m_is_predefined = false;

			std::string num;
			std::string un;
			bool is_unit = false;

			for (const auto chr : str)
			{
				if (!is_unit)
				{
					if (isdigit(chr) || chr == '.' || chr == '+' || chr == '-')
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
				char* end = nullptr;
				const auto val = strtof(num.c_str(), &end);

				if (end != num.c_str())
				{
					m_value = val;
					m_units = un.empty()
						          ? css_units_none
						          : static_cast<css_units>(value_index(un, css_units_strings, css_units_none));
				}
				else
				{
					m_is_predefined = true;
					m_predef = defValue;
				}
			}
			else
			{
				// not a number so it is predefined
				m_is_predefined = true;
				m_predef = defValue;
			}
		}
	}

	void parse_calc(const std::string& str)
	{
		m_is_calc = true;
		m_is_predefined = true;
		m_calc_percent = 0;
		m_calc_fixed = 0;

		// Find the expression inside calc(...)
		const auto open = str.find('(');

		if (open == std::string::npos)
			return;

		const auto close = find_close_bracket(str, open);

		if (close == std::string::npos)
			return;

		std::string expr = str.substr(open + 1, close - open - 1);
		trim(expr);

		if (expr.empty())
			return;

		float sign = 1.0f;
		float pending_val = 0.0f;
		css_units pending_units = css_units_none;
		bool has_pending = false;
		enum { op_none, op_multiply, op_divide } pending_op = op_none;

		auto flush_pending = [&]()
		{
			if (has_pending)
			{
				if (pending_units == css_units_percentage)
				{
					m_calc_percent += sign * pending_val;
				}
				else
				{
					m_calc_fixed += sign * resolve_term_to_px(pending_val, pending_units);
				}
				has_pending = false;
				pending_val = 0.0f;
				pending_units = css_units_none;
			}
		};

		std::string::size_type pos = 0;

		while (pos < expr.size())
		{
			// Skip whitespace
			while (pos < expr.size() && isspace(expr[pos]))
				pos++;

			if (pos >= expr.size())
				break;

			// Handle nested calc or parenthesized sub-expression
			if (expr[pos] == '(')
			{
				const auto sub_close = find_close_bracket(expr, pos);

				if (sub_close == std::string::npos)
					break;

				pos = sub_close + 1;
				continue;
			}

			if (pos > 0 && isspace(expr[pos - 1]))
			{
				if (expr[pos] == '*')
				{
					pending_op = op_multiply;
					pos++;
					continue;
				}
				if (expr[pos] == '/')
				{
					pending_op = op_divide;
					pos++;
					continue;
				}
				if (expr[pos] == '+' || expr[pos] == '-')
				{
					flush_pending();
					sign = (expr[pos] == '-') ? -1.0f : 1.0f;
					pos++;
					continue;
				}
			}

			// Parse a term: number followed by optional unit
			std::string num_str;
			std::string unit_str;

			// Handle leading sign
			if (expr[pos] == '+' || expr[pos] == '-')
			{
				if (expr[pos] == '-')
					sign = -sign;
				pos++;

				while (pos < expr.size() && isspace(expr[pos]))
					pos++;
			}

			// Parse digits
			while (pos < expr.size() && (isdigit(expr[pos]) || expr[pos] == '.'))
			{
				num_str += expr[pos];
				pos++;
			}

			if (num_str.empty())
			{
				pos++;
				continue;
			}

			// Parse unit
			while (pos < expr.size() && (isalpha(expr[pos]) || expr[pos] == '%'))
			{
				unit_str += expr[pos];
				pos++;
			}

			char* end_ptr = nullptr;
			const float term_val = strtof(num_str.c_str(), &end_ptr);

			if (end_ptr == num_str.c_str())
			{
				sign = 1.0f;
				pending_op = op_none;
				continue;
			}

			css_units term_units = css_units_none;

			if (!unit_str.empty())
			{
				if (unit_str == "%")
				{
					term_units = css_units_percentage;
				}
				else
				{
					term_units = static_cast<css_units>(value_index(unit_str, css_units_strings, css_units_none));
				}
			}

			if (pending_op == op_multiply)
			{
				// Multiply: one operand must be unitless
				if (has_pending && term_units == css_units_none)
				{
					pending_val *= term_val;
				}
				else if (has_pending && pending_units == css_units_none)
				{
					pending_val *= term_val;
					pending_units = term_units;
				}
				else
				{
					flush_pending();
					pending_val = term_val;
					pending_units = term_units;
					has_pending = true;
				}
				pending_op = op_none;
			}
			else if (pending_op == op_divide)
			{
				// Divide: right operand must be unitless
				if (has_pending && term_units == css_units_none && term_val != 0.0f)
				{
					pending_val /= term_val;
				}
				pending_op = op_none;
			}
			else
			{
				flush_pending();
				pending_val = term_val;
				pending_units = term_units;
				has_pending = true;
			}
		}

		flush_pending();
	}
};

struct css_margins
{
	css_length left;
	css_length right;
	css_length top;
	css_length bottom;
};

struct css_offsets
{
	css_length left;
	css_length top;
	css_length right;
	css_length bottom;
};

struct css_position
{
	css_length x;
	css_length y;
	css_length width;
	css_length height;
};


extern HINSTANCE g_hInstance;

inline std::string get_file_contents(const std::string& file_name)
{
	std::string result;
	std::ifstream in(file_name, std::ios::in | std::ios::binary);

	if (in)
	{
		in.seekg(0, std::ios::end);
		result.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(result.data(), result.size());
	}

	return result;
}

inline std::string load_resource_html(const int id)
{
	const auto hrsrc = ::FindResource(g_hInstance, MAKEINTRESOURCE(id), RT_HTML);

	if (hrsrc)
	{
		const auto hg = LoadResource(g_hInstance, hrsrc);

		if (hg)
		{
			const auto data = static_cast<LPCSTR>(LockResource(hg));
			const auto size = SizeofResource(g_hInstance, hrsrc);
			return std::string(data, size);
		}
	}

	return {};
}

class scope_locked_count
{
	std::atomic<long>& _i;

public:
	scope_locked_count(std::atomic<long>& i) : _i(i) { ++_i; }
	~scope_locked_count() { --_i; }
	scope_locked_count(const scope_locked_count&) = delete;
	scope_locked_count& operator=(const scope_locked_count&) = delete;
};

template <typename T1, typename T2, typename T3>
T1 Min(const T1& x, const T2& y, const T3& z)
{
	return std::min({x, static_cast<T1>(y), static_cast<T1>(z)});
}

template <typename T1, typename T2, typename T3>
T1 Max(const T1& x, const T2& y, const T3& z)
{
	return std::max({x, static_cast<T1>(y), static_cast<T1>(z)});
}

inline int round_f(const float val)
{
	return static_cast<int>(std::lround(val));
}

inline int round_d(const double val)
{
	return static_cast<int>(std::lround(val));
}


struct attr_color
{
	unsigned char rgbBlue;
	unsigned char rgbGreen;
	unsigned char rgbRed;
	unsigned char rgbAlpha;

	attr_color()
	{
		rgbAlpha = 255;
		rgbBlue = 0;
		rgbGreen = 0;
		rgbRed = 0;
	}
};

struct attr_border
{
	style_border border;
	int width;
	attr_color color;

	attr_border()
	{
		border = border_none;
		width = 0;
	}
};


// Rename utf16_instream since internal strings are now UTF-8.
// This class wraps a std::string (UTF-8) and returns codepoints for the scanner.


class utf8_instream
{
	const std::string& _s;
	std::string::const_iterator _i;

public:
	utf8_instream(const std::string& s) : _s(s), _i(_s.begin())
	{
	}

	wchar_t getb()
	{
		return _i == _s.end() ? 0 : static_cast<wchar_t>(static_cast<unsigned char>(*_i++));
	}

	wchar_t get_next_utf8(const wchar_t val)
	{
		return val & 0x3f;
	}

	wchar_t get_char()
	{
		const wchar_t b1 = getb();

		if (!b1)
		{
			return 0;
		}

		if ((b1 & 0x80) == 0)
		{
			return b1;
		}
		if ((b1 & 0xe0) == 0xc0)
		{
			wchar_t r = (b1 & 0x1f) << 6;
			r |= get_next_utf8(getb());
			return r;
		}
		if ((b1 & 0xf0) == 0xe0)
		{
			wchar_t r = (b1 & 0x0f) << 12;
			r |= get_next_utf8(getb()) << 6;
			r |= get_next_utf8(getb());
			return r;
		}
		if ((b1 & 0xf8) == 0xf0)
		{
			const int b2 = get_next_utf8(getb());
			const int b3 = get_next_utf8(getb());
			const int b4 = get_next_utf8(getb());
			return (b1 & 7) << 18 | (b2 & 0x3f) << 12 |
				(b3 & 0x3f) << 6 | b4 & 0x3f;
		}

		return '?';
	}

	static void wchar_to_chars(const wchar_t code, std::string& dst)
	{
		if (code < 0x80)
		{
			dst += static_cast<char>(code);
		}
		else if (code < 0x800)
		{
			dst += static_cast<char>(0xC0 | (code >> 6));
			dst += static_cast<char>(0x80 | (code & 0x3F));
		}
		else
		{
			dst += static_cast<char>(0xE0 | (code >> 12));
			dst += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
			dst += static_cast<char>(0x80 | (code & 0x3F));
		}
	}
};

using default_instream = utf8_instream;


class should
{
public:
	static void equal(const char* expected, const char* actual, const char* message = "Test")
	{
		if (!is_equal(actual, expected))
		{
			throw format("%s: expected '%s', got '%s'", message, expected, actual);
		}
	}

	static void equal(const std::string& expected, const std::string& actual, const char* message = "Test")
	{
		equal(expected.c_str(), actual.c_str(), message);
	}

	static void equal(const int expected, const int actual, const char* message = "Test")
	{
		static constexpr int size = 64;
		char expected_text[size], actual_text[size];
		_itoa_s(expected, expected_text, size, 10);
		_itoa_s(actual, actual_text, size, 10);
		equal(expected_text, actual_text, message);
	}

	static void equal(const bool expected, const bool actual, const char* message = "Test")
	{
		equal(from(expected), from(actual), message);
	}

	static void EqualTrue(const bool actual, const char* message = "Test")
	{
		equal(true, actual, message);
	}
};

class tests
{
	static std::chrono::high_resolution_clock::time_point now()
	{
		return std::chrono::high_resolution_clock::now();
	};

	static long long duration_in_microseconds(const std::chrono::high_resolution_clock::time_point& started)
	{
		const auto dur = now() - started;
		return std::chrono::duration_cast<std::chrono::microseconds>(dur).count();
	};

	std::map<std::string, std::function<void()>> _tests;

public:
	void register_test(const std::string& name, const std::function<void()>& f)
	{
		_tests[name] = f;
	}

	void run_tests(std::stringstream& output)
	{
		const auto started = now();
		auto count = 0;

		output << "<html>";
		output << "<style>";
		output << "body { background-color: LightSlateGray; }";
		output << "td.fail { background-color: OrangeRed; }";
		output << "</style>";
		output << "<body><table>";

		for (auto& test : _tests)
		{
			output << "<tr><td>" << test.first << "</td><td>";
			auto started = now();

			try
			{
				test.second();

				output << "<td>";
				output << " success in " << duration_in_microseconds(started) << " microseconds" << "<br>";
			}
			catch (const std::string& message)
			{
				output << "<td class='fail'>";
				output << " FAILED in " << duration_in_microseconds(started) << " microseconds" << "<br>";
				output << message;
			}
			catch (const std::exception& e)
			{
				output << "<td class='fail'>";
				output << " FAILED in " << duration_in_microseconds(started) << " microseconds" << "<br>";
				output << e.what();
			}

			output << "</td></tr>";

			count += 1;
		}

		output << "</table>";
		output << "<h1>" << "Completed " << count << " tests in " << duration_in_microseconds(started) <<
			" microseconds" << "</h1>";
		output << "</body></html>";
	}
};
