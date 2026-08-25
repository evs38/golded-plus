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
#include <gctype.h>
#include <grecode.h>
#include <gutf8.h>

//  ------------------------------------------------------------------

#ifdef GOLD_UTF8

bool __gutf8_mode = false;

void g_set_utf8_mode(bool onoff)
{
    __gutf8_mode = onoff;
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

static const cprange zero_width[] =
{
    { 0x0300, 0x036F }, { 0x0483, 0x0489 }, { 0x0591, 0x05BD },
    { 0x05BF, 0x05BF }, { 0x05C1, 0x05C2 }, { 0x05C4, 0x05C5 },
    { 0x05C7, 0x05C7 }, { 0x0610, 0x061A }, { 0x064B, 0x065F },
    { 0x0670, 0x0670 }, { 0x06D6, 0x06DC }, { 0x06DF, 0x06E4 },
    { 0x06E7, 0x06E8 }, { 0x06EA, 0x06ED }, { 0x0711, 0x0711 },
    { 0x0730, 0x074A }, { 0x07A6, 0x07B0 }, { 0x07EB, 0x07F3 },
    { 0x0816, 0x0819 }, { 0x081B, 0x0823 }, { 0x0825, 0x0827 },
    { 0x0829, 0x082D }, { 0x0859, 0x085B }, { 0x08E3, 0x0903 },
    { 0x093A, 0x093C }, { 0x093E, 0x094F }, { 0x0951, 0x0957 },
    { 0x0962, 0x0963 }, { 0x0981, 0x0983 }, { 0x09BC, 0x09BC },
    { 0x09BE, 0x09C4 }, { 0x09C7, 0x09C8 }, { 0x09CB, 0x09CD },
    { 0x09D7, 0x09D7 }, { 0x09E2, 0x09E3 }, { 0x0A01, 0x0A03 },
    { 0x0A3C, 0x0A3C }, { 0x0A3E, 0x0A42 }, { 0x0A47, 0x0A48 },
    { 0x0A4B, 0x0A4D }, { 0x0A51, 0x0A51 }, { 0x0A70, 0x0A71 },
    { 0x0A75, 0x0A75 }, { 0x0A81, 0x0A83 }, { 0x0ABC, 0x0ABC },
    { 0x0ABE, 0x0AC5 }, { 0x0AC7, 0x0AC9 }, { 0x0ACB, 0x0ACD },
    { 0x0AE2, 0x0AE3 }, { 0x0B01, 0x0B03 }, { 0x0B3C, 0x0B3C },
    { 0x0B3E, 0x0B44 }, { 0x0B47, 0x0B48 }, { 0x0B4B, 0x0B4D },
    { 0x0B56, 0x0B57 }, { 0x0B62, 0x0B63 }, { 0x0B82, 0x0B82 },
    { 0x0BBE, 0x0BC2 }, { 0x0BC6, 0x0BC8 }, { 0x0BCA, 0x0BCD },
    { 0x0BD7, 0x0BD7 }, { 0x0C00, 0x0C03 }, { 0x0C3E, 0x0C44 },
    { 0x0C46, 0x0C48 }, { 0x0C4A, 0x0C4D }, { 0x0C55, 0x0C56 },
    { 0x0C62, 0x0C63 }, { 0x0C81, 0x0C83 }, { 0x0CBC, 0x0CBC },
    { 0x0CBE, 0x0CC4 }, { 0x0CC6, 0x0CC8 }, { 0x0CCA, 0x0CCD },
    { 0x0CD5, 0x0CD6 }, { 0x0CE2, 0x0CE3 }, { 0x0D01, 0x0D03 },
    { 0x0D3E, 0x0D44 }, { 0x0D46, 0x0D48 }, { 0x0D4A, 0x0D4D },
    { 0x0D57, 0x0D57 }, { 0x0D62, 0x0D63 }, { 0x0D82, 0x0D83 },
    { 0x0DCA, 0x0DCA }, { 0x0DCF, 0x0DD4 }, { 0x0DD6, 0x0DD6 },
    { 0x0DD8, 0x0DDF }, { 0x0DF2, 0x0DF3 }, { 0x0E31, 0x0E31 },
    { 0x0E34, 0x0E3A }, { 0x0E47, 0x0E4E }, { 0x0EB1, 0x0EB1 },
    { 0x0EB4, 0x0EB9 }, { 0x0EBB, 0x0EBC }, { 0x0EC8, 0x0ECD },
    { 0x0F18, 0x0F19 }, { 0x0F35, 0x0F35 }, { 0x0F37, 0x0F37 },
    { 0x0F39, 0x0F39 }, { 0x0F3E, 0x0F3F }, { 0x0F71, 0x0F84 },
    { 0x0F86, 0x0F87 }, { 0x0F8D, 0x0F97 }, { 0x0F99, 0x0FBC },
    { 0x0FC6, 0x0FC6 }, { 0x102B, 0x103E }, { 0x1056, 0x1059 },
    { 0x105E, 0x1060 }, { 0x1062, 0x1064 }, { 0x1067, 0x106D },
    { 0x1071, 0x1074 }, { 0x1082, 0x108D }, { 0x108F, 0x108F },
    { 0x109A, 0x109D }, { 0x1160, 0x11FF }, { 0x135D, 0x135F },
    { 0x1712, 0x1714 }, { 0x1732, 0x1734 }, { 0x1752, 0x1753 },
    { 0x1772, 0x1773 }, { 0x17B4, 0x17D3 }, { 0x17DD, 0x17DD },
    { 0x180B, 0x180E }, { 0x18A9, 0x18A9 }, { 0x1920, 0x192B },
    { 0x1930, 0x193B }, { 0x1A17, 0x1A1B }, { 0x1A55, 0x1A5E },
    { 0x1A60, 0x1A7C }, { 0x1A7F, 0x1A7F }, { 0x1AB0, 0x1ABE },
    { 0x1B00, 0x1B04 }, { 0x1B34, 0x1B44 }, { 0x1B6B, 0x1B73 },
    { 0x1B80, 0x1B82 }, { 0x1BA1, 0x1BAD }, { 0x1BE6, 0x1BF3 },
    { 0x1C24, 0x1C37 }, { 0x1CD0, 0x1CD2 }, { 0x1CD4, 0x1CE8 },
    { 0x1CED, 0x1CED }, { 0x1CF2, 0x1CF4 }, { 0x1CF8, 0x1CF9 },
    { 0x1DC0, 0x1DFF }, { 0x200B, 0x200F }, { 0x202A, 0x202E },
    { 0x2060, 0x2064 }, { 0x2066, 0x206F }, { 0x20D0, 0x20F0 },
    { 0x2CEF, 0x2CF1 }, { 0x2D7F, 0x2D7F }, { 0x2DE0, 0x2DFF },
    { 0x302A, 0x302F }, { 0x3099, 0x309A }, { 0xA66F, 0xA672 },
    { 0xA674, 0xA67D }, { 0xA69E, 0xA69F }, { 0xA6F0, 0xA6F1 },
    { 0xA802, 0xA802 }, { 0xA806, 0xA806 }, { 0xA80B, 0xA80B },
    { 0xA823, 0xA827 }, { 0xA880, 0xA881 }, { 0xA8B4, 0xA8C4 },
    { 0xA8E0, 0xA8F1 }, { 0xA926, 0xA92D }, { 0xA947, 0xA953 },
    { 0xA980, 0xA983 }, { 0xA9B3, 0xA9C0 }, { 0xA9E5, 0xA9E5 },
    { 0xAA29, 0xAA36 }, { 0xAA43, 0xAA43 }, { 0xAA4C, 0xAA4D },
    { 0xAA7B, 0xAA7D }, { 0xAAB0, 0xAAB0 }, { 0xAAB2, 0xAAB4 },
    { 0xAAB7, 0xAAB8 }, { 0xAABE, 0xAABF }, { 0xAAC1, 0xAAC1 },
    { 0xAAEB, 0xAAEF }, { 0xAAF5, 0xAAF6 }, { 0xABE3, 0xABEA },
    { 0xABEC, 0xABED }, { 0xFB1E, 0xFB1E }, { 0xFE00, 0xFE0F },
    { 0xFE20, 0xFE2F }, { 0xFEFF, 0xFEFF }, { 0xFFF9, 0xFFFB },
    { 0x101FD, 0x101FD }, { 0x102E0, 0x102E0 }, { 0x10376, 0x1037A },
    { 0x10A01, 0x10A0F }, { 0x10A38, 0x10A3F }, { 0x10AE5, 0x10AE6 },
    { 0x11000, 0x11002 }, { 0x11038, 0x11046 }, { 0x1107F, 0x11082 },
    { 0x110B0, 0x110BA }, { 0x11100, 0x11102 }, { 0x11127, 0x11134 },
    { 0x11173, 0x11173 }, { 0x11180, 0x11182 }, { 0x111B3, 0x111C0 },
    { 0x1122C, 0x11237 }, { 0x112DF, 0x112EA }, { 0x11301, 0x11303 },
    { 0x1133C, 0x1133C }, { 0x1133E, 0x1134D }, { 0x11357, 0x11357 },
    { 0x11362, 0x11374 }, { 0x114B0, 0x114C3 }, { 0x115AF, 0x115C0 },
    { 0x11630, 0x11640 }, { 0x116AB, 0x116B7 }, { 0x1171D, 0x1172B },
    { 0x16AF0, 0x16AF4 }, { 0x16B30, 0x16B36 }, { 0x16F51, 0x16F92 },
    { 0x1BC9D, 0x1BC9E }, { 0x1BCA0, 0x1BCA3 }, { 0x1D165, 0x1D169 },
    { 0x1D16D, 0x1D182 }, { 0x1D185, 0x1D18B }, { 0x1D1AA, 0x1D1AD },
    { 0x1D242, 0x1D244 }, { 0x1DA00, 0x1DA36 }, { 0x1DA3B, 0x1DA6C },
    { 0x1DA75, 0x1DA75 }, { 0x1DA84, 0x1DA84 }, { 0x1DA9B, 0x1DAAF },
    { 0x1E8D0, 0x1E8D6 }, { 0xE0001, 0xE0001 }, { 0xE0020, 0xE007F },
    { 0xE0100, 0xE01EF },
};

static const cprange double_width[] =
{
    { 0x1100, 0x115F }, { 0x2329, 0x232A }, { 0x2E80, 0x303E },
    { 0x3041, 0x33FF }, { 0x3400, 0x4DBF }, { 0x4E00, 0x9FFF },
    { 0xA000, 0xA4CF }, { 0xA960, 0xA97F }, { 0xAC00, 0xD7A3 },
    { 0xF900, 0xFAFF }, { 0xFE10, 0xFE19 }, { 0xFE30, 0xFE6F },
    { 0xFF00, 0xFF60 }, { 0xFFE0, 0xFFE6 },
    { 0x16FE0, 0x16FE4 }, { 0x17000, 0x187F7 }, { 0x18800, 0x18CD5 },
    { 0x1B000, 0x1B2FB }, { 0x1F004, 0x1F004 }, { 0x1F0CF, 0x1F0CF },
    { 0x1F18E, 0x1F18E }, { 0x1F191, 0x1F19A }, { 0x1F200, 0x1F320 },
    { 0x1F32D, 0x1F335 }, { 0x1F337, 0x1F37C }, { 0x1F37E, 0x1F393 },
    { 0x1F3A0, 0x1F3CA }, { 0x1F3CF, 0x1F3D3 }, { 0x1F3E0, 0x1F3F0 },
    { 0x1F3F4, 0x1F3F4 }, { 0x1F3F8, 0x1F43E }, { 0x1F440, 0x1F440 },
    { 0x1F442, 0x1F4FC }, { 0x1F4FF, 0x1F53D }, { 0x1F54B, 0x1F54E },
    { 0x1F550, 0x1F567 }, { 0x1F57A, 0x1F57A }, { 0x1F595, 0x1F596 },
    { 0x1F5A4, 0x1F5A4 }, { 0x1F5FB, 0x1F64F }, { 0x1F680, 0x1F6C5 },
    { 0x1F6CC, 0x1F6CC }, { 0x1F6D0, 0x1F6D2 }, { 0x1F6EB, 0x1F6EC },
    { 0x1F6F4, 0x1F6F9 }, { 0x1F910, 0x1F93E }, { 0x1F940, 0x1F970 },
    { 0x1F973, 0x1F976 }, { 0x1F97A, 0x1F9A2 }, { 0x1F9B0, 0x1F9B9 },
    { 0x1F9C0, 0x1F9C2 }, { 0x1F9D0, 0x1F9FF }, { 0x20000, 0x2FFFD },
    { 0x30000, 0x3FFFD },
};


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
        return 2;
    return 1;
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
        int used = 1;
        w += g_cp_width(g_utf8_decode(p, end, &used));
        p += used ? used : 1;
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

    const char* s = p;
    size_t cur = 0;
    while(*s)
    {
        int used = 1;
        int w = g_cp_width(g_utf8_decode(s, &used));
        if(cur + (size_t)w > maxcols)
            break;
        cur += w;
        s += used ? used : 1;
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
