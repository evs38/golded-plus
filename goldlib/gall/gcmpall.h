/*  ------------------------------------------------------------------
 *  The Goldware Library
 *  Copyright (C) 1990-1999 Odinn Sorensen
 *  Copyright (C) 1999-2000 Alexander S. Aganichev
 *  ------------------------------------------------------------------
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *  MA 02111-1307, USA
 *  ------------------------------------------------------------------
 *  $Id$
 *  ------------------------------------------------------------------
 *  Compiler dependent definitions.
 *  --------------------------------------------------------------- */

#ifndef __gcmpall_h
#define __gcmpall_h


/*  --------------------------------------------------------------- */
/*  Check if type "char" is unsigned or signed                      */

#if '\x80' < 0
    #error Goldware Library requires -funsigned-char to operate properly
#endif


/*  --------------------------------------------------------------- */

#if !defined(__MSDOS__)
    #if defined(MSDOS)
        #define __MSDOS__ MSDOS
    #elif defined(__DOS__)
        #define __MSDOS__ __DOS__
    #endif
#endif

#if !defined(__OS2__)
    #if defined(OS2)
        #define __OS2__ OS2
    #elif defined(__EMX__) && !defined(__WIN32__)
        #define __OS2__ __EMX__
    #endif
#endif

/*  ------------------------------------------------------------------
 *  Borland C++ for 32-bit DOS.
 *
 *  That target predefines __WIN32__ next to __DPMI32__ - its RogueWave
 *  library needs the name to find a mutex implementation, and refuses
 *  to compile without it - but none of what this tree tests __WIN32__
 *  for is true under a DOS extender. So read the library in while the
 *  name still means to it what it expects, then take the name away and
 *  say plainly which platform this is.
 */
#if defined(__BORLANDC__) && defined(__DPMI32__) && defined(__cplusplus)
    /*  It also takes a DPMI32 build for a multithreaded one - the
     *  compiler predefines __MT__ - and then reaches for a Win32 mutex,
     *  which DOS has not got. One thread is all there is under a DOS
     *  extender, so say so before the library is read.
     */
    #undef __MT__

    #include <string>
    #include <vector>
    #include <list>
    #include <deque>
    #include <set>
    #include <map>
    #include <algorithm>
    #include <iostream.h>
    #include <fstream.h>

    #undef __WIN32__
    #undef _WIN32
    #undef WIN32
    #undef _Windows
    #ifndef __MSDOS__
        #define __MSDOS__ 1
    #endif
#endif

/*  Borland's 32-bit DOS target defines _WIN32 for its own runtime's
 *  benefit, and gdefs.h has already taken __WIN32__ away again because
 *  a DOS extender is not Win32 by any test in this tree. It has to stay
 *  away, so that target is excluded here.
 */
#if !defined(__WIN32__) && !(defined(__BORLANDC__) && defined(__DPMI32__))
    #if defined(_WIN32)
        #define __WIN32__ _WIN32
    #elif defined(__NT__)
        #define __WIN32__ __NT__
    #elif defined(WIN32)
        #define __WIN32__ WIN32
    #endif
#endif

/*  Haiku carries on the BeOS API - libbe, libtextencoding, the same
 *  clipboard and thread calls - so every __BEOS__ path in the tree
 *  applies to it. Its compiler defines only __HAIKU__, and neither
 *  __BEOS__ nor unix/__unix__, so say both here and let the rules below
 *  take it from there.
 */
#if !defined(__BEOS__) && defined(__HAIKU__)
    #define __BEOS__ __HAIKU__
#endif

/*  Open Watcom's Linux target says __LINUX__ and __UNIX__, but not
 *  __linux__, which is the spelling the rest of the tree tests - the
 *  version banner among other things, which without this reads /UNX.
 */
#if !defined(__linux__) && defined(__LINUX__)
    #define __linux__ __LINUX__
#endif

#if !defined(__UNIX__)
    #if defined(unix) || defined(__unix__) || defined(__unix)
        #define __UNIX__
    #endif
#endif
#if !defined(__UNIX__)
    #if defined(__linux__)
        #define __UNIX__ __linux__
    #endif
    #if defined(__FreeBSD__)
        #define __UNIX__ __FreeBSD__
    #endif
    #if defined(__OpenBSD__)
        #define __UNIX__ __OpenBSD__
    #endif
    #if defined(__DragonFly__)
        #define __UNIX__ __DragonFly__
    #endif
    #if defined(__BEOS__)
        #define __UNIX__ __BEOS__
    #endif
    #if defined(__QNXNTO__)
        #define __UNIX__ __QNXNTO__
    #endif
    #if defined(__APPLE__)
        #define __UNIX__ __APPLE__
    #endif
#endif

#if defined(__DJGPP__)
    #undef __UNIX__
#endif

#ifdef __GNUC__
    #if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 8)
        #error You definetly need to upgrade your gcc at least up to 2.8.x
    #endif
#endif

#ifdef _MSC_VER
    #define __attribute__(A)
    #define __inline__ __inline
    #define __extension__
    #define __MSVCRT__
    #define ssize_t size_t
#endif

/*  Open Watcom needs the same GNU-ism spellings taken care of. It has
 *  __inline in both C and C++, and knows nothing of __attribute__ or
 *  __extension__. */
#ifdef __WATCOMC__
    #define __attribute__(A)
    #define __inline__ __inline
    #define __extension__
#endif

/*  Borland C++ likewise. */
#ifdef __BORLANDC__
    #define __attribute__(A)
    #define __inline__ __inline
    #define __extension__
#endif


/*  --------------------------------------------------------------- */

#if defined(__MSDOS__) || defined(__OS2__) || defined(__WIN32__)
    #define __HAVE_DRIVES__
#endif

/*  ------------------------------------------------------------------
 *  Empty a container.
 *
 *  Every library here has clear() except the RogueWave one Borland C++
 *  5.02 ships, which offers only erase(first, last) - and Open Watcom's
 *  map is the other way round, clear() but no erase(first, last). One
 *  helper spells it once and reads the same everywhere.
 */

#ifdef __cplusplus
template <class Container>
inline void gclear(Container& c)
{
#if defined(__BORLANDC__) && (__BORLANDC__ < 0x0550)
    c.erase(c.begin(), c.end());
#else
    c.clear();
#endif
}
#endif


/*  Neither MSVC's library nor the RogueWave one Borland C++ 5.02 ships
 *  has a list::sort() taking a predicate; both sort on operator<, so
 *  the callers define one and sort with no argument at all. */
#if defined(_MSC_VER) || (defined(__BORLANDC__) && (__BORLANDC__ < 0x0550))
    #define GOLD_LIST_SORT_NO_PRED
#endif

/*  Borland C++ 5.02 instantiates every member of a class template as
 *  soon as the class is used, so std::list<T> wants operator== and
 *  operator< on T whether or not remove(), unique() and merge() are
 *  ever called. The types below define them for that reason. */
#if defined(__BORLANDC__) && (__BORLANDC__ < 0x0550)
    #define GOLD_TEMPLATE_EAGER
#endif

/*  Where there is no support for a variable-length array - which is a
 *  GNU extension in C++, not a language feature - alloca() stands in.
 *  Open Watcom and Borland C++ are in that group. */
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__WATCOMC__) \
 || defined(__BORLANDC__)
    #define __USE_ALLOCA__
#endif

/*  --------------------------------------------------------------- */
/*  System-wide constants                                           */

/* #define GOLD_MOUSE 1 */     /* Enable mouse code */

#define GTHROW_LOG
#define GTHROW_DEBUG
#define GTHROWCHKPTR_ENABLE
#define GFTRK_ENABLE


/*  --------------------------------------------------------------- */

#define GOLD_CANPACK
#define NW(x) x=x


/*  --------------------------------------------------------------- */

#endif
