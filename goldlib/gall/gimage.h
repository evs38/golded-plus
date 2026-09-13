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
//  An image in memory: the file it came from, and its pixels.
//
//  Decoding is done by stb_image (public domain, one header, kept
//  beside this file); PNG, JPEG, GIF and BMP are compiled in. The
//  file bytes are kept as well, because two of the terminal protocols
//  take the file as it is and only Sixel wants pixels.
//
//  Everything here is compiled only with GOLD_IMAGES; the builds that
//  cannot draw a picture - DOS, OS/2, the classic Windows console -
//  see an empty translation unit and no references to it.
//  ------------------------------------------------------------------

#ifndef __gimage_h
#define __gimage_h


//  ------------------------------------------------------------------

#include <gdefs.h>

#if defined(GOLD_IMAGES)

#include <string>
#include <vector>


//  ------------------------------------------------------------------

class GImage
{
public:

    int width;                          //  pixels
    int height;
    std::string format;                 //  "png", "jpeg", "gif", "bmp"
    std::vector<unsigned char> file;    //  the bytes as received
    std::vector<unsigned char> rgba;    //  width*height*4, when decoded

    GImage() : width(0), height(0) {}

    bool has_pixels() const { return not rgba.empty(); }
};


//  What the bytes are, without decoding them: the format by its magic
//  and the size from the header. False for anything that is not an
//  image we can read.
bool g_image_probe(const unsigned char* data, size_t len, int* width, int* height, std::string* format);

//  Take the bytes as the image's file, and decode them to RGBA.
//  Only 'file' and the size are filled when 'decode' is false.
bool g_image_load(const unsigned char* data, size_t len, GImage& img, bool decode = true);

//  Decode the file kept in 'img', if that has not been done.
bool g_image_decode(GImage& img);

//  Resample the decoded pixels to the given size, as RGB (three bytes
//  a pixel), the alpha composed over black - a terminal's background
//  is unknown, and black is what most of them are. Averages the source
//  when shrinking, repeats it when growing.
void g_image_scale_rgb(const GImage& img, int width, int height, std::vector<unsigned char>& rgb);

//  Base64 of a run of bytes, no line breaks.
std::string g_image_base64(const unsigned char* data, size_t len);

//  A part of the decoded image - sx,sy,sw,sh in pixels, the whole image
//  when sw or sh is 0 - scaled to fit maxw x maxh (never enlarged) and
//  written as a file again, for the protocol that takes only files and
//  cannot crop: a JPEG where the source was one (the picture is lossy
//  already and the file a fifth of the size), a PNG otherwise. False
//  without pixels.
bool g_image_encode(const GImage& img, int sx, int sy, int sw, int sh, int maxw, int maxh, bool jpeg, std::vector<unsigned char>& out);

#endif  // GOLD_IMAGES


//  ------------------------------------------------------------------

#endif

//  ------------------------------------------------------------------
