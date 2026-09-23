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
//  Transliteration: text a destination charset cannot hold, written
//  in Latin letters instead of a row of substitutes.
//
//  Two engines behind one call. ICU's transliterators, where ICU is
//  there - linked (GOLD_ICU), or loaded at run time from the library
//  every macOS and every Windows since 10 carries - know every script
//  and the standard rule sets (BGN/PCGN for Russian, say). Without ICU
//  a built-in table does the Cyrillic and Greek alphabets after
//  BGN/PCGN, the Latin letters with diacritics, and the Japanese kana
//  after Hepburn; ideographs it cannot.
//
//  What goes through here is decided by the recoder: only a word the
//  destination charset cannot represent, and only when the rules are
//  on. See grecode.cpp.
//  ------------------------------------------------------------------

#ifndef __GTRANSLIT_H
#define __GTRANSLIT_H

#include <string>
#include <stddef.h>


//  The rules: the ID of an ICU transliterator to run before the
//  general Any-Latin one ("Russian-Latin/BGN", the default), "TABLE"
//  for the built-in table only, or "NO" to switch transliteration off.
void        g_set_translit_rules(const char* rules);
bool        g_translit_enabled();

//  What answers: "ICU (rules)", "table (rules)" or "off" - for the log.
const char* g_translit_engine();

//  Transliterate UTF-8 text into Latin letters. With 'ascii' the
//  diacritics go too, so the result is plain ASCII where the engine
//  knows the script at all; without it a Latin letter keeps its accent,
//  for a destination that can hold it. Text the engine cannot render
//  comes back as it went in.
std::string g_translit(const char* utf8, size_t len, bool ascii);
std::string g_translit(const std::string& utf8, bool ascii);

#endif
