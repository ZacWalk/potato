// document.cpp - WinHTTP async downloads, HTML entity table, parser with
// implicit tag closing, document rendering, font caching via GDI+, and
// stylesheet application.

#include "pch.h"
#include "document.h"
// ui.h removed: document.cpp now uses view_host + dispatch_to_ui/async from core.h

namespace
{
	std::string svg_attribute(const std::string& tag, const std::string& name)
	{
		size_t pos = 0;
		while ((pos = tag.find(name, pos)) != std::string::npos)
		{
			const bool valid_start = pos == 0 || isspace(static_cast<unsigned char>(tag[pos - 1]));
			size_t equals = pos + name.size();
			while (equals < tag.size() && isspace(static_cast<unsigned char>(tag[equals]))) ++equals;
			if (!valid_start || equals >= tag.size() || tag[equals] != '=')
			{
				pos += name.size();
				continue;
			}

			++equals;
			while (equals < tag.size() && isspace(static_cast<unsigned char>(tag[equals]))) ++equals;
			if (equals >= tag.size()) return {};

			const char quote = tag[equals];
			if (quote == '\'' || quote == '"')
			{
				const auto end = tag.find(quote, equals + 1);
				return end == std::string::npos ? std::string{} : tag.substr(equals + 1, end - equals - 1);
			}

			const auto end = tag.find_first_of(" \t\r\n>", equals);
			return tag.substr(equals, end - equals);
		}
		return {};
	}

	double svg_number(const std::string& value)
	{
		if (value.empty() || value.find('%') != std::string::npos) return 0;
		char* end = nullptr;
		const double result = std::strtod(value.c_str(), &end);
		return end != value.c_str() && result > 0 ? result : 0;
	}

	pf::bitmap_ptr create_svg_placeholder(const std::string& text)
	{
		std::string lower = text.substr(0, std::min<size_t>(text.size(), 8192));
		transform_text(lower, text_transform_lowercase);
		const auto svg_start = lower.find("<svg");
		if (svg_start == std::string::npos) return nullptr;
		const auto svg_end = lower.find('>', svg_start + 4);
		if (svg_end == std::string::npos) return nullptr;

		const auto tag = lower.substr(svg_start, svg_end - svg_start + 1);
		double width = svg_number(svg_attribute(tag, "width"));
		double height = svg_number(svg_attribute(tag, "height"));

		double view_width = 0;
		double view_height = 0;
		const auto view_box = svg_attribute(tag, "viewbox");
		if (!view_box.empty())
		{
			const char* cursor = view_box.c_str();
			for (int part = 0; part < 4; ++part)
			{
				while (*cursor && (isspace(static_cast<unsigned char>(*cursor)) || *cursor == ',')) ++cursor;
				char* end = nullptr;
				const double value = std::strtod(cursor, &end);
				if (end == cursor) break;
				if (part == 2) view_width = value;
				if (part == 3) view_height = value;
				cursor = end;
			}
		}

		if (width <= 0) width = view_width;
		if (height <= 0) height = view_height;
		if (width <= 0 && height > 0 && view_width > 0 && view_height > 0)
			width = height * view_width / view_height;
		if (height <= 0 && width > 0 && view_width > 0 && view_height > 0)
			height = width * view_height / view_width;
		if (width <= 0) width = 300;
		if (height <= 0) height = 150;

		const int bitmap_width = std::clamp(static_cast<int>(width + 0.5), 1, 2048);
		const int bitmap_height = std::clamp(static_cast<int>(height + 0.5), 1, 2048);
		std::vector<uint32_t> pixels(static_cast<size_t>(bitmap_width) * bitmap_height, 0xffeeeeee);
		const auto set_pixel = [&](const int x, const int y, const uint32_t color)
		{
			pixels[static_cast<size_t>(y) * bitmap_width + x] = color;
		};

		for (int x = 0; x < bitmap_width; ++x)
		{
			set_pixel(x, 0, 0xff999999);
			set_pixel(x, bitmap_height - 1, 0xff999999);
		}
		for (int y = 0; y < bitmap_height; ++y)
		{
			set_pixel(0, y, 0xff999999);
			set_pixel(bitmap_width - 1, y, 0xff999999);
			const int diagonal = bitmap_height > 1
				                     ? y * (bitmap_width - 1) / (bitmap_height - 1)
				                     : 0;
			set_pixel(diagonal, y, 0xffbbbbbb);
			set_pixel(bitmap_width - 1 - diagonal, y, 0xffbbbbbb);
		}

		return std::make_shared<pf::bitmap>(bitmap_width, bitmap_height, std::move(pixels));
	}
}


// Async HTTP — wraps pf::async_http_session to download to a temp file and
// then deliver a single completion callback (file_path, error, status, url).

bool http::open(const std::string_view user_agent)
{
	m_session = pf::create_async_http_session(user_agent);
	return static_cast<bool>(m_session);
}

void http::close()
{
	stop();
	m_session.reset();
}

void http::stop()
{
	std::vector<std::shared_ptr<http_request>> snapshot;
	{
		std::lock_guard lk(m_mutex);
		snapshot = m_requests;
	}
	for (const auto& r : snapshot) r->cancel();
	if (m_session) m_session->stop();
}

bool http::download_file(const std::string& url_in, const std::shared_ptr<http_request>& request)
{
	if (!request || !m_session) return false;

	std::string url = url_in;
	if (!starts(url, "http://") && !starts(url, "https://"))
	{
		url = "https://" + url;
	}

	const std::string temp_path = pf::platform_temp_file_path("pot");
	auto file = pf::open_file_for_write(pf::file_path(temp_path));
	if (!file)
	{
		pf::platform_delete_file(pf::file_path(temp_path));
		return false;
	}

	{
		std::lock_guard lk(m_mutex);
		m_requests.push_back(request);
	}

	struct ctx_t
	{
		std::shared_ptr<http_request> req;
		pf::writable_file_handle_ptr file;
		std::string file_path;
		std::string url;
		http* parent = nullptr;
		std::atomic<int> status_code{0};
		std::atomic<bool> done{false};
	};
	auto ctx = std::make_shared<ctx_t>();
	ctx->req = request;
	ctx->file = std::move(file);
	ctx->file_path = temp_path;
	ctx->url = url;
	ctx->parent = this;

	pf::async_http_callbacks cb;
	cb.on_headers = [ctx](const int status, std::string, uint64_t)
	{
		ctx->status_code = status;
	};
	cb.on_data = [ctx](const uint8_t* data, const size_t size)
	{
		if (ctx->file) ctx->file->write(data, static_cast<uint32_t>(size));
	};
	auto finish = [ctx](uint32_t error)
	{
		if (ctx->done.exchange(true)) return;
		ctx->file.reset();
		auto cb_user = ctx->req->m_callback;
		auto file_path = ctx->file_path;
		auto url_capture = ctx->url;
		auto status = static_cast<uint32_t>(ctx->status_code.load());
		auto* parent = ctx->parent;
		auto req = ctx->req;
		// Remove the request from the parent http synchronously while we still
		// know the parent is alive (this callback fires on the http worker
		// thread, which the parent's destructor waits on via stop()).
		if (parent)
		{
			std::lock_guard lk(parent->m_mutex);
			std::erase(parent->m_requests, req);
		}
		dispatch_to_ui([cb_user = std::move(cb_user), file_path = std::move(file_path),
				error, status, url_capture = std::move(url_capture), req]()
			{
				if (cb_user) cb_user(file_path, error, status, url_capture);
				// The callback reads the body synchronously, so the download's
				// scratch file has no readers left once it returns.
				pf::platform_delete_file(pf::file_path(file_path));
			});
	};
	cb.on_complete = [finish]() { finish(0); };
	cb.on_error = [finish](std::string) { finish(1); };

	auto async = m_session->get(url, std::move(cb));
	if (!async)
	{
		ctx->file.reset();
		pf::platform_delete_file(pf::file_path(temp_path));
		std::lock_guard lk(m_mutex);
		std::erase(m_requests, request);
		return false;
	}
	request->set_async(std::move(async));
	return true;
}


html_entities g_html_entities[] =
{
	{"&quot;", 0x0022},
	{"&amp;", 0x0026},
	{"&lt;", 0x003C},
	{"&gt;", 0x003E},
	{"&nbsp;", 0x00A0},
	{"&iexcl;", 0x00A1},
	{"&cent;", 0x00A2},
	{"&pound;", 0x00A3},
	{"&curren;", 0x00A4},
	{"&yen;", 0x00A5},
	{"&brvbar;", 0x00A6},
	{"&sect;", 0x00A7},
	{"&uml;", 0x00A8},
	{"&copy;", 0x00A9},
	{"&ordf;", 0x00AA},
	{"&laquo;", 0x00AB},
	{"&lsaquo;", 0x00AB},
	{"&not;", 0x00AC},
	{"&shy;", 0x00AD},
	{"&reg;", 0x00AE},
	{"&macr;", 0x00AF},
	{"&deg;", 0x00B0},
	{"&plusmn;", 0x00B1},
	{"&sup2;", 0x00B2},
	{"&sup3;", 0x00B3},
	{"&acute;", 0x00B4},
	{"&micro;", 0x00B5},
	{"&para;", 0x00B6},
	{"&middot;", 0x00B7},
	{"&cedil;", 0x00B8},
	{"&sup1;", 0x00B9},
	{"&ordm;", 0x00BA},
	{"&raquo;", 0x00BB},
	{"&rsaquo;", 0x00BB},
	{"&frac14;", 0x00BC},
	{"&frac12;", 0x00BD},
	{"&frac34;", 0x00BE},
	{"&iquest;", 0x00BF},
	{"&Agrave;", 0x00C0},
	{"&Aacute;", 0x00C1},
	{"&Acirc;", 0x00C2},
	{"&Atilde;", 0x00C3},
	{"&Auml;", 0x00C4},
	{"&Aring;", 0x00C5},
	{"&AElig;", 0x00C6},
	{"&Ccedil;", 0x00C7},
	{"&Egrave;", 0x00C8},
	{"&Eacute;", 0x00C9},
	{"&Ecirc;", 0x00CA},
	{"&Euml;", 0x00CB},
	{"&Igrave;", 0x00CC},
	{"&Iacute;", 0x00CD},
	{"&Icirc;", 0x00CE},
	{"&Iuml;", 0x00CF},
	{"&ETH;", 0x00D0},
	{"&Ntilde;", 0x00D1},
	{"&Ograve;", 0x00D2},
	{"&Oacute;", 0x00D3},
	{"&Ocirc;", 0x00D4},
	{"&Otilde;", 0x00D5},
	{"&Ouml;", 0x00D6},
	{"&times;", 0x00D7},
	{"&Oslash;", 0x00D8},
	{"&Ugrave;", 0x00D9},
	{"&Uacute;", 0x00DA},
	{"&Ucirc;", 0x00DB},
	{"&Uuml;", 0x00DC},
	{"&Yacute;", 0x00DD},
	{"&THORN;", 0x00DE},
	{"&szlig;", 0x00DF},
	{"&agrave;", 0x00E0},
	{"&aacute;", 0x00E1},
	{"&acirc;", 0x00E2},
	{"&atilde;", 0x00E3},
	{"&auml;", 0x00E4},
	{"&aring;", 0x00E5},
	{"&aelig;", 0x00E6},
	{"&ccedil;", 0x00E7},
	{"&egrave;", 0x00E8},
	{"&eacute;", 0x00E9},
	{"&ecirc;", 0x00EA},
	{"&euml;", 0x00EB},
	{"&igrave;", 0x00EC},
	{"&iacute;", 0x00ED},
	{"&icirc;", 0x00EE},
	{"&iuml;", 0x00EF},
	{"&eth;", 0x00F0},
	{"&ntilde;", 0x00F1},
	{"&ograve;", 0x00F2},
	{"&oacute;", 0x00F3},
	{"&ocirc;", 0x00F4},
	{"&otilde;", 0x00F5},
	{"&ouml;", 0x00F6},
	{"&divide;", 0x00F7},
	{"&oslash;", 0x00F8},
	{"&ugrave;", 0x00F9},
	{"&uacute;", 0x00FA},
	{"&ucirc;", 0x00FB},
	{"&uuml;", 0x00FC},
	{"&yacute;", 0x00FD},
	{"&thorn;", 0x00FE},
	{"&yuml;", 0x00FF},
	{"&OElig;", 0x0152},
	{"&oelig;", 0x0153},
	{"&Scaron;", 0x0160},
	{"&scaron;", 0x0161},
	{"&Yuml;", 0x0178},
	{"&fnof;", 0x0192},
	{"&circ;", 0x02C6},
	{"&tilde;", 0x02DC},
	{"&Alpha;", 0x0391},
	{"&Beta;", 0x0392},
	{"&Gamma;", 0x0393},
	{"&Delta;", 0x0394},
	{"&Epsilon;", 0x0395},
	{"&Zeta;", 0x0396},
	{"&Eta;", 0x0397},
	{"&Theta;", 0x0398},
	{"&Iota;", 0x0399},
	{"&Kappa;", 0x039A},
	{"&Lambda;", 0x039B},
	{"&Mu;", 0x039C},
	{"&Nu;", 0x039D},
	{"&Xi;", 0x039E},
	{"&Omicron;", 0x039F},
	{"&Pi;", 0x03A0},
	{"&Rho;", 0x03A1},
	{"&Sigma;", 0x03A3},
	{"&Tau;", 0x03A4},
	{"&Upsilon;", 0x03A5},
	{"&Phi;", 0x03A6},
	{"&Chi;", 0x03A7},
	{"&Psi;", 0x03A8},
	{"&Omega;", 0x03A9},
	{"&alpha;", 0x03B1},
	{"&beta;", 0x03B2},
	{"&gamma;", 0x03B3},
	{"&delta;", 0x03B4},
	{"&epsilon;", 0x03B5},
	{"&zeta;", 0x03B6},
	{"&eta;", 0x03B7},
	{"&theta;", 0x03B8},
	{"&iota;", 0x03B9},
	{"&kappa;", 0x03BA},
	{"&lambda;", 0x03BB},
	{"&mu;", 0x03BC},
	{"&nu;", 0x03BD},
	{"&xi;", 0x03BE},
	{"&omicron;", 0x03BF},
	{"&pi;", 0x03C0},
	{"&rho;", 0x03C1},
	{"&sigmaf;", 0x03C2},
	{"&sigma;", 0x03C3},
	{"&tau;", 0x03C4},
	{"&upsilon;", 0x03C5},
	{"&phi;", 0x03C6},
	{"&chi;", 0x03C7},
	{"&psi;", 0x03C8},
	{"&omega;", 0x03C9},
	{"&thetasym;", 0x03D1},
	{"&upsih;", 0x03D2},
	{"&piv;", 0x03D6},
	{"&ensp;", 0x2002},
	{"&emsp;", 0x2003},
	{"&thinsp;", 0x2009},
	{"&zwnj;", 0x200C},
	{"&zwj;", 0x200D},
	{"&lrm;", 0x200E},
	{"&rlm;", 0x200F},
	{"&ndash;", 0x2013},
	{"&mdash;", 0x2014},
	{"&lsquo;", 0x2018},
	{"&rsquo;", 0x2019},
	{"&sbquo;", 0x201A},
	{"&ldquo;", 0x201C},
	{"&rdquo;", 0x201D},
	{"&bdquo;", 0x201E},
	{"&dagger;", 0x2020},
	{"&Dagger;", 0x2021},
	{"&bull;", 0x2022},
	{"&hellip;", 0x2026},
	{"&permil;", 0x2030},
	{"&prime;", 0x2032},
	{"&Prime;", 0x2033},
	{"&lsaquo;", 0x2039},
	{"&rsaquo;", 0x203A},
	{"&oline;", 0x203E},
	{"&frasl;", 0x2044},
	{"&euro;", 0x20AC},
	{"&image;", 0x2111},
	{"&weierp;", 0x2118},
	{"&real;", 0x211C},
	{"&trade;", 0x2122},
	{"&alefsym;", 0x2135},
	{"&larr;", 0x2190},
	{"&uarr;", 0x2191},
	{"&rarr;", 0x2192},
	{"&darr;", 0x2193},
	{"&harr;", 0x2194},
	{"&crarr;", 0x21B5},
	{"&lArr;", 0x21D0},
	{"&uArr;", 0x21D1},
	{"&rArr;", 0x21D2},
	{"&dArr;", 0x21D3},
	{"&hArr;", 0x21D4},
	{"&forall;", 0x2200},
	{"&part;", 0x2202},
	{"&exist;", 0x2203},
	{"&empty;", 0x2205},
	{"&nabla;", 0x2207},
	{"&isin;", 0x2208},
	{"&notin;", 0x2209},
	{"&ni;", 0x220B},
	{"&prod;", 0x220F},
	{"&sum;", 0x2211},
	{"&minus;", 0x2212},
	{"&lowast;", 0x2217},
	{"&radic;", 0x221A},
	{"&prop;", 0x221D},
	{"&infin;", 0x221E},
	{"&ang;", 0x2220},
	{"&and;", 0x2227},
	{"&or;", 0x2228},
	{"&cap;", 0x2229},
	{"&cup;", 0x222A},
	{"&int;", 0x222B},
	{"&there4;", 0x2234},
	{"&sim;", 0x223C},
	{"&cong;", 0x2245},
	{"&asymp;", 0x2248},
	{"&ne;", 0x2260},
	{"&equiv;", 0x2261},
	{"&le;", 0x2264},
	{"&ge;", 0x2265},
	{"&sub;", 0x2282},
	{"&sup;", 0x2283},
	{"&nsub;", 0x2284},
	{"&sube;", 0x2286},
	{"&supe;", 0x2287},
	{"&oplus;", 0x2295},
	{"&otimes;", 0x2297},
	{"&perp;", 0x22A5},
	{"&sdot;", 0x22C5},
	{"&lceil;", 0x2308},
	{"&rceil;", 0x2309},
	{"&lfloor;", 0x230A},
	{"&rfloor;", 0x230B},
	{"&lang;", 0x2329},
	{"&rang;", 0x232A},
	{"&loz;", 0x25CA},
	{"&spades;", 0x2660},
	{"&clubs;", 0x2663},
	{"&hearts;", 0x2665},
	{"&diams;", 0x2666},

	{"", 0}
};


stop_tags_t parser::m_stop_tags[] =
{
	{"body;head", "html"},
	{"td;th;tr;tbody;thead;tfoot", "table"},
	{nullptr, nullptr},
};

omitted_end_tags_t parser::m_omitted_end_tags[] =
{
	{"li", "li"},
	{"dt", "dt;dd"},
	{"dd", "dt;dd"},
	{
		"p",
		"address;article;aside;blockquote;div;dl;fieldset;footer;form;h1;h2;h3;h4;h5;h6;header;hgroup;hr;main;nav;ol;p;pre;section;table;ul"
	},
	{"rb", "rb;rt;rtc;rp"},
	{"rt", "rb;rt;rtc;rp"},
	{"rtc", "rb;rt;rtc;rp"},
	{"rp", "rb;rt;rtc;rp"},
	{"optgroup", "optgroup"},
	{"option", "optgroup;option"},
	{"thead", "tbody;tfoot"},
	{"tbody", "tbody;tfoot"},
	{"tfoot", "tbody;tfoot"},
	{"tr", "tr"},
	{"td", "td;th"},
	{"th", "td;th"},
	{nullptr, nullptr},
};


// ── html_scanner ────────────────────────────────────────────────────────────

namespace
{
	uint32_t lookup_entity(const std::string_view name)
	{
		uint32_t fallback = 0;

		for (int i = 0; g_html_entities[i].szCode[0]; i++)
		{
			// Table entries are stored as "&name;".
			const char* code = g_html_entities[i].szCode + 1;
			const size_t n = strlen(code);
			if (n != name.size() + 1 || code[name.size()] != ';') continue;

			if (memcmp(code, name.data(), name.size()) == 0)
			{
				return g_html_entities[i].Code;
			}
			if (!fallback && _strnicmp(code, name.data(), name.size()) == 0)
			{
				fallback = g_html_entities[i].Code;
			}
		}

		return fallback;
	}

	int hex_digit(const char c)
	{
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		return -1;
	}
}

bool html_scanner::starts_ci(const size_t at, const std::string_view with) const
{
	if (at + with.size() > m_src.size()) return false;
	for (size_t i = 0; i < with.size(); ++i)
		if (lower(m_src[at + i]) != lower(with[i])) return false;
	return true;
}

size_t html_scanner::find_ci(const std::string_view needle, const size_t from) const
{
	if (needle.empty() || needle.size() > m_src.size()) return std::string_view::npos;

	const char lo = lower(needle[0]);
	const char up = lo >= 'a' && lo <= 'z' ? static_cast<char>(lo - 'a' + 'A') : lo;

	for (size_t i = from; i + needle.size() <= m_src.size(); ++i)
	{
		const char c = m_src[i];
		if (c != lo && c != up) continue;
		if (starts_ci(i, needle)) return i;
	}

	return std::string_view::npos;
}

bool html_scanner::decode_entity(std::string& out)
{
	const size_t start = m_pos; // just past '&'

	if (peek() == '#')
	{
		size_t p = m_pos + 1;
		uint32_t cp = 0;
		bool any = false;

		if (p < m_src.size() && (m_src[p] == 'x' || m_src[p] == 'X'))
		{
			++p;
			for (int d; p < m_src.size() && (d = hex_digit(m_src[p])) >= 0; ++p)
			{
				cp = cp * 16 + d;
				any = true;
				if (cp > 0x10FFFF) break;
			}
		}
		else
		{
			for (; p < m_src.size() && m_src[p] >= '0' && m_src[p] <= '9'; ++p)
			{
				cp = cp * 10 + (m_src[p] - '0');
				any = true;
				if (cp > 0x10FFFF) break;
			}
		}

		if (any && cp && cp <= 0x10FFFF)
		{
			if (p < m_src.size() && m_src[p] == ';') ++p;
			m_pos = p;
			pf::char32_to_utf8(std::back_inserter(out), cp);
			return true;
		}
	}
	else
	{
		size_t p = m_pos;
		while (p < m_src.size() && isalnum(static_cast<unsigned char>(m_src[p]))) ++p;

		if (p > m_pos)
		{
			if (const uint32_t cp = lookup_entity(m_src.substr(m_pos, p - m_pos)))
			{
				if (p < m_src.size() && m_src[p] == ';') ++p;
				m_pos = p;
				pf::char32_to_utf8(std::back_inserter(out), cp);
				return true;
			}
		}
	}

	// Not a recognised reference — emit the ampersand literally.
	out += '&';
	m_pos = start;
	return false;
}

token_type html_scanner::scan_text()
{
	if (at_end()) return TT_EOF;

	const char c0 = m_src[m_pos];

	if (c0 == '<')
	{
		++m_pos;
		return scan_tag();
	}

	if (c0 != '&' && is_ws(c0))
	{
		const size_t ws_start = m_pos;
		skip_ws();
		m_value = m_src.substr(ws_start, m_pos - ws_start);
		return TT_SPACE;
	}

	// A run of non-space text. Only the first character may be an entity —
	// the run always terminates at the next '&' — so the decode buffer is
	// needed at most once per token.
	bool decoded = false;

	if (c0 == '&')
	{
		m_decoded.clear();
		++m_pos;
		decode_entity(m_decoded);
		decoded = true;
	}

	const size_t start = m_pos;

	while (m_pos < m_src.size())
	{
		const char c = m_src[m_pos];
		if (c == '<' || c == '&' || is_ws(c)) break;

		const size_t n = seq_len(m_pos);
		const uint32_t cp = n > 1 ? codepoint_at(m_pos) : static_cast<uint8_t>(c);
		m_pos += n;

		// One token per CJK codepoint so lines can break between them.
		if (cp >= 0x4E00 && cp <= 0x9FCC) break;
	}

	if (decoded)
	{
		m_decoded.append(m_src.substr(start, m_pos - start));
		m_value = m_decoded;
	}
	else
	{
		m_value = m_src.substr(start, m_pos - start);
	}

	return TT_WORD;
}

// '<' has already been consumed.
token_type html_scanner::scan_tag()
{
	const bool is_tail = peek() == '/';
	if (is_tail) ++m_pos;

	if (!is_tail)
	{
		if (starts_ci(m_pos, "!--"))
		{
			m_pos += 3;
			m_close = "-->";
			m_end_token = TT_COMMENT_END;
			m_scan = &html_scanner::scan_delimited;
			return TT_COMMENT_START;
		}
		if (starts_ci(m_pos, "![CDATA["))
		{
			m_pos += 8;
			m_close = "]]>";
			m_end_token = TT_CDATA_END;
			m_scan = &html_scanner::scan_delimited;
			return TT_CDATA_START;
		}
		if (starts_ci(m_pos, "!DOCTYPE"))
		{
			m_pos += 8;
			m_end_token = TT_DOCTYPE_END;
			m_scan = &html_scanner::scan_markup_decl;
			return TT_DOCTYPE_START;
		}
		if (starts_ci(m_pos, "!ENTITY"))
		{
			m_pos += 7;
			m_end_token = TT_ENTITY_END;
			m_scan = &html_scanner::scan_markup_decl;
			return TT_ENTITY_START;
		}
		if (peek() == '?')
		{
			++m_pos;
			m_close = "?>";
			m_end_token = TT_PI_END;
			m_scan = &html_scanner::scan_delimited;
			return TT_PI_START;
		}
	}

	m_tag_store.clear();

	while (!at_end())
	{
		const char c = m_src[m_pos];
		if (is_ws(c) || c == '/' || c == '>') break;
		m_tag_store += lower(c);
		++m_pos;
	}

	m_tag_name = m_tag_store;

	if (is_tail)
	{
		while (!at_end() && m_src[m_pos] != '>') ++m_pos;
		if (!at_end()) ++m_pos;
		m_scan = &html_scanner::scan_text;
		return TT_TAG_END;
	}

	m_scan = &html_scanner::scan_attributes;
	return TT_TAG_START;
}

token_type html_scanner::enter_content()
{
	// Raw text elements: everything up to the matching close tag is data.
	if (m_tag_store == "script" || m_tag_store == "style")
	{
		m_scan = &html_scanner::scan_raw_text;
		return scan_raw_text();
	}

	m_scan = &html_scanner::scan_text;
	return scan_text();
}

void html_scanner::scan_attr_value()
{
	const char q = peek();
	const bool quoted = q == '"' || q == '\'';
	if (quoted) ++m_pos;

	const size_t start = m_pos;
	size_t copied = start;
	bool decoded = false;

	while (!at_end())
	{
		const char c = m_src[m_pos];
		if (quoted ? c == q : is_ws(c) || c == '>') break;

		if (c == '&')
		{
			if (!decoded)
			{
				m_decoded.clear();
				decoded = true;
			}
			m_decoded.append(m_src.substr(copied, m_pos - copied));
			++m_pos;
			decode_entity(m_decoded);
			copied = m_pos;
			continue;
		}

		++m_pos;
	}

	if (decoded)
	{
		m_decoded.append(m_src.substr(copied, m_pos - copied));
		m_value = m_decoded;
	}
	else
	{
		m_value = m_src.substr(start, m_pos - start);
	}

	if (quoted && peek() == q) ++m_pos;
}

token_type html_scanner::scan_attributes()
{
	for (;;)
	{
		skip_ws();
		if (at_end()) return TT_EOF;

		const char c = m_src[m_pos];

		if (c == '>')
		{
			++m_pos;
			return enter_content();
		}

		if (c == '/')
		{
			++m_pos;
			skip_ws();
			if (peek() == '>')
			{
				++m_pos;
				m_scan = &html_scanner::scan_text;
				return TT_TAG_END_EMPTY;
			}
			continue;
		}

		if (c == '=' || c == '<')
		{
			++m_pos; // stray delimiter — resync
			continue;
		}

		m_attr_store.clear();
		while (!at_end())
		{
			const char n = m_src[m_pos];
			if (is_ws(n) || n == '=' || n == '>' || n == '/') break;
			m_attr_store += lower(n);
			++m_pos;
		}
		m_attr_name = m_attr_store;
		m_value = {};

		const size_t after_name = m_pos;
		skip_ws();

		if (peek() != '=')
		{
			m_pos = after_name; // valueless attribute
			return TT_ATTR;
		}

		++m_pos;
		skip_ws();
		scan_attr_value();
		return TT_ATTR;
	}
}

token_type html_scanner::scan_raw_text()
{
	if (m_got_tail)
	{
		m_got_tail = false;
		m_scan = &html_scanner::scan_text;
		return TT_TAG_END;
	}

	const std::string close = "</" + m_tag_store;
	const size_t at = find_ci(close, m_pos);

	m_value = m_src.substr(m_pos, (at == std::string_view::npos ? m_src.size() : at) - m_pos);

	if (at == std::string_view::npos)
	{
		m_pos = m_src.size();
	}
	else
	{
		m_pos = at + close.size();
		while (!at_end() && m_src[m_pos] != '>') ++m_pos;
		if (!at_end()) ++m_pos;
	}

	m_got_tail = true;
	return TT_DATA;
}

token_type html_scanner::scan_delimited()
{
	if (m_got_tail)
	{
		m_got_tail = false;
		m_scan = &html_scanner::scan_text;
		return m_end_token;
	}

	const size_t at = m_src.find(m_close, m_pos);

	m_value = m_src.substr(m_pos, (at == std::string_view::npos ? m_src.size() : at) - m_pos);
	m_pos = at == std::string_view::npos ? m_src.size() : at + m_close.size();
	m_got_tail = true;
	return TT_DATA;
}

token_type html_scanner::scan_markup_decl()
{
	if (m_got_tail)
	{
		m_got_tail = false;
		m_scan = &html_scanner::scan_text;
		return m_end_token;
	}

	const size_t start = m_pos;
	bool in_quote = false;

	while (!at_end())
	{
		const char c = m_src[m_pos];
		if (c == '"') in_quote = !in_quote;
		else if (c == '>' && !in_quote) break;
		++m_pos;
	}

	m_value = m_src.substr(start, m_pos - start);
	if (!at_end()) ++m_pos;
	m_got_tail = true;
	return TT_DATA;
}

// Render a token stream as a compact string so expectations stay readable.
static std::string dump_tokens(const std::string_view html)
{
	html_scanner sc(html);
	std::string out;

	for (;;)
	{
		const token_type t = sc.get_token();
		if (t == TT_EOF) break;

		if (!out.empty()) out += ' ';

		switch (t)
		{
		case TT_TAG_START: out += "<" + std::string(sc.get_tag_name());
			break;
		case TT_TAG_END: out += "</" + std::string(sc.get_tag_name());
			break;
		case TT_TAG_END_EMPTY: out += "/>";
			break;
		case TT_ATTR: out += std::string(sc.get_attr_name()) + "=" + std::string(sc.get_value());
			break;
		case TT_WORD: out += "w:" + std::string(sc.get_value());
			break;
		case TT_SPACE: out += "_";
			break;
		case TT_DATA: out += "d:" + std::string(sc.get_value());
			break;
		case TT_COMMENT_START: out += "<!--";
			break;
		case TT_COMMENT_END: out += "-->";
			break;
		case TT_CDATA_START: out += "<![";
			break;
		case TT_CDATA_END: out += "]]";
			break;
		case TT_DOCTYPE_START: out += "<!doctype";
			break;
		case TT_DOCTYPE_END: out += "doctype>";
			break;
		case TT_PI_START: out += "<?";
			break;
		case TT_PI_END: out += "?>";
			break;
		default: out += "?";
			break;
		}
	}

	return out;
}

static void should_scan_tags_and_attributes()
{
	should::equal("<a href=/x?a=1&b=2 disabled= w:hi </a",
	              dump_tokens(R"(<A HREF="/x?a=1&amp;b=2" DISABLED>hi</A>)").c_str());
}

static void should_scan_unquoted_and_empty_attributes()
{
	should::equal("<img src=a.png alt= /> w:t",
	              dump_tokens("<img src=a.png alt=\"\" />t").c_str());
}

static void should_decode_entities()
{
	// Named, decimal, hex (astral plane) and an unterminated reference. A word
	// run always ends at '&', so each reference starts a fresh token.
	should::equal("w:& w:< w:> _ w:A _ w:\xF0\x9F\x98\x80 _ w:&notanentity",
	              dump_tokens("&amp;&lt;&gt; &#65; &#x1F600; &notanentity").c_str());
}

static void should_scan_raw_text_elements()
{
	// The content of <style> must survive verbatim, including '<' and entities.
	should::equal("<style d:a{content:\"<b>&amp;\"} </style _ <p w:x </p",
	              dump_tokens("<style>a{content:\"<b>&amp;\"}</style> <p>x</p>").c_str());
}

static void should_scan_comments_and_doctype()
{
	should::equal("<!doctype d: html doctype> _ <!-- d: c  --> _ w:x",
	              dump_tokens("<!DOCTYPE html> <!-- c --> x").c_str());
}

static void should_split_cjk_words()
{
	// One token per ideograph so lines may break between them.
	should::equal("w:\xE6\x97\xA5 w:\xE6\x9C\xAC w:ab", dump_tokens("\xE6\x97\xA5\xE6\x9C\xAC" "ab").c_str());
}

static void should_terminate_on_malformed_markup()
{
	// Unclosed tag, unterminated comment and unterminated raw text must all
	// reach EOF with their regions implicitly closed.
	should::equal("<div class=x", dump_tokens("<div class=x").c_str());
	should::equal("<!-- d: never closed -->", dump_tokens("<!-- never closed").c_str());
	should::equal("<style d:oops </style", dump_tokens("<style>oops").c_str());
}

static void should_detect_charset()
{
	// Declared windows-1252 in a meta tag: 0x93/0x94 are curly quotes.
	const std::string src = "<meta charset=\"windows-1252\"><p>\x93hi\x94";
	should::equal("<meta charset=windows-1252 <p w:\xE2\x80\x9Chi\xE2\x80\x9D",
	              dump_tokens(decode_to_utf8(src, "")).c_str());

	// A UTF-8 BOM is stripped, and a header charset wins over sniffing.
	should::equal("a", decode_to_utf8("\xEF\xBB\xBF" "a", "").c_str());
	should::equal("\xC2\xA9", decode_to_utf8("\xA9", "text/html; charset=iso-8859-1").c_str());
}

void register_scanner_tests(tests& t)
{
	t.register_test("Scanner: tags and attributes", should_scan_tags_and_attributes);
	t.register_test("Scanner: unquoted and empty attributes", should_scan_unquoted_and_empty_attributes);
	t.register_test("Scanner: entity decoding", should_decode_entities);
	t.register_test("Scanner: raw text elements", should_scan_raw_text_elements);
	t.register_test("Scanner: comments and doctype", should_scan_comments_and_doctype);
	t.register_test("Scanner: CJK word splitting", should_split_cjk_words);
	t.register_test("Scanner: malformed markup terminates", should_terminate_on_malformed_markup);
	t.register_test("Charset detection", should_detect_charset);
}


// A view that accepts everything and shows nothing, so a document can be laid
// out with no window behind it.
namespace
{
	struct silent_view final : view_host
	{
		bool verbose = false;

		void layout() override
		{
		}

		void invalidate() override
		{
		}

		void open(const std::string&) override
		{
		}

		void diagnostic(const std::string& message) override
		{
			if (verbose) pf::write_stdout("  " + message + "\n");
		}

		void resource_started(const std::string& type, const std::string& url) override
		{
			if (verbose) pf::write_stdout(std::format("  {} requested: {}\n", type, url));
		}

		void resource_finished(const std::string& type, const std::string& url, const bool ok) override
		{
			if (verbose) pf::write_stdout(std::format("  {} {}: {}\n", type, ok ? "loaded" : "failed", url));
		}
	};
}

// Runs parse -> cascade -> layout with no window and no message loop, so no
// async stylesheet or image ever lands. Same input therefore gives same output.
layout_result layout_html_headless(const std::string& html, const int width, const int height,
								   const bool verbose, const int dump_depth, const bool dump_json)
{
	layout_result result;
	silent_view view;
	view.verbose = verbose;

	const auto t0 = std::chrono::steady_clock::now();
	const auto doc = document::create_from_bytes(view, "https://example.invalid/", html, "text/html");
	const auto t1 = std::chrono::steady_clock::now();

	if (!doc) return result;

	const position client_pos(0, 0, width, height);
	doc->client_pos(client_pos);
	doc->render(width);
	const auto t2 = std::chrono::steady_clock::now();

	result.width = doc->width();
	result.height = doc->height();
	result.parse_style_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
	result.layout_us = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
	result.stats = doc->analyse_layout(&result.anomalies);
	if (dump_depth > 0) result.box_dump = doc->dump_boxes(dump_depth);
	if (dump_json) result.layout_json = doc->dump_layout_json();
	return result;
}

// Lays out a snippet and returns the box of the element with the given id, in
// document coordinates. An empty box means the id was not found.
position layout_html_headless_probe(const std::string& html, const int width, const std::string& id)
{
	silent_view view;
	const auto doc = document::create_from_bytes(view, "https://example.invalid/", html, "text/html");
	if (!doc) return {};

	doc->client_pos(position(0, 0, width, 896));
	doc->render(width);

	position found;
	std::function<bool(const element*, int, int)> visit =
		[&](const element* el, const int parent_x, const int parent_y)
	{
		const int x = parent_x + el->left();
		const int y = parent_y + el->top();

		if (el->get_attr("id") == id)
		{
			found = position(x, y, el->width(), el->height());
			return true;
		}

		for (size_t i = 0; i < el->get_children_count(); ++i)
			if (visit(el->get_child(static_cast<int>(i)), x, y)) return true;

		return false;
	};

	visit(doc->root(), 0, 0);
	return found;
}


// Layout regressions on real pages. The fixtures are large and live outside the
// source tree, so a machine without them runs the rest of the suite instead.
// These sizes are a change detector, not a statement that the page is correct.
static void should_lay_out_fixture(const char* name, const int expect_w, const int expect_h)
{
	const auto html = get_file_contents(std::string("test-files/") + name);
	if (html.empty()) return;

	const auto r = layout_html_headless(html, 1902, 896);
	should::equal(expect_w, r.width, (std::string(name) + " width").c_str());
	should::equal(expect_h, r.height, (std::string(name) + " height").c_str());
}

// Lays out a self-contained snippet at a fixed width and returns the box of the
// first element carrying the given id.
static position should_box_of(const std::string& html, const char* id, const int width = 1000)
{
	return layout_html_headless_probe(html, width, id);
}

void register_layout_tests(tests& t)
{
	t.register_test("Layout: repeated render is stable", []
	{
		const auto html = get_file_contents("test-files/site-elements.html");
		if (html.empty()) return;

		silent_view view;
		const auto doc = document::create_from_bytes(view, "https://example.invalid/", html, "text/html");
		should::EqualTrue(doc != nullptr, "fixture document");
		doc->client_pos(position(0, 0, 1888, 871));
		doc->render(1902);
		doc->render(1888);
		const int first_height = doc->height();
		const auto probe_box = [&](const std::string_view key)
		{
			position result;
			bool found = false;
			std::function<void(const element*, int, int)> visit = [&](const element* el, const int parent_x,
			                                                          const int parent_y)
			{
				if (found) return;
				const int x = parent_x + el->left();
				const int y = parent_y + el->top();
				if (el->get_attr("data-probe") == key)
				{
					result = position(x, y, el->width(), el->height());
					found = true;
					return;
				}
				for (size_t index = 0; index < el->get_children_count(); ++index)
					visit(el->get_child(static_cast<int>(index)), x, y);
			};
			visit(doc->root(), 0, 0);
			return result;
		};
		const std::vector<position> first_sections = {
			probe_box("page"), probe_box("search-section"), probe_box("news-section"),
			probe_box("gallery-section"), probe_box("article-section"), probe_box("footer")
		};
		doc->render(1902);
		doc->render(1888);
		const std::vector<position> second_sections = {
			probe_box("page"), probe_box("search-section"), probe_box("news-section"),
			probe_box("gallery-section"), probe_box("article-section"), probe_box("footer")
		};
		should::equal(first_sections[0].height, second_sections[0].height, "repeated page height");
		should::equal(first_sections[1].y, second_sections[1].y, "repeated search y");
		should::equal(first_sections[2].y, second_sections[2].y, "repeated news y");
		should::equal(first_sections[3].y, second_sections[3].y, "repeated gallery y");
		should::equal(first_sections[4].y, second_sections[4].y, "repeated article y");
		should::equal(first_sections[5].y, second_sections[5].y, "repeated footer y");
		should::equal(first_height, doc->height(), "repeated document height");
	});

	t.register_test("Layout: wikipedia main page", []
	{
		should_lay_out_fixture("wikipedia-main-page.html", 1902, 14921);
	});
	t.register_test("Layout: wikipedia web browser", []
	{
		should_lay_out_fixture("wikipedia-web-browser.html", 2065, 15126);
	});
	t.register_test("Layout: wikipedia comparison table", []
	{
		should_lay_out_fixture("wikipedia-comparison-of-web-browsers.html", 2065, 36898);
	});
	t.register_test("Layout: bbc news", []
	{
		should_lay_out_fixture("bbc-news.html", 1904, 5369);
	});

	// Selector bucketing reads id/class before parse_styles runs, so these
	// rules were silently dropped on the first cascade.
	t.register_test("Style: class selector applies to first layout", []
	{
		const auto box = should_box_of(
			"<html><head><style>.w{width:300px;height:40px}</style></head>"
			"<body><div id='t' class='w'>x</div></body></html>", "t");
		should::equal(300, box.width, "class selector width");
		should::equal(40, box.height, "class selector height");
	});
	t.register_test("Style: id selector applies to first layout", []
	{
		const auto box = should_box_of(
			"<html><head><style>#t{width:250px;height:40px}</style></head>"
			"<body><div id='t'>x</div></body></html>", "t");
		should::equal(250, box.width, "id selector width");
		should::equal(40, box.height, "id selector height");
	});

	// calc() carries its own parts and never sets units, so cvt_units used to
	// read an unset value and collapse the box to nothing.
	t.register_test("Style: calc max-width constrains rather than collapses", []
	{
		const auto box = should_box_of(
			"<html><head><style>#h{width:1000px}#t{width:100%;max-width:calc(100% - 32px)}</style></head>"
			"<body><div id='h'><div id='t'>x</div></div></body></html>", "t");
		should::equal(968, box.width, "calc max-width");
	});
	t.register_test("Style: calc max-width above available width does not bind", []
	{
		const auto box = should_box_of(
			"<html><head><style>#h{width:1000px}#t{width:100%;max-width:calc(1280px - 32px)}</style></head>"
			"<body><div id='h'><div id='t'>x</div></div></body></html>", "t");
		should::equal(1000, box.width, "calc max-width slack");
	});

	// Grid and flex items are blockified, so an inline value on a child must not
	// shrink it to nothing.
	t.register_test("Style: grid items are blockified", []
	{
		const auto box = should_box_of(
			"<html><head><style>#h{width:600px;display:grid}#t{display:initial}</style></head>"
			"<body><div id='h'><div id='t'>x</div></div></body></html>", "t");
		should::equal(600, box.width, "grid item width");
	});

	t.register_test("Layout: flex auto basis uses intrinsic width", []
	{
		const auto box = should_box_of(
			"<html><head><style>#h{display:flex;width:600px}#t{font-size:20px;line-height:30px}</style></head>"
			"<body><div id='h'><div id='t'>brand words</div></div></body></html>", "t");
		should::EqualTrue(box.width > 0, "auto flex item width");
		should::equal(30, box.height, "auto flex item stays on one line");
	});
	t.register_test("Layout: nested flex reports intrinsic width", []
	{
		const auto box = should_box_of(
			"<html><head><style>#h,#t{display:flex}#h{width:600px}</style></head>"
			"<body><div id='h'><div id='t'><span>one</span><span>two</span></div></div></body></html>", "t");
		should::EqualTrue(box.width > 0, "nested flex item width");
	});
	t.register_test("Layout: flex ignores formatting whitespace", []
	{
		const auto compact = should_box_of(
			"<html><head><style>#h{display:flex;gap:16px;width:600px}</style></head>"
			"<body><div id='h'><div>one</div><div id='t'>two</div></div></body></html>", "t");
		const auto formatted = should_box_of(
			"<html><head><style>#h{display:flex;gap:16px;width:600px}</style></head>"
			"<body><div id='h'>\n  <div>one</div>\n  <div id='t'>two</div>\n</div></body></html>", "t");
		should::equal(compact.x, formatted.x, "formatted flex item x");
	});
	t.register_test("Layout: flex shrink preserves automatic content minimum", []
	{
		const auto box = should_box_of(
			"<html><head><style>#h{display:flex;width:200px}#a{width:100%;min-width:0}#t{padding:0 20px}</style></head>"
			"<body><div id='h'><div id='a'>input</div><button id='t'>Search</button></div></body></html>", "t");
		should::EqualTrue(box.width > 40, "flex item automatic minimum");
	});
	t.register_test("Layout: flex cross size counts padding once", []
	{
		const auto box = should_box_of(
			"<html><head><style>#h{display:flex}#t{line-height:18px;padding:8px 18px;border:1px solid}</style></head>"
			"<body><div id='h'><button id='t'>Search</button></div></body></html>", "t");
		should::equal(36, box.height, "padded flex item height");
	});
	t.register_test("Style: font inherit shorthand includes line height", []
	{
		const auto box = should_box_of(
			"<html><head><style>#h{font-size:16px;line-height:24px}#t{display:inline-block;font:inherit;padding:8px;border:1px solid}</style></head>"
			"<body><div id='h'><button id='t'>Search</button></div></body></html>", "t");
		should::equal(42, box.height, "inherited font line height");
	});
	t.register_test("Layout: padded input sets flex line height", []
	{
		const auto box = should_box_of(
			"<html><head><style>#h{display:flex;font-size:16px;line-height:24px}#h>*{font:inherit;border:1px solid}"
			"#i{padding:10px 16px}#b{padding:8px 18px}</style></head>"
			"<body><div id='h'><input id='i'><button id='b'>Search</button></div></body></html>", "i");
		should::equal(46, box.height, "padded input flex height");
	});
	t.register_test("Layout: flex item content width excludes edges once", []
	{
		const auto box = should_box_of(
			"<html><head><style>#h{display:flex}#b{font-size:16px;line-height:24px;padding:8px 13px;border:1px solid}</style></head>"
			"<body><div id='h'><button id='b'>Potato Search</button></div></body></html>", "b");
		should::equal(42, box.height, "single-line padded flex item");
	});
	t.register_test("Layout: percentage flex basis controls wrapping", []
	{
		const std::string html =
			"<html><head><style>#h{display:flex;flex-wrap:wrap;width:300px;gap:18px}#a{flex:1 1 58%}#b{flex:1 1 42%}</style></head>"
			"<body><div id='h'><div id='a'>A</div><div id='b'>B</div></div></body></html>";
		const auto first = should_box_of(html, "a");
		const auto second = should_box_of(html, "b");
		should::EqualTrue(second.y > first.y, "percentage bases wrap with gap");
	});
	t.register_test("Layout: flex item contains child margins", []
	{
		const auto box = should_box_of(
			"<html><head><style>#h{display:flex}#c{height:20px;margin:0 0 18px}</style></head>"
			"<body><div id='h'><div id='i'><div id='c'></div></div></div></body></html>", "i");
		should::equal(38, box.height, "flex item child bottom margin");
	});
	t.register_test("Layout: bordered sibling margins collapse", []
	{
		const std::string html =
			"<html><head><style>#a{height:20px;border-bottom:1px solid;margin-bottom:10px}#b{height:20px;margin-top:16px}</style></head>"
			"<body><div id='a'>A</div><div id='b'>B</div></body></html>";
		const auto first = should_box_of(html, "a");
		const auto second = should_box_of(html, "b");
		should::equal(37, second.y + 16 - first.y, "bordered sibling margin gap");
	});

	// An <img> is sized from CSS even before any bitmap arrives.
	t.register_test("Layout: image honours css size without a bitmap", []
	{
		const auto box = should_box_of(
			"<html><head><style>#h{width:800px}#t{width:50%;height:60px}</style></head>"
			"<body><div id='h'><img id='t' src='never.png'></div></body></html>", "t");
		should::equal(400, box.width, "image width");
		should::equal(60, box.height, "image height");
	});

	// A shorthand value splits on top-level separators only. Splitting inside
	// the parentheses turned "calc(20px + 10px) 0" into four bogus values, one
	// of which parsed as an auto margin.
	t.register_test("Style: shorthand keeps calc() intact", []
	{
		const auto box = should_box_of(
			"<html><head><style>#t{width:100px;height:20px;padding:calc(20px + 10px) 0}</style></head>"
			"<body><div id='t'></div></body></html>", "t");
		should::equal(100, box.width, "padded width");
		should::equal(80, box.height, "padded height");

		const auto margined = should_box_of(
			"<html><head><style>#t{width:100px;height:20px;margin:calc(5px + 5px) 0}</style></head>"
			"<body><div id='t'></div></body></html>", "t");
		should::equal(100, margined.width, "margined width");
		should::equal(40, margined.height, "margined height");
	});

	// A declaration separator inside url() is part of the value, not the end
	// of the declaration.
	t.register_test("Style: data url keeps the declaration intact", []
	{
		const auto box = should_box_of(
			"<html><head><style>#t{background:url(data:image/gif;base64,AA==) no-repeat;"
			"width:100px;height:20px}</style></head>"
			"<body><div id='t'></div></body></html>", "t");
		should::equal(100, box.width, "width survives the data url");
		should::equal(20, box.height, "height survives the data url");
	});

	// :nth-child(An+B), including a negative step, used to assert and return
	// nonsense rather than select anything.
	t.register_test("Style: nth-child selects An+B", []
	{
		const std::string html =
			"<html><head><style>li{height:10px}li:nth-child(2n+1){height:40px}</style></head>"
			"<body><ul><li id='a'></li><li id='b'></li><li id='c'></li></ul></body></html>";
		should::equal(40, should_box_of(html, "a").height, "first matches 2n+1");
		should::equal(10, should_box_of(html, "b").height, "second does not");
		should::equal(40, should_box_of(html, "c").height, "third matches 2n+1");

		const std::string negative =
			"<html><head><style>li{height:10px}li:nth-child(-n+2){height:40px}</style></head>"
			"<body><ul><li id='a'></li><li id='b'></li><li id='c'></li></ul></body></html>";
		should::equal(40, should_box_of(negative, "a").height, "first matches -n+2");
		should::equal(40, should_box_of(negative, "b").height, "second matches -n+2");
		should::equal(10, should_box_of(negative, "c").height, "third does not");
	});

	// A compound media query has to evaluate every expression, not just the
	// first one it can parse.
	t.register_test("Style: compound media query evaluates both terms", []
	{
		const std::string html =
			"<html><head><style>#t{width:100px;height:20px}"
			"@media screen and (min-width:5000px){#t{height:400px}}</style></head>"
			"<body><div id='t'></div></body></html>";
		should::equal(20, should_box_of(html, "t", 1000).height, "unmatched min-width suppresses the rule");
	});

	// A combinator is only a combinator between two compounds; whitespace
	// around it must not change which compound is the subject.
	t.register_test("Style: spaced combinators bind the same as tight ones", []
	{
		const std::string spaced =
			"<html><head><style>p{margin:0;height:5px}div > p{height:40px}</style></head>"
			"<body><div><p id='t'></p></div><p id='u'></p></body></html>";
		should::equal(40, should_box_of(spaced, "t").height, "child of div matches");
		should::equal(5, should_box_of(spaced, "u").height, "sibling of div does not");

		const std::string sibling =
			"<html><head><style>p{margin:0;height:5px}#a + p{height:40px}</style></head>"
			"<body><p id='a'></p><p id='b'></p><p id='c'></p></body></html>";
		should::equal(40, should_box_of(sibling, "b").height, "adjacent sibling matches");
		should::equal(5, should_box_of(sibling, "c").height, "next-but-one does not");
	});
}


std::unique_ptr<element> parser::create_element(const std::string_view tag_name)
{
	std::unique_ptr<element> newTag;

	if (!newTag)
	{
		if (is_equal(tag_name, "br"))
		{
			newTag = std::make_unique<element>(m_doc, el_break);
		}
		else if (is_equal(tag_name, "p"))
		{
			newTag = std::make_unique<element>(m_doc, el_para);
		}
		else if (is_equal(tag_name, "img"))
		{
			newTag = std::make_unique<element>(m_doc, el_image);
		}
		else if (is_equal(tag_name, "table"))
		{
			newTag = std::make_unique<element>(m_doc, el_table);
		}
		else if (is_equal(tag_name, "td") || is_equal(tag_name, "th"))
		{
			newTag = std::make_unique<element>(m_doc, el_td);
		}
		else if (is_equal(tag_name, "link"))
		{
			newTag = std::make_unique<element>(m_doc, el_link);
		}
		else if (is_equal(tag_name, "title"))
		{
			newTag = std::make_unique<element>(m_doc, el_title);
		}
		else if (is_equal(tag_name, "a"))
		{
			newTag = std::make_unique<element>(m_doc, el_anchor);
		}
		else if (is_equal(tag_name, "tr"))
		{
			newTag = std::make_unique<element>(m_doc, el_tr);
		}
		else if (is_equal(tag_name, "style"))
		{
			newTag = std::make_unique<element>(m_doc, el_style);
		}
		else if (is_equal(tag_name, "base"))
		{
			newTag = std::make_unique<element>(m_doc, el_base);
		}
		else if (is_equal(tag_name, "body"))
		{
			newTag = std::make_unique<element>(m_doc, el_body);
		}
		else if (is_equal(tag_name, "div"))
		{
			newTag = std::make_unique<element>(m_doc, el_div);
		}
		else if (is_equal(tag_name, "script"))
		{
			newTag = std::make_unique<element>(m_doc, el_script);
		}
		else if (is_equal(tag_name, "font"))
		{
			newTag = std::make_unique<element>(m_doc, el_font);
		}
		else if (is_equal(tag_name, "svg"))
		{
			newTag = std::make_unique<element>(m_doc, el_svg);
		}
		else
		{
			newTag = std::make_unique<element>(m_doc, el_html);
		}
	}

	if (newTag)
	{
		newTag->set_tag_name(tag_name);
	}

	return newTag;
}

void parser::parse_tag_start(const std::string_view tag_name)
{
	parse_pop_void_element();

	// We add the html(root) element before parsing
	if (is_equal(tag_name, "html"))
	{
		return;
	}

	auto el = create_element(tag_name);

	if (el)
	{
		if (!m_parse_stack.empty() && m_parse_stack.back()->get_tag_name() == "html")
		{
			// if last element is root we have to add head or body
			if (!value_in_list(tag_name, "head;body"))
			{
				parse_push_element(create_element("body"));
			}
		}

		parse_close_omitted_end(tag_name);
		parse_open_omitted_start(tag_name);
		parse_push_element(std::move(el));
	}
}


void parser::parse_tag_end(const std::string_view tag_name)
{
	if (!m_parse_stack.empty())
	{
		if (m_parse_stack.back()->get_tag_name() == tag_name)
		{
			parse_pop_element();
		}
		else
		{
			auto stop_tag = "";

			for (int i = 0; m_stop_tags[i].tags; i++)
			{
				if (value_in_list(tag_name, m_stop_tags[i].tags))
				{
					stop_tag = m_stop_tags[i].stop_parent;
					break;
				}
			}
			parse_pop_element(tag_name, stop_tag);
		}
	}
}


void parser::parse_push_element(std::unique_ptr<element> el)
{
	if (!m_parse_stack.empty())
	{
		const auto raw = el.get();

		if (auto refused = m_parse_stack.back()->append_child(std::move(el)))
		{
			m_detached.push_back(std::move(refused));
		}

		m_parse_stack.push_back(raw);
	}
}

void parser::parse_attribute(const std::string_view attr_name, const std::string_view attr_value)
{
	if (!m_parse_stack.empty())
	{
		m_parse_stack.back()->set_attr(attr_name, attr_value);
	}
}

void parser::parse_word(const std::string_view val)
{
	if (m_parse_stack.empty()) return;

	if (m_parse_stack.back()->get_tag_name() == "html")
	{
		parse_push_element(create_element("body"));
	}

	parse_pop_void_element();

	if (!m_parse_stack.empty())
	{
		m_parse_stack.back()->append_text(val);
	}
}

void parser::parse_space(const std::string_view val)
{
	parse_pop_void_element();
	if (!m_parse_stack.empty())
	{
		m_parse_stack.back()->append_space(val);
	}
}

void parser::parse_comment_start()
{
	parse_pop_void_element();
	parse_push_element(std::make_unique<element>(m_doc, el_comment));
}

void parser::parse_comment_end()
{
	parse_pop_element();
}

void parser::parse_cdata_start()
{
	parse_pop_void_element();
	parse_push_element(std::make_unique<element>(m_doc, el_cdata));
}

void parser::parse_cdata_end()
{
	parse_pop_element();
}

void parser::parse_data(const std::string_view val)
{
	if (!m_parse_stack.empty())
	{
		m_parse_stack.back()->set_data(val);
	}
}

bool parser::parse_pop_element()
{
	if (!m_parse_stack.empty())
	{
		m_parse_stack.pop_back();
		return true;
	}
	return false;
}

bool parser::parse_pop_element(const std::string_view tag, const char* stop_tags)
{
	bool found = false;
	for (auto iel = m_parse_stack.rbegin(); iel != m_parse_stack.rend(); ++iel)
	{
		if ((*iel)->get_tag_name() == tag)
		{
			found = true;
			break;
		}
		if (value_in_list((*iel)->get_tag_name(), stop_tags)) break;
	}

	if (!found) return false;

	while (found)
	{
		if (m_parse_stack.back()->get_tag_name() == tag)
		{
			found = false;
		}
		parse_pop_element();
	}
	return true;
}

void parser::parse_pop_void_element()
{
	if (!m_parse_stack.empty())
	{
		if (value_in_list(m_parse_stack.back()->get_tag_name(), void_elements))
		{
			parse_pop_element();
		}
	}
}

void parser::parse_pop_to_parent(const char* parents, const char* stop_parent)
{
	std::vector<element*>::size_type parent = 0;
	bool found = false;
	auto p = split_string(parents, ';');

	for (int i = static_cast<int>(m_parse_stack.size()) - 1; i >= 0 && !found; i--)
	{
		if (std::find(p.begin(), p.end(), m_parse_stack[i]->get_tag_name()) != p.end())
		{
			found = true;
			parent = i;
			break;
		}
		if (m_parse_stack[i]->get_tag_name() == stop_parent)
		{
			break;
		}
	}
	if (found)
	{
		m_parse_stack.erase(m_parse_stack.begin() + parent + 1, m_parse_stack.end());
	}
	else
	{
		parse_tag_start(p.front());
	}
}

void parser::parse_close_omitted_end(const std::string_view tag)
{
	if (m_parse_stack.empty()) return;

	for (int i = 0; m_omitted_end_tags[i].tag; i++)
	{
		if (m_parse_stack.back()->get_tag_name() == m_omitted_end_tags[i].tag)
		{
			if (value_in_list(tag, m_omitted_end_tags[i].followed_tags))
			{
				parse_pop_element();
				break;
			}
		}
	}
}

void parser::parse_open_omitted_start(const std::string_view tag)
{
	if (is_equal(tag, "col"))
	{
		if (m_parse_stack.back()->get_tag_name() != "colgroup")
		{
			parse_tag_start("colgroup");
		}
	}
	else if (is_equal(tag, "tr"))
	{
		if (m_parse_stack.back()->get_tag_name() != "tbody" &&
			m_parse_stack.back()->get_tag_name() != "thead" &&
			m_parse_stack.back()->get_tag_name() != "tfoot")
		{
			parse_tag_start("tbody");
		}
	}
}


document::document(view_host& v) : m_view(v), m_over_element(nullptr)
{
	m_http.open("potato/1.0");
}

document::~document()
{
	m_http.stop();
	clear();

	std::lock_guard lock(m_fonts_mutex);
	for (auto& [key, fi] : m_fonts)
	{
		if (fi.font)
		{
			pf::delete_font_handle(fi.font);
			fi.font = 0;
		}
	}
}

void document::clear()
{
	m_root.reset();
	m_over_element = nullptr;

	m_styles.clear();
	m_fixed_boxes.clear();
	m_media_lists.clear();
	m_images.clear();
}

void document::load_master_stylesheet(const std::string& text)
{
	auto media_list = media_query_list::create_from_string("screen");
	m_styles.parse_stylesheet(text, empty, *this, media_list);
}

static void parse_stream(html_scanner& sc, parser& par)
{
	token_type t;

	while ((t = sc.get_token()) != TT_EOF && !par.is_stack_empty())
	{
		switch (t)
		{
		case TT_CDATA_START:
			par.parse_cdata_start();
			break;
		case TT_CDATA_END:
			par.parse_cdata_end();
			break;
		case TT_COMMENT_START:
			par.parse_comment_start();
			break;
		case TT_COMMENT_END:
			par.parse_comment_end();
			break;
		case TT_DATA:
			par.parse_data(sc.get_value());
			break;
		case TT_TAG_START:
			// Markup declarations we do not model arrive here as "!name".
			if (!sc.get_tag_name().empty() && sc.get_tag_name().front() != '!')
			{
				par.parse_tag_start(sc.get_tag_name());
			}
			break;
		case TT_TAG_END_EMPTY:
		case TT_TAG_END:
			par.parse_tag_end(sc.get_tag_name());
			break;
		case TT_ATTR:
			par.parse_attribute(sc.get_attr_name(), sc.get_value());
			break;
		case TT_WORD:
			par.parse_word(sc.get_value());
			break;
		case TT_SPACE:
			par.parse_space(sc.get_value());
			break;
		default:
			break;
		}
	}
}

std::shared_ptr<document> document::create_from_bytes(view_host& view, const std::string& url,
                                                      const std::string_view bytes,
                                                      const std::string_view content_type)
{
	auto doc = std::make_shared<document>(view);

	doc->set_base_url(url);
	doc->m_source = decode_to_utf8(bytes, content_type);

	view.diagnostic(std::format("HTML parse started: {} ({} bytes)", url, doc->m_source.size()));

	parser par(*doc);
	html_scanner sc(doc->m_source);
	parse_stream(sc, par);

	view.diagnostic("HTML parse completed");

	doc->load_master_stylesheet(load_resource_html("master.css"));
	doc->set_root(par.release_root());

	return doc;
}

void document::set_root(std::unique_ptr<element> r)
{
	m_root = std::move(r);
	apply_stylesheet();
}

void document::update_styles(element* root_el)
{
	if (root_el)
	{
		root_el->parse_attributes();
	}
	m_styles.sort_selectors();

	if (!m_media_lists.empty())
	{
		media_features features;
		get_media_features(features);
		update_media_lists(features);
	}

	if (root_el)
	{
		const auto t0 = std::chrono::steady_clock::now();
		root_el->apply_stylesheet(m_styles);
		const auto t1 = std::chrono::steady_clock::now();
		root_el->parse_styles();
		const auto t2 = std::chrono::steady_clock::now();
		m_view.diagnostic(std::format("MATCH {} us, PARSE_STYLES {} us",
		                              std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count(),
		                              std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count()));
	}
}

void document::add_stylesheet(const std::string& text, const std::string& baseurl, const std::string& media)
{
	auto media_list = media_query_list::create_from_string(media);
	m_styles.parse_stylesheet(text, baseurl, *this, media_list);
}

void document::apply_stylesheet()
{
	update_styles(m_root.get());
	auto pThis = shared_from_this();
	dispatch_to_ui([pThis]() { pThis->m_view.layout(); });
}

// Stylesheets often land in a burst. Restyling per arrival costs a full cascade
// each time, so fold any that arrive before the queued pass runs into that pass.
void document::request_restyle()
{
	if (m_restyle_pending) return;
	m_restyle_pending = true;

	auto pThis = shared_from_this();
	dispatch_to_ui([pThis]()
	{
		pThis->m_restyle_pending = false;

		if (pThis->m_root)
		{
			pThis->update_styles(pThis->m_root.get());
		}
		pThis->m_view.layout();
	});
}

pf::font_handle document::add_font(const std::string& name_in, int size, const std::string& weight,
                                   const std::string& style,
                                   const std::string& decoration, font_metrics* fm)
{
	pf::font_handle ret = 0;
	auto name = name_in;

	if (name.empty() || is_equal(name.c_str(), "inherit"))
	{
		name = get_default_font_name();
	}

	if (!size)
	{
		size = get_default_font_size();
	}

	const auto key = std::format("{}:{}:{}:{}:{}", name, size, weight, style, decoration);

	{
		std::lock_guard lock(m_fonts_mutex);
		const auto it = m_fonts.find(key);
		if (it != m_fonts.end())
		{
			if (fm) *fm = it->second.metrics;
			return it->second.font;
		}
	}

	{
		const auto fs = static_cast<font_style>(value_index(style, font_style_strings, font_style_normal));

		int fw = value_index(weight, font_weight_strings, -1);
		if (fw >= 0)
		{
			switch (fw)
			{
			case font_weight_bold:
				fw = 700;
				break;
			case font_weight_bolder:
				fw = 600;
				break;
			case font_weight_lighter:
				fw = 300;
				break;
			default:
				fw = fw >= font_weight_100
					     ? (fw - font_weight_100 + 1) * 100
					     : 400;
				break;
			}
		}
		else
		{
			fw = safe_stoi(weight, 400);
			if (fw < 100)
			{
				fw = 400;
			}
		}

		unsigned int decor = 0;

		if (!decoration.empty())
		{
			const auto tokens = split_string(decoration);

			for (const auto& tok : tokens)
			{
				if (is_equal(tok.c_str(), "underline"))
				{
					decor |= font_decoration_underline;
				}
				else if (is_equal(tok.c_str(), "line-through"))
				{
					decor |= font_decoration_linethrough;
				}
				else if (is_equal(tok.c_str(), "overline"))
				{
					decor |= font_decoration_overline;
				}
			}
		}

		font_item fi = {};

		const auto fonts = split_string(name, ',');

		pf::font_def def;
		def.face = fonts.empty() ? get_default_font_name() : fonts.front();
		def.size = size;
		def.weight = fw;
		def.italic = fs == font_style_italic;
		def.underline = (decor & font_decoration_underline) != 0;
		def.strikeout = (decor & font_decoration_linethrough) != 0;

		pf::font_metrics_data m{};
		fi.font = pf::create_font_handle(def, &m);

		fi.metrics.height = m.height;
		fi.metrics.x_height = m.x_height;
		fi.metrics.ascent = m.ascent;
		fi.metrics.descent = m.descent;
		fi.metrics.draw_spaces = true;

		{
			std::lock_guard lock(m_fonts_mutex);
			// Another thread may have created the same font while the lock was
			// released; keep the existing entry and drop the duplicate handle.
			const auto it = m_fonts.find(key);
			if (it != m_fonts.end())
			{
				if (fi.font) pf::delete_font_handle(fi.font);
				fi = it->second;
			}
			else
			{
				m_fonts[key] = fi;
			}
		}
		ret = fi.font;

		if (fm)
		{
			*fm = fi.metrics;
		}
	}
	return ret;
}

pf::font_handle document::get_font(const std::string& name_in, int size, const std::string& weight,
                                   const std::string& style,
                                   const std::string& decoration, font_metrics* fm)
{
	auto name = name_in;

	if (name.empty() || is_equal(name, "inherit"))
	{
		name = get_default_font_name();
	}

	if (!size)
	{
		size = get_default_font_size();
	}

	const auto key = std::format("{}:{}:{}:{}:{}", name, size, weight, style, decoration);

	{
		std::lock_guard lock(m_fonts_mutex);
		const auto el = m_fonts.find(key);
		if (el != m_fonts.end())
		{
			if (fm) *fm = el->second.metrics;
			return el->second.font;
		}
	}

	return add_font(name, size, weight, style, decoration, fm);
}

int document::render(const int max_width, const render_type rt)
{
	const auto started = std::chrono::steady_clock::now();
	int ret = 0;
	if (m_root)
	{
		if (rt == render_fixed_only)
		{
			m_fixed_boxes.clear();
			m_root->render_positioned(rt);
		}
		else
		{
			ret = m_root->render(0, 0, max_width);
			if (m_root->fetch_positioned())
			{
				m_fixed_boxes.clear();
				m_root->render_positioned(rt);
			}
			m_size.width = 0;
			m_size.height = 0;
			m_root->calc_document_size(m_size);
		}
	}
	m_view.diagnostic(std::format("RENDER {} us", std::chrono::duration_cast<std::chrono::microseconds>(
		                              std::chrono::steady_clock::now() - started).count()));
	return ret;
}

void document::draw(render_win32& renderer, const int x, const int y, const position* clip)
{
	if (m_root)
	{
		m_root->draw(renderer, x, y, clip);
		m_root->draw_stacking_context(renderer, x, y, clip, true);
	}
}

int document::cvt_units(const std::string& str, const int fontSize, bool* is_percent/*= 0*/)
{
	if (str.empty()) return 0;

	css_length val;
	val.fromString(str);

	if (is_percent && val.units() == css_units_percentage && !val.is_predefined())
	{
		*is_percent = true;
	}
	return cvt_units(val, fontSize);
}

int document::cvt_units(css_length& val, const int fontSize, const int size)
{
	if (val.is_predefined())
	{
		return 0;
	}
	if (val.is_calc())
	{
		// calc() keeps its own percentage and fixed parts and never sets m_units,
		// so the switch below would read a value that was never stored.
		return val.calc_percent(size);
	}
	int ret = 0;
	switch (val.units())
	{
	case css_units_percentage:
		ret = val.calc_percent(size);
		break;
	case css_units_em:
		ret = round_f(val.val() * fontSize);
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	case css_units_rem:
		ret = round_f(val.val() * get_default_font_size());
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	case css_units_vw:
		ret = round_f(val.val() * pf::platform_screen_size().cx / 100.0f);
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	case css_units_vh:
		ret = round_f(val.val() * pf::platform_screen_size().cy / 100.0f);
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	case css_units_vmin:
		ret = round_f(val.val() * std::min(pf::platform_screen_size().cx, pf::platform_screen_size().cy) / 100.0f);
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	case css_units_vmax:
		ret = round_f(val.val() * std::max(pf::platform_screen_size().cx, pf::platform_screen_size().cy) / 100.0f);
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	case css_units_pt:
		ret = pt_to_px(static_cast<int>(val.val()));
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	case css_units_in:
		ret = pt_to_px(static_cast<int>(val.val() * 72));
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	case css_units_cm:
		ret = pt_to_px(static_cast<int>(val.val() * 0.3937 * 72));
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	case css_units_mm:
		ret = pt_to_px(static_cast<int>(val.val() * 0.3937 * 72) / 10);
		val.set_value(static_cast<float>(ret), css_units_px);
		break;
	default:
		ret = static_cast<int>(val.val());
		break;
	}
	return ret;
}

int document::width() const
{
	return m_size.width;
}

int document::height() const
{
	return m_size.height;
}

// Walks the laid-out tree and reports structure plus geometry that no correct
// layout should produce. Uses box geometry only, so it needs no window and is
// reproducible whenever the layout is.
layout_stats document::analyse_layout(std::vector<std::string>* samples) const
{
	layout_stats st;
	if (!m_root) return st;

	constexpr int max_samples = 12;
	const int viewport = m_client_pos.width;

	const auto note = [&](const std::string& what, const element* el, const int x, const int y)
	{
		if (!samples || static_cast<int>(samples->size()) >= max_samples) return;
		samples->push_back(std::format("{}: <{} id='{}' class='{}'> x={} y={} {}x{} display={}",
		                               what, el->get_tag_name(), el->get_attr("id"), el->get_attr("class"),
		                               x, y, el->width(), el->height(),
		                               el->get_style_property(prop_id::display, false)));
	};

	std::function<void(const element*, int, int, int, bool)> visit =
		[&](const element* el, const int parent_x, const int parent_y, const int depth, const bool ancestors_visible)
	{
		const int x = parent_x + el->left();
		const int y = parent_y + el->top();

		st.max_depth = std::max(st.max_depth, depth);

		// Geometry inside a hidden subtree is never painted, so judging it
		// produces only false positives.
		const bool visible = ancestors_visible && el->is_visible();

		if (ancestors_visible && !visible) ++st.hidden_subtrees;

		if (el->is_text_node())
		{
			++st.text_nodes;

			if (visible && el->width() > 0 && el->height() <= 0 &&
				!el->is_break() && ++st.zero_area_text <= max_samples)
			{
				note("zero-height text", el, x, y);
			}

			if (visible)
			{
				const int right = x + el->width();
				st.right_edge = std::max(st.right_edge, right);
				const bool parent_is_root = el->parent() && !el->parent()->parent();
				const bool parent_fits = parent_is_root ||
					parent_x + (el->parent() ? el->parent()->width() : 0) <= viewport;

				if (right > viewport && parent_fits)
				{
					++st.overflow_x;
					note("overflows viewport", el, x, y);
				}

				if (x < 0 && parent_x >= 0)
				{
					++st.negative_x;
					note("starts left of origin", el, x, y);
				}
			}
		}
		else
		{
			++st.elements;

			if (el->get_tag_name() == "img")
			{
				++st.images;
				// Only width is judged: height comes from the bitmap's aspect
				// ratio, and headless runs never receive one.
				if (visible && el->width() <= 0)
				{
					++st.unsized_image;
					note("unsized image", el, x, y);
				}
			}

			if (visible)
			{
				const int right = x + el->width();
				if (el->parent()) st.right_edge = std::max(st.right_edge, right);

				if (el->width() < 0 || el->height() < 0)
				{
					++st.negative_size;
					note("negative size", el, x, y);
				}

				// Only report the outermost offender: a wide ancestor makes
				// every descendant overflow too, which buries the cause.
				const bool parent_is_root = el->parent() && !el->parent()->parent();
				const bool parent_fits = parent_is_root ||
					parent_x + (el->parent() ? el->parent()->width() : 0) <= viewport;

				if (el->parent() && right > viewport && parent_fits)
				{
					++st.overflow_x;
					note("overflows viewport", el, x, y);
				}

				if (x < 0 && parent_x >= 0)
				{
					++st.negative_x;
					note("starts left of origin", el, x, y);
				}
			}
		}

		const int content_x = x + el->content_margins_left();
		const int content_y = y + el->content_margins_top();
		for (size_t i = 0; i < el->get_children_count(); ++i)
			visit(el->get_child(static_cast<int>(i)), content_x, content_y, depth + 1, visible);
	};

	visit(m_root.get(), 0, 0, 0, true);
	return st;
}

// Indented box tree in document coordinates. Whitespace-only text is skipped
// because it carries no geometry worth reading.
std::vector<std::string> document::dump_boxes(const int max_depth) const
{
	std::vector<std::string> lines;
	if (!m_root) return lines;

	std::function<void(const element*, int, int, int)> visit =
		[&](const element* el, const int parent_x, const int parent_y, const int depth)
	{
		const int x = parent_x + el->left();
		const int y = parent_y + el->top();

		if (depth <= max_depth)
		{
			const auto text = el->is_text_node() ? el->get_text() : std::string();
			const bool blank = el->is_text_node() && text.find_first_not_of(" \t\r\n") == std::string::npos;

			if (!blank)
			{
				std::string label;

				if (el->is_text_node())
				{
					auto t = text.substr(0, 40);
					std::replace(t.begin(), t.end(), '\n', ' ');
					label = std::format("\"{}\"", t);
				}
				else
				{
					label = "<" + el->get_tag_name();
					const auto id = el->get_attr("id");
					const auto cls = el->get_attr("class");
					if (!id.empty()) label += std::format(" id={}", id);
					if (!cls.empty()) label += std::format(" class={}", cls.substr(0, 40));
					label += ">";
				}

				lines.push_back(std::format("{}{} [{},{} {}x{}] {}",
				                            std::string(depth * 2, ' '), label, x, y,
				                            el->width(), el->height(),
				                            el->is_text_node()
					                            ? std::string()
					                            : el->get_style_property(prop_id::display, false)));
			}
		}

		const int content_x = x + el->content_margins_left();
		const int content_y = y + el->content_margins_top();
		for (size_t i = 0; i < el->get_children_count(); ++i)
			visit(el->get_child(static_cast<int>(i)), content_x, content_y, depth + 1);
	};

	visit(m_root.get(), 0, 0, 0);
	return lines;
}

namespace
{
	void append_json_string(std::string& out, const std::string_view value)
	{
		out.push_back('"');
		for (const unsigned char c : value)
		{
			switch (c)
			{
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\b': out += "\\b"; break;
			case '\f': out += "\\f"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (c < 0x20) out += std::format("\\u{:04x}", c);
				else out.push_back(static_cast<char>(c));
				break;
			}
		}
		out.push_back('"');
	}

	std::string_view indexed_value(const std::string_view values, const int index)
	{
		size_t start = 0;
		for (int current = 0; current < index; ++current)
		{
			start = values.find(';', start);
			if (start == std::string_view::npos) return {};
			++start;
		}
		const auto end = values.find(';', start);
		return values.substr(start, end == std::string_view::npos ? end : end - start);
	}

	void append_edges(std::string& out, const margins& edges)
	{
		out += std::format("{{\"top\":{},\"right\":{},\"bottom\":{},\"left\":{}}}",
		                   edges.top, edges.right, edges.bottom, edges.left);
	}
}

std::string document::dump_layout_json() const
{
	std::string out = std::format(
		"{{\"source\":\"potato\",\"viewport\":{{\"width\":{},\"height\":{},"
		"\"devicePixelRatio\":1,\"scrollX\":0,\"scrollY\":0}},"
		"\"document\":{{\"width\":{},\"height\":{}}},\"probes\":[",
		m_client_pos.width, m_client_pos.height, width(), height());
	bool first = true;

	std::function<void(const element*, int, int)> visit =
		[&](const element* el, const int parent_x, const int parent_y)
	{
		const int outer_x = parent_x + el->left();
		const int outer_y = parent_y + el->top();
		const auto key = el->get_attr("data-probe");

		if (!key.empty())
		{
			if (!first) out.push_back(',');
			first = false;
			out += "{\"key\":";
			append_json_string(out, key);
			out += ",\"tag\":";
			append_json_string(out, el->get_tag_name());
			out += std::format(
				",\"rect\":{{\"x\":{},\"y\":{},\"width\":{},\"height\":{}}},\"display\":" ,
				outer_x + el->margin_left(), outer_y + el->margin_top(),
				el->width() - el->margin_left() - el->margin_right(),
				el->height() - el->margin_top() - el->margin_bottom());
			append_json_string(out, indexed_value(style_display_strings, static_cast<int>(el->get_display())));
			out += ",\"position\":";
			append_json_string(out, indexed_value(element_position_strings,
			                                      static_cast<int>(el->get_element_position())));
			out += ",\"boxSizing\":";
			append_json_string(out, el->get_style_property(prop_id::box_sizing, false, "content-box"));
			out += ",\"overflowX\":";
			append_json_string(out, indexed_value(overflow_strings, static_cast<int>(el->get_overflow())));
			out += ",\"overflowY\":";
			append_json_string(out, indexed_value(overflow_strings, static_cast<int>(el->get_overflow())));
			out += ",\"margin\":";
			append_edges(out, el->get_margins());
			out += ",\"padding\":";
			append_edges(out, el->get_paddings());
			out += ",\"border\":";
			append_edges(out, el->get_borders());
			out += ",\"fontFamily\":";
			append_json_string(out, el->get_style_property(prop_id::font_family, true));
			out += std::format(",\"fontSize\":{},\"lineHeight\":{}}}",
			                   el->get_font_size(), el->line_height());
		}

		const int content_x = outer_x + el->content_margins_left();
		const int content_y = outer_y + el->content_margins_top();
		for (size_t i = 0; i < el->get_children_count(); ++i)
			visit(el->get_child(static_cast<int>(i)), content_x, content_y);
	};

	if (m_root) visit(m_root.get(), 0, 0);
	out += "]}";
	return out;
}

void document::diagnose_layout() const
{
	std::vector<std::string> samples;
	const auto st = analyse_layout(&samples);

	m_view.diagnostic(std::format(
		"Nodes: {} elements, {} text, {} images, depth {}; right edge {} (viewport {})",
		st.elements, st.text_nodes, st.images, st.max_depth, st.right_edge, m_client_pos.width));

	if (st.overflow_x || st.negative_x || st.zero_area_text || st.unsized_image || st.negative_size)
	{
		m_view.diagnostic(std::format(
			"Anomalies: {} overflow-x, {} negative-x, {} zero-height-text, {} unsized-image, {} negative-size",
			st.overflow_x, st.negative_x, st.zero_area_text, st.unsized_image, st.negative_size));

		for (const auto& s : samples) m_view.diagnostic("  " + s);
	}
}


bool document::on_mouse_over(const int x, const int y, const int client_x, const int client_y,
                             position::vector& redraw_boxes)
{
	if (!m_root)
	{
		return false;
	}

	element* over_el = m_root->get_element_by_point(x, y, client_x, client_y);

	bool state_was_changed = false;

	if (over_el != m_over_element)
	{
		if (m_over_element)
		{
			if (m_over_element->on_mouse_leave())
			{
				state_was_changed = true;
			}
		}
		m_over_element = over_el;
		if (m_over_element)
		{
			if (m_over_element->on_mouse_over())
			{
				state_was_changed = true;
			}
		}
	}

	std::string cursor = "auto";

	if (m_over_element)
	{
		cursor = m_over_element->get_cursor();
	}

	set_cursor(cursor);

	if (state_was_changed)
	{
		return m_root->find_styles_changes(redraw_boxes, 0, 0);
	}

	return false;
}

bool document::on_mouse_leave(position::vector& redraw_boxes)
{
	if (!m_root)
	{
		return false;
	}
	if (m_over_element)
	{
		if (m_over_element->on_mouse_leave())
		{
			return m_root->find_styles_changes(redraw_boxes, 0, 0);
		}
	}
	return false;
}

bool document::on_lbutton_down(const int x, const int y, const int client_x, const int client_y,
                               position::vector& redraw_boxes)
{
	if (!m_root)
	{
		return false;
	}

	element* over_el = m_root->get_element_by_point(x, y, client_x, client_y);

	bool state_was_changed = false;

	if (over_el != m_over_element)
	{
		if (m_over_element)
		{
			if (m_over_element->on_mouse_leave())
			{
				state_was_changed = true;
			}
		}
		m_over_element = over_el;
		if (m_over_element)
		{
			if (m_over_element->on_mouse_over())
			{
				state_was_changed = true;
			}
		}
	}

	std::string cursor = "auto";

	if (m_over_element)
	{
		if (m_over_element->on_lbutton_down())
		{
			state_was_changed = true;
		}
		cursor = m_over_element->get_cursor();
	}

	set_cursor(cursor);

	if (state_was_changed)
	{
		return m_root->find_styles_changes(redraw_boxes, 0, 0);
	}

	return false;
}

bool document::on_lbutton_up(int x, int y, int client_x, int client_y, position::vector& redraw_boxes)
{
	const auto root = m_root;
	if (!root)
	{
		return false;
	}
	if (m_over_element)
	{
		if (m_over_element->on_lbutton_up())
		{
			return root->find_styles_changes(redraw_boxes, 0, 0);
		}
	}
	return false;
}

void document::add_fixed_box(const position& pos)
{
	m_fixed_boxes.push_back(pos);
}

bool document::update_media_lists(const media_features& features)
{
	bool update_styles = false;

	for (const auto& ml : m_media_lists)
	{
		if (ml->apply_media_features(features))
		{
			update_styles = true;
		}
	}

	return update_styles;
}

void document::add_media_list(const std::shared_ptr<media_query_list>& list)
{
	if (list)
	{
		if (std::find(m_media_lists.begin(), m_media_lists.end(), list) == m_media_lists.end())
		{
			m_media_lists.push_back(list);
		}
	}
}


void document::set_caption(const std::string& caption)
{
	m_caption = caption;
}

void document::set_base_url(const std::string& base_url)
{
	if (!base_url.empty())
	{
		if (m_url.empty())
		{
			m_url = base_url;
			m_base_path = base_url;
		}
		else
		{
			m_base_path = pf::resolve_url(m_url, base_url);
		}
	}
	else
	{
		//m_base_path = m_url;
	}
}

void document::link(const element* el)
{
	const auto& rel = el->get_attr("rel");

	if (rel == "stylesheet")
	{
		const std::string href(el->get_attr("href"));
		if (href.empty()) return;

		const std::string media(el->get_attr("media", "all"));
		// Trigger an asynchronous CSS download. import_css already handles base path
		// resolution, parsing, and applying the rules to the live DOM.
		import_css(href, m_base_path, media);
	}
}

void document::import_css(const std::string& url, const std::string& baseurl, const std::string& media)
{
	auto base_path = baseurl;

	if (base_path.empty())
	{
		base_path = m_base_path;
	}

	auto css_url = make_url(url, base_path);
	auto pThis = shared_from_this();
	m_view.resource_started("stylesheet", css_url);

	m_http.download_file(css_url, std::make_shared<http_request>(
		                     [pThis, css_url, media](const std::string& file_name, const uint32_t error,
		                                             const uint32_t httpStatus,
		                                             const std::string& /*reqUrl*/)
		                     {
			                     if (error || httpStatus >= 400)
			                     {
				                     pThis->m_view.resource_finished("stylesheet", css_url, false);
				                     return;
			                     }
			                     const auto css_text = get_file_contents(file_name);
			                     if (css_text.empty())
			                     {
				                     pThis->m_view.resource_finished("stylesheet", css_url, false);
				                     return;
			                     }
			                     pThis->m_view.diagnostic(std::format(
				                     "Stylesheet downloaded: {} bytes, HTTP {}: {}",
				                     css_text.size(), httpStatus, css_url));

			                     dispatch_to_ui([pThis, css_url, css_text, media]()
			                     {
				                     const auto selectors_before = pThis->m_styles.selectors().size();
				                     pThis->add_stylesheet(css_text, css_url, media);
				                     pThis->m_styles.sort_selectors();
				                     pThis->m_view.diagnostic(std::format(
					                     "Stylesheet parsed: {} selectors added, {} total: {}",
					                     pThis->m_styles.selectors().size() - selectors_before,
					                     pThis->m_styles.selectors().size(), css_url));

				                     if (pThis->m_root)
				                     {
					                     pThis->request_restyle();
				                     }
				                     pThis->m_view.resource_finished("stylesheet", css_url, true);
			                     });
		                     }));
}

void document::on_anchor_click(const std::string& url, element* el)
{
	auto full = make_url(url, m_base_path);
	view_host* view = &m_view;
	dispatch_to_ui([view, full] { view->open(full); });
}

void document::set_cursor(const std::string& cursor)
{
	m_cursor = cursor;
}

void document::load_image(const std::string& url, const std::string& base)
{
	auto image_url = make_url(url, base.empty() ? m_base_path : base);
	auto pThis = shared_from_this();

	if (!m_images.contains(image_url))
	{
		m_images[image_url] = nullptr; // Indicate loading
		m_view.resource_started("image", image_url);

		m_http.download_file(image_url, std::make_shared<http_request>(
			                     [pThis, image_url](const std::string& file_name, const uint32_t error,
			                                        const uint32_t httpStatus, const std::string& reqUrl)
			                     {
				                     if (error || httpStatus >= 400)
				                     {
					                     pThis->m_view.resource_finished("image", image_url, false);
					                     return;
				                     }
				                     auto image = pf::load_bitmap_file(pf::file_path(file_name));
				                     if (!image)
				                     {
					                     image = create_svg_placeholder(get_file_contents(file_name));
					                     if (image)
					                     {
						                     pThis->m_view.diagnostic(std::format(
							                     "SVG placeholder: {}x{}: {}", image->width, image->height,
							                     image_url));
					                     }
				                     }
				                     pThis->m_images[image_url] = std::move(image);
				                     pThis->m_view.resource_finished(
					                     "image", image_url, pThis->m_images[image_url] != nullptr);
				                     pThis->m_view.layout();
			                     }));
	}
}

pf::bitmap_ptr document::find_image(const std::string& url)
{
	return find_image(url, m_base_path);
}

pf::bitmap_ptr document::find_image(const std::string& url, const std::string& base)
{
	const auto image_url = make_url(url, base.empty() ? m_base_path : base);
	const auto found = m_images.find(image_url);
	return found != m_images.end() ? found->second : nullptr;
}

bool document::is_image_cached(const std::string& src, const std::string& baseurl)
{
	const auto url = make_url(src, baseurl.empty() ? m_base_path : baseurl);
	return m_images.contains(url);
}


int document::text_width(const std::string_view text, const pf::font_handle hFont)
{
	return pf::measure_text_with_font(hFont, text).cx;
}

int document::pt_to_px(const int pt)
{
	return pt * pf::platform_screen_dpi() / 72;
}


void document::get_media_features(media_features& media)
{
	const auto dpi = pf::platform_screen_dpi();
	const auto sz = pf::platform_screen_size();

	media.type = media_type_screen;
	media.width = m_client_pos.width;
	media.height = m_client_pos.height;
	media.color = 8;
	media.monochrome = 0;
	media.color_index = 256;
	media.resolution = dpi;
	media.device_width = sz.cx;
	media.device_height = sz.cy;
}
