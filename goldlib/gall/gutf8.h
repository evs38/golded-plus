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
//
//  Every function here has a defined meaning in both of GoldED's two
//  internal representations:
//
//    * UTF-8 mode   - text is UTF-8, one character may span several bytes
//                     and may occupy zero, one or two screen cells.
//    * 8-bit mode   - text is in a single-byte charset (CP866, CP437,
//                     KOI8-R...), one byte is one character is one cell.
//
//  Call sites are therefore written once, against these functions, and
//  keep working on DOS and OS/2 where the 8-bit mode is the only one
//  available. The mode is a run-time property (see g_utf8_mode()) so the
//  charset can be switched while GoldED is running.
//  ------------------------------------------------------------------

#ifndef __gutf8_h
#define __gutf8_h

#include <cstddef>
#include <string>
#include <gdefs.h>

//  ------------------------------------------------------------------
//  The replacement character, substituted for anything undecodable.

const uint32_t GUTF8_REPLACEMENT = 0xFFFD;

//  Longest encoding of a single codepoint, excluding the terminator.
const int GUTF8_MAXLEN = 4;

//  Returned by the *_at_col()/*_at_offset() helpers when the position
//  asked for lies past the end of the string.
const size_t GUTF8_NPOS = (size_t)-1;


//  ------------------------------------------------------------------
//  Mode control.
//
//  GOLD_UTF8 is a build-time switch: when it is undefined (DOS, and any
//  other platform built without Unicode support) the mode is a compile
//  time constant false, so the compiler folds away every UTF-8 branch
//  and the resulting code is the plain 8-bit code we had before.

#ifdef GOLD_UTF8

extern bool __gutf8_mode;

inline bool g_utf8_mode()
{
    return __gutf8_mode;
}

void g_set_utf8_mode(bool onoff);

#else

inline bool g_utf8_mode()
{
    return false;
}

inline void g_set_utf8_mode(bool)
{
}

#endif


//  ------------------------------------------------------------------
//  Length in bytes of the sequence introduced by the given lead byte.
//  Returns 1 for ASCII, 2..4 for a valid lead byte, and 0 for a
//  continuation byte or one of the bytes that can never start a
//  sequence (0xC0, 0xC1, 0xF5..0xFF).

int g_utf8_seqlen(unsigned char lead);


//  ------------------------------------------------------------------
//  Decode the character at 'p'. '*used' receives the number of bytes
//  consumed, which is always at least one so that callers advancing by
//  it cannot loop forever. An ill-formed sequence yields
//  GUTF8_REPLACEMENT and consumes exactly one byte, which resynchronises
//  on the next lead byte.
//
//  In 8-bit mode the byte is returned as-is; converting it to Unicode is
//  the recoder's job, not this layer's.

uint32_t g_utf8_decode(const char* p, int* used);
uint32_t g_utf8_decode(const char* p, const char* end, int* used);


//  ------------------------------------------------------------------
//  Encode 'cp' into 'out', which must have room for GUTF8_MAXLEN bytes.
//  Returns the number of bytes written; no terminator is appended.
//
//  In 8-bit mode a codepoint above 0xFF cannot be represented and is
//  written as '?'.

int g_utf8_encode(uint32_t cp, char* out);
std::string g_utf8_encode(uint32_t cp);


//  ------------------------------------------------------------------
//  The same codec, but always speaking UTF-8 whatever mode GoldED is
//  running in.
//
//  The two above answer for the representation text is being *held* in,
//  which is what the editor and the screen want. A charset converter is
//  a different matter: when it is asked for UTF-8 it means UTF-8, in an
//  8-bit session as much as in a Unicode one. That is why grecode.cpp
//  used to carry copies of this bit-twiddling of its own - and why the
//  copies drifted, losing the overlong and surrogate checks on the way.
//
//  g_utf8_decode_raw() sets '*ok', when it is given, only for a
//  well-formed character, so a U+FFFD that was really in the text can
//  be told from one that stands for an error.

int      g_utf8_len_raw(uint32_t cp);
int      g_utf8_encode_raw(uint32_t cp, char* out);
uint32_t g_utf8_decode_raw(const char* p, const char* end, int* used, bool* ok = NULL);


//  ------------------------------------------------------------------
//  Step one character forwards or backwards. g_utf8_next() never walks
//  past the terminator; g_utf8_prev() never walks before 'start'.

const char* g_utf8_next(const char* p);
const char* g_utf8_prev(const char* start, const char* p);

inline char* g_utf8_next(char* p)
{
    return const_cast<char*>(g_utf8_next((const char*)p));
}
inline char* g_utf8_prev(char* start, char* p)
{
    return const_cast<char*>(g_utf8_prev((const char*)start, (const char*)p));
}


//  ------------------------------------------------------------------
//  True if 'p' points at a byte that continues a character rather than
//  starting one. Always false in 8-bit mode.

bool g_utf8_is_continuation(const char* p);


//  ------------------------------------------------------------------
//  Number of characters (not bytes, not cells) in the string.

size_t g_utf8_strlen(const char* p);
size_t g_utf8_strlen(const char* p, size_t nbytes);


//  ------------------------------------------------------------------
//  How many screen cells the codepoint occupies: 0 for a combining mark
//  or other zero-width character, 2 for East Asian wide and fullwidth
//  characters, 1 for everything else. Control characters count as one,
//  because GoldED renders them as a visible substitute rather than
//  acting on them.

int g_cp_width(uint32_t cp);


//  Forget what the host's wcwidth() was found to be capable of. Call
//  after changing the locale; g_set_utf8_mode() already does.

void g_reset_host_width(void);


//  ------------------------------------------------------------------
//  Step over one grapheme cluster - one character as a reader sees it,
//  which may be several codepoints: a letter and its accents, an emoji
//  and its skin tone, a flag, a joined family. Always advances by at
//  least one byte, and never past 'end'.

const char* g_utf8_cluster_next(const char* p, const char* end);
const char* g_utf8_cluster_next(const char* p);
const char* g_utf8_cluster_prev(const char* start, const char* p);

inline char* g_utf8_cluster_next(char* p)
{
    return const_cast<char*>(g_utf8_cluster_next((const char*)p));
}


//  How many columns one cluster occupies.

size_t g_utf8_cluster_width(const char* p, const char* end);


//  ------------------------------------------------------------------
//  Total width in screen cells.

size_t g_utf8_width(const char* p);
size_t g_utf8_width(const char* p, size_t nbytes);
size_t g_utf8_width(const std::string& s);


//  ------------------------------------------------------------------
//  Convert between a byte offset into the string and the screen column
//  that byte is displayed at.
//
//  g_utf8_offset_at_col() returns the offset of the character that
//  covers column 'col'. When 'col' falls on the second cell of a
//  double-width character the offset of that character is returned, so
//  the cursor never lands inside a glyph. When 'col' is past the end,
//  the offset of the terminator is returned.

size_t g_utf8_offset_at_col(const char* p, size_t col);
size_t g_utf8_col_at_offset(const char* p, size_t offset);

inline size_t g_utf8_offset_at_col(const std::string& s, size_t col)
{
    return g_utf8_offset_at_col(s.c_str(), col);
}
inline size_t g_utf8_col_at_offset(const std::string& s, size_t offset)
{
    return g_utf8_col_at_offset(s.c_str(), offset);
}


//  ------------------------------------------------------------------
//  Byte offset of the character 'n' characters into the string.

size_t g_utf8_offset_at_char(const char* p, size_t n);


//  ------------------------------------------------------------------
//  Largest prefix of 's' that fits in 'maxcols' screen cells. A
//  double-width character straddling the limit is left out entirely.

size_t g_utf8_bytes_for_cols(const char* p, size_t maxcols, size_t* cols = NULL);
std::string g_utf8_truncate(const std::string& s, size_t maxcols);


//  ------------------------------------------------------------------
//  Fit 's' into exactly 'cols' screen columns: truncated if it is too
//  wide, padded with blanks if it is too narrow.
//
//  This is what printf's "%-*.*s" was being used for, and that counts
//  bytes - so columns stopped lining up as soon as the text was not
//  single-byte.

std::string g_utf8_fit(const char* p, size_t cols);


//  ------------------------------------------------------------------
//  Validate. g_utf8_valid() rejects overlong forms, surrogates and
//  anything above U+10FFFF, so it can be used to sniff whether a buffer
//  of unknown provenance is UTF-8.

bool g_utf8_valid(const char* p);
bool g_utf8_valid(const char* p, size_t nbytes);


//  ------------------------------------------------------------------
//  Case-fold for matching: upper-case every character whose upper-case
//  form takes the same number of bytes, and leave the rest alone.
//
//  Keeping the length matters. It means a byte offset into the folded
//  text still points at the same character of the original, so a match
//  found in the folded copy can be reported against the real string,
//  and it means the fold can be done in place. The few characters whose
//  case forms differ in length - Turkish dotless i, the long s - are
//  left as they are; matching those case-insensitively is a curiosity
//  beside every offset in the program becoming approximate.
//
//  In 8-bit mode ASCII folds through g_toupper() and everything above
//  it through the configured charset's Unicode case mapping: the
//  process locale's toupper() knew nothing of the charset and left
//  Cyrillic unfolded whenever the locale and the charset disagreed.

std::string g_utf8_fold(const char* p);
std::string g_utf8_fold(const std::string& s);

//  Fold one codepoint the same way: unchanged if its upper-case form
//  would need a different number of bytes.
uint32_t g_cp_fold(uint32_t cp);


//  ------------------------------------------------------------------
//  Replace every ill-formed sequence with GUTF8_REPLACEMENT, so that the
//  result is guaranteed decodable. Returns 's' unchanged in 8-bit mode.

std::string g_utf8_sanitize(const std::string& s);


//  ------------------------------------------------------------------
//  Unicode character classification and case conversion. Covers ASCII,
//  Latin-1 Supplement, Latin Extended-A, Greek and Cyrillic - which is
//  what GoldED's audience actually writes in. Anything else is returned
//  unchanged.
//
//  These take a codepoint, not a byte, and do not consult the mode: in
//  8-bit mode a byte above 0x7F is not a codepoint, and the caller has
//  to fold it with the charset's own table (g_toupper and friends) or
//  translate it first with g_local_to_unicode().

uint32_t g_cp_toupper(uint32_t cp);
uint32_t g_cp_tolower(uint32_t cp);

bool g_cp_isalpha(uint32_t cp);
bool g_cp_isspace(uint32_t cp);
bool g_cp_isdigit(uint32_t cp);
bool g_cp_isalnum(uint32_t cp);
bool g_cp_ispunct(uint32_t cp);


//  ------------------------------------------------------------------

#endif // __gutf8_h

//  ------------------------------------------------------------------
