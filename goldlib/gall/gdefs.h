/*  This include file is used in C and C++ sources.
    Don't use C++ specific code here please.
*/
/*
  ------------------------------------------------------------------
  The Goldware Library
  Copyright (C) 1990-1999 Odinn Sorensen
  Copyright (C) 1999-2000 Alexander S. Aganichev
  ------------------------------------------------------------------
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this program; if not, write to the Free
  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
  MA 02111-1307, USA
  ------------------------------------------------------------------
  $Id$
  ------------------------------------------------------------------
  Basic definitions and types.
  ------------------------------------------------------------------
*/
#ifndef __gdefs_h
#define __gdefs_h

/*  ------------------------------------------------------------------
 *  Solaris.
 *
 *  Only the GNU makefile used to define __SUNOS__, so a CMake build did
 *  not know where it was and went on to define types the system already
 *  has. The compiler knows; ask it.
 */
#if defined(__sun) && defined(__SVR4) && !defined(__SUNOS__)
    #define __SUNOS__
#endif

/*  ------------------------------------------------------------------
 *  Open Watcom.
 *
 *  Its C++ library puts the C names in namespace std and nowhere else,
 *  so <cstdio> on its own leaves FILE, printf, va_list and the rest
 *  unreachable unqualified - and this tree spells them unqualified
 *  throughout. The .h forms do declare them globally, so pull those in
 *  once, early, and let every later <cstdio> add its std:: names on top.
 */
#if defined(__WATCOMC__) && defined(__cplusplus)
    #include <stddef.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <ctype.h>
    #include <stdarg.h>
    #include <time.h>
    #include <signal.h>
    #include <locale.h>

    /*  And the one C name it spells differently. The Linux target has
     *  it under neither name - that runtime does not carry it - so a
     *  replacement stands in; see gfilutl1.cpp. */
    #if !defined(__LINUX__)
        #define mktemp _mktemp
    #else
        #define mktemp gwatcom_mktemp
        char *gwatcom_mktemp(char *tmpl);
    #endif

    /*  Its library also has no operator<< for std::string, so every
     *  `cerr << some_string' in the tree would be read as a shift on an
     *  integer. One inline function covers the lot; it has to be
     *  declared before any of them, which this header is.
     */
    #include <string>
    #include <ostream>

    inline std::ostream& operator<<(std::ostream& os, const std::string& s)
    {
        return os << s.c_str();
    }
#endif

/*  ------------------------------------------------------------------
 *  Borland C++.
 *
 *  Its iostreams are older than namespace std and declare cin, cout,
 *  cerr and the rest at global scope only, while the container part of
 *  the same library does use std. The tree writes std::cerr, so give
 *  that name somewhere to resolve to. 0x0550 is C++Builder 5, the
 *  first Borland release whose iostreams are already in std.
 */
#if defined(__BORLANDC__) && (__BORLANDC__ < 0x0550) && defined(__cplusplus)
    #include <iostream.h>
    #include <fstream.h>
    #include <iomanip.h>

    namespace std
    {
        using ::istream;
        using ::ostream;
        using ::ifstream;
        using ::ofstream;
        using ::fstream;
        using ::cin;
        using ::cout;
        using ::cerr;
        using ::clog;
        using ::setw;
        using ::setfill;
        using ::setprecision;
        using ::setiosflags;
        using ::resetiosflags;
        using ::endl;
        using ::ends;
        using ::flush;
    }

#endif

/*  ------------------------------------------------------------------
 *  Borland C++, both releases.
 */
#if defined(__BORLANDC__)
    /*  <ctime> and <time.h> here each declare struct tm, and the
     *  compiler rejects whichever it sees second. The .h form first
     *  settles it: <ctime> then adds its std:: name on top.
     */
    #include <time.h>

    /*  Borland C++ 5.5.1 dies with an internal compiler error on the
     *  _wsopen() declaration in its own <io.h> when that header arrives
     *  after some combination of the tree's own - gmemdbg.h and
     *  gfile.h together are enough to provoke it. Reading it here,
     *  before anything else, avoids the whole thing.
     */
    #include <io.h>

    /*  And the one POSIX name the tree leans on that neither runtime
     *  has under any spelling. int is right for every target either of
     *  them builds: Win32 and the 32-bit DOS extender are both flat and
     *  32-bit.
     */
    #if !defined(_SSIZE_T_DEFINED)
        #define _SSIZE_T_DEFINED
        typedef int ssize_t;
    #endif
#endif

/*  ------------------------------------------------------------------
 *  Convenience macros to test the version of GNU C and C++ compiler
 * Use them like this:
 *  #if __GNUC_NOT_LESS (4,0)
 *  ... code requiring gcc 4.0 or later ...
 *  #endif
 * Note - they won't work for gcc1 or glibc1, since the _MINOR macros
 * were not defined then.
 */
#if defined __GNUC__ && defined __GNUC_MINOR__
# define __GNUC_NOT_LESS(maj, min) \
         ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
# define __GNUC_LESS(maj, min) (! __GNUC_NOT_LESS(maj, min) )
#else
# define __GNUC_NOT_LESS(maj, min)  0
# define __GNUC_LESS(maj, min)      0
#endif

/*  ------------------------------------------------------------------
 *  Convenience macros to test the version of Visua Studio and
 * macros to test the version of Visual C (cl.exe /? show it).
 * Use them like this:
 *  #if __VISUAL_STUDIO_NOT_LESS (6,0)
 *  ... code requiring MS VS 6.0 or later ...
 *  #endif
 */
#if defined _MSC_VER
/* FIXME: This condition not tested anywhere, it is true for VS 6.0 and VS 4.2 and  VS 5.0 */
/*
# define __VISUAL_STUDIO_NOT_LESS (maj, min) \
          ( (1000+ ((maj)-4)*100 + (min)*10) >= _MSC_VER )
*/
/* FIXME: This condition not tested anywhere, it is true for VS 4.2, 5.0, 6.0 and VS2005 (VC++ 8.0) */
# define __VISUAL_STUDIO_NOT_LESS(maj, min) \
         ( (maj==4)? ((1000+(min)*10)>=_MSC_VER) : \
           ( (maj==5)? ((1100+(min)*10)>=_MSC_VER) : \
             ( (maj==6)? ((1200+(min)*10)>=_MSC_VER) : \
               ( (maj==7)? ((1300+(min)*10)>=_MSC_VER) : \
                 ( (maj==8)? ((1400+(min)*10)>=_MSC_VER) : \
                   ( (maj==9)? ((1500+(min)*10)>=_MSC_VER) : \
                               0 \
         ) ) ) ) ) )
# define __VISUAL_C_NOT_LESS(maj, min) \
         ( _MSC_VER >= ((maj*100) + (min)) )
# define __VISUAL_STUDIO_LESS(maj, min)  (! __VISUAL_STUDIO_NOT_LESS(maj, min) )
# define __VISUAL_C_LESS(maj, min)  (! __VISUAL_C_NOT_LESS(maj, min) )
#else
# define __VISUAL_STUDIO_NOT_LESS(maj, min)  0
# define __VISUAL_STUDIO_LESS(maj, min)      0
# define __VISUAL_C_NOT_LESS(maj, min)       0
# define __VISUAL_C_LESS(maj, min)           0
#endif

/*  ------------------------------------------------------------------ */
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
    #include <malloc.h>
#endif
#include <string.h>
#include <limits.h>
#include <gcmpall.h>

#ifdef __WIN32__
    #include <tchar.h>
#else
    typedef char TCHAR;
#endif
#ifdef __cplusplus
    #include <cstddef>
#endif
/*  TCHAR is not in <tchar.h> on every Win32 toolchain - Borland's
 *  declares only _TCHAR there, and the plain name arrives with
 *  <winnt.h>. So the Borland Win32 targets take the same route MSVC
 *  does and pull <windows.h> in here, early, which is also where the
 *  note below says it has to be.
 */
#if defined(_MSC_VER) || (defined(__BORLANDC__) && defined(__WIN32__))
    #include <windows.h>
#elif defined(__MINGW32__) || defined(__CYGWIN__) || defined(__DJGPP__) \
      || defined(__WATCOMC__)
    #include <stdint.h>
#endif

//  A native Win32 build has to see <windows.h> before the control-code
//  macros further down: winnt.h declares a struct member called CR, and
//  the CR macro would rewrite it into a character constant. MSVC gets it
//  above already; mingw pulled it in only from the few sources that need
//  the Win32 API, which is too late.
#ifdef __MINGW32__
    //  WIN32_LEAN_AND_MEAN keeps the RPC headers out. rpcndr.h typedefs
    //  `byte', which is ambiguous against std::byte the moment any
    //  translation unit has opened namespace std. The few sources that
    //  want mmsystem, shellapi or the like already include them by name.
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
        #include <windows.h>
        #undef WIN32_LEAN_AND_MEAN
    #else
        #include <windows.h>
    #endif
#endif

#if !defined(ARRAYSIZE)
    #define ARRAYSIZE(A)  sizeof(A)/sizeof((A)[0])
#endif

#ifndef INT_MAX
    #define INT_MAX 214783647
#endif

/*  ------------------------------------------------------------------
// Disable some MS Visual C warnings */

#if defined(_MSC_VER)
    /*
    // C4786: 'identifier' : identifier was truncated to 'number'
    //        characters in the debug information
    //
    // C4065: switch statement contains 'default' but no 'case' labels
    //
    // C4200: nonstandard extension used : zero-sized array in struct/union
    */
    #pragma warning(disable: 4200 4786 4065)
#endif

/*  ------------------------------------------------------------------
//  Define portability and shorthand notation */

/* GCC after 2.95.x have "and", "not", and "or" predefined.
 * Watcom's C++ has them too - they are the standard alternative tokens,
 * so they are keywords there and #defining one is a hard error. Its C
 * compiler does not, and neither does MSVC, which is why this is still
 * here at all. */
#if !(defined(__WATCOMC__) && defined(__cplusplus))
    #if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 96)
        #ifndef and
            #define not      !
            #define and      &&
            #define or       ||
        #endif
    #endif
#endif

#ifndef true
    #define true  1
    #define false 0
#endif

#define NO     0
#define YES    1
#define ALWAYS 2
#define ASK    2
#define GAUTO  3
#define MAYBE  4

#define NUL ((char)'\x00')    /* Common ASCII control codes */
#define BEL '\x07'
#define BS  '\x08'
#define HT  '\x09'
#define LF  '\x0A'
#define FF  '\x0C'
#define CR  '\x0D'
#define ESC '\x1B'

#ifdef __UNIX__
    #define NL "\r\n"
#else
    #define NL "\n"
#endif

/*  ------------------------------------------------------------------
//  Spellchecker */
#if !defined(__WIN32__) && !defined(GCFG_NO_MSSPELL)
    #define GCFG_NO_MSSPELL
#endif

#if !(defined(GCFG_NO_MSSPELL) && defined(GCFG_NO_MYSPELL))
    #define GCFG_SPELL_INCLUDED
#endif


/*  ------------------------------------------------------------------
//  Special character constants */

#define CTRL_A '\x01'   /* FidoNet kludge line char  */
#define SOFTCR '\x8D'   /* "Soft" carriage-return    */


/*  ------------------------------------------------------------------
//  Supplements for the built-in types   */

/*  Borland C++ 5.2 predates <stdint.h> as much as MSVC 6 did, so it
 *  takes the same spelled-out typedefs. */
#if defined(_MSC_VER) || defined(__BORLANDC__)
    #if (UCHAR_MAX == 0xFF)
        typedef   signed char    int8_t;
        typedef unsigned char   uint8_t;
    #else
        #error Dont know how to define 8 bit integers
    #endif
    #if (USHRT_MAX == 0xFFFF)
        typedef   signed short   int16_t;
        typedef unsigned short  uint16_t;
    #else
        #error Dont know how to define 16 bit integers
    #endif
    #if (UINT_MAX == 0xFFFFFFFF)
        typedef   signed int     int32_t;
        typedef unsigned int    uint32_t;
    #else
        #error Dont know how to define 32 bit integers
    #endif
#endif  /*#if defined(_MSC_VER) || defined(__BORLANDC__) */

#if defined(__GNUC__) && !defined(__MINGW32__) && !defined(__CYGWIN__) && !defined(__DJGPP__)
    typedef unsigned char  uint8_t;
    typedef unsigned short uint16_t;
    typedef unsigned int   uint32_t;

    #if !defined(__APPLE__)
        #if !defined(__SUNOS__)
            typedef   signed char   int8_t;
        #endif
        typedef   signed short  int16_t;
        typedef   signed int    int32_t;
    #endif
#endif

typedef uint8_t   byte;
typedef uint16_t  word;
typedef uint32_t  dword;

/*#if !defined(__APPLE__)*/
typedef unsigned int uint;
/*#endif*/

typedef uint8_t   bits;

/*  Solaris has time32_t of its own, in <sys/types32.h>, and defines it
 *  as int32_t. Redefining it here is a hard error there, and the two
 *  agree on everything that matters short of dates past 2038 - which
 *  this program cannot represent either way.
 */
#if !defined(__SUNOS__)
typedef uint32_t  time32_t;   /* 32-bit time_t type */
#endif

/*  ------------------------------------------------------------------  */

#ifdef __cplusplus


/*  ------------------------------------------------------------------   */

#if defined(__GOLD_GUI__)
#define STD_PRINT(out) {  \
  std::strstream str;     \
  str << out;             \
  GUI_Print(str);         \
  }
#define STD_PRINTNL(out) {  \
  std::strstream str;       \
  str << out << NL;         \
  GUI_Print(str);           \
  }
#else
#define STD_PRINT(out) std::cerr << out;
#define STD_PRINTNL(out) std::cerr << out << NL;
#endif


/*  ------------------------------------------------------------------   */
/*  Common function-pointer types                                        */

typedef void (*VfvCP)();
typedef int (*IfvCP)();
typedef int (*IfcpCP)(char*);


/*  ------------------------------------------------------------------   */
/*  Function pointer for stdlib qsort(), bsearch() compare functions     */

typedef int (*StdCmpCP)(const void*, const void*);


/*  ------------------------------------------------------------------   */
/*  Utility templates                                                    */

template <class T> inline bool in_range(T a, T b, T c)
{
    return (a >= b) and (a <= c);
}
template <class T> inline    T absolute(T a)
{
    return a < 0 ? -a : a;
}
template <class T> inline  int compare_two(T a, T b)
{
    return a < b ? -1 : a > b ? 1 : 0;
}
template <class T> inline    T minimum_of_two(T a, T b)
{
    return (a < b) ? a : b;
}
template <class T> inline    T maximum_of_two(T a, T b)
{
    return (a > b) ? a : b;
}
template <class T> inline  int zero_or_one(T e)
{
    return e ? 1 : 0;
}
template <class T> inline bool make_bool(T a)
{
    return !!a;
}
template <class T> inline bool make_bool_not(T a)
{
    return !a;
}


/*  ------------------------------------------------------------------
//  Handy macro for safe casting.           Public domain by Bob Stout
//  ------------------------------------------------------------------
//
//  Example of CAST macro at work
//
//  union {
//    char  ch[4];
//    int   i[2];
//  } my_union;
//
//  long longvar;
//
//  longvar = (long)my_union;         // Illegal cast
//  longvar = CAST(long, my_union);   // Legal cast
//
//  ------------------------------------------------------------------ */

#define CAST(new_type,old_object) (*((new_type *)&(old_object)))


/*  ------------------------------------------------------------------ */
/*  Get size of structure member                                       */

#define sizeofmember(__struct, __member)  sizeof(((__struct*)0)->__member)


/*  ------------------------------------------------------------------ */
/*  Legacy defines                                                     */

#define RngV in_range
#define AbsV absolute
#define CmpV compare_two
#define MinV minimum_of_two
#define MaxV maximum_of_two

/*  ------------------------------------------------------------------ */

#endif  /*#ifdef __cplusplus*/

/*  ------------------------------------------------------------------ */

#endif

/*  ------------------------------------------------------------------ */
