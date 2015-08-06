#include "pch.h"
#include "document.h"
#include "stylesheet.h"
#include "strings.h"
#include "render_win32.h"
#include "util.h"
#include "instream.h"
#include "html_view.h"
#include "resource.h"
#include "ui.h"


document::document(CHtmlView &v) : _view(v), m_root(nullptr), m_over_element(nullptr)
{
    m_http.open(L"potato/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS);
}

document::~document()
{
    m_http.stop();
    clear();

    /*if(m_container)
    {
    for(auto f = m_fonts.begin(); f != m_fonts.end(); f++)
    {
    m_renderer->delete_font(f->second.font);
    }
    }*/
}

void document::clear()
{
    delete m_root;

    m_root = nullptr;
    m_over_element = nullptr;

    m_styles.clear();
    m_fixed_boxes.clear();
    m_media_lists.clear();
    m_images.clear();
}

void document::load_master_stylesheet(const std::wstring &text)
{
    auto media_list = media_query_list::create_from_string(L"screen");
    m_styles.parse_stylesheet(text, empty, *this, media_list);
}

template<class tstream>
static std::shared_ptr<document> createFromStream(CHtmlView &view, const std::wstring &url, tstream& str)
{
    auto doc = std::make_shared<document>(view);

    doc->set_base_url(url);

    scanner<tstream> sc(str);
    parser par(*doc);

    int t = 0;
    std::wstring tmp_str;

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
        {
            tmp_str = sc.get_tag_name();
            if (!tmp_str.empty() && tmp_str[0] != '!')
            {
                par.parse_tag_start(trim_lower(tmp_str));
            }
        }
            break;
        case TT_TAG_END_EMPTY:
        case TT_TAG_END:
        {
            par.parse_tag_end(trim_lower(sc.get_tag_name()));
        }
            break;
        case TT_ATTR:
        {
            par.parse_attribute(trim_lower(sc.get_attr_name()), sc.get_value());
        }
            break;
        case TT_WORD:
            par.parse_word(sc.get_value());
            break;
        case TT_SPACE:
            par.parse_space(sc.get_value());
            break;
        }
    }

    doc->load_master_stylesheet(ToUtf16(load_resource_html(IDR_CSS_MASTER)));
    doc->set_root(par.root());

    return doc;
}

void document::set_root(element *r)
{
    m_root = r;
    apply_stylesheet();
}

std::shared_ptr<document> document::createFromUTF16(CHtmlView &view, const std::wstring &url, const std::wstring &str)
{
    utf16_instream si(str);
    return createFromStream(view, url, si);
}

std::shared_ptr<document> document::createFromUTF8(CHtmlView &view, const std::wstring &url, const std::string &str)
{
    utf8_instream si(str);
    return createFromStream(view, url, si);
}

void document::add_stylesheet(const std::wstring &text, const std::wstring &baseurl, const std::wstring &media)
{
    auto media_list = media_query_list::create_from_string(media);
    m_styles.parse_stylesheet(text, baseurl, *this, media_list);
}

void document::apply_stylesheet()
{
    m_styles.sort_selectors();

    if (!m_media_lists.empty())
    {
        media_features features;
        get_media_features(features);
        update_media_lists(features);
    }

    if (m_root)
    {
        m_root->parse_attributes();
        m_root->apply_stylesheet(m_styles);
        m_root->parse_styles();
    }

    _view.layout();
}

HFONT document::add_font(const std::wstring &name_in, int size, const std::wstring &weight, const std::wstring &style, const std::wstring &decoration, font_metrics* fm)
{
    HFONT ret = 0;
    auto name = name_in;

    if (name.empty() || is_equal(name.c_str(), _t("inherit")))
    {
        name = get_default_font_name();
    }

    if (!size)
    {
        size = get_default_font_size();
    }

    wchar_t strSize[20];
    _itow_s(size, strSize, 20, 10);

    std::wstring key = name;
    key += _t(":");
    key += strSize;
    key += _t(":");
    key += weight;
    key += _t(":");
    key += style;
    key += _t(":");
    key += decoration;

    if (m_fonts.find(key) == m_fonts.end())
    {
        font_style fs = (font_style) value_index(style, font_style_strings, fontStyleNormal);

        int fw = value_index(weight, font_weight_strings, -1);
        if (fw >= 0)
        {
            switch (fw)
            {
            case fontWeightBold:
                fw = 700;
                break;
            case fontWeightBolder:
                fw = 600;
                break;
            case fontWeightLighter:
                fw = 300;
                break;
            default:
                fw = 400;
                break;
            }
        }
        else
        {
            fw = std::stoi(weight);
            if (fw < 100)
            {
                fw = 400;
            }
        }

        unsigned int decor = 0;

        if (!decoration.empty())
        {
            auto tokens = split_string(decoration);

            for (const auto &tok : tokens)
            {
                if (is_equal(tok.c_str(), _t("underline")))
                {
                    decor |= font_decoration_underline;
                }
                else if (is_equal(tok.c_str(), _t("line-through")))
                {
                    decor |= font_decoration_linethrough;
                }
                else if (is_equal(tok.c_str(), _t("overline")))
                {
                    decor |= font_decoration_overline;
                }
            }
        }

        font_item fi = { 0 };

        auto fonts = split_string(name, L',');
        trim(fonts[0]);

        LOGFONT lf;
        ZeroMemory(&lf, sizeof(lf));
        wcscpy_s(lf.lfFaceName, LF_FACESIZE, fonts[0].c_str());

        lf.lfHeight = -size;
        lf.lfWeight = fw;
        lf.lfItalic = (fs == fontStyleItalic) ? TRUE : FALSE;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = CLEARTYPE_QUALITY;
        lf.lfStrikeOut = (decor & font_decoration_linethrough) ? TRUE : FALSE;
        lf.lfUnderline = (decor & font_decoration_underline) ? TRUE : FALSE;

        fi.font = CreateFontIndirect(&lf);

        win_dc hdc(nullptr);
        auto oldFont = (HFONT) SelectObject(hdc, (HFONT) fi.font);
        TEXTMETRIC tm;
        GetTextMetrics(hdc, &tm);
        SelectObject(hdc, oldFont);

        fi.metrics.height = tm.tmHeight;
        fi.metrics.x_height = tm.tmHeight;
        fi.metrics.ascent = tm.tmAscent;
        fi.metrics.descent = tm.tmDescent;
        fi.metrics.draw_spaces = true;

        m_fonts[key] = fi;
        ret = fi.font;

        if (fm)
        {
            *fm = fi.metrics;
        }
    }
    return ret;
}

HFONT document::get_font(const std::wstring &name_in, int size, const std::wstring &weight, const std::wstring &style, const std::wstring &decoration, font_metrics* fm)
{
    auto name = name_in;

    if (name.empty() || is_equal(name, _t("inherit")))
    {
        name = get_default_font_name();
    }

    if (!size)
    {
        size = get_default_font_size();
    }

    wchar_t strSize[20];
    _itow_s(size, strSize, 20, 10);

    std::wstring key = name;
    key += _t(":");
    key += strSize;
    key += _t(":");
    key += weight;
    key += _t(":");
    key += style;
    key += _t(":");
    key += decoration;

    auto el = m_fonts.find(key);

    if (el != m_fonts.end())
    {
        if (fm)
        {
            *fm = el->second.metrics;
        }
        return el->second.font;
    }

    return add_font(name, size, weight, style, decoration, fm);
}

element *document::add_root()
{
    if (!m_root)
    {
        m_root = new element(*this, el_html);
        m_root->set_tagName(_t("html"));
    }
    return m_root;
}

element *document::add_body()
{
    if (!m_root)
    {
        add_root();
    }
    auto el = new element(*this, el_body);
    el->set_tagName(_t("body"));
    m_root->appendChild(el);
    return el;
}

int document::render(render_win32 &renderer, int max_width, render_type rt)
{
    int ret = 0;
    if (m_root)
    {
        if (rt == render_fixed_only)
        {
            m_fixed_boxes.clear();
            m_root->render_positioned(renderer, rt);
        }
        else
        {
            ret = m_root->render(renderer, 0, 0, max_width);
            if (m_root->fetch_positioned())
            {
                m_fixed_boxes.clear();
                m_root->render_positioned(renderer, rt);
            }
            m_size.width = 0;
            m_size.height = 0;
            m_root->calc_document_size(m_size);
        }
    }
    return ret;
}

void document::draw(render_win32 &renderer, int x, int y, const position* clip)
{
    if (m_root)
    {
        m_root->draw(renderer, x, y, clip);
        m_root->draw_stacking_context(renderer, x, y, clip, true);
    }
}

int document::cvt_units(const std::wstring &str, int fontSize, bool* is_percent/*= 0*/)
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

int document::cvt_units(css_length& val, int fontSize, int size)
{
    if (val.is_predefined())
    {
        return 0;
    }
    int ret = 0;
    switch (val.units())
    {
    case css_units_percentage:
        ret = val.calc_percent(size);
        break;
    case css_units_em:
        ret = round_f(val.val() * fontSize);
        val.set_value((float) ret, css_units_px);
        break;
    case css_units_pt:
        ret = pt_to_px((int) val.val());
        val.set_value((float) ret, css_units_px);
        break;
    case css_units_in:
        ret = pt_to_px((int) (val.val() * 72));
        val.set_value((float) ret, css_units_px);
        break;
    case css_units_cm:
        ret = pt_to_px((int) (val.val() * 0.3937 * 72));
        val.set_value((float) ret, css_units_px);
        break;
    case css_units_mm:
        ret = pt_to_px((int) (val.val() * 0.3937 * 72) / 10);
        val.set_value((float) ret, css_units_px);
        break;
    default:
        ret = (int) val.val();
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

bool document::on_mouse_over(int x, int y, int client_x, int client_y, position::vector& redraw_boxes)
{
    if (!m_root)
    {
        return false;
    }

    element *over_el = m_root->get_element_by_point(x, y, client_x, client_y);

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
    }

    std::wstring cursor = _t("auto");

    if (m_over_element)
    {
        if (m_over_element->on_mouse_over())
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

bool document::on_lbutton_down(int x, int y, int client_x, int client_y, position::vector& redraw_boxes)
{
    if (!m_root)
    {
        return false;
    }

    element *over_el = m_root->get_element_by_point(x, y, client_x, client_y);

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

    std::wstring cursor = _t("auto");

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
    if (!m_root)
    {
        return false;
    }
    if (m_over_element)
    {
        if (m_over_element->on_lbutton_up())
        {
            return m_root->find_styles_changes(redraw_boxes, 0, 0);
        }
    }
    return false;
}

void document::add_fixed_box(const position& pos)
{
    m_fixed_boxes.push_back(pos);
}

bool document::media_changed()
{
    if (!m_media_lists.empty())
    {
        media_features features;
        get_media_features(features);

        if (update_media_lists(features))
        {
            m_root->refresh_styles();
            m_root->parse_styles();
            return true;
        }
    }
    return false;
}

bool document::update_media_lists(const media_features& features)
{
    bool update_styles = false;

    for (auto &ml : m_media_lists)
    {
        if (ml->apply_media_features(features))
        {
            update_styles = true;
        }
    }

    return update_styles;
}

void document::add_media_list(std::shared_ptr<media_query_list> list)
{
    if (list)
    {
        if (std::find(m_media_lists.begin(), m_media_lists.end(), list) == m_media_lists.end())
        {
            m_media_lists.push_back(list);
        }
    }
}


void document::set_caption(const std::wstring &caption)
{
    m_caption = caption;
}

void document::set_base_url(const std::wstring &base_url)
{
    if (!base_url.empty())
    {
        if (PathIsRelative(base_url.c_str()) && !PathIsURL(base_url.c_str()))
        {
            m_base_path = make_url(base_url, m_url);
        }
        else
        {
            m_base_path = base_url;
        }
    }
    else
    {
        //m_base_path = m_url;
    }
}

void document::link(element *el)
{

    const auto &rel = el->get_attr(L"rel");

    if (rel == L"stylesheet")
    {
        const auto media = el->get_attr(L"media", L"screen");

        if (!media.empty() && (media != L"screen" || media != L"all"))
        {
            const auto &href = el->get_attr(L"href");

            if (!href.empty())
            {
                /* std::wstring url;
                make_url(href, nullptr, url);
                if (download_and_wait(url))
                {
                LPWSTR css = load_text_file(m_waited_file.c_str(), L"UTF-8");
                if (css)
                {
                doc->add_stylesheet(css, url.c_str());
                delete css;
                }
                }*/
            }
        }
    }

}

void document::import_css(const std::wstring& url, const std::wstring& baseurl)
{
    auto base_path = baseurl;

    if (base_path.empty())
    {
        base_path = m_base_path;
    }

    auto css_url = make_url(url, base_path);
    auto pThis = shared_from_this();

    m_http.download_file(css_url, std::make_shared<http_request>([pThis, css_url](const std::wstring &file_name) {

        auto css_text = get_file_contents(file_name);

        if (!css_text.empty())
        {
            pThis->add_stylesheet(ToUtf16(css_text), css_url, empty);
            pThis->apply_stylesheet();
        }

    }));
}

void document::on_anchor_click(const std::wstring &url, element *el)
{
    _view.open(make_url(url, m_base_path));
}

void document::set_cursor(const std::wstring &cursor)
{
    m_cursor = cursor;
}

void document::load_image(const std::wstring &url, const std::wstring &base)
{
    auto image_url = make_url(url, base);
    auto pThis = shared_from_this();

    if (m_images.find(image_url) == m_images.end())
    {
        m_images[image_url] = nullptr; // Indicate loading

        m_http.download_file(image_url, std::make_shared<http_request>([pThis, image_url](const std::wstring &file_name) {

            pThis->m_images[image_url] = std::make_shared<Gdiplus::Bitmap>(file_name.c_str());
            pThis->_view.layout();

        }));
    }
}

std::shared_ptr<Gdiplus::Bitmap> document::find_image(const std::wstring &url)
{
    auto found = m_images.find(url);
    return found != m_images.end() ? found->second : nullptr;
}

std::shared_ptr<Gdiplus::Bitmap> document::find_image(const std::wstring &url, const std::wstring &base)
{
    return find_image(make_url(url, base));
}

bool document::is_image_cached(const std::wstring &src, const std::wstring &baseurl)
{
    auto url = make_url(src, baseurl);
    return m_images.find(url) != m_images.end();
}


void document::delete_font(HFONT hFont)
{
    DeleteObject((HFONT) hFont);
}

int document::text_width(const std::wstring &text, HFONT hFont)
{
    win_dc hdc(nullptr);
    auto oldFont = (HFONT) SelectObject(hdc, (HFONT) hFont);

    SIZE sz = { 0, 0 };
    GetTextExtentPoint32(hdc, text.c_str(), text.size(), &sz);
    SelectObject(hdc, oldFont);

    return (int) sz.cx;
}

int document::pt_to_px(int pt)
{
    win_dc hdc(nullptr);
    return MulDiv(pt, GetDeviceCaps(hdc, LOGPIXELSY), 72);
}


void document::get_media_features(media_features& media)
{
    win_dc hdc(nullptr);

    media.type = media_type_screen;
    media.width = _client_pos.width;
    media.height = _client_pos.height;
    media.color = 8;
    media.monochrome = 0;
    media.color_index = 256;
    media.resolution = GetDeviceCaps(hdc, LOGPIXELSX);
    media.device_width = GetDeviceCaps(hdc, HORZRES);
    media.device_height = GetDeviceCaps(hdc, VERTRES);
}