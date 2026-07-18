// style.h - CSS engine: border/background structs, the render_win32 GDI+
// renderer, the style property map, media query evaluation, CSS selectors with
// specificity, and the css stylesheet container.

#pragma once
#include "core.h"


struct css_border
{
	css_length width;
	border_style style = border_style_none;
	web_color color;

	css_border() = default;
	css_border(const css_border&) = default;
	css_border& operator=(const css_border&) = default;
};

struct css_border_radius
{
	css_length top_left_x;
	css_length top_left_y;
	css_length top_right_x;
	css_length top_right_y;
	css_length bottom_right_x;
	css_length bottom_right_y;
	css_length bottom_left_x;
	css_length bottom_left_y;

	css_border_radius() = default;
	css_border_radius(const css_border_radius&) = default;
	css_border_radius& operator=(const css_border_radius&) = default;
};

struct css_borders
{
	css_border left;
	css_border top;
	css_border right;
	css_border bottom;
	css_border_radius radius;

	css_borders() = default;
	css_borders(const css_borders&) = default;
	css_borders& operator=(const css_borders&) = default;
};


class background
{
public:
	std::string m_image;
	std::string m_baseurl;
	web_color m_color{0, 0, 0, 0};
	background_attachment m_attachment = background_attachment_scroll;
	css_position m_position;
	background_repeat m_repeat = background_repeat_repeat;
	background_box m_clip = background_box_border;
	background_box m_origin = background_box_padding;
	css_border_radius m_radius;

	background() = default;
	background(const background&) = default;
	~background() = default;
	background& operator=(const background&) = default;
};

class background_paint
{
public:
	std::shared_ptr<Gdiplus::Bitmap> image;
	background_attachment attachment = background_attachment_scroll;
	background_repeat repeat = background_repeat_repeat;
	web_color color{0, 0, 0, 0};
	position clip_box;
	position origin_box;
	position border_box;
	css_border_radius border_radius;
	size image_size;
	int position_x = 0;
	int position_y = 0;
	bool is_root = false;

	background_paint() = default;
	background_paint(const background_paint&) = default;
};


struct list_marker
{
	std::string image;
	std::string baseurl;
	list_style_type marker_type;
	web_color color;
	position pos;
};

class render_win32
{
protected:
	position::vector m_clips;
	HRGN m_hClipRgn;
	HDC _hdc;
	position _client_pos;

public:
	render_win32(const HDC hdc, const position& client_pos) : m_hClipRgn(nullptr), _hdc(hdc), _client_pos(client_pos)
	{
	}

	~render_win32()
	{
		if (m_hClipRgn)
		{
			DeleteObject(m_hClipRgn);
		}
	}

	void draw_image(const std::shared_ptr<Gdiplus::Bitmap>& bm, const position& pos);
	size get_image_size(const std::shared_ptr<Gdiplus::Bitmap>& bm);

	void apply_clip();
	void del_clip();

	void draw_background(render_win32& renderer, const background_paint& bg);
	void draw_borders(const css_borders& borders, const position& draw_pos, bool root);
	void draw_ellipse(int x, int y, int width, int height, const web_color& color, int line_width);
	void draw_list_marker(const list_marker& marker);
	void draw_text(const char* text, HFONT hFont, const web_color& color, const position& pos);
	void fill_ellipse(int x, int y, int width, int height, const web_color& color);
	void fill_rect(const position& pos, const web_color& color, const css_border_radius& radius);
	void fill_rect(int x, int y, int width, int height, const web_color& color, const css_border_radius& radius);
	void release_clip();
	void set_clip(const position& pos, bool valid_x, bool valid_y);
	int line_height(HFONT hFont);
};


class property_value
{
public:
	std::string m_value;
	bool m_important = false;

	property_value() = default;

	property_value(std::string val, const bool imp) : m_value(std::move(val)), m_important(imp)
	{
	}

	property_value(const property_value&) = default;
	property_value(property_value&&) = default;
	property_value& operator=(const property_value&) = default;
	property_value& operator=(property_value&&) = default;
};


class style
{
	std::map<std::string, property_value, ltstr> m_properties;

public:
	style() = default;
	style(const style&) = default;
	style(style&&) = default;
	style& operator=(const style&) = default;
	style& operator=(style&&) = default;

	void add(const std::string& txt, const std::string& baseurl)
	{
		parse(txt, baseurl);
	}

	void add_property(const std::string& name, const std::string& val, const std::string& baseurl, bool important);

	const std::string get_property(const std::string& name) const
	{
		const auto f = m_properties.find(name);

		if (f != m_properties.end())
		{
			return f->second.m_value;
		}

		return empty;
	}

	void combine(const style& src);

	void clear()
	{
		m_properties.clear();
	}

private:
	void parse_property(const std::string& txt, const std::string& baseurl);
	void parse_property(const std::string& name, const std::string& val, const std::string& baseurl);
	void parse(const std::string& txt, const std::string& baseurl);
	void parse_short_border(const std::string& key, const std::string& val, bool important);
	void parse_border_style(const char* style, const std::string& val, bool important);
	void parse_short_background(const std::string& val, const std::string& baseurl, bool important);
	void parse_short_font(const std::string& val, bool important);

	void add_parsed_property(const std::string& name, const std::string& val, bool important);
	void remove_property(const std::string& name, bool important);
};


struct media_query_expression
{
	media_feature feature = media_feature_none;
	int val = 0;
	int val2 = 0;
	bool check_as_bool = false;

	media_query_expression() = default;
	bool check(const media_features& features) const;
};

class media_query
{
	std::vector<media_query_expression> m_expressions;
	bool m_not = false;
	media_type m_media_type = media_type_all;

public:
	media_query() = default;
	media_query(const media_query&) = default;

	static std::shared_ptr<media_query> create_from_string(const std::string& str);
	bool check(const media_features& features) const;
};

class media_query_list
{
	std::vector<std::shared_ptr<media_query>> m_queries;
	bool m_is_used = false;

public:
	media_query_list() = default;
	media_query_list(const media_query_list&) = default;

	static std::shared_ptr<media_query_list> create_from_string(const std::string& str);
	bool is_used() const { return m_is_used; }

	bool apply_media_features(const media_features& features); // returns true if the m_is_used changed
};


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
	std::string attribute;
	std::string val;
	attr_select_condition condition = select_exists;
};

//////////////////////////////////////////////////////////////////////////

class css_element_selector
{
public:
	std::string m_tag;
	std::vector<css_attribute_selector> m_attrs;


	void parse(const std::string& txt);
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

// Bucket key used to index a css_selector by its rightmost compound selector.
// Computed once at parse time so apply_stylesheet only probes selectors whose
// rightmost could possibly match the current element.
struct selector_key
{
	enum kind_t { bucket_id, bucket_class, bucket_tag, bucket_universal };
	kind_t kind = bucket_universal;
	// For bucket_class, this is a space-separated list of ALL classes the selector
	// requires; the selector is registered under each. For bucket_id/bucket_tag,
	// it's the single id/tag name. Unused for bucket_universal.
	std::vector<std::string> values;
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
	int m_order = 0;
	std::shared_ptr<media_query_list> m_media_query;
	selector_key m_key;

	css_selector(const std::shared_ptr<style>& s,
	             const std::shared_ptr<media_query_list>& media) : m_style(s), m_media_query(media)
	{
	}

	~css_selector()
	{
	}

	bool parse(const std::string& text);
	void calc_specificity();

	bool is_media_valid() const
	{
		if (!m_media_query)
		{
			return true;
		}

		return m_media_query->is_used();
	}

	void add_media_to_doc(const std::shared_ptr<document>& doc) const;
};


//////////////////////////////////////////////////////////////////////////

inline bool operator >(const css_selector& v1, const css_selector& v2)
{
	if (v1.m_specificity == v2.m_specificity)
	{
		return v1.m_order > v2.m_order;
	}
	return v1.m_specificity > v2.m_specificity;
}

inline bool operator <(const css_selector& v1, const css_selector& v2)
{
	if (v1.m_specificity == v2.m_specificity)
	{
		return v1.m_order < v2.m_order;
	}
	return v1.m_specificity < v2.m_specificity;
}

inline bool operator >(const std::shared_ptr<css_selector>& v1, const std::shared_ptr<css_selector>& v2)
{
	return *v1 > *v2;
}

inline bool operator <(const std::shared_ptr<css_selector>& v1, const std::shared_ptr<css_selector>& v2)
{
	return *v1 < *v2;
}

//////////////////////////////////////////////////////////////////////////

class used_selector
{
public:
	std::shared_ptr<css_selector> m_selector;
	bool m_used = false;

	used_selector(const std::shared_ptr<css_selector>& s, const bool used) : m_selector(s), m_used(used)
	{
	}

	used_selector(const used_selector& other) : m_selector(other.m_selector), m_used(other.m_used)
	{
	}
};


class css
{
	std::vector<std::shared_ptr<css_selector>> m_selectors;

	// Bucketed index: element selection probes only the buckets matching the
	// element's tag / id / class names plus a universal fallback. Rebuilt from
	// m_selectors each sort_selectors() call.
	using selector_list = std::vector<std::shared_ptr<css_selector>>;
	std::unordered_map<std::string, selector_list> m_by_id;
	std::unordered_map<std::string, selector_list> m_by_class;
	std::unordered_map<std::string, selector_list> m_by_tag;
	selector_list m_universal;

public:
	css() = default;
	~css() = default;

	const std::vector<std::shared_ptr<css_selector>>& selectors() const
	{
		return m_selectors;
	}

	// Bucketed accessors used by element::apply_stylesheet. Returns nullptr when
	// the bucket is empty; callers are expected to handle the empty case cheaply.
	const selector_list* selectors_by_id(const std::string& id) const
	{
		const auto it = m_by_id.find(id);
		return it == m_by_id.end() ? nullptr : &it->second;
	}
	const selector_list* selectors_by_class(const std::string& cls) const
	{
		const auto it = m_by_class.find(cls);
		return it == m_by_class.end() ? nullptr : &it->second;
	}
	const selector_list* selectors_by_tag(const std::string& tag) const
	{
		const auto it = m_by_tag.find(tag);
		return it == m_by_tag.end() ? nullptr : &it->second;
	}
	const selector_list& universal_selectors() const { return m_universal; }

	void clear()
	{
		m_selectors.clear();
		m_by_id.clear();
		m_by_class.clear();
		m_by_tag.clear();
		m_universal.clear();
	}

	void parse_stylesheet(const std::string& str, const std::string& baseurl, document& doc,
	                      std::shared_ptr<media_query_list>& media);
	void sort_selectors();

	static std::string parse_css_url(const std::string& str);

private:
	void parse_atrule(const std::string& text, const std::string& baseurl, document& doc,
	                  std::shared_ptr<media_query_list>& media);
	void parse_selectors(const std::string& txt, const std::shared_ptr<style>& styles,
	                     std::shared_ptr<media_query_list>& media);

	void add_selector(const std::shared_ptr<css_selector>& selector)
	{
		selector->m_order = static_cast<int>(m_selectors.size());
		m_selectors.push_back(selector);
	}

	// Build m_by_id/m_by_class/m_by_tag/m_universal from m_selectors. Assumes
	// m_selectors is already sorted by (specificity, order).
	void rebuild_buckets();
};
