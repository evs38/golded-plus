//  This may look like C code, but it is really -*- C++ -*-

//  ------------------------------------------------------------------
//  The Goldware Library
//  Copyright (C) 1990-1999 Odinn Sorensen
//  Copyright (C) 2000 Alexander S. Aganichev
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
//  Case-Insensitive Boyer-Moore-Horspool pattern match
//  ------------------------------------------------------------------

#ifndef __gbmh_h
#define __gbmh_h


//  ------------------------------------------------------------------

#include <limits.h>
#include <string>
#include <gdefs.h>


//  ------------------------------------------------------------------

class gbmh
{

protected:

    int   skip[256];   // skip-ahead count for test chars
    int   skip2;       // skip-ahead after non-match with matching final char
    char* pat;
    int   patlen;
    //  Case-insensitive matching of multibyte text cannot be done a
    //  byte at a time, because the case of a character is a property of
    //  the whole character. So when case is ignored both sides are
    //  folded first and the match itself stays a plain byte comparison.
    //
    //  Folding preserves byte length (see g_utf8_fold), which is what
    //  lets the skip table built from the pattern keep working.
    bool         ignore_case;
    std::string  foldbuf;   // reused across calls to keep find() cheap

    bool find_raw(const char* buffer) const;

public:

    gbmh();
    ~gbmh();

    void init(const char* pattern, bool ignorecase);
    bool find(const char* string);

};


//  ------------------------------------------------------------------

#endif

//  ------------------------------------------------------------------

