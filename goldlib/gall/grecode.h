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
//
//  This is the one place that knows how to turn text in one charset
//  into text in another. Where iconv is available it does the work.
//  Where it is not, the operating system usually still can: Windows
//  through its codepage API, OS/2 through ULS. Only where none of
//  the three exists - DOS, and anything else built without them - do
//  we fall back to a compiled-in table covering the codepages an FTN
//  system actually meets, so the DOS build keeps working unchanged.
//
//  Charset names are the ones that turn up in a FidoNet CHRS kludge
//  ("IBMPC 2", "CP866 2", "LATIN-1 2", "UTF-8 4") as well as the ones
//  iconv knows; GRecoder::canonical() maps between them.
//  ------------------------------------------------------------------

#ifndef __grecode_h
#define __grecode_h

#include <string>
#include <gdefs.h>

//  ------------------------------------------------------------------
//  Name of the charset GoldED represents text in internally. On a
//  Unicode build this is UTF-8; elsewhere it is whatever single-byte
//  codepage the console is in.

const char* g_local_charset();
void        g_set_local_charset(const char* name);

//  Charset the console expects, which is normally the same thing but
//  need not be - a UTF-8 build talking to a CP866 terminal, say.
const char* g_console_charset();
void        g_set_console_charset(const char* name);

//  Ask the operating system what the console is set to. Returns a
//  canonical name; never NULL.
const char* g_detect_console_charset();


//  ------------------------------------------------------------------
//  A single conversion, from one charset to another.
//
//  Constructing one is cheap enough to do per message but not per line,
//  so the common conversions are cached; see g_recoder() below.

class GRecoder
{

public:

    GRecoder();
    GRecoder(const char* from, const char* to);
    ~GRecoder();

    //  Set up (or re-target) the conversion. Returns false when neither
    //  iconv nor the built-in tables can do it, in which case convert()
    //  degrades to passing text through unchanged.
    bool open(const char* from, const char* to);
    void close();

    bool is_open() const
    {
        return __state != state_closed;
    }

    //  True when source and destination are the same charset, so callers
    //  can skip the copy altogether.
    bool is_identity() const
    {
        return __state == state_identity;
    }

    //  Convert. Characters with no representation in the destination
    //  charset become the substitute character, never an error: half a
    //  message is worse than an approximated one.
    std::string convert(const char* src) const;
    std::string convert(const char* src, size_t len) const;
    std::string convert(const std::string& src) const;

    //  Convert a single Unicode codepoint into the destination charset.
    std::string encode(uint32_t cp) const;

    //  Convert exactly one source character, appending the result to
    //  'out', and return how many bytes of 'src' it consumed - always at
    //  least one, so a caller stepping by the return value terminates.
    //
    //  This exists for the message reader, which recodes as it walks the
    //  text looking for line breaks and quote markers and so cannot hand
    //  over the whole buffer at once.
    size_t convert_char(const char* src, size_t len, std::string& out) const;

    //  True when a source character may span more than one byte, and the
    //  caller therefore cannot assume one byte is one character.
    bool from_is_multibyte() const
    {
        return __from == "UTF-8";
    }

    const char* from() const
    {
        return __from.c_str();
    }
    const char* to() const
    {
        return __to.c_str();
    }

    //  Map a charset name onto the spelling iconv understands. Handles
    //  the FTN aliases (IBMPC, LATIN-1, +7_FIDO) and the bare-number
    //  forms ("866" -> "CP866"). Returns the input unchanged when it
    //  recognises nothing, so unusual names still reach iconv.
    static std::string canonical(const char* name);

    //  True if the two names denote the same charset.
    static bool same(const char* a, const char* b);

    //  True if the named charset is UTF-8.
    static bool is_utf8(const char* name);

private:

    enum state_t
    {
        state_closed,
        state_identity,
        state_iconv,
        state_uls,          // OS/2's own Unicode API, through UCONV.DLL
        state_win32,        // MultiByteToWideChar/WideCharToMultiByte
        state_table         // the compiled-in tables, last resort
    };

    std::string __from;
    std::string __to;
    state_t     __state;

    //  What each source byte converts to, filled in the first time
    //  convert_char() is asked. A single-byte source charset makes the
    //  conversion of one character a pure function of one byte, and
    //  every message read from an 8-bit echo goes through
    //  convert_char() once per character - a full convert() there,
    //  with its shift-state reset, was the cost this removes. Only
    //  built for a single-byte source; UTF-8 input takes the ordinary
    //  path.
    mutable bool        __chartab_ready;
    mutable std::string __chartab[256];

    //  When iconv is unavailable we go through Unicode using a pair of
    //  256-entry tables, one for each direction.
    const uint16_t* __to_unicode;   // source byte  -> codepoint
    const uint16_t* __from_unicode; // used to build the reverse lookup

    void* __cd;                     // iconv_t, opaque here

    //  Windows codepage numbers for the state_win32 path.
    unsigned __cp_from;
    unsigned __cp_to;

    //  The state_uls path needs two of OS/2's conversion objects:
    //  unlike iconv, ULS only converts between a charset and UCS-2,
    //  so the text goes through Unicode the same way the Win32 one
    //  takes it through UTF-16.
    void* __uconv_from;
    void* __uconv_to;

    std::string convert_table(const char* src, size_t len) const;
    std::string convert_win32(const char* src, size_t len) const;
    std::string convert_uls(const char* src, size_t len) const;

    GRecoder(const GRecoder&);              // not copyable: __cd is owned
    GRecoder& operator=(const GRecoder&);
};


//  ------------------------------------------------------------------
//  Cached recoders. The reader converts every line of a message with
//  the same pair of charsets, so handing out a shared instance saves
//  reopening iconv thousands of times.
//
//  The returned reference stays valid until g_recoder_flush().

GRecoder& g_recoder(const char* from, const char* to);
void      g_recoder_flush();

//  Shorthands for the two conversions that happen constantly.
GRecoder& g_to_local(const char* from);
GRecoder& g_from_local(const char* to);


//  ------------------------------------------------------------------
//  Translation between the local charset and Unicode.
//
//  These matter when GoldED holds text in a single-byte charset but the
//  console speaks UTF-8 - reading a CP866 message base on a modern
//  terminal, say. The screen layer converts on the way out and the
//  keyboard layer on the way in, so the rest of the program never has to
//  know the two differ.
//
//  In UTF-8 mode both are the identity and cost nothing.

//  Fill a 256-entry byte-to-codepoint table for one charset - see the
//  note on the definition.
void g_build_charset_table(const char* charset, uint32_t table[256]);


//  ------------------------------------------------------------------
//  The kludges a message declares its charset with. One recogniser,
//  used both by the message-base drivers - which need the charset
//  without reading the text, for the header fields in a list - and by
//  the text scanner in the reader. The two must agree: a kludge one
//  recognises and the other does not decodes the same message two
//  ways.

enum GChsKludgeKind
{
    GCHS_NONE = 0,
    GCHS_PLAIN,         //  CHRS: or CHARSET: - the FTS-5003 forms
    GCHS_XCHARSET,      //  X-Charset: - 8859 names fold to latin-N
    GCHS_CODEPAGE,      //  CODEPAGE: - a bare number, CP goes in front
    GCHS_I51            //  I51 - FSC-0051, which means LATIN-1
};

//  What kind of declaration this line is, and where its value starts.
GChsKludgeKind g_charset_kludge_tag(const char* line, const char** value);

//  The value in the canonical form the reader adopts.
void g_charset_kludge_value(GChsKludgeKind kind, const char* value, char* out, size_t size);

//  Both at once: false when the line declares no charset.
bool g_charset_kludge(const char* line, char* out, size_t size);

//  Undo the '_'-for-space some readers write into a charset name,
//  leaving the one identifier that is spelled with an underscore.
void g_charset_fix_underscores(char* name);

//  ISO-8859-n and latin-n, in both directions.
char* ISO2Latin(char* latin_encoding, const char* iso_encoding);
char* Latin2ISO(char* iso_encoding, const char* latin_encoding);

uint32_t g_local_to_unicode(unsigned char byte);

//  The exact reverse: the byte that stands for this codepoint, or -1 if
//  the charset has none. Exact means exact - no approximation - because
//  the callers use it where a byte has to map back to itself.
int      g_unicode_to_local(uint32_t cp);

//  Encode a codepoint in the local charset, approximating when there is
//  no exact byte for it. The converter's transliteration is used where
//  it has one, so a typed 'a-acute' becomes what the same character
//  would become on its way in from a message rather than turning into a
//  bare question mark; only characters it cannot approximate at all end
//  up as the substitute. May return more than one byte - "ss" for the
//  German sharp s - or, in UTF-8 mode, the UTF-8 encoding itself.
std::string g_local_from_unicode(uint32_t cp);


//  ------------------------------------------------------------------
//  The charsets this build knows by name, for the menu that lets the
//  charset be switched while GoldED is running. These are the ones the
//  recoder can name; iconv accepts plenty more, and a charset typed
//  into the config is passed through whether or not it is listed here.

//  The name and level FTS-5003 gives a charset in a CHRS kludge.
//  Pass NULL for either when only the other is wanted.
void        g_charset_ftn(const char* name, char* out, size_t size, int* level);

//  The name this program knows a charset by, given the one FTS-5003
//  writes into a CHRS kludge. For naming a charset to anything that is
//  not Fidonet - an RFC header, where IANA's spelling is what counts
//  and LATIN-1 or CP10000 name nothing.
void        g_charset_from_ftn(const char* ftn, char* out, size_t size);
bool        g_charset_is_level1(const char* name);

size_t      g_charset_count();
const char* g_charset_name(size_t n);


//  ------------------------------------------------------------------
//  True when the platform can recode at all beyond the built-in tables:
//  iconv where there is one, the Windows codepage API on Win32 and
//  OS/2's ULS, all of which cover the same ground.

bool g_have_iconv();


//  ------------------------------------------------------------------

#endif // __grecode_h

//  ------------------------------------------------------------------
