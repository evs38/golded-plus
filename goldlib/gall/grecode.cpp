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
//  Charset recoding.
//  ------------------------------------------------------------------

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <clocale>
#include <map>
#include <gdefs.h>
#include <gstrall.h>
#include <gutf8.h>
#include <gcharset.h>
#include <vector>
#include <grecode.h>

#ifdef HAS_ICONV
    #include <iconv.h>
    #include <cerrno>

//  ------------------------------------------------------------------
//  iconv()'s input argument is `char **' in POSIX and in the C library
//  on Linux and the BSDs, and `const char **' in GNU libiconv - which
//  is what DJGPP and several package managers install. The preprocessor
//  cannot tell them apart: libiconv bakes the answer into the header it
//  installs, so the same _LIBICONV_VERSION appears with either.
//
//  So let overload resolution answer it. Whichever of the two the
//  declaration asks for, one of these conversions matches and the other
//  is never considered.

class gold_iconv_inbuf
{
public:
    explicit gold_iconv_inbuf(char** p) : __p(p) { }
    operator char**() const
    {
        return __p;
    }
    operator const char**() const
    {
        return const_cast<const char**>(__p);
    }
private:
    char** __p;
};

#endif

#ifdef __WIN32__
    #include <windows.h>
#endif

#ifdef __OS2__
    #define INCL_DOS
    #include <os2.h>
#endif

#ifdef HAS_ULS
    //  OS/2's own Unicode conversion, which every OS/2 compiler can
    //  reach: the headers ship with Watcom, with kLIBC and with EMX,
    //  and the code lives in UCONV.DLL - not LIBUNI.DLL, which has
    //  none of these entry points.
    #include <unidef.h>
    #include <uconv.h>
#endif

#include "gcptabs.inc"

//  ------------------------------------------------------------------
//  Character put in place of anything the destination charset cannot
//  represent. '?' is what iconv itself substitutes, so the two paths
//  agree.

static const char SUBSTITUTE = '?';

extern bool local_unicode_ready;


//  ------------------------------------------------------------------
//  Charset names.
//
//  The left column is what may turn up in a CHRS kludge, a config file
//  or an environment variable; the right is what iconv is asked for and
//  what the built-in tables are keyed on.

static const struct
{
    const char* alias;
    const char* name;
}
charset_aliases[] =
{
    { "IBMPC",      "CP437"        },
    { "CP437",      "CP437"        },
    { "437",        "CP437"        },
    { "CP850",      "CP850"        },
    { "850",        "CP850"        },
    { "CP852",      "CP852"        },
    { "852",        "CP852"        },
    { "CP855",      "CP855"        },
    { "CP858",      "CP858"        },
    { "858",        "CP858"        },
    { "CP866",      "CP866"        },
    { "866",        "CP866"        },
    { "ALT",        "CP866"        },
    { "CP1125",     "CP1125"       },
    { "1125",       "CP1125"       },
    { "RUSCII",     "CP1125"       },
    { "CP1250",     "CP1250"       },
    { "1250",       "CP1250"       },
    { "CP1251",     "CP1251"       },
    { "1251",       "CP1251"       },
    { "WIN",        "CP1251"       },
    { "CP1252",     "CP1252"       },
    { "1252",       "CP1252"       },
    { "LATIN-1",    "ISO-8859-1"   },
    { "LATIN1",     "ISO-8859-1"   },
    { "ISO-1",      "ISO-8859-1"   },
    { "ISO1",       "ISO-8859-1"   },
    { "LATIN-2",    "ISO-8859-2"   },
    { "LATIN2",     "ISO-8859-2"   },
    { "ISO-2",      "ISO-8859-2"   },
    { "LATIN-5",    "ISO-8859-9"   },
    { "ISO-5",      "ISO-8859-5"   },
    { "ISO5",       "ISO-8859-5"   },
    { "LATIN-9",    "ISO-8859-15"  },
    { "ISO-15",     "ISO-8859-15"  },
    { "KOI8",       "KOI8-R"       },
    { "KOI8R",      "KOI8-R"       },
    { "KOI8-R",     "KOI8-R"       },
    { "KOI8U",      "KOI8-U"       },
    { "KOI8-U",     "KOI8-U"       },
    { "MAC",        "MACINTOSH"    },
    { "MACINTOSH",  "MACINTOSH"    },
    { "MACCYR",     "MAC-CYRILLIC" },
    { "UTF-8",      "UTF-8"        },
    { "UTF8",       "UTF-8"        },
    { "CP65001",    "UTF-8"        },
    { "65001",      "UTF-8"        },
    //  ASCII-only charsets. Anything outside ASCII gets substituted,
    //  which is exactly what the old asc_* tables did.
    { "ASCII",      "US-ASCII"     },
    { "ASC",        "US-ASCII"     },
    { "US-ASCII",   "US-ASCII"     },
};


//  ------------------------------------------------------------------
//  Strip the FTN charset level ("CP866 2" -> "CP866") and any leading
//  or trailing blanks, then uppercase what is left.

static std::string charset_bare(const char* name)
{
    if(name == NULL)
        return std::string();

    while(*name == ' ' or *name == '\t')
        name++;

    std::string s;
    while(*name and *name != ' ' and *name != '\t')
    {
        s += (char)g_toupper(*name);
        name++;
    }
    return s;
}


std::string GRecoder::canonical(const char* name)
{
    std::string s = charset_bare(name);
    if(s.empty())
        return s;

    for(size_t n = 0; n < ARRAYSIZE(charset_aliases); n++)
    {
        if(s == charset_aliases[n].alias)
            return charset_aliases[n].name;
    }

    //  "CP<digits>" that we have no alias for is still a perfectly good
    //  iconv name, as is anything else; hand it over untouched.
    return s;
}


bool GRecoder::same(const char* a, const char* b)
{
    return canonical(a) == canonical(b);
}


bool GRecoder::is_utf8(const char* name)
{
    return canonical(name) == "UTF-8";
}


//  ------------------------------------------------------------------
//  Windows codepages.
//
//  Windows has no iconv, but it has the same capability built in:
//  MultiByteToWideChar and WideCharToMultiByte convert through UTF-16
//  for every codepage the system knows, which is a great deal more than
//  the handful of tables compiled in below.

#ifdef __WIN32__

static unsigned win_codepage(const std::string& name)
{
    static const struct
    {
        const char* name;
        unsigned    cp;
    }
    known[] =
    {
        { "UTF-8",        CP_UTF8 },
        { "US-ASCII",     20127 },
        { "KOI8-R",       20866 },
        { "KOI8-U",       21866 },
        { "ISO-8859-1",   28591 },
        { "ISO-8859-2",   28592 },
        { "ISO-8859-5",   28595 },
        { "ISO-8859-9",   28599 },
        { "ISO-8859-15",  28605 },
        { "MACINTOSH",    10000 },
        { "MAC-CYRILLIC", 10007 },
    };

    for(size_t n = 0; n < ARRAYSIZE(known); n++)
    {
        if(name == known[n].name)
            return known[n].cp;
    }

    //  "CP<digits>" is the codepage number itself, which is how most of
    //  them are named.
    //  Not compare(pos, len, const char*): Open Watcom's basic_string
    //  has no such overload.
    if(name.length() >= 2 and strncmp(name.c_str(), "CP", 2) == 0)
    {
        unsigned cp = 0;
        for(size_t n = 2; n < name.length(); n++)
        {
            if(name[n] < '0' or name[n] > '9')
                return 0;
            cp = cp * 10 + (unsigned)(name[n] - '0');
        }
        return cp;
    }

    return 0;
}


//  True when the system actually has this codepage installed.
static bool win_codepage_usable(unsigned cp)
{
    if(cp == 0)
        return false;
    if(cp == CP_UTF8)
        return true;

    CPINFO info;
    return GetCPInfo(cp, &info) != 0;
}

#endif


//  ------------------------------------------------------------------
//  OS/2's ULS names its charsets the IBM way - "IBM-866" where we say
//  "CP866" - and knows a great many of them, so the mapping is the same
//  shape as the Win32 one above: a few names spelled out, the numbered
//  codepages derived, and anything else handed over as it stands for
//  ULS to accept or refuse.

#ifdef HAS_ULS

static std::string uls_charset_name(const std::string& name)
{
    static const struct
    {
        const char* ours;
        const char* uls;
    }
    known[] =
    {
        { "UTF-8",       "UTF-8"      },
        { "US-ASCII",    "IBM-367"    },
        { "ISO-8859-1",  "IBM-819"    },
        { "ISO-8859-2",  "IBM-912"    },
        { "ISO-8859-5",  "IBM-915"    },
        { "ISO-8859-9",  "IBM-920"    },
        { "ISO-8859-15", "IBM-923"    },
        { "KOI8-R",      "IBM-878"    },
        { "KOI8-U",      "KOI8-U"     },
        { "MACINTOSH",   "IBM-1275"   },
    };

    for(size_t n = 0; n < ARRAYSIZE(known); n++)
    {
        if(name == known[n].ours)
            return std::string(known[n].uls);
    }

    //  "CP<digits>" is the codepage number, which ULS spells "IBM-<n>".
    if(name.length() > 2 and strncmp(name.c_str(), "CP", 2) == 0)
    {
        bool digits = true;
        for(size_t n = 2; n < name.length(); n++)
        {
            if(name[n] < '0' or name[n] > '9')
                digits = false;
        }
        if(digits)
            return std::string("IBM-") + name.substr(2);
    }

    return name;
}


//  Make a conversion object, or NULL when ULS does not know the
//  charset. The name goes in as a NUL-terminated UniChar string.

static UconvObject uls_open(const std::string& name)
{
    std::string  spec = uls_charset_name(name);
    UconvObject  cv   = NULL;

    //  A vector, not an array: charset names are short, but nothing in
    //  the callers guarantees it.
    std::vector<UniChar> wide(spec.length() + 1);
    for(size_t n = 0; n < spec.length(); n++)
        wide[n] = (UniChar)(unsigned char)spec[n];
    wide[spec.length()] = 0;

    if(UniCreateUconvObject(&wide[0], &cv) != ULS_SUCCESS)
        return NULL;

    //  Left alone, ULS *drops* what it cannot represent: a line of
    //  Vietnamese converted for a CP866 screen comes back as its bare
    //  consonants, with nothing to show that anything was lost. Ask
    //  instead for the substitute this file uses everywhere else, so
    //  the text keeps its shape and the loss is visible.
    {
        uconv_attribute_t attr;
        if(UniQueryUconvObject(cv, &attr, sizeof(attr),
                               NULL, NULL, NULL) == ULS_SUCCESS)
        {
            attr.options     = UCONV_OPTION_SUBSTITUTE_BOTH;
            attr.subchar_len = 1;
            attr.subchar[0]  = SUBSTITUTE;
            UniSetUconvObject(cv, &attr);
        }
    }

    return cv;
}

#endif // HAS_ULS


//  ------------------------------------------------------------------
//  The built-in tables, used when iconv is missing.

static const uint16_t* find_cptable(const std::string& name)
{
    for(size_t n = 0; n < ARRAYSIZE(cptables); n++)
    {
        if(name == cptables[n].name)
            return cptables[n].tab;
    }
    return NULL;
}


//  Reverse lookup: Unicode codepoint back to a byte in the given table.
//  Linear over 128 entries, which is fine - this path only runs on
//  platforms without iconv, converting a screenful of text at a time.

static int cptable_encode(const uint16_t* tab, uint32_t cp)
{
    if(cp < 0x80)
        return (int)cp;
    if(cp > 0xFFFF)
        return -1;
    for(int n = 0; n < 128; n++)
    {
        if(tab[n] == (uint16_t)cp)
            return 0x80 + n;
    }
    return -1;
}


//  ------------------------------------------------------------------

GRecoder::GRecoder()
    : __from(), __to(), __state(state_closed),
      __to_unicode(NULL), __from_unicode(NULL), __cd(NULL),
      __cp_from(0), __cp_to(0), __uconv_from(NULL), __uconv_to(NULL)
{
}


GRecoder::GRecoder(const char* from, const char* to)
    : __from(), __to(), __state(state_closed),
      __to_unicode(NULL), __from_unicode(NULL), __cd(NULL),
      __cp_from(0), __cp_to(0), __uconv_from(NULL), __uconv_to(NULL)
{
    open(from, to);
}


GRecoder::~GRecoder()
{
    close();
}


void GRecoder::close()
{
#ifdef HAS_ICONV
    if(__cd)
    {
        iconv_close((iconv_t)__cd);
        __cd = NULL;
    }
#endif
#ifdef HAS_ULS
    if(__uconv_from)
    {
        UniFreeUconvObject((UconvObject)__uconv_from);
        __uconv_from = NULL;
    }
    if(__uconv_to)
    {
        UniFreeUconvObject((UconvObject)__uconv_to);
        __uconv_to = NULL;
    }
#endif
    __to_unicode = __from_unicode = NULL;
    __cp_from = __cp_to = 0;
    __state = state_closed;
}


bool GRecoder::open(const char* from, const char* to)
{
    close();

    __from = canonical(from);
    __to   = canonical(to);

    if(__from.empty() or __to.empty())
        return false;

    if(__from == __to)
    {
        __state = state_identity;
        return true;
    }

#ifdef HAS_ICONV
    //  //TRANSLIT asks iconv to approximate what it cannot represent -
    //  a Cyrillic quote becoming a plain one beats the whole line being
    //  refused. Not every implementation supports the suffix, so fall
    //  back to the bare name.
    std::string tospec = __to + "//TRANSLIT";
    iconv_t cd = iconv_open(tospec.c_str(), __from.c_str());
    if(cd == (iconv_t)(-1))
        cd = iconv_open(__to.c_str(), __from.c_str());
    if(cd != (iconv_t)(-1))
    {
        __cd = (void*)cd;
        __state = state_iconv;
        return true;
    }
#endif

#ifdef HAS_ULS
    //  No iconv, but OS/2 recodes for itself. This is what keeps the
    //  built-in tables out of the OS/2 builds, the Watcom one included -
    //  it has no iconv and no prospect of one.
    {
        UconvObject uf = uls_open(__from);
        UconvObject ut = uf ? uls_open(__to) : NULL;
        if(uf and ut)
        {
            __uconv_from = (void*)uf;
            __uconv_to   = (void*)ut;
            __state      = state_uls;
            return true;
        }
        if(uf)
            UniFreeUconvObject(uf);
    }
#endif

#ifdef __WIN32__
    //  No iconv here, but the system converts codepages itself.
    {
        unsigned cpf = win_codepage(__from);
        unsigned cpt = win_codepage(__to);
        if(win_codepage_usable(cpf) and win_codepage_usable(cpt))
        {
            __cp_from = cpf;
            __cp_to   = cpt;
            __state   = state_win32;
            return true;
        }
    }
#endif

    //  Nothing better available. Try the built-in tables, which can go
    //  from any of them to any other by way of Unicode.
    bool from_utf8 = (__from == "UTF-8");
    bool to_utf8   = (__to == "UTF-8");

    __to_unicode   = from_utf8 ? NULL : find_cptable(__from);
    __from_unicode = to_utf8   ? NULL : find_cptable(__to);

    if((from_utf8 or __to_unicode) and (to_utf8 or __from_unicode))
    {
        __state = state_table;
        return true;
    }

    return false;
}


//  ------------------------------------------------------------------

std::string GRecoder::convert_table(const char* src, size_t len) const
{
    std::string result;
    result.reserve(len);

    bool from_utf8 = (__to_unicode == NULL);
    bool to_utf8   = (__from_unicode == NULL);

    const char* p = src;
    const char* end = src + len;

    while(p < end)
    {
        uint32_t cp;

        if(from_utf8)
        {
            //  The raw decoder, not g_utf8_decode(): that one honours
            //  the global mode, and here the source charset says UTF-8
            //  regardless of the mode we are running in.
            int seq = 0;
            cp = g_utf8_decode_raw(p, end, &seq);
            p += seq ? seq : 1;
        }
        else
        {
            unsigned char c = (unsigned char)*p++;
            cp = (c < 0x80) ? c : __to_unicode[c - 0x80];
        }

        if(to_utf8)
        {
            //  Raw again, for the same reason.
            char buf[GUTF8_MAXLEN];
            result.append(buf, g_utf8_encode_raw(cp, buf));
        }
        else
        {
            int b = cptable_encode(__from_unicode, cp);
            result += (char)(b < 0 ? SUBSTITUTE : b);
        }
    }

    return result;
}


//  ------------------------------------------------------------------

std::string GRecoder::convert_win32(const char* src, size_t len) const
{
#ifdef __WIN32__

    //  Through UTF-16, which is what the system speaks natively.
    int wlen = MultiByteToWideChar(__cp_from, 0, src, (int)len, NULL, 0);
    if(wlen <= 0)
        return std::string(src, len);

    //  A vector, not std::wstring: the buffer is only ever handed to the
    //  Win32 calls below, and Borland C++ 5.2 has no wide string type at
    //  all.
    std::vector<wchar_t> wide(wlen);
    MultiByteToWideChar(__cp_from, 0, src, (int)len, &wide[0], wlen);

    //  UTF-8 and the other Unicode codepages refuse these two arguments
    //  and fail the call outright if they are supplied.
    const bool unicode_target = (__cp_to == CP_UTF8) or (__cp_to == 54936) or
                                (__cp_to == 1200) or (__cp_to == 1201) or
                                (__cp_to == 12000) or (__cp_to == 12001) or
                                (__cp_to == 65000);
    const char  defchar = SUBSTITUTE;
    const char* pdef    = unicode_target ? NULL : &defchar;
    BOOL        useddef = FALSE;
    BOOL*       pused   = unicode_target ? NULL : &useddef;

    int blen = WideCharToMultiByte(__cp_to, 0, &wide[0], wlen,
                                   NULL, 0, pdef, pused);
    if(blen <= 0)
        return std::string(src, len);

    std::string result;
    result.resize(blen);
    WideCharToMultiByte(__cp_to, 0, &wide[0], wlen,
                        &result[0], blen, pdef, pused);
    return result;

#else

    return std::string(src, len);

#endif
}


//  ------------------------------------------------------------------
//  ULS converts between a charset and UCS-2, never directly between two
//  charsets, so the text goes through Unicode in two steps - the same
//  shape as the Win32 path above.
//
//  A refusal returns the source unchanged rather than an error: the
//  contract everywhere in this file is that text always comes back.

std::string GRecoder::convert_uls(const char* src, size_t len) const
{
#ifdef HAS_ULS

    //  One UniChar per source byte is always enough: no encoding this
    //  understands produces more UCS-2 units than it has bytes.
    std::vector<UniChar> wide(len + 1);

    void*    inbuf    = (void*)src;
    size_t   inbytes  = len;
    UniChar* outbuf   = &wide[0];
    size_t   outchars = wide.size();
    size_t   subs     = 0;

    if(UniUconvToUcs((UconvObject)__uconv_from, &inbuf, &inbytes,
                     &outbuf, &outchars, &subs) != ULS_SUCCESS)
        return std::string(src, len);

    size_t wlen = wide.size() - outchars;
    if(wlen == 0)
        return std::string();

    //  Four bytes per UCS-2 unit covers every target, UTF-8 included:
    //  a unit needs at most three, and the pair that makes a supple-
    //  mentary character needs four between them.
    std::string result;
    result.resize(wlen * 4 + 4);

    UniChar* pin      = &wide[0];
    size_t   inchars  = wlen;
    void*    pout     = &result[0];
    size_t   outbytes = result.size();
    subs = 0;

    if(UniUconvFromUcs((UconvObject)__uconv_to, &pin, &inchars,
                       &pout, &outbytes, &subs) != ULS_SUCCESS)
        return std::string(src, len);

    result.resize(result.size() - outbytes);
    return result;

#else

    (void)src;
    (void)len;
    return std::string(src, len);

#endif
}


//  ------------------------------------------------------------------

std::string GRecoder::convert(const char* src, size_t len) const
{
    if(src == NULL or len == 0)
        return std::string();

    if(__state == state_closed or __state == state_identity)
        return std::string(src, len);

    if(__state == state_win32)
        return convert_win32(src, len);

    if(__state == state_uls)
        return convert_uls(src, len);

    if(__state == state_table)
        return convert_table(src, len);

#ifdef HAS_ICONV

    iconv_t cd = (iconv_t)__cd;
    const bool from_utf8 = (__from == "UTF-8");

    //  Reset the shift state; some stateful encodings need it and it is
    //  harmless for the rest.
    iconv(cd, NULL, NULL, NULL, NULL);

    std::string result;
    //  Four bytes per input byte is the worst UTF-8 can do, and covers
    //  every other destination charset too.
    result.resize(len * 4 + 4);

    char*  inbuf   = const_cast<char*>(src);
    size_t inleft  = len;
    char*  outbuf  = &result[0];
    size_t outleft = result.size();

    while(inleft)
    {
        size_t rc = iconv(cd, gold_iconv_inbuf(&inbuf), &inleft, &outbuf, &outleft);
        if(rc != (size_t)(-1))
            break;

        switch(errno)
        {
        case E2BIG:
        {
            //  Grow and carry on from where we stopped.
            size_t used = result.size() - outleft;
            result.resize(result.size() * 2);
            outbuf  = &result[0] + used;
            outleft = result.size() - used;
            break;
        }

        case EILSEQ:
        case EINVAL:
        default:
        {
            //  An unconvertible or truncated character. Substitute it and
            //  step over it; dropping the rest of the line, which is what
            //  returning an error would amount to, is worse.
            //
            //  Skipping a whole character matters: stepping a single byte
            //  through a two-byte source character would emit two
            //  substitutes where the text had one letter.
            size_t skip = 1;
            if(from_utf8)
            {
                int seq = g_utf8_seqlen((unsigned char)*inbuf);
                skip = (seq > 1) ? (size_t)seq : 1;
                if(skip > inleft)
                    skip = inleft;
            }

            if(outleft == 0)
            {
                size_t used = result.size() - outleft;
                result.resize(result.size() * 2);
                outbuf  = &result[0] + used;
                outleft = result.size() - used;
            }
            *outbuf++ = SUBSTITUTE;
            outleft--;
            inbuf  += skip;
            inleft -= skip;
            break;
        }
        }
    }

    result.resize(result.size() - outleft);
    return result;

#else

    return std::string(src, len);

#endif
}


std::string GRecoder::convert(const char* src) const
{
    return src ? convert(src, strlen(src)) : std::string();
}


std::string GRecoder::convert(const std::string& src) const
{
    return convert(src.data(), src.length());
}


size_t GRecoder::convert_char(const char* src, size_t len, std::string& out) const
{
    if(src == NULL or len == 0)
        return 1;

    //  How much of the input makes up one character depends on the
    //  source charset, not on the mode GoldED happens to be running in.
    size_t used = 1;
    if(from_is_multibyte())
    {
        int seq = g_utf8_seqlen((unsigned char)*src);
        if(seq > 1)
            used = (size_t)seq;
        if(used > len)
            used = len;
    }

    if(__state == state_closed or __state == state_identity)
        out.append(src, used);
    else
        out += convert(src, used);

    return used;
}


std::string GRecoder::encode(uint32_t cp) const
{
    //  Build the UTF-8 form of the codepoint, then let the normal path
    //  turn it into the destination charset.
    char buf[GUTF8_MAXLEN];
    int  len = g_utf8_encode_raw(cp, buf);

    if(__to == "UTF-8")
        return std::string(buf, len);

    GRecoder& r = g_recoder("UTF-8", __to.c_str());
    return r.convert(buf, len);
}


//  ------------------------------------------------------------------
//  The recoder cache.

typedef std::pair<std::string, std::string> RecoderKey;
typedef std::map<RecoderKey, GRecoder*, std::less<RecoderKey> > RecoderMap;

static RecoderMap* recoders = NULL;


GRecoder& g_recoder(const char* from, const char* to)
{
    if(recoders == NULL)
        recoders = new RecoderMap;

    std::pair<std::string, std::string> key(GRecoder::canonical(from),
                                            GRecoder::canonical(to));

    RecoderMap::iterator it = recoders->find(key);
    if(it != recoders->end())
        return *(*it).second;   //  (*it), not it-> : Borland's iterator

    GRecoder* r = new GRecoder;
    r->open(key.first.c_str(), key.second.c_str());
    (*recoders)[key] = r;
    return *r;
}


void g_recoder_flush()
{
    if(recoders == NULL)
        return;

    for(RecoderMap::iterator it = recoders->begin(); it != recoders->end(); ++it)
        delete (*it).second;

    delete recoders;
    recoders = NULL;
}


GRecoder& g_to_local(const char* from)
{
    return g_recoder(from, g_local_charset());
}


GRecoder& g_from_local(const char* to)
{
    return g_recoder(g_local_charset(), to);
}


//  ------------------------------------------------------------------
//  The charsets we can name.
//
//  Taken from the alias table, with the duplicates that the aliases
//  necessarily produce collapsed, so the menu lists each charset once.

static const char* known_charsets[] =
{
    "UTF-8",
    "CP866",
    "CP1251",
    "KOI8-R",
    "KOI8-U",
    "CP1125",
    "ISO-8859-5",
    "MAC-CYRILLIC",
    "CP437",
    "CP850",
    "CP852",
    "CP858",
    "CP1250",
    "CP1252",
    "ISO-8859-1",
    "ISO-8859-2",
    "ISO-8859-15",
    "MACINTOSH",
    "US-ASCII",
};


size_t g_charset_count()
{
    return ARRAYSIZE(known_charsets);
}


const char* g_charset_name(size_t n)
{
    return (n < ARRAYSIZE(known_charsets)) ? known_charsets[n] : "";
}


//  ------------------------------------------------------------------
//  Local charset <-> Unicode.
//
//  Backed by a 256-entry cache built on first use: the screen layer asks
//  once per character drawn, which is far too often to go through iconv
//  each time.

static uint32_t local_unicode[256];
bool            local_unicode_ready = false;


//  Fill a 256-entry byte-to-codepoint table for one charset. Asking the
//  converter per character is far too slow for a screen layer that does
//  it once per character drawn, so the answer is built once.
//
//  A byte the converter cannot place stands for itself, and so does
//  NUL, which nobody draws and which would come back from the converter
//  looking like an empty string. Decoding goes through the mode-free
//  reader: what the converter produced is UTF-8 whatever GoldED happens
//  to be holding text in.

void g_build_charset_table(const char* __charset, uint32_t __table[256])
{
    GRecoder& rec = g_recoder(__charset, "UTF-8");

    for(int n = 0; n < 256; n++)
    {
        __table[n] = (uint32_t)n;

        if((n == 0) or not rec.is_open())
            continue;

        char        src = (char)n;
        std::string out = rec.convert(&src, 1);
        if(out.empty())
            continue;

        int  seq = 0;
        bool ok  = false;
        uint32_t cp = g_utf8_decode_raw(out.data(), out.data() + out.length(), &seq, &ok);
        if(ok)
            __table[n] = cp;
    }
}


//  The reverse of local_unicode, sorted so that it can be searched
//  rather than scanned. g_utf8_fold() asks for it once per non-ASCII
//  byte of every line it compares, and a hundred and twenty-eight
//  comparisons each time is a lot of nothing.

struct localrev
{
    uint32_t      cp;
    unsigned char byte;
};

static localrev local_reverse[128];
static int      local_reverse_count = 0;

static int localrev_cmp(const void* __a, const void* __b)
{
    const localrev* a = (const localrev*)__a;
    const localrev* b = (const localrev*)__b;

    if(a->cp != b->cp)
        return (a->cp < b->cp) ? -1 : 1;

    //  Two bytes can name the same character; the lower one wins, as
    //  it did when this was a scan from the bottom up.
    return (int)a->byte - (int)b->byte;
}


static void build_local_unicode()
{
    g_build_charset_table(g_local_charset(), local_unicode);

    local_reverse_count = 0;
    for(int n = 0x80; n < 256; n++)
    {
        if(local_unicode[n])
        {
            local_reverse[local_reverse_count].cp   = local_unicode[n];
            local_reverse[local_reverse_count].byte = (unsigned char)n;
            local_reverse_count++;
        }
    }
    qsort(local_reverse, (size_t)local_reverse_count, sizeof(localrev), localrev_cmp);

    local_unicode_ready = true;
}


uint32_t g_local_to_unicode(unsigned char byte)
{
    if(g_utf8_mode() or byte < 0x80)
        return byte;

    if(not local_unicode_ready)
        build_local_unicode();

    return local_unicode[byte];
}


std::string g_local_from_unicode(uint32_t cp)
{
    if(g_utf8_mode())
        return g_utf8_encode(cp);

    if(cp < 0x80)
        return std::string(1, (char)cp);

    //  Go through the recoder rather than the reverse table, so that
    //  typing a character and receiving one in a message give the same
    //  answer. The recoder transliterates where it can.
    char buf[GUTF8_MAXLEN];
    int  len = g_utf8_encode_raw(cp, buf);

    GRecoder& rec = g_recoder("UTF-8", g_local_charset());
    if(rec.is_open() and not rec.is_identity())
    {
        std::string out = rec.convert(buf, len);
        if(not out.empty())
            return out;
    }

    //  Nothing could convert it; fall back to the exact lookup.
    int b = g_unicode_to_local(cp);
    return std::string(1, (char)((b >= 0) ? b : SUBSTITUTE));
}


int g_unicode_to_local(uint32_t cp)
{
    if(g_utf8_mode() or cp < 0x80)
        return (int)cp;

    if(not local_unicode_ready)
        build_local_unicode();

    int lo = 0, hi = local_reverse_count - 1;
    while(lo <= hi)
    {
        int mid = lo + (hi - lo) / 2;

        if(local_reverse[mid].cp == cp)
        {
            //  Step back to the first byte naming this character.
            while(mid > 0 and local_reverse[mid-1].cp == cp)
                mid--;
            return local_reverse[mid].byte;
        }

        if(local_reverse[mid].cp < cp)
            lo = mid + 1;
        else
            hi = mid - 1;
    }

    return -1;
}


//  ------------------------------------------------------------------

bool g_have_iconv()
{
#if defined(HAS_ICONV) || defined(__WIN32__) || defined(HAS_ULS)
    return true;
#else
    return false;
#endif
}


//  ------------------------------------------------------------------
//  The local and console charsets.

static std::string local_charset;
static std::string console_charset;


const char* g_local_charset()
{
    if(local_charset.empty())
        local_charset = g_detect_console_charset();
    return local_charset.c_str();
}


void g_set_local_charset(const char* name)
{
    std::string canon = GRecoder::canonical(name);
    if(canon.empty())
        return;

    if(canon != local_charset)
    {
        //  Every cached recoder that mentioned the old local charset is
        //  now wrong, and the mode may have flipped between UTF-8 and
        //  single-byte. Start over.
        local_charset = canon;
        g_recoder_flush();
        local_unicode_ready = false;
        g_set_utf8_mode(canon == "UTF-8");
    }
}


const char* g_console_charset()
{
    if(console_charset.empty())
        console_charset = g_detect_console_charset();
    return console_charset.c_str();
}


void g_set_console_charset(const char* name)
{
    std::string canon = GRecoder::canonical(name);
    if(not canon.empty())
        console_charset = canon;
}


//  ------------------------------------------------------------------
//  What is the console actually in?

const char* g_detect_console_charset()
{
    static std::string detected;

    if(not detected.empty())
        return detected.c_str();

#if defined(__WIN32__)

    UINT cp = GetConsoleOutputCP();
    if(cp == 0)
        cp = GetOEMCP();
    char buf[16];
    if(cp == 65001)
        detected = "UTF-8";
    else
    {
        sprintf(buf, "CP%u", cp);
        detected = GRecoder::canonical(buf);
    }

#elif defined(__OS2__) && !defined(__USE_NCURSES__)

    //  OS/2 reports the process codepage, which is what the VIO console
    //  renders in. Codepage 1208 is OS/2's name for UTF-8 - though no
    //  VIO screen can be set to it, a cell there being one byte wide.
    //
    //  A curses build is not asking about the VIO screen at all: it is
    //  talking to whatever terminal is on the other end, so it falls
    //  through to the locale like every other curses platform.
    ULONG cp[8];
    ULONG cb;
    detected = GOLDED_DEFAULT_CHARSET;
    if(DosQueryCp(sizeof(cp), cp, &cb) == 0)
    {
        char buf[16];
        if(cp[0] == 1208)
            detected = "UTF-8";
        else
        {
            sprintf(buf, "CP%lu", cp[0]);
            detected = GRecoder::canonical(buf);
        }
    }

#elif defined(__MSDOS__)

    //  DOS has no notion of UTF-8; whatever get_charset() reports from
    //  the DOS country info is what the screen shows.
    detected = GRecoder::canonical(get_charset());

#else

    //  Unix: the locale decides. nl_langinfo(CODESET) would be the tidy
    //  answer but is not universally available, so read what setlocale()
    //  hands back and fall back to the environment.
    const char* cp = setlocale(LC_CTYPE, "");
    const char* dot = cp ? strchr(cp, '.') : NULL;

    if(dot == NULL)
    {
        const char* env = getenv("LC_ALL");
        if(env == NULL or *env == NUL)
            env = getenv("LC_CTYPE");
        if(env == NULL or *env == NUL)
            env = getenv("LANG");
        dot = env ? strchr(env, '.') : NULL;
    }

    if(dot)
    {
        //  Trim a trailing modifier such as "@euro".
        std::string s(dot + 1);
        size_t at = s.find('@');
        if(at != std::string::npos)
            strerase(s, at);
        detected = GRecoder::canonical(s.c_str());
    }
    else
    {
        //  A locale with no charset ("C", "POSIX", unset) means ASCII,
        //  but treating it as UTF-8 is the friendlier guess on a modern
        //  system and degrades to ASCII by itself.
        detected = "UTF-8";
    }

#endif

    if(detected.empty())
        detected = GOLDED_DEFAULT_CHARSET;

    return detected.c_str();
}


//  ------------------------------------------------------------------


//  ------------------------------------------------------------------
//  See grecode.h: the one recogniser for the kludges a message
//  declares its charset with.

GChsKludgeKind g_charset_kludge_tag(const char* line, const char** value)
{
    if(line == NULL)
        return GCHS_NONE;

    if(*line == '\001')
        line++;

    //  Some tossers write the RFC headers as ^ARFC-X-Charset.
    if(strnieql(line, "RFC", 3))
    {
        line += 3;
        if(not g_isalpha(*line))
            line++;
    }

    const char* p = line;
    while(*p and (*p != ' ') and (*p != ':'))
        p++;

    size_t taglen = (size_t)(p - line);

    //  Skip the separator and the blanks after it.
    if(*p == ':')
        p++;
    while((*p == ' ') or (*p == '\t'))
        p++;

    GChsKludgeKind kind = GCHS_NONE;

    if((taglen == 3) and strnieql(line, "I51", 3))
        kind = GCHS_I51;
    else if((taglen == 4) and strnieql(line, "CHRS", 4))
        kind = GCHS_PLAIN;
    else if((taglen == 7) and strnieql(line, "CHARSET", 7))
        kind = GCHS_PLAIN;
    else if((taglen == 8) and strnieql(line, "CODEPAGE", 8))
        kind = GCHS_CODEPAGE;
    else if((taglen == 9) and strnieql(line, "X-Charset", 9))
        kind = GCHS_XCHARSET;

    if(value)
        *value = p;

    return kind;
}


void g_charset_kludge_value(GChsKludgeKind kind, const char* value, char* out, size_t size)
{
    if(out == NULL or size == 0)
        return;
    *out = NUL;
    if(kind == GCHS_NONE)
        return;

    if(kind == GCHS_I51)
    {
        strxcpy(out, "LATIN-1", size);
        return;
    }

    //  The value runs to the end of its line, never into the next
    //  kludge: the drivers hand in a pointer into a whole block.
    char val[100];
    size_t n = 0;
    while(value and value[n] and (value[n] != '\r') and (value[n] != '\n')
          and (value[n] != '\001') and (n < sizeof(val)-1))
    {
        val[n] = value[n];
        n++;
    }
    while(n and (val[n-1] == ' '))
        n--;
    val[n] = NUL;

    switch(kind)
    {
    case GCHS_CODEPAGE:
        //  A bare codepage number; the charset name puts CP in front.
        strxmerge(out, size, "CP", val, NULL);
        strchg(out, '_', ' ');
        break;

    case GCHS_XCHARSET:
        //  The RFC spelling of the 8859 family folds to latin-N, which
        //  is what the conversions are named after.
        if(striinc("8859", val))
            ISO2Latin(out, val);
        else
            strxcpy(out, val, size);
        break;

    default:
    {
        //  A name may carry the QP suffix of FSC-0054 level 3; the
        //  charset itself is the name without it.
        const char* word = strlword(val);
        size_t wlen = strlen(word);
        bool qp = (wlen > 2) and strnieql("QP", word + wlen - 2, 2);

        strxcpy(out, val, size);
        if(qp and (wlen - 2) < size)
        {
            //  Cut the suffix out of the first word, keeping what
            //  follows it - the CHRS level.
            memmove(out + wlen - 2, out + wlen, strlen(out + wlen) + 1);
        }
        //  Some mail readers store '_' where the name has a space.
        strchg(out, '_', ' ');
        break;
    }
    }
}


bool g_charset_kludge(const char* line, char* out, size_t size)
{
    const char* value = NULL;
    GChsKludgeKind kind = g_charset_kludge_tag(line, &value);

    if(kind == GCHS_NONE)
        return false;

    g_charset_kludge_value(kind, value, out, size);
    return *out != NUL;
}


//  ------------------------------------------------------------------
//  ISO-8859-n and latin-n, in both directions. Here because the
//  recogniser above needs the first and the two belong together.

char* ISO2Latin(char* latin_encoding, const char* iso_encoding)
{
    static const char* latinno[] = { NULL, "1", "2", "3", "4", NULL, NULL, NULL, NULL, "5", "6", NULL, NULL, "7", "8", "9" };
    int chsno = atoi(strstr(iso_encoding, "8859")+5);
    chsno = chsno > (int)(sizeof(latinno)/sizeof(const char*)) ? 0 : chsno;
    if(latinno[chsno] == NULL)
        return strxmerge(latin_encoding, 12, iso_encoding, NULL);

    return strxmerge(latin_encoding, 8, "latin-", latinno[chsno], NULL);
}


char* Latin2ISO(char* iso_encoding, const char* latin_encoding)
{
    static const char* isono[] = { "15", "1", "2", "3", "4", "9", "10", "13", "14", "15" };
    int chsno = atoi(latin_encoding+5);
    if(chsno < 0) chsno = -chsno; // support for both latin-1 and latin1
    chsno = chsno > (int)(sizeof(isono)/sizeof(const char*)) ? 0 : chsno;
    return strxmerge(iso_encoding, 12, "iso-8859-", isono[chsno], NULL);
}
