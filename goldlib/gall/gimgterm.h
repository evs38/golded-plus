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
//  Pictures on a terminal.
//
//  Three protocols are in use: Sixel (xterm, foot, mlterm, mintty,
//  Windows Terminal), the kitty graphics protocol (kitty, Ghostty,
//  WezTerm, Konsole) and iTerm2's inline images (iTerm2, WezTerm,
//  mintty). Which one a terminal speaks is found from the environment
//  and, for Sixel, from its answer to the primary device attributes
//  query. Sixel takes pixels and nothing else, so the image is scaled
//  here and its colours cut to a palette; the other two take the file
//  and scale it themselves to a rectangle of cells.
//
//  A picture is described by a placement: the cells it covers and the
//  part of the image drawn there. The viewer draws a whole image into
//  a rectangle; an image drawn inline with the text will draw parts
//  of one as the text scrolls, which is what the source rectangle in
//  the placement is for.
//  ------------------------------------------------------------------

#ifndef __gimgterm_h
#define __gimgterm_h


//  ------------------------------------------------------------------

#include <gimage.h>

#if defined(GOLD_IMAGES)


//  ------------------------------------------------------------------

enum GImgProto
{
    GIMG_NONE = 0,      //  the terminal cannot draw a picture
    GIMG_AUTO,          //  find out
    GIMG_SIXEL,
    GIMG_KITTY,
    GIMG_ITERM2
};


//  What was found out about the terminal, once.
struct GImgTerm
{
    GImgProto proto;    //  never GIMG_AUTO after g_imgterm_probe()
    int cell_width;     //  one cell in pixels; 0 when the terminal
    int cell_height;    //  would not say
    int fd_in;          //  the tty, for queries
    int fd_out;

    GImgTerm() : proto(GIMG_NONE), cell_width(0), cell_height(0), fd_in(0), fd_out(1) {}
};


//  Where a picture goes and what part of it.
struct GImgPlace
{
    int row, col;       //  top left cell, zero-based
    int cols, rows;     //  cells covered
    int sx, sy;         //  the part of the image drawn there, in
    int sw, sh;         //  pixels; sw or sh of 0 means the whole image
    int id;             //  tells placements apart where the protocol
                        //  keeps them (kitty); 1 and up
    int z;              //  kitty: 0 draws over the text, -1 under it
                        //  (over the cell background) - what a picture
                        //  inline with the text wants, so that a window
                        //  opened on top of it still shows
    bool transmit;      //  kitty: send the image with this placement;
                        //  false places an image sent earlier under
                        //  the same id - a scrolled picture is not sent
                        //  again
    int full_rows;      //  the rows the whole picture takes; what a
                        //  raw kitty image is scaled to when sent

    GImgPlace() : row(0), col(0), cols(0), rows(0), sx(0), sy(0), sw(0), sh(0), id(1), z(0), transmit(true), full_rows(0) {}
};


//  Find out what the terminal speaks and how big its cells are.
//  'wanted' is what the configuration asks for; GIMG_AUTO means look.
void g_imgterm_probe(GImgTerm& term, GImgProto wanted);

//  The protocol by name ("auto", "none", "sixel", "kitty", "iterm2")
//  and back.
GImgProto   g_imgterm_parse(const char* name);
const char* g_imgterm_name(GImgProto proto);

//  How many cells a w x h image takes inside max_cols x max_rows with
//  its shape kept and never enlarged. Assumes a 1:2 cell where the
//  terminal did not say.
void g_imgterm_fit(const GImgTerm& term, int width, int height, int max_cols, int max_rows, int* cols, int* rows);

//  The bytes that draw the picture, cursor positioning included.
//  False when the protocol cannot draw this image (no pixels for
//  Sixel, say).
bool g_imgterm_encode(const GImgTerm& term, const GImage& img, const GImgPlace& place, std::string& out);

//  The bytes that take a placed picture off the screen again where
//  the protocol keeps pictures apart from the cells; empty where
//  redrawing the cells is enough.
//  With 'data' the image itself is let go of as well (kitty keeps it
//  for further placements until told otherwise).
void g_imgterm_erase(const GImgTerm& term, const GImgPlace& place, std::string& out, bool data = false);

#endif  // GOLD_IMAGES


//  ------------------------------------------------------------------

#endif

//  ------------------------------------------------------------------
