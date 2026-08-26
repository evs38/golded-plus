//  This may look like C code, but it is really -*- C++ -*-

//  ------------------------------------------------------------------
//  The Goldware Library
//  Copyright (C) 1990-1999 Odinn Sorensen
//  Copyright (C) 1999-2000 Alexander S. Aganichev
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
//  UTF-8 primitives.
//  ------------------------------------------------------------------

#include <cstring>
#if defined(__USE_WIDE_NCURSES__)
//  For wcwidth(): where curses draws, curses decides how wide a
//  character is - see g_cp_width().
#include <wchar.h>
#endif
#include <gctype.h>
#include <grecode.h>
#include <gutf8.h>

//  ------------------------------------------------------------------

#ifdef GOLD_UTF8

bool __gutf8_mode = false;

void g_set_utf8_mode(bool onoff)
{
    __gutf8_mode = onoff;

    //  The charset is settled around the same time as the locale, and
    //  what the host's wcwidth() can do depends on the locale.
    g_reset_host_width();
}

#endif


//  ------------------------------------------------------------------

int g_utf8_seqlen(unsigned char lead)
{
    if(lead < 0x80)
        return 1;
    if(lead < 0xC2)         // continuation byte, or an overlong lead
        return 0;
    if(lead < 0xE0)
        return 2;
    if(lead < 0xF0)
        return 3;
    if(lead < 0xF5)
        return 4;
    return 0;               // F5..FF can never start a sequence
}


//  ------------------------------------------------------------------

uint32_t g_utf8_decode_raw(const char* p, const char* end, int* used, bool* ok)
{
    if(used)
        *used = 1;
    if(ok)
        *ok = false;

    if(p == NULL or p == end or *p == NUL)
    {
        if(used)
            *used = 0;
        return 0;
    }

    unsigned char lead = (unsigned char)*p;

    if(lead < 0x80)
    {
        if(ok)
            *ok = true;
        return lead;
    }

    int len = g_utf8_seqlen(lead);
    if(len < 2)
        return GUTF8_REPLACEMENT;

    //  Make sure the whole sequence is there and is well formed before
    //  committing to it; a truncated one must consume a single byte only,
    //  or a partially written line would swallow the text after it.

    static const uint32_t leadmask[5] = { 0, 0, 0x1F, 0x0F, 0x07 };
    uint32_t cp = lead & leadmask[len];

    for(int n = 1; n < len; n++)
    {
        if(end and (p + n) >= end)
            return GUTF8_REPLACEMENT;
        unsigned char c = (unsigned char)p[n];
        if((c & 0xC0) != 0x80)
            return GUTF8_REPLACEMENT;
        cp = (cp << 6) | (c & 0x3F);
    }

    //  Reject overlong forms, UTF-16 surrogates and out-of-range values.
    static const uint32_t minimum[5] = { 0, 0, 0x80, 0x800, 0x10000 };
    if(cp < minimum[len] or cp > 0x10FFFF or (cp >= 0xD800 and cp <= 0xDFFF))
        return GUTF8_REPLACEMENT;

    if(used)
        *used = len;
    if(ok)
        *ok = true;
    return cp;
}


uint32_t g_utf8_decode(const char* p, const char* end, int* used)
{
    //  In 8-bit mode the byte is returned as-is; turning it into a
    //  codepoint is the recoder's job, not this layer's.
    if(not g_utf8_mode())
    {
        if(used)
            *used = 1;
        if(p == NULL or p == end or *p == NUL)
        {
            if(used)
                *used = 0;
            return 0;
        }
        return (unsigned char)*p;
    }

    return g_utf8_decode_raw(p, end, used);
}


uint32_t g_utf8_decode(const char* p, int* used)
{
    return g_utf8_decode(p, (const char*)NULL, used);
}


//  ------------------------------------------------------------------

int g_utf8_len_raw(uint32_t cp)
{
    if(cp < 0x80)
        return 1;
    if(cp < 0x800)
        return 2;
    if(cp < 0x10000)
        return 3;
    return 4;
}


int g_utf8_encode_raw(uint32_t cp, char* out)
{
    if(cp > 0x10FFFF or (cp >= 0xD800 and cp <= 0xDFFF))
        cp = GUTF8_REPLACEMENT;

    if(cp < 0x80)
    {
        out[0] = (char)cp;
        return 1;
    }
    if(cp < 0x800)
    {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if(cp < 0x10000)
    {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}


int g_utf8_encode(uint32_t cp, char* out)
{
    //  A codepoint above 0xFF has no single-byte form; the recoder is
    //  what turns one into the local charset, so all this layer can do
    //  is say that it could not.
    if(not g_utf8_mode())
    {
        *out = (char)(cp < 0x100 ? cp : '?');
        return 1;
    }

    return g_utf8_encode_raw(cp, out);
}


std::string g_utf8_encode(uint32_t cp)
{
    char buf[GUTF8_MAXLEN];
    int len = g_utf8_encode(cp, buf);
    return std::string(buf, len);
}


//  ------------------------------------------------------------------

const char* g_utf8_next(const char* p)
{
    if(p == NULL or *p == NUL)
        return p;

    if(not g_utf8_mode())
        return p + 1;

    int len = g_utf8_seqlen((unsigned char)*p);
    if(len < 2)
        return p + 1;

    //  Stop at the terminator even if the sequence claims to be longer,
    //  so a truncated tail cannot take us past the end of the buffer.
    for(int n = 1; n < len; n++)
    {
        if(p[n] == NUL or ((unsigned char)p[n] & 0xC0) != 0x80)
            return p + n;
    }
    return p + len;
}


const char* g_utf8_prev(const char* start, const char* p)
{
    if(p <= start)
        return start;

    p--;
    if(not g_utf8_mode())
        return p;

    //  Back up over continuation bytes, but never more than the longest
    //  possible sequence: a run of stray 0x80 bytes must not send us
    //  scanning to the front of the line.
    int n = 0;
    while(p > start and ((unsigned char)*p & 0xC0) == 0x80 and n < GUTF8_MAXLEN - 1)
    {
        p--;
        n++;
    }
    return p;
}


//  ------------------------------------------------------------------

bool g_utf8_is_continuation(const char* p)
{
    return g_utf8_mode() and p and (((unsigned char)*p & 0xC0) == 0x80);
}


//  ------------------------------------------------------------------

size_t g_utf8_strlen(const char* p)
{
    if(p == NULL)
        return 0;
    if(not g_utf8_mode())
        return strlen(p);

    size_t n = 0;
    while(*p)
    {
        p = g_utf8_next(p);
        n++;
    }
    return n;
}


size_t g_utf8_strlen(const char* p, size_t nbytes)
{
    if(p == NULL)
        return 0;
    if(not g_utf8_mode())
        return nbytes;

    const char* end = p + nbytes;
    size_t n = 0;
    while(p < end and *p)
    {
        p = g_utf8_next(p);
        n++;
    }
    return n;
}


//  ------------------------------------------------------------------
//  Width.
//
//  Zero-width: the combining marks, the format characters (soft hyphen
//  excepted, which we render) and the Hangul conjoining jamo.
//  Double-width: the East Asian Wide and Fullwidth ranges, plus emoji.
//
//  The tables are sorted, so a binary search settles the question in a
//  handful of comparisons.

struct cprange
{
    uint32_t first, last;
};

//  The tables themselves are generated from the Unicode Character
//  Database - see chsgen/uwidgen.cc, and chsgen/README.txt for how to
//  move them to a newer revision of Unicode.

#include "guwidth.inc"


static bool in_ranges(uint32_t cp, const cprange* tab, size_t count)
{
    size_t lo = 0, hi = count;
    while(lo < hi)
    {
        size_t mid = lo + (hi - lo) / 2;
        if(cp < tab[mid].first)
            hi = mid;
        else if(cp > tab[mid].last)
            lo = mid + 1;
        else
            return true;
    }
    return false;
}


#if defined(__USE_WIDE_NCURSES__)

//  Is the host's wcwidth() worth asking?
//
//  Only in a locale it understands. In the C locale it answers -1 to
//  everything above ASCII, and trusting that would flatten every wide
//  character to one column - worse than the tables it was meant to
//  correct. Two characters settle it: an ideograph every wcwidth since
//  the nineties calls two columns, and a combining accent every one of
//  them calls none.

static int host_width_state = -1;       // -1 not asked yet

static bool host_width_usable()
{
    if(host_width_state < 0)
        host_width_state = (wcwidth((wchar_t)0x4E00) == 2 and
                            wcwidth((wchar_t)0x0301) == 0) ? 1 : 0;

    return host_width_state != 0;
}

#endif


void g_reset_host_width(void)
{
#if defined(__USE_WIDE_NCURSES__)
    host_width_state = -1;
#endif
}


int g_cp_width(uint32_t cp)
{
    //  Every byte occupies one cell when we are not decoding UTF-8, and
    //  that includes the control range: GoldED draws control characters
    //  as a visible substitute rather than obeying them.
    if(not g_utf8_mode() or cp < 0x0300)
        return 1;

    if(in_ranges(cp, zero_width, ARRAYSIZE(zero_width)))
        return 0;

    if(in_ranges(cp, double_width, ARRAYSIZE(double_width)))
    {
#if defined(__USE_WIDE_NCURSES__)
        //  Our tables follow the current Unicode, which is sometimes
        //  ahead of the host. Where curses does the drawing it advances
        //  the cursor by its own wcwidth(), and for a character it has
        //  never heard of that is one column - measured on macOS with
        //  U+1FABE. Reporting two there would leave the rest of the
        //  line a column out. Being right where the renderer is wrong
        //  is worse than agreeing with it.
        if(host_width_usable() and wcwidth((wchar_t)cp) < 0)
            return 1;
#endif
        return 2;
    }

    return 1;
}


//  ------------------------------------------------------------------
//  Grapheme cluster breaking, after UAX #29.
//
//  A cluster is what a reader calls one character and what a terminal
//  draws in one place: a letter with its accents, an emoji with its
//  skin tone, a flag made of two regional indicators, a family joined
//  by zero-width joiners. Measuring by codepoint counts each piece
//  separately, which is how a thumb with a skin tone came to be four
//  columns wide when the terminal draws it in two.

static gcbclass gcb_class(uint32_t cp)
{
    //  Hangul syllables are not in the table. The data file spells out
    //  all 11172 of them one at a time, and one division says the same
    //  thing: a syllable that divides evenly has no trailing consonant.
    if(cp >= 0xAC00 and cp <= 0xD7A3)
        return ((cp - 0xAC00) % 28) == 0 ? GCB_LV : GCB_LVT;

    size_t lo = 0, hi = ARRAYSIZE(gcb_classes);
    while(lo < hi)
    {
        size_t mid = lo + (hi - lo) / 2;
        if(cp < gcb_classes[mid].first)
            hi = mid;
        else if(cp > gcb_classes[mid].last)
            lo = mid + 1;
        else
            return gcb_classes[mid].cls;
    }

    return GCB_Other;
}


//  ------------------------------------------------------------------
//  Step over one cluster. Never walks past 'end', and always advances
//  by at least one byte so a caller cannot be caught in a loop.

const char* g_utf8_cluster_next(const char* p, const char* end)
{
    if(p == NULL or p >= end or *p == NUL)
        return p;

    int used = 1;
    gcbclass prev = gcb_class(g_utf8_decode(p, end, &used));
    const char* s = p + (used ? used : 1);

    //  GB3 to GB5. A line ending is a cluster of its own, and CR LF is
    //  one cluster rather than two.
    if(prev == GCB_CR)
    {
        if(s < end and *s)
        {
            int u = 1;
            if(gcb_class(g_utf8_decode(s, end, &u)) == GCB_LF)
                s += u ? u : 1;
        }
        return s;
    }
    if(prev == GCB_LF or prev == GCB_Control)
        return s;

    //  Two rules need to remember more than the previous character:
    //  GB11 joins pictographs across a zero-width joiner, and GB12/GB13
    //  join regional indicators in pairs and not in threes.
    bool pict = (prev == GCB_ExtPict);
    int  ri   = (prev == GCB_Regional_Indicator) ? 1 : 0;

    while(s < end and *s)
    {
        int u = 1;
        gcbclass cur = gcb_class(g_utf8_decode(s, end, &u));

        bool join;

        if(cur == GCB_Control or cur == GCB_CR or cur == GCB_LF)
            join = false;                                       // GB5
        else if(cur == GCB_Extend or cur == GCB_ZWJ)
            join = true;                                        // GB9
        else if(cur == GCB_SpacingMark)
            join = true;                                        // GB9a
        else if(prev == GCB_Prepend)
            join = true;                                        // GB9b
        else if(prev == GCB_ZWJ and pict and cur == GCB_ExtPict)
            join = true;                                        // GB11
        else if(prev == GCB_L and (cur == GCB_L or cur == GCB_V or
                                   cur == GCB_LV or cur == GCB_LVT))
            join = true;                                        // GB6
        else if((prev == GCB_LV or prev == GCB_V) and
                (cur == GCB_V or cur == GCB_T))
            join = true;                                        // GB7
        else if((prev == GCB_LVT or prev == GCB_T) and cur == GCB_T)
            join = true;                                        // GB8
        else if(prev == GCB_Regional_Indicator and
                cur == GCB_Regional_Indicator and (ri % 2))
            join = true;                                        // GB12, GB13
        else
            join = false;                                       // GB999

        if(not join)
            break;

        //  GB11 only holds while everything since the pictograph has
        //  been a mark or the joiner itself.
        if(cur == GCB_ExtPict)
            pict = true;
        else if(cur != GCB_Extend and cur != GCB_ZWJ)
            pict = false;

        ri = (cur == GCB_Regional_Indicator) ? ri + 1 : 0;

        prev = cur;
        s += u ? u : 1;
    }

    return s;
}


const char* g_utf8_cluster_next(const char* p)
{
    if(p == NULL)
        return p;
    return g_utf8_cluster_next(p, p + strlen(p));
}


//  ------------------------------------------------------------------
//  Step back over one cluster.
//
//  Clusters are defined forwards only, so this finds the last boundary
//  before 'p' by walking from the start of the line. That is a whole
//  line per keystroke, which is nothing next to redrawing it, and it
//  avoids having to reason about the rules in reverse.

const char* g_utf8_cluster_prev(const char* start, const char* p)
{
    if(start == NULL or p == NULL or p <= start)
        return start;

    const char* s    = start;
    const char* last = start;

    while(s < p and *s)
    {
        const char* nxt = g_utf8_cluster_next(s, p);
        if(nxt <= s)
            break;
        last = s;
        s = nxt;
    }

    return last;
}


//  ------------------------------------------------------------------
//  How wide one cluster is: the width of the character it is built
//  around, since everything attached to it is drawn on top of or
//  inside that. The one thing that changes the answer is an emoji
//  presentation selector, which turns a text symbol into a picture,
//  and a picture always takes two columns.

size_t g_utf8_cluster_width(const char* p, const char* end)
{
    if(p == NULL or p >= end)
        return 0;

    int used = 1;
    size_t w = (size_t)g_cp_width(g_utf8_decode(p, end, &used));

    const char* s = p + (used ? used : 1);
    while(s < end and *s)
    {
        int u = 1;
        if(g_utf8_decode(s, end, &u) == 0xFE0F)
            return 2;
        s += u ? u : 1;
    }

    return w;
}


//  ------------------------------------------------------------------

size_t g_utf8_width(const char* p, size_t nbytes)
{
    if(p == NULL)
        return 0;
    if(not g_utf8_mode())
        return nbytes;

    const char* end = p + nbytes;
    size_t w = 0;
    while(p < end and *p)
    {
        const char* nxt = g_utf8_cluster_next(p, end);
        if(nxt <= p)
            break;
        w += g_utf8_cluster_width(p, nxt);
        p = nxt;
    }
    return w;
}


size_t g_utf8_width(const char* p)
{
    if(p == NULL)
        return 0;
    if(not g_utf8_mode())
        return strlen(p);
    return g_utf8_width(p, strlen(p));
}


size_t g_utf8_width(const std::string& s)
{
    return g_utf8_width(s.data(), s.length());
}


//  ------------------------------------------------------------------

size_t g_utf8_offset_at_col(const char* p, size_t col)
{
    //  The same walk as g_utf8_bytes_for_cols(): stop at the character
    //  the column falls in, so a column landing on the trailing half of
    //  a wide one belongs to that character and not to the next.
    return g_utf8_bytes_for_cols(p, col);
}


size_t g_utf8_col_at_offset(const char* p, size_t offset)
{
    if(p == NULL)
        return 0;
    if(not g_utf8_mode())
        return offset;
    return g_utf8_width(p, offset);
}


//  ------------------------------------------------------------------

size_t g_utf8_offset_at_char(const char* p, size_t n)
{
    if(p == NULL)
        return 0;
    if(not g_utf8_mode())
    {
        size_t len = strlen(p);
        return n < len ? n : len;
    }

    const char* s = p;
    while(n-- and *s)
        s = g_utf8_next(s);
    return (size_t)(s - p);
}


//  ------------------------------------------------------------------

size_t g_utf8_bytes_for_cols(const char* p, size_t maxcols)
{
    if(p == NULL)
        return 0;
    if(not g_utf8_mode())
    {
        size_t len = strlen(p);
        return maxcols < len ? maxcols : len;
    }

    const char* s   = p;
    const char* end = p + strlen(p);
    size_t cur = 0;
    while(s < end and *s)
    {
        const char* nxt = g_utf8_cluster_next(s, end);
        if(nxt <= s)
            break;
        size_t w = g_utf8_cluster_width(s, nxt);
        if(cur + w > maxcols)
            break;
        cur += w;
        s = nxt;
    }
    return (size_t)(s - p);
}


std::string g_utf8_truncate(const std::string& s, size_t maxcols)
{
    return s.substr(0, g_utf8_bytes_for_cols(s.c_str(), maxcols));
}


std::string g_utf8_fit(const char* p, size_t cols)
{
    if(cols == 0)
        return std::string();

    if(p == NULL)
        return std::string(cols, ' ');

    size_t bytes = g_utf8_bytes_for_cols(p, cols);
    size_t width = g_utf8_width(p, bytes);

    std::string result(p, bytes);
    if(width < cols)
        result.append(cols - width, ' ');

    return result;
}


//  ------------------------------------------------------------------

bool g_utf8_valid(const char* p, size_t nbytes)
{
    if(p == NULL)
        return true;

    const char* end = p + nbytes;
    while(p < end)
    {
        //  A NUL inside the range is a byte like any other here - the
        //  caller gave a length, so it did not mean it as a terminator.
        if(*p == NUL)
        {
            p++;
            continue;
        }

        int  used = 0;
        bool ok   = false;
        g_utf8_decode_raw(p, end, &used, &ok);
        if(not ok)
            return false;
        p += used;
    }
    return true;
}


bool g_utf8_valid(const char* p)
{
    return p ? g_utf8_valid(p, strlen(p)) : true;
}


//  ------------------------------------------------------------------
//  Case folding for matching.

uint32_t g_cp_fold(uint32_t cp)
{
    //  Folding must not change how many bytes the character takes, or
    //  the skip table gbmh builds from the pattern stops lining up.
    uint32_t up = g_cp_toupper(cp);
    return (g_utf8_len_raw(up) == g_utf8_len_raw(cp)) ? up : cp;
}


std::string g_utf8_fold(const char* p)
{
    if(p == NULL)
        return std::string();

    if(not g_utf8_mode())
    {
        //  A single-byte charset folds through what its bytes actually
        //  mean, not through the C library's idea of case: the process
        //  locale need not describe the charset GoldED is holding text
        //  in, and on a UTF-8 console it usually does not.
        std::string result(p);
        for(size_t n = 0; n < result.length(); n++)
        {
            unsigned char b = (unsigned char)result[n];
            if(b < 0x80)
            {
                result[n] = (char)g_toupper(b);
                continue;
            }
            int folded = g_unicode_to_local(g_cp_toupper(g_local_to_unicode(b)));
            if(folded >= 0)
                result[n] = (char)folded;
        }
        return result;
    }

    std::string result;
    result.reserve(strlen(p));

    while(*p)
    {
        int used = 1;
        uint32_t cp = g_utf8_decode(p, &used);
        if(used == 0)
            break;

        uint32_t folded = g_cp_fold(cp);

        if(folded == cp)
            result.append(p, used);     // also keeps ill-formed bytes as they are
        else
        {
            char buf[GUTF8_MAXLEN];
            int len = g_utf8_encode(folded, buf);
            result.append(buf, len);
        }

        p += used;
    }

    return result;
}


std::string g_utf8_fold(const std::string& s)
{
    return g_utf8_fold(s.c_str());
}


//  ------------------------------------------------------------------

std::string g_utf8_sanitize(const std::string& s)
{
    if(not g_utf8_mode() or g_utf8_valid(s.data(), s.length()))
        return s;

    std::string result;
    result.reserve(s.length());

    const char* p = s.data();
    const char* end = p + s.length();
    while(p < end)
    {
        int used = 1;
        uint32_t cp = g_utf8_decode(p, end, &used);
        if(used == 0)
            break;
        if(cp == GUTF8_REPLACEMENT and used == 1)
            result += g_utf8_encode(GUTF8_REPLACEMENT);
        else
            result.append(p, used);
        p += used;
    }
    return result;
}


//  ------------------------------------------------------------------
//  Case conversion.
//
//  The alphabets GoldED's users actually write in: ASCII, Latin-1,
//  Latin Extended-A, Greek and Cyrillic. Each of these is regular enough
//  to fold arithmetically once the handful of exceptions is taken out,
//  which beats carrying a 1400-entry table around on DOS.

uint32_t g_cp_toupper(uint32_t cp)
{
    if(cp < 0x80)
        return (cp >= 'a' and cp <= 'z') ? cp - 0x20 : cp;

    //  Latin-1 Supplement
    if(cp >= 0xE0 and cp <= 0xFE and cp != 0xF7)
        return cp - 0x20;
    if(cp == 0xFF)
        return 0x178;   // small y with diaeresis
    if(cp == 0xB5)
        return 0x39C;   // micro sign -> capital mu

    //  Latin Extended-A. Upper and lower alternate, but the phase of the
    //  alternation flips three times across the block.
    if(cp >= 0x100 and cp <= 0x17F)
    {
        if(cp == 0x131)
            return 'I';         // dotless i
        if(cp == 0x17F)
            return 'S';         // long s
        if(cp == 0x138 or cp == 0x149 or cp == 0x130 or cp == 0x178)
            return cp;          // no uppercase counterpart
        if(cp <= 0x137 or (cp >= 0x14A and cp <= 0x177))
            return (cp & 1) ? cp - 1 : cp;
        return (cp & 1) ? cp : cp - 1;
    }

    //  Greek
    if(cp == 0x3C2)
        return 0x3A3;           // final sigma
    if(cp >= 0x3B1 and cp <= 0x3C9)
        return cp - 0x20;
    if(cp == 0x3AC)
        return 0x386;
    if(cp >= 0x3AD and cp <= 0x3AF)
        return cp - 0x25;
    if(cp == 0x3CC)
        return 0x38C;
    if(cp >= 0x3CD and cp <= 0x3CE)
        return cp - 0x3F;

    //  Cyrillic
    if(cp >= 0x430 and cp <= 0x44F)
        return cp - 0x20;
    if(cp >= 0x450 and cp <= 0x45F)
        return cp - 0x50;
    if((cp >= 0x460 and cp <= 0x481) or (cp >= 0x48A and cp <= 0x4BF) or
       (cp >= 0x4D0 and cp <= 0x52F))
        return (cp & 1) ? cp - 1 : cp;
    if(cp >= 0x4C1 and cp <= 0x4CE)
        return (cp & 1) ? cp : cp - 1;

    return cp;
}


uint32_t g_cp_tolower(uint32_t cp)
{
    if(cp < 0x80)
        return (cp >= 'A' and cp <= 'Z') ? cp + 0x20 : cp;

    //  Latin-1 Supplement
    if(cp >= 0xC0 and cp <= 0xDE and cp != 0xD7)
        return cp + 0x20;

    //  Latin Extended-A
    if(cp >= 0x100 and cp <= 0x17F)
    {
        if(cp == 0x130)
            return 'i';         // capital I with dot above
        if(cp == 0x178)
            return 0xFF;
        if(cp == 0x131 or cp == 0x138 or cp == 0x149 or cp == 0x17F)
            return cp;
        if(cp <= 0x137 or (cp >= 0x14A and cp <= 0x177))
            return (cp & 1) ? cp : cp + 1;
        return (cp & 1) ? cp + 1 : cp;
    }

    //  Greek
    if(cp >= 0x391 and cp <= 0x3A9)
        return cp + 0x20;
    if(cp == 0x386)
        return 0x3AC;
    if(cp >= 0x388 and cp <= 0x38A)
        return cp + 0x25;
    if(cp == 0x38C)
        return 0x3CC;
    if(cp >= 0x38E and cp <= 0x38F)
        return cp + 0x3F;

    //  Cyrillic
    if(cp >= 0x410 and cp <= 0x42F)
        return cp + 0x20;
    if(cp >= 0x400 and cp <= 0x40F)
        return cp + 0x50;
    if((cp >= 0x460 and cp <= 0x481) or (cp >= 0x48A and cp <= 0x4BF) or
       (cp >= 0x4D0 and cp <= 0x52F))
        return (cp & 1) ? cp : cp + 1;
    if(cp >= 0x4C1 and cp <= 0x4CE)
        return (cp & 1) ? cp + 1 : cp;

    return cp;
}


//  ------------------------------------------------------------------

bool g_cp_isalpha(uint32_t cp)
{
    if(cp < 0x80)
        return (cp >= 'A' and cp <= 'Z') or (cp >= 'a' and cp <= 'z');

    if(cp < 0xC0)
        return cp == 0xAA or cp == 0xB5 or cp == 0xBA;
    if(cp <= 0x24F)
        return cp != 0xD7 and cp != 0xF7;
    if(cp >= 0x370 and cp <= 0x3FF)     // Greek
        return cp != 0x374 and cp != 0x375 and cp != 0x37E and cp != 0x384
               and cp != 0x385 and cp != 0x387;
    if(cp >= 0x400 and cp <= 0x52F)     // Cyrillic
        return true;
    if(cp >= 0x531 and cp <= 0x58F)     // Armenian
        return true;
    if(cp >= 0x5D0 and cp <= 0x5EA)     // Hebrew
        return true;
    if(cp >= 0x620 and cp <= 0x64A)     // Arabic
        return true;
    if(cp >= 0x4E00 and cp <= 0x9FFF)   // CJK
        return true;
    if(cp >= 0x3040 and cp <= 0x30FF)   // Kana
        return true;
    if(cp >= 0xAC00 and cp <= 0xD7A3)   // Hangul
        return true;
    return false;
}


bool g_cp_isspace(uint32_t cp)
{
    if(cp == ' ' or (cp >= 0x09 and cp <= 0x0D))
        return true;
    return cp == 0xA0 or cp == 0x1680 or (cp >= 0x2000 and cp <= 0x200A)
           or cp == 0x2028 or cp == 0x2029 or cp == 0x202F or cp == 0x205F
           or cp == 0x3000;
}


bool g_cp_isdigit(uint32_t cp)
{
    return cp >= '0' and cp <= '9';
}


bool g_cp_isalnum(uint32_t cp)
{
    return g_cp_isalpha(cp) or g_cp_isdigit(cp);
}


bool g_cp_ispunct(uint32_t cp)
{
    if(cp < 0x80)
        return cp > 0x20 and cp < 0x7F and not g_cp_isalnum(cp);
    if(cp >= 0x2010 and cp <= 0x205E)
        return true;
    return cp == 0xA1 or cp == 0xAB or cp == 0xB7 or cp == 0xBB or cp == 0xBF;
}


//  ------------------------------------------------------------------
