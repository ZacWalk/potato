// style.cpp - CSS shorthand parsing, media query evaluation, selector matching,
// at-rule handling, stylesheet loading, and GDI+ rendering of text, borders,
// backgrounds, and images.

#include "pch.h"
#include "document.h"


void style::parse(const std::string& txt, const std::string& baseurl)
{
	const auto declarations = split_string(txt, ';');

	for (const auto& decl : declarations)
	{
		parse_property(decl, baseurl);
	}
}

void style::parse_property(const std::string& txt, const std::string& baseurl)
{
	const auto pos = txt.find_first_of(':');

	if (pos != std::string::npos)
	{
		const auto name = txt.substr(0, pos);
		auto val = txt.substr(pos + 1);

		trim(val);

		parse_property(trim_lower(name), val, baseurl);
	}
}

void style::parse_property(const std::string& name, const std::string& val, const std::string& baseurl)
{
	if (!name.empty() && !val.empty())
	{
		const auto pos = val.find_first_of('!');

		if (pos == std::string::npos)
		{
			add_property(name, val, baseurl, false);
		}
		else
		{
			auto left = val.substr(0, pos);
			const auto right = val.substr(pos + 1);

			trim(left);

			add_property(name, left, baseurl, is_equal(right, "important"));
		}
	}
}

void style::combine(const style& src)
{
	for (const auto& prop : src.m_properties)
	{
		add_parsed_property(prop.first, prop.second.m_value, prop.second.m_important);
	}
}

void style::add_property(const std::string& name, const std::string& val, const std::string& baseurl,
                         const bool important)
{
	if (name.empty() || val.empty())
	{
		return;
	}

	// Add baseurl for background image 
	if (name == "background-image")
	{
		add_parsed_property(name, val, important);

		if (!baseurl.empty())
		{
			add_parsed_property("background-image-baseurl", baseurl, important);
		}
	}
	// Parse border spacing properties 
	else if (name == "border-spacing")
	{
		const auto tokens = split_string(val);

		if (tokens.size() == 1)
		{
			add_property("-potato-border-spacing-x", tokens[0], baseurl, important);
			add_property("-potato-border-spacing-y", tokens[0], baseurl, important);
		}
		else if (tokens.size() == 2)
		{
			add_property("-potato-border-spacing-x", tokens[0], baseurl, important);
			add_property("-potato-border-spacing-y", tokens[1], baseurl, important);
		}
	}
	// Parse borders shorthand properties 
	else if (name == "border")
	{
		const auto tokens = split_string(val, " ", "(");

		for (const auto& tok : tokens)
		{
			const auto idx = value_index(tok, border_style_strings, -1);

			if (idx >= 0)
			{
				add_property("border-left-style", tok, baseurl, important);
				add_property("border-right-style", tok, baseurl, important);
				add_property("border-top-style", tok, baseurl, important);
				add_property("border-bottom-style", tok, baseurl, important);
			}
			else
			{
				if (web_color::is_color(tok))
				{
					add_property("border-left-color", tok, baseurl, important);
					add_property("border-right-color", tok, baseurl, important);
					add_property("border-top-color", tok, baseurl, important);
					add_property("border-bottom-color", tok, baseurl, important);
				}
				else
				{
					add_property("border-left-width", tok, baseurl, important);
					add_property("border-right-width", tok, baseurl, important);
					add_property("border-top-width", tok, baseurl, important);
					add_property("border-bottom-width", tok, baseurl, important);
				}
			}
		}
	}
	// Parse border radius shorthand properties 
	else if (name == "border-bottom-left-radius" ||
		name == "border-bottom-right-radius" ||
		name == "border-top-right-radius" ||
		name == "border-top-left-radius")
	{
		const auto tokens = split_string(val);
		add_property(name + "-x", tokens[0], baseurl, important);
		add_property(name + "-y", tokens.size() >= 2 ? tokens[1] : tokens[0], baseurl, important);
	}
	// Parse border-radius shorthand properties 
	else if (name == "border-radius")
	{
		const auto tokens = split_string(val, '/');
		if (tokens.size() == 1)
		{
			add_property("border-radius-x", tokens[0], baseurl, important);
			add_property("border-radius-y", tokens[0], baseurl, important);
		}
		else if (tokens.size() >= 2)
		{
			add_property("border-radius-x", tokens[0], baseurl, important);
			add_property("border-radius-y", tokens[1], baseurl, important);
		}
	}
	else if (name == "border-radius-x" || name == "border-radius-y")
	{
		const auto tokens = split_string(val);
		const auto axis = name.substr(name.size() - 1); // "x" or "y"
		const char* corners[] = {
			"border-top-left-radius-", "border-top-right-radius-", "border-bottom-right-radius-",
			"border-bottom-left-radius-"
		};
		// CSS box model: 1 value = all, 2 = top-left/bottom-right + top-right/bottom-left, 3 = top-left + top-right/bottom-left + bottom-right, 4 = each
		size_t idx[] = {0, 0, 0, 0};
		if (tokens.size() == 1) { idx[0] = idx[1] = idx[2] = idx[3] = 0; }
		else if (tokens.size() == 2)
		{
			idx[0] = 0;
			idx[1] = 1;
			idx[2] = 0;
			idx[3] = 1;
		}
		else if (tokens.size() == 3)
		{
			idx[0] = 0;
			idx[1] = 1;
			idx[2] = 2;
			idx[3] = 1;
		}
		else if (tokens.size() >= 4)
		{
			idx[0] = 0;
			idx[1] = 1;
			idx[2] = 2;
			idx[3] = 3;
		}
		for (int i = 0; i < 4; i++)
		{
			add_property(std::string(corners[i]) + axis, tokens[idx[i]], baseurl, important);
		}
	}
	// Parse list-style shorthand properties 
	else if (name == "list-style")
	{
		add_parsed_property("list-style-type", "disc", important);
		add_parsed_property("list-style-position", "outside", important);
		add_parsed_property("list-style-image", empty, important);
		add_parsed_property("list-style-image-baseurl", empty, important);

		const auto tokens = split_string(val, " ", "(");

		for (const auto& tok : tokens)
		{
			int idx = value_index(tok, list_style_type_strings, -1);
			if (idx >= 0)
			{
				add_parsed_property("list-style-type", tok, important);
			}
			else
			{
				idx = value_index(tok, list_style_position_strings, -1);
				if (idx >= 0)
				{
					add_parsed_property("list-style-position", tok, important);
				}
				else if (starts(val, "url"))
				{
					add_parsed_property("list-style-image", tok, important);
					if (!baseurl.empty())
					{
						add_parsed_property("list-style-image-baseurl", baseurl, important);
					}
				}
			}
		}
	}
	// Add baseurl for background image 
	else if (name == "list-style-image")
	{
		add_parsed_property(name, val, important);
		if (!baseurl.empty())
		{
			add_parsed_property("list-style-image-baseurl", baseurl, important);
		}
	}
	// Parse background shorthand properties 
	else if (name == "background")
	{
		parse_short_background(val, baseurl, important);
	}
	// Parse margin and padding shorthand properties 
	else if (name == "margin" || name == "padding")
	{
		const auto tokens = split_string(val);
		if (tokens.size() >= 4)
		{
			add_parsed_property(name + "-top", tokens[0], important);
			add_parsed_property(name + "-right", tokens[1], important);
			add_parsed_property(name + "-bottom", tokens[2], important);
			add_parsed_property(name + "-left", tokens[3], important);
		}
		else if (tokens.size() == 3)
		{
			add_parsed_property(name + "-top", tokens[0], important);
			add_parsed_property(name + "-right", tokens[1], important);
			add_parsed_property(name + "-left", tokens[1], important);
			add_parsed_property(name + "-bottom", tokens[2], important);
		}
		else if (tokens.size() == 2)
		{
			add_parsed_property(name + "-top", tokens[0], important);
			add_parsed_property(name + "-bottom", tokens[0], important);
			add_parsed_property(name + "-right", tokens[1], important);
			add_parsed_property(name + "-left", tokens[1], important);
		}
		else if (tokens.size() == 1)
		{
			add_parsed_property(name + "-top", tokens[0], important);
			add_parsed_property(name + "-bottom", tokens[0], important);
			add_parsed_property(name + "-right", tokens[0], important);
			add_parsed_property(name + "-left", tokens[0], important);
		}
	}
	// Parse border-* shorthand properties 
	else if (name == "border-left" ||
		name == "border-right" ||
		name == "border-top" ||
		name == "border-bottom")
	{
		parse_short_border(name, val, important);
	}
	// Parse border-width/style/color shorthand properties 
	else if (name == "border-width")
	{
		parse_border_style("width", val, important);
	}
	else if (name == "border-style")
	{
		parse_border_style("style", val, important);
	}
	else if (name == "border-color")
	{
		parse_border_style("color", val, important);
	}
	// Parse font shorthand properties 
	else if (name == "font")
	{
		parse_short_font(val, important);
	}
	// Handle text-decoration as shorthand
	else if (name == "text-decoration")
	{
		add_parsed_property("text-decoration", val, important);
	}
	// Handle overflow shorthand (overflow-x and overflow-y)
	else if (name == "overflow")
	{
		const auto tokens = split_string(val);
		if (tokens.size() == 1)
		{
			add_parsed_property(name, tokens[0], important);
		}
		else if (tokens.size() >= 2)
		{
			add_parsed_property(name, tokens[0], important);
		}
	}
	// Flex container properties
	else if (name == "flex-direction" || name == "flex-wrap" ||
		name == "align-items" || name == "align-self" || name == "justify-content")
	{
		add_parsed_property(name, val, important);
	}
	// Flex item properties
	else if (name == "flex-grow" || name == "flex-shrink" || name == "flex-basis")
	{
		add_parsed_property(name, val, important);
	}
	// Flex shorthand
	else if (name == "flex")
	{
		const auto tokens = split_string(val);
		if (val == "none")
		{
			add_parsed_property("flex-grow", "0", important);
			add_parsed_property("flex-shrink", "0", important);
			add_parsed_property("flex-basis", "auto", important);
		}
		else if (val == "auto")
		{
			add_parsed_property("flex-grow", "1", important);
			add_parsed_property("flex-shrink", "1", important);
			add_parsed_property("flex-basis", "auto", important);
		}
		else if (tokens.size() == 1)
		{
			add_parsed_property("flex-grow", tokens[0], important);
			add_parsed_property("flex-shrink", "1", important);
			add_parsed_property("flex-basis", "0", important);
		}
		else if (tokens.size() == 2)
		{
			add_parsed_property("flex-grow", tokens[0], important);
			add_parsed_property("flex-shrink", tokens[1], important);
			add_parsed_property("flex-basis", "0", important);
		}
		else if (tokens.size() >= 3)
		{
			add_parsed_property("flex-grow", tokens[0], important);
			add_parsed_property("flex-shrink", tokens[1], important);
			add_parsed_property("flex-basis", tokens[2], important);
		}
	}
	// Flex-flow shorthand
	else if (name == "flex-flow")
	{
		const auto tokens = split_string(val);
		for (const auto& tok : tokens)
		{
			if (value_index(tok, flex_direction_strings, -1) >= 0)
				add_parsed_property("flex-direction", tok, important);
			else if (value_index(tok, flex_wrap_strings, -1) >= 0)
				add_parsed_property("flex-wrap", tok, important);
		}
	}
	// Gap properties
	else if (name == "gap" || name == "row-gap" || name == "column-gap")
	{
		add_parsed_property(name, val, important);
	}
	// Silently ignore grid properties we don't support yet
	else if (name == "grid-template-columns" || name == "grid-template-rows" ||
		name == "grid-column" || name == "grid-row" || name == "grid-area" ||
		name == "grid-template-areas" || name == "grid-auto-columns" || name == "grid-auto-rows" ||
		name == "grid-auto-flow" || name == "grid-template")
	{
		// Silently ignore grid properties
	}
	else
	{
		add_parsed_property(name, val, important);
	}
}

void style::parse_short_border(const std::string& key, const std::string& val, const bool important)
{
	const auto tokens = split_string(val, " ", "(");

	if (tokens.size() >= 3)
	{
		add_parsed_property(key + "-width", tokens[0], important);
		add_parsed_property(key + "-style", tokens[1], important);
		add_parsed_property(key + "-color", tokens[2], important);
	}
	else if (tokens.size() == 2)
	{
		if (isdigit(tokens[0][0]) || value_index(val, border_width_strings) >= 0)
		{
			add_parsed_property(key + "-width", tokens[0], important);
			add_parsed_property(key + "-style", tokens[1], important);
		}
		else
		{
			add_parsed_property(key + "-style", tokens[0], important);
			add_parsed_property(key + "-color", tokens[1], important);
		}
	}
}

void style::parse_border_style(const char* style, const std::string& val, const bool important)
{
	const std::string key("border");

	const auto tokens = split_string(val);

	if (tokens.size() >= 4)
	{
		add_parsed_property(key + "-top-" + style, tokens[0], important);
		add_parsed_property(key + "-right-" + style, tokens[1], important);
		add_parsed_property(key + "-bottom-" + style, tokens[2], important);
		add_parsed_property(key + "-left-" + style, tokens[3], important);
	}
	else if (tokens.size() == 3)
	{
		add_parsed_property(key + "-top-" + style, tokens[0], important);
		add_parsed_property(key + "-right-" + style, tokens[1], important);
		add_parsed_property(key + "-left-" + style, tokens[1], important);
		add_parsed_property(key + "-bottom-" + style, tokens[2], important);
	}
	else if (tokens.size() == 2)
	{
		add_parsed_property(key + "-top-" + style, tokens[0], important);
		add_parsed_property(key + "-bottom-" + style, tokens[0], important);
		add_parsed_property(key + "-right-" + style, tokens[1], important);
		add_parsed_property(key + "-left-" + style, tokens[1], important);
	}
	else if (tokens.size() == 1)
	{
		add_parsed_property(key + "-top-" + style, tokens[0], important);
		add_parsed_property(key + "-bottom-" + style, tokens[0], important);
		add_parsed_property(key + "-right-" + style, tokens[0], important);
		add_parsed_property(key + "-left-" + style, tokens[0], important);
	}
}

void style::parse_short_background(const std::string& val, const std::string& baseurl, const bool important)
{
	add_parsed_property("background-color", "transparent", important);
	add_parsed_property("background-image", empty, important);
	add_parsed_property("background-image-baseurl", empty, important);
	add_parsed_property("background-repeat", "repeat", important);
	add_parsed_property("background-origin", "padding-box", important);
	add_parsed_property("background-clip", "border-box", important);
	add_parsed_property("background-attachment", "scroll", important);

	if (val == "none")
	{
		return;
	}

	const auto tokens = split_string(val, " ", "(");
	auto origin_found = false;

	for (const auto& tok : tokens)
	{
		if (web_color::is_color(tok))
		{
			add_parsed_property("background-color", tok, important);
		}
		else if (tok.substr(0, 3) == "url")
		{
			add_parsed_property("background-image", tok, important);
			if (!baseurl.empty())
			{
				add_parsed_property("background-image-baseurl", baseurl, important);
			}
		}
		else if (value_in_list(tok, background_repeat_strings))
		{
			add_parsed_property("background-repeat", tok, important);
		}
		else if (value_in_list(tok, background_attachment_strings))
		{
			add_parsed_property("background-attachment", tok, important);
		}
		else if (value_in_list(tok, background_box_strings))
		{
			if (!origin_found)
			{
				add_parsed_property("background-origin", tok, important);
				origin_found = true;
			}
			else
			{
				add_parsed_property("background-clip", tok, important);
			}
		}
		else if (value_in_list(tok, "left;right;top;bottom;center") ||
			isdigit(tok[0]) ||
			tok[0] == '-' ||
			tok[0] == '.' ||
			tok[0] == '+')
		{
			if (m_properties.contains("background-position"))
			{
				m_properties["background-position"].m_value = m_properties["background-position"].m_value +
					" " + tok;
			}
			else
			{
				add_parsed_property("background-position", tok, important);
			}
		}
	}
}

void style::parse_short_font(const std::string& val, const bool important)
{
	add_parsed_property("font-style", "normal", important);
	add_parsed_property("font-variant", "normal", important);
	add_parsed_property("font-weight", "normal", important);
	add_parsed_property("font-size", "medium", important);
	add_parsed_property("line-height", "normal", important);

	const auto tokens = split_string(val, " ", "\"");

	int idx = 0;
	constexpr bool was_normal = false;
	bool is_family = false;
	std::string font_family;

	for (const auto& tok : tokens)
	{
		idx = value_index(tok, font_style_strings);

		if (!is_family)
		{
			if (idx >= 0)
			{
				if (idx == 0)
				{
					add_parsed_property("font-weight", tok, important);
					add_parsed_property("font-variant", tok, important);
					add_parsed_property("font-style", tok, important);
				}
				else
				{
					add_parsed_property("font-style", tok, important);
				}
			}
			else
			{
				if (value_in_list(tok, font_weight_strings))
				{
					add_parsed_property("font-weight", tok, important);
				}
				else
				{
					if (value_in_list(tok, font_variant_strings))
					{
						add_parsed_property("font-variant", tok, important);
					}
					else if (isdigit(tok[0]))
					{
						auto szlh = split_string(tok, '/');

						if (szlh.size() == 1)
						{
							add_parsed_property("font-size", szlh[0], important);
						}
						else if (szlh.size() >= 2)
						{
							add_parsed_property("font-size", szlh[0], important);
							add_parsed_property("line-height", szlh[1], important);
						}
					}
					else
					{
						is_family = true;
						font_family += tok;
					}
				}
			}
		}
		else
		{
			font_family += tok;
		}
	}

	add_parsed_property("font-family", font_family, important);
}

void style::add_parsed_property(const std::string& name, const std::string& val_in, const bool important)
{
	auto val = val_in;

	// unquot 
	if (val.size() > 1 && val.front() == val.back() && val.front() == '"')
	{
		val.erase(0, 1);
		val.pop_back();
	}

	const auto found = m_properties.find(name);

	if (found != m_properties.end())
	{
		if (!found->second.m_important || (important && found->second.m_important))
		{
			found->second.m_value = val;
			found->second.m_important = important;
		}
	}
	else
	{
		m_properties[name] = property_value(val, important);
	}
}

void style::remove_property(const std::string& name, const bool important)
{
	const auto found = m_properties.find(name);

	if (found != m_properties.end())
	{
		if (!found->second.m_important || (important && found->second.m_important))
		{
			m_properties.erase(found);
		}
	}
}


std::shared_ptr<media_query> media_query::create_from_string(const std::string& str)
{
	auto query = std::make_shared<media_query>();
	const auto tokens = split_string(str, " \t\r\n", "(");

	for (auto tok : tokens)
	{
		if (tok == "not")
		{
			query->m_not = true;
		}
		else if (tok.at(0) == '(')
		{
			tok.erase(0, 1);

			if (tok.at(tok.length() - 1) == ')')
			{
				tok.erase(tok.length() - 1, 1);
			}

			media_query_expression expr;
			auto expr_tokens = split_string(tok, ':');

			if (!expr_tokens.empty())
			{
				trim(expr_tokens[0]);
				expr.feature = static_cast<media_feature>(value_index(expr_tokens[0], media_feature_strings,
				                                                      media_feature_none));
				if (expr.feature != media_feature_none)
				{
					if (expr_tokens.size() == 1)
					{
						expr.check_as_bool = true;
					}
					else
					{
						trim(expr_tokens[1]);
						expr.check_as_bool = false;
						if (expr.feature == media_feature_orientation)
						{
							expr.val = value_index(expr_tokens[1], media_orientation_strings,
							                       media_orientation_landscape);
						}
						else
						{
							const auto slash_pos = expr_tokens[1].find_first_of('/');

							if (slash_pos != std::string::npos)
							{
								std::string val1 = expr_tokens[1].substr(0, slash_pos);
								std::string val2 = expr_tokens[1].substr(slash_pos + 1);
								trim(val1);
								trim(val2);
								expr.val = safe_stoi(val1);
								expr.val2 = safe_stoi(val2);
							}
							else
							{
								css_length length;
								length.fromString(expr_tokens[1]);
								if (length.units() == css_units_dpcm)
								{
									expr.val = static_cast<int>(length.val() * 2.54);
								}
								else if (length.units() == css_units_dpi)
								{
									expr.val = static_cast<int>(length.val() * 2.54);
								}
								else
								{
									document::cvt_units(length, document::get_default_font_size());
									expr.val = static_cast<int>(length.val());
								}
							}
						}
					}
					query->m_expressions.push_back(expr);
				}
			}
		}
		else
		{
			query->m_media_type = static_cast<media_type>(value_index(tok, media_type_strings, media_type_all));
		}
	}

	return query;
}

bool media_query::check(const media_features& features) const
{
	bool res = false;
	if (m_media_type == media_type_all || m_media_type == features.type)
	{
		res = true;
		for (auto expr = m_expressions.begin(); expr != m_expressions.end() && res; ++expr)
		{
			if (!expr->check(features))
			{
				res = false;
			}
		}
	}

	if (m_not)
	{
		res = !res;
	}

	return res;
}

//////////////////////////////////////////////////////////////////////////

std::shared_ptr<media_query_list> media_query_list::create_from_string(const std::string& str)
{
	auto list = std::make_shared<media_query_list>();
	const auto tokens = split_string(str, ',');

	for (auto tok : tokens)
	{
		auto query = media_query::create_from_string(trim_lower(tok));

		if (query)
		{
			list->m_queries.push_back(query);
		}
	}
	if (list->m_queries.empty())
	{
		list = nullptr;
	}

	return list;
}

bool media_query_list::apply_media_features(const media_features& features)
{
	bool apply = false;

	for (const auto& q : m_queries)
	{
		if (q->check(features))
		{
			apply = true;
		}
	}

	const bool ret = apply != m_is_used;
	m_is_used = apply;
	return ret;
}

bool media_query_expression::check(const media_features& features) const
{
	switch (feature)
	{
	case media_feature_width:
		{
			if (check_as_bool)
			{
				return features.width != 0;
			}
			if (features.width == val)
			{
				return true;
			}
		}
		break;
	case media_feature_min_width:
		if (features.width >= val)
		{
			return true;
		}
		break;
	case media_feature_max_width:
		if (features.width <= val)
		{
			return true;
		}
		break;
	case media_feature_height:
		{
			if (check_as_bool)
			{
				return features.height != 0;
			}
			if (features.height == val)
			{
				return true;
			}
		}
		break;
	case media_feature_min_height:
		if (features.height >= val)
		{
			return true;
		}
		break;
	case media_feature_max_height:
		if (features.height <= val)
		{
			return true;
		}
		break;

	case media_feature_device_width:
		{
			if (check_as_bool)
			{
				return features.device_width != 0;
			}
			if (features.device_width == val)
			{
				return true;
			}
		}
		break;
	case media_feature_min_device_width:
		if (features.device_width >= val)
		{
			return true;
		}
		break;
	case media_feature_max_device_width:
		if (features.device_width <= val)
		{
			return true;
		}
		break;
	case media_feature_device_height:
		{
			if (check_as_bool)
			{
				return features.device_height != 0;
			}
			if (features.device_height == val)
			{
				return true;
			}
		}
		break;
	case media_feature_min_device_height:
		if (features.device_height <= val)
		{
			return true;
		}
		break;
	case media_feature_max_device_height:
		if (features.device_height <= val)
		{
			return true;
		}
		break;

	case media_feature_orientation:
		if (features.height >= features.width)
		{
			if (val == media_orientation_portrait)
			{
				return true;
			}
		}
		else
		{
			if (val == media_orientation_landscape)
			{
				return true;
			}
		}
		break;
	case media_feature_aspect_ratio:
		if (features.height && val2)
		{
			const int ratio_this = round_d(static_cast<double>(val) / static_cast<double>(val2) * 100);
			const int ratio_feat = round_d(
				static_cast<double>(features.width) / static_cast<double>(features.height) * 100.0);
			if (ratio_this == ratio_feat)
			{
				return true;
			}
		}
		break;
	case media_feature_min_aspect_ratio:
		if (features.height && val2)
		{
			const int ratio_this = round_d(static_cast<double>(val) / static_cast<double>(val2) * 100);
			const int ratio_feat = round_d(
				static_cast<double>(features.width) / static_cast<double>(features.height) * 100.0);
			if (ratio_feat >= ratio_this)
			{
				return true;
			}
		}
		break;
	case media_feature_max_aspect_ratio:
		if (features.height && val2)
		{
			const int ratio_this = round_d(static_cast<double>(val) / static_cast<double>(val2) * 100);
			const int ratio_feat = round_d(
				static_cast<double>(features.width) / static_cast<double>(features.height) * 100.0);
			if (ratio_feat <= ratio_this)
			{
				return true;
			}
		}
		break;

	case media_feature_device_aspect_ratio:
		if (features.device_height && val2)
		{
			const int ratio_this = round_d(static_cast<double>(val) / static_cast<double>(val2) * 100);
			const int ratio_feat = round_d(
				static_cast<double>(features.device_width) / static_cast<double>(features.device_height) * 100.0);
			if (ratio_feat == ratio_this)
			{
				return true;
			}
		}
		break;
	case media_feature_min_device_aspect_ratio:
		if (features.device_height && val2)
		{
			const int ratio_this = round_d(static_cast<double>(val) / static_cast<double>(val2) * 100);
			const int ratio_feat = round_d(
				static_cast<double>(features.device_width) / static_cast<double>(features.device_height) * 100.0);
			if (ratio_feat >= ratio_this)
			{
				return true;
			}
		}
		break;
	case media_feature_max_device_aspect_ratio:
		if (features.device_height && val2)
		{
			const int ratio_this = round_d(static_cast<double>(val) / static_cast<double>(val2) * 100);
			const int ratio_feat = round_d(
				static_cast<double>(features.device_width) / static_cast<double>(features.device_height) * 100.0);
			if (ratio_feat <= ratio_this)
			{
				return true;
			}
		}
		break;

	case media_feature_color:
		{
			if (check_as_bool)
			{
				return features.color != 0;
			}
			if (features.color == val)
			{
				return true;
			}
		}
		break;
	case media_feature_min_color:
		if (features.color >= val)
		{
			return true;
		}
		break;
	case media_feature_max_color:
		if (features.color <= val)
		{
			return true;
		}
		break;

	case media_feature_color_index:
		{
			if (check_as_bool)
			{
				return features.color_index != 0;
			}
			if (features.color_index == val)
			{
				return true;
			}
		}
		break;
	case media_feature_min_color_index:
		if (features.color_index >= val)
		{
			return true;
		}
		break;
	case media_feature_max_color_index:
		if (features.color_index <= val)
		{
			return true;
		}
		break;

	case media_feature_monochrome:
		{
			if (check_as_bool)
			{
				return features.monochrome != 0;
			}
			if (features.monochrome == val)
			{
				return true;
			}
		}
		break;
	case media_feature_min_monochrome:
		if (features.monochrome >= val)
		{
			return true;
		}
		break;
	case media_feature_max_monochrome:
		if (features.monochrome <= val)
		{
			return true;
		}
		break;

	case media_feature_resolution:
		if (features.resolution == val)
		{
			return true;
		}
		break;
	case media_feature_min_resolution:
		if (features.resolution >= val)
		{
			return true;
		}
		break;
	case media_feature_max_resolution:
		if (features.resolution <= val)
		{
			return true;
		}
		break;
	default:
		return false;
	}

	return false;
}


void css_element_selector::parse(const std::string& text)
{
	auto el_end = text.find_first_of(".#[:");

	m_tag = trim_lower(text.substr(0, el_end));

	while (el_end != std::string::npos)
	{
		if (text[el_end] == '.')
		{
			css_attribute_selector attribute;

			auto pos = text.find_first_of(".#[:", el_end + 1);
			attribute.val = text.substr(el_end + 1, pos - el_end - 1);
			attribute.condition = select_equal;
			attribute.attribute = "class";
			m_attrs.push_back(attribute);
			el_end = pos;
		}
		else if (text[el_end] == ':')
		{
			css_attribute_selector attribute;

			if (text[el_end + 1] == ':')
			{
				auto pos = text.find_first_of(".#[:", el_end + 2);
				attribute.val = trim_lower(text.substr(el_end + 2, pos - el_end - 2));
				attribute.condition = select_pseudo_element;
				attribute.attribute = "pseudo-el";
				m_attrs.push_back(attribute);
				el_end = pos;
			}
			else
			{
				auto pos = text.find_first_of(".#[:(", el_end + 1);
				if (pos != std::string::npos && text.at(pos) == '(')
				{
					pos = find_close_bracket(text, pos);
					if (pos != std::string::npos)
					{
						pos++;
					}
				}
				if (pos != std::string::npos)
				{
					attribute.val = trim_lower(text.substr(el_end + 1, pos - el_end - 1));
				}
				else
				{
					attribute.val = trim_lower(text.substr(el_end + 1));
				}

				if (attribute.val == "after" || attribute.val == "before")
				{
					attribute.condition = select_pseudo_element;
				}
				else
				{
					attribute.condition = select_pseudo_class;
				}
				attribute.attribute = "pseudo";
				m_attrs.push_back(attribute);
				el_end = pos;
			}
		}
		else if (text[el_end] == '#')
		{
			css_attribute_selector attribute;

			auto pos = text.find_first_of(".#[:", el_end + 1);
			attribute.val = text.substr(el_end + 1, pos - el_end - 1);
			attribute.condition = select_equal;
			attribute.attribute = "id";
			m_attrs.push_back(attribute);
			el_end = pos;
		}
		else if (text[el_end] == '[')
		{
			css_attribute_selector attribute;

			auto pos = text.find_first_of("]~=|$*^", el_end + 1);
			std::string attr = trim_lower(text.substr(el_end + 1, pos - el_end - 1));

			if (pos != std::string::npos)
			{
				if (text[pos] == ']')
				{
					attribute.condition = select_exists;
				}
				else if (text[pos] == '=')
				{
					attribute.condition = select_equal;
					pos++;
				}
				else if (text.substr(pos, 2) == "~=")
				{
					attribute.condition = select_contain_str;
					pos += 2;
				}
				else if (text.substr(pos, 2) == "|=")
				{
					attribute.condition = select_start_str;
					pos += 2;
				}
				else if (text.substr(pos, 2) == "^=")
				{
					attribute.condition = select_start_str;
					pos += 2;
				}
				else if (text.substr(pos, 2) == "$=")
				{
					attribute.condition = select_end_str;
					pos += 2;
				}
				else if (text.substr(pos, 2) == "*=")
				{
					attribute.condition = select_contain_str;
					pos += 2;
				}
				else
				{
					attribute.condition = select_exists;
					pos += 1;
				}
				pos = text.find_first_not_of(" \t", pos);
				if (pos != std::string::npos)
				{
					if (text[pos] == '"')
					{
						auto pos2 = text.find_first_of("\"", pos + 1);
						attribute.val = text.substr(pos + 1, pos2 == std::string::npos ? pos2 : pos2 - pos - 1);
						pos = pos2 == std::string::npos ? pos2 : pos2 + 1;
					}
					else if (text[pos] == ']')
					{
						pos++;
					}
					else
					{
						auto pos2 = text.find_first_of("]", pos + 1);
						attribute.val = text.substr(pos, pos2 == std::string::npos ? pos2 : pos2 - pos);
						pos = pos2 == std::string::npos ? pos2 : pos2 + 1;
					}
				}
			}
			else
			{
				attribute.condition = select_exists;
			}
			attribute.attribute = attr;
			m_attrs.push_back(attribute);
			el_end = pos;
		}
		else
		{
			el_end++;
		}

		el_end = text.find_first_of(".#[:", el_end);
	}
}


bool css_selector::parse(const std::string& text)
{
	if (text.empty())
	{
		return false;
	}

	const auto split = text.find_last_of(" \t>+~");
	//tokenize(text, tokens, "", " \t>+~", "()");

	if (split == std::string::npos)
	{
		m_right.parse(trim_lower(text));
	}
	else
	{
		m_right.parse(trim_lower(text.substr(split + 1)));

		switch (text[split])
		{
		case '>':
			m_combinator = combinator_child;
			break;
		case '+':
			m_combinator = combinator_adjacent_sibling;
			break;
		case '~':
			m_combinator = combinator_general_sibling;
			break;
		default:
			m_combinator = combinator_descendant;
			break;
		}

		if (split > 0)
		{
			const auto left = trim_lower(text.substr(0, split));

			if (!left.empty())
			{
				m_left = std::make_shared<css_selector>(nullptr, std::shared_ptr<media_query_list>());

				if (!m_left->parse(left))
				{
					return false;
				}
			}
		}
	}

	return true;
}

void css_selector::calc_specificity()
{
	if (!m_right.m_tag.empty() && m_right.m_tag != "*")
	{
		m_specificity += 0x0001;
	}

	for (const auto& a : m_right.m_attrs)
	{
		if (a.attribute == "id")
		{
			m_specificity += 0x0100;
		}
		else if (a.attribute == "class")
		{
			auto tokens = split_string(a.val);
			m_specificity += static_cast<int>(tokens.size()) * 0x0010;
		}
		else if (a.condition == select_pseudo_element)
		{
			m_specificity += 0x0001;
		}
		else
		{
			m_specificity += 0x0010;
		}
	}

	if (m_left)
	{
		m_left->calc_specificity();
		m_specificity += m_left->m_specificity;
	}
}

void css_selector::add_media_to_doc(const std::shared_ptr<document>& doc) const
{
	if (m_media_query && doc)
	{
		doc->add_media_list(m_media_query);
	}
}

void css::parse_stylesheet(const std::string& str, const std::string& baseurl, document& doc,
                           std::shared_ptr<media_query_list>& media)
{
	std::string text = str;

	// remove comments
	auto c_start = text.find("/*");

	while (c_start != std::string::npos)
	{
		const auto c_end = text.find("*/", c_start + 2);
		text.erase(c_start, c_end - c_start + 2);
		c_start = text.find("/*");
	}

	auto pos = text.find_first_not_of(" \n\r\t");

	while (pos != std::string::npos)
	{
		while (pos != std::string::npos && text[pos] == '@')
		{
			const auto sPos = pos;
			pos = text.find_first_of('{', pos);

			if (pos != std::string::npos && text[pos] == '{')
			{
				pos = find_close_bracket(text, pos, '{', '}');
			}
			if (pos != std::string::npos)
			{
				parse_atrule(text.substr(sPos, pos - sPos + 1), baseurl, doc, media);
			}
			else
			{
				parse_atrule(text.substr(sPos), baseurl, doc, media);
			}

			if (pos != std::string::npos)
			{
				pos = text.find_first_not_of(" \n\r\t", pos + 1);
			}
		}

		if (pos == std::string::npos)
		{
			break;
		}

		const auto style_start = text.find_first_of("{", pos);
		const auto style_end = text.find_first_of("}", pos);

		if (style_start != std::string::npos && style_end != std::string::npos)
		{
			auto selectors = text.substr(pos, style_start - pos);
			auto styles = text.substr(style_start + 1, style_end - style_start - 1);

			auto st = std::make_shared<style>();
			st->add(styles, baseurl);

			parse_selectors(selectors, st, media);

			if (media)
			{
				doc.add_media_list(media);
			}

			pos = style_end + 1;
		}
		else
		{
			pos = std::string::npos;
		}

		if (pos != std::string::npos)
		{
			pos = text.find_first_not_of(" \n\r\t", pos);
		}
	}
}

std::string css::parse_css_url(const std::string& str)
{
	std::string result;

	const size_t pos1 = str.find_first_of('(');
	const size_t pos2 = str.find_first_of(')');

	if (pos1 != std::string::npos && pos2 != std::string::npos)
	{
		result = str.substr(pos1 + 1, pos2 - pos1 - 1);
		if (result.length())
		{
			if (result[0] == '\'' || result[0] == '"')
			{
				result.erase(0, 1);
			}
		}
		if (result.length())
		{
			if (result[result.length() - 1] == '\'' || result[result.length() - 1] == '"')
			{
				result.erase(result.length() - 1, 1);
			}
		}
	}

	return result;
}

void css::parse_selectors(const std::string& text, const std::shared_ptr<style>& styles,
                          std::shared_ptr<media_query_list>& media)
{
	const auto tokens = split_string(text, ',');

	for (auto tok : tokens)
	{
		auto selector = std::make_shared<css_selector>(styles, media);

		if (selector->parse(tok))
		{
			selector->calc_specificity();
			add_selector(selector);
		}
	}
}

void css::sort_selectors()
{
	sort(m_selectors.begin(), m_selectors.end(), std::less<std::shared_ptr<css_selector>>());
	rebuild_buckets();
}

// Classify each selector by the most specific part of its rightmost compound.
// Priority: id > class > tag > universal. A class-keyed selector may require
// multiple classes (e.g. ".foo.bar"); we register it in every matching bucket so
// the element probe finds it on ANY of its required classes, and the existing
// per-selector match re-checks the rest.
static selector_key compute_selector_key(const css_selector& sel)
{
	selector_key key;

	for (const auto& a : sel.m_right.m_attrs)
	{
		if (a.attribute == "id" && a.condition == select_equal)
		{
			key.kind = selector_key::bucket_id;
			key.values = {a.val};
			return key;
		}
	}

	std::vector<std::string> all_classes;
	for (const auto& a : sel.m_right.m_attrs)
	{
		if (a.attribute == "class" && a.condition == select_equal)
		{
			auto tokens = split_string(a.val);
			for (auto& t : tokens)
			{
				trim(t);
				if (!t.empty()) all_classes.push_back(std::move(t));
			}
		}
	}
	if (!all_classes.empty())
	{
		key.kind = selector_key::bucket_class;
		key.values = std::move(all_classes);
		return key;
	}

	if (!sel.m_right.m_tag.empty() && sel.m_right.m_tag != "*")
	{
		key.kind = selector_key::bucket_tag;
		key.values = {sel.m_right.m_tag};
		return key;
	}

	key.kind = selector_key::bucket_universal;
	return key;
}

void css::rebuild_buckets()
{
	m_by_id.clear();
	m_by_class.clear();
	m_by_tag.clear();
	m_universal.clear();

	for (const auto& sel : m_selectors)
	{
		sel->m_key = compute_selector_key(*sel);
		switch (sel->m_key.kind)
		{
		case selector_key::bucket_id:
			m_by_id[sel->m_key.values.front()].push_back(sel);
			break;
		case selector_key::bucket_class:
			for (const auto& c : sel->m_key.values)
			{
				m_by_class[c].push_back(sel);
			}
			break;
		case selector_key::bucket_tag:
			m_by_tag[sel->m_key.values.front()].push_back(sel);
			break;
		case selector_key::bucket_universal:
			m_universal.push_back(sel);
			break;
		}
	}
}

void css::parse_atrule(const std::string& text, const std::string& baseurl, document& doc,
                       std::shared_ptr<media_query_list>& media)
{
	if (text.substr(0, 7) == "@import")
	{
		constexpr int sPos = 7;
		std::string iStr = text.substr(sPos);
		if (iStr[iStr.length() - 1] == ';')
		{
			iStr.erase(iStr.length() - 1);
		}
		trim(iStr);
		auto tokens = split_string(iStr, " ", "(\"");
		//tokenize(iStr, tokens, " ", "", "()\"");
		if (!tokens.empty())
		{
			auto url = parse_css_url(tokens.front());

			if (url.empty())
			{
				url = tokens.front();
			}
			tokens.erase(tokens.begin());
			std::string css_baseurl;

			if (!baseurl.empty())
			{
				css_baseurl = baseurl;
			}

			doc.import_css(url, css_baseurl);

			/*if (!css_text.empty())
			{
			std::shared_ptr<media_query_list> new_media = media;
			if (!tokens.empty())
			{
			std::string media_str;
			for (const auto &tok : tokens)
			{
			if (iter != tokens.begin())
			{
			media_str += " ";
			}
			media_str += (*iter);
			}
			new_media = media_query_list::create_from_string(media_str, doc);
			if (!new_media)
			{
			new_media = media;
			}
			}
			parse_stylesheet(css_text, css_baseurl, doc, new_media);
			}*/
		}
	}
	else if (text.substr(0, 6) == "@media")
	{
		const auto b1 = text.find_first_of('{');
		const auto b2 = text.find_last_of('}');

		if (b1 != std::string::npos)
		{
			std::string media_type = text.substr(6, b1 - 6);
			trim(media_type);
			std::shared_ptr<media_query_list> new_media = media_query_list::create_from_string(media_type);

			std::string media_style;
			if (b2 != std::string::npos)
			{
				media_style = text.substr(b1 + 1, b2 - b1 - 1);
			}
			else
			{
				media_style = text.substr(b1 + 1);
			}

			parse_stylesheet(media_style, baseurl, doc, new_media);
		}
	}
	else if (text.substr(0, 9) == "@supports")
	{
		// Include @supports content unconditionally — optimistic feature detection
		const auto b1 = text.find_first_of('{');
		const auto b2 = text.find_last_of('}');

		if (b1 != std::string::npos)
		{
			std::string supports_style;
			if (b2 != std::string::npos)
			{
				supports_style = text.substr(b1 + 1, b2 - b1 - 1);
			}
			else
			{
				supports_style = text.substr(b1 + 1);
			}

			parse_stylesheet(supports_style, baseurl, doc, media);
		}
	}
}


static size get_img_size(const std::shared_ptr<Gdiplus::Bitmap>& bmp)
{
	size result;

	if (bmp)
	{
		result.width = bmp->GetWidth();
		result.height = bmp->GetHeight();
	}

	return result;
}


static void draw_img(const HDC hdc, const std::shared_ptr<Gdiplus::Bitmap>& bmp, const position& pos)
{
	if (bmp)
	{
		Gdiplus::Graphics graphics(hdc);
		graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
		graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
		graphics.DrawImage(bmp.get(), pos.x, pos.y, pos.width, pos.height);
	}
}

static void draw_img_bg(const HDC hdc, const std::shared_ptr<Gdiplus::Bitmap>& bgbmp, const position& draw_pos,
                        const position& pos, const background_repeat repeat, background_attachment attachment)
{
	int img_width = bgbmp->GetWidth();
	int img_height = bgbmp->GetHeight();

	Gdiplus::Graphics graphics(hdc);
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
	graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

	const Gdiplus::Region reg(Gdiplus::Rect(draw_pos.left(), draw_pos.top(), draw_pos.width, draw_pos.height));
	graphics.SetClip(&reg);

	switch (repeat)
	{
	case background_repeat_no_repeat:
		{
			graphics.DrawImage(bgbmp.get(), pos.x, pos.y, bgbmp->GetWidth(), bgbmp->GetHeight());
		}
		break;
	case background_repeat_repeat_x:
		{
			Gdiplus::CachedBitmap bmp(bgbmp.get(), &graphics);
			for (int x = pos.left(); x < pos.right(); x += bgbmp->GetWidth())
			{
				graphics.DrawCachedBitmap(&bmp, x, pos.top());
			}

			for (int x = pos.left() - bgbmp->GetWidth(); x + static_cast<int>(bgbmp->GetWidth()) > draw_pos.left(); x -=
			     bgbmp->GetWidth())
			{
				graphics.DrawCachedBitmap(&bmp, x, pos.top());
			}
		}
		break;
	case background_repeat_repeat_y:
		{
			Gdiplus::CachedBitmap bmp(bgbmp.get(), &graphics);
			for (int y = pos.top(); y < pos.bottom(); y += bgbmp->GetHeight())
			{
				graphics.DrawCachedBitmap(&bmp, pos.left(), y);
			}

			for (int y = pos.top() - bgbmp->GetHeight(); y + static_cast<int>(bgbmp->GetHeight()) > draw_pos.top(); y -=
			     bgbmp->GetHeight())
			{
				graphics.DrawCachedBitmap(&bmp, pos.left(), y);
			}
		}
		break;
	case background_repeat_repeat:
		{
			Gdiplus::CachedBitmap bmp(bgbmp.get(), &graphics);
			if (bgbmp->GetHeight() >= 0)
			{
				for (int x = pos.left(); x < pos.right(); x += bgbmp->GetWidth())
				{
					for (int y = pos.top(); y < pos.bottom(); y += bgbmp->GetHeight())
					{
						graphics.DrawCachedBitmap(&bmp, x, y);
					}
				}
			}
		}
		break;
	}
}


int render_win32::line_height(const HFONT hFont)
{
	const auto oldFont = static_cast<HFONT>(SelectObject(_hdc, hFont));
	TEXTMETRIC tm;
	GetTextMetrics(_hdc, &tm);
	SelectObject(_hdc, oldFont);
	return static_cast<int>(tm.tmHeight);
}


void render_win32::draw_text(const char* text, const HFONT hFont, const web_color& color, const position& pos)
{
	apply_clip();

	const auto oldFont = static_cast<HFONT>(SelectObject(_hdc, hFont));

	SetBkMode(_hdc, TRANSPARENT);

	SetTextColor(_hdc, RGB(color.red, color.green, color.blue));

	const auto wtext = to_utf16(text);
	ExtTextOutW(_hdc, pos.left(), pos.top(), 0, nullptr, wtext.c_str(), static_cast<UINT>(wtext.size()), nullptr);

	SelectObject(_hdc, oldFont);

	release_clip();
}

void render_win32::fill_rect(const position& pos, const web_color& color, const css_border_radius& radius)
{
	apply_clip();
	fill_rect(pos.x, pos.y, pos.width, pos.height, color, radius);
	release_clip();
}


void render_win32::draw_list_marker(const list_marker& marker)
{
	apply_clip();

	const int top_margin = marker.pos.height / 3;

	const int draw_x = marker.pos.x;
	const int draw_y = marker.pos.y + top_margin;
	const int draw_width = marker.pos.height - top_margin * 2;
	const int draw_height = marker.pos.height - top_margin * 2;

	switch (marker.marker_type)
	{
	case list_style_type_circle:
		{
			draw_ellipse(draw_x, draw_y, draw_width, draw_height, marker.color, 1);
		}
		break;
	case list_style_type_disc:
		{
			fill_ellipse(draw_x, draw_y, draw_width, draw_height, marker.color);
		}
		break;
	case list_style_type_square:
		{
			fill_rect(draw_x, draw_y, draw_width, draw_height, marker.color, css_border_radius());
		}
		break;
	}
	release_clip();
}

void render_win32::draw_image(const std::shared_ptr<Gdiplus::Bitmap>& bm, const position& pos)
{
	draw_img(_hdc, bm, pos);
}

size render_win32::get_image_size(const std::shared_ptr<Gdiplus::Bitmap>& bm)
{
	return get_img_size(bm);
}


void render_win32::draw_background(render_win32& renderer, const background_paint& bg)
{
	apply_clip();

	if (bg.color.alpha > 0)
	{
		fill_rect(bg.border_box, bg.color, bg.border_radius);
	}

	const auto img = bg.image;

	if (img)
	{
		auto img_sz = get_img_size(img);
		const position pos(bg.position_x, bg.position_y, bg.image_size.width, bg.image_size.height);
		const auto draw_pos = pos;

		/*if (bg_pos.x.units() != css_units_percentage)
		{
		pos.x += (int) bg_pos.x.val();
		}
		else
		{
		pos.x += (int) ((float) (draw_pos.width - img_sz.width) * bg_pos.x.val() / 100.0);
		}

		if (bg_pos.y.units() != css_units_percentage)
		{
		pos.y += (int) bg_pos.y.val();
		}
		else
		{
		pos.y += (int) ((float) (draw_pos.height - img_sz.height) * bg_pos.y.val() / 100.0);
		}*/

		draw_img_bg(_hdc, img, draw_pos, pos, bg.repeat, bg.attachment);
	}

	release_clip();
}

void render_win32::set_clip(const position& pos, const bool valid_x, const bool valid_y)
{
	position clip_pos = pos;

	if (!valid_x)
	{
		clip_pos.x = _client_pos.x;
		clip_pos.width = _client_pos.width;
	}
	if (!valid_y)
	{
		clip_pos.y = _client_pos.y;
		clip_pos.height = _client_pos.height;
	}
	m_clips.push_back(clip_pos);
}

void render_win32::del_clip()
{
	if (!m_clips.empty())
	{
		m_clips.pop_back();
		if (!m_clips.empty())
		{
			position clip_pos = m_clips.back();
		}
	}
}

void render_win32::apply_clip()
{
	if (m_hClipRgn)
	{
		DeleteObject(m_hClipRgn);
		m_hClipRgn = nullptr;
	}

	if (!m_clips.empty())
	{
		POINT ptView = {0, 0};
		GetWindowOrgEx(_hdc, &ptView);

		const position clip_pos = m_clips.back();
		m_hClipRgn = CreateRectRgn(clip_pos.left() - ptView.x, clip_pos.top() - ptView.y, clip_pos.right() - ptView.x,
		                           clip_pos.bottom() - ptView.y);
		SelectClipRgn(_hdc, m_hClipRgn);
	}
}

void render_win32::release_clip()
{
	SelectClipRgn(_hdc, nullptr);

	if (m_hClipRgn)
	{
		DeleteObject(m_hClipRgn);
		m_hClipRgn = nullptr;
	}
}

void render_win32::draw_ellipse(const int x, const int y, const int width, const int height, const web_color& color,
                                int line_width)
{
	Gdiplus::Graphics graphics(_hdc);
	Gdiplus::LinearGradientBrush* brush = nullptr;

	graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
	graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

	const Gdiplus::Pen pen(Gdiplus::Color(color.alpha, color.red, color.green, color.blue));
	graphics.DrawEllipse(&pen, x, y, width, height);
}

void render_win32::fill_ellipse(const int x, const int y, const int width, const int height, const web_color& color)
{
	Gdiplus::Graphics graphics(_hdc);

	graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
	graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

	const Gdiplus::SolidBrush brush(Gdiplus::Color(color.alpha, color.red, color.green, color.blue));
	graphics.FillEllipse(&brush, x, y, width, height);
}

void render_win32::fill_rect(const int x, const int y, const int width, const int height, const web_color& color,
                             const css_border_radius& radius)
{
	Gdiplus::Graphics graphics(_hdc);

	const Gdiplus::SolidBrush brush(Gdiplus::Color(color.alpha, color.red, color.green, color.blue));
	graphics.FillRectangle(&brush, x, y, width, height);
}


void render_win32::draw_borders(const css_borders& borders, const position& draw_pos, bool root)
{
	apply_clip();

	// draw left border
	if (borders.left.width.val() != 0 && borders.left.style > border_style_hidden)
	{
		const HPEN pen = CreatePen(PS_SOLID, 1, RGB(borders.left.color.red, borders.left.color.green,
		                                            borders.left.color.blue));
		const auto oldPen = static_cast<HPEN>(SelectObject(_hdc, pen));
		for (int x = 0; x < borders.left.width.val(); x++)
		{
			MoveToEx(_hdc, draw_pos.left() + x, draw_pos.top(), nullptr);
			LineTo(_hdc, draw_pos.left() + x, draw_pos.bottom());
		}
		SelectObject(_hdc, oldPen);
		DeleteObject(pen);
	}
	// draw right border
	if (borders.right.width.val() != 0 && borders.right.style > border_style_hidden)
	{
		const HPEN pen = CreatePen(PS_SOLID, 1, RGB(borders.right.color.red, borders.right.color.green,
		                                            borders.right.color.blue));
		const auto oldPen = static_cast<HPEN>(SelectObject(_hdc, pen));
		for (int x = 0; x < borders.right.width.val(); x++)
		{
			MoveToEx(_hdc, draw_pos.right() - x - 1, draw_pos.top(), nullptr);
			LineTo(_hdc, draw_pos.right() - x - 1, draw_pos.bottom());
		}
		SelectObject(_hdc, oldPen);
		DeleteObject(pen);
	}
	// draw top border
	if (borders.top.width.val() != 0 && borders.top.style > border_style_hidden)
	{
		const HPEN pen = CreatePen(PS_SOLID, 1, RGB(borders.top.color.red, borders.top.color.green,
		                                            borders.top.color.blue));
		const auto oldPen = static_cast<HPEN>(SelectObject(_hdc, pen));
		for (int y = 0; y < borders.top.width.val(); y++)
		{
			MoveToEx(_hdc, draw_pos.left(), draw_pos.top() + y, nullptr);
			LineTo(_hdc, draw_pos.right(), draw_pos.top() + y);
		}
		SelectObject(_hdc, oldPen);
		DeleteObject(pen);
	}
	// draw bottom border
	if (borders.bottom.width.val() != 0 && borders.bottom.style > border_style_hidden)
	{
		const HPEN pen = CreatePen(PS_SOLID, 1, RGB(borders.bottom.color.red, borders.bottom.color.green,
		                                            borders.bottom.color.blue));
		const auto oldPen = static_cast<HPEN>(SelectObject(_hdc, pen));
		for (int y = 0; y < borders.bottom.width.val(); y++)
		{
			MoveToEx(_hdc, draw_pos.left(), draw_pos.bottom() - y - 1, nullptr);
			LineTo(_hdc, draw_pos.right(), draw_pos.bottom() - y - 1);
		}
		SelectObject(_hdc, oldPen);
		DeleteObject(pen);
	}

	release_clip();
}
