//  This may look like C code, but it is really -*- C++ -*-

//  ------------------------------------------------------------------
//  The Goldware Library
//  Copyright (C) 1990-1999 Odinn Sorensen
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
//  Nodelist index.
//  ------------------------------------------------------------------

#include <cstdio>
#include <cstring>
#include <string>
#include <gstrall.h>
#include <gftnnl.h>
#include <grecode.h>
#include <gutf8.h>
#include <stdlib.h>


//  ------------------------------------------------------------------

void ftn_nodelist_recode_line(char* line, size_t size, const char* import)
{
    bool high = false;
    for(const char* p = line; *p; p++)
    {
        if((unsigned char)*p >= 0x80)
        {
            high = true;
            break;
        }
    }
    if(not high)
        return;

    const char* local = g_local_charset();
    if((local == NULL) or (*local == NUL))
        return;

    //  Only across the UTF-8 line: an 8-bit nodelist in an 8-bit
    //  session is taken to be in that session's charset, as it always
    //  was - guessing between two codepages there (XLATIMPORT is
    //  itself a guess when the config does not say) turned Cyrillic
    //  names into question marks.
    bool local_utf8 = strieql(local, "UTF-8") or strieql(local, "UTF8");
    bool line_utf8  = g_utf8_looks_utf8(line);
    if(line_utf8 == local_utf8)
        return;

    const char* from = line_utf8 ? "UTF-8" : import;
    if((from == NULL) or (*from == NUL) or strieql(from, local))
        return;

    GRecoder& rec = g_recoder(from, local);
    if(not rec.is_open() or rec.is_identity())
        return;

    std::string out = rec.convert(line);
    if(out.length() < size)
        memcpy(line, out.c_str(), out.length() + 1);
}

//  ------------------------------------------------------------------

void ftn_nodelist_entry::unpack(char* line)
{

    *status = NUL;
    *system = NUL;
    *location = NUL;
    *name = NUL;
    *phone = NUL;
    *baud = NUL;
    *flags = NUL;

    strchg(strtrim(line), '_', ' ');

    char* q = line;
    char* p = strchr(line, ',');
    if(p)
    {
        *p++ = NUL;
        strxcpy(status, q, sizeof(status));
        p = strchr((q=p), ',');
        if(p)
        {
            *p++ = NUL;
            // Skip over number
            p = strchr((q=p), ',');
            if(p)
            {
                *p++ = NUL;
                strxcpy(system, q, sizeof(system));
                p = strchr((q=p), ',');
                if(p)
                {
                    *p++ = NUL;
                    strxcpy(location, q, sizeof(location));
                    p = strchr((q=p), ',');
                    if(p)
                    {
                        *p++ = NUL;
                        strxcpy(name, q, sizeof(name));
                        p = strchr((q=p), ',');
                        if(p)
                        {
                            *p++ = NUL;
                            strxcpy(phone, q, sizeof(phone));
                            p = strchr((q=p), ',');
                            sprintf(baud, "%lu", atol(q));
                            if(p)
                                strxcpy(flags, p+1, sizeof(flags));
                        }
                    }
                }
            }
        }
    }
}


//  ------------------------------------------------------------------

ftn_nodelist_entry& ftn_nodelist_entry::operator=(const ftn_nodelist_entry& e)
{

    addr = e.addr;
    strcpy(address,  e.address);
    strcpy(name,     e.name);
    strcpy(status,   e.status);
    strcpy(system,   e.system);
    strcpy(location, e.location);
    strcpy(phone,    e.phone);
    strcpy(baud,     e.baud);
    strcpy(flags,    e.flags);

    return *this;
}


//  ------------------------------------------------------------------

