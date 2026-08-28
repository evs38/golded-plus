//  This may look like C code, but it is really -*- C++ -*-

//  ------------------------------------------------------------------
//  The Goldware Library
//  Copyright (C) 1990-1999 Odinn Sorensen
//  Copyright (C) 1999-2000 Alexander S. Aganichev
//  Copyright (C) 2000 Jacobo Tarrio
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
//  $Id$
//  ------------------------------------------------------------------
//  GCUI: Golded+ Character-oriented User Interface.
//  Device-independent video functions.
//  ------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
//  Borland C++ 5.2 has no <wchar.h> to forward to. Nothing here needs it
//  unless curses is driving the screen with wide characters.
#if !defined(__BORLANDC__)
#include <cwchar>
#endif
#include <gmemall.h>
#include <gmemdbg.h>
#include <gstrall.h>
#include <gutf8.h>
#include <grecode.h>
#include <gvidall.h>

#if defined(__OS2__)
    #define INCL_BASE
    #include <os2.h>
    #ifndef __EMX__
        #define PCCH CHAR*
    #endif
    //  What the Vio calls want for a cell buffer. Open Watcom's OS/2
    //  headers are specific about it and disagree between the two: the
    //  reader takes a char*, the writers a __far16 unsigned char*, and a
    //  plain BYTE* converts to neither.
    #if defined(__WATCOMC__)
        #define GVIO_CELL_R (PCH)
        #define GVIO_CELL_W (PBYTE)
    #else
        #define GVIO_CELL_R (BYTE *)
        #define GVIO_CELL_W (BYTE *)
    #endif
#endif

#ifdef __WIN32__
    #include <windows.h>
#endif

#if defined(__GNUC__) || (defined(__WATCOMC__) && defined(__LINUX__))
    #include <unistd.h>
#endif

#if defined(__WATCOMC__) && !defined(__LINUX__)
    //  outp()/outpw() - the I/O port helpers used by the text-mode
    //  cursor code below - live here in Open Watcom.
    #include <conio.h>
#endif

#if defined(__DJGPP__)
    #include <sys/farptr.h>
#endif


//  ------------------------------------------------------------------
//  refresh(), unless we have already given the terminal back
//
//  ncurses reads any drawing after endwin() as a request to come back,
//  and re-enters the alternate screen. Once vshutdown() has handed the
//  terminal over - because there is a message the user has to read -
//  nothing may draw again, or the message ends up hidden behind a screen
//  nobody asked for and the terminal is left sitting in it.

#if defined(__USE_NCURSES__)
static inline int gvid_refresh()
{
    return vscreendown() ? OK : refresh();
}
#endif


//  ------------------------------------------------------------------
//  Check if Borland C++ for OS/2 1.0 header has been fixed

#if defined(__OS2__) && defined(__BORLANDC__)
    #if __BORLANDC__ <= 0x400
        #ifndef BCOS2_BSESUB_FIXED
            #error There is a bug in the BSESUB.H header. Please fix it.
            //
            // Add/change the following in BSESUB.H:
            //
            // #define BCOS2_BSESUB_FIXED
            // APIRET16  APIENTRY16    VioGetState (PVOID16 pState, HVIO hvio);
            // APIRET16  APIENTRY16    VioSetState (PVOID16 pState, HVIO hvio);
            //
            // Borland forgot this (was only PVOID)      ^^
            //
        #endif
    #endif
#endif


//  ------------------------------------------------------------------

static bool __vcurhidden = false;
//  Box-drawing conversion is wanted on every screen, curses or not.
    vchar gvid_boxcvtc(vchar);


#if defined(__USE_WIDE_NCURSES__)
//  Turn what a screen cell holds into the wide character curses wants.
//
//  In UTF-8 mode the cell already holds a codepoint and the terminal is
//  UTF-8, so it goes straight through.
//
//  In 8-bit mode the cell holds a byte in the local charset, and what
//  that byte has to become depends on the locale, because wchar_t is
//  only Unicode when the locale says so:
//
//    * A UTF-8 locale - a CP866 or KOI8-R message base shown on a UTF-8
//      terminal. The byte means nothing to the C library on its own, so
//      translate it through the charset the text is in, or the message
//      comes out as mojibake.
//
//    * A single-byte locale - a KOI8-R or CP866 session, which is what
//      people running those charsets actually have. Here the library has
//      its own wide representation, which on the BSDs is not Unicode at
//      all: handing it a codepoint makes wcrtomb() fail and curses draws
//      a blank, so every Cyrillic letter vanished from the screen. Ask
//      the library what the byte means to it instead. That value encodes
//      back to the same byte, so it reaches the terminal untouched, the
//      way it did before any of this was wide.

static inline uint32_t g_local_to_unicode_wide(vchar chr)
{
    if(g_utf8_mode() or chr < 0x80 or chr > 0xFF)
        return (uint32_t)chr;

    wint_t wc = btowc((int)(unsigned char)chr);
    if(wc != WEOF)
        return (uint32_t)wc;

    return g_local_to_unicode((unsigned char)chr);
}
#endif

#if !defined(__USE_NCURSES__)

//  ------------------------------------------------------------------

#ifdef __WIN32__
extern HANDLE gvid_hout;
extern OSVERSIONINFO WinVer; // defined in gutlwin.cpp
extern WCHAR oem2unicode[]; // defined in gutlwin.cpp

//  ------------------------------------------------------------------
//  Turn what the program holds into the Unicode a console cell wants.
//
//  In UTF-8 mode the value already is a codepoint and passes straight
//  through. In 8-bit mode it is a byte in the local charset, and
//  oem2unicode[] - built by the system from the console's own codepage -
//  says what it means. That table also renders the characters below 32
//  as the glyphs CP437 assigns them, which is what GoldED means by them.

vchar gvid_tcpr(vchar chr)
{
    //  Already a codepoint - the function may meet its own output,
    //  since vputs() converts before building cells and vcatch()
    //  converts again. Feeding U+0440 back through the OEM table
    //  turned every Cyrillic letter into the glyph of its low byte.
    if(chr > 0xFF)
        return chr;

    if(g_utf8_mode() and chr >= 0x80)
    {
        //  A lone byte in that range cannot occur in well-formed UTF-8
        //  text, so it is a codepoint that happens to be small.
        return chr;
    }

    return oem2unicode[chr & 0xff];
}

#endif


//  ------------------------------------------------------------------

#if defined(__MSDOS__) || defined(__UNIX__)

#if defined(__MSDOS__)
    extern int __gdvdetected;
#endif

#ifndef __DJGPP__
const uint16_t _dos_ds = 0;

inline uint16_t _my_ds(void)
{

    return 0;
}

inline void _farpokew(uint16_t s, gdma ptr, word chat)
{

    NW(s);
    *ptr = chat;
}

inline void _farnspokew(gdma ptr, word chat)
{

    *ptr = chat;
}

inline word _farpeekw(uint16_t s, gdma ptr)
{

    NW(s);
    return *ptr;
}

inline void _farnspokeb(byte *ptr, byte chr)
{

    *ptr = chr;
}

inline void _farsetsel(uint16_t s)
{

    NW(s);
}
#endif

#ifdef __DJGPP__
    const int ATTRSIZE = sizeof(word);
#else
    const int ATTRSIZE = 1;
#endif

inline void gdmacpy(uint16_t seg_d, gdma sel_d, uint16_t seg_s, gdma sel_s, int len)
{

#ifdef __DJGPP__
    movedata(seg_s, sel_s, seg_d, sel_d, len);
#else
    NW(seg_d);
    NW(seg_s);
    memcpy(sel_d, sel_s, len);
#endif
}

inline gdma gdmaptr(int col, int row)
{

    return gvid->dmaptr+ATTRSIZE*((row*gvid->numcols)+col);
}
#endif


//  ------------------------------------------------------------------

#if defined(__UNIX__)


//  ------------------------------------------------------------------

extern int gvid_stdout;
extern const char* gvid_acs_enable;
extern const char* gvid_acs_disable;
int gvid_last_attr = 0;


//  ------------------------------------------------------------------

void gvid_printf(const char* fmt, ...)
{

    char buf[1024];
    va_list argptr;
    va_start(argptr, fmt);
    int n = vsprintf(buf, fmt, argptr);
    va_end(argptr);

    write(gvid_stdout, buf, n);
}


//  ------------------------------------------------------------------
//  Control chars      01234567890123456789012345678901

const char* gvid_x0 = "x@xxxxxxxxxxxxxx><xxxxxx^vxxxx^v";


//  ------------------------------------------------------------------

inline void gvid_cvtchr(char& ch)
{

    const char* x0 = gvid_x0;

    if(ch < ' ')
        ch = x0[ch];
}


//  ------------------------------------------------------------------

void gvid_cvtstr(char* s, int len)
{

    char* p = s;

    for(int n=0; n<len; n++,p++)
        gvid_cvtchr(*p);
}


//  ------------------------------------------------------------------

void gvid_cvtstr(word* ws, int len)
{

    word* wp = ws;

    for(int n=0; n<len; n++,wp++)
        gvid_cvtchr(*(char*)wp);
}


//  ------------------------------------------------------------------

static int _atr_to_ans[8] =
{
    0,  // BLACK    0
    4,  // BLUE     1
    2,  // GREEN    2
    6,  // CYAN     3
    1,  // RED      4
    5,  // MAGENTA  5
    3,  // BROWN    6
    7   // WHITE    7
};


//  ------------------------------------------------------------------

inline int vatr2ansin(int x)
{

    return (x & 8) ? 1 : 0;
}


//  ------------------------------------------------------------------

inline int vatr2ansfg(int x)
{

    return _atr_to_ans[x & 7];
}


//  ------------------------------------------------------------------

inline int vatr2ansbg(int x)
{

    return _atr_to_ans[(x>>4) & 7];
}


//  ------------------------------------------------------------------

void vputansi(int row, int col, word* buf, int len)
{

    char ch;
    int in, fg, bg, acs;
    int atr = gvid_last_attr;
    int in0 = vatr2ansin(atr);
    int fg0 = vatr2ansfg(atr);
    int bg0 = vatr2ansbg(atr);
    int acs0 = atr & ACSET;

    // Get pointer to ANSI line buffer
    char* ptr = gvid->bufansi;

    // Get pointer to video memory image
    byte* p = (byte*)buf;

    for(int n=0; n<len; n++,p+=2)      // For each screen element
    {

        if(p[1] != atr)                   // If attribute is different
        {

            atr = p[1];                     // Store new attribute
            gvid_last_attr = atr;

            in = vatr2ansin(atr);           // Get intensity
            fg = vatr2ansfg(atr);           // Get foreground color
            bg = vatr2ansbg(atr);           // Get background color
            acs = atr & ACSET;              // Get Alt Color Set

            if(acs != acs0)
            {
                ptr = stpcpy(ptr, acs ? gvid_acs_enable : gvid_acs_disable);
                acs0 = acs;
            }

            *ptr++ = 0x1B;                  // Start ANSI color sequence
            *ptr++ = '[';

            if(in != in0)                   // Set intensity if different
            {
                if(in)
                    *ptr++ = '1';               // Intense
                else
                {
                    *ptr++ = '0';               // Reset
                    fg0 = bg0 = -1;
                }
                in0 = in;
                if((fg != fg0) or (bg != bg0))
                    *ptr++ = ';';
            }

            if(fg != fg0)                   // Set foreground if different
            {
                *ptr++ = '3';
                *ptr++ = (char)('0' + fg);
                fg0 = fg;
                if(bg != bg0)
                    *ptr++ = ';';
            }

            if(bg != bg0)                   // Set background if different
            {
                *ptr++ = '4';
                *ptr++ = (char)('0' + bg);
                bg0 = bg;
            }

            *ptr++ = 'm';                   // End ANSI color sequence
        }

        ch = p[0];
        gvid_cvtchr(ch);
        *ptr++ = ch;                    // Output the character
    }

    *ptr = NUL;                         // Terminate string

    // Print complete ANSI string at the specified position
    gvid_printf("\x1B[%u;%uH%s", row+1, col+1, gvid->bufansi);
}


//  ------------------------------------------------------------------

#endif

#endif // !defined(__USE_NCURSES__)


//  ------------------------------------------------------------------
//  Converts an attribute to monochrome equivalent

vattr mapattr(vattr attr)
{
    switch(attr&112)        // test for a light background
    {

    case _LGREY:
    case _GREEN:
    case _CYAN:
    case _BROWN:
        attr &= 240;        // foreground = black
        attr |= 112;        // background = light grey
        break;

    default:
        if((attr&15)==8)    // if foreground = dark grey
            attr &= 247;      // clear intensity bit
        attr |= 7;          // foreground = light grey
        attr &= 143;        // background = black
    }

    return attr;              // return converted attribute
}


//  ------------------------------------------------------------------
//  Reverses the attribute given

vattr revsattr(vattr attr)
{
    return (vattr)(((attr>>4)&0x07)|((attr<<4)&0x70)|(attr&0x80)|(attr&0x08));
}

#if !defined(__USE_NCURSES__)


//  ------------------------------------------------------------------

#if defined(__UNIX__)
char* gvid_newattr(int& attr)
{

    // 12345678901234567890
    // E[1;33;44mE[11m
    static char newattr[20];
    *newattr = NUL;
    if(attr != gvid_last_attr)
    {
        if((attr & ~ACSET) != (gvid_last_attr & ~ACSET))
        {
            sprintf(newattr, "\033[%c;3%u;4%um",
                    vatr2ansin(attr) ? '1' : '0',
                    vatr2ansfg(attr),
                    vatr2ansbg(attr)
                   );
        }
        if((attr & ACSET) != (gvid_last_attr & ACSET))
            strcat(newattr, (attr & ACSET) ? gvid_acs_enable : gvid_acs_disable);
        gvid_last_attr = attr;
    }

    return newattr;
}
#endif


//  ------------------------------------------------------------------
//  OS/2 Vio* wrappers for prevent 16-bit segment overrun

#if defined(__OS2__)

#ifndef _THUNK_PTR_SIZE_OK
    #define _THUNK_PTR_SIZE_OK(ptr,size) (((ULONG)(ptr) & ~0xffff) == (((ULONG)(ptr) + (size) - 1) & ~0xffff))
#endif

static USHORT VioReadCellStr_(PCH str, PUSHORT pcb, USHORT row, USHORT col, HVIO hvio)
{
    USHORT rc, cb = *pcb;

    if(_THUNK_PTR_SIZE_OK(str, cb))
        return VioReadCellStr(str, pcb, row, col, hvio);
    PCH newstr = (PCH)throw_xmalloc(cb * 2);
    if(_THUNK_PTR_SIZE_OK(newstr, cb))
    {
        rc = VioReadCellStr(newstr, pcb, row, col, hvio);
        if(rc == 0)
            memcpy(str, newstr, *pcb);
    }
    else
    {
        rc = VioReadCellStr(newstr + cb, pcb, row, col, hvio);
        if(rc == 0)
            memcpy(str, newstr + cb, *pcb);
    }
    throw_xfree(newstr);
    return rc;
}


static USHORT VioWrtCellStr_(PCCH str, USHORT cb, USHORT row, USHORT col, HVIO hvio)
{
    USHORT rc;

    if(_THUNK_PTR_SIZE_OK(str, cb ))
        return VioWrtCellStr(str, cb, row, col, hvio);
    PCH newstr = (PCH)throw_xmalloc(cb * 2);
    if(_THUNK_PTR_SIZE_OK(newstr, cb))
    {
        memcpy(newstr, str, cb);
        rc = VioWrtCellStr(newstr, cb, row, col, hvio);
    }
    else
    {
        memcpy(newstr + cb, str, cb);
        rc = VioWrtCellStr(newstr + cb, cb, row, col, hvio);
    }
    throw_xfree(newstr);
    return rc;
}


static USHORT VioWrtCharStrAtt_(PCCH str, USHORT cb, USHORT row, USHORT col, PBYTE attr, HVIO hvio)
{
    USHORT rc;

    if(_THUNK_PTR_SIZE_OK(str, cb))
        return VioWrtCharStrAtt(str, cb, row, col, attr, hvio);
    PCH newstr = (PCH)throw_xmalloc(cb * 2);
    if(_THUNK_PTR_SIZE_OK(newstr, cb))
    {
        memcpy(newstr, str, cb);
        rc = VioWrtCharStrAtt(newstr, cb, row, col, attr, hvio);
    }
    else
    {
        memcpy(newstr + cb, str, cb);
        rc = VioWrtCharStrAtt(newstr + cb, cb, row, col, attr, hvio);
    }
    throw_xfree(newstr);
    return rc;
}

#define VioReadCellStr         VioReadCellStr_
#define VioWrtCellStr          VioWrtCellStr_
#define VioWrtCharStrAtt       VioWrtCharStrAtt_

#endif

//  ------------------------------------------------------------------
//  ncurses support functions

#else // defined(__USE_NCURSES__)

//  ------------------------------------------------------------------
//  Compute our attributes from DOS attributes

int gvid_attrcalc(int dosattr)
{

    // DOS attrs: XRGBxrgb
    // color pair definition: 00RGBrgb, with last 3 bits negated
    int attr;
    attr = COLOR_PAIR(((dosattr & 0x70) >> 1) | ((~dosattr) & 0x07));
    if(dosattr & 0x08)
        attr |= A_BOLD;
//  if(dosattr & 0x80)
//    attr |= A_BLINK;

    return attr;
}

//  ------------------------------------------------------------------
//  Compute DOS attributes from our attributes

int gvid_dosattrcalc(int ourattr)
{

    int attr = 0;
    attr = PAIR_NUMBER(ourattr);
    attr = ((attr & 0x38) << 1) | ((~attr) & 0x07);
    if(ourattr & A_BLINK)
        attr |= 0x80;
    if(ourattr & A_BOLD)
        attr |= 0x08;

    return attr;
}

//  ------------------------------------------------------------------
//  Transform character < 32 into printable equivalent


#if defined(__USE_WIDE_NCURSES__)

vchar gvid_tcpr(vchar chr)
{

    //  GoldED inherits the DOS convention that a byte below 32 is a
    //  glyph, not a control code - CP437 draws smileys and arrows there
    //  and message text uses them. With the wide API we can render the
    //  characters CP437 actually meant instead of approximating them
    //  from the line-drawing set.

    static const uint16_t gvid_cpr[32] =
    {
        0x0020, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
        0x25D8, 0x25CB, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C,
        0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8,
        0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC
    };

    return (chr < 32) ? (vchar)gvid_cpr[chr] : chr;
}

#else

chtype gvid_tcpr(vchar chr)
{

    const chtype gvid_cpr[] =
    {
        (chtype)' ', (chtype)'@', (chtype)'@', (chtype)'x',
        (chtype) ACS_DIAMOND, (chtype)'x', (chtype)'x', ACS_BULLET,
        ACS_BULLET, ACS_BULLET, ACS_BULLET, (chtype)'x',
        (chtype)'x', (chtype)'x', (chtype)'x', ACS_LANTERN,
        (chtype)ACS_RARROW, ACS_LARROW, (chtype)'x', (chtype)'!',
        (chtype)'x', (chtype)'x', ACS_S1, (chtype)'x',
        ACS_UARROW, ACS_DARROW, ACS_LARROW, (chtype)ACS_RARROW,
        (chtype)'x', (chtype)'x', ACS_UARROW, ACS_DARROW
    };

    chtype ch = chr & A_CHARTEXT;
    chtype at = chr & (~A_CHARTEXT);

    if(ch<' ')
        return gvid_cpr[ch] | at;
    else
        return ch | at;
}

#endif


//  ------------------------------------------------------------------
//  Emitting characters through curses.
//
//  gvid_addcp() puts a single character; gvid_addstr() puts a string,
//  decoding it from UTF-8 when that is what we are holding text in, and
//  optionally padding it out to a fixed number of screen columns.
//
//  Note that 'width' counts columns, not bytes: a padded field has to
//  line up on screen, which is not the same as lining up in memory once
//  characters stop being one byte each.

//  ------------------------------------------------------------------
//  Frames on a system that calls box drawing double-width
//
//  Solaris resolves the East Asian "ambiguous" width class as two
//  columns, and every box-drawing character is in it - so is the solid
//  block, the shade and the arrows. curses then advances two cells for a
//  frame character while the rest of GoldED counts one: windows come out
//  stretched, and anything landing in the last column has nowhere to go
//  and wraps onto the next line.
//
//  The alternate character set draws the same lines and is always one
//  column wide, so use it where that happens. Double-line frames come
//  out single-line there, which is the price of having them line up.
//  Decided once, from what the C library says about a plain horizontal
//  line.

bool gvid_acs_box(vchar chr, wchar_t* key)
{
#if defined(__USE_WIDE_NCURSES__)

    static int needed = -1;

    if(needed < 0)
    {
#if defined(GOLD_HAVE_WCWIDTH)
        needed = (wcwidth((wchar_t)0x2500) != 1) ? 1 : 0;
#else
        //  No wcwidth() to ask. Every system that has been looked at
        //  bar Solaris draws box characters one column wide, so take
        //  that and leave the alternate character set alone.
        needed = 0;
#endif
    }

    if(not needed)
        return false;

    chtype acs = 0;

    switch(chr)
    {
    case 0x2500: case 0x2550: acs = ACS_HLINE;    break;
    case 0x2502: case 0x2551: acs = ACS_VLINE;    break;
    case 0x250C: case 0x2554: acs = ACS_ULCORNER; break;
    case 0x2510: case 0x2557: acs = ACS_URCORNER; break;
    case 0x2514: case 0x255A: acs = ACS_LLCORNER; break;
    case 0x2518: case 0x255D: acs = ACS_LRCORNER; break;
    case 0x251C: case 0x2560: acs = ACS_LTEE;     break;
    case 0x2524: case 0x2563: acs = ACS_RTEE;     break;
    case 0x252C: case 0x2566: acs = ACS_TTEE;     break;
    case 0x2534: case 0x2569: acs = ACS_BTEE;     break;
    case 0x253C: case 0x256C: acs = ACS_PLUS;     break;
    case 0x2588:              acs = ACS_BLOCK;    break;
    case 0x2591: case 0x2592:
    case 0x2593:              acs = ACS_CKBOARD;  break;
    case 0x2190:              acs = ACS_LARROW;   break;
    case 0x2191:              acs = ACS_UARROW;   break;
    case 0x2192:              acs = ACS_RARROW;   break;
    case 0x2193:              acs = ACS_DARROW;   break;
    case 0x2022: case 0x25CF: acs = ACS_BULLET;   break;
    default:                                      break;
    }

    if(not acs)
        return false;

    *key = (wchar_t)(acs & A_CHARTEXT);
    return true;

#else

    (void)chr;
    (void)key;
    return false;

#endif
}


//  ------------------------------------------------------------------

static void gvid_addcp(vchar chr, int attr)
{
#if defined(__USE_WIDE_NCURSES__)

    vatch   chat;
    wchar_t wch[2];

    //  Cells carry Unicode. In UTF-8 mode 'chr' already is a codepoint;
    //  in 8-bit mode it is a byte in the local charset and has to be
    //  translated, or a CP866 message base would come out as mojibake on
    //  a UTF-8 terminal.
    vchar cp = gvid_tcpr((vchar)g_local_to_unicode_wide(chr));

    wch[0] = (wchar_t)cp;
    wch[1] = L'\0';

    if(gvid_acs_box(cp, &wch[0]))
        attr |= A_ALTCHARSET;

    setcchar(&chat, wch, (attr_t)(attr & ~A_COLOR), (short)PAIR_NUMBER(attr), NULL);
    add_wch(&chat);

#else

    addch(gvid_tcpr(chr) | attr);

#endif
}


//  A byte out of a CP437 string literal, as the character it stands for.
//  Built once from the recoder, so it needs no table of its own.

static uint32_t gvid_cp437_to_unicode(unsigned char b)
{
    static uint32_t table[256];
    static bool     ready = false;

    if(not ready)
    {
        g_build_charset_table("CP437", table);
        ready = true;
    }

    return table[b];
}


static void gvid_addstr(const char* str, int attr, uint width, bool boxcvt)
{
    uint col = 0;

    while(*str and (width == 0 or col < width))
    {
        int   used = 1;
        vchar chr;

        if(boxcvt)
        {
            //  These strings are runs of CP437 bytes written straight
            //  into the source - the GoldED logo is one - so they are
            //  not text in the current charset and must not be decoded
            //  as such. Reading them as UTF-8 turned the logo into
            //  replacement characters. Take one byte and say what CP437
            //  means by it; gvid_boxcvtc() below recognises either form.
            chr = g_utf8_mode() ? (vchar)gvid_cp437_to_unicode((unsigned char)*str)
                                : (vchar)(unsigned char)*str;
        }
        else
            chr = (vchar)g_utf8_decode(str, &used);

        str += used ? used : 1;

        if(boxcvt)
            chr = gvid_boxcvtc(chr);

        //  A double-width character that would hang over the end of the
        //  field is dropped rather than half-drawn.
        int cw = g_cp_width(chr);
        if(width and col + (uint)cw > width)
            break;

        gvid_addcp(chr, attr);
        col += cw ? cw : 1;
    }

    while(width and col < width)
    {
        gvid_addcp((vchar)' ', attr);
        col++;
    }
}


//  ------------------------------------------------------------------

#endif // defined(__USE_NCURSES__)


//  ------------------------------------------------------------------
//  Text on Win32 goes out through the stream API rather than the cell
//  API. A CHAR_INFO holds one UTF-16 unit, so nothing above U+FFFF fits
//  in one, and a surrogate pair spread over two cells draws as two
//  unknown characters however it is flagged - measured on the console,
//  with and without COMMON_LVB_LEADING_BYTE. WriteConsoleW takes UTF-16
//  and builds the character itself. It also leaves the width
//  arithmetic to the console, which the cell path got wrong for every
//  wide character and not only for the astral ones.

#ifdef __WIN32__

//  One codepoint as UTF-16, forming a surrogate pair where it must.
static int gvid_utf16(vchar cp, WCHAR* w)
{
    if(cp > 0xFFFF and cp <= 0x10FFFF)
    {
        cp -= 0x10000;
        w[0] = (WCHAR)(0xD800 + (cp >> 10));
        w[1] = (WCHAR)(0xDC00 + (cp & 0x3FF));
        return 2;
    }

    w[0] = (WCHAR)cp;
    return 1;
}


#ifndef COMMON_LVB_LEADING_BYTE
#define COMMON_LVB_LEADING_BYTE  0x0100
#define COMMON_LVB_TRAILING_BYTE 0x0200
#endif

//  Append one codepoint to a run of cells the way the console itself
//  models text: a fullwidth character occupies two cells, the same
//  character in both, the first flagged leading and the second
//  trailing - how conhost has carried DBCS text from the beginning,
//  and what FAR Manager writes. One cell per fullwidth character, as
//  before, drew the glyph over its neighbour and put every following
//  character one column early, and the previous screen showed through
//  past the shortened line. A character above U+FFFF is stored as its
//  surrogate halves, one per cell: the classic console draws that
//  pair; where only the stream draws it, vputs() takes that path.
static int gvid_cellcp(vatch* cells, int at, vchar cp, vattr atr)
{
    WCHAR w[2];

    if(gvid_utf16(cp, w) > 1)
    {
        cells[at++] = vcatch((vchar)w[0], atr);
        cells[at++] = vcatch((vchar)w[1], atr);
    }
    else if(g_cp_width((uint32_t)cp) == 2)
    {
        cells[at++] = vcatch(cp, atr | COMMON_LVB_LEADING_BYTE);
        cells[at++] = vcatch(cp, atr | COMMON_LVB_TRAILING_BYTE);
    }
    else
    {
        cells[at++] = vcatch(cp, atr);
    }

    return at;
}


//  The two consoles want an astral character written differently, and
//  neither way suits both - measured in each:
//
//    conhost           the pair across two cells draws and stays put;
//                      a stream write onto a row otherwise written as
//                      cells makes conhost lay the row out afresh, and
//                      the characters shuffle as the cursor moves.
//    Windows Terminal  the pair across two cells draws as two unknown
//                      characters (microsoft/terminal#10810); only the
//                      stream draws it.
//
//  Windows Terminal sets WT_SESSION for the programs it starts, which
//  is the documented way of knowing it is there. The real cure is a
//  VT backend like FAR Manager's - see todowork.txt.

static bool gvid_wt_console()
{
    static int known = -1;

    if(known < 0)
    {
        //  GOLDED_CONSOLE overrides the guess: "cells" for a console
        //  that draws a surrogate pair from two cells (classic
        //  conhost), "stream" for one that only draws it written as a
        //  stream (Windows Terminal). The variable rather than a
        //  configuration keyword because the same golded.cfg serves
        //  both kinds of window; which console this is belongs to the
        //  window, and a shortcut can set it.
        const char* ov = getenv("GOLDED_CONSOLE");
        if(ov and strieql(ov, "cells"))
            known = 0;
        else if(ov and strieql(ov, "stream"))
            known = 1;
        else
        {
            //  The one positive mark of a classic conhost: a visible
            //  console window of class ConsoleWindowClass. Windows
            //  Terminal's window has a class of its own, and the
            //  default-terminal handoff can leave GetConsoleWindow()
            //  pointing at that visible window, so visibility alone
            //  told the two apart wrongly. Anything unrecognised is
            //  treated as the stream kind: a missing emoji column is
            //  better than two broken cells.
            //  Resolved at run time: the VC98 headers predate
            //  GetConsoleWindow(). Where the function itself does not
            //  exist - a Windows that old - the console is a classic
            //  one by definition, so cells.
            typedef HWND (WINAPI* GetConsoleWindow_fn)(void);
            HMODULE k32 = GetModuleHandleA("kernel32.dll");
            GetConsoleWindow_fn getwin = k32 ?
                (GetConsoleWindow_fn)GetProcAddress(k32, "GetConsoleWindow") : NULL;

            if(getwin == NULL)
            {
                known = 0;
            }
            else
            {
                HWND cw = getwin();
                char cls[64] = "";

                if(cw and IsWindowVisible(cw))
                    GetClassNameA(cw, cls, (int)sizeof(cls));

                known = strieql(cls, "ConsoleWindowClass") ? 0 : 1;
            }
        }
    }

    return known != 0;
}


static void gvid_wwrite(int row, int col, vattr atr, const WCHAR* w, int n)
{
    CONSOLE_SCREEN_BUFFER_INFO sb;
    CONSOLE_CURSOR_INFO ci;
    COORD at;
    DWORD wrote = 0;
    DWORD mode = 0;
    bool hidden = false;
    bool unwrapped = false;

    if(n <= 0)
        return;

    GetConsoleScreenBufferInfo(gvid_hout, &sb);

    //  Writing this way is a stream, and a stream that reaches the last
    //  cell of the last line makes the console scroll - which the cell
    //  API never did. The clock sits in the corner of the status line,
    //  so every tick pushed the whole screen up by a line. Turn the
    //  wrap off for the write and put it back: leaving it off would
    //  follow us out of the program and into the shell.
    if(GetConsoleMode(gvid_hout, &mode) and (mode & ENABLE_WRAP_AT_EOL_OUTPUT))
    {
        SetConsoleMode(gvid_hout, mode & ~(DWORD)ENABLE_WRAP_AT_EOL_OUTPUT);
        unwrapped = true;
    }

    //  Keep the caret away from the text while it is being written.
    //  FAR Manager does the same, and for the same reason: a caret
    //  standing on a surrogate pair spoils the character.
    if(GetConsoleCursorInfo(gvid_hout, &ci) and ci.bVisible)
    {
        ci.bVisible = FALSE;
        SetConsoleCursorInfo(gvid_hout, &ci);
        hidden = true;
    }

    at.X = (SHORT)col;
    at.Y = (SHORT)row;
    SetConsoleTextAttribute(gvid_hout, (WORD)atr);
    SetConsoleCursorPosition(gvid_hout, at);
    WriteConsoleW(gvid_hout, w, (DWORD)n, &wrote, NULL);

    //  Writing this way moves the caret and changes the current
    //  attribute, neither of which the cell API did; put both back.
    SetConsoleTextAttribute(gvid_hout, sb.wAttributes);
    SetConsoleCursorPosition(gvid_hout, sb.dwCursorPosition);

    if(hidden)
    {
        ci.bVisible = TRUE;
        SetConsoleCursorInfo(gvid_hout, &ci);
    }

    if(unwrapped)
        SetConsoleMode(gvid_hout, mode);
}

#endif


//  ------------------------------------------------------------------
//  Print character and attribute at specfied location

#if (defined(__MSDOS__) || defined(__UNIX__)) && !defined(__USE_NCURSES__)
inline void _vputw(int row, int col, word chat)
{

    _farpokew(_dos_ds, gdmaptr(col, row), chat);
}
#endif


void vputw(int row, int col, vatch chat)
{

#if defined(__USE_NCURSES__)

#if defined(__USE_WIDE_NCURSES__)
    mvadd_wch(row, col, &chat);
#else
    mvaddch(row, col, chat);
#endif
    gvid_refresh();

#elif defined(__MSDOS__)

    if(gvid->isdma())
    {
        _vputw(row, col, chat);
    }
    else if(gvid->isbios() or gvid->iscga())
    {
        i86 cpu;
        cpu.ah(2);
        cpu.bh(0);
        cpu.dh((byte)row);
        cpu.dl((byte)col);
        cpu.genint(0x10);
        cpu.ah(9);
        cpu.al(vgchar(chat));
        cpu.bh(0);
        cpu.bl(vgattr(chat));
        cpu.cx(1);
        cpu.genint(0x10);
    }

#elif defined(__OS2__)

    VioWrtNCell(GVIO_CELL_W &chat, 1, (USHORT)row, (USHORT)col, 0);

#elif defined(__WIN32__)

    const COORD coord = {0, 0};
    const COORD size = {1, 1};
    SMALL_RECT rect;

    rect.Top = row;
    rect.Left = col;
    rect.Bottom = row+size.Y-1;
    rect.Right = col+size.X-1;
    WriteConsoleOutputW(gvid_hout, &chat, size, coord, &rect);

#elif defined(__UNIX__)

    char chr = vgchar(chat);
    int atr = vgattr(chat);
    char* color = gvid_newattr(atr);

    gvid_cvtstr(&chat, 1);
    _vputw(row, col, chat);

    gvid_printf("\033[%u;%uH%s%c", row+1, col+1, color, chr);

#endif
}


//  ------------------------------------------------------------------
//  Print attrib/char buffer at specfied location

void vputws(int row, int col, vatch* buf, uint len)
{

#if defined(__USE_NCURSES__)

#if defined(__USE_WIDE_NCURSES__)

    //  The buffer holds one cell per column of GoldED's model, but a
    //  wide character covers two columns on screen. Writing the run in
    //  sequence lets it creep to the right by one column per wide
    //  character and spill out of the field it was given, over whatever
    //  is drawn next to it. Place every cell at the column it belongs
    //  to instead, and stop at the end of the field.
    int c = 0;
    for(uint counter = 0; counter < len and c < (int)len; counter++)
    {
        wchar_t wch[CCHARW_MAX];
        attr_t  attrs;
        short   pair;
        int     wide = 1;

        mvadd_wch(row, col + c, &buf[counter]);

        if(getcchar(&buf[counter], wch, &attrs, &pair, NULL) == OK and wch[0])
            wide = g_cp_width((uint32_t)wch[0]);

        c += (wide > 0) ? wide : 1;
    }

#else

    move(row, col);
    for(uint counter = 0; counter < len; counter++)
        addch(buf[counter]);

#endif

    gvid_refresh();

#elif defined(__MSDOS__)

    if(gvid->isdma())
    {
        gdmacpy(_dos_ds, (gdma)gdmaptr(col, row), _my_ds(), (gdma)buf, len*sizeof(word));
    }
    else if(gvid->isbios() or gvid->iscga())
    {
        i86 cpu;
        byte* p = (byte*)buf;
        for(uint n=0; n<len; n++)
        {
            cpu.ah(2);
            cpu.bh(0);
            cpu.dh((byte)row);
            cpu.dl((byte)col++);
            cpu.genint(0x10);
            cpu.ah(9);
            cpu.al(*p++);
            cpu.bh(0);
            cpu.bl(*p++);
            cpu.cx(1);
            cpu.genint(0x10);
        }
    }

#elif defined(__OS2__)

    VioWrtCellStr((PCCH)buf, (USHORT)(len*2), (USHORT)row, (USHORT)col, 0);

#elif defined(__WIN32__)

    const COORD coord = {0, 0};
    COORD size = {len, 1};
    SMALL_RECT rect;

    rect.Top = row;
    rect.Left = col;
    rect.Bottom = row+size.Y-1;
    rect.Right = col+size.X-1;
    //  The cells hold Unicode already, put there by vcatch().
    WriteConsoleOutputW(gvid_hout, buf, size, coord, &rect);

#elif defined(__UNIX__)

    gvid_cvtstr(buf, len);
    gdmacpy(_dos_ds, (gdma)gdmaptr(col, row), _my_ds(), (gdma)buf, len*sizeof(word));
    vputansi(row, col, buf, len);

#endif
}


//  ------------------------------------------------------------------
//  Print character and attribute at specfied location

void vputc(int row, int col, vattr atr, vchar chr)
{

#if defined(__USE_NCURSES__)

#if defined(__USE_WIDE_NCURSES__)
    //  As in gvid_addcp(): the cell holds a codepoint, and in 8-bit mode
    //  the caller handed us a byte in the local charset.
    vatch chat = vcatch(gvid_tcpr((vchar)g_local_to_unicode_wide(chr)), atr);
    mvadd_wch(row, col, &chat);
#else
    mvaddch(row, col, vcatch(gvid_tcpr(chr), atr));
#endif
    gvid_refresh();

#elif defined(__MSDOS__)

    if(gvid->isdma())
    {
        _vputw(row, col, vcatch(chr, atr));
    }
    else if(gvid->isbios() or gvid->iscga())
    {
        i86 cpu;
        cpu.ah(2);
        cpu.bh(0);
        cpu.dh((byte)row);
        cpu.dl((byte)col);
        cpu.genint(0x10);
        cpu.ah(9);
        cpu.al(chr);
        cpu.bh(0);
        cpu.bl((byte)atr);
        cpu.cx(1);
        cpu.genint(0x10);
    }

#elif defined(__WIN32__)

    //  The same rule as for a whole string: a cell holds it unless it
    //  is above U+FFFF. The editor draws the character under the
    //  cursor through here, which is why an emoji has to reach the
    //  stream API - but nothing else should.
    {
        vchar cp = gvid_tcpr(chr);

        if(cp > 0xFFFF)
        {
            WCHAR w[2];
            int n = gvid_utf16(cp, w);

            if(gvid_wt_console())
            {
                gvid_wwrite(row, col, atr, w, n);
            }
            else
            {
                //  Both halves in one write. Written one cell at a
                //  time, conhost sees a lone surrogate after the first
                //  and lays the row out around it, so the characters
                //  jumped as the cursor crossed an emoji.
                vatch two[2];
                two[0] = vcatch((vchar)w[0], atr);
                two[1] = vcatch((vchar)w[1], atr);
                vputws(row, col, two, 2);
            }
        }
        else if(g_cp_width((uint32_t)cp) == 2)
        {
            //  A fullwidth character is a pair of cells here too, or
            //  redrawing the character under the cursor would collapse
            //  it to one cell and shift the rest of the row.
            vatch two[2];
            two[0] = vcatch(cp, atr | COMMON_LVB_LEADING_BYTE);
            two[1] = vcatch(cp, atr | COMMON_LVB_TRAILING_BYTE);
            vputws(row, col, two, 2);
        }
        else
        {
            vputw(row, col, vcatch(cp, atr));
        }
    }

#elif defined(__OS2__)

    vputw(row, col, vcatch(chr, atr));

#elif defined(__UNIX__)

    char* color = gvid_newattr(atr);
    gvid_cvtstr(&chr, 1);
    _vputw(row, col, vcatch(chr, atr));

    gvid_printf("\033[%u;%uH%s%c", row+1, col+1, color, chr);

#endif
}


//  ------------------------------------------------------------------
//  Print string with attribute at specfied location

void vputvs(int row, int col, vattr atr, const vchar* str)
{

#if defined(__USE_NCURSES__)

    uint counter;
    int attr = gvid_attrcalc(atr);
    move(row, col);
    for(counter = 0; str[counter] != 0; counter++)
        gvid_addcp(str[counter], attr);
    gvid_refresh();

#elif defined(__WIN32__)

    //  Already a run of codepoints, so it goes straight into cells -
    //  passing it to vputs() would mean reinterpreting it as bytes.
    int i, cells = 0;
    for(i = 0; str[i] and (cells < gvid->numcols); i++)
    {
        WCHAR w16[2];
        int need = (gvid_utf16(str[i], w16) > 1 or g_cp_width((uint32_t)str[i]) == 2) ? 2 : 1;
        if(cells + need > gvid->numcols)
            break;
        cells = gvid_cellcp(gvid->bufwrd, cells, str[i], atr);
    }
    if(cells)
        vputws(row, col, gvid->bufwrd, cells);

#else

    //  Elsewhere a vchar is a byte, so the two are the same thing.
    vputs(row, col, atr, (const char*)str);

#endif
}


//  ------------------------------------------------------------------
//  Print string with attribute at specfied location

void vputs_box(int row, int col, vattr atr, const char* str)
{
#if defined(__USE_NCURSES__)
    move(row, col);
    gvid_addstr(str, gvid_attrcalc(atr), 0, true);
    gvid_refresh();
#else

    //  These strings are drawn with the box-drawing bytes of CP437 -
    //  the startup screen is written that way. That is not UTF-8, so in
    //  a UTF-8 session each byte would be taken for the codepoint it
    //  happens to name and the frame would come out as accented Latin
    //  letters. Convert each byte to the character the current box
    //  style asks for and hand the result on as UTF-8. In a single-byte
    //  session the bytes are already right and nothing is touched.
    if(g_utf8_mode())
    {
        std::string out;

        for(const char* s = str; *s; s++)
            out += g_utf8_encode(gvid_boxcvtc((vchar)(unsigned char)*s));

        vputs(row, col, atr, out.c_str());
        return;
    }

    vputs(row, col, atr, str);

#endif
}

void vputs(int row, int col, vattr atr, const char* str)
{

#if defined(__USE_NCURSES__)

    move(row, col);
    gvid_addstr(str, gvid_attrcalc(atr), 0, false);
    gvid_refresh();

#elif defined(__MSDOS__)

    if(gvid->isdma())
    {
        gdma p = gdmaptr(col, row);
        _farsetsel(_dos_ds);
        while(*str)
        {
            _farnspokew(p, vcatch(*str++, atr));
            p += ATTRSIZE;
        }
    }
    else if(gvid->isbios() or gvid->iscga())
    {
        i86 cpu;
        for(const char* q=str; *q; q++)
        {
            // Write as fast as possible on XT bios...
            cpu.ah(2);
            cpu.bh(0);
            cpu.dh((byte)row);
            cpu.dl((byte)(col++));
            cpu.genint(0x10);
            cpu.ah(9);
            cpu.al(*q);
            cpu.bh(0);
            cpu.bl((byte)atr);
            cpu.cx(1);
            cpu.genint(0x10);
        }
    }

#elif defined(__OS2__)

    VioWrtCharStrAtt((PCCH)str, (USHORT)strlen(str), (USHORT)row, (USHORT)col, (PBYTE)&atr, 0);

#elif defined(__WIN32__)

    //  Not through the cell API. A CHAR_INFO holds one UTF-16 unit, so
    //  nothing above U+FFFF fits in one, and a surrogate pair spread
    //  over two cells draws as two unknown characters however it is
    //  flagged - measured on the console, with and without
    //  COMMON_LVB_LEADING_BYTE. WriteConsoleW takes UTF-16 and builds
    //  the character itself, and that is the only way an emoji appears
    //  at all. It also leaves the width arithmetic to the console,
    //  which the cell loop got wrong for every wide character and not
    //  only for the astral ones.

    WCHAR wbuf[1024];
    int   n = 0;
    int   cols = 0;

    //  Stop at the right edge, counting columns from where we start
    //  writing rather than characters from zero. The cell API clipped
    //  whatever ran past the edge; WriteConsoleW wraps to the next line
    //  instead and would push the rest of the screen up, so the text
    //  has to be cut here. A character that would straddle the edge is
    //  left out rather than half-drawn.
    const int room = gvid->numcols - col;

    int  cells  = 0;
    bool astral = false;

    //  bufwrd holds numcols+1 cells, so even a surrogate pair landing
    //  on the last column fits: cells can reach numcols-1 here and the
    //  pair takes it to numcols+1, the capacity exactly.
    while(*str and n < (int)ARRAYSIZE(wbuf)-2 and cells < gvid->numcols)
    {
        int used = 1;
        vchar cp = gvid_tcpr((vchar)g_utf8_decode(str, &used));
        int  w   = g_cp_width(cp);

        if(cols + (w > 0 ? w : 1) > room)
            break;

        str += used ? used : 1;
        cols += (w > 0) ? w : 1;

        WCHAR pair[2];
        int   np = gvid_utf16(cp, pair);

        if(np > 1)
            astral = true;
        cells = gvid_cellcp(gvid->bufwrd, cells, cp, atr);

        n += gvid_utf16(cp, wbuf + n);
    }

    //  Prefer the cells. They are what this console was built for: the
    //  write neither moves the caret nor scrolls the screen nor has to
    //  borrow the wrap flag, and everything that worked before keeps
    //  working. The stream API is only reached for a character that
    //  cannot be held in a cell at all - one above U+FFFF - because a
    //  surrogate pair spread over two cells draws as two unknown
    //  characters however it is flagged, measured on the console.
    if(astral and gvid_wt_console())
        gvid_wwrite(row, col, atr, wbuf, n);
    else if(cells)
        vputws(row, col, gvid->bufwrd, (uint)cells);

#elif defined(__UNIX__)

    char buf[1024];
    strcpy(buf, str);
    char* color = gvid_newattr(atr);
    gvid_cvtstr(buf, strlen(buf));
    gdma p = gdmaptr(col, row);
    _farsetsel(_dos_ds);
    while(*str)
    {
        _farnspokew(p, vcatch(*str++, atr));
        p += ATTRSIZE;
    }

    gvid_printf("\033[%u;%uH%s%s", row+1, col+1, color, buf);

#endif
}


//  ------------------------------------------------------------------
//  Print string with attribute at specfied location

#if (defined(__MSDOS__) || defined(__UNIX__)) && !defined(__USE_NCURSES__)
static void _vputns(int row, int col, int atr, const char* str, uint width)
{

    char fillchar = ' ';

    gdma p = gdmaptr(col, row);
    _farsetsel(_dos_ds);
    while(width--)
    {
        _farnspokew(p, (atr << 8) | (*str ? *str++ : fillchar));
        p += ATTRSIZE;
    }
}
#endif


//  ------------------------------------------------------------------
//  Print string with attribute at specfied location

void vputns(int row, int col, vattr atr, const char* str, uint width)
{

    char fillchar = ' ';

#if defined(__USE_NCURSES__)

    move(row, col);
    gvid_addstr(str, gvid_attrcalc(atr), width, false);
    gvid_refresh();

#elif defined(__MSDOS__)

    if(gvid->isdma())
    {
        _vputns(row, col, atr, str, width);
    }
    else if(gvid->isbios() or gvid->iscga())
    {
        i86 cpu;
        while(width--)
        {
            // Write as fast as possible on XT bios...
            cpu.ah(2);
            cpu.bh(0);
            cpu.dh((byte)row);
            cpu.dl((byte)(col++));
            cpu.genint(0x10);
            cpu.ah(9);
            cpu.al(*str ? *str++ : fillchar);
            cpu.bh(0);
            cpu.bl((byte)atr);
            cpu.cx(1);
            cpu.genint(0x10);
        }
    }

#elif defined(__OS2__)

    uint len = strlen(str);

    VioWrtCharStrAtt((PCCH)str, (USHORT)minimum_of_two(len,width), (USHORT)row, (USHORT)col, (PBYTE)&atr, 0);

    if(width > len)
    {
        vatch filler = vcatch(fillchar, atr);
        VioWrtNCell(GVIO_CELL_W &filler, (USHORT)(width-len), (USHORT)row, (USHORT)(col+len), 0);
    }

#elif defined(__WIN32__)

    int cells = 0;

    if (width > gvid->numcols)
        width = gvid->numcols;

    while((cells < (int)width) and *str)
    {
        int used = 1;
        vchar chr = (vchar)g_utf8_decode(str, &used);
        str += used ? used : 1;

        //  A fullwidth character takes two of the field's cells; one
        //  that would only half fit is left out, and the filler pads
        //  the rest of the field.
        WCHAR w16[2];
        int need = (gvid_utf16(chr, w16) > 1 or g_cp_width((uint32_t)chr) == 2) ? 2 : 1;
        if(cells + need > (int)width)
            break;

        cells = gvid_cellcp(gvid->bufwrd, cells, chr, atr);
    }
    vatch filler = vcatch(fillchar, atr);
    for(; cells < (int)width; cells++)
        gvid->bufwrd[cells] = filler;
    vputws(row, col, gvid->bufwrd, width);

#elif defined(__UNIX__)

    char* color = gvid_newattr(atr);

    uint len = strlen(str);
    uint min_len = minimum_of_two(len, width);
    char buf[1024];
    strcpy(buf, str);
    gvid_cvtstr(buf, len);

    _vputns(row, col, atr, buf, width);

    char fillbuf[256];
    if(width > len)
    {
        memset(fillbuf, fillchar, width-len);
        fillbuf[width-len] = NUL;
    }
    else
    {
        *fillbuf = NUL;
    }

    gvid_printf("\033[%u;%uH%s%*.*s%s", row+1, col+1, color,
                min_len, min_len, buf, fillbuf
               );

#endif
}


//  ------------------------------------------------------------------
//  Print horizontal line of character and attribute

#if (defined(__MSDOS__) || defined(__UNIX__)) && !defined(__USE_NCURSES__)
void _vputx(int row, int col, int atr, char chr, uint len)
{

    gdma p = gdmaptr(col, row);
    word tmp = vcatch(chr, atr);
    _farsetsel(_dos_ds);
    for(uint n=0; n<len; n++)
    {
        _farnspokew(p, tmp);
        p += ATTRSIZE;
    }
}
#endif


//  ------------------------------------------------------------------
//  Print horizontal line of character and attribute

void vputx(int row, int col, vattr atr, vchar chr, uint len)
{

#if defined(__USE_NCURSES__)

#if defined(__USE_WIDE_NCURSES__)
    vatch chat = vcatch(gvid_tcpr((vchar)g_local_to_unicode_wide(chr)), atr);
    mvhline_set(row, col, &chat, len);
#else
    mvhline(row, col, vcatch(gvid_tcpr(chr), atr), len);
#endif
    gvid_refresh();

#elif defined(__MSDOS__)

    if(gvid->isdma())
    {
        _vputx(row, col, atr, chr, len);
    }
    else if(gvid->isbios() or gvid->iscga())
    {
        i86 cpu;
        cpu.ah(2);
        cpu.bh(0);
        cpu.dh((byte)row);
        cpu.dl((byte)col);
        cpu.genint(0x10);
        cpu.ah(9);
        cpu.al(chr);
        cpu.bh(0);
        cpu.bl((byte)atr);
        cpu.cx((word)len);
        cpu.genint(0x10);
    }

#elif defined(__OS2__)

    vatch filler = vcatch(chr, atr);
    VioWrtNCell(GVIO_CELL_W &filler, (USHORT)len, (USHORT)row, (USHORT)col, 0);

#elif defined(__WIN32__)

    if (len > gvid->numcols)
        len = gvid->numcols;

    vatch filler = vcatch(chr, atr);
    for(int i = 0; i < len; i++)
        gvid->bufwrd[i] = filler;
    vputws(row, col, gvid->bufwrd, len);

#elif defined(__UNIX__)

    char buf[256];
    char* color = gvid_newattr(atr);
    gvid_cvtchr(chr);
    _vputx(row, col, atr, chr, len);
    memset(buf, chr, len);
    buf[len] = NUL;
    gvid_printf("\033[%u;%uH%s%s", row+1, col+1, color, buf);

#endif
}


//  ------------------------------------------------------------------
//  Print vertical line of character and attribute

#if (defined(__MSDOS__) || defined(__UNIX__)) && !defined(__USE_NCURSES__)
inline void _vputy(int row, int col, int atr, char chr, uint len)
{

    gdma p = gdmaptr(col, row);
    word tmp = vcatch(chr, atr);
    _farsetsel(_dos_ds);
    for(uint n=0; n<len; n++)
    {
        _farnspokew(p, tmp);
        p += ATTRSIZE*gvid->numcols;
    }
}
#endif


//  ------------------------------------------------------------------
//  Print vertical line of character and attribute

void vputy(int row, int col, vattr atr, vchar chr, uint len)
{

#if defined(__USE_NCURSES__)

#if defined(__USE_WIDE_NCURSES__)
    vatch chat = vcatch(gvid_tcpr((vchar)g_local_to_unicode_wide(chr)), atr);
    mvvline_set(row, col, &chat, len);
#else
    mvvline(row, col, vcatch(gvid_tcpr(chr), atr), len);
#endif
    gvid_refresh();

#elif defined(__MSDOS__)

    if(gvid->isdma())
    {
        _vputy(row, col, atr, chr, len);
    }
    else if(gvid->isbios() or gvid->iscga())
    {
        for(uint n=0; n<len; n++)
        {
            i86 cpu;
            cpu.ah(2);
            cpu.bh(0);
            cpu.dh((byte)row++);
            cpu.dl((byte)col);
            cpu.genint(0x10);
            cpu.ah(9);
            cpu.al(chr);
            cpu.bh(0);
            cpu.bl((byte)atr);
            cpu.cx(1);
            cpu.genint(0x10);
        }
    }

#elif defined(__OS2__)

    vatch filler = vcatch(chr, atr);
    for(int n=0; n<len; n++)
        VioWrtNCell(GVIO_CELL_W &filler, 1, (USHORT)row++, (USHORT)col, 0);

#elif defined(__WIN32__)

    vatch filler = vcatch(chr, atr);
    for(int i=0; i < len; i++)
        vputw(row++, col, filler);

#elif defined(__UNIX__)

    char* color = gvid_newattr(atr);
    gvid_cvtchr(chr);
    _vputy(row, col, atr, chr, len);

    char buf[2048];
    sprintf(buf, "\033[%u;%uH%s", row+1, col+1, color);

    char* p = buf + strlen(buf);
    for(uint n=0; n<(len-1); n++)
    {
        *p++ = chr;
        if(col == gvid->numcols-1)
        {
            sprintf(p, "\033[%u;%uH", row+n+2, col+1);
            p += strlen(p);
        }
        else
        {
            strcpy(p, "\033[D\033[B");
            p += 6;
        }
    }
    *p++ = chr;
    *p = NUL;
    gvid_printf("%s", buf);

#endif
}


//  ------------------------------------------------------------------
//  Get character and attribute at cursor position

#if (defined(__MSDOS__) || defined(__UNIX__)) && !defined(__USE_NCURSES__)
inline word _vgetw(int row, int col)
{

    return _farpeekw(_dos_ds, gdmaptr(col, row));
}
#endif


//  ------------------------------------------------------------------
//  Get character and attribute at cursor position

vatch vgetw(int row, int col)
{

#if defined(__USE_NCURSES__)

#if defined(__USE_WIDE_NCURSES__)
    vatch chat;
    if(mvin_wch(row, col, &chat) == ERR)
        return vcatch(' ', BLACK_|_BLACK);
    return chat;
#else
    return mvinch(row, col);
#endif

#elif defined(__MSDOS__)

    if(gvid->isdma())
    {
        return _vgetw(row, col);
    }
    else if(gvid->isbios() or gvid->iscga())
    {
        i86 cpu;
        cpu.ah(2);
        cpu.bh(0);
        cpu.dh((byte)row);
        cpu.dl((byte)col);
        cpu.genint(0x10);
        cpu.ah(8);
        cpu.bh(0);
        cpu.genint(0x10);
        return cpu.ax();
    }
    return 0;

#elif defined(__OS2__)

    vatch chat;
    USHORT len=sizeof(chat);

    VioReadCellStr(GVIO_CELL_R &chat, &len, (USHORT)row, (USHORT)col, 0);

    return chat;

#elif defined(__WIN32__)

    vatch chat;
    const COORD coord = {0, 0};
    const COORD size = {1, 1};
    SMALL_RECT rect;

    rect.Top = row;
    rect.Left = col;
    rect.Bottom = row+size.Y-1;
    rect.Right = col+size.X-1;
    ReadConsoleOutput(gvid_hout, &chat, size, coord, &rect);

    return chat;

#elif defined(__UNIX__)

    return _vgetw(row, col);

#endif
}


//  ------------------------------------------------------------------
//  Get character and attribute at cursor position

void vgetc(int row, int col, vattr* atr, vchar* chr)
{

    if((row < 0) or (row > gvid->numrows-1) or (col < 0) or (col > gvid->numcols-1))
    {
        *chr = ' ';
        *atr = BLACK_|_BLACK;
    }
    else
    {
        vatch tmp = vgetw(row, col);

        *chr = vgchar(tmp);
        *atr = vgattr(tmp);
    }
}


//  ------------------------------------------------------------------
//  Scroll screen area

#if (defined(__MSDOS__) || defined(__UNIX__)) && !defined(__USE_NCURSES__)
static void _vscroll(int srow, int scol, int erow, int ecol, int atr, int lines)
{

    word empty = (atr << 8) | ' ';
    if(lines > 0)
    {
        while(lines--)
        {
            int nrow = srow;
            int l = ((ecol - scol) + 1);
            gdma scrptr = gdmaptr(scol, srow);
            while(nrow++ < erow)
            {
                gdmacpy(_dos_ds, (gdma)scrptr, _dos_ds, (gdma)(scrptr+ATTRSIZE*gvid->numcols), l*sizeof(word));
                scrptr += ATTRSIZE*gvid->numcols;
            }
            _farsetsel(_dos_ds);
            for(l *= ATTRSIZE; l>0;)
            {
                l -= ATTRSIZE;
                _farnspokew(scrptr+l, empty);
            }
        }
    }
    else
    {
        while(lines++)
        {
            int nrow = erow;
            int l = ((ecol - scol) + 1);
            gdma scrptr = gdmaptr(scol, erow);
            while(nrow-- >= (srow + 1))
            {
                gdmacpy(_dos_ds, (gdma)scrptr, _dos_ds, (gdma)(scrptr-ATTRSIZE*gvid->numcols), l*sizeof(word));
                scrptr -= ATTRSIZE*gvid->numcols;
            }
            _farsetsel(_dos_ds);
            for(l *= ATTRSIZE; l>0;)
            {
                l -= ATTRSIZE;
                _farnspokew(scrptr+l, empty);
            }
        }
    }
}
#endif


//  ------------------------------------------------------------------
//  Scroll screen area

void vscroll(int srow, int scol, int erow, int ecol, vattr atr, int lines)
{

#if defined(__USE_WIDE_NCURSES__)

    //  Let curses scroll its own window.
    //
    //  This used to read the region back with in_wchnstr() and write it
    //  out again one row over. That cannot work once a cell may be two
    //  columns wide: the read gives one element per character while the
    //  write is asked for one per column, so every row holding a wide
    //  character came back shifted, with the tail of that character left
    //  standing on screen. Reading the screen back is the wrong tool
    //  anyway - curses already knows where its wide characters are.

    vatch filler = vcatch(' ', atr);

    int height = 1 + erow - srow;
    int width  = 1 + ecol - scol;

    if((height <= 0) or (width <= 0) or (lines == 0))
        return;

    int count = lines;
    if(count >  height) count =  height;
    if(count < -height) count = -height;

    if(absolute(count) < height)
    {
        //  Move the cells with copywin() rather than scrolling a subwin.
        //  A subwindow shares its lines with the parent, and wscrl() is
        //  free to rearrange them; what came back had lost the colour
        //  pair on part of the region while keeping the other
        //  attributes, so a scrolled quote turned from yellow into the
        //  terminal's default. copywin() moves whole cells - character,
        //  attributes and colour pair together - and is what vsave() and
        //  vrestore() already use here.
        int rows    = height - absolute(count);
        int srcrow  = (count > 0) ? srow + count : srow;
        int dstrow  = (count > 0) ? srow : srow - count;

        //  A pad, not a window: it has no place on the screen, so it
        //  cannot take part in the update at all - a plain newwin() sits
        //  over stdscr and curses has to reason about the overlap.
        WINDOW* tmp = newpad(rows, width);
        if(tmp)
        {
            copywin(stdscr, tmp, srcrow, scol, 0, 0, rows-1, width-1, FALSE);
            copywin(tmp, stdscr, 0, 0, dstrow, scol,
                    dstrow+rows-1, scol+width-1, FALSE);
            delwin(tmp);
            //  Mark exactly the rows that were moved, and no others.
            //  touchwin() here claimed the whole screen had changed,
            //  which sent the scroll optimisation off a stale model of
            //  the terminal and left moved rows drawn without their
            //  colour; leaving it out entirely lost the destination
            //  rows that copywin() did not itself flag.
            //  NetBSD's own curses mis-tracks one row of a scrolled
            //  region - its update optimisation decides the row already
            //  matches and never rewrites it, so a quote line is left in
            //  the terminal's default colour. Marking the rows changed
            //  and telling curses the terminal contents there are
            //  unknown covers both implementations.
            //  The rows that moved, and no others: touchwin() over the
            //  whole screen sent the update optimisation off a stale
            //  model of the terminal and left moved rows without their
            //  colour.
            //
            //  NetBSD's own curses still mis-tracks exactly one row of a
            //  scrolled region under one particular rhythm of keys - it
            //  decides that row already matches and never rewrites it.
            //  Neither wtouchln() nor redrawwin() reaches it; the cell is
            //  correct in memory throughout. pkgsrc ncurses does not do
            //  it, so build with GOLD_EXTERNAL_CURSES=ON there if it
            //  matters.
            wredrawln(stdscr, srow, height);
        }
    }

    //  Blank what the scroll left behind, in the caller's attribute
    //  rather than the window's background.
    if(count > 0)
        for(int n = 0; n < count; n++)
            mvhline_set(1 + erow - count + n, scol, &filler, width);
    else
        for(int n = 0; n < -count; n++)
            mvhline_set(srow + n, scol, &filler, width);

    //  Flush the move now. Leaving it to the caller's repaint meant that
    //  anything refreshing in between - the status-line clock, once a
    //  second - pushed out a half-moved screen, and one row could be
    //  left standing in the wrong colour.
    gvid_refresh();

#ifdef GOLD_PAIR_PROBE
    if(getenv("GOLD_PAIR_PROBE"))
    {
        fprintf(stderr, "PAIRS after scroll(count=%d, rows %d..%d):", count, srow, erow);
        for(int r = srow; r <= erow; r++)
        {
            cchar_t c; attr_t a; short pr; wchar_t w[8];
            mvin_wch(r, scol + 2, &c);
            getcchar(&c, w, &a, &pr, NULL);
            fprintf(stderr, " %d:%d/%lx", r, (int)pr, (unsigned long)(a & A_BOLD ? 1 : 0));
        }
        fprintf(stderr, "\n");
    }
#endif

#elif defined(__USE_NCURSES__)

    vatch filler = vcatch(' ', atr);

    // Currently implemented with vsave/vrestore
    // Does anyone know a better solution?

    if(lines >= 0)
    {
        if(lines <= 1 + erow - srow)
        {
            vsavebuf *buf = vsave(srow + lines, scol, erow, ecol);
            vrestore(buf, srow, scol, erow - lines, ecol);
            throw_xfree(buf);
        }
        else
            lines = 1 + erow - srow;

        for(int counter = 0; counter < lines; counter++)
            mvhline(1 + erow + counter - lines, scol, filler, 1 + ecol - scol);
        gvid_refresh();
    }
    else
    {
        lines*=-1;
        if(lines <= 1 + erow - srow)
        {
            vsavebuf *buf = vsave(srow, scol, erow - lines, ecol);
            vrestore(buf, srow + lines, scol, erow, ecol);
            throw_xfree(buf);
        }
        else
            lines = 1 + erow - srow;

        for(int counter = 0; counter < lines; counter++)
            mvhline(srow + counter, scol, filler, 1 + ecol - scol);
        gvid_refresh();
    }

#elif defined(__MSDOS__)

    if(gvid->isdma())
    {
        _vscroll(srow, scol, erow, ecol, atr, lines);
    }
    else if(gvid->isbios() or gvid->iscga())
    {
        i86 cpu;
        cpu.ah((byte)(lines > 0 ? 6 : 7));
        cpu.al((byte)absolute(lines));
        cpu.bh((byte)atr);
        cpu.ch((byte)srow);
        cpu.cl((byte)scol);
        cpu.dh((byte)erow);
        cpu.dl((byte)ecol);
        cpu.genint(0x10);
    }

#elif defined(__OS2__)

    vatch filler = vcatch(' ', atr);

    if(lines > 0)
        VioScrollUp((USHORT)srow, (USHORT)scol, (USHORT)erow, (USHORT)ecol, (USHORT)lines, GVIO_CELL_W &filler, 0);
    else
        VioScrollDn((USHORT)srow, (USHORT)scol, (USHORT)erow, (USHORT)ecol, (USHORT)-lines, GVIO_CELL_W &filler, 0);

#elif defined(__WIN32__)

    SMALL_RECT r;
    COORD c = {scol, srow - lines};
    vatch filler = vcatch(' ', atr);

    r.Left   = (SHORT)scol;
    r.Top    = (SHORT)srow;
    r.Right  = (SHORT)ecol;
    r.Bottom = (SHORT)erow;

    ScrollConsoleScreenBuffer(gvid_hout, &r, &r, c, &filler);

#elif defined(__UNIX__)

    _vscroll(srow, scol, erow, ecol, atr, lines);

    gdma ptr = gdmaptr(scol, srow);
    int len = ecol-scol+1;
    for(int nrow=srow; nrow<=erow; nrow++)
    {
        vputansi(nrow, scol, ptr, len);
        ptr += ATTRSIZE*gvid->numcols;
    }

#endif
}


//  ------------------------------------------------------------------
//  Returns true if cursor invisible

bool vcurhidden()
{

    return __vcurhidden;
}

//  ------------------------------------------------------------------
//  Get cursor position

void vposget(int* row, int* col)
{

#if defined(__USE_NCURSES__)

    getyx(stdscr, gvid->currow, gvid->curcol);

#elif defined(__MSDOS__)

    i86 cpu;
    cpu.ah(3);
    cpu.bh(0);
    cpu.genint(0x10);
    gvid->currow = cpu.dh();
    gvid->curcol = cpu.dl();

#elif defined(__OS2__)

    USHORT _getrow, _getcol;
    VioGetCurPos(&_getrow, &_getcol, 0);
    gvid->currow = _getrow;
    gvid->curcol = _getcol;

#elif defined(__WIN32__)

    CONSOLE_SCREEN_BUFFER_INFO i;
    GetConsoleScreenBufferInfo(gvid_hout, &i);
    gvid->currow = i.dwCursorPosition.Y;
    gvid->curcol = i.dwCursorPosition.X;

#elif defined(__UNIX__)

    // Not available

#endif

    *row = gvid->currow;
    *col = gvid->curcol;
}


//  ------------------------------------------------------------------
//  Set cursor position

void vposset(int row, int col)
{

    gvid->currow = row;
    gvid->curcol = col;

#if defined(__USE_NCURSES__)

    move(row, col);
    gvid_refresh();

#elif defined(__MSDOS__)

    i86 cpu;
    cpu.ah(2);
    cpu.bh(0);
    cpu.dh((byte)row);
    cpu.dl((byte)col);
    cpu.genint(0x10);

#elif defined(__OS2__)

    VioSetCurPos((USHORT)row, (USHORT)col, 0);

#elif defined(__WIN32__)

    // No need to set the cursor position if its not visible
    // Strangely, this is a major speedup to screen-output

    if(__vcurhidden)
        return;

    COORD c = {col, row};
    SetConsoleCursorPosition(gvid_hout, c);

#elif defined(__UNIX__)

    gvid_printf("\x1B[%u;%uH", row+1, col+1);

#endif
}


//  ------------------------------------------------------------------
//  Clears the screen and homes the cursor

void vclrscr()
{

    vclrscr(vgattr(vgetw(gvid->currow, gvid->curcol)));
}


//  ------------------------------------------------------------------
//  Clears the screen using given attribute and homes the cursor

#if (defined(__MSDOS__) || defined(__UNIX__)) && !defined(__USE_NCURSES__)
static void _vclrscr(vattr atr)
{

    int len = gvid->numrows * gvid->numcols;

    _vputx(0, 0, atr, ' ', len);
}
#endif



//  ------------------------------------------------------------------
//  Clears the screen using given attribute and homes the cursor

void vclrscr(vattr atr)
{

#if defined(__USE_NCURSES__)

    clearok(stdscr, TRUE);
    vatch filler = vcatch(' ', atr);
    for(int row = 0; row < LINES; row++)
#if defined(__USE_WIDE_NCURSES__)
        mvhline_set(row, 0, &filler, COLS);
#else
        mvhline(row, 0, filler, COLS);
#endif
    move(0, 0);
    gvid_refresh();

#elif defined(__MSDOS__)

    if(gvid->isdma())
    {
        _vclrscr(atr);
    }
    else if(gvid->isbios() or gvid->iscga())
    {
        i86 cpu;
        cpu.ax(0x0600);           // clear screen by scrolling it
        cpu.bh((byte)atr);
        cpu.cx(0);
        cpu.dh((byte)(gvid->numrows - 1));
        cpu.dl((byte)(gvid->numcols - 1));
        cpu.genint(0x10);
    }

#elif defined(__OS2__)

    vatch filler = vcatch(' ', atr);
    VioScrollUp(0, 0, 0xFFFF, 0xFFFF, 0xFFFF, GVIO_CELL_W &filler, 0);

#elif defined(__WIN32__)

    COORD c = {0, 0};
    DWORD wr, len = gvid->numrows * gvid->numcols;
    // Filling with space seems to work for both Unicode and regular functions
    FillConsoleOutputCharacter(gvid_hout,       ' ', len, c, &wr);
    FillConsoleOutputAttribute(gvid_hout, (WORD)atr, len, c, &wr);

#elif defined(__UNIX__)

    _vclrscr(atr);

    gvid_printf("%s\x1B[2J", gvid_newattr(atr));

#endif

    vposset(0,0);
}


//  ------------------------------------------------------------------
//  Saves the current screen and returns pointer to buffer

#if (defined(__MSDOS__) || defined(__UNIX__)) && !defined(__USE_NCURSES__)
static void _vsave(word* buf, int len1, int srow, int scol, int erow)
{

    const int len2 = len1*sizeof(word);
    gdma p = gdmaptr(scol, srow);
    for(int nrow=srow; nrow<=erow; nrow++)
    {
        gdmacpy(_my_ds(), (gdma)buf, _dos_ds, (gdma)p, len2);
        p += ATTRSIZE*gvid->numcols;
        buf += len1;
    }
}
#endif


//  ------------------------------------------------------------------
//  Saves the current screen and returns pointer to buffer

vsavebuf* vsave(int srow, int scol, int erow, int ecol)
{

    if(srow == -1)  srow = 0;
    if(scol == -1)  scol = 0;
    if(erow == -1)  erow = gvid->numrows-1;
    if(ecol == -1)  ecol = gvid->numcols-1;

    // Calculate the number of rows and columns to save
    int num_rows = erow - srow + 1;
    int num_cols = ecol - scol + 1;

#if defined(__USE_NCURSES__)

    //  Let curses keep the rectangle for us.
    //
    //  Reading the cells back into an array of our own does not survive
    //  contact with wide characters: in_wchnstr() returns one element per
    //  character while the array is indexed per column, so everything to
    //  the right of a double-width character came back displaced -
    //  restoring a dialog over Japanese text scrambled it. NetBSD's
    //  curses adds an off-by-one of its own on top, losing the last
    //  column of every row. copywin() moves cells between windows inside
    //  curses, where the widths are known, and has neither problem.

    //  Take a column extra on each side. A double-width character
    //  straddling the edge would otherwise be saved by halves, and half a
    //  character cannot be put back - curses blanks both of its cells, so
    //  a dialog opened over Japanese text left a hole behind it.
    int padl = (scol > 0) ? 1 : 0;
    int padr = (ecol < gvid->numcols - 1) ? 1 : 0;

    scol     -= padl;
    ecol     += padr;
    num_cols += padl + padr;

    vsavebuf *sbuf = reinterpret_cast<vsavebuf *>(throw_xmalloc(sizeof(vsavebuf)));

    if(sbuf)
    {
        sbuf->top    = srow;
        sbuf->left   = scol;
        sbuf->bottom = erow;
        sbuf->right  = ecol;
        sbuf->padl   = padl;
        sbuf->padr   = padr;

        WINDOW* w = newwin(num_rows, num_cols, 0, 0);
        sbuf->win  = w;

        if(w == NULL)
        {
            throw_xfree(sbuf);
            return NULL;
        }

        copywin(stdscr, w, srow, scol, 0, 0, num_rows - 1, num_cols - 1, FALSE);
    }

    return sbuf;

#else

    vsavebuf *sbuf = reinterpret_cast<vsavebuf *>(throw_xmalloc(
        sizeof(vsavebuf) + num_rows * (num_cols) * sizeof(vatch) +
        sizeof(vatch)));

    if (sbuf)
      {
      vatch *buf = sbuf->data;

      sbuf->top = srow;
      sbuf->left = scol;
      sbuf->bottom = erow;
      sbuf->right = ecol;
#if defined(__MSDOS__)

        int len1 = ecol-scol+1;

        if(gvid->isdma())
        {
            _vsave(buf, len1, srow, scol, erow);
        }
        else if(gvid->isbios() or gvid->iscga())
        {
            i86 cpu;
            byte* p = (byte*)buf;
            for(byte row=(byte)srow; row<=erow; row++)
            {
                for(byte col=(byte)scol; col<=ecol; col++)
                {
                    cpu.ah(2);
                    cpu.bh(0);
                    cpu.dh(row);
                    cpu.dl(col);
                    cpu.genint(0x10);
                    cpu.ah(8);
                    cpu.bh(0);
                    cpu.genint(0x10);
                    *p++ = cpu.al();
                    *p++ = cpu.ah();
                }
            }
        }

#elif defined(__OS2__)

        int len1 = (int)(ecol-scol+1);

#if defined(__BORLANDC__)
        PCHAR16 ptr = (PCHAR16)buf;
#else
        PCH ptr = (PCH)buf;
#endif

        USHORT len2 = (USHORT)(len1*sizeof(word));
        for(int nrow=srow; nrow<=erow; nrow++)
        {
            VioReadCellStr(ptr, &len2, nrow, scol, 0);
            ptr += len2;
        }

#elif defined(__WIN32__)

        const COORD coord = {0, 0};
        COORD size = {ecol-scol+1, erow-srow+1};
        SMALL_RECT r;

        // Set the source rectangle.
        r.Top = srow;
        r.Left = scol;
        r.Bottom = erow;
        r.Right = ecol;

        if(WinVer.dwPlatformId == VER_PLATFORM_WIN32_NT)
            ReadConsoleOutputW(gvid_hout, buf, size, coord, &r);
        else
            ReadConsoleOutputA(gvid_hout, buf, size, coord, &r);

#elif defined(__UNIX__)

        int len1 = ecol-scol+1;

        _vsave(buf, len1, srow, scol, erow);

#endif
    }

    return sbuf;

#endif  /* __USE_NCURSES__ */
}


//  ------------------------------------------------------------------
//  Recolour one cell, leaving its character alone

void vsetattr(int row, int col, vattr atr)
{
#if defined(__USE_NCURSES__)

    int attr = gvid_attrcalc(atr);
    mvchgat(row, col, 1, (attr_t)(attr & ~A_COLOR), (short)PAIR_NUMBER(attr), NULL);

#elif defined(__WIN32__)

    //  Not by reading the cell and writing it back: ReadConsoleOutputW
    //  does not return surrogate pairs (microsoft/terminal#10810, and
    //  FAR Manager says the same), so a round trip would replace an
    //  astral character with half of itself. This API changes the
    //  colour and leaves the character alone, which is what the name
    //  promises.
    {
        COORD at;
        WORD  a = (WORD)atr;
        WORD  had = 0;
        DWORD done = 0;

        at.X = (SHORT)col;
        at.Y = (SHORT)row;

        //  The attribute word also carries the cell's half of a
        //  fullwidth pair - the leading/trailing flags. Blunt
        //  replacement stripped them, and the row lost its layout
        //  wherever the highlight passed over a wide character.
        //  Reading attributes back is safe; it is reading characters
        //  that loses surrogate pairs.
        if(ReadConsoleOutputAttribute(gvid_hout, &had, 1, at, &done))
            a = (WORD)((a & ~(COMMON_LVB_LEADING_BYTE|COMMON_LVB_TRAILING_BYTE))
                     | (had & (COMMON_LVB_LEADING_BYTE|COMMON_LVB_TRAILING_BYTE)));

        WriteConsoleOutputAttribute(gvid_hout, &a, 1, at, &done);
    }

#else

    //  Elsewhere a cell is a byte and a word, and writing it back is the
    //  same thing.
    vputw(row, col, vsattr(vgetw(row, col), atr));

#endif
}


//  ------------------------------------------------------------------
//  Release a buffer from vsave()

void vfreesave(vsavebuf* sbuf)
{
    if(sbuf == NULL)
        return;

#if defined(__USE_NCURSES__)
    if(sbuf->win)
        delwin((WINDOW*)sbuf->win);
#endif

    throw_xfree(sbuf);
}


//  ------------------------------------------------------------------
//  Redraws a previously saved screen

#if (defined(__MSDOS__) || defined(__UNIX__)) && !defined(__USE_NCURSES__)
static void _vredraw(word* buf, int len1, int srow, int scol, int erow)
{

    const int len2 = len1*sizeof(word);
    gdma p = gdmaptr(scol, srow);
    for(int nrow=srow; nrow<=erow; nrow++)
    {
        gdmacpy(_dos_ds, (gdma)p, _my_ds(), (gdma)buf, len2);
        p += ATTRSIZE*gvid->numcols;
        buf += len1;
    }
}
#endif

//  ------------------------------------------------------------------
//  Redraws a previously saved screen

void vrestore(vsavebuf* sbuf, int srow, int scol, int erow, int ecol)
{

    if(srow != -1)  sbuf->top = srow;
    if(erow != -1)  sbuf->bottom = erow;

#if defined(__USE_NCURSES__)
    //  The saved rectangle is a column wider on each side than what the
    //  caller asked for; a caller giving new coordinates means the same
    //  rectangle somewhere else, so pad those the same way.
    if(scol != -1)  sbuf->left  = scol - sbuf->padl;
    if(ecol != -1)  sbuf->right = ecol + sbuf->padr;
#else
    if(scol != -1)  sbuf->left = scol;
    if(ecol != -1)  sbuf->right = ecol;
#endif

    srow = sbuf->top;
    scol = sbuf->left;
    erow = sbuf->bottom;
    ecol = sbuf->right;

#if defined(__USE_NCURSES__)

    WINDOW* w = (WINDOW*)sbuf->win;

    if(w)
    {
        copywin(w, stdscr, 0, 0, srow, scol, erow, ecol, FALSE);
        gvid_refresh();
    }

    return;

#else

    vatch *buf = sbuf->data;

#if defined(__MSDOS__)

    int len1 = ecol-scol+1;

    if(gvid->isdma())
    {
        _vredraw(buf, len1, srow, scol, erow);
    }
    else if(gvid->isbios() or gvid->iscga())
    {
        i86 cpu;
        byte* p = (byte*)buf;
        for(byte row=(byte)srow; row<=erow; row++)
        {
            for(byte col=(byte)scol; col<=ecol; col++)
            {
                cpu.ah(2);
                cpu.bh(0);
                cpu.dh(row);
                cpu.dl(col);
                cpu.genint(0x10);
                cpu.ah(9);
                cpu.al(*p++);
                cpu.bh(0);
                cpu.bl(*p++);
                cpu.cx(1);
                cpu.genint(0x10);
            }
        }
    }

#elif defined(__OS2__)

    USHORT len1 = (USHORT)(ecol-scol+1);
    USHORT len2 = (USHORT)(len1*sizeof(word));

#if defined(__BORLANDC__)
    PCHAR16 ptr = (PCHAR16)buf;
#else
    PCH ptr = (PCH)buf;
#endif

    for(USHORT nrow=srow; nrow<=erow; nrow++)
    {
        VioWrtCellStr(ptr, len2, nrow, scol, 0);
        ptr += len2;
    }

#elif defined(__WIN32__)

    const COORD coord = {0, 0};
    COORD size = {ecol-scol+1, erow-srow+1};
    SMALL_RECT r;

    // Set the source rectangle.
    r.Top = srow;
    r.Left = scol;
    r.Bottom = erow;
    r.Right = ecol;

    if(WinVer.dwPlatformId == VER_PLATFORM_WIN32_NT)
        WriteConsoleOutputW(gvid_hout, buf, size, coord, &r);
    else
        WriteConsoleOutputA(gvid_hout, buf, size, coord, &r);

#elif defined(__UNIX__)

    int len1 = ecol-scol+1;

    _vredraw(buf, len1, srow, scol, erow);

    int atr = vgattr(*buf);
    char* color = gvid_newattr(atr);
    gvid_printf("%s", color);

    for(int nrow=srow; nrow<=erow; nrow++)
    {
        vputansi(nrow, scol, buf, len1);
        buf += len1;
    }

#endif

#endif  /* __USE_NCURSES__ */
}


//  ------------------------------------------------------------------
//  Sets the cursor shape/size

void vcurset(int sline, int eline)
{

    if(eline)
    {
        gvid->curr.cursor.start = sline;
        gvid->curr.cursor.end = eline;
        __vcurhidden = false;
    }

#if defined(__USE_NCURSES__)

    if((sline == 0) and (eline == 0))
        curs_set(0);
    else if((eline - sline) <= 4)
        curs_set(1);
    else
        curs_set(2);

#elif defined(__MSDOS__)

    if(eline == 0)
    {
        int _dvhide = __gdvdetected ? 0x01 : 0x30;
        sline = ((gvid->adapter>=V_HGC) and (gvid->adapter<=V_INCOLOR)) ? 0x3F : _dvhide;
    }

    i86 cpu;
    cpu.ah(1);
    cpu.ch((byte)sline);
    cpu.cl((byte)eline);
    cpu.genint(0x10);

#elif defined(__OS2__)

    VIOCURSORINFO vioci;
    VioGetCurType(&vioci, 0);
    vioci.yStart = (USHORT)sline;
    vioci.cEnd   = (USHORT)eline;
    vioci.attr   = (USHORT)((eline == 0) ? 0xFFFF : gvid->curr.color.textattr);
    VioSetCurType(&vioci, 0);

#elif defined(__WIN32__)

    CONSOLE_CURSOR_INFO cci;

    if(eline)
        vposset(gvid->currow, gvid->curcol);
    else  /* Move cursor to bottom right corner (workaround of the win9x console bug) */
        vposset(gvid->numrows-1, gvid->numcols-1);

    cci.dwSize = (eline and sline) ? sline : 100;
    cci.bVisible = make_bool(eline);

// To hide cursor in w98 needs change byte sequnce in compiled gedcyg.exe:
//   0F 95 C0 89 45 FC
//   B0 01 90 -- -- --

    SetConsoleCursorInfo(gvid_hout, &cci);

#elif defined(__UNIX__)

    gvid_printf("\033[?25%c", eline ? 'h' : 'l');

#endif
}


//  ------------------------------------------------------------------
//  Hides the cursor

void vcurhide()
{

    if(not __vcurhidden)
    {
#if defined(__USE_NCURSES__)
        curs_set(0);
#else
        vcurset(0,0);
#endif
        __vcurhidden = true;
    }
}


//  ------------------------------------------------------------------
//  Reveals the cursor

void vcurshow()
{

    if(__vcurhidden)
    {
        vcurset(gvid->curr.cursor.start, gvid->curr.cursor.end);
        __vcurhidden = false;
    }
}


//  ------------------------------------------------------------------
//  Sets a large cursor

void vcurlarge()
{

#if defined(__USE_NCURSES__)
    curs_set(2);
#else
    vcurshow();

#if defined(__MSDOS__)

    switch(gvid->adapter)
    {
    case V_CGA:
        vcurset(1,7);
        break;
    case V_EGA:
        if(gvid->numrows == 25)
        {
            vcurset(1,7);
        }
        else
        {
            word* p = (word*)0x0463;  // video BIOS data area
            outpw(*p,0x000A);         // update cursor start register
            outpw(*p,0x0A0B);         // update cursor end register
        }
        break;
    case V_VGA:
        vcurset(1,7);
        break;
    default:    // one of the monochrome cards
        vcurset(1,12);
    }

#elif defined(__OS2__)

    vcurset(1, gvid->curr.screen.cheight-1);

#elif defined(__WIN32__)

    vcurset(90, true);

#endif
#endif
}


//  ------------------------------------------------------------------
//  Sets a small cursor

void vcursmall()
{

#if defined(__USE_NCURSES__)
    curs_set(1);
#else
    vcurshow();

#if defined(__MSDOS__)

    switch(gvid->adapter)
    {
    case V_CGA:
        vcurset(6,7);
        break;
    case V_EGA:
        if(gvid->numrows == 25)
        {
            vcurset(6,7);
        }
        else
        {
            word* p = (word*)0x0463;    // video BIOS data area
            outpw(*p,0x060A);           // update cursor start register
            outpw(*p,0x000B);           // update cursor end register
        }
        break;
    case V_VGA:
        vcurset(6,7);
        break;
    default:    // one of the monochrome cards
        vcurset(11,12);
    }

#elif defined(__OS2__)

    vcurset(gvid->curr.screen.cheight-2, gvid->curr.screen.cheight-1);

#elif defined(__WIN32__)

    vcurset(13, true);

#endif
#endif
}


//  ------------------------------------------------------------------
//  Table of characters used to display boxes
//
//  Access box table characters via:
//      _box_table(boxtype, x)
//
//  where:
//      boxtype is the number of the box type you want to use (0 - 5)
//
//      x will be one of the following:
//           0 - upper left corner
//           1 - upper horizontal line
//           2 - upper right corner
//           3 - left vertical line
//           4 - right vertical line
//           5 - lower left corner
//           6 - lower horizontal line
//           7 - lower right corner
//           8 - middle junction
//           9 - left vertical junction
//          10 - right vertical junction
//          11 - upper horizontal junction
//          12 - lower horizontal junction
//          13 - checkerboard
//          14 - solid block
//  ------------------------------------------------------------------

//  ------------------------------------------------------------------
//  The box characters as Unicode.
//
//  Used wherever a screen cell holds a codepoint - the wide curses API
//  and the Windows console both do - so that every box type is drawn as
//  the characters it was designed as rather than approximated.

const uint16_t gvid_unibox[9][15] =
{
    //  box type 0  Single border
    { 0x250C, 0x2500, 0x2510, 0x2502, 0x2502, 0x2514, 0x2500, 0x2518, 0x253C, 0x251C, 0x2524, 0x252C, 0x2534, 0x2591, 0x2592 },
    //  box type 1  Double border
    { 0x2554, 0x2550, 0x2557, 0x2551, 0x2551, 0x255A, 0x2550, 0x255D, 0x256C, 0x2560, 0x2563, 0x2566, 0x2569, 0x2591, 0x2592 },
    //  box type 2  Single top
    { 0x2553, 0x2500, 0x2556, 0x2551, 0x2551, 0x2559, 0x2500, 0x255C, 0x256B, 0x255F, 0x2562, 0x2565, 0x2568, 0x2591, 0x2592 },
    //  box type 3  Double top
    { 0x2552, 0x2550, 0x2555, 0x2502, 0x2502, 0x2558, 0x2550, 0x255B, 0x256A, 0x255E, 0x2561, 0x2564, 0x2567, 0x2591, 0x2592 },
    //  box type 4  With empty border
    { 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x2591, 0x2592 },
    //  box type 5  No border at all
    { 0x250C, 0x2500, 0x2510, 0x2502, 0x2502, 0x2514, 0x2500, 0x2518, 0x253C, 0x251C, 0x2524, 0x252C, 0x2534, 0x2591, 0x2592 },
    //  box type 6  Blocky border
    { 0x2584, 0x2584, 0x2584, 0x258C, 0x2590, 0x2580, 0x2580, 0x2580, 0x258C, 0x258C, 0x258C, 0x258C, 0x258C, 0x2591, 0x2592 },
    //  box type 7  ASCII border
    { 0x002E, 0x002D, 0x002E, 0x007C, 0x007C, 0x0060, 0x002D, 0x0027, 0x002B, 0x007C, 0x007C, 0x002D, 0x002D, 0x0020, 0x0023 },
    //  box type 8  xterm single border
    { 0x006C, 0x0071, 0x006B, 0x0078, 0x0078, 0x006D, 0x0071, 0x006A, 0x006E, 0x0074, 0x0075, 0x0077, 0x0076, 0x0061, 0x0061 },
};


//  ------------------------------------------------------------------

#if !defined(__USE_NCURSES__)

char* __box_table[] =
{

#if defined(__UNIX__) // This table will be actually patched at startup...

    ".-.||`-'+||-- #",    // box type 0     Single border
    ".-.||`-'+||-- #",    // box type 1     Double border
    ".-.||`-'+||-- #",    // box type 2     Single top
    ".-.||`-'+||-- #",    // box type 3     Double top
    "              #",    // box type 4     With empty border
    ".-.||`-'+||-- #",    // box type 5     No border at all
    ".-.||`-'+||-- #",    // box type 6     Blocky border
    ".-.||`-'+||-- #",    // box type 7     ASCII border
    "lqkxxmqjntuwvaa",    // box type 8     xterm single border

#else

    "ÚÄ¿³³ÀÄÙÅÃ´ÂÁ°±",    // box type 0     Single border
    "ÉÍ»ººÈÍ¼ÎÌ¹ËÊ°±",    // box type 1     Double border
    "ÖÄ·ººÓÄ½×Ç¶ÒÐ°±",    // box type 2     Single top
    "ÕÍ¸³³ÔÍ¾ØÆµÑÏ°±",    // box type 3     Double top
    "             °±",    // box type 4     With empty border
    "ÚÄ¿³³ÀÄÙÅÃ´ÂÁ°±",    // box type 5     No border at all
    "ÜÜÜÝÞßßßÝÝÝÝÝ°±",    // box type 6     Blocky border
    ".-.||`-'+||--##",    // box type 7     ASCII border
    "lqkxxmqjntuwvaa",    // box type 8     xterm single border

#endif
};


//  ------------------------------------------------------------------
//  The table holds CP437 bytes. In UTF-8 mode a byte is not a
//  character, so hand back the codepoint that byte stands for; the
//  frames are drawn from constants baked into the source, not from
//  message text, so they cannot be decoded like the rest.

vchar _box_table(int type, int c)
{
    if(type < 0 or type > 8)
        type = 0;
    if(c < 0 or c > 14)
        c = 14;

    if(g_utf8_mode())
        return (vchar)gvid_unibox[type][c];

    return (vchar)(unsigned char)__box_table[type][c];
}


vchar _block_char()
{
    return g_utf8_mode() ? (vchar)0x2588 : (vchar)0xDB;
}
#else

// ncurses ACS_nnn characters are usually computed at runtime, so
// we cannot use a static array

#if defined(__USE_WIDE_NCURSES__)

//  With the wide API every box type can be drawn as the characters it
//  was designed as, instead of being flattened onto the single-line ACS
//  set: a double border stays double, a blocky one stays blocky.


vchar _box_table(int type, int c)
{

    if(type < 0 or type >= (int)ARRAYSIZE(gvid_unibox))
        type = 0;
    if(c < 0 or c > 14)
        c = 14;

    return (vchar)gvid_unibox[type][c];
}


vchar _block_char()
{
    return (vchar)0x2588;
}

#else

chtype _block_char()
{
    return ACS_BLOCK;
}


chtype _box_table(int type, int c)
{

    char asciiborder[] = ".-.||-'+||--##";

    switch(type)
    {
    case 4:
        switch(c)
        {
        case 13:
            return ACS_BOARD;
        case 14:
            return ACS_BLOCK;
        default:
            return (chtype) ' ';
        }
    case 6:
        switch(c)
        {
        case 13:
            return ACS_BOARD;
        default:
            return ACS_BLOCK;
        }
    case 7:
        return (chtype) (asciiborder[c]);
    default:
        switch (c)
        {
        case 0:
            return ACS_ULCORNER;
        case 1:
        case 6:
            return ACS_HLINE;
        case 2:
            return ACS_URCORNER;
        case 3:
        case 4:
            return ACS_VLINE;
        case 5:
            return ACS_LLCORNER;
        case 7:
            return ACS_LRCORNER;
        case 8:
            return ACS_PLUS;
        case 9:
            return ACS_LTEE;
        case 10:
            return ACS_RTEE;
        case 11:
            return ACS_TTEE;
        case 12:
            return ACS_BTEE;
        case 13:
            return ACS_BOARD;
        default:
            return ACS_BLOCK;
        }
    }
}

#endif

#endif


//  ------------------------------------------------------------------

//  Available on every platform: vputs_box() needs it wherever the
//  screen is not driven through curses.

//  ------------------------------------------------------------------
//  Box character substitution.
//
//  Text coming from help files, templates and the colour setup is
//  written with the CP437 line-drawing characters. On a terminal those
//  have to be replaced by whatever the current box type draws with -
//  the ACS set, or the real Unicode characters on a wide build.
//
//  A character may arrive either as its CP437 byte (8-bit mode) or as
//  the codepoint it stands for (UTF-8 mode), so both are recognised.

static const struct
{
    uint8_t  dos;       // the CP437 byte
    uint16_t uni;       // the same character as a codepoint
    uint8_t  type;      // which box type it belongs to
    uint8_t  slot;      // and which position in it
}
gvid_boxmap[] =
{
    { 0xDA, 0x250C, 0,  0 },
    { 0xC4, 0x2500, 0,  1 },
    { 0xBF, 0x2510, 0,  2 },
    { 0xB3, 0x2502, 0,  4 },
    { 0xC0, 0x2514, 0,  5 },
    { 0xD9, 0x2518, 0,  7 },
    { 0xC5, 0x253C, 0,  8 },
    { 0xC3, 0x251C, 0,  9 },
    { 0xB4, 0x2524, 0, 10 },
    { 0xC2, 0x252C, 0, 11 },
    { 0xC1, 0x2534, 0, 12 },
    { 0xC9, 0x2554, 1,  0 },
    { 0xCD, 0x2550, 1,  1 },
    { 0xBB, 0x2557, 1,  2 },
    { 0xBA, 0x2551, 1,  4 },
    { 0xC8, 0x255A, 1,  5 },
    { 0xBC, 0x255D, 1,  7 },
    { 0xCE, 0x256C, 1,  8 },
    { 0xCC, 0x2560, 1,  9 },
    { 0xB9, 0x2563, 1, 10 },
    { 0xCB, 0x2566, 1, 11 },
    { 0xCA, 0x2569, 1, 12 },
};


vchar gvid_boxcvtc(vchar c)
{
    for(size_t n = 0; n < ARRAYSIZE(gvid_boxmap); n++)
    {
        if(c == gvid_boxmap[n].dos or c == gvid_boxmap[n].uni)
            return _box_table(gvid_boxmap[n].type, gvid_boxmap[n].slot);
    }
    return c;
}


void gvid_boxcvt(char* s)
{
    //  Only meaningful byte-for-byte, so it stays an 8-bit operation;
    //  a substitution that widened the string would overrun the buffer
    //  the caller handed us. Strings that may hold multibyte characters
    //  go through gvid_addstr(), which converts as it draws.
    for(; *s; s++)
    {
        vchar c = gvid_boxcvtc((vchar)(unsigned char)*s);
        if(c < 0x100)
            *s = (char)c;
    }
}




//  ------------------------------------------------------------------
//  Draws a text box on the screen

void vbox(int srow, int scol, int erow, int ecol, int box, vattr hiattr, vattr loattr)
{
    if (loattr == DEFATTR)
        loattr = hiattr;
    else if(loattr == -2)
        loattr = (int)((hiattr & 0x08) ? (hiattr & 0xF7) : (hiattr | 0x08));

#if defined(__UNIX__)
    hiattr |= ACSET;
    loattr |= ACSET;
#endif

    vputc(srow,   scol,   hiattr, _box_table(box, 0));               // Top left corner
    vputx(srow,   scol+1, hiattr, _box_table(box, 1), ecol-scol-1);  // Top border
    vputc(srow,   ecol,   loattr, _box_table(box, 2));               // Top right corner
    vputy(srow+1, scol,   hiattr, _box_table(box, 3), erow-srow-1);  // Left border
    vputy(srow+1, ecol,   loattr, _box_table(box, 4), erow-srow-1);  // Right border
    vputc(erow,   scol,   hiattr, _box_table(box, 5));               // Bottom left corner
    vputx(erow,   scol+1, loattr, _box_table(box, 6), ecol-scol-1);  // Bottom border
    vputc(erow,   ecol,   loattr, _box_table(box, 7));               // Bottom right corner
}


//  ------------------------------------------------------------------
//  Fills an area of screen with a character & attribute

void vfill(int srow, int scol, int erow, int ecol, vchar chr, vattr atr)
{

    int width = ecol-scol+1;
    for(int crow=srow; crow<=erow; crow++)
        vputx(crow, scol, atr, chr, width);
}


//  ------------------------------------------------------------------
