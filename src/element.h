// element.h - DOM element node, box layout model (block/inline), table grid
// layout, and tree iterators. Defines the element class that owns children,
// computes the box model, and participates in selector matching.

#pragma once
#include "core.h"
#include "style.h"
#include <memory>


class element;

enum box_type
{
	box_block,
	box_line
};

class box
{
protected:
	box_type m_type;

	int m_box_top;
	int m_box_left;
	int m_box_right;

	element* m_element;
	std::vector<element*> m_items;
	int m_height;
	int m_width;
	int m_line_height;
	font_metrics m_font_metrics;
	int m_baseline;
	text_align m_text_align;

public:
	box(const box_type t, const int top, const int left, const int right, const int line_height, const font_metrics& fm,
	    const text_align align)
	{
		m_type = t;
		m_box_top = top;
		m_box_left = left;
		m_box_right = right;
		m_element = nullptr;
		m_height = 0;
		m_width = 0;
		m_font_metrics = fm;
		m_line_height = line_height;
		m_baseline = 0;
		m_text_align = align;
	}

	int bottom() { return m_box_top + height(); }
	int top() { return m_box_top; }
	int right() { return m_box_left + width(); }
	int left() { return m_box_left; }

	box_type get_type() const { return m_type; };

	int height() const;
	int width() const;

	void add_element(element* el);
	bool can_hold(element* el, white_space ws) const;
	void finish(bool last_box = false);
	bool is_empty() const;
	int baseline() const;
	void get_elements(std::vector<element*>& els);
	int top_margin() const;
	int bottom_margin() const;
	void y_shift(int shift);
	void new_width(int left, int right, std::vector<element*>& els);

private:
	element* get_last_space();
	bool is_break_only() const;
};


struct table_row
{
	int height;
	int border_top;
	int border_bottom;
	element* el_row;
	int top;
	int bottom;

	table_row()
	{
		top = 0;
		bottom = 0;
		border_bottom = 0;
		border_top = 0;
		height = 0;
		el_row = nullptr;
	}

	table_row(const int h, element* row)
	{
		height = h;
		el_row = row;
		border_bottom = 0;
		border_top = 0;
		top = 0;
		bottom = 0;
	}

	table_row(const table_row& val)
	{
		top = val.top;
		bottom = val.bottom;
		border_bottom = val.border_bottom;
		border_top = val.border_top;
		height = val.height;
		el_row = val.el_row;
	}
};

struct table_column
{
	int min_width;
	int max_width;
	int width;
	css_length css_width;
	int border_left;
	int border_right;
	int left;
	int right;

	table_column()
	{
		left = 0;
		right = 0;
		border_left = 0;
		border_right = 0;
		min_width = 0;
		max_width = 0;
		width = 0;
		css_width.predef(0);
	}

	table_column(const int min_w, const int max_w)
	{
		left = 0;
		right = 0;
		border_left = 0;
		border_right = 0;
		max_width = max_w;
		min_width = min_w;
		width = 0;
		css_width.predef(0);
	}

	table_column(const table_column& val)
	{
		left = val.left;
		right = val.right;
		border_left = val.border_left;
		border_right = val.border_right;
		max_width = val.max_width;
		min_width = val.min_width;
		width = val.width;
		css_width = val.css_width;
	}
};


class table_column_accessor_max_width
{
public:
	int& get(table_column& col);
};

class table_column_accessor_min_width
{
public:
	int& get(table_column& col);
};

class table_column_accessor_width
{
public:
	int& get(table_column& col);
};


struct table_cell
{
	element* el;
	int colspan;
	int rowspan;
	int min_width;
	int min_height;
	int max_width;
	int max_height;
	int width;
	int height;

	margins borders;

	table_cell()
	{
		min_width = 0;
		min_height = 0;
		max_width = 0;
		max_height = 0;
		width = 0;
		height = 0;
		colspan = 1;
		rowspan = 1;
		el = nullptr;
	}

	table_cell(const table_cell& val)
	{
		el = val.el;
		colspan = val.colspan;
		rowspan = val.rowspan;
		width = val.width;
		height = val.height;
		min_width = val.min_width;
		min_height = val.min_height;
		max_width = val.max_width;
		max_height = val.max_height;
		borders = val.borders;
	}
};

class table_grid
{
	int m_rows_count = 0;
	int m_cols_count = 0;

	std::vector<std::vector<table_cell>> m_cells;
	std::vector<table_column> m_columns;
	std::vector<table_row> m_rows;

public:
	table_grid()
	{
	}

	void clear();
	void begin_row(element* row);
	void add_cell(element* el);
	bool is_rowspanned(int r, int c);
	void finish();

	table_cell* cell(int t_col, int t_row);
	table_column& column(const int c) { return m_columns[c]; }
	table_row& row(const int r) { return m_rows[r]; }

	int rows_count() { return m_rows_count; }
	int cols_count() { return m_cols_count; }

	void distribute_max_width(int width, int start, int end);
	void distribute_min_width(int width, int start, int end);
	void distribute_width(int width, int start, int end);
	int calc_table_width(int block_width, bool is_auto);
	void calc_horizontal_positions(const margins& table_borders, border_collapse bc, int bdr_space_x);
	void calc_vertical_positions(const margins& table_borders, border_collapse bc, int bdr_space_y);

	template <class table_column_accessor>
	void distribute_width(const int width, const int start, const int end, table_column_accessor& acc)
	{
		if (!(start >= 0 && start < m_cols_count && end >= 0 && end < m_cols_count) || start > end)
		{
			return;
		}

		int cols_width = 0;
		for (int col = start; col <= end; col++)
		{
			cols_width += m_columns[col].max_width;
		}

		int add = width / (end - start + 1);
		int added_width = 0;
		for (int col = start; col <= end; col++)
		{
			if (cols_width)
			{
				add = round_f(
					static_cast<float>(width) * (static_cast<float>(m_columns[col].max_width) / static_cast<float>(
						cols_width)));
			}
			added_width += add;
			acc.get(m_columns[col]) += add;
		}
		if (added_width < width)
		{
			acc.get(m_columns[start]) += width - added_width;
		}
	}
};


class box;
class background;
class render_win32;

enum element_type
{
	el_html,
	el_anchor,
	el_base,
	el_before,
	el_after,
	el_body,
	el_break,
	el_cdata,
	el_comment,
	el_div,
	el_font,
	el_image,
	el_link,
	el_para,
	el_script,
	el_space,
	el_style,
	el_svg,
	el_table,
	el_td,
	el_text,
	el_title,
	el_tr
};


class element
{
	friend class box;
	friend class el_table;
	friend class element;
	friend class table_grid;

protected:
	enum element_type m_type;
	element* m_parent;
	document& m_doc;

	position m_pos;
	margins m_margins;
	margins m_padding;
	margins m_borders;
	bool m_skip;
	bool m_loaded;
	std::vector<std::unique_ptr<element>> m_children;

	std::string m_id;
	std::string m_class;
	std::string m_text;
	std::string m_transformed_text;
	size m_size;
	text_transform m_text_transform;
	bool m_use_transformed;
	bool m_draw_spaces;
	std::string m_src;
	std::string m_tag;
	style m_style;
	std::map<std::string, std::string, ltstr> m_attrs;
	vertical_align m_vertical_align;
	text_align m_text_align;
	style_display m_display;
	list_style_type m_list_style_type;
	list_style_position m_list_style_position;
	white_space m_white_space;
	element_float m_float;
	element_clear m_clear;
	std::vector<floated_box> m_floats_left;
	std::vector<floated_box> m_floats_right;
	std::vector<element*> m_positioned;
	background m_bg;
	element_position m_el_position;
	int m_line_height;
	bool m_lh_predefined;
	std::vector<std::string> m_pseudo_classes;
	std::vector<used_selector> m_used_styles;

	box* m_box;
	std::vector<std::unique_ptr<box>> m_boxes;

	pf::font_handle m_font;
	int m_font_size;
	font_metrics m_font_metrics;

	css_margins m_css_margins;
	css_margins m_css_padding;
	css_borders m_css_borders;
	css_length m_css_width;
	css_length m_css_height;
	css_length m_css_min_width;
	css_length m_css_min_height;
	css_length m_css_max_width;
	css_length m_css_max_height;
	css_offsets m_css_offsets;
	css_length m_css_text_indent;

	overflow m_overflow;
	visibility m_visibility;
	int m_z_index;
	box_sizing m_box_sizing;

	int_int_cache m_cache_line_left;
	int_int_cache m_cache_line_right;

	// flexbox
	flex_direction m_flex_direction;
	flex_wrap m_flex_wrap;
	flex_justify_content m_flex_justify_content;
	flex_align_items m_flex_align_items;
	float m_flex_grow;
	float m_flex_shrink;
	css_length m_flex_basis;
	flex_align_items m_flex_align_self;
	int m_flex_gap;

	// table
	table_grid m_grid;
	css_length m_css_border_spacing_x;
	css_length m_css_border_spacing_y;
	int m_border_spacing_x;
	int m_border_spacing_y;
	border_collapse m_border_collapse;

public:
	element(document& doc, enum element_type, std::string text = empty);
	~element();

	bool collapse_bottom_margin() const;
	bool collapse_top_margin() const;
	bool in_normal_flow() const;
	bool is_inline_box() const;
	bool is_positioned() const;
	bool is_visible() const;
	bool skip();
	element* parent() const { return m_parent; }

	int border_bottom() const;
	int border_left() const;
	int border_right() const;
	int border_top() const;
	int bottom() const;
	int calc_width(int defVal) const;
	int content_margins_bottom() const;
	int content_margins_left() const;
	int content_margins_right() const;
	int content_margins_top() const;
	int finish_last_box(bool end_of_render = false);
	int get_cleared_top(const element* el, int line_top);
	int get_inline_shift_left();
	int get_inline_shift_right();
	int height() const;
	int left() const;
	int margin_bottom() const;
	int margin_left() const;
	int margin_right() const;
	int margin_top() const;
	int new_box(const element* el, int max_width);
	int padding_bottom() const;
	int padding_left() const;
	int padding_right() const;
	int padding_top() const;
	int right() const;
	int top() const;
	int width() const;
	margins get_borders() const;
	margins get_margins() const;
	margins get_paddings() const;
	position get_placement() const;
	pf::font_handle get_font(font_metrics* fm = nullptr);
	background* get_background(bool own_only = false);

	bool append_child(std::unique_ptr<element> el);
	bool append_space(const std::string& val);
	bool append_text(const std::string& val);

	bool fetch_positioned();
	bool find_styles_changes(position::vector& redraw_boxes, int x, int y);
	bool get_predefined_height(int& p_height) const;
	bool have_inline_child();
	bool is_ancestor(const element* el);
	bool is_body() const { return m_type == el_body; };
	bool is_break() const;
	bool is_first_child_inline(const element* el);
	bool is_floats_holder() const;
	bool is_last_child_inline(const element* el);
	bool is_nth_child(const element* el, int num, int off, bool of_type);
	bool is_nth_last_child(const element* el, int num, int off, bool of_type);
	bool is_only_child(const element* el, bool of_type);
	bool is_point_inside(int x, int y);
	bool is_replaced() const;
	bool is_white_space();
	bool on_lbutton_down();
	bool on_lbutton_up();
	bool on_mouse_leave();
	bool on_mouse_over();
	bool set_pseudo_class(const std::string& pclass, bool add);
	const std::string& get_tag_name() const { return m_tag; }
	const std::string get_attr(const std::string& name, const std::string& def = empty) const;
	const std::string get_cursor() const;
	const std::string get_style_property(const std::string& name, bool inherited,
	                                     const std::string& def = empty) const;
	const std::string get_text() const;
	css_length get_css_bottom() const;
	css_length get_css_height() const;
	css_length get_css_left() const;
	css_length get_css_right() const;
	css_length get_css_top() const;
	css_length get_css_width() const;
	css_offsets get_css_offsets() const;
	element_clear get_clear() const;
	element_float get_float() const;
	element_position get_element_position(css_offsets* offsets = nullptr) const;
	int find_next_line_top(int top, int width, int def_right);
	int get_base_line() const;
	int get_floats_height(element_float el_float = float_none) const;
	int get_font_size() const;
	int get_left_floats_height() const;
	int get_line_left(int y);
	int get_line_right(int y, int def_right);
	int get_right_floats_height() const;
	int get_zindex() const;
	int line_height() const;
	int place_element(render_win32& renderer, element* el, int max_width);
	int render(render_win32& renderer, int x, int y, int max_width, bool second_pass = false);
	int render_inline(render_win32& renderer, element* container, int max_width);
	int select(const css_element_selector& selector, bool apply_pseudo = true);
	int select(const css_selector& selector, bool apply_pseudo = true);
	overflow get_overflow() const;
	size_t get_children_count() const;
	element* find_adjacent_sibling(const element* el, const css_selector& selector, bool apply_pseudo = true,
	                               bool* is_pseudo = nullptr);
	element* find_ancestor(const css_selector& selector, bool apply_pseudo = true, bool* is_pseudo = nullptr);
	element* find_sibling(const element* el, const css_selector& selector, bool apply_pseudo = true,
	                      bool* is_pseudo = nullptr);
	element* get_child(int idx) const;
	element* get_child_by_point(int x, int y, int client_x, int client_y, draw_flag flag, int zindex);
	element* get_element_by_point(int x, int y, int client_x, int client_y);
	style_display get_display() const;
	vertical_align get_vertical_align() const;
	visibility get_visibility() const;
	void add_float(element* el, int x, int y);
	void add_positioned(element* el);
	void add_style(const std::shared_ptr<style>& st);
	void apply_stylesheet(const css& styles);
	void apply_vertical_align();
	void calc_document_size(size& sz, int x = 0, int y = 0);
	void calc_outlines(int parent_width);
	void draw(render_win32& renderer, int x, int y, const position* clip);
	void draw_background(render_win32& renderer, int x, int y, const position* clip);
	void draw_children(render_win32& renderer, int x, int y, const position* clip, draw_flag flag, int zindex);
	void draw_stacking_context(render_win32& renderer, int x, int y, const position* clip, bool with_positioned);
	void get_content_size(render_win32& renderer, size& sz, int max_width);
	void get_inline_boxes(position::vector& boxes);
	void get_line_left_right(int y, int def_right, int& ln_left, int& ln_right);
	void get_redraw_box(position& pos, int x = 0, int y = 0);
	void init();
	void init_font();
	void on_click();
	void parse_attributes();
	void parse_styles(bool is_reparse = false);
	void refresh_styles();
	void render_positioned(render_win32& renderer, render_type rt = render_all);
	void set_attr(const std::string& name, const std::string& val);
	void set_css_width(const css_length& w);
	void set_data(const std::string& data);
	void set_tag_name(const std::string& tag);
	void update_floats(int dy, element* parent);
	white_space get_white_space() const;
	void parent(element* par);
	void skip(bool val);
	web_color get_color(const char* prop_name, bool inherited, const web_color& def_color = web_color());

protected:
	int fix_line_width(render_win32& renderer, int max_width, element_float flt);
	void parse_background();
	void init_background_paint(render_win32& renderer, position pos, background_paint& bg_paint, const background* bg);
	void draw_list_marker(render_win32& renderer, const position& pos);
	void parse_nth_child_params(const std::string& param, int& num, int& off);
	void remove_before_after();
	void add_text(const std::string& txt);
	void add_function(const std::string& fnc, const std::string& params);

	char convert_escape(const char* txt);

	std::string resolve_custom_property(const std::string& name) const;

	element* get_element_before();
	element* get_element_after();
};


class element_zindex_sort
{
public:
	bool operator()(const element* lhs, const element* rhs) const
	{
		return lhs->get_zindex() < rhs->get_zindex();
	}
};

inline int element::right() const
{
	return left() + width();
}

inline int element::left() const
{
	return m_pos.left() - margin_left() - m_padding.left - m_borders.left;
}

inline int element::top() const
{
	return m_pos.top() - margin_top() - m_padding.top - m_borders.top;
}

inline int element::bottom() const
{
	return top() + height();
}

inline int element::height() const
{
	return m_pos.height + margin_top() + margin_bottom() + m_padding.height() + m_borders.height();
}

inline int element::width() const
{
	return m_pos.width + margin_left() + margin_right() + m_padding.width() + m_borders.width();
}

inline int element::content_margins_top() const
{
	return margin_top() + m_padding.top + m_borders.top;
}

inline int element::content_margins_bottom() const
{
	return margin_bottom() + m_padding.bottom + m_borders.bottom;
}

inline int element::content_margins_left() const
{
	return margin_left() + m_padding.left + m_borders.left;
}

inline int element::content_margins_right() const
{
	return margin_right() + m_padding.right + m_borders.right;
}

inline margins element::get_paddings() const
{
	return m_padding;
}

inline margins element::get_borders() const
{
	return m_borders;
}

inline int element::padding_top() const
{
	return m_padding.top;
}

inline int element::padding_bottom() const
{
	return m_padding.bottom;
}

inline int element::padding_left() const
{
	return m_padding.left;
}

inline int element::padding_right() const
{
	return m_padding.right;
}

inline bool element::in_normal_flow() const
{
	if (get_element_position() != element_position_absolute && get_display() != display_none)
	{
		return true;
	}
	return false;
}

inline int element::border_top() const
{
	return m_borders.top;
}

inline int element::border_bottom() const
{
	return m_borders.bottom;
}

inline int element::border_left() const
{
	return m_borders.left;
}

inline int element::border_right() const
{
	return m_borders.right;
}

inline bool element::skip()
{
	return m_skip;
}

inline void element::skip(const bool val)
{
	m_skip = val;
}

inline void element::parent(element* par)
{
	m_parent = par;
}

inline int element::margin_top() const
{
	return m_margins.top;
}

inline int element::margin_bottom() const
{
	return m_margins.bottom;
}

inline int element::margin_left() const
{
	return m_margins.left;
}

inline int element::margin_right() const
{
	return m_margins.right;
}

inline margins element::get_margins() const
{
	margins ret;
	ret.left = margin_left();
	ret.right = margin_right();
	ret.top = margin_top();
	ret.bottom = margin_bottom();

	return ret;
}

inline bool element::is_positioned() const
{
	return get_element_position() > element_position_static;
}

inline bool element::is_visible() const
{
	return !(m_skip || get_display() == display_none || get_visibility() != visibility_visible);
}


class element;


template <class t_go_inside, class t_select>
class elements_iterator
{
	struct stack_item
	{
		int idx;
		element* el;
	};

	std::vector<stack_item> m_stack;

	element* m_el;
	int m_idx;

	t_go_inside& m_go_inside;
	t_select& m_select;

public:
	elements_iterator(element* el, t_go_inside& go_inside, t_select& select) :
		m_el(el),
		m_idx(-1),
		m_go_inside(go_inside),
		m_select(select)
	{
	}

	element* next(const bool ret_parent = true)
	{
		next_idx();

		while (m_idx < static_cast<int>(m_el->get_children_count()))
		{
			auto el = m_el->get_child(m_idx);

			if (el->get_children_count() && m_go_inside.select(el))
			{
				stack_item si;
				si.idx = m_idx;
				si.el = m_el;
				m_stack.push_back(si);
				m_el = el;
				m_idx = -1;
				if (ret_parent)
				{
					return el;
				}
				next_idx();
			}
			else if (m_select.select(m_el->get_child(m_idx)))
			{
				return m_el->get_child(m_idx);
			}
			else
			{
				next_idx();
			}
		}

		return nullptr;
	}

private:
	void next_idx()
	{
		m_idx++;

		while (m_idx >= static_cast<int>(m_el->get_children_count()) && m_stack.size())
		{
			auto si = m_stack.back();
			m_stack.pop_back();

			m_idx = si.idx;
			m_el = si.el;
			m_idx++;
		}
	}
};

class go_inside_inline
{
public:
	bool select(const element* el)
	{
		return el->get_display() == display_inline || el->get_display() == display_inline_text;
	}
};

class go_inside_table
{
public:
	bool select(const element* el)
	{
		return el->get_display() == display_table_row_group ||
			el->get_display() == display_table_header_group ||
			el->get_display() == display_table_footer_group;
	}
};

class table_rows_selector
{
public:
	bool select(const element* el)
	{
		return el->get_display() == display_table_row;
	}
};

class table_cells_selector
{
public:
	bool select(const element* el)
	{
		return el->get_display() == display_table_cell;
	}
};
