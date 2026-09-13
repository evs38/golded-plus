//  This may look like C code, but it is really -*- C++ -*-

//  ------------------------------------------------------------------
//  GoldED+
//  Copyright (C) 2026 The GoldED+ team
//  ------------------------------------------------------------------
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License as
//  published by the Free Software Foundation; either version 2 of the
//  License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
//  02111-1307, USA
//  ------------------------------------------------------------------
//  Pictures carried in a message, shown on the terminal.
//
//  Two ways. Inline: when the reader binds a message to the body
//  view, the uuencoded and MIME base64 blocks in its line index that
//  decode to an image are taken out of the index and a caption line
//  plus as many empty rows as the picture needs are put in their
//  place; painting the body then draws, over those rows, the part of
//  each picture that is on the screen. The lines themselves stay in
//  the message - only the index the body view walks is changed - so
//  quoting, saving and decoding to disk see the message as it is.
//
//  The viewer: READviewimage hands the message's text to uulib, which
//  finds and decodes whatever is encoded in it - the same code the
//  READuudecode key uses to write attachments to disk - offers the
//  images found, and draws the chosen one over the whole body,
//  scaled to its width and height, until a key is pressed.
//
//  The drawing itself, and knowing what the terminal can draw, is in
//  goldlib/gall/gimgterm; this file is the reader's side of it. It is
//  compiled only where GOLD_IMAGES is defined - the curses builds on
//  unix - and nothing calls into it anywhere else.
//  ------------------------------------------------------------------

#include <golded.h>

#if defined(GOLD_IMAGES)

#include <gimgterm.h>
#include <uudeview.h>
#include <algorithm>
#include <time.h>


//  ------------------------------------------------------------------

struct ReadImage
{
    std::string name;
    GImage      image;
};


static void ReadImagesOverlay(int srow, int scol, int erow, int ecol, bool restore);
static bool inline_hooked;


//  ------------------------------------------------------------------
//  What the terminal speaks, asked once. The answer holds for the
//  session: the terminal does not change under a running program.

static GImgTerm* ReadImageTerm()
{
    static GImgTerm term;
    static bool probed = false;

    if(not probed)
    {
        probed = true;
        g_imgterm_probe(term, g_imgterm_parse(CFG->imageprotocol));
        if(not inline_hooked)
        {
            inline_hooked = true;
            gvid_set_overlay_hook(ReadImagesOverlay);
        }
        LOG.printf("- Images: IMAGEPROTOCOL \"%s\" -> %s, cell %dx%d", CFG->imageprotocol, g_imgterm_name(term.proto), term.cell_width, term.cell_height);
    }
    return &term;
}


//  ------------------------------------------------------------------
//  Decoding the two encodings a picture travels in, from the lines of
//  the index. uulib does this too, but from a file and with no word
//  about which lines it took; here the lines matter, since they are
//  the ones to be replaced.

//  One uuencoded line into bytes. False when it is not one.

static bool ReadImageUuLine(const char* p, std::vector<unsigned char>& out)
{
    int len = (*p - ' ') & 63;
    if(len == 0)
        return true;
    size_t need = (size_t)((len + 2) / 3) * 4;
    if(strlen(p) < need + 1)
        return false;

    const char* s = p + 1;
    for(int n = 0; n < len; n += 3, s += 4)
    {
        unsigned char c[4];
        for(int k = 0; k < 4; k++)
        {
            if(s[k] < ' ' or s[k] > '`')
                return false;
            c[k] = (unsigned char)((s[k] - ' ') & 63);
        }
        unsigned char b0 = (unsigned char)((c[0] << 2) | (c[1] >> 4));
        unsigned char b1 = (unsigned char)((c[1] << 4) | (c[2] >> 2));
        unsigned char b2 = (unsigned char)((c[2] << 6) | c[3]);
        out.push_back(b0);
        if(n + 1 < len) out.push_back(b1);
        if(n + 2 < len) out.push_back(b2);
    }
    return true;
}


//  A base64 line into bytes, appended. False on a character that is
//  not base64.

static bool ReadImageB64Line(const char* p, std::vector<unsigned char>& out, unsigned& acc, int& bits)
{
    for(; *p; p++)
    {
        int v;
        if(*p >= 'A' and *p <= 'Z') v = *p - 'A';
        else if(*p >= 'a' and *p <= 'z') v = *p - 'a' + 26;
        else if(*p >= '0' and *p <= '9') v = *p - '0' + 52;
        else if(*p == '+') v = 62;
        else if(*p == '/') v = 63;
        else if(*p == '=' or *p == ' ' or *p == '\r' or *p == '\n') continue;
        else return false;
        acc = (acc << 6) | (unsigned)v;
        bits += 6;
        if(bits >= 8)
        {
            bits -= 8;
            out.push_back((unsigned char)((acc >> bits) & 0xFF));
        }
    }
    return true;
}


//  The value of a name=/filename= parameter in a MIME header line, or
//  nothing.

static std::string ReadImageMimeName(const char* line)
{
    const char* p = striinc("filename=", line);
    if(p == NULL)
        p = striinc("name=", line);
    if(p == NULL)
        return std::string();
    p = strchr(p, '=') + 1;
    std::string name;
    if(*p == '"')
    {
        const char* e = strchr(p + 1, '"');
        name.assign(p + 1, e ? (size_t)(e - p - 1) : strlen(p + 1));
    }
    else
    {
        const char* e = p;
        while(*e and *e != ';' and not isspace(*e))
            e++;
        name.assign(p, (size_t)(e - p));
    }
    return name;
}


//  ------------------------------------------------------------------
//  A block of lines in the index that decodes to a picture.

struct ReadInlineBlock
{
    int first;          //  index of its first line in msg->line
    int last;           //  and its last, inclusive
    std::string name;
    std::vector<unsigned char> bytes;
};


//  Find every block, uuencoded or MIME base64, in the index.

//  A line of the message as it was written: the pieces the display
//  index wrapped it into at the margin, joined again. Returns the
//  index of the last piece. Spaces at the break were dropped when the
//  line was wrapped; where the line is known to be 'want' characters
//  long - a uuencoded line says so in its count character - the run
//  is put back where the pieces meet. A line cut in the middle of a
//  word lost nothing.

static int ReadImageJoinLine(const GMsg* msg, int i, size_t want, std::string& out)
{
    out = msg->line[i]->txt;
    while((msg->line[i]->type & GLINE_WRAP) and i + 1 < msg->lines)
    {
        bool cut = make_bool(msg->line[i]->type & GLINE_CUTW);
        i++;
        const std::string& more = msg->line[i]->txt;
        if(want and not cut and out.size() + more.size() < want)
            out.append(want - out.size() - more.size(), ' ');
        out += more;
    }
    return i;
}


static void ReadImageFindBlocks(GMsg* msg, std::vector<ReadInlineBlock>& blocks)
{
    for(int i = 0; i < msg->lines; i++)
    {
        const char* txt = msg->line[i]->txt.c_str();

        //  uuencode: "begin 644 name" ... lines ... "end".
        if(strneql(txt, "begin ", 6) and isdigit(txt[6]))
        {
            const char* p = txt + 6;
            while(isdigit(*p)) p++;
            while(*p == ' ') p++;
            if(*p == NUL)
                continue;

            ReadInlineBlock b;
            b.first = i;
            b.name = p;
            strtrim(b.name);

            int j = i + 1;
            bool ok = false;
            for(; j < msg->lines; j++)
            {
                const char* d = msg->line[j]->txt.c_str();
                if(streql(d, "end") or strneql(d, "end ", 4))
                {
                    ok = true;
                    break;
                }
                if(*d == NUL or *d < ' ' or *d > '`')
                    break;
                //  The count character gives the line's length; a line
                //  wrapped on a narrow screen is joined back to it.
                size_t want = (size_t)(((((*d - ' ') & 63) + 2) / 3) * 4) + 1;
                std::string whole;
                j = ReadImageJoinLine(msg, j, want, whole);
                if(not ReadImageUuLine(whole.c_str(), b.bytes))
                    break;
            }
            if(ok and not b.bytes.empty())
            {
                b.last = j;
                blocks.push_back(b);
                i = j;
            }
            continue;
        }

        //  MIME: a part's headers, a blank line, the base64 lines, up
        //  to a blank line or the next boundary. The block taken out
        //  runs from the first of the part's headers.
        if(strnieql(txt, "Content-Transfer-Encoding:", 26) and striinc("base64", txt))
        {
            ReadInlineBlock b;
            int h = i;
            while(h > 0 and (strnieql(msg->line[h-1]->txt.c_str(), "Content-", 8) or isspace(*msg->line[h-1]->txt.c_str())))
                h--;
            b.first = h;
            for(int k = h; k <= i + 8 and k < msg->lines; k++)
            {
                std::string nm = ReadImageMimeName(msg->line[k]->txt.c_str());
                if(not nm.empty())
                {
                    b.name = nm;
                    break;
                }
            }

            int j = i + 1;
            while(j < msg->lines and not strblank(msg->line[j]->txt.c_str()))
                j++;
            unsigned acc = 0;
            int bits = 0;
            int k = j + 1;
            for(; k < msg->lines; k++)
            {
                const char* d = msg->line[k]->txt.c_str();
                if(strblank(d) or strneql(d, "--", 2))
                    break;
                std::string whole;
                k = ReadImageJoinLine(msg, k, 0, whole);
                if(not ReadImageB64Line(whole.c_str(), b.bytes, acc, bits))
                    break;
            }
            if(not b.bytes.empty())
            {
                b.last = k - 1;
                if(b.name.empty())
                    b.name = "attachment";
                blocks.push_back(b);
                i = k - 1;
            }
        }
    }
}


//  ------------------------------------------------------------------
//  The pictures of the message the body view holds, with their rows.

struct ReadInlineImage
{
    ReadImage image;
    int  first;         //  index in msg->line of the first picture row
    int  rows;          //  rows reserved
    int  cols;          //  columns used
    int  id;            //  placement id, 1 and up
    bool decoded;       //  pixels ready
    bool undecodable;   //  and never will be
    bool transmitted;   //  kitty holds the image under its id already
};

//  A picture as it stands on the screen: where, what part, and how
//  many windows lie over it at the moment.
struct ReadDrawnImage
{
    GImgPlace place;
    size_t    image;    //  index into inline_images
    int       covered;
};

static std::vector<ReadInlineImage> inline_images;
static std::vector<Line*>           inline_lines;    //  the rows we own
static std::vector<Line*>           inline_view;     //  the body view's index, NULL-ended
static const GMsg*                  inline_msg = NULL;
static uint32_t                     inline_msgno = 0;
static int                          inline_cols = 0;
static Line**                       inline_src = NULL;   //  the message's index the view was made from,
static int                          inline_srclines = 0; //  to tell when it has been rebuilt
static Line*                        inline_src0 = NULL;
static std::vector<ReadDrawnImage>  inline_drawn;    //  pictures on the screen


//  Whether the view made for this message still stands: the message
//  and its own index are the ones it was made from - a reload or a
//  toggle rebuilds that index, and then the pointers held here are
//  stale and the view has to be made again.

static bool ReadImagesValid(const GMsg* msg)
{
    if(msg == NULL or msg != inline_msg or msg->msgno != inline_msgno or inline_images.empty())
        return false;
    if(msg->line != inline_src or msg->lines != inline_srclines)
        return false;
    if(msg->lines > 0 and msg->line[0] != inline_src0)
        return false;
    return true;
}


//  The bytes that take one drawn picture off the screen: the cells
//  erased where the picture lives in them, the placement deleted
//  where the terminal keeps it apart.

static void ReadImagesEraseOne(const ReadDrawnImage& d, std::string& out)
{
    GImgTerm* term = ReadImageTerm();
    if(term->proto == GIMG_KITTY)
    {
        std::string one;
        g_imgterm_erase(*term, d.place, one);
        out += one;
    }
    else
    {
        char buf[48];
        for(int r = 0; r < d.place.rows; r++)
        {
            sprintf(buf, "\x1b[%d;%dH\x1b[%dX", d.place.row + r + 1, d.place.col + 1, d.place.cols);
            out += buf;
        }
    }
}


//  The bytes that put a drawn picture back, after the window that
//  covered it is gone.

static void ReadImagesDrawOne(ReadDrawnImage& d, std::string& out)
{
    GImgTerm* term = ReadImageTerm();
    if(d.image >= inline_images.size())
        return;
    ReadInlineImage& im = inline_images[d.image];
    GImgPlace place = d.place;
    place.transmit = not im.transmitted;
    std::string seq;
    if(g_imgterm_encode(*term, im.image.image, place, seq))
    {
        out += seq;
        if(term->proto == GIMG_KITTY)
            im.transmitted = true;
    }
}


//  Every picture off the screen, and forgotten as drawn.

static void ReadImagesEraseDrawn()
{
    if(inline_drawn.empty())
        return;

    //  Ahead of whatever curses has pending: the text about to land
    //  in these cells - the caption, say, after a scroll - must come
    //  after the erasure, or the erasure takes it away.
    std::string out;
    for(size_t n = 0; n < inline_drawn.size(); n++)
        if(inline_drawn[n].covered == 0)
            ReadImagesEraseOne(inline_drawn[n], out);
    inline_drawn.clear();
    if(not out.empty())
        vputraw_now(out.c_str(), out.length());
}


//  A window is about to cover a rectangle of the screen, or has just
//  been taken away from it. A picture under it goes away with the
//  cells the window saves - the cells come back, the picture would
//  not - and is drawn again once nothing covers it any more.

static void ReadImagesOverlay(int srow, int scol, int erow, int ecol, bool restore)
{
    if(inline_drawn.empty())
        return;

    std::string out;
    for(size_t n = 0; n < inline_drawn.size(); n++)
    {
        ReadDrawnImage& d = inline_drawn[n];
        bool hits = not (d.place.row > erow or d.place.row + d.place.rows - 1 < srow or
                         d.place.col > ecol or d.place.col + d.place.cols - 1 < scol);
        if(not hits)
            continue;
        if(not restore)
        {
            if(d.covered == 0)
                ReadImagesEraseOne(d, out);
            d.covered++;
        }
        else if(d.covered > 0)
        {
            d.covered--;
            if(d.covered == 0)
                ReadImagesDrawOne(d, out);
        }
    }
    if(not out.empty())
        vputraw(out.c_str(), out.length());
}


static void ReadImagesForget()
{
    ReadImagesEraseDrawn();

    //  What kitty holds under our ids goes too: the next message
    //  starts its ids from 1 again.
    {
        std::string out, one;
        GImgTerm* term = inline_images.empty() ? NULL : ReadImageTerm();
        for(size_t n = 0; term and n < inline_images.size(); n++)
        {
            if(not inline_images[n].transmitted)
                continue;
            GImgPlace place;
            place.id = inline_images[n].id;
            g_imgterm_erase(*term, place, one, true);
            out += one;
        }
        if(not out.empty())
            vputraw(out.c_str(), out.length());
    }

    inline_images.clear();
    for(size_t n = 0; n < inline_lines.size(); n++)
        delete inline_lines[n];
    inline_lines.clear();
    inline_view.clear();
    inline_msg = NULL;
    inline_msgno = 0;
    inline_cols = 0;
    inline_src = NULL;
    inline_srclines = 0;
    inline_src0 = NULL;
}


//  ------------------------------------------------------------------

bool ReadImagesInline(const GMsg* msg)
{
    return ReadImagesValid(msg);
}


//  The screen changed size: the terminal laid its cells out afresh
//  and the pictures went with them to places we no longer know, so
//  the coordinates held here are worthless. Clear the whole screen
//  through the terminal - erasing the cells is what takes a Sixel or
//  an iTerm2 picture away - delete every kitty placement, and start
//  from nothing; GoldED paints the screen again right after this.

void ReadImagesScreenReset()
{
    if(inline_drawn.empty() and inline_images.empty())
        return;

    GImgTerm* term = ReadImageTerm();
    std::string out;
    if(term->proto == GIMG_KITTY)
        out += "\x1b_Ga=d,d=a,q=2;\x1b\\";
    else if(term->proto != GIMG_NONE)
        out += "\x1b[2J";
    inline_drawn.clear();
    if(not out.empty())
        vputraw_now(out.c_str(), out.length());
    ReadImagesForget();
}


//  Before the body is painted: the pictures on the screen are taken
//  off ahead of the text - but only from the rows that will not hold
//  the same picture after the paint. A picture scrolled by a row is
//  then redrawn over itself, which the terminal does without a blink,
//  and only the row it left behind is erased. A picture that will be
//  gone is taken off entirely, kitty's placement included.

void ReadImagesErase(const GMsg* msg, int upperline, int lowerline, int at_row)
{
    if(inline_drawn.empty())
        return;

    if(not ReadImagesValid(msg))
    {
        ReadImagesEraseDrawn();
        return;
    }

    GImgTerm* term = ReadImageTerm();
    std::string out;
    for(size_t n = 0; n < inline_drawn.size(); n++)
    {
        const ReadDrawnImage& d = inline_drawn[n];
        if(d.covered or d.image >= inline_images.size())
            continue;
        const ReadInlineImage& im = inline_images[d.image];

        //  The screen rows the same picture will take after the paint.
        int r0 = im.first > upperline ? im.first : upperline;
        int r1 = im.first + im.rows - 1 < lowerline ? im.first + im.rows - 1 : lowerline;
        bool stays = r0 <= r1;
        int s0 = stays ? at_row + (r0 - upperline) : -1;
        int s1 = stays ? at_row + (r1 - upperline) : -1;

        if(term->proto == GIMG_KITTY)
        {
            if(not stays)
                ReadImagesEraseOne(d, out);
            continue;
        }

        char buf[48];
        for(int r = d.place.row; r < d.place.row + d.place.rows; r++)
        {
            if(stays and r >= s0 and r <= s1)
                continue;
            sprintf(buf, "\x1b[%d;%dH\x1b[%dX", r + 1, d.place.col + 1, d.place.cols);
            out += buf;
        }
    }
    inline_drawn.clear();
    if(not out.empty())
        vputraw_now(out.c_str(), out.length());
}


//  ------------------------------------------------------------------

void ReadImagesIndex(GMsg* msg, int cols, Line*** vline, int* vlines)
{
    if(msg == NULL or msg->line == NULL)
    {
        ReadImagesForget();
        *vline  = msg ? msg->line : NULL;
        *vlines = msg ? msg->lines : 0;
        return;
    }

    if(ReadImagesValid(msg) and cols == inline_cols)
    {
        *vline  = &inline_view[0];
        *vlines = (int)inline_view.size() - 1;
        return;
    }

    ReadImagesForget();
    *vline  = msg->line;
    *vlines = msg->lines;

    if(CFG->dispimages != YES or cols < 2)
        return;

    GImgTerm* term = ReadImageTerm();
    if(term->proto == GIMG_NONE)
        return;

    std::vector<ReadInlineBlock> blocks;
    ReadImageFindBlocks(msg, blocks);
    if(blocks.empty())
        return;

    //  Which of them are pictures, and how many rows each takes.
    std::vector<ReadInlineImage> images;
    std::vector<size_t> which;
    for(size_t n = 0; n < blocks.size(); n++)
    {
        ReadInlineImage im;
        if(not g_image_load(&blocks[n].bytes[0], blocks[n].bytes.size(), im.image.image, false))
            continue;
        im.image.name = blocks[n].name;
        g_imgterm_fit(*term, im.image.image.width, im.image.image.height, cols, 1000, &im.cols, &im.rows);
        if(im.cols < 1 or im.rows < 1)
            continue;
        im.first = 0;
        im.id = (int)images.size() + 1;
        im.decoded = im.undecodable = im.transmitted = false;
        images.push_back(im);
        which.push_back(n);
    }
    if(images.empty())
        return;

    //  The view's index: the message's with each block replaced by a
    //  caption and the picture's rows. The message's own index is
    //  not touched - the reply and change templates, the writing to
    //  disk and the decoding to disk all read it and want the lines.
    std::vector<Line*> index;
    size_t b = 0;
    for(int i = 0; i < msg->lines; i++)
    {
        if(b < which.size() and i == blocks[which[b]].first)
        {
            ReadInlineImage& im = images[b];
            char buf[256];
            gsprintf(PRINTF_DECLARE_BUFFER(buf), "[%s, %dx%d %s, %lu bytes]",
                     im.image.name.c_str(), im.image.image.width, im.image.image.height,
                     im.image.image.format.c_str(), (unsigned long)im.image.image.file.size());
            Line* cap = new Line(buf);
            throw_xnew(cap);
            cap->type = GLINE_HARD;
            cap->color = C_READW;
            inline_lines.push_back(cap);
            index.push_back(cap);

            im.first = (int)index.size();
            for(int r = 0; r < im.rows; r++)
            {
                Line* row = new Line();
                throw_xnew(row);
                row->type = GLINE_HARD | GLINE_IMGROW;
                row->color = C_READW;
                inline_lines.push_back(row);
                index.push_back(row);
            }
            i = blocks[which[b]].last;
            b++;
            continue;
        }
        index.push_back(msg->line[i]);
    }
    index.push_back(NULL);

    inline_view = index;
    inline_images = images;
    inline_msg = msg;
    inline_msgno = msg->msgno;
    inline_cols = cols;
    inline_src = msg->line;
    inline_srclines = msg->lines;
    inline_src0 = msg->lines > 0 ? msg->line[0] : NULL;

    *vline  = &inline_view[0];
    *vlines = (int)inline_view.size() - 1;
}


//  ------------------------------------------------------------------

void ReadImagesPaint(const GMsg* msg, int upperline, int lowerline, int at_row, int at_col)
{
    if(not ReadImagesValid(msg))
        return;

    GImgTerm* term = ReadImageTerm();
    ReadImagesEraseDrawn();

    clock_t t0 = clock();
    std::string out;
    for(size_t n = 0; n < inline_images.size(); n++)
    {
        ReadInlineImage& im = inline_images[n];
        int r0 = im.first > upperline ? im.first : upperline;
        int r1 = im.first + im.rows - 1 < lowerline ? im.first + im.rows - 1 : lowerline;
        if(r0 > r1)
            continue;

        GImage& img = im.image.image;
        bool need_pixels = (term->proto == GIMG_SIXEL) or
                           (term->proto == GIMG_KITTY and img.format != "png") or
                           (term->proto == GIMG_ITERM2 and (r0 > im.first or r1 < im.first + im.rows - 1));
        if(need_pixels and not im.decoded)
        {
            if(im.undecodable)
                continue;
            if(not g_image_decode(img))
            {
                im.undecodable = true;
                continue;
            }
            im.decoded = true;
        }

        GImgPlace place;
        place.row  = at_row + (r0 - upperline);
        place.col  = at_col;
        place.cols = im.cols;
        place.rows = r1 - r0 + 1;
        place.full_rows = im.rows;
        place.id   = im.id;
        place.z    = -1;
        place.transmit = not im.transmitted;
        place.sx   = 0;
        place.sw   = img.width;
        place.sy   = (int)((long)img.height * (r0 - im.first) / im.rows);
        place.sh   = (int)((long)img.height * (r1 - r0 + 1) / im.rows);
        if(place.sh < 1)
            place.sh = 1;
        if(place.sy + place.sh > img.height)
            place.sh = img.height - place.sy;

        std::string seq;
        if(g_imgterm_encode(*term, img, place, seq))
        {
            out += seq;
            if(term->proto == GIMG_KITTY)
                im.transmitted = true;
            ReadDrawnImage d;
            d.place = place;
            d.image = n;
            d.covered = 0;
            inline_drawn.push_back(d);
        }
    }

    clock_t t1 = clock();
    if(not out.empty())
        vputraw(out.c_str(), out.length());
    if(not out.empty())
        LOG.printf("- Images: painted %lu bytes, encode %ld ms, write %ld ms",
                   (unsigned long)out.length(), (long)((t1 - t0) * 1000 / CLOCKS_PER_SEC), (long)((clock() - t1) * 1000 / CLOCKS_PER_SEC));
}


//  ------------------------------------------------------------------
//  The viewer.
//  ------------------------------------------------------------------

//  Every image uulib can dig out of the message. The text goes to a
//  file first, as READuudecode does it, because that is what uulib
//  reads; each decoded part lands in a temporary file of uulib's own,
//  which is read back and let go of with the rest of uulib's state.

static void ReadImageCollect(GMsg* msg, std::vector<ReadImage>& found)
{
    Path infile;
    mktemp(strcpy(infile, AddPath(CFG->temppath, "GDXXXXXX")));

    bool old_quotespacing = CFG->switches.get(quotespacing);
    CFG->switches.set(quotespacing, false);
    AA->LoadMsg(msg, msg->msgno, 79);
    SaveLines(MODE_WRITE, infile, msg, 79);
    CFG->switches.set(quotespacing, old_quotespacing);

    UUInitialize();
    if(UULoadFile(infile, NULL, 0) == UURET_OK)
    {
        uulist* item;
        for(int i = 0; (item = UUGetFileListItem(i)) != NULL; i++)
        {
            if((item->state & UUFILE_OK) == 0)
                continue;
            if(UUDecodeToTemp(item) != UURET_OK or item->binfile == NULL)
                continue;

            gfile fp(item->binfile, "rb");
            if(not fp.isopen())
                continue;

            long len = fp.FileLength();
            if(len <= 0)
                continue;

            std::vector<unsigned char> bytes((size_t)len);
            if(fp.Fread(&bytes[0], 1, (size_t)len) != (size_t)len)
                continue;

            ReadImage ri;
            if(not g_image_load(&bytes[0], bytes.size(), ri.image, false))
                continue;
            ri.name = item->filename ? item->filename : "";
            if(ri.name.empty())
                ri.name = "image";
            found.push_back(ri);
        }
    }
    UUCleanUp();

    remove(infile);
}


//  ------------------------------------------------------------------
//  Draw one over the body of the message and wait for a key.

static bool ReadImageShow(const ReadImage& ri)
{
    GImgTerm* term = ReadImageTerm();
    if(term->proto == GIMG_NONE)
        return false;

    //  Sixel wants pixels; kitty wants them for anything but a PNG.
    GImage img = ri.image;
    if(term->proto == GIMG_SIXEL or (term->proto == GIMG_KITTY and img.format != "png"))
    {
        if(not g_image_decode(img))
            return false;
    }

    int cols = BodyView->VisibleWidth();
    int rows = BodyView->height;

    GImgPlace place;
    place.row = BodyView->at_row;
    place.col = BodyView->at_column;
    place.id  = 100;
    g_imgterm_fit(*term, img.width, img.height, cols, rows, &place.cols, &place.rows);

    std::string seq;
    if(not g_imgterm_encode(*term, img, place, seq))
        return false;

    //  Inline pictures out of the way, clean cells under the picture,
    //  then the picture, then a key.
    ReadImagesEraseDrawn();
    for(int r = 0; r < rows; r++)
        vputx(place.row + r, place.col, C_READW, ' ', (uint)cols);
    update_statusline(LNG->ViewImageStat);
    vcurhide();
    vputraw(seq.c_str(), seq.length());

    kbxget();

    std::string erase;
    g_imgterm_erase(*term, place, erase, true);
    if(term->proto != GIMG_KITTY)
    {
        char buf[48];
        for(int r = 0; r < place.rows; r++)
        {
            sprintf(buf, "\x1b[%d;%dH\x1b[%dX", place.row + r + 1, place.col + 1, place.cols);
            erase += buf;
        }
    }
    if(not erase.empty())
        vputraw(erase.c_str(), erase.length());

    //  The cells the terminal drew over are repainted from scratch;
    //  curses alone would leave those it believes unchanged.
    vredraw();

    return true;
}


//  ------------------------------------------------------------------

void ReadViewImages(GMsg* msg)
{
    if(not CFG->dispimages)
        return;

    GImgTerm* term = ReadImageTerm();
    if(term->proto == GIMG_NONE)
    {
        w_info(LNG->NoImageSupport);
        waitkeyt(10000);
        w_info(NULL);
        return;
    }

    w_info(LNG->Wait);
    std::vector<ReadImage> found;
    ReadImageCollect(msg, found);
    w_info(NULL);

    if(found.empty())
    {
        w_info(LNG->NoImages);
        waitkeyt(10000);
        w_info(NULL);
        return;
    }

    size_t chosen = 0;
    if(found.size() > 1)
    {
        gstrarray Listi;
        for(size_t n = 0; n < found.size(); n++)
        {
            char buf[256];
            gsprintf(PRINTF_DECLARE_BUFFER(buf), " %-32.32s %5dx%-5d %-4s %7lu ",
                     found[n].name.c_str(), found[n].image.width, found[n].image.height,
                     found[n].image.format.c_str(), (unsigned long)found[n].image.file.size());
            Listi.push_back(buf);
        }

        size_t n = MinV(found.size(), (size_t)(MAXROW-10));
        set_title(LNG->ViewImageMenuTitle, TCENTER, C_ASKT);
        update_statusline(LNG->ViewImageStat);
        int pick = wpickstr(6, 0, 6 + (int)n + 1, -1, W_BASK, C_ASKB, C_ASKW, C_ASKS, Listi, 0, title_shadow);
        if(pick == -1)
            return;
        chosen = (size_t)pick;
    }

    ReadImageShow(found[chosen]);
}

#endif  // GOLD_IMAGES

//  ------------------------------------------------------------------
