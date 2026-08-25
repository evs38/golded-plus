//  This may look like C code, but it is really -*- C++ -*-
//  ------------------------------------------------------------------
//  The Goldware Library. Copyright (C) Odinn Sorensen.
//  ------------------------------------------------------------------
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License as
//  published by the Free Software Foundation; either version 2 of the
//  License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//  ------------------------------------------------------------------
//  $Id$
//  ------------------------------------------------------------------

/*  Win32 entry points that Borland's DOS extender does not carry.
 *
 *  BC5.02's C runtime, cw32.lib, is the same library for its Win32 and
 *  its 32-bit DOS targets, and a few of its locale and code-page
 *  modules call the Win32 API. DPMI32.LIB, the DOS extender's Win32
 *  subset, does not implement those, so even an empty C program leaves
 *  GetDateFormatA, GetStringTypeW, GetCPInfo and GetVersionExA
 *  unresolved. Anything using the C++ library adds CreateMutexA and
 *  ReleaseMutex on top - RogueWave takes a DPMI32 build for a threaded
 *  one.
 *
 *  Under a DOS extender there is one thread, one code page and no
 *  Windows, so each of these has an honest answer.
 */

#include <windows.h>
#include <string.h>
#include <ctype.h>

HANDLE WINAPI CreateMutexA(LPSECURITY_ATTRIBUTES sa, BOOL owner, LPCSTR name)
{
    (void)sa; (void)owner; (void)name;
    /*  One thread: a mutex is a token nobody ever contends for. It must
        not be NULL, which the callers read as failure. */
    return (HANDLE)1;
}

BOOL WINAPI ReleaseMutex(HANDLE h)
{
    (void)h;
    return TRUE;
}

BOOL WINAPI GetVersionExA(LPOSVERSIONINFOA v)
{
    if(!v || v->dwOSVersionInfoSize < sizeof(OSVERSIONINFOA))
        return FALSE;
    v->dwMajorVersion    = 6;      /*  DOS 6.x is the usual host       */
    v->dwMinorVersion    = 22;
    v->dwBuildNumber     = 0;
    v->dwPlatformId      = 0;      /*  VER_PLATFORM_WIN32s, "not NT"   */
    v->szCSDVersion[0]   = '\0';
    return TRUE;
}

BOOL WINAPI GetCPInfo(UINT cp, LPCPINFO info)
{
    int i;
    (void)cp;
    if(!info)
        return FALSE;
    info->MaxCharSize = 1;         /*  single-byte throughout          */
    info->DefaultChar[0] = '?';
    info->DefaultChar[1] = '\0';
    for(i = 0; i < MAX_LEADBYTES; i++)
        info->LeadByte[i] = 0;     /*  no lead bytes: no DBCS here     */
    return TRUE;
}

int WINAPI GetDateFormatA(LCID locale, DWORD flags, CONST SYSTEMTIME* st,
                          LPCSTR fmt, LPSTR out, int cch)
{
    (void)locale; (void)flags; (void)st; (void)fmt;
    /*  No locale service in DOS. Report "not supported" the way the
        API does, which is what the caller checks. */
    if(out && cch > 0)
        *out = '\0';
    return 0;
}

int WINAPI GetStringTypeW(DWORD type, LPCWSTR src, int len, LPWORD out)
{
    (void)type; (void)src; (void)len; (void)out;
    return 0;
}


/*  ------------------------------------------------------------------
 *  Port I/O.
 *
 *  <conio.h> turns outportb() and outport() into calls to the compiler
 *  intrinsics __outportb__ and __outportw__, and the DOS extender's
 *  libraries have no code for them - but they do carry the plain C
 *  functions outportb() and outpw(), so the intrinsics can simply be
 *  handed on. The names are declared here rather than taken from
 *  <conio.h>, which has already replaced them with the macros above.
 */

void __cdecl outportb(unsigned __portid, unsigned char __value);
unsigned __cdecl outpw(unsigned __portid, unsigned __value);

unsigned char _RTLENTRY __outportb__(unsigned portid, unsigned char value)
{
    outportb(portid, value);
    return value;
}

unsigned _RTLENTRY __outportw__(unsigned portid, unsigned value)
{
    return outpw(portid, value);
}


/*  ------------------------------------------------------------------
 *  errno for the extender's own INT 386h helper.
 *
 *  -WX predefines __MT__, so <errno.h> in cw32.lib reaches errno
 *  through a function, and the plain variables are never emitted.
 *  DPMI32.LIB's int386 module was built against the single-threaded
 *  runtime and wants them by name. Nothing reads these: the tree checks
 *  the carry flag, not errno, after an interrupt.
 */

#undef errno
#undef _doserrno

int errno;
int _doserrno;


/*  ------------------------------------------------------------------
 *  The wide-character and path calls that the runtime's file modules
 *  name but that nothing on this target ever reaches: DOS has no
 *  UTF-16 file API, and its paths are already short.
 */

DWORD WINAPI GetFileAttributesW(LPCWSTR name)
{
    (void)name;
    return (DWORD)-1;                   /*  INVALID_FILE_ATTRIBUTES    */
}

HANDLE WINAPI CreateFileW(LPCWSTR name, DWORD access, DWORD share,
                          LPSECURITY_ATTRIBUTES sa, DWORD disp,
                          DWORD flags, HANDLE templ)
{
    (void)name; (void)access; (void)share;
    (void)sa; (void)disp; (void)flags; (void)templ;
    return INVALID_HANDLE_VALUE;
}

BOOL WINAPI DeleteFileW(LPCWSTR name)
{
    (void)name;
    return FALSE;
}

DWORD WINAPI GetShortPathNameA(LPCSTR longpath, LPSTR shortpath, DWORD cch)
{
    DWORD n;
    if(!longpath)
        return 0;
    n = (DWORD)strlen(longpath);
    if(shortpath && cch > n)
    {
        strcpy(shortpath, longpath);    /*  8.3 already                */
        return n;
    }
    return n + 1;                       /*  room needed, including NUL */
}

int WINAPI LCMapStringA(LCID locale, DWORD flags, LPCSTR src, int srclen,
                        LPSTR dst, int dstlen)
{
    int i;
    (void)locale;
    if(!src)
        return 0;
    if(srclen < 0)
        srclen = (int)strlen(src) + 1;
    if(dstlen == 0)
        return srclen;
    if(!dst || dstlen < srclen)
        return 0;
    for(i = 0; i < srclen; i++)
    {
        unsigned char c = (unsigned char)src[i];
        if(flags & LCMAP_LOWERCASE)
            c = (unsigned char)tolower(c);
        else if(flags & LCMAP_UPPERCASE)
            c = (unsigned char)toupper(c);
        dst[i] = (char)c;
    }
    return srclen;
}


/*  ------------------------------------------------------------------
 *  Stack size.
 *
 *  Borland's startup gives a DOS program a small stack, and GoldED goes
 *  a long way down before it does anything - the config reader, the
 *  message reader and the editor all keep line-sized buffers as locals.
 *  _stklen is the runtime's own knob for it.
 */

#include <dos.h>

unsigned _stklen = 256U * 1024U;
