#pragma once
#include "types.h"
#include "attributes.h"
#include "css.h"
#include "web_color.h"
#include "borders.h"

class background
{
public:
    std::wstring  m_image;
    std::wstring  m_baseurl;
    web_color m_color;
    background_attachment m_attachment;
    css_position m_position;
    background_repeat m_repeat;
    background_box m_clip;
    background_box m_origin;
    css_border_radius m_radius;

public:

    background(void)
    {
        m_attachment = background_attachment_scroll;
        m_repeat = background_repeat_repeat;
        m_clip = background_box_border;
        m_origin = background_box_padding;
        m_color.alpha = 0;
        m_color.red = 0;
        m_color.green = 0;
        m_color.blue = 0;
    }

    background(const background& val)
    {
        m_image = val.m_image;
        m_baseurl = val.m_baseurl;
        m_color = val.m_color;
        m_attachment = val.m_attachment;
        m_position = val.m_position;
        m_repeat = val.m_repeat;
        m_clip = val.m_clip;
        m_origin = val.m_origin;
    }

    ~background(void)
    {
    }

    background& operator=(const background& val)
    {
        m_image = val.m_image;
        m_baseurl = val.m_baseurl;
        m_color = val.m_color;
        m_attachment = val.m_attachment;
        m_position = val.m_position;
        m_repeat = val.m_repeat;
        m_clip = val.m_clip;
        m_origin = val.m_origin;
        return *this;
    }

};

class background_paint
{
public:
    std::shared_ptr<Gdiplus::Bitmap> image;
    background_attachment attachment;
    background_repeat repeat;
    web_color color;
    position clip_box;
    position origin_box;
    position border_box;
    css_border_radius border_radius;
    size  image_size;
    int  position_x;
    int  position_y;
    bool  is_root;

public:

    background_paint() : color(0, 0, 0, 0)
    {
        position_x = 0;
        position_y = 0;
        attachment = background_attachment_scroll;
        repeat = background_repeat_repeat;
        is_root = false;
    }

    background_paint(const background_paint& val)
    {
        image = val.image;
        attachment = val.attachment;
        repeat = val.repeat;
        color = val.color;
        clip_box = val.clip_box;
        origin_box = val.origin_box;
        border_box = val.border_box;
        border_radius = val.border_radius;
        image_size = val.image_size;
        position_x = val.position_x;
        position_y = val.position_y;
        is_root = val.is_root;
    }

};

