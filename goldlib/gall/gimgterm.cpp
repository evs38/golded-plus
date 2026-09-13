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
//  Pictures on a terminal - see gimgterm.h.
//  ------------------------------------------------------------------

#include <gimgterm.h>

#if defined(GOLD_IMAGES)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__WIN32__) || defined(_WIN32)
#include <windows.h>
#include <io.h>
#define GIMG_WINDOWS 1
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <termios.h>
#endif
#include <gstrall.h>


//  ------------------------------------------------------------------
//  Asking the terminal something and reading its answer. The tty is in
//  raw mode already - curses put it there - so the answer arrives byte
//  by byte with no line to wait for; it ends in a known final byte,
//  and a terminal that does not answer at all is given a little time
//  and then taken at its silence. Whatever else is waiting on the
//  input - a key pressed meanwhile - is kept and pushed back to the
//  caller's buffer... except that it cannot be: curses holds no
//  interface for that. A key typed during the few milliseconds of a
//  query is lost. The queries run once per session.

static bool g_imgterm_query(const GImgTerm& term, const char* query, char final, std::string& answer, int ms)
{
    answer.erase();

#if defined(GIMG_WINDOWS)
    //  The console's answers would arrive as input records, not as
    //  bytes on a descriptor; the terminal is known from the
    //  environment instead, and nothing is asked.
    (void)term; (void)query; (void)final; (void)ms;
    return false;
#else
    if(term.fd_in < 0 or term.fd_out < 0)
        return false;

    size_t qlen = strlen(query);
    if(write(term.fd_out, query, qlen) != (ssize_t)qlen)
        return false;

    struct timeval tv;
    tv.tv_sec  = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;

    while(true)
    {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(term.fd_in, &rfds);

        int r = select(term.fd_in + 1, &rfds, NULL, NULL, &tv);
        if(r <= 0)
            break;

        char c;
        if(read(term.fd_in, &c, 1) != 1)
            break;
        answer += c;
        if(c == final)
            return true;
        if(answer.length() > 256)
            break;
    }

    return false;
#endif
}


//  ------------------------------------------------------------------

GImgProto g_imgterm_parse(const char* name)
{
    if(name == NULL or *name == NUL)
        return GIMG_AUTO;
    if(strieql(name, "auto"))
        return GIMG_AUTO;
    if(strieql(name, "none") or strieql(name, "no") or strieql(name, "off"))
        return GIMG_NONE;
    if(strieql(name, "sixel"))
        return GIMG_SIXEL;
    if(strieql(name, "kitty"))
        return GIMG_KITTY;
    if(strieql(name, "iterm2") or strieql(name, "iterm"))
        return GIMG_ITERM2;
    return GIMG_AUTO;
}


const char* g_imgterm_name(GImgProto proto)
{
    switch(proto)
    {
    case GIMG_SIXEL:  return "sixel";
    case GIMG_KITTY:  return "kitty";
    case GIMG_ITERM2: return "iterm2";
    case GIMG_AUTO:   return "auto";
    default:          return "none";
    }
}


//  ------------------------------------------------------------------
//  Which protocol, from what the environment says about the terminal.
//  GIMG_AUTO when it says nothing.

static GImgProto g_imgterm_from_env()
{
    const char* e;

    //  kitty sets this for every program it starts; nothing else does.
    if((e = getenv("KITTY_WINDOW_ID")) != NULL and *e)
        return GIMG_KITTY;

    //  Windows Terminal, which draws Sixel since 1.22 and neither of
    //  the other two; it names itself to what it starts.
    if((e = getenv("WT_SESSION")) != NULL and *e)
        return GIMG_SIXEL;

    //  What the terminal calls itself. Over ssh TERM_PROGRAM does not
    //  travel, but LC_TERMINAL does where the locale variables are
    //  passed on, and iTerm2 sets it for exactly that reason.
    const char* names[] = { "TERM_PROGRAM", "LC_TERMINAL", NULL };
    for(int n = 0; names[n]; n++)
    {
        e = getenv(names[n]);
        if(e == NULL or *e == NUL)
            continue;
        if(striinc("iTerm", e))
            return GIMG_ITERM2;
        if(striinc("WezTerm", e))
            return GIMG_ITERM2;
        if(striinc("ghostty", e))
            return GIMG_KITTY;
        if(striinc("mintty", e))
            return GIMG_ITERM2;
        if(striinc("kitty", e))
            return GIMG_KITTY;
    }

    if((e = getenv("TERM")) != NULL and *e)
    {
        if(striinc("kitty", e) or striinc("ghostty", e))
            return GIMG_KITTY;
        if(striinc("foot", e) or striinc("mlterm", e) or striinc("yaft", e))
            return GIMG_SIXEL;
    }

    return GIMG_AUTO;
}


//  ------------------------------------------------------------------
//  The terminal by its own name. XTVERSION - "CSI > q" - is answered
//  with "DCS > | name version ST" by xterm, iTerm2, kitty, WezTerm,
//  foot, Ghostty and others, and ignored by the rest. Over ssh this
//  is the only thing that tells iTerm2 from an xterm: the environment
//  does not travel, and iTerm2 answers the Sixel question too, so it
//  would be drawn with Sixel where its own protocol does better.

static GImgProto g_imgterm_by_xtversion(const GImgTerm& term)
{
    std::string answer;
    if(not g_imgterm_query(term, "\x1b[>q", '\\', answer, 300))
        return GIMG_AUTO;

    size_t bar = answer.find("|");
    if(bar == std::string::npos)
        return GIMG_AUTO;
    const char* name = answer.c_str() + bar + 1;

    if(striinc("iTerm", name) or striinc("WezTerm", name) or striinc("mintty", name))
        return GIMG_ITERM2;
    if(striinc("kitty", name) or striinc("ghostty", name) or striinc("Konsole", name))
        return GIMG_KITTY;
    if(striinc("XTerm", name) or striinc("foot", name) or striinc("mlterm", name))
        return GIMG_SIXEL;
    return GIMG_AUTO;
}


//  ------------------------------------------------------------------
//  Whether the terminal admits to Sixel in its primary device
//  attributes: a "4" among the parameters of "CSI ? 6x ; ... c".

static bool g_imgterm_sixel_by_da(const GImgTerm& term)
{
    std::string answer;
    if(not g_imgterm_query(term, "\x1b[c", 'c', answer, 300))
        return false;

    //  Only the parameter list between the '?' and the final byte.
    size_t q = answer.find('?');
    if(q == std::string::npos)
        return false;

    const char* p = answer.c_str() + q + 1;
    while(*p and *p != 'c')
    {
        char* end;
        long v = strtol(p, &end, 10);
        if(end == p)
            break;
        if(v == 4)
            return true;
        p = (*end == ';') ? end + 1 : end;
    }
    return false;
}


//  ------------------------------------------------------------------
//  The size of a cell in pixels. The window size ioctl carries it on
//  most terminals; where those fields are zero, XTWINOPS 16 asks for
//  it outright: "CSI 16 t" is answered with "CSI 6 ; height ; width t".

static void g_imgterm_cell_size(GImgTerm& term)
{
    term.cell_width = term.cell_height = 0;

#if defined(GIMG_WINDOWS)
    //  The console font's cell, as the console reports it. Windows
    //  Terminal answers through its conpty with the size it draws at.
    {
        CONSOLE_FONT_INFO fi;
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if(h != INVALID_HANDLE_VALUE and GetCurrentConsoleFont(h, FALSE, &fi))
        {
            COORD sz = GetConsoleFontSize(h, fi.nFont);
            if(sz.X > 0 and sz.Y > 0)
            {
                term.cell_width  = sz.X;
                term.cell_height = sz.Y;
            }
        }
    }
    return;
#endif

#if defined(TIOCGWINSZ)
    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    if(ioctl(term.fd_out, TIOCGWINSZ, &ws) == 0 or ioctl(term.fd_in, TIOCGWINSZ, &ws) == 0)
    {
        if(ws.ws_col and ws.ws_row and ws.ws_xpixel and ws.ws_ypixel)
        {
            term.cell_width  = ws.ws_xpixel / ws.ws_col;
            term.cell_height = ws.ws_ypixel / ws.ws_row;
        }
    }
#endif

    if(term.cell_width > 0 and term.cell_height > 0)
        return;

    std::string answer;
    if(g_imgterm_query(term, "\x1b[16t", 't', answer, 300))
    {
        int kind = 0, h = 0, w = 0;
        const char* p = strchr(answer.c_str(), '[');
        if(p and sscanf(p + 1, "%d;%d;%d", &kind, &h, &w) == 3 and kind == 6 and w > 0 and h > 0)
        {
            term.cell_width  = w;
            term.cell_height = h;
        }
    }
}


//  ------------------------------------------------------------------

void g_imgterm_probe(GImgTerm& term, GImgProto wanted)
{
    term.fd_in  = 0;
    term.fd_out = 1;

#if defined(GIMG_WINDOWS)
    if(not _isatty(_fileno(stdout)))
#else
    if(not isatty(term.fd_out))
#endif
    {
        term.proto = GIMG_NONE;
        return;
    }

    if(wanted == GIMG_AUTO)
    {
        wanted = g_imgterm_from_env();
        if(wanted == GIMG_AUTO)
            wanted = g_imgterm_by_xtversion(term);
        if(wanted == GIMG_AUTO)
            wanted = g_imgterm_sixel_by_da(term) ? GIMG_SIXEL : GIMG_NONE;
    }

    term.proto = wanted;

    if(term.proto != GIMG_NONE)
        g_imgterm_cell_size(term);
}


//  ------------------------------------------------------------------

void g_imgterm_fit(const GImgTerm& term, int width, int height, int max_cols, int max_rows, int* cols, int* rows)
{
    int cw = term.cell_width  > 0 ? term.cell_width  : 8;
    int ch = term.cell_height > 0 ? term.cell_height : 16;

    if(width <= 0 or height <= 0 or max_cols <= 0 or max_rows <= 0)
    {
        *cols = *rows = 0;
        return;
    }

    //  The scale that fits both ways, and never more than one: a
    //  small picture is drawn at its size, not blown up.
    double sx = (double)(max_cols * cw) / width;
    double sy = (double)(max_rows * ch) / height;
    double s  = sx < sy ? sx : sy;
    if(s > 1.0)
        s = 1.0;

    int c = (int)((width  * s + cw - 1) / cw);
    int r = (int)((height * s + ch - 1) / ch);
    if(c < 1) c = 1;
    if(r < 1) r = 1;
    if(c > max_cols) c = max_cols;
    if(r > max_rows) r = max_rows;

    *cols = c;
    *rows = r;
}


//  ------------------------------------------------------------------
//  Cursor to the placement's top left cell - the rows and columns of
//  the escape sequence count from one.

static void g_imgterm_goto(const GImgPlace& place, std::string& out)
{
    char buf[32];
    sprintf(buf, "\x1b[%d;%dH", place.row + 1, place.col + 1);
    out += buf;
}


//  ------------------------------------------------------------------
//  Sixel. The pixels are scaled to the cells' size, cut to a palette
//  of at most 256 colours by median cut over a 5-6-5 histogram, and
//  written six rows at a time, one pass per colour present in the
//  band, runs compressed with the repeat introducer.

namespace
{

struct SixelBox
{
    int r0, r1, g0, g1, b0, b1;     //  inclusive, in 5/6/5 bins
    long count;
    bool solid;                     //  cannot be split any further
};

}


static inline int sixel_bin(int r, int g, int b)
{
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}


static bool g_imgterm_sixel(const GImgTerm& term, const GImage& img, const GImgPlace& place, std::string& out)
{
    if(not img.has_pixels())
        return false;

    int cw = term.cell_width  > 0 ? term.cell_width  : 8;
    int ch = term.cell_height > 0 ? term.cell_height : 16;

    //  The part of the image to draw, and the size to draw it at.
    GImage part;
    const GImage* src = &img;
    if(place.sw > 0 and place.sh > 0 and (place.sx or place.sy or place.sw != img.width or place.sh != img.height))
    {
        part.width  = place.sw;
        part.height = place.sh;
        part.rgba.resize((size_t)place.sw * place.sh * 4);
        for(int y = 0; y < place.sh; y++)
        {
            int syy = place.sy + y;
            if(syy < 0 or syy >= img.height)
                continue;
            for(int x = 0; x < place.sw; x++)
            {
                int sxx = place.sx + x;
                if(sxx < 0 or sxx >= img.width)
                    continue;
                memcpy(&part.rgba[((size_t)y * place.sw + x) * 4], &img.rgba[((size_t)syy * img.width + sxx) * 4], 4);
            }
        }
        src = &part;
    }

    int W = place.cols * cw;
    int H = place.rows * ch;
    if(W <= 0 or H <= 0)
        return false;

    //  Keep the shape inside the cells rather than stretch to them.
    double s = (double)W / src->width;
    if((double)H / src->height < s)
        s = (double)H / src->height;
    W = (int)(src->width  * s);
    H = (int)(src->height * s);
    if(W < 1) W = 1;
    if(H < 1) H = 1;

    std::vector<unsigned char> rgb;
    g_image_scale_rgb(*src, W, H, rgb);
    if(rgb.empty())
        return false;

    //  Histogram over 5-6-5 bins.
    std::vector<long> hist(65536, 0);
    const size_t npix = (size_t)W * H;
    for(size_t i = 0; i < npix; i++)
        hist[sixel_bin(rgb[i*3], rgb[i*3+1], rgb[i*3+2])]++;

    //  Median cut. A box is split along its longest side at the bin
    //  where half its weight lies, until there are 256 or nothing
    //  left to split.
    std::vector<SixelBox> boxes;
    {
        SixelBox b;
        b.r0 = 0; b.r1 = 31; b.g0 = 0; b.g1 = 63; b.b0 = 0; b.b1 = 31;
        b.count = (long)npix;
        b.solid = false;
        boxes.push_back(b);
    }

    while(boxes.size() < 256)
    {
        //  The heaviest box that can still be split.
        int best = -1;
        long bestcount = 0;
        for(size_t n = 0; n < boxes.size(); n++)
        {
            const SixelBox& b = boxes[n];
            if(b.count > bestcount and not b.solid and ((b.r1 > b.r0) or (b.g1 > b.g0) or (b.b1 > b.b0)))
            {
                best = (int)n;
                bestcount = b.count;
            }
        }
        if(best < 0)
            break;

        SixelBox b = boxes[best];
        int lr = b.r1 - b.r0, lg = b.g1 - b.g0, lb = b.b1 - b.b0;
        //  Green weighs most to the eye; the 6-bit axis is doubled
        //  down to the same scale as the others before comparing.
        int axis = (lg >= lr * 2 and lg >= lb * 2) ? 1 : (lr >= lb ? 0 : 2);

        //  Weight along the axis.
        int lo = axis == 0 ? b.r0 : axis == 1 ? b.g0 : b.b0;
        int hi = axis == 0 ? b.r1 : axis == 1 ? b.g1 : b.b1;
        std::vector<long> w(hi - lo + 1, 0);
        for(int r = b.r0; r <= b.r1; r++)
            for(int g = b.g0; g <= b.g1; g++)
                for(int bb = b.b0; bb <= b.b1; bb++)
                {
                    long c = hist[(r << 11) | (g << 5) | bb];
                    if(c)
                        w[(axis == 0 ? r : axis == 1 ? g : bb) - lo] += c;
                }

        long half = b.count / 2, acc = 0;
        int cut = lo;
        for(int i = lo; i < hi; i++)
        {
            acc += w[i - lo];
            cut = i;
            if(acc >= half)
                break;
        }
        //  Both halves must hold something, or the split is a waste.
        long left = 0;
        for(int i = lo; i <= cut; i++)
            left += w[i - lo];
        if(left == 0 or left == b.count)
        {
            //  Move the cut to the first non-empty boundary instead.
            left = 0;
            cut = lo - 1;
            for(int i = lo; i < hi; i++)
            {
                left += w[i - lo];
                if(left > 0 and left < b.count)
                {
                    cut = i;
                    break;
                }
            }
            if(cut < lo)
            {
                //  Nothing to split on this axis. The box keeps its
                //  bounds - every bin in it still has to map to its
                //  colour - and is left alone from here on.
                boxes[best].solid = true;
                continue;
            }
        }

        SixelBox a = b, c = b;
        a.solid = c.solid = false;
        if(axis == 0)      { a.r1 = cut; c.r0 = cut + 1; }
        else if(axis == 1) { a.g1 = cut; c.g0 = cut + 1; }
        else               { a.b1 = cut; c.b0 = cut + 1; }
        a.count = left;
        c.count = b.count - left;
        boxes[best] = a;
        boxes.push_back(c);
    }

    //  The palette: the weighted mean of each box; and the map from
    //  bin to palette index.
    std::vector<unsigned char> pal(boxes.size() * 3);
    std::vector<unsigned char> binmap(65536, 0);
    for(size_t n = 0; n < boxes.size(); n++)
    {
        const SixelBox& b = boxes[n];
        double sr = 0, sg = 0, sb = 0;
        long cnt = 0;
        for(int r = b.r0; r <= b.r1; r++)
            for(int g = b.g0; g <= b.g1; g++)
                for(int bb = b.b0; bb <= b.b1; bb++)
                {
                    int bin = (r << 11) | (g << 5) | bb;
                    binmap[bin] = (unsigned char)n;
                    long c = hist[bin];
                    if(c)
                    {
                        sr += (double)c * ((r << 3) | 4);
                        sg += (double)c * ((g << 2) | 2);
                        sb += (double)c * ((bb << 3) | 4);
                        cnt += c;
                    }
                }
        if(cnt == 0)
        {
            sr = ((b.r0 + b.r1) << 2) | 4;
            sg = ((b.g0 + b.g1) << 1) | 2;
            sb = ((b.b0 + b.b1) << 2) | 4;
            cnt = 1;
        }
        pal[n*3]   = (unsigned char)(sr / cnt);
        pal[n*3+1] = (unsigned char)(sg / cnt);
        pal[n*3+2] = (unsigned char)(sb / cnt);
    }

    //  Every pixel as a palette index.
    std::vector<unsigned char> idx(npix);
    for(size_t i = 0; i < npix; i++)
        idx[i] = binmap[sixel_bin(rgb[i*3], rgb[i*3+1], rgb[i*3+2])];

    //  And out it goes.
    g_imgterm_goto(place, out);

    char buf[64];
    sprintf(buf, "\x1bP0;1;0q\"1;1;%d;%d", W, H);
    out += buf;
    for(size_t n = 0; n < boxes.size(); n++)
    {
        sprintf(buf, "#%u;2;%u;%u;%u", (unsigned)n, (pal[n*3] * 100u) / 255u, (pal[n*3+1] * 100u) / 255u, (pal[n*3+2] * 100u) / 255u);
        out += buf;
    }

    std::vector<unsigned char> band(W);
    std::vector<char> seen(boxes.size());
    for(int y0 = 0; y0 < H; y0 += 6)
    {
        int rows = (H - y0 < 6) ? H - y0 : 6;

        //  The colours present in this band.
        memset(&seen[0], 0, seen.size());
        for(int yy = 0; yy < rows; yy++)
            for(int x = 0; x < W; x++)
                seen[idx[(size_t)(y0 + yy) * W + x]] = 1;

        bool first = true;
        for(size_t n = 0; n < boxes.size(); n++)
        {
            if(not seen[n])
                continue;

            //  The six bits of each column that are this colour.
            for(int x = 0; x < W; x++)
            {
                unsigned char bits = 0;
                for(int yy = 0; yy < rows; yy++)
                    if(idx[(size_t)(y0 + yy) * W + x] == n)
                        bits |= (unsigned char)(1 << yy);
                band[x] = bits;
            }

            if(not first)
                out += '$';
            first = false;
            sprintf(buf, "#%u", (unsigned)n);
            out += buf;

            //  Runs. Trailing blanks are left off: the row ends there
            //  anyway.
            int last = W - 1;
            while(last >= 0 and band[last] == 0)
                last--;
            for(int x = 0; x <= last; )
            {
                int run = 1;
                while(x + run <= last and band[x + run] == band[x])
                    run++;
                char c = (char)(63 + band[x]);
                if(run > 3)
                {
                    sprintf(buf, "!%d%c", run, c);
                    out += buf;
                }
                else
                    out.append((size_t)run, c);
                x += run;
            }
        }
        out += '-';
    }
    out += "\x1b\\";

    return true;
}


//  ------------------------------------------------------------------
//  kitty. A PNG goes as it is; anything else goes as raw RGB at the
//  size of the cells, or at its own size where the cell size is not
//  known. The payload is base64 in chunks of 4096, the control data
//  on the first. Placed at the cursor, which is told to stay put.

static bool g_imgterm_kitty(const GImgTerm& term, const GImage& img, const GImgPlace& place, std::string& out)
{
    char control[256];
    char rect[80];
    bool crop = (place.sw > 0 and place.sh > 0);

    //  Placing an image sent before: no payload, the source rectangle
    //  in the coordinates of what was sent - the file's own for a PNG,
    //  the scaled raw image's otherwise.
    if(not place.transmit)
    {
        int x = place.sx, y = place.sy, w = place.sw, h = place.sh;
        if(crop and img.format != "png" and term.cell_width > 0 and term.cell_height > 0)
        {
            int cw = term.cell_width * place.cols;
            int chh = term.cell_height * (place.full_rows > 0 ? place.full_rows : place.rows);
            if(img.width > cw or img.height > chh)
            {
                double s = (double)cw / img.width;
                if((double)chh / img.height < s) s = (double)chh / img.height;
                x = (int)(x * s); y = (int)(y * s); w = (int)(w * s); h = (int)(h * s);
                if(w < 1) w = 1;
                if(h < 1) h = 1;
            }
        }
        else if(crop and img.format != "png")
        {
            int cw = 8 * place.cols;
            int chh = 16 * (place.full_rows > 0 ? place.full_rows : place.rows);
            if(img.width > cw or img.height > chh)
            {
                double s = (double)cw / img.width;
                if((double)chh / img.height < s) s = (double)chh / img.height;
                x = (int)(x * s); y = (int)(y * s); w = (int)(w * s); h = (int)(h * s);
                if(w < 1) w = 1;
                if(h < 1) h = 1;
            }
        }
        g_imgterm_goto(place, out);
        sprintf(control, "a=p,i=%d,p=%d,c=%d,r=%d,C=1,z=%d,q=2", place.id, place.id, place.cols, place.rows, place.z);
        out += "\x1b_G";
        out += control;
        if(crop)
        {
            sprintf(rect, ",x=%d,y=%d,w=%d,h=%d", x, y, w, h);
            out += rect;
        }
        out += ";\x1b\\";
        return true;
    }

    std::string payload;

    if(img.format == "png" and not img.file.empty())
    {
        payload = g_image_base64(&img.file[0], img.file.size());
        sprintf(control, "a=T,f=100,t=d,q=2,C=1,i=%d,p=%d,c=%d,r=%d,z=%d", place.id, place.id, place.cols, place.rows, place.z);
        if(crop)
        {
            sprintf(rect, ",x=%d,y=%d,w=%d,h=%d", place.sx, place.sy, place.sw, place.sh);
            strcat(control, rect);
        }
    }
    else
    {
        if(not img.has_pixels())
            return false;

        //  The whole image goes, as raw RGB, no larger than the cells
        //  the whole picture takes - the terminal scales to the cells
        //  anyway, and a photograph sent whole as raw RGB is megabytes
        //  down a ssh link. Where the cell size is unknown the same
        //  guess as in g_imgterm_fit() bounds it. The part placed now
        //  is named in the scaled image's coordinates.
        int cw = (term.cell_width  > 0 ? term.cell_width  : 8)  * place.cols;
        int chh = (term.cell_height > 0 ? term.cell_height : 16) * (place.full_rows > 0 ? place.full_rows : place.rows);
        int W = img.width, H = img.height;
        double s = 1.0;
        if(W > cw or H > chh)
        {
            s = (double)cw / W;
            if((double)chh / H < s) s = (double)chh / H;
            W = (int)(W * s); H = (int)(H * s);
            if(W < 1) W = 1;
            if(H < 1) H = 1;
        }

        std::vector<unsigned char> rgb;
        g_image_scale_rgb(img, W, H, rgb);
        if(rgb.empty())
            return false;
        payload = g_image_base64(&rgb[0], rgb.size());
        sprintf(control, "a=T,f=24,t=d,q=2,C=1,i=%d,p=%d,s=%d,v=%d,c=%d,r=%d,z=%d", place.id, place.id, W, H, place.cols, place.rows, place.z);
        if(crop)
        {
            int x = (int)(place.sx * s), y = (int)(place.sy * s), w = (int)(place.sw * s), h = (int)(place.sh * s);
            if(w < 1) w = 1;
            if(h < 1) h = 1;
            sprintf(rect, ",x=%d,y=%d,w=%d,h=%d", x, y, w, h);
            strcat(control, rect);
        }
    }

    g_imgterm_goto(place, out);

    const size_t chunk = 4096;
    size_t pos = 0;
    bool firstchunk = true;
    do
    {
        size_t n = payload.length() - pos;
        if(n > chunk)
            n = chunk;
        bool more = (pos + n) < payload.length();

        out += "\x1b_G";
        if(firstchunk)
        {
            out += control;
            out += ',';
        }
        out += more ? "m=1" : "m=0";
        out += ';';
        out.append(payload, pos, n);
        out += "\x1b\\";

        pos += n;
        firstchunk = false;
    }
    while(pos < payload.length());

    return true;
}


//  ------------------------------------------------------------------
//  iTerm2. The file, whatever it is, base64 in one OSC; the terminal
//  decodes and scales it to the cells named. There is no cropping in
//  this protocol, so a part of an image is cut out here and written
//  as a small PNG of its own, no larger than the cells it fills.

static bool g_imgterm_iterm2(const GImgTerm& term, const GImage& img, const GImgPlace& place, std::string& out)
{
    const std::vector<unsigned char>* file = &img.file;
    std::vector<unsigned char> part;

    bool whole = (place.sw <= 0 or place.sh <= 0) or
                 (place.sx == 0 and place.sy == 0 and place.sw == img.width and place.sh == img.height);
    if(not whole)
    {
        int cw = (term.cell_width  > 0 ? term.cell_width  : 8)  * place.cols;
        int ch = (term.cell_height > 0 ? term.cell_height : 16) * place.rows;
        if(not g_image_encode(img, place.sx, place.sy, place.sw, place.sh, cw, ch, img.format == "jpeg", part))
            return false;
        file = &part;
    }

    if(file->empty())
        return false;

    g_imgterm_goto(place, out);

    char buf[160];
    sprintf(buf, "\x1b]1337;File=inline=1;size=%lu;width=%d;height=%d;preserveAspectRatio=1;doNotMoveCursor=1:",
            (unsigned long)file->size(), place.cols, place.rows);
    out += buf;
    out += g_image_base64(&(*file)[0], file->size());
    out += '\a';

    return true;
}


//  ------------------------------------------------------------------

bool g_imgterm_encode(const GImgTerm& term, const GImage& img, const GImgPlace& place, std::string& out)
{
    out.erase();

    if(place.cols <= 0 or place.rows <= 0)
        return false;

    switch(term.proto)
    {
    case GIMG_SIXEL:  return g_imgterm_sixel(term, img, place, out);
    case GIMG_KITTY:  return g_imgterm_kitty(term, img, place, out);
    case GIMG_ITERM2: return g_imgterm_iterm2(term, img, place, out);
    default:          return false;
    }
}


//  ------------------------------------------------------------------

void g_imgterm_erase(const GImgTerm& term, const GImgPlace& place, std::string& out, bool data)
{
    out.erase();

    if(term.proto == GIMG_KITTY)
    {
        char buf[64];
        sprintf(buf, "\x1b_Ga=d,d=%c,i=%d,q=2;\x1b\\", data ? 'I' : 'i', place.id);
        out += buf;
    }
}

#endif  // GOLD_IMAGES

//  ------------------------------------------------------------------
