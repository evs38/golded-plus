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
//  $Id$
//  ------------------------------------------------------------------
//  GoldED nodelist indexing.
//  ------------------------------------------------------------------

#ifndef __gftnnlge_h
#define __gftnnlge_h


//  ------------------------------------------------------------------

#include <gftnnl.h>


//  ------------------------------------------------------------------

#if defined(GOLD_CANPACK)
    #pragma pack(1)
#endif

//  The name field used to be 36 bytes, which was 35 characters while a
//  character was a byte. It is not any more: a Cyrillic or Greek name
//  takes two bytes per character and a nodelist may well be UTF-8 (there
//  is a DAILYUTF), so 35 bytes cut such a name in half. 80 matches
//  searchname below and leaves room for the longest name in the lists,
//  in any script.
//
//  This is the on-disk record, so the size is the file format. GoldNODE
//  writes what it is built with and ftn_golded_nodelist_index::open()
//  refuses an index whose length is not a whole number of records, so an
//  index from an older build is rebuilt rather than misread.

struct _GEIdx
{
    uint32_t pos;        // File Number OR'ed with pos in nodelist file
    ftn_addr addr;       // Node address
    char     name[80];   // Name in reversed form
    _GEIdx() : pos(0), addr()
    {
        *name = NUL;
    }
    void reset()
    {
        pos = 0;
        addr.reset();
        *name = NUL;
    }
};

#if defined(GOLD_TEMPLATE_EAGER)
inline bool operator==(const _GEIdx& a, const _GEIdx& b)
{
    return a.pos == b.pos and a.addr.equals(b.addr) and strcmp(a.name, b.name) == 0;
}
#endif

#if defined(GOLD_CANPACK)
    #pragma pack()
#endif


//  ------------------------------------------------------------------

class ftn_golded_nodelist_index : public ftn_nodelist_index_base
{

protected:

    struct fstamp
    {
        Path filename;
        long stamp;
    };

    int      fha;
    int      fhn;
    int      fhx;
    bool     index32;            // New (32-bit) address index used?

    fstamp*  nodelist;
    int      nodelists;

    int      lastfileno;

    _GEIdx   current;

    long     node;
    long     maxnode;

    long     statenode;

    char     searchname[80];
    ftn_addr searchaddr;

    void     fetchdata();
    void     getnode();
    int      namecmp() const;
    int      addrcmp() const;
    void     compare()
    {
        exactmatch = not (namebrowse ? namecmp() : addrcmp());
    }
    bool     searchfirst();
    bool     search();

public:

    ftn_golded_nodelist_index();
    virtual ~ftn_golded_nodelist_index();

    bool can_browse_name() const
    {
        return true;
    }
    bool can_browse_address() const
    {
        return true;
    }

    bool open();
    void close();

    bool find(const char* name);
    bool find(const ftn_addr& addr);

    bool previous();
    bool next();

    void first();
    void last();

    void push_state();
    void pop_state();

    const char* index_name() const;
    const char* nodelist_name() const;

};


//  ------------------------------------------------------------------

#endif

//  ------------------------------------------------------------------
