//  This may look like C code, but it is really -*- C++ -*-

//  ------------------------------------------------------------------
//  The Goldware Library
//  Copyright (C) 2026 The GoldED+ team
//  ------------------------------------------------------------------
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Library General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Library General Public License for more details.
//
//  You should have received a copy of the GNU Library General Public
//  License along with this program; if not, write to the Free
//  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
//  MA 02111-1307, USA
//  ------------------------------------------------------------------
//  Images in memory - see gimage.h.
//  ------------------------------------------------------------------

#include <gimage.h>

#if defined(GOLD_IMAGES)

#include <string.h>
#include <stdlib.h>


//  ------------------------------------------------------------------
//  stb_image, compiled here and nowhere else. Only the formats a
//  message is likely to carry; no stdio, since the bytes come from the
//  message and never from a file of their own. The header is written
//  in C and tripped over -pedantic and a few of the warnings the tree
//  is built with; it is not ours to tidy.

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_GIF
#define STBI_ONLY_BMP
#define STBI_NO_THREAD_LOCALS

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wshadow"
#endif

//  gdefs.h spells the line ending as NL, and stb_image has a local
//  of that name; the macro is not wanted in this file anyway.
#undef NL
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif


//  ------------------------------------------------------------------
//  The format by its first bytes. stb_image would tell us as well, but
//  it has to be asked before the size, and the name is wanted on the
//  screen.

static const char* g_image_magic(const unsigned char* d, size_t n)
{
    if(n >= 8 and memcmp(d, "\x89PNG\r\n\x1a\n", 8) == 0)
        return "png";
    if(n >= 3 and d[0] == 0xFF and d[1] == 0xD8 and d[2] == 0xFF)
        return "jpeg";
    if(n >= 6 and (memcmp(d, "GIF87a", 6) == 0 or memcmp(d, "GIF89a", 6) == 0))
        return "gif";
    if(n >= 2 and d[0] == 'B' and d[1] == 'M')
        return "bmp";
    return NULL;
}


//  ------------------------------------------------------------------

bool g_image_probe(const unsigned char* data, size_t len, int* width, int* height, std::string* format)
{
    if(data == NULL or len == 0)
        return false;

    const char* fmt = g_image_magic(data, len);
    if(fmt == NULL)
        return false;

    int w = 0, h = 0, comp = 0;
    if(not stbi_info_from_memory(data, (int)len, &w, &h, &comp))
        return false;
    if(w <= 0 or h <= 0)
        return false;

    if(width)  *width  = w;
    if(height) *height = h;
    if(format) *format = fmt;
    return true;
}


//  ------------------------------------------------------------------

bool g_image_load(const unsigned char* data, size_t len, GImage& img, bool decode)
{
    img.rgba.clear();
    img.file.clear();
    img.width = img.height = 0;
    img.format.erase();

    if(not g_image_probe(data, len, &img.width, &img.height, &img.format))
        return false;

    img.file.assign(data, data + len);

    return decode ? g_image_decode(img) : true;
}


//  ------------------------------------------------------------------

bool g_image_decode(GImage& img)
{
    if(img.has_pixels())
        return true;
    if(img.file.empty())
        return false;

    int w = 0, h = 0, comp = 0;
    unsigned char* px = stbi_load_from_memory(&img.file[0], (int)img.file.size(), &w, &h, &comp, 4);
    if(px == NULL or w <= 0 or h <= 0)
    {
        if(px)
            stbi_image_free(px);
        return false;
    }

    img.width  = w;
    img.height = h;
    img.rgba.assign(px, px + (size_t)w * (size_t)h * 4);
    stbi_image_free(px);
    return true;
}


//  ------------------------------------------------------------------

void g_image_scale_rgb(const GImage& img, int width, int height, std::vector<unsigned char>& rgb)
{
    rgb.clear();
    if(not img.has_pixels() or width <= 0 or height <= 0)
        return;

    rgb.resize((size_t)width * (size_t)height * 3);

    const int sw = img.width, sh = img.height;
    const unsigned char* src = &img.rgba[0];

    for(int y = 0; y < height; y++)
    {
        //  The source rows this destination row covers - at least one.
        int y0 = (int)((long)y * sh / height);
        int y1 = (int)((long)(y + 1) * sh / height);
        if(y1 <= y0) y1 = y0 + 1;
        if(y1 > sh) y1 = sh;

        for(int x = 0; x < width; x++)
        {
            int x0 = (int)((long)x * sw / width);
            int x1 = (int)((long)(x + 1) * sw / width);
            if(x1 <= x0) x1 = x0 + 1;
            if(x1 > sw) x1 = sw;

            unsigned long r = 0, g = 0, b = 0, n = 0;
            for(int yy = y0; yy < y1; yy++)
            {
                const unsigned char* p = src + ((size_t)yy * sw + x0) * 4;
                for(int xx = x0; xx < x1; xx++, p += 4)
                {
                    //  Alpha over black: the channel scaled by it.
                    unsigned a = p[3];
                    r += (p[0] * a) / 255;
                    g += (p[1] * a) / 255;
                    b += (p[2] * a) / 255;
                    n++;
                }
            }
            unsigned char* d = &rgb[((size_t)y * width + x) * 3];
            d[0] = (unsigned char)(r / n);
            d[1] = (unsigned char)(g / n);
            d[2] = (unsigned char)(b / n);
        }
    }
}


//  ------------------------------------------------------------------

std::string g_image_base64(const unsigned char* data, size_t len)
{
    static const char tab[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    size_t i = 0;
    while(i + 2 < len)
    {
        unsigned v = ((unsigned)data[i] << 16) | ((unsigned)data[i+1] << 8) | data[i+2];
        out += tab[(v >> 18) & 63];
        out += tab[(v >> 12) & 63];
        out += tab[(v >> 6) & 63];
        out += tab[v & 63];
        i += 3;
    }
    if(i + 1 == len)
    {
        unsigned v = (unsigned)data[i] << 16;
        out += tab[(v >> 18) & 63];
        out += tab[(v >> 12) & 63];
        out += "==";
    }
    else if(i + 2 == len)
    {
        unsigned v = ((unsigned)data[i] << 16) | ((unsigned)data[i+1] << 8);
        out += tab[(v >> 18) & 63];
        out += tab[(v >> 12) & 63];
        out += tab[(v >> 6) & 63];
        out += '=';
    }
    return out;
}

//  ------------------------------------------------------------------

static void g_image_png_sink(void* ctx, void* data, int size)
{
    std::vector<unsigned char>* out = (std::vector<unsigned char>*)ctx;
    const unsigned char* p = (const unsigned char*)data;
    out->insert(out->end(), p, p + size);
}


bool g_image_encode(const GImage& img, int sx, int sy, int sw, int sh, int maxw, int maxh, bool jpeg, std::vector<unsigned char>& png)
{
    png.clear();
    if(not img.has_pixels())
        return false;

    if(sw <= 0 or sh <= 0)
    {
        sx = sy = 0;
        sw = img.width;
        sh = img.height;
    }
    if(sx < 0) sx = 0;
    if(sy < 0) sy = 0;
    if(sx + sw > img.width)  sw = img.width  - sx;
    if(sy + sh > img.height) sh = img.height - sy;
    if(sw <= 0 or sh <= 0)
        return false;

    //  The part, as an image of its own, then scaled.
    GImage part;
    part.width  = sw;
    part.height = sh;
    part.rgba.resize((size_t)sw * sh * 4);
    for(int y = 0; y < sh; y++)
        memcpy(&part.rgba[(size_t)y * sw * 4], &img.rgba[((size_t)(sy + y) * img.width + sx) * 4], (size_t)sw * 4);

    int W = sw, H = sh;
    if(maxw > 0 and maxh > 0 and (W > maxw or H > maxh))
    {
        double s = (double)maxw / W;
        if((double)maxh / H < s)
            s = (double)maxh / H;
        W = (int)(W * s);
        H = (int)(H * s);
        if(W < 1) W = 1;
        if(H < 1) H = 1;
    }

    std::vector<unsigned char> rgb;
    g_image_scale_rgb(part, W, H, rgb);
    if(rgb.empty())
        return false;

    if(jpeg)
        return stbi_write_jpg_to_func(g_image_png_sink, &png, W, H, 3, &rgb[0], 85) != 0;
    return stbi_write_png_to_func(g_image_png_sink, &png, W, H, 3, &rgb[0], W * 3) != 0;
}

#endif  // GOLD_IMAGES

//  ------------------------------------------------------------------
