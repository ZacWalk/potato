// element.cpp - Element lifecycle, CSS property resolution, block/inline/table
// layout, float positioning, background/border painting, and selector matching.

#include "pch.h"
#include "element.h"
#include "document.h"


const css_props& css_props::defaults()
{
	static const css_props instance;
	return instance;
}

css_props& element::props_mut()
{
	if (!m_css) m_css = std::make_unique<css_props>();
	return *m_css;
}

table_grid& element::grid()
{
	if (!m_grid) m_grid = std::make_unique<table_grid>();
	return *m_grid;
}

element::element(document& doc, const enum element_type t, std::string text) : m_type(t), m_doc(doc),
                                                                               m_text(std::move(text))
{
	m_box = nullptr;
	m_parent = nullptr;
	m_skip = false;
	m_box_sizing = box_sizing_content_box;
	m_z_index = 0;
	m_overflow = overflow_visible;
	m_text_align = text_align_left;
	m_el_position = element_position_static;
	m_display = display_inline;
	m_vertical_align = va_baseline;
	m_list_style_type = list_style_type_none;
	m_list_style_position = list_style_position_outside;
	m_float = float_none;
	m_clear = clear_none;
	m_font = 0;
	m_font_size = 0;
	m_white_space = white_space_normal;
	m_lh_predefined = false;
	m_line_height = 0;
	m_visibility = visibility_visible;
	m_loaded = false;

	m_border_spacing_x = 0;
	m_border_spacing_y = 0;
	m_border_collapse = border_collapse_separate;

	m_flex_direction = flex_direction_row;
	m_flex_wrap = flex_wrap_nowrap;
	m_flex_justify_content = flex_justify_content_flex_start;
	m_flex_align_items = flex_align_items_stretch;
	m_flex_grow = 0.0f;
	m_flex_shrink = 1.0f;
	m_flex_align_self = flex_align_items_stretch;
	m_flex_gap = 0;

	m_text_transform = text_transform_none;
	m_use_transformed = false;
	m_draw_spaces = true;

	if (m_type == el_before)
	{
		set_tag_name("::before");
	}
	else if (m_type == el_after)
	{
		set_tag_name("::after");
	}
	else if (m_type == el_cdata || m_type == el_comment)
	{
		m_skip = true;
	}
	else if (m_type == el_image)
	{
		m_display = display_inline_block;
	}
}

element::~element() = default;

bool element::is_point_inside(const int x, const int y)
{
	if (get_display() != display_inline && get_display() != display_table_row)
	{
		position pos = m_pos;
		pos += m_padding;
		pos += m_borders;

		return pos.is_point_inside(x, y);
	}
	position::vector boxes;
	get_inline_boxes(boxes);

	for (auto box = boxes.begin(); box != boxes.end(); ++box)
	{
		if (box->is_point_inside(x, y))
		{
			return true;
		}
	}
	return false;
}

web_color element::get_color(const prop_id prop_name, const bool inherited, const web_color& def_color)
{
	const auto clrstr = get_style_property(prop_name, inherited);

	if (clrstr.empty())
	{
		return def_color;
	}

	return web_color::from_string(clrstr);
}

position element::get_placement() const
{
	position pos = m_pos;
	auto cur_el = parent();

	while (cur_el)
	{
		pos.x += cur_el->m_pos.x;
		pos.y += cur_el->m_pos.y;
		cur_el = cur_el->parent();
	}
	return pos;
}

bool element::is_inline_box() const
{
	const style_display d = get_display();

	return d == display_inline ||
		d == display_inline_block ||
		d == display_inline_flex ||
		d == display_inline_text;
}

bool element::collapse_top_margin() const
{
	if (!m_borders.top && !m_padding.top && in_normal_flow() && get_float() == float_none && m_margins.top >= 0 &&
		parent() && parent()->get_display() != display_flex && parent()->get_display() != display_inline_flex)
	{
		return true;
	}
	return false;
}

bool element::collapse_bottom_margin() const
{
	if (!m_borders.bottom && !m_padding.bottom && in_normal_flow() && get_float() == float_none && m_margins.bottom >= 0
		&& parent() && parent()->get_display() != display_flex && parent()->get_display() != display_inline_flex)
	{
		return true;
	}
	return false;
}

bool element::get_predefined_height(int& p_height) const
{
	css_length h = get_css_height();
	if (h.is_predefined())
	{
		p_height = m_pos.height;
		return false;
	}
	if (h.units() == css_units_percentage)
	{
		if (!m_parent)
		{
			const position client_pos = m_doc.client_pos();
			p_height = h.calc_percent(client_pos.height);
			return true;
		}
		int ph = 0;
		if (m_parent->get_predefined_height(ph))
		{
			p_height = h.calc_percent(ph);
			return true;
		}
		p_height = m_pos.height;
		return false;
	}
	p_height = m_doc.cvt_units(h, get_font_size());
	return true;
}

int element::calc_width(const int defVal) const
{
	css_length w = get_css_width();
	if (w.is_predefined())
	{
		return defVal;
	}
	if (w.units() == css_units_percentage)
	{
		if (!m_parent)
		{
			const position client_pos = m_doc.client_pos();
			return w.calc_percent(client_pos.width);
		}
		const int pw = m_parent->calc_width(defVal);
		return w.calc_percent(pw);
	}
	return m_doc.cvt_units(w, get_font_size());
}

bool element::is_ancestor(const element* el)
{
	auto el_parent = parent();

	while (el_parent)
	{
		if (el_parent == el) return true;
		el_parent = el_parent->parent();
	}
	return false;
}

int element::get_inline_shift_left()
{
	int ret = 0;

	if (m_parent->get_display() == display_inline)
	{
		const style_display disp = get_display();

		if (disp == display_inline_text || disp == display_inline_block)
		{
			auto parent = m_parent;
			auto el = this;

			while (parent && parent->get_display() == display_inline)
			{
				if (parent->is_first_child_inline(el))
				{
					ret += parent->padding_left() + parent->border_left() + parent->margin_left();
				}
				el = parent;
				parent = parent->m_parent;
			}
		}
	}

	return ret;
}

int element::get_inline_shift_right()
{
	int ret = 0;

	if (m_parent->get_display() == display_inline)
	{
		const auto disp = get_display();

		if (disp == display_inline_text || disp == display_inline_block)
		{
			auto parent = m_parent;
			auto el = this;

			while (parent && parent->get_display() == display_inline)
			{
				if (parent->is_last_child_inline(el))
				{
					ret += parent->padding_right() + parent->border_right() + parent->margin_right();
				}

				el = parent;
				parent = parent->m_parent;
			}
		}
	}

	return ret;
}

bool element::append_space(const std::string_view val)
{
	if (m_type == el_style || m_type == el_script || m_type == el_svg)
	{
		m_text += val;
	}
	else
	{
		append_child(std::make_unique<element>(m_doc, el_space, std::string(val)));
	}

	return true;
}

bool element::append_text(const std::string_view val)
{
	if (m_type == el_style || m_type == el_script || m_type == el_svg)
	{
		m_text += val;
	}
	else
	{
		append_child(std::make_unique<element>(m_doc, el_text, std::string(val)));
	}

	return true;
}

std::unique_ptr<element> element::append_child(std::unique_ptr<element> el)
{
	assert(el);

	if (!el) return {};

	// SVG elements skip all child elements
	if (m_type == el_svg)
	{
		return el;
	}

	if (m_type == el_table)
	{
		const auto& tag = el->get_tag_name();

		if (tag != "tbody" && tag != "thead" && tag != "tfoot" &&
			tag != "tr" && tag != "caption" && tag != "colgroup")
		{
			return el;
		}
	}

	el->parent(this);
	m_children.push_back(std::move(el));
	return {};
}

void element::set_attr(const std::string_view k, const std::string_view val)
{
	if (!k.empty())
	{
		m_attrs[std::string(k)] = val;

		// Selector bucketing reads these in apply_stylesheet, which runs before
		// parse_styles, so they must be live as soon as the parser sets them.
		if (is_equal(k, "id")) m_id = val;
		else if (is_equal(k, "class")) m_class = val;
	}
}

std::string_view element::get_attr(const std::string_view name, const std::string_view def) const
{
	const auto attr = m_attrs.find(name);

	if (attr != m_attrs.end())
	{
		return attr->second;
	}
	return def;
}

// True when every whitespace-delimited token of `needles` also appears in
// `haystack`. Allocation-free; both sides are scanned in place.
static bool contains_all_tokens(const std::string_view haystack, const std::string_view needles)
{
	const auto is_space = [](const char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f'; };

	const auto next_token = [&](std::string_view& s) -> std::string_view
	{
		while (!s.empty() && is_space(s.front())) s.remove_prefix(1);
		size_t n = 0;
		while (n < s.size() && !is_space(s[n])) ++n;
		const auto tok = s.substr(0, n);
		s.remove_prefix(n);
		return tok;
	};

	auto rest = needles;

	while (true)
	{
		const auto needle = next_token(rest);
		if (needle.empty()) break;

		auto scan = haystack;
		auto found = false;

		while (!found)
		{
			const auto tok = next_token(scan);
			if (tok.empty()) break;
			found = is_equal(tok, needle);
		}

		if (!found) return false;
	}

	return true;
}

void element::apply_stylesheet(const css& styles)
{
	if (m_type == el_before || m_type == el_after || m_type == el_text || m_type == el_space || m_type == el_style ||
		m_type
		== el_script || m_type == el_svg)
	{
		return;
	}

	if (m_type == el_anchor)
	{
		if (!get_attr("href").empty())
		{
			m_pseudo_classes.push_back("link");
		}
	}

	remove_before_after();
	m_used_styles.clear();

	// Build a small candidate list from the buckets that could possibly match this
	// element's rightmost compound, instead of scanning every selector in the sheet.
	// Typically 10-100x fewer candidates on real pages (e.g. Wikipedia).
	using selector_list = std::vector<std::shared_ptr<css_selector>>;
	const selector_list* probe_lists[32];
	size_t probe_count = 0;

	const auto add_list = [&](const selector_list* list)
	{
		if (list && !list->empty() && probe_count < std::size(probe_lists))
		{
			probe_lists[probe_count++] = list;
		}
	};

	const auto& universal = styles.universal_selectors();
	if (!universal.empty()) probe_lists[probe_count++] = &universal;
	if (!m_tag.empty()) add_list(styles.selectors_by_tag(m_tag)); // m_tag is already lowercased by the parser
	if (!m_id.empty())
	{
		// Selector side was lowercased via trim_lower during selector parse; the
		// id attribute is raw, so lowercase here to match the old case-insensitive behavior.
		std::string id_lower = m_id;
		for (auto& ch : id_lower) ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
		add_list(styles.selectors_by_id(id_lower));
	}
	if (!m_class.empty())
	{
		// Split the element's class attribute once rather than per candidate
		// selector as the old select_equal/class path did.
		auto classes = split_string(m_class);
		for (auto& c : classes)
		{
			trim(c);
			if (c.empty()) continue;
			for (auto& ch : c) ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
			add_list(styles.selectors_by_class(c));
		}
	}

	// A selector keyed by class list {a,b} is registered in both bucket a and
	// bucket b; dedup by shared_ptr identity while preserving (specificity, order).
	selector_list candidates;
	candidates.reserve(64);
	for (size_t i = 0; i < probe_count; ++i)
	{
		for (const auto& s : *probe_lists[i]) candidates.push_back(s);
	}
	if (probe_count > 1)
	{
		// Sort by pointer identity for dedup, then resort by selector order.
		std::sort(candidates.begin(), candidates.end(),
		          [](const auto& a, const auto& b) { return a.get() < b.get(); });
		candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
		std::sort(candidates.begin(), candidates.end(), std::less<std::shared_ptr<css_selector>>());
	}

	for (const auto& sel : candidates)
	{
		if (!sel->is_media_valid())
		{
			continue;
		}

		// The bucket narrows by rightmost key; the selector's tag may still disqualify
		// (e.g. selector "a.foo" is in bucket "foo" but only applies to <a>).
		const auto& sel_tag = sel->m_right.m_tag;
		if (!sel_tag.empty() && sel_tag != "*" && sel_tag != m_tag)
		{
			continue;
		}

		const int apply = select(*sel, false);

		if (apply != select_no_match)
		{
			used_selector us(sel, false);

			if (apply & select_match_pseudo_class)
			{
				if (select(*sel, true))
				{
					add_style(sel->m_style);
					us.m_used = true;
				}
			}
			else if (apply & select_match_with_after)
			{
				const auto el = get_element_after();

				if (el)
				{
					el->add_style(sel->m_style);
				}
			}
			else if (apply & select_match_with_before)
			{
				const auto el = get_element_before();

				if (el)
				{
					el->add_style(sel->m_style);
				}
			}
			else
			{
				add_style(sel->m_style);
				us.m_used = true;
			}

			m_used_styles.push_back(us);
		}
	}

	for (const auto& child : m_children)
	{
		child->apply_stylesheet(styles);
	}
}

void element::get_content_size(size& sz, const int max_width)
{
	if (m_type == el_text || m_type == el_space)
	{
		sz = m_size;
	}
	else if (m_type == el_image)
	{
		sz = image_size(m_doc.find_image(m_src));
	}
	else
	{
		sz.height = 0;

		if (m_display == display_block)
		{
			sz.width = max_width;
		}
		else
		{
			sz.width = 0;
		}
	}
}

void element::draw(render_win32& renderer, const int x, const int y, const position* clip)
{
	position pos = m_pos;
	pos.x += x;
	pos.y += y;

	if (m_type == el_text || m_type == el_space)
	{
		if (is_white_space() && !m_draw_spaces)
		{
			return;
		}

		if (pos.does_intersect(clip))
		{
			const auto font = m_parent->get_font();
			const auto color = m_parent->get_color(prop_id::color, true, m_doc.get_def_color());
			renderer.draw_text(m_use_transformed ? m_transformed_text.c_str() : m_text.c_str(), font, color, pos);
		}
	}
	else if (m_type == el_image)
	{
		draw_background(renderer, x, y, clip);

		if (pos.does_intersect(clip))
		{
			background_paint bg;
			bg.image = m_doc.find_image(m_src);
			bg.clip_box = pos;
			bg.origin_box = pos;
			bg.border_box = pos;
			bg.border_box += m_padding;
			bg.border_box += m_borders;
			bg.repeat = background_repeat_no_repeat;
			bg.image_size.width = pos.width;
			bg.image_size.height = pos.height;
			bg.border_radius = props().borders.radius;
			bg.position_x = pos.x;
			bg.position_y = pos.y;
			renderer.draw_background(renderer, bg);
		}
	}
	else
	{
		draw_background(renderer, x, y, clip);

		if (m_display == display_list_item && m_list_style_type != list_style_type_none)
		{
			if (m_overflow > overflow_visible)
			{
				renderer.set_clip(pos, true, true);
			}

			draw_list_marker(renderer, pos);

			if (m_overflow > overflow_visible)
			{
				renderer.del_clip();
			}
		}
	}
}

pf::font_handle element::get_font(font_metrics* fm)
{
	if (m_type == el_text || m_type == el_space)
	{
		return m_parent->get_font(fm);
	}
	if (fm)
	{
		*fm = m_font_metrics;
	}

	return m_font;
}

std::string element::get_style_property(const prop_id name, const bool inherited,
                                        const std::string_view def) const
{
	if (m_type == el_text || m_type == el_space)
	{
		if (inherited)
		{
			return m_parent->get_style_property(name, inherited, def);
		}
		return std::string(def);
	}
	auto found = m_style.get_property(name);
	auto pass_parent = false;
	auto explicit_inherit = false;

	if (m_parent)
	{
		if (!found.empty() && is_equal(found, "inherit"))
		{
			pass_parent = true;
			explicit_inherit = true;
		}
		else if (found.empty() && inherited)
		{
			pass_parent = true;
		}
	}

	if (pass_parent)
	{
		const auto parent_ret = m_parent->get_style_property(name, inherited || explicit_inherit, def);

		if (!parent_ret.empty())
		{
			return parent_ret;
		}
	}

	if (found.empty())
		found = def;

	// The overwhelming majority of values hold no var(), so stop here.
	if (found.find("var(") == std::string_view::npos)
		return std::string(found);

	std::string result(found);

	// Resolve var() references (handles nested var() via re-scanning)
	auto var_pos = result.find("var(");
	int var_depth = 0;
	constexpr int max_var_depth = 32;
	std::set<std::string> seen_vars;

	while (var_pos != std::string::npos && var_depth < max_var_depth)
	{
		var_depth++;
		const auto close = find_close_bracket(result, var_pos + 3);

		if (close == std::string::npos)
			break;

		auto var_content = result.substr(var_pos + 4, close - var_pos - 4);
		std::string var_name;
		std::string fallback;

		const auto comma = var_content.find_first_of(',');

		if (comma != std::string::npos)
		{
			var_name = var_content.substr(0, comma);
			fallback = var_content.substr(comma + 1);
			trim(var_name);
			trim(fallback);
		}
		else
		{
			var_name = var_content;
			trim(var_name);
		}

		std::string resolved;

		if (!var_name.empty() && var_name.size() > 2 && var_name[0] == '-' && var_name[1] == '-')
		{
			// Detect cycles: if we have already resolved this name, fall through
			// to the fallback to avoid infinite expansion (e.g. --a: var(--a)).
			if (!seen_vars.insert(var_name).second)
				resolved.clear();
			else
				resolved = resolve_custom_property(var_name);
		}

		if (resolved.empty())
			resolved = fallback;

		result = result.substr(0, var_pos) + resolved + result.substr(close + 1);
		var_pos = result.find("var(", var_pos);
	}

	return result;
}

void element::parse_styles(const bool is_reparse)
{
	if (m_type == el_text || m_type == el_space)
	{
		m_text_transform = static_cast<text_transform>(value_index(
			get_style_property(prop_id::text_transform, true, "none"),
			text_transform_strings, text_transform_none));

		if (m_text_transform != text_transform_none)
		{
			m_transformed_text = m_text;
			m_use_transformed = true;
			transform_text(m_transformed_text, m_text_transform);
		}

		if (is_white_space())
		{
			m_transformed_text = " ";
			m_use_transformed = true;
		}
		else if (m_text == "\t")
		{
			m_transformed_text = " ";
			m_use_transformed = true;
		}
		else if (m_text == "\n" || m_text == "\r")
		{
			m_transformed_text.clear();
			m_use_transformed = true;
		}

		font_metrics fm;
		const auto font = m_parent->get_font(&fm);

		if (is_break())
		{
			m_size.height = 0;
			m_size.width = 0;
		}
		else
		{
			m_size.height = fm.height;
			m_size.width = m_doc.text_width(m_use_transformed
				                                ? std::string_view(m_transformed_text)
				                                : std::string_view(m_text), font);
		}

		m_draw_spaces = fm.draw_spaces;

		return;
	}

	m_id = get_attr("id");
	m_class = get_attr("class");

	const std::string style(get_attr("style"));

	if (!style.empty())
	{
		m_style.add(style, "");
	}

	init_font();

	const auto position_str = get_style_property(prop_id::position, false, "static");
	m_el_position = position_str == "sticky"
		                ? element_position_relative
		                : static_cast<element_position>(value_index(position_str, element_position_strings,
		                                                            element_position_static));
	m_text_align = static_cast<text_align>(value_index(get_style_property(prop_id::text_align, true, "left"),
	                                                   text_align_strings,
	                                                   text_align_left));
	{
		auto overflow_str = get_style_property(prop_id::overflow, false, "visible");
		const auto sp = overflow_str.find(' ');
		if (sp != std::string::npos) overflow_str = overflow_str.substr(0, sp);
		m_overflow = static_cast<overflow>(value_index(overflow_str, overflow_strings, overflow_visible));
	}
	m_white_space = static_cast<white_space>(value_index(get_style_property(prop_id::white_space, true, "normal"),
	                                                     white_space_strings,
	                                                     white_space_normal));
	const auto display_str = get_style_property(prop_id::display, false, "inline");
	auto display_val = value_index(display_str, style_display_strings, -1);

	m_is_grid_container = false;

	if (display_val < 0)
	{
		if (display_str == "grid")
		{
			display_val = display_block;
			m_is_grid_container = true;
		}
		else if (display_str == "inline-grid")
		{
			display_val = display_inline_block;
			m_is_grid_container = true;
		}
		else if (display_str == "contents")
		{
			// No box of its own: the children join the parent's flow. Treating
			// it as a block keeps them in normal flow at full width, which is
			// far closer than the inline fallback.
			display_val = display_block;
		}
		else if (display_str == "initial")
		{
			display_val = display_inline;
		}
		else
			display_val = display_inline;
	}
	m_display = static_cast<style_display>(display_val);

	// Grid and flex items are blockified (CSS Display 3), so an inline value on
	// a child of one becomes block rather than collapsing to zero width.
	if (m_parent && (m_parent->m_is_grid_container ||
		m_parent->m_display == display_flex || m_parent->m_display == display_inline_flex))
	{
		if (m_display == display_inline) m_display = display_block;
		else if (m_display == display_inline_block) m_display = display_block;
	}

	m_visibility = static_cast<visibility>(value_index(get_style_property(prop_id::visibility, true, "visible"),
	                                                   visibility_strings,
	                                                   visibility_visible));
	m_box_sizing = static_cast<box_sizing>(value_index(get_style_property(prop_id::box_sizing, false, "content-box"),
	                                                   box_sizing_strings,
	                                                   box_sizing_content_box));

	// Parse flex container properties
	if (m_display == display_flex || m_display == display_inline_flex)
	{
		m_flex_direction = static_cast<flex_direction>(value_index(
			get_style_property(prop_id::flex_direction, false, "row"),
			flex_direction_strings, flex_direction_row));
		m_flex_wrap = static_cast<flex_wrap>(value_index(
			get_style_property(prop_id::flex_wrap, false, "nowrap"),
			flex_wrap_strings, flex_wrap_nowrap));
		m_flex_justify_content = static_cast<flex_justify_content>(value_index(
			get_style_property(prop_id::justify_content, false, "flex-start"),
			flex_justify_content_strings, flex_justify_content_flex_start));
		m_flex_align_items = static_cast<flex_align_items>(value_index(
			get_style_property(prop_id::align_items, false, "stretch"),
			flex_align_items_strings, flex_align_items_stretch));

		const auto gap_str = get_style_property(prop_id::gap, false, "0");
		css_length gap_len;
		gap_len.fromString(gap_str, "0");
		m_doc.cvt_units(gap_len, m_font_size);
		m_flex_gap = static_cast<int>(gap_len.val());
	}
	else
	{
		m_flex_direction = flex_direction_row;
		m_flex_wrap = flex_wrap_nowrap;
		m_flex_justify_content = flex_justify_content_flex_start;
		m_flex_align_items = flex_align_items_stretch;
		m_flex_gap = 0;
	}

	// Parse flex item properties
	{
		const auto fg = get_style_property(prop_id::flex_grow, false, "0");
		m_flex_grow = safe_stof(fg);
		const auto fs = get_style_property(prop_id::flex_shrink, false, "1");
		m_flex_shrink = safe_stof(fs);
		props_mut().flex_basis.fromString(get_style_property(prop_id::flex_basis, false, "auto"), "auto");
		m_doc.cvt_units(props_mut().flex_basis, m_font_size);
		m_flex_align_self = static_cast<flex_align_items>(value_index(
			get_style_property(prop_id::align_self, false, "auto"),
			flex_align_items_strings, -1));
		if (static_cast<int>(m_flex_align_self) < 0)
			m_flex_align_self = flex_align_items_stretch; // "auto" inherits from parent
	}

	if (m_el_position != element_position_static)
	{
		const auto val = get_style_property(prop_id::z_index, false);

		if (!val.empty())
		{
			m_z_index = safe_stoi(val);
		}
	}

	const auto va = get_style_property(prop_id::vertical_align, true, "baseline");
	m_vertical_align = static_cast<vertical_align>(value_index(va, vertical_align_strings, va_baseline));

	const auto fl = get_style_property(prop_id::float_, false, "none");
	m_float = static_cast<element_float>(value_index(fl, element_float_strings, float_none));

	m_clear = static_cast<element_clear>(value_index(get_style_property(prop_id::clear, false, "none"),
	                                                 element_clear_strings,
	                                                 clear_none));

	if (m_display != display_none && m_display != display_flex && m_display != display_inline_flex)
	{
		if (m_el_position == element_position_absolute || m_float != float_none)
		{
			m_display = display_block;
		}
		if (m_el_position == element_position_fixed)
		{
			m_display = display_block;
		}
	}

	props_mut().text_indent.fromString(get_style_property(prop_id::text_indent, true, "0"), "0");

	props_mut().width.fromString(get_style_property(prop_id::width, false, "auto"), "auto");
	props_mut().height.fromString(get_style_property(prop_id::height, false, "auto"), "auto");

	m_doc.cvt_units(props_mut().width, m_font_size);
	m_doc.cvt_units(props_mut().height, m_font_size);

	props_mut().min_width.fromString(get_style_property(prop_id::min_width, false, "0"));
	props_mut().min_height.fromString(get_style_property(prop_id::min_height, false, "0"));

	props_mut().max_width.fromString(get_style_property(prop_id::max_width, false, "none"), "none");
	props_mut().max_height.fromString(get_style_property(prop_id::max_height, false, "none"), "none");

	m_doc.cvt_units(props_mut().min_width, m_font_size);
	m_doc.cvt_units(props_mut().min_height, m_font_size);

	props_mut().offsets.left.fromString(get_style_property(prop_id::left, false, "auto"), "auto");
	props_mut().offsets.right.fromString(get_style_property(prop_id::right, false, "auto"), "auto");
	props_mut().offsets.top.fromString(get_style_property(prop_id::top, false, "auto"), "auto");
	props_mut().offsets.bottom.fromString(get_style_property(prop_id::bottom, false, "auto"), "auto");

	m_doc.cvt_units(props_mut().offsets.left, m_font_size);
	m_doc.cvt_units(props_mut().offsets.right, m_font_size);
	m_doc.cvt_units(props_mut().offsets.top, m_font_size);
	m_doc.cvt_units(props_mut().offsets.bottom, m_font_size);

	props_mut().margins.left.fromString(get_style_property(prop_id::margin_left, false, "0"), "auto");
	props_mut().margins.right.fromString(get_style_property(prop_id::margin_right, false, "0"), "auto");
	props_mut().margins.top.fromString(get_style_property(prop_id::margin_top, false, "0"), "auto");
	props_mut().margins.bottom.fromString(get_style_property(prop_id::margin_bottom, false, "0"), "auto");

	props_mut().padding.left.fromString(get_style_property(prop_id::padding_left, false, "0"));
	props_mut().padding.right.fromString(get_style_property(prop_id::padding_right, false, "0"));
	props_mut().padding.top.fromString(get_style_property(prop_id::padding_top, false, "0"));
	props_mut().padding.bottom.fromString(get_style_property(prop_id::padding_bottom, false, "0"));

	props_mut().borders.left.width.fromString(get_style_property(prop_id::border_left_width, false, "medium"),
	                                          border_width_strings);
	props_mut().borders.right.width.fromString(get_style_property(prop_id::border_right_width, false, "medium"),
	                                           border_width_strings);
	props_mut().borders.top.width.fromString(get_style_property(prop_id::border_top_width, false, "medium"),
	                                         border_width_strings);
	props_mut().borders.bottom.width.fromString(get_style_property(prop_id::border_bottom_width, false, "medium"),
	                                            border_width_strings);

	props_mut().borders.left.color = web_color::from_string(get_style_property(prop_id::border_left_color, false));
	props_mut().borders.left.style = static_cast<border_style>(value_index(
		get_style_property(prop_id::border_left_style, false, "none"),
		border_style_strings, border_style_none));

	props_mut().borders.right.color = web_color::from_string(get_style_property(prop_id::border_right_color, false));
	props_mut().borders.right.style = static_cast<border_style>(value_index(
		get_style_property(prop_id::border_right_style, false, "none"),
		border_style_strings, border_style_none));

	props_mut().borders.top.color = web_color::from_string(get_style_property(prop_id::border_top_color, false));
	props_mut().borders.top.style = static_cast<border_style>(value_index(
		get_style_property(prop_id::border_top_style, false, "none"),
		border_style_strings, border_style_none));

	props_mut().borders.bottom.color = web_color::from_string(get_style_property(prop_id::border_bottom_color, false));
	props_mut().borders.bottom.style = static_cast<border_style>(value_index(
		get_style_property(prop_id::border_bottom_style, false, "none"),
		border_style_strings, border_style_none));

	props_mut().borders.radius.top_left_x.fromString(get_style_property(prop_id::border_top_left_radius_x, false, "0"));
	props_mut().borders.radius.top_left_y.fromString(get_style_property(prop_id::border_top_left_radius_y, false, "0"));

	props_mut().borders.radius.top_right_x.fromString(
		get_style_property(prop_id::border_top_right_radius_x, false, "0"));
	props_mut().borders.radius.top_right_y.fromString(
		get_style_property(prop_id::border_top_right_radius_y, false, "0"));

	props_mut().borders.radius.bottom_right_x.fromString(
		get_style_property(prop_id::border_bottom_right_radius_x, false, "0"));
	props_mut().borders.radius.bottom_right_y.fromString(
		get_style_property(prop_id::border_bottom_right_radius_y, false, "0"));

	props_mut().borders.radius.bottom_left_x.
	            fromString(get_style_property(prop_id::border_bottom_left_radius_x, false, "0"));
	props_mut().borders.radius.bottom_left_y.
	            fromString(get_style_property(prop_id::border_bottom_left_radius_y, false, "0"));

	m_doc.cvt_units(props_mut().borders.radius.bottom_left_x, m_font_size);
	m_doc.cvt_units(props_mut().borders.radius.bottom_left_y, m_font_size);
	m_doc.cvt_units(props_mut().borders.radius.bottom_right_x, m_font_size);
	m_doc.cvt_units(props_mut().borders.radius.bottom_right_y, m_font_size);
	m_doc.cvt_units(props_mut().borders.radius.top_left_x, m_font_size);
	m_doc.cvt_units(props_mut().borders.radius.top_left_y, m_font_size);
	m_doc.cvt_units(props_mut().borders.radius.top_right_x, m_font_size);
	m_doc.cvt_units(props_mut().borders.radius.top_right_y, m_font_size);

	m_doc.cvt_units(props_mut().text_indent, m_font_size);

	m_margins.left = m_doc.cvt_units(props_mut().margins.left, m_font_size);
	m_margins.right = m_doc.cvt_units(props_mut().margins.right, m_font_size);
	m_margins.top = m_doc.cvt_units(props_mut().margins.top, m_font_size);
	m_margins.bottom = m_doc.cvt_units(props_mut().margins.bottom, m_font_size);

	m_padding.left = m_doc.cvt_units(props_mut().padding.left, m_font_size);
	m_padding.right = m_doc.cvt_units(props_mut().padding.right, m_font_size);
	m_padding.top = m_doc.cvt_units(props_mut().padding.top, m_font_size);
	m_padding.bottom = m_doc.cvt_units(props_mut().padding.bottom, m_font_size);

	m_borders.left = props().borders.left.style == border_style_none
		                 ? 0
		                 : m_doc.cvt_units(props_mut().borders.left.width, m_font_size);
	m_borders.right = props().borders.right.style == border_style_none
		                  ? 0
		                  : m_doc.cvt_units(props_mut().borders.right.width, m_font_size);
	m_borders.top = props().borders.top.style == border_style_none
		                ? 0
		                : m_doc.cvt_units(props_mut().borders.top.width, m_font_size);
	m_borders.bottom = props().borders.bottom.style == border_style_none
		                   ? 0
		                   : m_doc.cvt_units(props_mut().borders.bottom.width, m_font_size);

	css_length line_height;
	line_height.fromString(get_style_property(prop_id::line_height, true, "normal"), "normal");

	if (line_height.is_predefined())
	{
		m_line_height = m_font_metrics.height;
		m_lh_predefined = true;
	}
	else if (line_height.units() == css_units_none)
	{
		m_line_height = static_cast<int>(line_height.val() * m_font_size);
		m_lh_predefined = false;
	}
	else
	{
		m_line_height = m_doc.cvt_units(line_height, m_font_size, m_font_size);
		m_lh_predefined = false;
	}

	if (m_display == display_list_item)
	{
		const auto list_type = get_style_property(prop_id::list_style_type, true, "disc");
		m_list_style_type = static_cast<list_style_type>(value_index(list_type, list_style_type_strings,
		                                                             list_style_type_disc));

		const auto list_pos = get_style_property(prop_id::list_style_position, true, "outside");
		m_list_style_position = static_cast<list_style_position>(value_index(
			list_pos, list_style_position_strings, list_style_position_outside));

		const auto list_image = get_style_property(prop_id::list_style_image, true);

		if (!list_image.empty())
		{
			const auto url = css::parse_css_url(list_image);
			const auto list_image_baseurl = get_style_property(prop_id::list_style_image_baseurl, true);

			m_doc.load_image(url, list_image_baseurl);
		}
	}

	parse_background();

	if (!is_reparse)
	{
		for (const auto& child : m_children)
		{
			child->parse_styles();
		}

		init();
	}


	if (m_type == el_table)
	{
		m_border_collapse = static_cast<border_collapse>(value_index(
			get_style_property(prop_id::border_collapse, true, "separate"),
			border_collapse_strings, border_collapse_separate));

		if (m_border_collapse == border_collapse_separate)
		{
			props_mut().border_spacing_x.fromString(get_style_property(prop_id::potato_border_spacing_x, true, "0px"));
			props_mut().border_spacing_y.fromString(get_style_property(prop_id::potato_border_spacing_y, true, "0px"));

			const int fntsz = get_font_size();
			m_border_spacing_x = m_doc.cvt_units(props_mut().border_spacing_x, fntsz);
			m_border_spacing_y = m_doc.cvt_units(props_mut().border_spacing_y, fntsz);
		}
		else
		{
			m_border_spacing_x = 0;
			m_border_spacing_y = 0;
			m_padding.bottom = 0;
			m_padding.top = 0;
			m_padding.left = 0;
			m_padding.right = 0;
			props_mut().padding.bottom.set_value(0, css_units_px);
			props_mut().padding.top.set_value(0, css_units_px);
			props_mut().padding.left.set_value(0, css_units_px);
			props_mut().padding.right.set_value(0, css_units_px);
		}
	}
	else if (m_type == el_image)
	{
		if (!m_src.empty())
		{
			if (!m_loaded)
			{
				m_loaded = true;
				m_doc.load_image(m_src, empty);
			}
		}
	}
}

int element::render(int x, int y, int max_width, bool second_pass)
{
	if (m_type == el_text || m_type == el_space)
	{
		return 0;
	}
	if (m_type == el_table)
	{
		int parent_width = max_width;

		// reset auto margins
		if (props().margins.left.is_predefined())
		{
			m_margins.left = 0;
		}
		if (props().margins.right.is_predefined())
		{
			m_margins.right = 0;
		}

		m_pos.clear();
		m_pos.move_to(x, y);

		m_pos.x += content_margins_left();
		m_pos.y += content_margins_top();

		def_value<int> block_width(0);

		if (!props().width.is_predefined())
		{
			max_width = block_width = calc_width(parent_width - (content_margins_left() + content_margins_right()));
		}

		// Apply max-width to table
		if (!props().max_width.is_predefined())
		{
			int mw = m_doc.cvt_units(props_mut().max_width, m_font_size, parent_width);
			mw -= content_margins_left() + content_margins_right();
			if (max_width > mw) max_width = mw;
			if (!block_width.is_default() && block_width > mw) block_width = mw;
		}
		else
		{
			if (max_width)
			{
				max_width -= content_margins_left() + content_margins_right();
			}
		}

		calc_outlines(parent_width);

		// Calculate table spacing
		int table_width_spacing = 0;
		if (m_border_collapse == border_collapse_separate)
		{
			table_width_spacing = m_border_spacing_x * (grid().cols_count() + 1);
		}
		else
		{
			table_width_spacing = 0;

			if (grid().cols_count())
			{
				table_width_spacing -= std::min(border_left(), grid().column(0).border_left);
				table_width_spacing -= std::min(border_right(), grid().column(grid().cols_count() - 1).border_right);
			}

			for (int col = 1; col < grid().cols_count(); col++)
			{
				table_width_spacing -= std::min(grid().column(col).border_left, grid().column(col - 1).border_right);
			}
		}


		// Calculate the minimum content width (MCW) of each cell: the formatted content may span any number of lines but may not overflow the cell box. 
		// If the specified 'width' (W) of the cell is greater than MCW, W is the minimum cell width. A value of 'auto' means that MCW is the minimum 
		// cell width.
		// 
		// Also, calculate the "maximum" cell width of each cell: formatting the content without breaking lines other than where explicit line breaks occur.

		if (grid().cols_count() == 1 && !block_width.is_default())
		{
			for (int row = 0; row < grid().rows_count(); row++)
			{
				table_cell* cell = grid().cell(0, row);
				if (cell && cell->el)
				{
					cell->min_width = cell->max_width = cell->el->render(
						0, 0, max_width - table_width_spacing);
					cell->el->m_pos.width = cell->min_width - cell->el->content_margins_left() - cell->el->
						content_margins_right();
				}
			}
		}
		else
		{
			for (int row = 0; row < grid().rows_count(); row++)
			{
				for (int col = 0; col < grid().cols_count(); col++)
				{
					table_cell* cell = grid().cell(col, row);
					if (cell && cell->el)
					{
						if (!grid().column(col).css_width.is_predefined() && grid().column(col).css_width.units() !=
							css_units_percentage)
						{
							int css_w = grid().column(col).css_width.calc_percent(block_width);
							int el_w = cell->el->render(0, 0, css_w);
							cell->min_width = cell->max_width = std::max(css_w, el_w);
							cell->el->m_pos.width = cell->min_width - cell->el->content_margins_left() - cell->el->
								content_margins_right();
						}
						else
						{
							// calculate minimum content width
							cell->min_width = cell->el->render(0, 0, 1);
							// calculate maximum content width
							cell->max_width = cell->el->render(0, 0, max_width - table_width_spacing);
						}
					}
				}
			}
		}

		// For each column, determine a maximum and minimum column width from the cells that span only that column. 
		// The minimum is that required by the cell with the largest minimum cell width (or the column 'width', whichever is larger). 
		// The maximum is that required by the cell with the largest maximum cell width (or the column 'width', whichever is larger).

		for (int col = 0; col < grid().cols_count(); col++)
		{
			grid().column(col).max_width = 0;
			grid().column(col).min_width = 0;
			for (int row = 0; row < grid().rows_count(); row++)
			{
				if (grid().cell(col, row)->colspan <= 1)
				{
					grid().column(col).max_width = std::max(grid().column(col).max_width,
					                                        grid().cell(col, row)->max_width);
					grid().column(col).min_width = std::max(grid().column(col).min_width,
					                                        grid().cell(col, row)->min_width);
				}
			}
		}

		// For each cell that spans more than one column, increase the minimum widths of the columns it spans so that together, 
		// they are at least as wide as the cell. Do the same for the maximum widths. 
		// If possible, widen all spanned columns by approximately the same amount.

		for (int col = 0; col < grid().cols_count(); col++)
		{
			for (int row = 0; row < grid().rows_count(); row++)
			{
				if (grid().cell(col, row)->colspan > 1)
				{
					int max_total_width = grid().column(col).max_width;
					int min_total_width = grid().column(col).min_width;
					for (int col2 = col + 1; col2 < col + grid().cell(col, row)->colspan; col2++)
					{
						max_total_width += grid().column(col2).max_width;
						min_total_width += grid().column(col2).min_width;
					}
					if (min_total_width < grid().cell(col, row)->min_width)
					{
						grid().distribute_min_width(grid().cell(col, row)->min_width - min_total_width, col,
						                            col + grid().cell(col, row)->colspan - 1);
					}
					if (max_total_width < grid().cell(col, row)->max_width)
					{
						grid().distribute_max_width(grid().cell(col, row)->max_width - max_total_width, col,
						                            col + grid().cell(col, row)->colspan - 1);
					}
				}
			}
		}

		// If the 'table' or 'inline-table' element's 'width' property has a computed value (W) other than 'auto', the used width is the 
		// greater of W, CAPMIN, and the minimum width required by all the columns plus cell spacing or borders (MIN). 
		// If the used width is greater than MIN, the extra width should be distributed over the columns.
		//
		// If the 'table' or 'inline-table' element has 'width: auto', the used width is the greater of the table's containing block width, 
		// CAPMIN, and MIN. However, if either CAPMIN or the maximum width required by the columns plus cell spacing or borders (MAX) is 
		// less than that of the containing block, use max(MAX, CAPMIN).


		int table_width = 0;

		if (!block_width.is_default())
		{
			table_width = grid().calc_table_width(block_width - table_width_spacing, false);
		}
		else
		{
			table_width = grid().calc_table_width(max_width - table_width_spacing, true);
		}

		table_width += table_width_spacing;
		grid().calc_horizontal_positions(m_borders, m_border_collapse, m_border_spacing_x);

		bool row_span_found = false;

		// render cells with computed width
		for (int row = 0; row < grid().rows_count(); row++)
		{
			grid().row(row).height = 0;
			for (int col = 0; col < grid().cols_count(); col++)
			{
				table_cell* cell = grid().cell(col, row);
				if (cell->el)
				{
					int span_col = col + cell->colspan - 1;
					if (span_col >= grid().cols_count())
					{
						span_col = grid().cols_count() - 1;
					}
					int cell_width = grid().column(span_col).right - grid().column(col).left;

					if (cell->el->m_pos.width != cell_width - cell->el->content_margins_left() - cell->el->
						content_margins_right())
					{
						cell->el->render(grid().column(col).left, 0, cell_width);
						cell->el->m_pos.width = cell_width - cell->el->content_margins_left() - cell->el->
							content_margins_right();
					}
					else
					{
						cell->el->m_pos.x = grid().column(col).left + cell->el->content_margins_left();
					}

					if (cell->rowspan <= 1)
					{
						grid().row(row).height = std::max(grid().row(row).height, cell->el->height());
					}
					else
					{
						row_span_found = true;
					}
				}
			}
		}

		if (row_span_found)
		{
			for (int col = 0; col < grid().cols_count(); col++)
			{
				for (int row = 0; row < grid().rows_count(); row++)
				{
					table_cell* cell = grid().cell(col, row);
					if (cell->el)
					{
						int span_row = row + cell->rowspan - 1;
						if (span_row >= grid().rows_count())
						{
							span_row = grid().rows_count() - 1;
						}
						if (span_row != row)
						{
							int h = 0;
							for (int i = row; i <= span_row; i++)
							{
								h += grid().row(i).height;
							}
							if (h < cell->el->height())
							{
								grid().row(span_row).height += cell->el->height() - h;
							}
						}
					}
				}
			}
		}

		grid().calc_vertical_positions(m_borders, m_border_collapse, m_border_spacing_y);

		int table_height = 0;
		// place cells vertically
		for (int col = 0; col < grid().cols_count(); col++)
		{
			for (int row = 0; row < grid().rows_count(); row++)
			{
				table_cell* cell = grid().cell(col, row);
				if (cell->el)
				{
					int span_row = row + cell->rowspan - 1;
					if (span_row >= grid().rows_count())
					{
						span_row = grid().rows_count() - 1;
					}
					cell->el->m_pos.y = grid().row(row).top + cell->el->content_margins_top();
					cell->el->m_pos.height = grid().row(span_row).bottom - grid().row(row).top - cell->el->
						content_margins_top() - cell->el->content_margins_bottom();
					table_height = std::max(table_height, grid().row(span_row).bottom);
					cell->el->apply_vertical_align();
				}
			}
		}

		if (m_border_collapse == border_collapse_collapse)
		{
			if (grid().rows_count())
			{
				table_height -= std::min(border_bottom(), grid().row(grid().rows_count() - 1).border_bottom);
			}
		}
		else
		{
			table_height += m_border_spacing_y;
		}

		m_pos.width = table_width;

		calc_outlines(parent_width);

		m_pos.move_to(x, y);
		m_pos.x += content_margins_left();
		m_pos.y += content_margins_top();
		m_pos.width = table_width;
		m_pos.height = table_height;

		return table_width;
	}
	if (m_type == el_image)
	{
		int parent_width = max_width;

		// restore margins after collapse
		m_margins.top = m_doc.cvt_units(props_mut().margins.top, m_font_size);
		m_margins.bottom = m_doc.cvt_units(props_mut().margins.bottom, m_font_size);

		m_pos.move_to(x, y);

		auto sz = image_size(m_doc.find_image(m_src));

		m_pos.width = sz.width;
		m_pos.height = sz.height;

		if (props().height.is_predefined() && props().width.is_predefined())
		{
			m_pos.height = sz.height;
			m_pos.width = sz.width;

			// check for max-height
			if (!props().max_width.is_predefined())
			{
				int max_width = m_doc.cvt_units(props_mut().max_width, m_font_size, parent_width);
				if (m_pos.width > max_width)
				{
					m_pos.width = max_width;
				}
				if (sz.width)
				{
					m_pos.height = static_cast<int>(static_cast<float>(m_pos.width) * static_cast<float>(sz.height) /
						static_cast<float>(sz.width));
				}
				else
				{
					m_pos.height = sz.height;
				}
			}

			// check for max-height
			if (!props().max_height.is_predefined())
			{
				int max_height = m_doc.cvt_units(props_mut().max_height, m_font_size);
				if (m_pos.height > max_height)
				{
					m_pos.height = max_height;
				}
				if (sz.height)
				{
					m_pos.width = static_cast<int>(m_pos.height * static_cast<float>(sz.width) / static_cast<float>(sz.
						height));
				}
				else
				{
					m_pos.width = sz.width;
				}
			}
		}
		else if (!props().height.is_predefined() && props().width.is_predefined())
		{
			m_pos.height = static_cast<int>(props().height.val());

			// check for max-height
			if (!props().max_height.is_predefined())
			{
				int max_height = m_doc.cvt_units(props_mut().max_height, m_font_size);
				if (m_pos.height > max_height)
				{
					m_pos.height = max_height;
				}
			}

			if (sz.height)
			{
				m_pos.width = static_cast<int>(m_pos.height * static_cast<float>(sz.width) / static_cast<float>(sz.
					height));
			}
			else
			{
				m_pos.width = sz.width;
			}
		}
		else if (props().height.is_predefined() && !props().width.is_predefined())
		{
			m_pos.width = props().width.calc_percent(parent_width);

			// check for max-width
			if (!props().max_width.is_predefined())
			{
				int max_width = m_doc.cvt_units(props_mut().max_width, m_font_size, parent_width);
				if (m_pos.width > max_width)
				{
					m_pos.width = max_width;
				}
			}

			if (sz.width)
			{
				m_pos.height = static_cast<int>(static_cast<float>(m_pos.width) * static_cast<float>(sz.height) /
					static_cast<float>(sz.width));
			}
			else
			{
				m_pos.height = sz.height;
			}
		}
		else
		{
			m_pos.width = props().width.calc_percent(parent_width);
			m_pos.height = static_cast<int>(props().height.val());

			// check for max-height
			if (!props().max_height.is_predefined())
			{
				int max_height = m_doc.cvt_units(props_mut().max_height, m_font_size);
				if (m_pos.height > max_height)
				{
					m_pos.height = max_height;
				}
			}

			// check for max-height
			if (!props().max_width.is_predefined())
			{
				int max_width = m_doc.cvt_units(props_mut().max_width, m_font_size, parent_width);
				if (m_pos.width > max_width)
				{
					m_pos.width = max_width;
				}
			}
		}

		calc_outlines(parent_width);

		m_pos.x += content_margins_left();
		m_pos.y += content_margins_top();

		return m_pos.width + content_margins_left() + content_margins_right();
	}
	if (m_type == el_svg)
	{
		int parent_width = max_width;

		m_margins.top = m_doc.cvt_units(props_mut().margins.top, m_font_size);
		m_margins.bottom = m_doc.cvt_units(props_mut().margins.bottom, m_font_size);

		m_pos.move_to(x, y);

		// Use CSS width/height (set from attributes in parse_attributes)
		m_pos.width = props().width.is_predefined() ? 24 : static_cast<int>(props().width.val());
		m_pos.height = props().height.is_predefined() ? 24 : static_cast<int>(props().height.val());

		if (!props().max_width.is_predefined())
		{
			int mw = m_doc.cvt_units(props_mut().max_width, m_font_size, parent_width);
			if (m_pos.width > mw) m_pos.width = mw;
		}
		if (!props().max_height.is_predefined())
		{
			int mh = m_doc.cvt_units(props_mut().max_height, m_font_size);
			if (m_pos.height > mh) m_pos.height = mh;
		}

		calc_outlines(parent_width);

		m_pos.x += content_margins_left();
		m_pos.y += content_margins_top();

		return m_pos.width + content_margins_left() + content_margins_right();
	}
	if (m_display == display_flex || m_display == display_inline_flex)
	{
		int parent_width = max_width;

		m_margins.top = m_doc.cvt_units(props_mut().margins.top, m_font_size, max_width);
		m_margins.bottom = m_doc.cvt_units(props_mut().margins.bottom, m_font_size, max_width);

		if (props().margins.left.is_predefined()) m_margins.left = 0;
		if (props().margins.right.is_predefined()) m_margins.right = 0;

		m_pos.clear();
		m_pos.move_to(x, y);
		m_pos.x += content_margins_left();
		m_pos.y += content_margins_top();

		def_value<int> block_width(0);

		if (!props().width.is_predefined())
		{
			int w = calc_width(parent_width);
			if (m_box_sizing == box_sizing_border_box)
				w -= m_padding.left + m_borders.left + m_padding.right + m_borders.right;
			max_width = block_width = w;
		}
		else
		{
			if (max_width)
				max_width -= content_margins_left() + content_margins_right();
		}

		if (!props().max_width.is_predefined())
		{
			int mw = m_doc.cvt_units(props_mut().max_width, m_font_size, parent_width);
			if (m_box_sizing == box_sizing_border_box)
				mw -= m_padding.left + m_borders.left + m_padding.right + m_borders.right;
			if (max_width > mw) max_width = mw;
		}

		calc_outlines(parent_width);

		const bool is_row = (m_flex_direction == flex_direction_row || m_flex_direction == flex_direction_row_reverse);
		const bool is_reverse = (m_flex_direction == flex_direction_row_reverse || m_flex_direction ==
			flex_direction_column_reverse);

		// Collect flex items (visible children that are in normal flow)
		struct flex_item
		{
			element* el;
			int base_size;
			int min_main;
			int cross_size;
			float grow;
			float shrink;
			int final_main;
			int final_cross;
		};

		std::vector<flex_item> items;

		std::function<int(const element*)> max_content_width = [&](const element* root)
		{
			if (root->is_text_node() || root->is_replaced()) return root->width();

			const bool row_flex = (root->m_display == display_flex || root->m_display == display_inline_flex) &&
				(root->m_flex_direction == flex_direction_row || root->m_flex_direction == flex_direction_row_reverse);
			int content_width = 0;

			if (row_flex)
			{
				int visible_count = 0;
				for (const auto& child : root->m_children)
				{
					if (child->is_white_space()) continue;
					if (!child->is_text_node() && !child->is_visible()) continue;
					content_width += max_content_width(child.get());
					++visible_count;
				}
				content_width += root->m_flex_gap * std::max(0, visible_count - 1);
			}
			else
			{
				int inline_width = 0;
				for (const auto& child : root->m_children)
				{
					if (!child->is_text_node() && !child->is_visible()) continue;
					const int child_width = max_content_width(child.get());
					if (child->is_text_node() || child->is_inline_box())
					{
						inline_width += child_width;
					}
					else
					{
						content_width = std::max(content_width, inline_width);
						content_width = std::max(content_width, child_width);
						inline_width = 0;
					}
				}
				content_width = std::max(content_width, inline_width);
			}

			return content_width + root->content_margins_left() + root->content_margins_right();
		};

		std::function<int(const element*)> min_content_width = [&](const element* root)
		{
			if (root->is_text_node() || root->is_replaced()) return root->width();
			int content_width = 0;
			for (const auto& child : root->m_children)
			{
				if (child->is_white_space()) continue;
				if (!child->is_text_node() && !child->is_visible()) continue;
				content_width = std::max(content_width, min_content_width(child.get()));
			}
			return content_width + root->content_margins_left() + root->content_margins_right();
		};

		for (const auto& child : m_children)
		{
			if (child->is_white_space()) continue;
			if (child->get_display() == display_none) continue;
			auto child_pos = child->get_element_position();
			if (child_pos == element_position_absolute || child_pos == element_position_fixed)
			{
				child->render(0, 0, max_width);
				continue;
			}

			flex_item fi;
			fi.el = child.get();
			fi.grow = child->m_flex_grow;
			fi.shrink = child->m_flex_shrink;
			fi.min_main = 0;

			// Determine base size
			if (!child->props().flex_basis.is_predefined() && child->props().flex_basis.val() > 0)
			{
				fi.base_size = child->props().flex_basis.units() == css_units_percentage && is_row
					               ? child->props().flex_basis.calc_percent(max_width)
					               : static_cast<int>(child->props().flex_basis.val());
			}
			else if (!child->props().width.is_predefined() && is_row)
			{
				fi.base_size = child->calc_width(max_width);
			}
			else if (!child->props().height.is_predefined() && !is_row)
			{
				fi.base_size = static_cast<int>(child->props().height.val());
			}
			else
			{
				// Render to measure intrinsic size
				int rendered = child->render(0, 0, is_row ? 0 : max_width);
				fi.base_size = is_row
					               ? std::max(rendered, max_content_width(child.get()))
					               : child->height() + child->content_margins_top() + child->content_margins_bottom();
			}

			fi.final_main = fi.base_size;
			if (is_row)
			{
				const auto min_width = child->get_style_property(prop_id::min_width, false);
				if (min_width.empty() || min_width == "auto")
					fi.min_main = min_content_width(child.get());
			}
			fi.cross_size = 0;
			fi.final_cross = 0;
			items.push_back(fi);
		}

		// Wrap items into lines
		struct flex_line
		{
			int start;
			int end;
			int total_main;
			int max_cross;
		};

		std::vector<flex_line> lines;
		int container_main = is_row ? max_width : 0;

		if (m_flex_wrap == flex_wrap_nowrap || container_main <= 0)
		{
			flex_line line;
			line.start = 0;
			line.end = static_cast<int>(items.size());
			line.total_main = 0;
			line.max_cross = 0;
			for (auto& item : items)
				line.total_main += item.base_size;
			line.total_main += m_flex_gap * (std::max(0, static_cast<int>(items.size()) - 1));
			lines.push_back(line);
		}
		else
		{
			int current_main = 0;
			int line_start = 0;
			for (int i = 0; i < static_cast<int>(items.size()); i++)
			{
				int gap = (i > line_start) ? m_flex_gap : 0;
				if (current_main + gap + items[i].base_size > container_main && i > line_start)
				{
					flex_line line;
					line.start = line_start;
					line.end = i;
					line.total_main = current_main;
					line.max_cross = 0;
					lines.push_back(line);
					line_start = i;
					current_main = items[i].base_size;
				}
				else
				{
					current_main += gap + items[i].base_size;
				}
			}
			flex_line line;
			line.start = line_start;
			line.end = static_cast<int>(items.size());
			line.total_main = current_main;
			line.max_cross = 0;
			lines.push_back(line);
		}

		// Flex grow/shrink
		for (auto& line : lines)
		{
			int item_count = line.end - line.start;
			int total_gaps = m_flex_gap * std::max(0, item_count - 1);
			int free_space = container_main - line.total_main;

			if (free_space > 0)
			{
				float total_grow = 0;
				for (int i = line.start; i < line.end; i++)
					total_grow += items[i].grow;

				if (total_grow > 0)
				{
					for (int i = line.start; i < line.end; i++)
					{
						items[i].final_main = items[i].base_size +
							static_cast<int>(static_cast<float>(free_space) * items[i].grow / total_grow);
					}
				}
			}
			else if (free_space < 0)
			{
				int deficit = -free_space;
				while (deficit > 0)
				{
					float total_shrink = 0;
					for (int i = line.start; i < line.end; i++)
					{
						if (items[i].final_main > items[i].min_main)
							total_shrink += items[i].shrink * static_cast<float>(items[i].base_size);
					}
					if (total_shrink <= 0) break;

					int removed = 0;
					for (int i = line.start; i < line.end; i++)
					{
						if (items[i].final_main <= items[i].min_main) continue;
						const float scaled_shrink = items[i].shrink * static_cast<float>(items[i].base_size);
						const int share = std::max(1, static_cast<int>(static_cast<float>(deficit) * scaled_shrink /
							total_shrink));
						const int shrink_amount = std::min(share, items[i].final_main - items[i].min_main);
						items[i].final_main -= shrink_amount;
						removed += shrink_amount;
					}
					if (removed == 0) break;
					deficit -= std::min(deficit, removed);
				}
			}
		}

		// Render items with final sizes and compute cross sizes
		for (auto& item : items)
		{
			if (is_row)
			{
				int item_content_width = item.final_main - item.el->content_margins_left() - item.el->
					content_margins_right();
				if (item_content_width < 0) item_content_width = 0;
				item.el->render(0, 0, item.final_main);
				item.el->m_pos.width = item_content_width;
				item.cross_size = item.el->height();
			}
			else
			{
				item.el->render(0, 0, max_width);
				item.cross_size = item.el->width();
			}
		}

		// Compute line cross sizes
		for (auto& line : lines)
		{
			line.max_cross = 0;
			for (int i = line.start; i < line.end; i++)
			{
				if (items[i].cross_size > line.max_cross)
					line.max_cross = items[i].cross_size;
			}
		}

		// Position items
		int cross_offset = 0;

		for (auto& line : lines)
		{
			int item_count = line.end - line.start;
			int total_gaps = m_flex_gap * std::max(0, item_count - 1);

			// Recalculate used main space after grow/shrink
			int used_main = total_gaps;
			for (int i = line.start; i < line.end; i++)
				used_main += items[i].final_main;

			int remaining = container_main - used_main;
			if (remaining < 0) remaining = 0;

			int main_offset = 0;
			int gap_extra = 0;

			switch (m_flex_justify_content)
			{
			case flex_justify_content_flex_start:
				main_offset = 0;
				break;
			case flex_justify_content_flex_end:
				main_offset = remaining;
				break;
			case flex_justify_content_center:
				main_offset = remaining / 2;
				break;
			case flex_justify_content_space_between:
				if (item_count > 1)
					gap_extra = remaining / (item_count - 1);
				break;
			case flex_justify_content_space_around:
				if (item_count > 0)
				{
					int space = remaining / item_count;
					main_offset = space / 2;
					gap_extra = space;
				}
				break;
			case flex_justify_content_space_evenly:
				if (item_count > 0)
				{
					int space = remaining / (item_count + 1);
					main_offset = space;
					gap_extra = space;
				}
				break;
			}

			for (int idx = line.start; idx < line.end; idx++)
			{
				int i = is_reverse ? (line.end - 1 - (idx - line.start)) : idx;
				auto& item = items[i];

				// Determine cross position based on align-items
				int item_cross = cross_offset;
				flex_align_items align = m_flex_align_items;
				if (static_cast<int>(item.el->m_flex_align_self) >= 0 &&
					item.el->m_flex_align_self != flex_align_items_stretch)
				{
					align = item.el->m_flex_align_self;
				}

				int cross_extra = line.max_cross - item.cross_size;
				switch (align)
				{
				case flex_align_items_flex_start:
					break;
				case flex_align_items_flex_end:
					item_cross += cross_extra;
					break;
				case flex_align_items_center:
					item_cross += cross_extra / 2;
					break;
				case flex_align_items_baseline:
					// Approximate: treat as flex-start
					break;
				case flex_align_items_stretch:
					if (is_row && item.el->props().height.is_predefined())
					{
						int stretch_h = line.max_cross - item.el->content_margins_top() - item.el->
							content_margins_bottom();
						if (stretch_h > item.el->m_pos.height)
							item.el->m_pos.height = stretch_h;
					}
					break;
				}

				if (is_row)
				{
					item.el->m_pos.x = main_offset + item.el->content_margins_left();
					item.el->m_pos.y = item_cross + item.el->content_margins_top();
				}
				else
				{
					item.el->m_pos.x = item_cross + item.el->content_margins_left();
					item.el->m_pos.y = main_offset + item.el->content_margins_top();
				}

				main_offset += item.final_main + m_flex_gap + gap_extra;
			}

			cross_offset += line.max_cross + m_flex_gap;
		}

		// Set container size
		if (is_row)
		{
			int used_width = 0;
			for (const auto& line : lines)
				used_width = std::max(used_width, line.total_main);
			m_pos.width = max_width > 0 ? max_width : used_width;

			int total_height = 0;
			for (const auto& line : lines)
				total_height += line.max_cross;
			total_height += m_flex_gap * std::max(0, static_cast<int>(lines.size()) - 1);
			m_pos.height = total_height;
		}
		else
		{
			m_pos.width = max_width;

			int total_main = 0;
			for (const auto& line : lines)
			{
				int line_main = 0;
				for (int i = line.start; i < line.end; i++)
					line_main += items[i].final_main + m_flex_gap;
				if (!items.empty()) line_main -= m_flex_gap;
				if (line_main > total_main) total_main = line_main;
			}
			m_pos.height = total_main;
		}

		// Apply min/max height
		if (!props().min_height.is_predefined())
		{
			int min_h = m_doc.cvt_units(props_mut().min_height, m_font_size);
			if (m_pos.height < min_h) m_pos.height = min_h;
		}
		if (!props().max_height.is_predefined())
		{
			int max_h = m_doc.cvt_units(props_mut().max_height, m_font_size);
			if (m_pos.height > max_h) m_pos.height = max_h;
		}
		if (!props().height.is_predefined())
		{
			m_pos.height = m_doc.cvt_units(props_mut().height, m_font_size);
		}

		calc_outlines(parent_width);

		// Handle auto margins for centering
		if (props().margins.left.is_predefined() && props().margins.right.is_predefined())
		{
			int extra = parent_width - m_pos.width - content_margins_left() - content_margins_right();
			if (extra > 0)
			{
				m_margins.left = extra / 2;
				m_margins.right = extra - m_margins.left;
				m_pos.x = x + content_margins_left();
			}
		}

		return m_pos.width + content_margins_left() + content_margins_right();
	}
	int parent_width = max_width;

	// restore margins after collapse
	m_margins.top = m_doc.cvt_units(props_mut().margins.top, m_font_size, max_width);
	m_margins.bottom = m_doc.cvt_units(props_mut().margins.bottom, m_font_size, max_width);

	// reset auto margins
	if (props().margins.left.is_predefined())
	{
		m_margins.left = 0;
	}
	if (props().margins.right.is_predefined())
	{
		m_margins.right = 0;
	}

	m_pos.clear();
	m_pos.move_to(x, y);

	m_pos.x += content_margins_left();
	m_pos.y += content_margins_top();

	int ret_width = 0;

	def_value<int> block_width(0);

	if (m_display != display_table_cell && !props().width.is_predefined())
	{
		int w = calc_width(parent_width);
		if (m_box_sizing == box_sizing_border_box)
		{
			w -= m_padding.left + m_borders.left + m_padding.right + m_borders.right;
		}
		ret_width = max_width = block_width = w;
	}
	else
	{
		if (max_width)
		{
			max_width -= content_margins_left() + content_margins_right();
		}
	}

	// check for max-width
	if (!props().max_width.is_predefined())
	{
		int mw = m_doc.cvt_units(props_mut().max_width, m_font_size, parent_width);
		if (m_box_sizing == box_sizing_border_box)
		{
			mw -= m_padding.left + m_borders.left + m_padding.right + m_borders.right;
		}
		if (max_width > mw)
		{
			max_width = mw;
		}
	}

	m_floats_left.clear();
	m_floats_right.clear();
	m_boxes.clear();
	m_cache_line_left.invalidate();
	m_cache_line_right.invalidate();

	calc_outlines(parent_width);


	for (const auto& el : m_children)
	{
		auto el_position = el->get_element_position();
		if ((el_position == element_position_absolute || el_position == element_position_fixed) && second_pass)
			continue
				;

		int rw = place_element(el.get(), max_width);
		if (rw > ret_width)
		{
			ret_width = rw;
		}
	}

	m_pos.height = 0;

	finish_last_box(true);

	if (block_width.is_default() && is_inline_box())
	{
		m_pos.width = ret_width;
	}
	else
	{
		m_pos.width = max_width;
	}
	calc_outlines(parent_width);

	if (!m_boxes.empty())
	{
		if (collapse_top_margin())
		{
			int old_top = m_margins.top;
			m_margins.top = std::max(m_boxes.front()->top_margin(), m_margins.top);
			if (m_margins.top != old_top)
			{
				update_floats(m_margins.top - old_top, this);
			}
		}
		if (collapse_bottom_margin())
		{
			m_margins.bottom = std::max(m_boxes.back()->bottom_margin(), m_margins.bottom);
			m_pos.height = m_boxes.back()->bottom() - m_boxes.back()->bottom_margin();
		}
		else
		{
			m_pos.height = m_boxes.back()->bottom();
		}
	}

	if ((m_tag == "input" || m_tag == "textarea" || m_tag == "select" || m_tag == "button") &&
		props().height.is_predefined())
	{
		m_pos.height = std::max(m_pos.height, m_line_height);
	}

	// add the floats height to the block height
	if (is_floats_holder())
	{
		int floats_height = get_floats_height();
		if (floats_height > m_pos.height)
		{
			m_pos.height = floats_height;
		}
	}

	// calculate the final position

	m_pos.move_to(x, y);
	m_pos.x += content_margins_left();
	m_pos.y += content_margins_top();

	int block_height = 0;
	if (get_predefined_height(block_height))
	{
		m_pos.height = block_height;
	}

	int min_height = 0;
	if (!props().min_height.is_predefined() && props().min_height.units() == css_units_percentage)
	{
		if (m_parent)
		{
			if (m_parent->get_predefined_height(block_height))
			{
				min_height = props().min_height.calc_percent(block_height);
			}
		}
	}
	else
	{
		min_height = static_cast<int>(props().min_height.val());
	}
	if (min_height != 0 && m_box_sizing == box_sizing_border_box)
	{
		min_height -= m_padding.top + m_borders.top + m_padding.bottom + m_borders.bottom;
		if (min_height < 0) min_height = 0;
	}

	if (m_display == display_list_item)
	{
		auto list_image = get_style_property(prop_id::list_style_image, true);

		if (!list_image.empty())
		{
			auto url = css::parse_css_url(list_image);
			auto list_image_baseurl = get_style_property(prop_id::list_style_image_baseurl, true);
			auto sz = image_size(m_doc.find_image(url, list_image_baseurl));

			if (min_height < sz.height)
			{
				min_height = sz.height;
			}
		}
	}

	if (min_height > m_pos.height)
	{
		m_pos.height = min_height;
	}

	// Apply max-height to block elements
	if (!props().max_height.is_predefined())
	{
		int max_height = m_doc.cvt_units(props_mut().max_height, m_font_size);
		if (props().max_height.units() == css_units_percentage && m_parent)
		{
			if (m_parent->get_predefined_height(block_height))
			{
				max_height = props().max_height.calc_percent(block_height);
			}
		}
		if (max_height != 0 && m_box_sizing == box_sizing_border_box)
		{
			max_height -= m_padding.top + m_borders.top + m_padding.bottom + m_borders.bottom;
			if (max_height < 0) max_height = 0;
		}
		if (m_pos.height > max_height)
		{
			m_pos.height = max_height;
		}
	}

	int min_width = props().min_width.calc_percent(parent_width);

	if (min_width != 0 && m_box_sizing == box_sizing_border_box)
	{
		min_width -= m_padding.left + m_borders.left + m_padding.right + m_borders.right;
		if (min_width < 0) min_width = 0;
	}

	if (min_width != 0)
	{
		if (min_width > m_pos.width)
		{
			m_pos.width = min_width;
		}
		if (min_width > ret_width)
		{
			ret_width = min_width;
		}
	}

	ret_width += content_margins_left() + content_margins_right();

	// re-render with new width
	if (ret_width < max_width && !second_pass && m_parent)
	{
		if (m_display == display_inline_block ||
			props().width.is_predefined() &&
			(m_float != float_none ||
				m_display == display_table ||
				m_el_position == element_position_absolute ||
				m_el_position == element_position_fixed
			)
		)
		{
			render(x, y, ret_width, true);
			m_pos.width = ret_width - (content_margins_left() + content_margins_right());
		}
	}

	return ret_width;
}

bool element::is_white_space()
{
	if (m_type == el_space)
	{
		const auto ws = get_white_space();

		return ws == white_space_normal ||
			ws == white_space_nowrap ||
			ws == white_space_pre_line;
	}

	return false;
}

int element::get_font_size() const
{
	return m_font_size;
}

int element::get_base_line() const
{
	if (m_type == el_text || m_type == el_space)
	{
		return m_parent->get_base_line();
	}

	if (is_replaced())
	{
		return 0;
	}

	int bl = 0;
	if (!m_boxes.empty())
	{
		bl = m_boxes.back()->baseline() + content_margins_bottom();
	}

	return bl;
}

void element::init()
{
	if (m_type == el_table)
	{
		grid().clear();

		go_inside_table table_selector;
		table_rows_selector row_selector;
		table_cells_selector cell_selector;

		elements_iterator<go_inside_table, table_rows_selector> row_iter(this, table_selector, row_selector);

		auto row = row_iter.next(false);
		while (row)
		{
			grid().begin_row(row);

			elements_iterator<go_inside_table, table_cells_selector> cell_iter(row, table_selector, cell_selector);
			auto cell = cell_iter.next();

			while (cell)
			{
				grid().add_cell(cell);

				cell = cell_iter.next(false);
			}
			row = row_iter.next(false);
		}

		grid().finish();
	}
	else
	{
		//remove duplicate white spaces
		auto i = m_children.begin();

		while (i != m_children.end())
		{
			if ((*i)->is_white_space())
			{
				++i;

				while (i != m_children.end() && (*i)->is_white_space())
				{
					i = m_children.erase(i);
				}
			}
			else
			{
				++i;
			}
		}
	}
}

int element::select(const css_selector& selector, const bool apply_pseudo)
{
	if (m_type == el_text || m_type == el_space)
	{
		return select_no_match;
	}

	int right_res = select(selector.m_right, apply_pseudo);

	if (right_res == select_no_match)
	{
		return select_no_match;
	}

	if (selector.m_left)
	{
		if (!m_parent)
		{
			return select_no_match;
		}

		switch (selector.m_combinator)
		{
		case combinator_descendant:
			{
				bool is_pseudo = false;
				const auto res = find_ancestor(*selector.m_left, apply_pseudo, &is_pseudo);

				if (!res)
				{
					return select_no_match;
				}
				if (is_pseudo)
				{
					right_res |= select_match_pseudo_class;
				}
			}
			break;

		case combinator_child:
			{
				const int res = m_parent->select(*selector.m_left, apply_pseudo);
				if (res == select_no_match)
				{
					return select_no_match;
				}
				if (right_res != select_match_pseudo_class)
				{
					right_res |= res;
				}
			}
			break;

		case combinator_adjacent_sibling:
			{
				bool is_pseudo = false;
				const auto res = m_parent->find_adjacent_sibling(this, *selector.m_left, apply_pseudo, &is_pseudo);

				if (!res)
				{
					return select_no_match;
				}
				if (is_pseudo)
				{
					right_res |= select_match_pseudo_class;
				}
			}
			break;

		case combinator_general_sibling:
			{
				bool is_pseudo = false;
				const auto res = m_parent->find_sibling(this, *selector.m_left, apply_pseudo, &is_pseudo);
				if (!res)
				{
					return select_no_match;
				}
				if (is_pseudo)
				{
					right_res |= select_match_pseudo_class;
				}
			}
			break;

		default:
			right_res = select_no_match;
		}
	}

	return right_res;
}

int element::select(const css_element_selector& selector, const bool apply_pseudo)
{
	if (m_type == el_text || m_type == el_space)
	{
		return select_no_match;
	}

	if (!selector.m_tag.empty() && selector.m_tag != "*")
	{
		if (m_tag != selector.m_tag)
		{
			return select_no_match;
		}
	}

	int res = select_match;

	for (const auto& sa : selector.m_attrs)
	{
		// Pseudo cases never read an attribute; keep the map probe out of their way.
		const auto attr_value = (sa.condition == select_pseudo_element || sa.condition == select_pseudo_class)
			                        ? std::string_view()
			                        : get_attr(sa.attribute);

		switch (sa.condition)
		{
		case select_exists:
			if (!m_attrs.contains(sa.attribute))
			{
				return select_no_match;
			}
			break;
		case select_equal:
			{
				if (attr_value.empty())
				{
					return select_no_match;
				}
				if (sa.attribute == "class")
				{
					// Every whitespace-delimited token of the selector must appear
					// in the element's class list. Walked in place -- this runs for
					// every candidate selector on every element, so the old
					// split_string pair was the single hottest allocation in the cascade.
					if (!contains_all_tokens(attr_value, sa.val))
					{
						return select_no_match;
					}
				}
				else
				{
					if (!is_equal(sa.val, attr_value))
					{
						return select_no_match;
					}
				}
			}
			break;
		case select_contain_str:
			{
				if (attr_value.empty())
				{
					return select_no_match;
				}
				if (attr_value.find(sa.val) == std::string_view::npos)
				{
					return select_no_match;
				}
			}
			break;
		case select_start_str:
			{
				if (attr_value.empty())
				{
					return select_no_match;
				}
				if (attr_value.length() < sa.val.length() ||
					_strnicmp(attr_value.data(), sa.val.c_str(), sa.val.length()))
				{
					return select_no_match;
				}
			}
			break;
		case select_end_str:
			{
				if (attr_value.empty())
				{
					return select_no_match;
				}
				if (attr_value.length() < sa.val.length())
				{
					return select_no_match;
				}
				const auto offset = attr_value.length() - sa.val.length();
				if (_strnicmp(attr_value.data() + offset, sa.val.c_str(), sa.val.length()) != 0)
				{
					return select_no_match;
				}
			}
			break;
		case select_pseudo_element:
			if (sa.val == "after")
			{
				res |= select_match_with_after;
			}
			else if (sa.val == "before")
			{
				res |= select_match_with_before;
			}
			else
			{
				return select_no_match;
			}
			break;
		case select_pseudo_class:
			if (apply_pseudo)
			{
				if (!m_parent) return select_no_match;

				std::string selector_param;
				std::string selector_name;

				const auto begin = sa.val.find_first_of('(');
				const auto end = begin == std::string::npos ? std::string::npos : find_close_bracket(sa.val, begin);

				if (begin != std::string::npos && end != std::string::npos)
				{
					selector_param = sa.val.substr(begin + 1, end - begin - 1);
				}

				if (begin != std::string::npos)
				{
					selector_name = sa.val.substr(0, begin);
					trim(selector_name);
				}
				else
				{
					selector_name = sa.val;
				}

				const int selector = value_index(selector_name, pseudo_class_strings);

				switch (selector)
				{
				case pseudo_class_only_child:
					if (!m_parent->is_only_child(this, false))
					{
						return select_no_match;
					}
					break;
				case pseudo_class_only_of_type:
					if (!m_parent->is_only_child(this, true))
					{
						return select_no_match;
					}
					break;
				case pseudo_class_first_child:
					if (!m_parent->is_nth_child(this, 0, 1, false))
					{
						return select_no_match;
					}
					break;
				case pseudo_class_first_of_type:
					if (!m_parent->is_nth_child(this, 0, 1, true))
					{
						return select_no_match;
					}
					break;
				case pseudo_class_last_child:
					if (!m_parent->is_nth_last_child(this, 0, 1, false))
					{
						return select_no_match;
					}
					break;
				case pseudo_class_last_of_type:
					if (!m_parent->is_nth_last_child(this, 0, 1, true))
					{
						return select_no_match;
					}
					break;
				case pseudo_class_nth_child:
				case pseudo_class_nth_of_type:
				case pseudo_class_nth_last_child:
				case pseudo_class_nth_last_of_type:
					{
						if (selector_param.empty()) return select_no_match;

						int num = 0;
						int off = 0;

						parse_nth_child_params(selector_param, num, off);
						if (!num && !off) return select_no_match;
						switch (selector)
						{
						case pseudo_class_nth_child:
							if (!m_parent->is_nth_child(this, num, off, false))
							{
								return select_no_match;
							}
							break;
						case pseudo_class_nth_of_type:
							if (!m_parent->is_nth_child(this, num, off, true))
							{
								return select_no_match;
							}
							break;
						case pseudo_class_nth_last_child:
							if (!m_parent->is_nth_last_child(this, num, off, false))
							{
								return select_no_match;
							}
							break;
						case pseudo_class_nth_last_of_type:
							if (!m_parent->is_nth_last_child(this, num, off, true))
							{
								return select_no_match;
							}
							break;
						}
					}
					break;
				case pseudo_class_not:
					{
						css_element_selector sel;
						sel.parse(selector_param);
						if (select(sel, apply_pseudo))
						{
							return select_no_match;
						}
					}
					break;
				case pseudo_class_root:
					if (m_parent != nullptr && m_parent->m_parent != nullptr)
					{
						return select_no_match;
					}
					break;
				default:
					if (std::find(m_pseudo_classes.begin(), m_pseudo_classes.end(), sa.val) == m_pseudo_classes.end())
					{
						return select_no_match;
					}
					break;
				}
			}
			else
			{
				res |= select_match_pseudo_class;
			}
			break;
		}
	}
	return res;
}

element* element::find_ancestor(const css_selector& selector, const bool apply_pseudo, bool* is_pseudo)
{
	if (!m_parent)
	{
		return nullptr;
	}
	const int res = m_parent->select(selector, apply_pseudo);
	if (res != select_no_match)
	{
		if (is_pseudo)
		{
			*is_pseudo = (res & select_match_pseudo_class) != 0;
		}
		return m_parent;
	}
	return m_parent->find_ancestor(selector, apply_pseudo, is_pseudo);
}

int element::get_floats_height(const element_float el_float) const
{
	if (is_floats_holder())
	{
		int h = 0;

		bool process = false;

		for (auto i = m_floats_left.begin(); i != m_floats_left.end(); ++i)
		{
			process = false;
			switch (el_float)
			{
			case float_none:
				process = true;
				break;
			case float_left:
				if (i->clear_floats == clear_left || i->clear_floats == clear_both)
				{
					process = true;
				}
				break;
			case float_right:
				if (i->clear_floats == clear_right || i->clear_floats == clear_both)
				{
					process = true;
				}
				break;
			}
			if (process)
			{
				if (el_float == float_none)
				{
					h = std::max(h, i->pos.bottom());
				}
				else
				{
					h = std::max(h, i->pos.top());
				}
			}
		}


		for (auto i = m_floats_right.begin(); i != m_floats_right.end(); ++i)
		{
			process = false;
			switch (el_float)
			{
			case float_none:
				process = true;
				break;
			case float_left:
				if (i->clear_floats == clear_left || i->clear_floats == clear_both)
				{
					process = true;
				}
				break;
			case float_right:
				if (i->clear_floats == clear_right || i->clear_floats == clear_both)
				{
					process = true;
				}
				break;
			}
			if (process)
			{
				if (el_float == float_none)
				{
					h = std::max(h, i->pos.bottom());
				}
				else
				{
					h = std::max(h, i->pos.top());
				}
			}
		}

		return h;
	}
	const int h = m_parent->get_floats_height(el_float);
	return h - m_pos.y;
}

int element::get_left_floats_height() const
{
	if (is_floats_holder())
	{
		int h = 0;
		if (!m_floats_left.empty())
		{
			for (auto el = m_floats_left.begin(); el != m_floats_left.end(); ++el)
			{
				h = std::max(h, el->pos.bottom());
			}
		}
		return h;
	}
	const int h = m_parent->get_left_floats_height();
	return h - m_pos.y;
}

int element::get_right_floats_height() const
{
	if (is_floats_holder())
	{
		int h = 0;
		if (!m_floats_right.empty())
		{
			for (auto el = m_floats_right.begin(); el != m_floats_right.end(); ++el)
			{
				h = std::max(h, el->pos.bottom());
			}
		}
		return h;
	}
	const int h = m_parent->get_right_floats_height();
	return h - m_pos.y;
}

int element::get_line_left(const int y)
{
	if (is_floats_holder())
	{
		if (m_cache_line_left.is_valid && m_cache_line_left.hash == y)
		{
			return m_cache_line_left.val;
		}

		int w = 0;
		for (auto el = m_floats_left.begin(); el != m_floats_left.end(); ++el)
		{
			if (y >= el->pos.top() && y < el->pos.bottom())
			{
				w = std::max(w, el->pos.right());
				if (w < el->pos.right())
				{
					break;
				}
			}
		}
		m_cache_line_left.set_value(y, w);
		return w;
	}
	int w = m_parent->get_line_left(y + m_pos.y);
	if (w < 0)
	{
		w = 0;
	}
	return w - (w ? m_pos.x : 0);
}

int element::get_line_right(const int y, const int def_right)
{
	if (m_type == el_text || m_type == el_space)
	{
		return def_right;
	}

	if (is_floats_holder())
	{
		if (m_cache_line_right.is_valid && m_cache_line_right.hash == y)
		{
			if (m_cache_line_right.is_default)
			{
				return def_right;
			}
			return std::min(m_cache_line_right.val, def_right);
		}

		int w = def_right;
		m_cache_line_right.is_default = true;
		for (auto el = m_floats_right.begin(); el != m_floats_right.end(); ++el)
		{
			if (y >= el->pos.top() && y < el->pos.bottom())
			{
				w = std::min(w, el->pos.left());
				m_cache_line_right.is_default = false;
				if (w > el->pos.left())
				{
					break;
				}
			}
		}
		m_cache_line_right.set_value(y, w);
		return w;
	}
	const int w = m_parent->get_line_right(y + m_pos.y, def_right + m_pos.x);
	return w - m_pos.x;
}


void element::get_line_left_right(const int y, const int def_right, int& ln_left, int& ln_right)
{
	if (is_floats_holder())
	{
		ln_left = get_line_left(y);
		ln_right = get_line_right(y, def_right);
	}
	else
	{
		m_parent->get_line_left_right(y + m_pos.y, def_right + m_pos.x, ln_left, ln_right);
		ln_right -= m_pos.x;

		if (ln_left < 0)
		{
			ln_left = 0;
		}
		else if (ln_left)
		{
			ln_left -= m_pos.x;
		}
	}
}

int element::fix_line_width(const int max_width, const element_float flt)
{
	int ret_width = 0;
	if (!m_boxes.empty())
	{
		std::vector<element*> els;
		m_boxes.back()->get_elements(els);
		bool was_cleared = false;
		if (!els.empty() && els.front()->get_clear() != clear_none)
		{
			if (els.front()->get_clear() == clear_both)
			{
				was_cleared = true;
			}
			else
			{
				if ((flt == float_left && els.front()->get_clear() == clear_left) ||
					(flt == float_right && els.front()->get_clear() == clear_right))
				{
					was_cleared = true;
				}
			}
		}

		if (!was_cleared)
		{
			m_boxes.pop_back();

			for (auto i = els.begin(); i != els.end(); ++i)
			{
				const int rw = place_element(*i, max_width);
				if (rw > ret_width)
				{
					ret_width = rw;
				}
			}
		}
		else
		{
			int line_top = 0;
			if (m_boxes.back()->get_type() == box_line)
			{
				line_top = m_boxes.back()->top();
			}
			else
			{
				line_top = m_boxes.back()->bottom();
			}

			int line_left = 0;
			int line_right = max_width;
			get_line_left_right(line_top, max_width, line_left, line_right);

			if (m_boxes.back()->get_type() == box_line)
			{
				if (m_boxes.size() == 1 && m_list_style_type != list_style_type_none && m_list_style_position ==
					list_style_position_inside)
				{
					const int sz_font = get_font_size();
					line_left += sz_font;
				}

				if (props().text_indent.val() != 0)
				{
					bool line_box_found = false;
					for (auto iter = m_boxes.begin(); iter < m_boxes.end(); ++iter)
					{
						if ((*iter)->get_type() == box_line)
						{
							line_box_found = true;
							break;
						}
					}
					if (!line_box_found)
					{
						line_left += props().text_indent.calc_percent(max_width);
					}
				}
			}

			std::vector<element*> els;
			m_boxes.back()->new_width(line_left, line_right, els);
			for (auto i = els.begin(); i != els.end(); ++i)
			{
				const int rw = place_element(*i, max_width);
				if (rw > ret_width)
				{
					ret_width = rw;
				}
			}
		}
	}

	return ret_width;
}

void element::add_float(element* el, const int x, const int y)
{
	if (is_floats_holder())
	{
		floated_box fb;
		fb.pos.x = el->left() + x;
		fb.pos.y = el->top() + y;
		fb.pos.width = el->width();
		fb.pos.height = el->height();
		fb.float_side = el->get_float();
		fb.clear_floats = el->get_clear();
		fb.el = el;

		if (fb.float_side == float_left)
		{
			if (m_floats_left.empty())
			{
				m_floats_left.push_back(fb);
			}
			else
			{
				bool inserted = false;
				for (auto i = m_floats_left.begin(); i != m_floats_left.end(); ++i)
				{
					if (fb.pos.right() > i->pos.right())
					{
						m_floats_left.insert(i, fb);
						inserted = true;
						break;
					}
				}
				if (!inserted)
				{
					m_floats_left.push_back(fb);
				}
			}
			m_cache_line_left.invalidate();
		}
		else if (fb.float_side == float_right)
		{
			if (m_floats_right.empty())
			{
				m_floats_right.push_back(fb);
			}
			else
			{
				bool inserted = false;
				for (auto i = m_floats_right.begin(); i != m_floats_right.end(); ++i)
				{
					if (fb.pos.left() < i->pos.left())
					{
						m_floats_right.insert(i, fb);
						inserted = true;
						break;
					}
				}
				if (!inserted)
				{
					m_floats_right.push_back(fb);
				}
			}
			m_cache_line_right.invalidate();
		}
	}
	else
	{
		m_parent->add_float(el, x + m_pos.x, y + m_pos.y);
	}
}

int element::find_next_line_top(const int top, const int width, const int def_right)
{
	if (is_floats_holder())
	{
		int new_top = top;
		std::vector<int> points;

		for (auto el = m_floats_left.begin(); el != m_floats_left.end(); ++el)
		{
			if (el->pos.top() >= top)
			{
				if (find(points.begin(), points.end(), el->pos.top()) == points.end())
				{
					points.push_back(el->pos.top());
				}
			}
			if (el->pos.bottom() >= top)
			{
				if (find(points.begin(), points.end(), el->pos.bottom()) == points.end())
				{
					points.push_back(el->pos.bottom());
				}
			}
		}

		for (auto el = m_floats_right.begin(); el != m_floats_right.end(); ++el)
		{
			if (el->pos.top() >= top)
			{
				if (find(points.begin(), points.end(), el->pos.top()) == points.end())
				{
					points.push_back(el->pos.top());
				}
			}
			if (el->pos.bottom() >= top)
			{
				if (find(points.begin(), points.end(), el->pos.bottom()) == points.end())
				{
					points.push_back(el->pos.bottom());
				}
			}
		}

		if (!points.empty())
		{
			sort(points.begin(), points.end(), std::less<int>());
			new_top = points.back();

			for (auto i = points.begin(); i != points.end(); ++i)
			{
				int pos_left = 0;
				int pos_right = def_right;
				get_line_left_right(*i, def_right, pos_left, pos_right);

				if (pos_right - pos_left >= width)
				{
					new_top = *i;
					break;
				}
			}
		}
		return new_top;
	}
	const int new_top = m_parent->find_next_line_top(top + m_pos.y, width, def_right + m_pos.x);
	return new_top - m_pos.y;
}

void element::parse_background()
{
	// parse background-color
	props_mut().bg.m_color = get_color(prop_id::background_color, false, web_color(0, 0, 0, 0));

	// parse background-position
	auto str = get_style_property(prop_id::background_position, false, "0% 0%");

	if (!str.empty())
	{
		const auto res = split_string(str, " \t");

		if (res.size() > 0)
		{
			if (res.size() == 1)
			{
				if (value_in_list(res[0], "left;right;center"))
				{
					props_mut().bg.m_position.x.fromString(res[0], "left;right;center");
					props_mut().bg.m_position.y.set_value(50, css_units_percentage);
				}
				else if (value_in_list(res[0], "top;bottom;center"))
				{
					props_mut().bg.m_position.y.fromString(res[0], "top;bottom;center");
					props_mut().bg.m_position.x.set_value(50, css_units_percentage);
				}
				else
				{
					props_mut().bg.m_position.x.fromString(res[0], "left;right;center");
					props_mut().bg.m_position.y.set_value(50, css_units_percentage);
				}
			}
			else
			{
				if (value_in_list(res[0], "left;right"))
				{
					props_mut().bg.m_position.x.fromString(res[0], "left;right;center");
					props_mut().bg.m_position.y.fromString(res[1], "top;bottom;center");
				}
				else if (value_in_list(res[0], "top;bottom"))
				{
					props_mut().bg.m_position.x.fromString(res[1], "left;right;center");
					props_mut().bg.m_position.y.fromString(res[0], "top;bottom;center");
				}
				else if (value_in_list(res[1], "left;right"))
				{
					props_mut().bg.m_position.x.fromString(res[1], "left;right;center");
					props_mut().bg.m_position.y.fromString(res[0], "top;bottom;center");
				}
				else if (value_in_list(res[1], "top;bottom"))
				{
					props_mut().bg.m_position.x.fromString(res[0], "left;right;center");
					props_mut().bg.m_position.y.fromString(res[1], "top;bottom;center");
				}
				else
				{
					props_mut().bg.m_position.x.fromString(res[0], "left;right;center");
					props_mut().bg.m_position.y.fromString(res[1], "top;bottom;center");
				}
			}

			if (props().bg.m_position.x.is_predefined())
			{
				switch (props().bg.m_position.x.predef())
				{
				case 0:
					props_mut().bg.m_position.x.set_value(0, css_units_percentage);
					break;
				case 1:
					props_mut().bg.m_position.x.set_value(100, css_units_percentage);
					break;
				case 2:
					props_mut().bg.m_position.x.set_value(50, css_units_percentage);
					break;
				}
			}
			if (props().bg.m_position.y.is_predefined())
			{
				switch (props().bg.m_position.y.predef())
				{
				case 0:
					props_mut().bg.m_position.y.set_value(0, css_units_percentage);
					break;
				case 1:
					props_mut().bg.m_position.y.set_value(100, css_units_percentage);
					break;
				case 2:
					props_mut().bg.m_position.y.set_value(50, css_units_percentage);
					break;
				}
			}
		}
		else
		{
			props_mut().bg.m_position.x.set_value(0, css_units_percentage);
			props_mut().bg.m_position.y.set_value(0, css_units_percentage);
		}
	}
	else
	{
		props_mut().bg.m_position.y.set_value(0, css_units_percentage);
		props_mut().bg.m_position.x.set_value(0, css_units_percentage);
	}

	str = get_style_property(prop_id::background_size, false, "auto");

	if (!str.empty())
	{
		const auto res = split_string(str, " \t");

		if (!res.empty())
		{
			props_mut().bg.m_position.width.fromString(res[0], background_size_strings);
			if (res.size() > 1)
			{
				props_mut().bg.m_position.height.fromString(res[1], background_size_strings);
			}
			else
			{
				props_mut().bg.m_position.height.predef(background_size_auto);
			}
		}
		else
		{
			props_mut().bg.m_position.width.predef(background_size_auto);
			props_mut().bg.m_position.height.predef(background_size_auto);
		}
	}

	m_doc.cvt_units(props_mut().bg.m_position.x, m_font_size);
	m_doc.cvt_units(props_mut().bg.m_position.y, m_font_size);
	m_doc.cvt_units(props_mut().bg.m_position.width, m_font_size);
	m_doc.cvt_units(props_mut().bg.m_position.height, m_font_size);

	// parse background_attachment
	props_mut().bg.m_attachment = static_cast<background_attachment>(value_index(
		get_style_property(prop_id::background_attachment, false, "scroll"),
		background_attachment_strings,
		background_attachment_scroll));

	// parse background_attachment
	props_mut().bg.m_repeat = static_cast<background_repeat>(value_index(
		get_style_property(prop_id::background_repeat, false, "repeat"),
		background_repeat_strings,
		background_repeat_repeat));

	// parse background_clip
	props_mut().bg.m_clip = static_cast<background_box>(value_index(
		get_style_property(prop_id::background_clip, false, "border-box"),
		background_box_strings,
		background_box_border));

	// parse background_origin
	props_mut().bg.m_origin = static_cast<background_box>(value_index(
		get_style_property(prop_id::background_origin, false, "padding-box"),
		background_box_strings,
		background_box_content));

	// parse background-image
	props_mut().bg.m_image = css::parse_css_url(get_style_property(prop_id::background_image, false));
	props_mut().bg.m_baseurl = get_style_property(prop_id::background_image_baseurl, false);

	if (!props().bg.m_image.empty())
	{
		m_doc.load_image(props().bg.m_image, props().bg.m_baseurl.empty() ? "" : props().bg.m_baseurl);
	}
}

void element::add_positioned(element* el)
{
	if (m_el_position != element_position_static || !m_parent)
	{
		m_positioned.push_back(el);
	}
	else
	{
		m_parent->add_positioned(el);
	}
}

void element::calc_outlines(const int parent_width)
{
	m_padding.left = props().padding.left.calc_percent(parent_width);
	m_padding.right = props().padding.right.calc_percent(parent_width);

	m_borders.left = props().borders.left.width.calc_percent(parent_width);
	m_borders.right = props().borders.right.width.calc_percent(parent_width);

	m_margins.left = props().margins.left.calc_percent(parent_width);
	m_margins.right = props().margins.right.calc_percent(parent_width);

	m_margins.top = props().margins.top.calc_percent(parent_width);
	m_margins.bottom = props().margins.bottom.calc_percent(parent_width);

	m_padding.top = props().padding.top.calc_percent(parent_width);
	m_padding.bottom = props().padding.bottom.calc_percent(parent_width);

	if (m_display == display_block || m_display == display_table)
	{
		if (props().margins.left.is_predefined() && props().margins.right.is_predefined())
		{
			const int el_width = m_pos.width + m_borders.left + m_borders.right + m_padding.left + m_padding.right;
			if (el_width <= parent_width)
			{
				m_margins.left = (parent_width - el_width) / 2;
				m_margins.right = parent_width - el_width - m_margins.left;
			}
			else
			{
				m_margins.left = 0;
				m_margins.right = 0;
			}
		}
		else if (props().margins.left.is_predefined() && !props().margins.right.is_predefined())
		{
			const int el_width = m_pos.width + m_borders.left + m_borders.right + m_padding.left + m_padding.right +
				m_margins.right;
			m_margins.left = parent_width - el_width;
			if (m_margins.left < 0) m_margins.left = 0;
		}
		else if (!props().margins.left.is_predefined() && props().margins.right.is_predefined())
		{
			const int el_width = m_pos.width + m_borders.left + m_borders.right + m_padding.left + m_padding.right +
				m_margins.left;
			m_margins.right = parent_width - el_width;
			if (m_margins.right < 0) m_margins.right = 0;
		}
	}
}

void element::parse_attributes()
{
	if (m_type == el_tr)
	{
		std::string str(get_attr("align"));

		if (!str.empty())
		{
			m_style.add_property("text-align", str, "", false);
		}

		str = get_attr("valign");

		if (!str.empty())
		{
			m_style.add_property("vertical-align", str, "", false);
		}
	}
	else if (m_type == el_title)
	{
		m_doc.set_caption(get_text());
	}
	else if (m_type == el_td)
	{
		std::string str(get_attr("width"));

		if (!str.empty())
		{
			m_style.add_property("width", str, "", false);
		}

		str = get_attr("background");

		if (!str.empty())
		{
			std::string url = "url('";
			url += str;
			url += "')";
			m_style.add_property("background-image", url, "", false);
		}

		str = get_attr("align");

		if (!str.empty())
		{
			m_style.add_property("text-align", str, "", false);
		}

		str = get_attr("valign");

		if (!str.empty())
		{
			m_style.add_property("vertical-align", str, "", false);
		}
	}
	else if (m_type == el_table)
	{
		std::string str(get_attr("width"));

		if (!str.empty())
		{
			m_style.add_property("width", str, "", false);
		}

		str = get_attr("align");

		if (!str.empty())
		{
			int align = value_index(str, "left;center;right");

			switch (align)
			{
			case 1:
				m_style.add_property("margin-left", "auto", "", false);
				m_style.add_property("margin-right", "auto", "", false);
				break;
			case 2:
				m_style.add_property("margin-left", "auto", "", false);
				m_style.add_property("margin-right", "0", "", false);
				break;
			}
		}

		str = get_attr("cellspacing");

		if (!str.empty())
		{
			std::string val = str;
			val += " ";
			val += str;
			m_style.add_property("border-spacing", val, "", false);
		}

		str = get_attr("border");

		if (!str.empty())
		{
			m_style.add_property("border-width", str, "", false);
		}
	}
	else if (m_type == el_style)
	{
		if (!m_loaded)
		{
			m_loaded = true;
			m_doc.add_stylesheet(m_text, "", std::string(get_attr("media", "")));
		}
	}
	else if (m_type == el_para)
	{
		const std::string str(get_attr("align"));

		if (!str.empty())
		{
			m_style.add_property("text-align", str, "", false);
		}
	}
	if (m_type == el_link)
	{
		if (!m_loaded)
		{
			m_loaded = true;

			const auto rel = get_attr("rel");

			if (!rel.empty() && rel == "stylesheet")
			{
				const std::string media(get_attr("media"));
				const std::string href(get_attr("href"));

				if (!href.empty())
				{
					m_doc.import_css(href, empty, media);
				}
			}
			else
			{
				m_doc.link(this);
			}
		}
	}
	else if (m_type == el_image)
	{
		m_src = get_attr("src");

		const std::string attr_height(get_attr("height"));

		if (!attr_height.empty())
		{
			m_style.add_property("height", attr_height, empty, false);
		}

		const std::string attr_width(get_attr("width"));

		if (!attr_width.empty())
		{
			m_style.add_property("width", attr_width, empty, false);
		}
	}
	else if (m_type == el_svg)
	{
		std::string str(get_attr("width"));
		if (!str.empty())
		{
			m_style.add_property("width", str, empty, false);
		}
		else
		{
			m_style.add_property("width", "24px", empty, false);
		}

		str = get_attr("height");
		if (!str.empty())
		{
			m_style.add_property("height", str, empty, false);
		}
		else
		{
			m_style.add_property("height", "24px", empty, false);
		}
	}
	else if (m_type == el_font)
	{
		std::string str(get_attr("color"));

		if (!str.empty())
		{
			m_style.add_property("color", str, empty, false);
		}

		str = get_attr("face");

		if (!str.empty())
		{
			m_style.add_property("font-family", str, empty, false);
		}

		str = get_attr("size");

		if (!str.empty())
		{
			int sz = safe_stoi(str);
			if (sz <= 1)
			{
				m_style.add_property("font-size", "x-small", empty, false);
			}
			else if (sz >= 6)
			{
				m_style.add_property("font-size", "xx-large", empty, false);
			}
			else
			{
				switch (sz)
				{
				case 2:
					m_style.add_property("font-size", "small", empty, false);
					break;
				case 3:
					m_style.add_property("font-size", "medium", empty, false);
					break;
				case 4:
					m_style.add_property("font-size", "large", empty, false);
					break;
				case 5:
					m_style.add_property("font-size", "x-large", empty, false);
					break;
				}
			}
		}
	}
	else if (m_type == el_div)
	{
		const std::string str(get_attr("align"));

		if (!str.empty())
		{
			m_style.add_property("text-align", str, empty, false);
		}
	}
	else if (m_type == el_break)
	{
		const std::string attr_clear(get_attr("clear"));

		if (!attr_clear.empty())
		{
			m_style.add_property("clear", attr_clear, empty, false);
		}
	}
	else if (m_type == el_base)
	{
		m_doc.set_base_url(std::string(get_attr("href")));
		return; //?
	}

	for (const auto& child : m_children)
	{
		child->parse_attributes();
	}
}

std::string element::get_text() const
{
	if (m_type == el_cdata || m_type == el_comment || m_type == el_text || m_type == el_style || m_type == el_space)
	{
		return m_text;
	}

	std::string result;

	for (const auto& child : m_children)
	{
		result += child->get_text();
	}

	return result;
}

void element::set_data(const std::string_view data)
{
	// Raw-text elements (<style>, <script>, <svg>) deliver their content as a
	// single data token rather than as words, so they must accumulate it too.
	if (m_type == el_cdata || m_type == el_comment ||
		m_type == el_style || m_type == el_script || m_type == el_svg)
	{
		m_text += data;
	}
}

void element::get_inline_boxes(position::vector& boxes)
{
	if (m_type == el_tr)
	{
		for (const auto& el : m_children)
		{
			if (el->get_display() == display_table_cell)
			{
				position pos;

				pos.x = el->left() + el->margin_left();
				pos.y = el->top() - m_padding.top - m_borders.top;
				pos.width = el->right() - pos.x - el->margin_right() - el->margin_left();
				pos.height = el->height() + m_padding.top + m_padding.bottom + m_borders.top + m_borders.bottom;

				boxes.push_back(pos);
			}
		}
	}
	else
	{
		const box* old_box = nullptr;
		position pos;
		for (const auto& el : m_children)
		{
			if (!el->skip())
			{
				if (el->m_box)
				{
					if (el->m_box != old_box)
					{
						if (old_box)
						{
							if (boxes.empty())
							{
								pos.x -= m_padding.left + m_borders.left;
								pos.width += m_padding.left + m_borders.left;
							}
							boxes.push_back(pos);
						}
						old_box = el->m_box;
						pos.x = el->left() + el->margin_left();
						pos.y = el->top() - m_padding.top - m_borders.top;
						pos.width = 0;
						pos.height = 0;
					}
					pos.width = el->right() - pos.x - el->margin_right() - el->margin_left();
					pos.height = std::max(
						pos.height, el->height() + m_padding.top + m_padding.bottom + m_borders.top + m_borders.bottom);
				}
				else if (el->get_display() == display_inline)
				{
					position::vector sub_boxes;
					el->get_inline_boxes(sub_boxes);
					if (!sub_boxes.empty())
					{
						sub_boxes.rbegin()->width += el->margin_right();
						if (boxes.empty())
						{
							if (m_padding.left + m_borders.left > 0)
							{
								position padding_box = *sub_boxes.begin();
								padding_box.x -= m_padding.left + m_borders.left + el->margin_left();
								padding_box.width = m_padding.left + m_borders.left + el->margin_left();
								boxes.push_back(padding_box);
							}
						}

						sub_boxes.rbegin()->width += el->margin_right();

						boxes.insert(boxes.end(), sub_boxes.begin(), sub_boxes.end());
					}
				}
			}
		}
		if (pos.width || pos.height)
		{
			if (boxes.empty())
			{
				pos.x -= m_padding.left + m_borders.left;
				pos.width += m_padding.left + m_borders.left;
			}
			boxes.push_back(pos);
		}
		if (!boxes.empty())
		{
			if (m_padding.right + m_borders.right > 0)
			{
				position padding_box = *boxes.rbegin();
				padding_box.x += padding_box.width;
				padding_box.width = m_padding.right + m_borders.right;
				boxes.push_back(padding_box);
			}
		}
	}
}

bool element::on_mouse_over()
{
	bool ret = false;
	auto el = this;

	while (el)
	{
		if (el->set_pseudo_class("hover", true))
		{
			ret = true;
		}
		el = el->parent();
	}

	return ret;
}

bool element::find_styles_changes(position::vector& redraw_boxes, const int x, const int y)
{
	if (m_display == display_inline_text)
	{
		return false;
	}

	bool ret = false;
	bool apply = false;
	for (auto iter = m_used_styles.begin(); iter != m_used_styles.end() && !apply; ++iter)
	{
		if (iter->m_selector->is_media_valid())
		{
			const int res = select(*iter->m_selector, true);
			if ((res == select_no_match && iter->m_used) || (res == select_match && !iter->m_used))
			{
				apply = true;
			}
		}
	}

	if (apply)
	{
		if (m_display == display_inline || m_display == display_table_row)
		{
			position::vector boxes;
			get_inline_boxes(boxes);
			for (auto pos = boxes.begin(); pos != boxes.end(); ++pos)
			{
				pos->x += x;
				pos->y += y;
				redraw_boxes.push_back(*pos);
			}
		}
		else
		{
			position pos = m_pos;
			if (m_el_position != element_position_fixed)
			{
				pos.x += x;
				pos.y += y;
			}
			pos += m_padding;
			pos += m_borders;
			redraw_boxes.push_back(pos);
		}

		ret = true;
		refresh_styles();
		parse_styles();
	}
	for (const auto& child : m_children)
	{
		if (!child->skip())
		{
			if (m_el_position != element_position_fixed)
			{
				if (child->find_styles_changes(redraw_boxes, x + m_pos.x, y + m_pos.y))
				{
					ret = true;
				}
			}
			else
			{
				if (child->find_styles_changes(redraw_boxes, m_pos.x, m_pos.y))
				{
					ret = true;
				}
			}
		}
	}
	return ret;
}

bool element::on_mouse_leave()
{
	bool ret = false;
	auto el = this;

	while (el)
	{
		if (el->set_pseudo_class("hover", false))
		{
			ret = true;
		}
		if (el->set_pseudo_class("active", false))
		{
			ret = true;
		}
		el = el->parent();
	}

	return ret;
}

bool element::on_lbutton_down()
{
	return set_pseudo_class("active", true);
}

bool element::on_lbutton_up()
{
	bool ret = false;

	if (set_pseudo_class("active", false))
	{
		ret = true;
		on_click();
	}

	return ret;
}

void element::on_click()
{
	if (m_type == el_anchor)
	{
		const std::string href(get_attr("href"));

		if (!href.empty())
		{
			m_doc.on_anchor_click(href, this);
		}
	}
	else if (parent())
	{
		parent()->on_click();
	}
}

std::string element::get_cursor() const
{
	return get_style_property(prop_id::cursor, true);
}

static const int font_size_table[8][7] =
{
	{9, 9, 9, 9, 11, 14, 18},
	{9, 9, 9, 10, 12, 15, 20},
	{9, 9, 9, 11, 13, 17, 22},
	{9, 9, 10, 12, 14, 18, 24},
	{9, 9, 10, 13, 16, 20, 26},
	{9, 9, 11, 14, 17, 21, 28},
	{9, 10, 12, 15, 17, 23, 30},
	{9, 10, 13, 16, 18, 24, 32}
};


void element::init_font()
{
	// initialize font size
	const auto str = get_style_property(prop_id::font_size, false);

	int parent_sz = 0;
	const int doc_font_size = m_doc.get_default_font_size();
	if (m_parent)
	{
		parent_sz = m_parent->get_font_size();
	}
	else
	{
		parent_sz = doc_font_size;
	}


	if (str.empty())
	{
		m_font_size = parent_sz;
	}
	else
	{
		m_font_size = parent_sz;

		css_length sz;
		sz.fromString(str, font_size_strings);
		if (sz.is_predefined())
		{
			const int idx_in_table = doc_font_size - 9;
			if (idx_in_table >= 0 && idx_in_table <= 7)
			{
				if (sz.predef() >= font_size_xx_small && sz.predef() <= font_size_xx_large)
				{
					m_font_size = font_size_table[idx_in_table][sz.predef()];
				}
				else
				{
					m_font_size = doc_font_size;
				}
			}
			else
			{
				switch (sz.predef())
				{
				case font_size_xx_small:
					m_font_size = doc_font_size * 3 / 5;
					break;
				case font_size_x_small:
					m_font_size = doc_font_size * 3 / 4;
					break;
				case font_size_small:
					m_font_size = doc_font_size * 8 / 9;
					break;
				case font_size_large:
					m_font_size = doc_font_size * 6 / 5;
					break;
				case font_size_x_large:
					m_font_size = doc_font_size * 3 / 2;
					break;
				case font_size_xx_large:
					m_font_size = doc_font_size * 2;
					break;
				default:
					m_font_size = doc_font_size;
					break;
				}
			}
		}
		else
		{
			if (sz.units() == css_units_percentage)
			{
				m_font_size = sz.calc_percent(parent_sz);
			}
			else if (sz.units() == css_units_none)
			{
				m_font_size = parent_sz;
			}
			else
			{
				m_font_size = m_doc.cvt_units(sz, parent_sz);
			}
		}
	}

	// initialize font
	const auto name = get_style_property(prop_id::font_family, true, "inherit");
	const auto weight = get_style_property(prop_id::font_weight, true, "normal");
	const auto style = get_style_property(prop_id::font_style, true, "normal");
	const auto decoration = get_style_property(prop_id::text_decoration, true, "none");


	m_font = m_doc.get_font(name, m_font_size, weight, style, decoration, &m_font_metrics);
}

bool element::is_break() const
{
	if (m_type == el_space)
	{
		const auto ws = get_white_space();

		if (ws == white_space_pre ||
			ws == white_space_pre_line ||
			ws == white_space_pre_wrap)
		{
			if (m_text == "\n")
			{
				return true;
			}
		}

		return false;
	}

	return m_type == el_break;
}

void element::set_tag_name(const std::string_view name)
{
	m_tag = name;
}

void element::draw_background(render_win32& renderer, int x, int y, const position* clip)
{
	position pos = m_pos;
	pos.x += x;
	pos.y += y;

	position el_pos = pos;
	el_pos += m_padding;
	el_pos += m_borders;

	if (m_display != display_inline && m_display != display_table_row)
	{
		if (el_pos.does_intersect(clip))
		{
			const background* bg = get_background();
			if (bg)
			{
				background_paint bg_paint;
				init_background_paint(pos, bg_paint, bg);

				renderer.draw_background(renderer, bg_paint);
			}
			position border_box = pos;
			border_box += m_padding;
			border_box += m_borders;
			renderer.draw_borders(props().borders, border_box, parent() ? false : true);
		}
	}
	else
	{
		const background* bg = get_background();

		position::vector boxes;
		get_inline_boxes(boxes);

		background_paint bg_paint;
		position content_box;

		for (auto box = boxes.begin(); box != boxes.end(); ++box)
		{
			box->x += x;
			box->y += y;

			if (box->does_intersect(clip))
			{
				content_box = *box;
				content_box -= m_borders;
				content_box -= m_padding;

				if (bg)
				{
					init_background_paint(content_box, bg_paint, bg);
				}

				css_borders bdr;

				// set left borders radius for the first box
				if (box == boxes.begin())
				{
					bdr.radius.bottom_left_x = props().borders.radius.bottom_left_x;
					bdr.radius.bottom_left_y = props().borders.radius.bottom_left_y;
					bdr.radius.top_left_x = props().borders.radius.top_left_x;
					bdr.radius.top_left_y = props().borders.radius.top_left_y;
				}

				// set right borders radius for the last box
				if (box == boxes.end() - 1)
				{
					bdr.radius.bottom_right_x = props().borders.radius.bottom_right_x;
					bdr.radius.bottom_right_y = props().borders.radius.bottom_right_y;
					bdr.radius.top_right_x = props().borders.radius.top_right_x;
					bdr.radius.top_right_y = props().borders.radius.top_right_y;
				}


				bdr.top = props().borders.top;
				bdr.bottom = props().borders.bottom;
				if (box == boxes.begin())
				{
					bdr.left = props().borders.left;
				}
				if (box == boxes.end() - 1)
				{
					bdr.right = props().borders.right;
				}


				if (bg)
				{
					bg_paint.border_radius = bdr.radius;
					renderer.draw_background(renderer, bg_paint);
				}

				renderer.draw_borders(bdr, *box, false);
			}
		}
	}
}

int element::render_inline(element* container, const int max_width)
{
	if (m_type == el_text || m_type == el_space)
	{
		return 0;
	}

	int ret_width = 0;
	int rw = 0;
	for (const auto& child : m_children)
	{
		rw = container->place_element(child.get(), max_width);

		if (rw > ret_width)
		{
			ret_width = rw;
		}
	}
	return ret_width;
}

int element::place_element(element* el, const int max_width)
{
	if (el->get_display() == display_none) return 0;

	if (el->get_display() == display_inline)
	{
		return el->render_inline(this, max_width);
	}

	const auto el_position = el->get_element_position();

	if (el_position == element_position_absolute || el_position == element_position_fixed)
	{
		int line_top = 0;
		if (!m_boxes.empty())
		{
			if (m_boxes.back()->get_type() == box_line)
			{
				line_top = m_boxes.back()->top();
				if (!m_boxes.back()->is_empty())
				{
					line_top += line_height();
				}
			}
			else
			{
				line_top = m_boxes.back()->bottom();
			}
		}

		el->render(0, line_top, max_width);
		el->m_pos.x += el->content_margins_left();
		el->m_pos.y += el->content_margins_top();

		return 0;
	}

	int ret_width = 0;

	switch (el->get_float())
	{
	case float_left:
		{
			int line_top = 0;
			if (!m_boxes.empty())
			{
				if (m_boxes.back()->get_type() == box_line)
				{
					line_top = m_boxes.back()->top();
				}
				else
				{
					line_top = m_boxes.back()->bottom();
				}
			}
			line_top = get_cleared_top(el, line_top);
			int line_left = 0;
			int line_right = max_width;
			get_line_left_right(line_top, max_width, line_left, line_right);

			el->render(line_left, line_top, line_right);
			if (el->right() > line_right)
			{
				const int new_top = find_next_line_top(el->top(), el->width(), max_width);
				el->m_pos.x = get_line_left(new_top) + el->content_margins_left();
				el->m_pos.y = new_top + el->content_margins_top();
			}
			add_float(el, 0, 0);
			ret_width = fix_line_width(max_width, float_left);
			if (!ret_width)
			{
				ret_width = el->right();
			}
		}
		break;
	case float_right:
		{
			int line_top = 0;
			if (!m_boxes.empty())
			{
				if (m_boxes.back()->get_type() == box_line)
				{
					line_top = m_boxes.back()->top();
				}
				else
				{
					line_top = m_boxes.back()->bottom();
				}
			}
			line_top = get_cleared_top(el, line_top);
			int line_left = 0;
			int line_right = max_width;
			get_line_left_right(line_top, max_width, line_left, line_right);

			el->render(0, line_top, line_right);

			if (line_left + el->width() > line_right)
			{
				const int new_top = find_next_line_top(el->top(), el->width(), max_width);
				el->m_pos.x = get_line_right(new_top, max_width) - el->width() + el->content_margins_left();
				el->m_pos.y = new_top + el->content_margins_top();
			}
			else
			{
				el->m_pos.x = line_right - el->width() + el->content_margins_left();
			}
			add_float(el, 0, 0);
			ret_width = fix_line_width(max_width, float_right);

			if (!ret_width)
			{
				line_left = 0;
				line_right = max_width;
				get_line_left_right(line_top, max_width, line_left, line_right);

				ret_width = ret_width + (max_width - line_right);
			}
		}
		break;
	default:
		{
			int line_top = 0;
			if (!m_boxes.empty())
			{
				line_top = m_boxes.back()->top();
			}
			int line_left = 0;
			int line_right = max_width;
			get_line_left_right(line_top, max_width, line_left, line_right);

			switch (el->get_display())
			{
			case display_inline_block:
			case display_inline_flex:
				ret_width = el->render(line_left, line_top, line_right);
				break;
			case display_block:
			case display_flex:
				if (el->is_replaced() || el->is_floats_holder())
				{
					el->m_pos.width = el->get_css_width().calc_percent(line_right - line_left);
					el->m_pos.height = el->get_css_height().calc_percent(0);
					if (el->m_pos.width || el->m_pos.height)
					{
						el->calc_outlines(line_right - line_left);
					}
				}
				break;
			case display_inline_text:
				{
					size sz;
					el->get_content_size(sz, line_right);
					el->m_pos = sz;
				}
				break;
			default:
				ret_width = 0;
				break;
			}

			bool add_box = true;
			if (!m_boxes.empty())
			{
				if (m_boxes.back()->can_hold(el, m_white_space))
				{
					add_box = false;
				}
			}
			if (add_box)
			{
				line_top = new_box(el, max_width);
			}
			else if (!m_boxes.empty())
			{
				line_top = m_boxes.back()->top();
			}

			line_left = 0;
			line_right = max_width;
			get_line_left_right(line_top, max_width, line_left, line_right);

			if (!el->is_inline_box())
			{
				if (m_boxes.size() == 1)
				{
					if (collapse_top_margin())
					{
						const int shift = el->margin_top();
						if (shift >= 0)
						{
							line_top -= shift;
							m_boxes.back()->y_shift(-shift);
						}
					}
				}
				else
				{
					int shift = 0;
					const int prev_margin = m_boxes[m_boxes.size() - 2]->bottom_margin();

					if (prev_margin > el->margin_top())
					{
						shift = el->margin_top();
					}
					else
					{
						shift = prev_margin;
					}
					if (shift >= 0)
					{
						line_top -= shift;
						m_boxes.back()->y_shift(-shift);
					}
				}
			}

			switch (el->get_display())
			{
			case display_table:
			case display_list_item:
				ret_width = el->render(line_left, line_top, line_right - line_left);
				break;
			case display_block:
			case display_flex:
			case display_table_cell:
			case display_table_caption:
			case display_table_row:
				if (el->is_replaced() || el->is_floats_holder())
				{
					ret_width = el->render(line_left, line_top, line_right - line_left) + line_left + (
						max_width - line_right);
				}
				else
				{
					ret_width = el->render(0, line_top, max_width);
				}
				break;
			case display_inline_flex:
				ret_width = el->render(line_left, line_top, line_right - line_left);
				break;
			default:
				ret_width = 0;
				break;
			}

			m_boxes.back()->add_element(el);

			if (el->is_inline_box() && !el->skip())
			{
				ret_width = el->right() + (max_width - line_right);
			}
		}
		break;
	}

	return ret_width;
}

bool element::set_pseudo_class(const std::string& pclass, const bool add)
{
	bool ret = false;
	if (add)
	{
		if (std::find(m_pseudo_classes.begin(), m_pseudo_classes.end(), pclass) == m_pseudo_classes.end())
		{
			m_pseudo_classes.push_back(pclass);
			ret = true;
		}
	}
	else
	{
		const auto pi = std::find(m_pseudo_classes.begin(), m_pseudo_classes.end(), pclass);
		if (pi != m_pseudo_classes.end())
		{
			m_pseudo_classes.erase(pi);
			ret = true;
		}
	}
	return ret;
}

int element::line_height() const
{
	if (m_type == el_text || m_type == el_space)
	{
		return m_parent->line_height();
	}
	if (m_type == el_image)
	{
		return height();
	}

	return m_line_height;
}

bool element::is_replaced() const
{
	return m_type == el_image || m_type == el_svg;
}

int element::finish_last_box(const bool end_of_render)
{
	int line_top = 0;

	if (!m_boxes.empty())
	{
		m_boxes.back()->finish(end_of_render);

		if (m_boxes.back()->is_empty())
		{
			line_top = m_boxes.back()->top();
			m_boxes.pop_back();
		}

		if (!m_boxes.empty())
		{
			line_top = m_boxes.back()->bottom();
		}
	}
	return line_top;
}

int element::new_box(const element* el, const int max_width)
{
	int line_top = get_cleared_top(el, finish_last_box());

	int line_left = 0;
	int line_right = max_width;
	get_line_left_right(line_top, max_width, line_left, line_right);

	if (el->is_inline_box() || el->is_floats_holder())
	{
		if (el->width() > line_right - line_left)
		{
			line_top = find_next_line_top(line_top, el->width(), max_width);
			line_left = 0;
			line_right = max_width;
			get_line_left_right(line_top, max_width, line_left, line_right);
		}
	}

	int first_line_margin = 0;
	if (m_boxes.empty() && m_list_style_type != list_style_type_none && m_list_style_position ==
		list_style_position_inside)
	{
		const int sz_font = get_font_size();
		first_line_margin = sz_font;
	}

	if (el->is_inline_box())
	{
		int text_indent = 0;
		if (props().text_indent.val() != 0)
		{
			bool line_box_found = false;
			for (auto iter = m_boxes.begin(); iter != m_boxes.end(); ++iter)
			{
				if ((*iter)->get_type() == box_line)
				{
					line_box_found = true;
					break;
				}
			}
			if (!line_box_found)
			{
				text_indent = props().text_indent.calc_percent(max_width);
			}
		}

		font_metrics fm;
		get_font(&fm);

		m_boxes.push_back(std::make_unique<box>(box_line, line_top, line_left + first_line_margin + text_indent,
		                                        line_right,
		                                        line_height(), fm, m_text_align));
	}
	else
	{
		m_boxes.push_back(std::make_unique<box>(box_block, line_top, line_left, line_right, line_height(),
		                                        m_font_metrics,
		                                        m_text_align));
	}

	return line_top;
}

int element::get_cleared_top(const element* el, int line_top)
{
	switch (el->get_clear())
	{
	case clear_left:
		{
			const int fh = get_left_floats_height();
			if (fh && fh > line_top)
			{
				line_top = fh;
			}
		}
		break;
	case clear_right:
		{
			const int fh = get_right_floats_height();
			if (fh && fh > line_top)
			{
				line_top = fh;
			}
		}
		break;
	case clear_both:
		{
			const int fh = get_floats_height();
			if (fh && fh > line_top)
			{
				line_top = fh;
			}
		}
		break;
	default:
		if (el->get_float() != float_none)
		{
			const int fh = get_floats_height(el->get_float());
			if (fh && fh > line_top)
			{
				line_top = fh;
			}
		}
		break;
	}
	return line_top;
}

style_display element::get_display() const
{
	if (m_type == el_text || m_type == el_space)
	{
		return display_inline_text;
	}

	return m_display;
}

element_float element::get_float() const
{
	return m_float;
}

bool element::is_floats_holder() const
{
	if (m_type == el_text || m_type == el_space)
	{
		return false;
	}

	return m_display == display_inline_block ||
		m_display == display_flex ||
		m_display == display_inline_flex ||
		m_display == display_table_cell ||
		(m_parent && (m_parent->get_display() == display_flex ||
			m_parent->get_display() == display_inline_flex)) ||
		!m_parent ||
		is_body() ||
		m_float != float_none ||
		m_el_position == element_position_absolute ||
		m_el_position == element_position_fixed ||
		m_overflow > overflow_visible;
}

bool element::is_first_child_inline(const element* el)
{
	for (const auto& child : m_children)
	{
		if (!child->is_white_space())
		{
			if (el == child.get())
			{
				return true;
			}
			if (child->get_display() == display_inline)
			{
				if (child->have_inline_child())
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
	}
	return false;
}

bool element::is_last_child_inline(const element* el)
{
	for (auto iter = m_children.rbegin(); iter != m_children.rend(); ++iter)
	{
		if (!(*iter)->is_white_space())
		{
			if (el == iter->get())
			{
				return true;
			}
			if ((*iter)->get_display() == display_inline)
			{
				if ((*iter)->have_inline_child())
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
	}
	return false;
}

white_space element::get_white_space() const
{
	if (m_type == el_text || m_type == el_space)
	{
		if (m_parent) return m_parent->get_white_space();
		return white_space_normal;
	}

	return m_white_space;
}

vertical_align element::get_vertical_align() const
{
	return m_vertical_align;
}

css_length element::get_css_left() const
{
	if (m_type == el_text || m_type == el_space)
	{
		return css_length();
	}

	return props().offsets.left;
}

css_length element::get_css_right() const
{
	if (m_type == el_text || m_type == el_space)
	{
		return css_length();
	}

	return props().offsets.right;
}

css_length element::get_css_top() const
{
	if (m_type == el_text || m_type == el_space)
	{
		return css_length();
	}

	return props().offsets.top;
}

css_length element::get_css_bottom() const
{
	if (m_type == el_text || m_type == el_space)
	{
		return css_length();
	}

	return props().offsets.bottom;
}


css_offsets element::get_css_offsets() const
{
	if (m_type == el_text || m_type == el_space)
	{
		auto p = parent();

		while (p && p->get_display() == display_inline)
		{
			if (p->get_element_position() == element_position_relative)
			{
				return p->get_css_offsets();
			}
			p = p->parent();
		}
	}

	return props().offsets;
}

element_clear element::get_clear() const
{
	if (m_type == el_text || m_type == el_space)
	{
		return clear_none;
	}

	return m_clear;
}

css_length element::get_css_width() const
{
	if (m_type == el_text || m_type == el_space)
	{
		return css_length();
	}

	return props().width;
}

css_length element::get_css_height() const
{
	if (m_type == el_text || m_type == el_space)
	{
		return css_length();
	}

	return props().height;
}

size_t element::get_children_count() const
{
	return m_children.size();
}

element* element::get_child(const int idx) const
{
	return m_children[idx].get();
}

void element::set_css_width(const css_length& w)
{
	props_mut().width = w;
}

void element::apply_vertical_align()
{
	if (!m_boxes.empty())
	{
		int add = 0;
		const int content_height = m_boxes.back()->bottom();

		if (m_pos.height > content_height)
		{
			switch (m_vertical_align)
			{
			case va_middle:
				add = (m_pos.height - content_height) / 2;
				break;
			case va_bottom:
				add = m_pos.height - content_height;
				break;
			default:
				add = 0;
				break;
			}
		}

		if (add)
		{
			for (size_t i = 0; i < m_boxes.size(); i++)
			{
				m_boxes[i]->y_shift(add);
			}
		}
	}
}

element_position element::get_element_position(css_offsets* offsets) const
{
	if (m_type == el_text || m_type == el_space)
	{
		auto p = parent();
		while (p && p->get_display() == display_inline)
		{
			if (p->get_element_position() == element_position_relative)
			{
				if (offsets)
				{
					*offsets = p->get_css_offsets();
				}
				return element_position_relative;
			}
			p = p->parent();
		}
		return element_position_static;
	}

	if (offsets && m_el_position != element_position_static)
	{
		*offsets = props().offsets;
	}
	return m_el_position;
}

void element::init_background_paint(const position pos, background_paint& bg_paint,
                                    const background* bg)
{
	if (!bg) return;


	bg_paint.attachment = bg->m_attachment;
	bg_paint.image = m_doc.find_image(bg->m_image, bg->m_baseurl);
	bg_paint.repeat = bg->m_repeat;
	bg_paint.color = bg->m_color;

	const position content_box = pos;
	position padding_box = pos;
	padding_box += m_padding;
	position border_box = padding_box;
	border_box += m_borders;

	switch (bg->m_clip)
	{
	case background_box_padding:
		bg_paint.clip_box = padding_box;
		break;
	case background_box_content:
		bg_paint.clip_box = content_box;
		break;
	default:
		bg_paint.clip_box = border_box;
		break;
	}

	switch (bg->m_origin)
	{
	case background_box_border:
		bg_paint.origin_box = border_box;
		break;
	case background_box_content:
		bg_paint.origin_box = content_box;
		break;
	default:
		bg_paint.origin_box = padding_box;
		break;
	}

	if (bg_paint.image)
	{
		bg_paint.image_size = image_size(bg_paint.image);

		if (bg_paint.image_size.width && bg_paint.image_size.height)
		{
			size img_new_sz = bg_paint.image_size;
			const double img_ar_width = static_cast<double>(bg_paint.image_size.width) / static_cast<double>(bg_paint.
				image_size.height);
			const double img_ar_height = static_cast<double>(bg_paint.image_size.height) / static_cast<double>(bg_paint.
				image_size.width);


			if (bg->m_position.width.is_predefined())
			{
				switch (bg->m_position.width.predef())
				{
				case background_size_contain:
					if (static_cast<int>(static_cast<double>(bg_paint.origin_box.width) * img_ar_height) <= bg_paint.
						origin_box.height)
					{
						img_new_sz.width = bg_paint.origin_box.width;
						img_new_sz.height = static_cast<int>(static_cast<double>(bg_paint.origin_box.width) *
							img_ar_height);
					}
					else
					{
						img_new_sz.height = bg_paint.origin_box.height;
						img_new_sz.width = static_cast<int>(static_cast<double>(bg_paint.origin_box.height) *
							img_ar_width);
					}
					break;
				case background_size_cover:
					if (static_cast<int>(static_cast<double>(bg_paint.origin_box.width) * img_ar_height) >= bg_paint.
						origin_box.height)
					{
						img_new_sz.width = bg_paint.origin_box.width;
						img_new_sz.height = static_cast<int>(static_cast<double>(bg_paint.origin_box.width) *
							img_ar_height);
					}
					else
					{
						img_new_sz.height = bg_paint.origin_box.height;
						img_new_sz.width = static_cast<int>(static_cast<double>(bg_paint.origin_box.height) *
							img_ar_width);
					}
					break;
				case background_size_auto:
					if (!bg->m_position.height.is_predefined())
					{
						img_new_sz.height = bg->m_position.height.calc_percent(bg_paint.origin_box.height);
						img_new_sz.width = static_cast<int>(static_cast<double>(img_new_sz.height) * img_ar_width);
					}
					break;
				}
			}
			else
			{
				img_new_sz.width = bg->m_position.width.calc_percent(bg_paint.origin_box.width);
				if (bg->m_position.height.is_predefined())
				{
					img_new_sz.height = static_cast<int>(static_cast<double>(img_new_sz.width) * img_ar_height);
				}
				else
				{
					img_new_sz.height = bg->m_position.height.calc_percent(bg_paint.origin_box.height);
				}
			}

			bg_paint.image_size = img_new_sz;
			bg_paint.position_x = bg_paint.origin_box.x + bg->m_position.x.calc_percent(
				bg_paint.origin_box.width - bg_paint.image_size.width);
			bg_paint.position_y = bg_paint.origin_box.y + bg->m_position.y.calc_percent(
				bg_paint.origin_box.height - bg_paint.image_size.height);
		}
	}
	bg_paint.border_radius = props().borders.radius;
	bg_paint.border_box = border_box;
	bg_paint.is_root = parent() ? false : true;
}

visibility element::get_visibility() const
{
	return m_visibility;
}

void element::draw_list_marker(render_win32& renderer, const position& pos)
{
	list_marker lm;

	const auto list_image = get_style_property(prop_id::list_style_image, true);
	size img_size;

	if (!list_image.empty())
	{
		lm.image = css::parse_css_url(list_image);
		lm.baseurl = get_style_property(prop_id::list_style_image_baseurl, true);
		img_size = image_size(m_doc.find_image(lm.image, lm.baseurl));
	}

	const int ln_height = line_height();
	const int sz_font = get_font_size();
	lm.pos.x = pos.x;
	lm.pos.width = sz_font - sz_font * 2 / 3;
	lm.pos.height = sz_font - sz_font * 2 / 3;
	lm.pos.y = pos.y + ln_height / 2 - lm.pos.height / 2;

	if (img_size.width && img_size.height)
	{
		if (lm.pos.y + img_size.height > pos.y + pos.height)
		{
			lm.pos.y = pos.y + pos.height - img_size.height;
		}
		if (img_size.width > lm.pos.width)
		{
			lm.pos.x -= img_size.width - lm.pos.width;
		}

		lm.pos.width = img_size.width;
		lm.pos.height = img_size.height;
	}
	if (m_list_style_position == list_style_position_outside)
	{
		lm.pos.x -= sz_font;
	}

	lm.color = get_color(prop_id::color, true, web_color(0, 0, 0));
	lm.marker_type = m_list_style_type;
	renderer.draw_list_marker(lm);
}

void element::draw_children(render_win32& renderer, const int x, const int y, const position* clip,
                            const draw_flag flag, const int zindex)
{
	position pos = m_pos;
	pos.x += x;
	pos.y += y;

	if (m_type == el_table)
	{
		for (int row = 0; row < grid().rows_count(); row++)
		{
			if (flag == draw_block)
			{
				grid().row(row).el_row->draw_background(renderer, pos.x, pos.y, clip);
			}
			for (int col = 0; col < grid().cols_count(); col++)
			{
				const table_cell* cell = grid().cell(col, row);
				if (cell->el)
				{
					if (flag == draw_block)
					{
						cell->el->draw(renderer, pos.x, pos.y, clip);
					}
					cell->el->draw_children(renderer, pos.x, pos.y, clip, flag, zindex);
				}
			}
		}
	}
	else
	{
		if (m_overflow > overflow_visible)
		{
			renderer.set_clip(pos, true, true);
		}

		const auto browser_wnd = m_doc.client_pos();

		for (const auto& child_ptr : m_children)
		{
			auto el = child_ptr.get();
			if (el->is_visible())
			{
				switch (flag)
				{
				case draw_positioned:
					if (el->is_positioned() && el->get_zindex() == zindex)
					{
						if (el->get_element_position() == element_position_fixed)
						{
							el->draw(renderer, browser_wnd.x, browser_wnd.y, clip);
							el->draw_stacking_context(renderer, browser_wnd.x, browser_wnd.y, clip, true);
						}
						else
						{
							el->draw(renderer, pos.x, pos.y, clip);
							el->draw_stacking_context(renderer, pos.x, pos.y, clip, true);
						}
						el = nullptr;
					}
					break;
				case draw_block:
					if (!el->is_inline_box() && el->get_float() == float_none && !el->is_positioned())
					{
						el->draw(renderer, pos.x, pos.y, clip);
					}
					break;
				case draw_floats:
					if (el->get_float() != float_none && !el->is_positioned())
					{
						el->draw(renderer, pos.x, pos.y, clip);
						el->draw_stacking_context(renderer, pos.x, pos.y, clip, false);
						el = nullptr;
					}
					break;
				case draw_inlines:
					if (el->is_inline_box() && el->get_float() == float_none && !el->is_positioned())
					{
						el->draw(renderer, pos.x, pos.y, clip);
						if (el->get_display() == display_inline_block)
						{
							el->draw_stacking_context(renderer, pos.x, pos.y, clip, false);
							el = nullptr;
						}
					}
					break;
				default:
					break;
				}

				if (el)
				{
					if (flag == draw_positioned)
					{
						if (!el->is_positioned())
						{
							el->draw_children(renderer, pos.x, pos.y, clip, flag, zindex);
						}
					}
					else
					{
						if (el->get_float() == float_none &&
							el->get_display() != display_inline_block &&
							!el->is_positioned())
						{
							el->draw_children(renderer, pos.x, pos.y, clip, flag, zindex);
						}
					}
				}
			}
		}

		if (m_overflow > overflow_visible)
		{
			renderer.del_clip();
		}
	}
}

bool element::fetch_positioned()
{
	bool ret = false;

	m_positioned.clear();

	for (const auto& el : m_children)
	{
		if (el->get_element_position() != element_position_static)
		{
			add_positioned(el.get());
		}
		if (!ret && (el->get_element_position() == element_position_absolute || el->get_element_position() ==
			element_position_fixed))
		{
			ret = true;
		}
		if (el->fetch_positioned())
		{
			ret = true;
		}
	}
	return ret;
}

int element::get_zindex() const
{
	return m_z_index;
}

void element::render_positioned(const render_type rt)
{
	if (m_type == el_text || m_type == el_space)
	{
		return;
	}

	const position wnd_position = m_doc.client_pos();

	for (const auto& el : m_positioned)
	{
		const element_position el_position = el->get_element_position();

		bool process = false;
		if (el->get_display() != display_none)
		{
			if (el_position == element_position_absolute)
			{
				if (rt != render_fixed_only)
				{
					process = true;
				}
			}
			else if (el_position == element_position_fixed)
			{
				if (rt != render_no_fixed)
				{
					process = true;
				}
			}
		}

		if (process)
		{
			int parent_height = 0;
			int parent_width = 0;
			int client_x = 0;
			int client_y = 0;
			if (el_position == element_position_fixed)
			{
				parent_height = wnd_position.height;
				parent_width = wnd_position.width;
				client_x = wnd_position.left();
				client_y = wnd_position.top();
			}
			else
			{
				if (el->parent())
				{
					parent_height = el->parent()->height();
					parent_width = el->parent()->width();
				}
			}

			css_length css_left = el->get_css_left();
			css_length css_right = el->get_css_right();
			css_length css_top = el->get_css_top();
			css_length css_bottom = el->get_css_bottom();

			bool need_render = false;

			css_length el_w = el->get_css_width();
			css_length el_h = el->get_css_height();
			if (el_w.units() == css_units_percentage && parent_width)
			{
				const int w = el_w.calc_percent(parent_width);
				if (el->m_pos.width != w)
				{
					need_render = true;
					el->m_pos.width = w;
				}
			}

			if (el_h.units() == css_units_percentage && parent_height)
			{
				const int h = el_h.calc_percent(parent_height);
				if (el->m_pos.height != h)
				{
					need_render = true;
					el->m_pos.height = h;
				}
			}

			bool cvt_x = false;
			bool cvt_y = false;

			if (el_position == element_position_fixed)
			{
				if (!css_left.is_predefined() || !css_right.is_predefined())
				{
					if (!css_left.is_predefined() && css_right.is_predefined())
					{
						el->m_pos.x = css_left.calc_percent(parent_width) + el->content_margins_left();
					}
					else if (css_left.is_predefined() && !css_right.is_predefined())
					{
						el->m_pos.x = parent_width - css_right.calc_percent(parent_width) - el->m_pos.width - el->
							content_margins_right();
					}
					else
					{
						el->m_pos.x = css_left.calc_percent(parent_width) + el->content_margins_left();
						el->m_pos.width = parent_width - css_left.calc_percent(parent_width) - css_right.
							calc_percent(parent_width) - (el->content_margins_left() + el->content_margins_right());
						need_render = true;
					}
				}

				if (!css_top.is_predefined() || !css_bottom.is_predefined())
				{
					if (!css_top.is_predefined() && css_bottom.is_predefined())
					{
						el->m_pos.y = css_top.calc_percent(parent_height) + el->content_margins_top();
					}
					else if (css_top.is_predefined() && !css_bottom.is_predefined())
					{
						el->m_pos.y = parent_height - css_bottom.calc_percent(parent_height) - el->m_pos.height - el->
							content_margins_bottom();
					}
					else
					{
						el->m_pos.y = css_top.calc_percent(parent_height) + el->content_margins_top();
						el->m_pos.height = parent_height - css_top.calc_percent(parent_height) - css_bottom.
							calc_percent(parent_height) - (el->content_margins_top() + el->content_margins_bottom());
						need_render = true;
					}
				}
			}
			else
			{
				if (!css_left.is_predefined() || !css_right.is_predefined())
				{
					if (!css_left.is_predefined() && css_right.is_predefined())
					{
						el->m_pos.x = css_left.calc_percent(parent_width) + el->content_margins_left() - m_padding.left;
					}
					else if (css_left.is_predefined() && !css_right.is_predefined())
					{
						el->m_pos.x = m_pos.width + m_padding.right - css_right.calc_percent(parent_width) - el->m_pos.
							width - el->content_margins_right();
					}
					else
					{
						el->m_pos.x = css_left.calc_percent(parent_width) + el->content_margins_left() - m_padding.left;
						el->m_pos.width = m_pos.width + m_padding.left + m_padding.right - css_left.
							calc_percent(parent_width) - css_right.calc_percent(parent_width) - (el->
								content_margins_left() + el->content_margins_right());
						need_render = true;
					}
					cvt_x = true;
				}

				if (!css_top.is_predefined() || !css_bottom.is_predefined())
				{
					if (!css_top.is_predefined() && css_bottom.is_predefined())
					{
						el->m_pos.y = css_top.calc_percent(parent_height) + el->content_margins_top() - m_padding.top;
					}
					else if (css_top.is_predefined() && !css_bottom.is_predefined())
					{
						el->m_pos.y = m_pos.height + m_padding.bottom - css_bottom.calc_percent(parent_height) - el->
							m_pos.height - el->content_margins_bottom();
					}
					else
					{
						el->m_pos.y = css_top.calc_percent(parent_height) + el->content_margins_top() - m_padding.top;
						el->m_pos.height = m_pos.height + m_padding.top + m_padding.bottom - css_top.
							calc_percent(parent_height) - css_bottom.calc_percent(parent_height) - (el->
								content_margins_top() + el->content_margins_bottom());
						need_render = true;
					}
					cvt_y = true;
				}
			}

			if (cvt_x || cvt_y)
			{
				int offset_x = 0;
				int offset_y = 0;
				auto cur_el = el->parent();
				const auto pThis = this;

				while (cur_el && cur_el != pThis)
				{
					offset_x += cur_el->m_pos.x;
					offset_y += cur_el->m_pos.y;
					cur_el = cur_el->parent();
				}
				if (cvt_x) el->m_pos.x -= offset_x;
				if (cvt_y) el->m_pos.y -= offset_y;
			}

			if (need_render)
			{
				const position pos = el->m_pos;
				el->render(el->left(), el->top(), el->width(), true);
				el->m_pos = pos;
			}

			if (el_position == element_position_fixed)
			{
				position fixed_pos;
				el->get_redraw_box(fixed_pos);
				m_doc.add_fixed_box(fixed_pos);
			}
		}

		el->render_positioned();
	}

	if (!m_positioned.empty())
	{
		std::stable_sort(m_positioned.begin(), m_positioned.end(), element_zindex_sort());
	}
}

void element::draw_stacking_context(render_win32& renderer, const int x, const int y, const position* clip,
                                    const bool with_positioned)
{
	if (is_visible())
	{
		std::set<int> zindexes;

		if (with_positioned)
		{
			for (const auto& el : m_positioned)
			{
				zindexes.insert(el->get_zindex());
			}

			for (const auto& idx : zindexes)
			{
				if (idx < 0)
				{
					draw_children(renderer, x, y, clip, draw_positioned, idx);
				}
			}
		}

		draw_children(renderer, x, y, clip, draw_block, 0);
		draw_children(renderer, x, y, clip, draw_floats, 0);
		draw_children(renderer, x, y, clip, draw_inlines, 0);

		if (with_positioned)
		{
			for (const auto& idx : zindexes)
			{
				if (idx == 0)
				{
					draw_children(renderer, x, y, clip, draw_positioned, idx);
				}
			}

			for (const auto& idx : zindexes)
			{
				if (idx > 0)
				{
					draw_children(renderer, x, y, clip, draw_positioned, idx);
				}
			}
		}
	}
}

overflow element::get_overflow() const
{
	return m_overflow;
}

bool element::is_nth_child(const element* el, const int num, const int off, const bool of_type)
{
	int idx = 1;

	for (const auto& child : m_children)
	{
		if (child->get_display() != display_inline_text)
		{
			if (!of_type || (of_type && el->get_tag_name() == child->get_tag_name()))
			{
				if (el == child.get())
				{
					if (num != 0)
					{
						// An+B matches when (idx - B) is a non-negative multiple of A,
						// which for a negative A means counting back from B.
						if ((idx - off) % num == 0 && (idx - off) / num >= 0)
						{
							return true;
						}
					}
					else if (idx == off)
					{
						return true;
					}
					return false;
				}
				idx++;
			}
			if (el == child.get()) break;
		}
	}
	return false;
}

bool element::is_nth_last_child(const element* el, const int num, const int off, const bool of_type)
{
	int idx = 1;
	for (auto child = m_children.rbegin(); child != m_children.rend(); ++child)
	{
		if ((*child)->get_display() != display_inline_text)
		{
			if (!of_type || (of_type && el->get_tag_name() == (*child)->get_tag_name()))
			{
				if (el == child->get())
				{
					if (num != 0)
					{
						if ((idx - off) % num == 0 && (idx - off) / num >= 0)
						{
							return true;
						}
					}
					else if (idx == off)
					{
						return true;
					}
					return false;
				}
				idx++;
			}
			if (el == child->get()) break;
		}
	}
	return false;
}

// Parses the An+B microsyntax: "odd", "even", "3", "n", "2n", "-n+3", "2n + 1".
void element::parse_nth_child_params(const std::string& param, int& num, int& off)
{
	num = 0;
	off = 0;

	std::string s;
	for (const auto c : param)
	{
		if (!is_space_char(c)) s += static_cast<char>(tolower(static_cast<unsigned char>(c)));
	}

	if (s == "odd")
	{
		num = 2;
		off = 1;
		return;
	}
	if (s == "even")
	{
		num = 2;
		return;
	}

	const auto n = s.find('n');

	if (n == std::string::npos)
	{
		off = safe_stoi(s);
		return;
	}

	const auto a = s.substr(0, n);
	num = a.empty() || a == "+" ? 1 : a == "-" ? -1 : safe_stoi(a);
	off = safe_stoi(s.substr(n + 1));
}

void element::calc_document_size(size& sz, const int x /*= 0*/, const int y /*= 0*/)
{
	if (is_visible() && m_el_position != element_position_fixed)
	{
		sz.width = std::max(sz.width, x + right());
		sz.height = std::max(sz.height, y + bottom());

		if (m_overflow == overflow_visible)
		{
			for (const auto& el : m_children)
			{
				el->calc_document_size(sz, x + m_pos.x, y + m_pos.y);
			}
		}

		// root element (<html>) must to cover entire window
		if (!parent())
		{
			const position client_pos = m_doc.client_pos();
			m_pos.height = std::max(sz.height, client_pos.height) - content_margins_top() - content_margins_bottom();
			m_pos.width = std::max(sz.width, client_pos.width) - content_margins_left() - content_margins_right();
		}
	}
}


void element::get_redraw_box(position& pos, const int x /*= 0*/, const int y /*= 0*/)
{
	if (is_visible())
	{
		const int p_left = std::min(pos.left(), x + m_pos.left() - m_padding.left - m_borders.left);
		const int p_right = std::max(pos.right(), x + m_pos.right() + m_padding.left + m_borders.left);
		const int p_top = std::min(pos.top(), y + m_pos.top() - m_padding.top - m_borders.top);
		const int p_bottom = std::max(pos.bottom(), y + m_pos.bottom() + m_padding.bottom + m_borders.bottom);

		pos.x = p_left;
		pos.y = p_top;
		pos.width = p_right - p_left;
		pos.height = p_bottom - p_top;

		if (m_overflow == overflow_visible)
		{
			for (const auto& el : m_children)
			{
				if (el->get_element_position() != element_position_fixed)
				{
					el->get_redraw_box(pos, x + m_pos.x, y + m_pos.y);
				}
			}
		}
	}
}

element* element::find_adjacent_sibling(const element* el, const css_selector& selector,
                                        const bool apply_pseudo /*= true*/, bool* is_pseudo /*= 0*/)
{
	element* ret = nullptr;

	for (const auto& e : m_children)
	{
		if (e->get_display() != display_inline_text)
		{
			if (e.get() == el)
			{
				if (ret)
				{
					const int res = ret->select(selector, apply_pseudo);
					if (res != select_no_match)
					{
						if (is_pseudo)
						{
							*is_pseudo = (res & select_match_pseudo_class) != 0;
						}
						return ret;
					}
				}
				return nullptr;
			}
			ret = e.get();
		}
	}
	return nullptr;
}

element* element::find_sibling(const element* el, const css_selector& selector, const bool apply_pseudo /*= true*/,
                               bool* is_pseudo /*= 0*/)
{
	element* ret = nullptr;

	for (const auto& e : m_children)
	{
		if (e->get_display() != display_inline_text)
		{
			if (e.get() == el)
			{
				return ret;
			}
			if (!ret)
			{
				const int res = e->select(selector, apply_pseudo);
				if (res != select_no_match)
				{
					if (is_pseudo)
					{
						*is_pseudo = (res & select_match_pseudo_class) != 0;
					}
					ret = e.get();
				}
			}
		}
	}
	return nullptr;
}

bool element::is_only_child(const element* el, const bool of_type)
{
	int child_count = 0;
	for (const auto& child : m_children)
	{
		if (child->get_display() != display_inline_text)
		{
			if (!of_type || (of_type && el->get_tag_name() == child->get_tag_name()))
			{
				child_count++;
			}
			if (child_count > 1) break;
		}
	}
	if (child_count > 1)
	{
		return false;
	}
	return true;
}

void element::update_floats(const int dy, element* parent)
{
	if (is_floats_holder())
	{
		bool reset_cache = false;
		for (auto fb = m_floats_left.rbegin(); fb != m_floats_left.rend(); ++fb)
		{
			if (fb->el->is_ancestor(parent))
			{
				reset_cache = true;
				fb->pos.y += dy;
			}
		}
		if (reset_cache)
		{
			m_cache_line_left.invalidate();
		}
		reset_cache = false;
		for (auto fb = m_floats_right.rbegin(); fb != m_floats_right.rend(); ++fb)
		{
			if (fb->el->is_ancestor(parent))
			{
				reset_cache = true;
				fb->pos.y += dy;
			}
		}
		if (reset_cache)
		{
			m_cache_line_right.invalidate();
		}
	}
	else
	{
		m_parent->update_floats(dy, parent);
	}
}

void element::remove_before_after()
{
	if (!m_children.empty())
	{
		if (m_children.front()->get_tag_name() == "::before")
		{
			m_children.erase(m_children.begin());
		}
	}
	if (!m_children.empty())
	{
		if (m_children.back()->get_tag_name() == "::after")
		{
			m_children.erase(m_children.end() - 1);
		}
	}
}

element* element::get_element_before()
{
	if (!m_children.empty())
	{
		if (m_children.front()->get_tag_name() == "::before")
		{
			return m_children.front().get();
		}
	}

	auto el = std::make_unique<element>(m_doc, el_before);
	const auto raw = el.get();
	raw->parent(this);
	m_children.insert(m_children.begin(), std::move(el));
	return raw;
}

element* element::get_element_after()
{
	if (!m_children.empty())
	{
		if (m_children.back()->get_tag_name() == "::after")
		{
			return m_children.back().get();
		}
	}
	auto el = std::make_unique<element>(m_doc, el_after);
	const auto raw = el.get();
	raw->parent(this);
	m_children.push_back(std::move(el));
	return raw;
}

void element::add_style(const std::shared_ptr<style>& st)
{
	if (m_type == el_text || m_type == el_space)
	{
		return;
	}

	m_style.combine(*st);

	if (m_type == el_before || m_type == el_after)
	{
		const auto content = get_style_property(prop_id::content, false);

		if (!content.empty())
		{
			const int idx = value_index(content, content_property_string);
			if (idx < 0)
			{
				std::string fnc;
				std::string::size_type i = 0;
				while (i < content.length() && i != std::string::npos)
				{
					if (content.at(i) == '\"')
					{
						fnc.clear();
						i++;

						const auto pos = content.find_first_of('\"', i);
						std::string txt;

						if (pos == std::string::npos)
						{
							txt = content.substr(i);
							i = std::string::npos;
						}
						else
						{
							txt = content.substr(i, pos - i);
							i = pos + 1;
						}
						add_text(txt);
					}
					else if (content.at(i) == '(')
					{
						i++;

						const auto pos = content.find_first_of(')', i);
						std::string params;

						if (pos == std::string::npos)
						{
							params = content.substr(i);
							i = std::string::npos;
						}
						else
						{
							params = content.substr(i, pos - i);
							i = pos + 1;
						}

						add_function(trim_lower(fnc), params);
						fnc.clear();
					}
					else
					{
						fnc += content.at(i);
						i++;
					}
				}
			}
		}
	}
}

bool element::have_inline_child()
{
	for (const auto& child : m_children)
	{
		if (!child->is_white_space())
		{
			return true;
		}
	}

	return false;
}

void element::refresh_styles()
{
	remove_before_after();

	for (const auto& child : m_children)
	{
		if (child->get_display() != display_inline_text)
		{
			child->refresh_styles();
		}
	}

	m_style.clear();

	for (auto& usel : m_used_styles)
	{
		usel.m_used = false;

		if (usel.m_selector->is_media_valid())
		{
			const int apply = select(*usel.m_selector, false);

			if (apply != select_no_match)
			{
				if (apply & select_match_pseudo_class)
				{
					if (select(*usel.m_selector, true))
					{
						add_style(usel.m_selector->m_style);
						usel.m_used = true;
					}
				}
				else if (apply & select_match_with_after)
				{
					element* el = get_element_after();
					if (el)
					{
						el->add_style(usel.m_selector->m_style);
					}
				}
				else if (apply & select_match_with_before)
				{
					element* el = get_element_before();
					if (el)
					{
						el->add_style(usel.m_selector->m_style);
					}
				}
				else
				{
					add_style(usel.m_selector->m_style);
					usel.m_used = true;
				}
			}
		}
	}
}

element* element::get_child_by_point(const int x, const int y, const int client_x, const int client_y,
                                     const draw_flag flag, const int zindex)
{
	element* ret = nullptr;

	if (m_overflow > overflow_visible)
	{
		if (!m_pos.is_point_inside(x, y))
		{
			return ret;
		}
	}

	position pos = m_pos;
	pos.x = x - pos.x;
	pos.y = y - pos.y;

	for (auto i = m_children.rbegin(); i != m_children.rend() && !ret; ++i)
	{
		auto el = i->get();

		if (el->is_visible() && el->get_display() != display_inline_text)
		{
			switch (flag)
			{
			case draw_positioned:
				if (el->is_positioned() && el->get_zindex() == zindex)
				{
					if (el->get_element_position() == element_position_fixed)
					{
						ret = el->get_element_by_point(client_x, client_y, client_x, client_y);
						if (!ret && (*i)->is_point_inside(client_x, client_y))
						{
							ret = i->get();
						}
					}
					else
					{
						ret = el->get_element_by_point(pos.x, pos.y, client_x, client_y);
						if (!ret && (*i)->is_point_inside(pos.x, pos.y))
						{
							ret = i->get();
						}
					}
					el = nullptr;
				}
				break;
			case draw_block:
				if (!el->is_inline_box() && el->get_float() == float_none && !el->is_positioned())
				{
					if (el->is_point_inside(pos.x, pos.y))
					{
						ret = el;
					}
				}
				break;
			case draw_floats:
				if (el->get_float() != float_none && !el->is_positioned())
				{
					ret = el->get_element_by_point(pos.x, pos.y, client_x, client_y);

					if (!ret && (*i)->is_point_inside(pos.x, pos.y))
					{
						ret = i->get();
					}
					el = nullptr;
				}
				break;
			case draw_inlines:
				if (el->is_inline_box() && el->get_float() == float_none && !el->is_positioned())
				{
					if (el->get_display() == display_inline_block)
					{
						ret = el->get_element_by_point(pos.x, pos.y, client_x, client_y);
						el = nullptr;
					}
					if (!ret && (*i)->is_point_inside(pos.x, pos.y))
					{
						ret = i->get();
					}
				}
				break;
			default:
				break;
			}

			if (el && !el->is_positioned())
			{
				if (flag == draw_positioned)
				{
					const auto child = el->get_child_by_point(pos.x, pos.y, client_x, client_y, flag, zindex);

					if (child)
					{
						ret = child;
					}
				}
				else
				{
					if (el->get_float() == float_none &&
						el->get_display() != display_inline_block)
					{
						const auto child = el->get_child_by_point(pos.x, pos.y, client_x, client_y, flag, zindex);

						if (child)
						{
							ret = child;
						}
					}
				}
			}
		}
	}

	return ret;
}

element* element::get_element_by_point(const int x, const int y, const int client_x, const int client_y)
{
	element* ret = nullptr;

	if (is_visible())
	{
		std::set<int> zindexes;

		for (const auto& i : m_positioned)
		{
			zindexes.insert(i->get_zindex());
		}

		for (const auto& idx : zindexes)
		{
			if (idx > 0)
			{
				ret = get_child_by_point(x, y, client_x, client_y, draw_positioned, idx);
				if (ret) return ret;
			}
		}

		for (const auto& idx : zindexes)
		{
			if (idx == 0)
			{
				ret = get_child_by_point(x, y, client_x, client_y, draw_positioned, idx);
				if (ret) return ret;
			}
		}

		ret = get_child_by_point(x, y, client_x, client_y, draw_inlines, 0);
		if (ret) return ret;

		ret = get_child_by_point(x, y, client_x, client_y, draw_floats, 0);
		if (ret) return ret;

		ret = get_child_by_point(x, y, client_x, client_y, draw_block, 0);
		if (ret) return ret;

		for (const auto& idx : zindexes)
		{
			if (idx < 0)
			{
				ret = get_child_by_point(x, y, client_x, client_y, draw_positioned, idx);
				if (ret) return ret;
			}
		}

		if (m_el_position == element_position_fixed)
		{
			if (is_point_inside(client_x, client_y))
			{
				ret = this;
			}
		}
		else if (is_point_inside(x, y))
		{
			ret = this;
		}
	}

	return ret;
}

const background* element::get_background(const bool own_only)
{
	if (own_only)
	{
		// return own background with check for empty one
		if (props().bg.m_image.empty() && !props().bg.m_color.alpha)
		{
			return nullptr;
		}
		return &props().bg;
	}

	if (props().bg.m_image.empty() && !props().bg.m_color.alpha)
	{
		// if this is root element (<html>) try to get background from body
		if (!parent())
		{
			for (const auto& child : m_children)
			{
				if (child->is_body())
				{
					// return own body background
					return child->get_background(true);
				}
			}
		}
		return nullptr;
	}

	if (is_body())
	{
		if (!m_parent->get_background(true))
		{
			// parent of body will draw background for body
			return nullptr;
		}
	}

	return &props().bg;
}


void element::add_text(const std::string& txt)
{
	std::string word;
	std::string esc;

	for (auto i = 0; i < txt.length(); i++)
	{
		if (txt.at(i) == ' ' || txt.at(i) == '\t' || (txt.at(i) == '\\' && !esc.empty()))
		{
			if (esc.empty())
			{
				if (!word.empty())
				{
					append_child(std::make_unique<element>(m_doc, el_text, word));
					word.clear();
				}

				append_child(std::make_unique<element>(m_doc, el_text, txt.substr(i, 1)));
			}
			else
			{
				word += convert_escape(esc.c_str() + 1);
				esc.clear();
				if (txt.at(i) == '\\')
				{
					esc += txt.at(i);
				}
			}
		}
		else
		{
			if (!esc.empty() || txt.at(i) == '\\')
			{
				esc += txt.at(i);
			}
			else
			{
				word += txt.at(i);
			}
		}
	}

	if (!esc.empty())
	{
		word += convert_escape(esc.c_str() + 1);
	}
	if (!word.empty())
	{
		append_child(std::make_unique<element>(m_doc, el_text, word));
		word.clear();
	}
}

void element::add_function(const std::string& fnc, const std::string& params)
{
	const int idx = value_index(fnc, "attr;counter;url");

	switch (idx)
	{
	// attr
	case 0:
		{
			const std::string attr_value(m_parent->get_attr(trim_lower(params)));

			if (!attr_value.empty())
			{
				add_text(attr_value);
			}
		}
		break;
	// counter
	case 1:
		break;
	// url
	case 2:
		{
			std::string p_url = params;
			trim(p_url);
			if (!p_url.empty())
			{
				if (p_url.at(0) == '\'' || p_url.at(0) == '\"')
				{
					p_url.erase(0, 1);
				}
			}
			if (!p_url.empty())
			{
				if (p_url.at(p_url.length() - 1) == '\'' || p_url.at(p_url.length() - 1) == '\"')
				{
					p_url.erase(p_url.length() - 1, 1);
				}
			}
			if (!p_url.empty())
			{
				auto el = std::make_unique<element>(m_doc, el_image);
				el->set_attr("src", p_url);
				el->set_attr("style", "display:inline-block");
				el->set_tag_name("img");
				const auto raw = el.get();

				if (!append_child(std::move(el)))
				{
					raw->parse_attributes();
				}
			}
		}
		break;
	}
}

char element::convert_escape(const char* txt)
{
	return static_cast<char>(safe_stol(txt, 16));
}

std::string element::resolve_custom_property(const std::string& name) const
{
	const auto* el = this;

	while (el)
	{
		const auto val = el->m_style.get_custom_property(name);

		if (!val.empty())
		{
			return std::string(val);
		}

		el = el->m_parent;
	}

	return empty;
}


int box::height() const
{
	return m_type == box_block ? m_element->height() : m_height;
}

int box::width() const
{
	return m_type == box_block ? m_element->width() : m_width;
}

void box::add_element(element* el)
{
	if (m_type == box_block)
	{
		m_element = el;
		el->m_box = this;
	}
	else
	{
		el->m_skip = false;
		el->m_box = nullptr;

		bool add = true;
		if ((m_items.empty() && el->is_white_space()) || el->is_break())
		{
			el->m_skip = true;
		}
		else if (el->is_white_space())
		{
			const element* ws = get_last_space();
			if (ws)
			{
				add = false;
				el->m_skip = true;
			}
		}

		if (add)
		{
			el->m_box = this;
			m_items.push_back(el);

			if (!el->m_skip)
			{
				const int el_shift_left = el->get_inline_shift_left();
				const int el_shift_right = el->get_inline_shift_right();

				el->m_pos.x = m_box_left + m_width + el_shift_left + el->content_margins_left();
				el->m_pos.y = m_box_top + el->content_margins_top();
				m_width += el->width() + el_shift_left + el_shift_right;
			}
		}
	}
}

void box::finish(const bool last_box)
{
	if (m_type == box_block)
	{
		if (!m_element) return;

		css_offsets offsets;
		if (m_element->get_element_position(&offsets) == element_position_relative)
		{
			if (!offsets.left.is_predefined())
			{
				m_element->m_pos.x += offsets.left.calc_percent(m_box_right - m_box_left);
			}
			else if (!offsets.right.is_predefined())
			{
				m_element->m_pos.x -= offsets.right.calc_percent(m_box_right - m_box_left);
			}
			if (!offsets.top.is_predefined())
			{
				int h = 0;
				if (offsets.top.units() == css_units_percentage)
				{
					if (m_element->parent())
					{
						m_element->parent()->get_predefined_height(h);
					}
				}
				m_element->m_pos.y += offsets.top.calc_percent(h);
			}
			else if (!offsets.bottom.is_predefined())
			{
				int h = 0;
				if (offsets.bottom.units() == css_units_percentage)
				{
					if (m_element->parent())
					{
						m_element->parent()->get_predefined_height(h);
					}
				}
				m_element->m_pos.y -= offsets.bottom.calc_percent(h);
			}
		}
	}
	else
	{
		if (is_empty() || (!is_empty() && last_box && is_break_only()))
		{
			m_height = 0;
			return;
		}

		for (auto i = m_items.rbegin(); i != m_items.rend(); ++i)
		{
			if ((*i)->is_white_space() || (*i)->is_break())
			{
				if (!(*i)->m_skip)
				{
					(*i)->m_skip = true;
					m_width -= (*i)->width();
				}
			}
			else
			{
				break;
			}
		}

		int base_line = m_font_metrics.base_line();
		int line_height = m_line_height;

		int add_x = 0;
		switch (m_text_align)
		{
		case text_align_right:
			if (m_width < m_box_right - m_box_left)
			{
				add_x = m_box_right - m_box_left - m_width;
			}
			break;
		case text_align_center:
			if (m_width < m_box_right - m_box_left)
			{
				add_x = (m_box_right - m_box_left - m_width) / 2;
			}
			break;
		default:
			add_x = 0;
		}

		m_height = 0;

		// find line box baseline and line-height
		for (const auto& i : m_items)
		{
			if (i->get_display() == display_inline_text)
			{
				font_metrics fm;
				i->get_font(&fm);
				base_line = std::max(base_line, fm.base_line());
				line_height = std::max(line_height, i->line_height());
				m_height = std::max(m_height, fm.height);
			}
			i->m_pos.x += add_x;
		}

		if (m_height)
		{
			base_line += (line_height - m_height) / 2;
		}

		m_height = line_height;

		int y1 = 0;
		int y2 = m_height;

		for (const auto& i : m_items)
		{
			if (i->get_display() == display_inline_text)
			{
				font_metrics fm;
				i->get_font(&fm);
				i->m_pos.y = m_height - base_line - fm.ascent;
			}
			else
			{
				switch (i->get_vertical_align())
				{
				case va_super:
				case va_sub:
				case va_baseline:
					i->m_pos.y = m_height - base_line - i->height() + i->get_base_line() + i->content_margins_top();
					break;
				case va_top:
					i->m_pos.y = y1 + i->content_margins_top();
					break;
				case va_text_top:
					i->m_pos.y = m_height - base_line - m_font_metrics.ascent + i->content_margins_top();
					break;
				case va_middle:
					i->m_pos.y = m_height - base_line - m_font_metrics.x_height / 2 - i->height() / 2 + i->
						content_margins_top();
					break;
				case va_bottom:
					i->m_pos.y = y2 - i->height() + i->content_margins_top();
					break;
				case va_text_bottom:
					i->m_pos.y = m_height - base_line + m_font_metrics.descent - i->height() + i->content_margins_top();
					break;
				}
				y1 = std::min(y1, i->top());
				y2 = std::max(y2, i->bottom());
			}
		}

		css_offsets offsets;

		for (const auto& i : m_items)
		{
			i->m_pos.y -= y1;
			i->m_pos.y += m_box_top;

			if (i->get_display() != display_inline_text)
			{
				switch (i->get_vertical_align())
				{
				case va_top:
					i->m_pos.y = m_box_top + i->content_margins_top();
					break;
				case va_bottom:
					i->m_pos.y = m_box_top + (y2 - y1) - i->height() + i->content_margins_top();
					break;
				case va_baseline:
					//TODO: process vertical align "baseline"
					break;
				case va_middle:
					//TODO: process vertical align "middle"
					break;
				case va_sub:
					//TODO: process vertical align "sub"
					break;
				case va_super:
					//TODO: process vertical align "super"
					break;
				case va_text_bottom:
					//TODO: process vertical align "text-bottom"
					break;
				case va_text_top:
					//TODO: process vertical align "text-top"
					break;
				}
			}

			// update position for relative positioned elements

			if (i->get_element_position(&offsets) == element_position_relative)
			{
				if (!offsets.left.is_predefined())
				{
					i->m_pos.x += offsets.left.calc_percent(m_box_right - m_box_left);
				}
				else if (!offsets.right.is_predefined())
				{
					i->m_pos.x -= offsets.right.calc_percent(m_box_right - m_box_left);
				}
				if (!offsets.top.is_predefined())
				{
					// TODO: m_line_height is not correct here
					i->m_pos.y += offsets.top.calc_percent(m_line_height);
				}
				else if (!offsets.bottom.is_predefined())
				{
					// TODO: m_line_height is not correct here
					i->m_pos.y -= offsets.bottom.calc_percent(m_line_height);
				}
			}
		}
		m_height = y2 - y1;
		m_baseline = base_line - y1 - (m_height - line_height);
	}
}

bool box::can_hold(element* el, const white_space ws) const
{
	if (m_type == box_block)
	{
		if (m_element || el->is_inline_box())
		{
			return false;
		}
	}
	else
	{
		if (!el->is_inline_box()) return false;

		if (el->is_break())
		{
			return false;
		}

		if (ws == white_space_nowrap || ws == white_space_pre)
		{
			return true;
		}

		if (m_box_left + m_width + el->width() + el->get_inline_shift_left() + el->get_inline_shift_right() >
			m_box_right)
		{
			return false;
		}
	}

	return true;
}

bool box::is_empty() const
{
	if (m_type == box_block)
	{
		if (m_element)
		{
			return false;
		}
	}
	else
	{
		if (m_items.empty()) return true;

		for (auto i = m_items.rbegin(); i != m_items.rend(); ++i)
		{
			if (!(*i)->m_skip || (*i)->is_break())
			{
				return false;
			}
		}
	}

	return true;
}

int box::baseline() const
{
	if (m_type == box_block)
	{
		if (m_element)
		{
			return m_element->get_base_line();
		}
	}
	else
	{
		return m_baseline;
	}

	return 0;
}

void box::get_elements(std::vector<element*>& els)
{
	if (m_type == box_block)
	{
		els.push_back(m_element);
	}
	else
	{
		els.insert(els.begin(), m_items.begin(), m_items.end());
	}
}

int box::top_margin() const
{
	if (m_type == box_block)
	{
		if (m_element && m_element->in_normal_flow() && m_element->get_float() == float_none &&
			m_element->m_margins.top >= 0 && m_element->parent())
		{
			return m_element->m_margins.top;
		}
	}

	return 0;
}

int box::bottom_margin() const
{
	if (m_type == box_block)
	{
		if (m_element && m_element->in_normal_flow() && m_element->get_float() == float_none &&
			m_element->m_margins.bottom >= 0 && m_element->parent())
		{
			return m_element->m_margins.bottom;
		}
	}

	return 0;
}

void box::y_shift(const int shift)
{
	if (m_type == box_block)
	{
		m_box_top += shift;
		if (m_element)
		{
			m_element->m_pos.y += shift;
		}
	}
	else
	{
		m_box_top += shift;
		for (auto i = m_items.begin(); i != m_items.end(); ++i)
		{
			(*i)->m_pos.y += shift;
		}
	}
}

void box::new_width(const int left, const int right, std::vector<element*>& els)
{
	if (m_type == box_block)
	{
	}
	else
	{
		const int add = left - m_box_left;
		if (add)
		{
			m_box_left = left;
			m_box_right = right;
			m_width = 0;
			auto remove_begin = m_items.end();

			for (auto i = m_items.begin() + 1; i != m_items.end(); ++i)
			{
				element* el = *i;

				if (!el->m_skip)
				{
					if (m_box_left + m_width + el->width() + el->get_inline_shift_right() + el->get_inline_shift_left()
						> m_box_right)
					{
						remove_begin = i;
						break;
					}
					el->m_pos.x += add;
					m_width += el->width() + el->get_inline_shift_right() + el->get_inline_shift_left();
				}
			}
			if (remove_begin != m_items.end())
			{
				els.insert(els.begin(), remove_begin, m_items.end());
				m_items.erase(remove_begin, m_items.end());

				for (auto i = els.begin(); i != els.end(); ++i)
				{
					(*i)->m_box = nullptr;
				}
			}
		}
	}
}


element* box::get_last_space()
{
	element* ret = nullptr;

	for (auto i = m_items.rbegin(); i != m_items.rend() && !ret; ++i)
	{
		if ((*i)->is_white_space() || (*i)->is_break())
		{
			ret = *i;
		}
		else
		{
			break;
		}
	}
	return ret;
}


bool box::is_break_only() const
{
	if (m_items.empty()) return true;

	if (m_items.front()->is_break())
	{
		for (auto i = m_items.begin() + 1; i != m_items.end(); ++i)
		{
			if (!(*i)->m_skip)
			{
				return false;
			}
		}
		return true;
	}
	return false;
}


void table_grid::add_cell(element* el)
{
	table_cell cell;
	cell.el = el;
	cell.colspan = safe_stoi(std::string(el->get_attr("colspan", "1")), 1);
	cell.rowspan = safe_stoi(std::string(el->get_attr("rowspan", "1")), 1);
	cell.borders = el->get_borders();

	while (is_rowspanned(static_cast<int>(m_cells.size()) - 1, static_cast<int>(m_cells.back().size())))
	{
		m_cells.back().push_back(table_cell());
	}

	m_cells.back().push_back(cell);
	for (int i = 1; i < cell.colspan; i++)
	{
		table_cell empty_cell;
		m_cells.back().push_back(empty_cell);
	}
}


void table_grid::begin_row(element* row)
{
	m_cells.emplace_back();

	m_rows.push_back(table_row(0, row));
}


bool table_grid::is_rowspanned(const int r, const int c)
{
	for (int row = r - 1; row >= 0; row--)
	{
		if (c < static_cast<int>(m_cells[row].size()))
		{
			if (m_cells[row][c].rowspan > 1)
			{
				if (m_cells[row][c].rowspan >= r - row + 1)
				{
					return true;
				}
			}
		}
	}
	return false;
}

void table_grid::finish()
{
	m_rows_count = static_cast<int>(m_cells.size());
	m_cols_count = 0;
	for (int i = 0; i < static_cast<int>(m_cells.size()); i++)
	{
		m_cols_count = std::max(m_cols_count, static_cast<int>(m_cells[i].size()));
	}
	for (int i = 0; i < static_cast<int>(m_cells.size()); i++)
	{
		for (int j = static_cast<int>(m_cells[i].size()); j < m_cols_count; j++)
		{
			table_cell empty_cell;
			m_cells[i].push_back(empty_cell);
		}
	}

	m_columns.clear();
	for (int i = 0; i < m_cols_count; i++)
	{
		m_columns.push_back(table_column(0, 0));
	}

	for (int col = 0; col < m_cols_count; col++)
	{
		for (int row = 0; row < m_rows_count; row++)
		{
			if (cell(col, row)->el)
			{
				// CSS border-collapse: wider border wins
				m_columns[col].border_left = std::max(m_columns[col].border_left, cell(col, row)->borders.left);
				m_columns[col].border_right = std::max(m_columns[col].border_right, cell(col, row)->borders.right);
				m_rows[row].border_top = std::max(m_rows[row].border_top, cell(col, row)->borders.top);
				m_rows[row].border_bottom = std::max(m_rows[row].border_bottom, cell(col, row)->borders.bottom);
			}

			if (cell(col, row)->el && cell(col, row)->colspan <= 1)
			{
				if (!cell(col, row)->el->get_css_width().is_predefined())
				{
					m_columns[col].css_width = cell(col, row)->el->get_css_width();
					break;
				}
			}
		}
	}

	for (int col = 0; col < m_cols_count; col++)
	{
		for (int row = 0; row < m_rows_count; row++)
		{
			if (cell(col, row)->el && cell(col, row)->colspan <= 1)
			{
				cell(col, row)->el->set_css_width(m_columns[col].css_width);
			}
		}
	}
}

table_cell* table_grid::cell(const int t_col, const int t_row)
{
	if (t_col >= 0 && t_col < m_cols_count && t_row >= 0 && t_row < m_rows_count)
	{
		return &m_cells[t_row][t_col];
	}
	return nullptr;
}

void table_grid::distribute_max_width(const int width, const int start, const int end)
{
	table_column_accessor_max_width selector;
	distribute_width(width, start, end, selector);
}

void table_grid::distribute_min_width(const int width, const int start, const int end)
{
	table_column_accessor_min_width selector;
	distribute_width(width, start, end, selector);
}


void table_grid::distribute_width(int width, const int start, const int end)
{
	if (!(start >= 0 && start < m_cols_count && end >= 0 && end < m_cols_count) || start > end)
	{
		return;
	}

	std::vector<table_column*> distribute_columns;

	for (int step = 0; step < 3; step++)
	{
		distribute_columns.clear();

		switch (step)
		{
		case 0:
			{
				// distribute between the columns with width == auto
				for (int col = start; col <= end; col++)
				{
					if (m_columns[col].css_width.is_predefined())
					{
						distribute_columns.push_back(&m_columns[col]);
					}
				}
			}
			break;
		case 1:
			{
				// distribute between the columns with percents
				for (int col = start; col <= end; col++)
				{
					if (!m_columns[col].css_width.is_predefined() && m_columns[col].css_width.units() ==
						css_units_percentage)
					{
						distribute_columns.push_back(&m_columns[col]);
					}
				}
			}
			break;
		case 2:
			{
				//well distribute between all columns
				for (int col = start; col <= end; col++)
				{
					distribute_columns.push_back(&m_columns[col]);
				}
			}
			break;
		}

		int added_width = 0;

		if (!distribute_columns.empty() || step == 2)
		{
			int cols_width = 0;
			for (auto col = distribute_columns.begin(); col != distribute_columns.end(); ++col)
			{
				cols_width += (*col)->max_width - (*col)->min_width;
			}

			if (cols_width)
			{
				int add = width / static_cast<int>(distribute_columns.size());
				for (auto col = distribute_columns.begin(); col != distribute_columns.end(); ++col)
				{
					add = round_f(
						static_cast<float>(width) * (static_cast<float>((*col)->max_width - (*col)->min_width) /
							static_cast<float>(cols_width)));
					if ((*col)->width + add >= (*col)->min_width)
					{
						(*col)->width += add;
						added_width += add;
					}
					else
					{
						const int sign = add > 0 ? 1 : (add < 0 ? -1 : 0);
						added_width += ((*col)->width - (*col)->min_width) * sign;
						(*col)->width = (*col)->min_width;
					}
				}
				if (added_width < width && step)
				{
					distribute_columns.front()->width += width - added_width;
					added_width = width;
				}
			}
			else
			{
				distribute_columns.back()->width += width;
				added_width = width;
			}
		}

		if (added_width == width)
		{
			break;
		}
		width -= added_width;
	}
}

int table_grid::calc_table_width(const int block_width, const bool is_auto)
{
	//int table_width = 0;

	int min_table_width = 0; // MIN
	int max_table_width = 0; // MAX

	int cur_width = 0;
	int max_w = 0;
	int min_w = 0;

	for (int col = 0; col < m_cols_count; col++)
	{
		min_table_width += m_columns[col].min_width;
		max_table_width += m_columns[col].max_width;

		if (!m_columns[col].css_width.is_predefined())
		{
			m_columns[col].width = m_columns[col].css_width.calc_percent(block_width);
			m_columns[col].width = std::max(m_columns[col].width, m_columns[col].min_width);
		}
		else
		{
			m_columns[col].width = m_columns[col].min_width;
			max_w += m_columns[col].max_width;
			min_w += m_columns[col].min_width;
		}

		cur_width += m_columns[col].width;
	}

	if (cur_width == block_width)
	{
		return cur_width;
	}

	if (cur_width < block_width)
	{
		if (cur_width - min_w + max_w <= block_width)
		{
			cur_width = 0;
			for (int col = 0; col < m_cols_count; col++)
			{
				if (m_columns[col].css_width.is_predefined())
				{
					m_columns[col].width = m_columns[col].max_width;
				}
				cur_width += m_columns[col].width;
			}
			if (cur_width == block_width || is_auto)
			{
				return cur_width;
			}
		}
		distribute_width(block_width - cur_width, 0, m_cols_count - 1);
		cur_width = 0;
		for (int col = 0; col < m_cols_count; col++)
		{
			cur_width += m_columns[col].width;
		}
	}
	else
	{
		int fixed_width = 0;
		float percent = 0;
		for (int col = 0; col < m_cols_count; col++)
		{
			if (!m_columns[col].css_width.is_predefined() && m_columns[col].css_width.units() == css_units_percentage)
			{
				percent += m_columns[col].css_width.val();
			}
			else
			{
				fixed_width += m_columns[col].width;
			}
		}
		float scale = 0.0f;
		if (percent > 0)
		{
			scale = static_cast<float>(100.0 / percent);
		}
		cur_width = 0;
		for (int col = 0; col < m_cols_count; col++)
		{
			if (!m_columns[col].css_width.is_predefined() && m_columns[col].css_width.units() == css_units_percentage)
			{
				css_length w;
				w.set_value(m_columns[col].css_width.val() * scale, css_units_percentage);
				m_columns[col].width = w.calc_percent(block_width - fixed_width);
				if (m_columns[col].width < m_columns[col].min_width)
				{
					m_columns[col].width = m_columns[col].min_width;
				}
			}
			cur_width += m_columns[col].width;
		}
	}
	return cur_width;
}

void table_grid::clear()
{
	m_rows_count = 0;
	m_cols_count = 0;
	m_cells.clear();
	m_columns.clear();
	m_rows.clear();
}

void table_grid::calc_horizontal_positions(const margins& table_borders, const border_collapse bc,
                                           const int bdr_space_x)
{
	if (bc == border_collapse_separate)
	{
		int left = bdr_space_x;
		for (int i = 0; i < m_cols_count; i++)
		{
			m_columns[i].left = left;
			m_columns[i].right = m_columns[i].left + m_columns[i].width;
			left = m_columns[i].right + bdr_space_x;
		}
	}
	else
	{
		int left = 0;
		if (m_cols_count)
		{
			left -= std::min(table_borders.left, m_columns[0].border_left);
		}
		for (int i = 0; i < m_cols_count; i++)
		{
			if (i > 0)
			{
				left -= std::min(m_columns[i - 1].border_right, m_columns[i].border_left);
			}

			m_columns[i].left = left;
			m_columns[i].right = m_columns[i].left + m_columns[i].width;
			left = m_columns[i].right;
		}
	}
}

void table_grid::calc_vertical_positions(const margins& table_borders, const border_collapse bc, const int bdr_space_y)
{
	if (bc == border_collapse_separate)
	{
		int top = bdr_space_y;
		for (int i = 0; i < m_rows_count; i++)
		{
			m_rows[i].top = top;
			m_rows[i].bottom = m_rows[i].top + m_rows[i].height;
			top = m_rows[i].bottom + bdr_space_y;
		}
	}
	else
	{
		int top = 0;
		if (m_rows_count)
		{
			top -= std::min(table_borders.top, m_rows[0].border_top);
		}
		for (int i = 0; i < m_rows_count; i++)
		{
			if (i > 0)
			{
				top -= std::min(m_rows[i - 1].border_bottom, m_rows[i].border_top);
			}

			m_rows[i].top = top;
			m_rows[i].bottom = m_rows[i].top + m_rows[i].height;
			top = m_rows[i].bottom;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

int& table_column_accessor_max_width::get(table_column& col)
{
	return col.max_width;
}

int& table_column_accessor_min_width::get(table_column& col)
{
	return col.min_width;
}

int& table_column_accessor_width::get(table_column& col)
{
	return col.width;
}
