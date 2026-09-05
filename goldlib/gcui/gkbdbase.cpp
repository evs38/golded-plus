//  This may look like C code, but it is really -*- C++ -*-

//  ------------------------------------------------------------------
//  The Goldware Library
//  Copyright (C) 1990-1999 Odinn Sorensen
//  Copyright (C) 1999-2000 Alexander S. Aganichev
//  Copyright (C) 2000 Jacobo Tarrio
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
//  GCUI: Golded+ Character-oriented User Interface.
//  Keyboard functions.
//  ------------------------------------------------------------------

#include <gctype.h>
#include <gmemdbg.h>
#include <gkbdcode.h>
#include <gkbdbase.h>
#include <gmemall.h>
#include <gutf8.h>
#include <grecode.h>
#if !(defined(__BORLANDC__) && (__BORLANDC__ < 0x0550))
    //  Borland C++ 5.02 has no <wchar.h> at all; nothing below needs it
    //  on that target, where the console is byte-oriented.
    #include <cwchar>
#endif
#if defined(__UNIX__)
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#endif
#include <string.h>

#include <stdlib.h>

#include <locale.h>

#if defined(__OS2__)
    #define INCL_BASE
    #include <os2.h>
#endif

#ifdef __WIN32__
    #include <windows.h>
    extern OSVERSIONINFO WinVer;
#endif

#if defined(__UNIX__) && !defined(__USE_NCURSES__)
    #include <gkbdunix.h>
#endif

#if defined(__DJGPP__)
    #include <sys/farptr.h>
#endif

#if defined(__USE_NCURSES__)
    #include <gcurses.h>
#endif

//  GOLD_LINUX_TIOCL: the console shift state, read through TIOCLINUX.
//  It needs a kernel header, which Open Watcom's Linux target does not
//  ship - it carries a C library and nothing else - so that build asks
//  the terminal for nothing and reports no modifiers, which is what the
//  other Unix targets do anyway.
#if defined(__linux__) && !defined(__WATCOMC__)
    #define GOLD_LINUX_TIOCL 1
    #include <sys/ioctl.h>
    #include <linux/tiocl.h>
    #include <stdio.h>
#endif

#if defined(__BEOS__)
    #include <InterfaceDefs.h>
#endif

//  ------------------------------------------------------------------

#if defined(__USE_NCURSES__)
int curses_initialized = 0;

enum
{
    ARR_UP = 0,
    ARR_DN,
    ARR_RIT,
    ARR_LFT
};

typedef int arrow_t;

enum
{
    MOD_SHIFT = 1,
    MOD_ALT = 2,
    MOD_CTRL = 4,
    MOD_META = 8
};

typedef int modmask_t;

const modmask_t MOD_MIN = MOD_ALT;
const modmask_t MOD_MAX = MOD_ALT|MOD_CTRL|MOD_META|MOD_SHIFT;

typedef int modifier_t;

static modifier_t modifier(modmask_t mask)
{
    return mask + 1;
}

void gkbd_setarrow(modifier_t m, arrow_t a, int k);
void gkbd_setfnkeys(void);

#if defined(NCURSES_VERSION_MAJOR) && NCURSES_VERSION_MAJOR-0 >= 5
    #define HAVE_EXTENDED_NAMES 1
#endif
//#define NO_MOD_DECFNK_XTERM_R7 1
//#define NO_MOD_RXVT 1
#endif


//  ------------------------------------------------------------------

#if defined(__WIN32__)
    #define KBD_TEXTMODE (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT)
#endif


//  ------------------------------------------------------------------
//  Global keyboard data

#if defined(__WIN32__) && !defined(__USE_NCURSES__)
    HANDLE gkbd_hin;
    DWORD  gkbd_kbdmode;
    int    gkbd_nt;
#endif

GKbd gkbd;

int blanked = false;

bool right_alt_same_as_left = false;

//  ------------------------------------------------------------------
//  The last typed character.
//
//  A gkey has one byte for the character, which stopped being enough
//  when a character stopped being a byte. So the keyboard also records
//  what it really read here, and the few places that insert text into a
//  message pick it up. Every platform's keyboard fills this in, so it
//  lives outside the per-platform sections below.

static GKbdChar gkbd_last = { 0, "" };


const char* gkbd_lastchars(int* len)
{
    if(len)
        *len = gkbd_last.len;
    return gkbd_last.buf;
}


//  The bytes the key stands for, or NULL when the key was not a whole
//  character. A gkey has one byte for the character, so a key that was
//  typed as two or more arrives here as a fragment; the rest is in the
//  side channel above.
//
//  'checkfirst' compares the first byte against the key, guarding
//  against the key having been remapped on the way. The area picker
//  asks for that to be skipped: by the time it sees the key it has been
//  case-folded, and in a UTF-8 locale that maps a lead byte like 0xD0
//  onto 0xF0. A length above one is guard enough there, because
//  gkbd_setlastcp(0) clears the side channel for every key that is not
//  a character.

const char* gkbd_keychars(gkey key, int* len, bool checkfirst)
{
    int _len = 0;
    const char* _chars = gkbd_lastchars(&_len);

    if(len)
        *len = 0;

    if(_len <= 1)
        return NULL;
    if(checkfirst and ((byte)_chars[0] != KCodAsc(key)))
        return NULL;

    if(len)
        *len = _len;
    return _chars;
}


//  ------------------------------------------------------------------
//  Record the character just typed, as bytes in the local charset.
//
//  'islocalwide' says the value came from the C library's wide input in
//  a single-byte locale, where it is the library's own representation
//  and not a codepoint - see g_local_to_unicode_wide() in gvidbase.cpp
//  for the same problem on the way out. There the byte is what the
//  library says it is, and no charset conversion belongs in between.

void gkbd_setlastchars(const char* chars, int len)
{
    if(len > (int)sizeof(gkbd_last.buf) - 1)
        len = (int)sizeof(gkbd_last.buf) - 1;
    if(len < 0)
        len = 0;

    gkbd_last.len = len;
    memcpy(gkbd_last.buf, chars, len);
    gkbd_last.buf[len] = NUL;
}


//  ------------------------------------------------------------------

static void gkbd_setlastcp(uint32_t cp, bool islocalwide = false)
{
    gkbd_last.len = 0;

    if(cp == 0)
        return;

#if defined(__USE_WIDE_NCURSES__)
    if(islocalwide and not g_utf8_mode())
    {
        int byte = wctob((wint_t)cp);
        if(byte != EOF)
        {
            gkbd_last.buf[0] = (char)byte;
            gkbd_last.buf[1] = NUL;
            gkbd_last.len    = 1;
            return;
        }
    }
#else
    (void)islocalwide;
#endif

    //  The keyboard reports a codepoint; text is held in the local
    //  charset. This is the same conversion a message goes through on
    //  its way in, so typing a character and receiving one give the
    //  same result - a charset without an exact byte for it gets the
    //  converter's approximation rather than a bare question mark.
    std::string enc = g_local_from_unicode(cp);

    gkbd_last.len = (int)enc.length();
    if(gkbd_last.len > (int)sizeof(gkbd_last.buf) - 1)
        gkbd_last.len = (int)sizeof(gkbd_last.buf) - 1;

    memcpy(gkbd_last.buf, enc.data(), gkbd_last.len);
    gkbd_last.buf[gkbd_last.len] = NUL;
}


//  ------------------------------------------------------------------
//  Keyboard Class Initializer

void GKbd::Init()
{

#if defined(__USE_NCURSES__)

    // Both screen and keyboard must be initialized at once
    if(0 == (curses_initialized++))
    {
        setlocale(LC_ALL, "");
        initscr();
        raw();
        noecho();
        nonl();
        intrflush(stdscr, FALSE);
        keypad(stdscr, TRUE);
    }

    // WARNING: this might break with an old version of ncurses, or
    // with another implementation of curses. I'm putting it here because
    // it is quote useful most of the time :-) For other implementations of
    // curses, you might have to compile curses yourself to achieve this.  -jt
#if defined(NCURSES_VERSION)
    if(not getenv("ESCDELAY")) // If not specified by user via environment, set
    {
        // ms, slow for a 300bps terminal, fast for humans :-)
        //
        //  ESCDELAY is a plain variable in some builds of ncurses and a
        //  macro reading a function in others - the reentrant ones, and
        //  the DLL that Cygwin ships - where assigning to it does not
        //  compile. set_escdelay() is the supported way, and has been
        //  there since 5.9.
  #if defined(NCURSES_VERSION_MAJOR) && \
      ((NCURSES_VERSION_MAJOR > 5) || \
       (NCURSES_VERSION_MAJOR == 5 && NCURSES_VERSION_MINOR >= 9))
        set_escdelay(50);
  #else
        ESCDELAY = 50;
  #endif
    }
#endif
    // For more ncurses-dependent code, look at the gkbd_curstable array
    // and at the kbxget_raw() function  -jt

    //  Moved here from the constructor along with initscr() above: all
    //  of it wants curses to be up, and none of it may run before the
    //  program has decided it wants the screen.
#ifdef HAVE_EXTENDED_NAMES
    use_extended_names(TRUE);
#endif

    for(modifier_t m = modifier(MOD_MIN); m <= modifier(MOD_MAX); m++)
    {
        for(arrow_t a = ARR_UP; a <= ARR_LFT; a++)
        {
            gkbd_setarrow(m, a, L_KEY_UNUSED);
        }
    }

    gkbd_setarrow(modifier(MOD_ALT),  ARR_UP,  L_KEY_AUP);
    gkbd_setarrow(modifier(MOD_ALT),  ARR_DN,  L_KEY_ADOWN);
    gkbd_setarrow(modifier(MOD_ALT),  ARR_RIT, L_KEY_ARIGHT);
    gkbd_setarrow(modifier(MOD_ALT),  ARR_LFT, L_KEY_ALEFT);

    gkbd_setarrow(modifier(MOD_CTRL), ARR_UP,  L_KEY_CUP);
    gkbd_setarrow(modifier(MOD_CTRL), ARR_DN,  L_KEY_CDOWN);
    gkbd_setarrow(modifier(MOD_CTRL), ARR_RIT, L_KEY_CRIGHT);
    gkbd_setarrow(modifier(MOD_CTRL), ARR_LFT, L_KEY_CLEFT);

    gkbd_setfnkeys();

#elif defined(__OS2__)

    KBDINFO kbstInfo;
    kbstInfo.cb = sizeof(kbstInfo);
    KbdGetStatus(&kbstInfo, 0);
    kbstInfo.fsMask = (USHORT)((kbstInfo.fsMask & 0xFFF7) | 0x0004);
    KbdSetStatus(&kbstInfo, 0);

#elif defined(__WIN32__)

    OSVERSIONINFO osversion;
    osversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osversion);
    gkbd_nt = make_bool(osversion.dwPlatformId & VER_PLATFORM_WIN32_NT);
    gkbd_hin = CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                          OPEN_EXISTING, 0, NULL);
    GetConsoleMode(gkbd_hin, &gkbd_kbdmode);
    if(gkbd_kbdmode & KBD_TEXTMODE)
        //  Window events stay on: WINDOW_BUFFER_SIZE_EVENT is how the
        //  console says it was resized.
        SetConsoleMode(gkbd_hin, (gkbd_kbdmode & ~KBD_TEXTMODE) | ENABLE_WINDOW_INPUT);

#elif defined(__UNIX__)

    gkbd_tty_init();

#endif
}


//  ------------------------------------------------------------------
//  Whether a resize the input layer reports is one: the size the screen
//  has now against the size we hold. A console sends its buffer-size
//  event without the window having changed - at startup, say - and a
//  KEY_RESIZE can follow a SIGWINCH for a window the same size as
//  before. Handed on, such an event cut the area list short as if
//  abandoned, and GoldED walked into the first area instead of showing
//  the list.

//  Answered by the screen layer, gvidbase.cpp; declared here rather
//  than through gvidall.h, whose vchar collides with the one BeOS's
//  SupportDefs.h declares in this file.
bool gvid_size_changed();

static bool gkbd_resize_real()
{
    return gvid_size_changed();
}


//  ------------------------------------------------------------------
//  Keyboard Class constructor

GKbd::GKbd()
{

    kbuf = NULL;
    onkey  = NULL;
    curronkey = NULL;
    inmenu = 0;
    source = 0;
    polling = 0;
    tickinterval = 0;
    tickpress = tickvalue = gclock();
    tickfunc = NULL;
    inidle = 0;
    quitall = NO;
    resize_pending = false;

    // Detect enhanced keyboard by checking bit 4 at 0x00000496
#if defined(__USE_NCURSES__)
    extkbd = true;
#elif defined(__DJGPP__)
    extkbd = _farpeekb (_dos_ds, 0x0496) & (1 << 4);
#elif defined(__BORLANDC__) && defined(__DPMI32__)
    //  The BIOS data area is out of reach here: the extender maps the
    //  first megabyte nowhere the program can address, and peekb() wants
    //  a protected-mode selector rather than a real-mode segment, so
    //  neither a plain pointer nor peekb(0x0000, ...) will do. Anything
    //  running a 386 DOS extender has an enhanced keyboard, which is the
    //  same assumption the Win32 and OS/2 builds make.
    extkbd = true;
#elif defined(__MSDOS__)
    extkbd = *((byte*)0x0496) & (1 << 4);
#elif defined(__OS2__) || defined(__WIN32__)
    extkbd = true;
#endif

#if defined(__USE_NCURSES__)

    //  Not from here. This object is a global, so its constructor runs
    //  before main(), and under curses Init() calls initscr() - which
    //  takes the terminal over and switches to the alternate screen
    //  before the program has read its own command line. Everything
    //  printed after that went onto a screen the user never sees and
    //  vanished at exit: the startup banner, the -H help, warnings
    //  about the command line. With a TERM curses does not know it was
    //  worse, since initscr() ends the process where it stands.
    //
    //  Initialize() calls Init() at the point the screen is really
    //  wanted. Every other platform keeps the old behaviour, where the
    //  keyboard setup touches no terminal state and costs nothing.

#else

    Init();

#endif
#if defined(__UNIX__) && !defined(__USE_NCURSES__) && !defined(__BEOS__)

    gkbd_keymap_init();

    char escseq[2];
    escseq[1] = NUL;
    for(int n=0; n<256; n++)
    {
        escseq[0] = (char)n;
        if(n == 0x7F or n == 0x08)
            gkbd_define_keysym(escseq, Key_BS);
        else if(n == 0x09)
            gkbd_define_keysym(escseq, Key_Tab);
        else if(n == 0x0D)
            gkbd_define_keysym(escseq, Key_Ent);
        else
            gkbd_define_keysym(escseq, (n < 128) ? (scancode_table[n]|n) : n);
    }

    gkbd_define_keysym("^@", 0);

    gkbd_define_keysym("\033[A", Key_Up);
    gkbd_define_keysym("\033[B", Key_Dwn);
    gkbd_define_keysym("\033[C", Key_Rgt);
    gkbd_define_keysym("\033[D", Key_Lft);

    gkbd_define_keysym("\033[[W", Key_C_Up);
    gkbd_define_keysym("\033[[Z", Key_C_Dwn);
    gkbd_define_keysym("\033[[Y", Key_C_Rgt);
    gkbd_define_keysym("\033[[X", Key_C_Lft);

    gkbd_define_keysym("\033[1~", Key_Home);
    gkbd_define_keysym("\033[7~", Key_Home);
    gkbd_define_keysym("\033[H",  Key_Home);
    gkbd_define_keysym("\033[2~", Key_Ins);
    gkbd_define_keysym("\033[3~", Key_Del);
    gkbd_define_keysym("\033[4~", Key_End);
    gkbd_define_keysym("\033[8~", Key_End);
    gkbd_define_keysym("\033[F",  Key_End);
    gkbd_define_keysym("\033[5~", Key_PgUp);
    gkbd_define_keysym("\033[6~", Key_PgDn);

    gkbd_define_keysym("\033[[A", Key_F1);
    gkbd_define_keysym("\033[[B", Key_F2);
    gkbd_define_keysym("\033[[C", Key_F3);
    gkbd_define_keysym("\033[[D", Key_F4);
    gkbd_define_keysym("\033[[E", Key_F5);
    gkbd_define_keysym("\033[17~", Key_F6);
    gkbd_define_keysym("\033[18~", Key_F7);
    gkbd_define_keysym("\033[19~", Key_F8);
    gkbd_define_keysym("\033[20~", Key_F9);
    gkbd_define_keysym("\033[21~", Key_F10);

    gkbd_define_keysym("\033[23~", Key_S_F1);
    gkbd_define_keysym("\033[24~", Key_S_F2);
    gkbd_define_keysym("\033[25~", Key_S_F3);
    gkbd_define_keysym("\033[26~", Key_S_F4);
    gkbd_define_keysym("\033[28~", Key_S_F5);
    gkbd_define_keysym("\033[29~", Key_S_F6);
    gkbd_define_keysym("\033[31~", Key_S_F7);
    gkbd_define_keysym("\033[32~", Key_S_F8);
    gkbd_define_keysym("\033[33~", Key_S_F9);
    gkbd_define_keysym("\033[34~", Key_S_F10);

    gkbd_define_keysym("\033""0", Key_A_0);
    gkbd_define_keysym("\033""1", Key_A_1);
    gkbd_define_keysym("\033""2", Key_A_2);
    gkbd_define_keysym("\033""3", Key_A_3);
    gkbd_define_keysym("\033""4", Key_A_4);
    gkbd_define_keysym("\033""5", Key_A_5);
    gkbd_define_keysym("\033""6", Key_A_6);
    gkbd_define_keysym("\033""7", Key_A_7);
    gkbd_define_keysym("\033""8", Key_A_8);
    gkbd_define_keysym("\033""9", Key_A_9);

    gkbd_define_keysym("\033a", Key_A_A);
    gkbd_define_keysym("\033b", Key_A_B);
    gkbd_define_keysym("\033c", Key_A_C);
    gkbd_define_keysym("\033d", Key_A_D);
    gkbd_define_keysym("\033e", Key_A_E);
    gkbd_define_keysym("\033f", Key_A_F);
    gkbd_define_keysym("\033g", Key_A_G);
    gkbd_define_keysym("\033h", Key_A_H);
    gkbd_define_keysym("\033i", Key_A_I);
    gkbd_define_keysym("\033j", Key_A_J);
    gkbd_define_keysym("\033k", Key_A_K);
    gkbd_define_keysym("\033l", Key_A_L);
    gkbd_define_keysym("\033m", Key_A_M);
    gkbd_define_keysym("\033n", Key_A_N);
    gkbd_define_keysym("\033o", Key_A_O);
    gkbd_define_keysym("\033p", Key_A_P);
    gkbd_define_keysym("\033q", Key_A_Q);
    gkbd_define_keysym("\033r", Key_A_R);
    gkbd_define_keysym("\033s", Key_A_S);
    gkbd_define_keysym("\033t", Key_A_T);
    gkbd_define_keysym("\033u", Key_A_U);
    gkbd_define_keysym("\033v", Key_A_V);
    gkbd_define_keysym("\033w", Key_A_W);
    gkbd_define_keysym("\033x", Key_A_X);
    gkbd_define_keysym("\033y", Key_A_Y);
    gkbd_define_keysym("\033z", Key_A_Z);

    gkbd_define_keysym("^?", Key_BS);
    gkbd_define_keysym("\033\x7F", Key_A_BS);
    gkbd_define_keysym("\033\x0D", Key_A_Ent);
    gkbd_define_keysym("\033\x09", Key_A_Tab);

//  Only when curses is not driving the keyboard: this branch is the
//  BeOS stand-in for the raw-terminal path above, and gkbd_keymap_init()
//  and gkbd_define_keysym() do not even exist in an ncurses build. Haiku
//  has ncurses and takes that route instead.
#elif defined(__BEOS__) && !defined(__USE_NCURSES__)

    gkbd_keymap_init();

    char escseq[2];
    escseq[1] = NUL;
    for(int n=0; n<256; n++)
    {
        escseq[0] = (char)n;
        if(n == 0x08)
            gkbd_define_keysym(escseq, Key_BS);
        else if(n == 0x09)
            gkbd_define_keysym(escseq, Key_Tab);
        else if(n == 0x0D)
            gkbd_define_keysym(escseq, Key_Ent);
        else if(n == 0x7F)
            gkbd_define_keysym(escseq, Key_Del); // ?????
        else
            gkbd_define_keysym(escseq, (n < 128) ? (scancode_table[n]|n) : n);
    }

    // gkbd_define_keysym("^@", 0); ?????????

    gkbd_define_keysym("\033[A", Key_Up);
    gkbd_define_keysym("\033[B", Key_Dwn);
    gkbd_define_keysym("\033[C", Key_Rgt);
    gkbd_define_keysym("\033[D", Key_Lft);

    gkbd_define_keysym("\033[1~", Key_Home);
    gkbd_define_keysym("\033[2~", Key_Ins);
    gkbd_define_keysym("\033[4~", Key_End);
    gkbd_define_keysym("\033[5~", Key_PgUp);
    gkbd_define_keysym("\033[6~", Key_PgDn);

    // gkbd_define_keysym("\033[3~", Key_Del);

    gkbd_define_keysym("\033[11~", Key_F1);
    gkbd_define_keysym("\033[12~", Key_F2);
    gkbd_define_keysym("\033[13~", Key_F3);
    gkbd_define_keysym("\033[14~", Key_F4);
    gkbd_define_keysym("\033[15~", Key_F5);
    gkbd_define_keysym("\033[16~", Key_F6);
    gkbd_define_keysym("\033[17~", Key_F7);
    gkbd_define_keysym("\033[18~", Key_F8);
    gkbd_define_keysym("\033[19~", Key_F9);
    gkbd_define_keysym("\033[20~", Key_F10);

    /*
      gkbd_define_keysym("\033\x7F", Key_A_BS);
      gkbd_define_keysym("\033\x0D", Key_A_Ent);
      gkbd_define_keysym("\033\x09", Key_A_Tab);
    */

#endif
}


//  ------------------------------------------------------------------
//  Keyboard Class destructor

GKbd::~GKbd()
{

#if defined(__USE_NCURSES__)

    if(0 == (--curses_initialized))
        endwin();

#elif defined(__WIN32__)

    if(gkbd_kbdmode & KBD_TEXTMODE)
        SetConsoleMode(gkbd_hin, gkbd_kbdmode);

#elif defined(__UNIX__)

    gkbd_keymap_reset();
    gkbd_tty_reset();

#endif
}


//  ------------------------------------------------------------------
//  Local table for scancode()

gkey scancode_table[] =
{

    Key_C_2   & 0xFF00u,     //  0x0300          C <2 @>     [NUL]
    Key_C_A   & 0xFF00u,     //  0x1E01          C <A>       [SOH]
    Key_C_B   & 0xFF00u,     //  0x3002          C <B>       [STX]
    Key_C_C   & 0xFF00u,     //  0x2E03          C <C>       [ETX]
    Key_C_D   & 0xFF00u,     //  0x2004          C <D>       [EOT]
    Key_C_E   & 0xFF00u,     //  0x1205          C <E>       [ENQ]
    Key_C_F   & 0xFF00u,     //  0x2106          C <F>       [ACK]
    Key_C_G   & 0xFF00u,     //  0x2207          C <G>       [BEL]
    Key_C_H   & 0xFF00u,     //  0x2308          C <H>       [BS]
    Key_C_I   & 0xFF00u,     //  0x1709          C <I>       [HT]
    Key_C_J   & 0xFF00u,     //  0x240A          C <J>       [LF]
    Key_C_K   & 0xFF00u,     //  0x250B          C <K>       [VT]
    Key_C_L   & 0xFF00u,     //  0x260C          C <L>       [FF]
    Key_C_M   & 0xFF00u,     //  0x320D          C <M>       [CR]
    Key_C_N   & 0xFF00u,     //  0x310E          C <N>       [SO]
    Key_C_O   & 0xFF00u,     //  0x180F          C <O>       [SI]
    Key_C_P   & 0xFF00u,     //  0x1910          C <P>       [DLE]
    Key_C_Q   & 0xFF00u,     //  0x1011          C <Q>       [DC1]
    Key_C_R   & 0xFF00u,     //  0x1312          C <R>       [DC2]
    Key_C_S   & 0xFF00u,     //  0x1F13          C <S>       [DC3]
    Key_C_T   & 0xFF00u,     //  0x1414          C <T>       [DC4]
    Key_C_U   & 0xFF00u,     //  0x1615          C <U>       [NAK]
    Key_C_V   & 0xFF00u,     //  0x2F16          C <V>       [SYN]
    Key_C_W   & 0xFF00u,     //  0x1117          C <W>       [ETB]
    Key_C_X   & 0xFF00u,     //  0x2D18          C <X>       [CAN]
    Key_C_Y   & 0xFF00u,     //  0x1519          C <Y>       [EM]
    Key_C_Z   & 0xFF00u,     //  0x2C1A          C <Z>       [SUB]
    Key_Esc   & 0xFF00u,     //  0x011B          C <[ {>     [ESC] (was: 0x1A1B)
    Key_C_Bsl & 0xFF00u,     //  0x2B1C          C <\ |>     [FS]
    Key_C_Rbr & 0xFF00u,     //  0x1B1D          C <] }>     [GS]
    Key_C_6   & 0xFF00u,     //  0x071E          C <7 &>     [RS]
    Key_C_Min & 0xFF00u,     //  0x0C1F          C <- _>
    Key_Space & 0xFF00u,     //  0x3920            <Space>
    Key_S_1   & 0xFF00u,     //  0x0221          <1 !>
    Key_S_Quo & 0xFF00u,     //  0x2822          <' ">
    Key_S_3   & 0xFF00u,     //  0x0423          <3 #>
    Key_S_4   & 0xFF00u,     //  0x0524          <4 $>
    Key_S_5   & 0xFF00u,     //  0x0625          <5 %>
    Key_S_7   & 0xFF00u,     //  0x0826          <7 &>
    Key_Quo   & 0xFF00u,     //  0x2827          <'>
    Key_S_9   & 0xFF00u,     //  0x0A28          <9 (>
    Key_S_0   & 0xFF00u,     //  0x0B29          <0 )>
    Key_S_8   & 0xFF00u,     //  0x092A          <8 *>
    Key_S_Equ & 0xFF00u,     //  0x0D2B          <= +>
    Key_Com   & 0xFF00u,     //  0x332C            <,>
    Key_Min   & 0xFF00u,     //  0x0C2D          <->
    Key_Dot   & 0xFF00u,     //  0x342E            <.>
    Key_Sls   & 0xFF00u,     //  0x352F          </>
    Key_0     & 0xFF00u,     //  0x0B30          <0>
    Key_1     & 0xFF00u,     //  0x0231          <1>
    Key_2     & 0xFF00u,     //  0x0332          <2>
    Key_3     & 0xFF00u,     //  0x0433          <3>
    Key_4     & 0xFF00u,     //  0x0534          <4>
    Key_5     & 0xFF00u,     //  0x0635          <5>
    Key_6     & 0xFF00u,     //  0x0736          <6>
    Key_7     & 0xFF00u,     //  0x0837          <7>
    Key_8     & 0xFF00u,     //  0x0938          <8>
    Key_9     & 0xFF00u,     //  0x0A39          <9>
    Key_S_Smi & 0xFF00u,     //  0x273A          <; :>
    Key_Smi   & 0xFF00u,     //  0x273B          <;>
    Key_S_Com & 0xFF00u,     //  0x333C          <, >>
    Key_Equ   & 0xFF00u,     //  0x0D3D          <=>
    Key_S_Dot & 0xFF00u,     //  0x343E          <. <>
    Key_S_Sls & 0xFF00u,     //  0x353F          </ ?>
    Key_S_2   & 0xFF00u,     //  0x0340          <2 @>
    Key_S_A   & 0xFF00u,     //  0x1E41          <A>
    Key_S_B   & 0xFF00u,     //  0x3042          <B>
    Key_S_C   & 0xFF00u,     //  0x2E43          <C>
    Key_S_D   & 0xFF00u,     //  0x2044          <D>
    Key_S_E   & 0xFF00u,     //  0x1245          <E>
    Key_S_F   & 0xFF00u,     //  0x2146          <F>
    Key_S_G   & 0xFF00u,     //  0x2247          <G>
    Key_S_H   & 0xFF00u,     //  0x2348          <H>
    Key_S_I   & 0xFF00u,     //  0x1749          <I>
    Key_S_J   & 0xFF00u,     //  0x244A          <J>
    Key_S_K   & 0xFF00u,     //  0x254B          <K>
    Key_S_L   & 0xFF00u,     //  0x264C          <L>
    Key_S_M   & 0xFF00u,     //  0x324D          <M>
    Key_S_N   & 0xFF00u,     //  0x314E          <N>
    Key_S_O   & 0xFF00u,     //  0x184F          <O>
    Key_S_P   & 0xFF00u,     //  0x1950          <P>
    Key_S_Q   & 0xFF00u,     //  0x1051          <Q>
    Key_S_R   & 0xFF00u,     //  0x1352          <R>
    Key_S_S   & 0xFF00u,     //  0x1F53          <S>
    Key_S_T   & 0xFF00u,     //  0x1454          <T>
    Key_S_U   & 0xFF00u,     //  0x1655          <U>
    Key_S_V   & 0xFF00u,     //  0x2F56          <V>
    Key_S_W   & 0xFF00u,     //  0x1157          <W>
    Key_S_X   & 0xFF00u,     //  0x2D58          <X>
    Key_S_Y   & 0xFF00u,     //  0x1559          <Y>
    Key_S_Z   & 0xFF00u,     //  0x2C5A          <Z>
    Key_Lbr   & 0xFF00u,     //  0x1A5B          <[>
    Key_Bsl   & 0xFF00u,     //  0x2B5C          <\>
    Key_Rbr   & 0xFF00u,     //  0x1B5D          <]>
    Key_S_6   & 0xFF00u,     //  0x075E          <6 ^>
    Key_S_Min & 0xFF00u,     //  0x0C5F          <- _>
    Key_Grv   & 0xFF00u,     //  0x2960          <`>
    Key_A     & 0xFF00u,     //  0x1E61          <a>
    Key_B     & 0xFF00u,     //  0x3062          <b>
    Key_C     & 0xFF00u,     //  0x2E63          <c>
    Key_D     & 0xFF00u,     //  0x2064          <d>
    Key_E     & 0xFF00u,     //  0x1265          <e>
    Key_F     & 0xFF00u,     //  0x2166          <f>
    Key_G     & 0xFF00u,     //  0x2267          <g>
    Key_H     & 0xFF00u,     //  0x2368          <h>
    Key_I     & 0xFF00u,     //  0x1769          <i>
    Key_J     & 0xFF00u,     //  0x246A          <j>
    Key_K     & 0xFF00u,     //  0x256B          <k>
    Key_L     & 0xFF00u,     //  0x266C          <l>
    Key_M     & 0xFF00u,     //  0x326D          <m>
    Key_N     & 0xFF00u,     //  0x316E          <n>
    Key_O     & 0xFF00u,     //  0x186F          <o>
    Key_P     & 0xFF00u,     //  0x1970          <p>
    Key_Q     & 0xFF00u,     //  0x1071          <q>
    Key_R     & 0xFF00u,     //  0x1372          <r>
    Key_S     & 0xFF00u,     //  0x1F73          <s>
    Key_T     & 0xFF00u,     //  0x1474          <t>
    Key_U     & 0xFF00u,     //  0x1675          <u>
    Key_V     & 0xFF00u,     //  0x2F76          <v>
    Key_W     & 0xFF00u,     //  0x1177          <w>
    Key_X     & 0xFF00u,     //  0x2D78          <x>
    Key_Y     & 0xFF00u,     //  0x1579          <y>
    Key_Z     & 0xFF00u,     //  0x2C7A          <z>
    Key_S_Lbr & 0xFF00u,     //  0x1A7B          <[ {>
    Key_S_Bsl & 0xFF00u,     //  0x2B7C          <\ |>
    Key_S_Rbr & 0xFF00u,     //  0x1B7D          <] }>
    Key_S_Grv & 0xFF00u,     //  0x297E          <` ~>
    Key_C_BS  & 0xFF00u      //  0x0E7F          C <BS>      [RUB]
};


//  ------------------------------------------------------------------
//  Returns the scan code of an ASCII character

byte scancode(gkey ch)
{

    if(KCodAsc(ch) <= 127)
        return (byte)(scancode_table[KCodAsc(ch)] >> 8);
    return 0;
}


//  ------------------------------------------------------------------
//  Translate scancode for ASCII keys

gkey keyscanxlat(gkey k)
{

    // Only translate ASCII keys
    if(KCodAsc(k))
    {

        // Check for certain ctrl-keys
        switch(KCodAsc(k))
        {

        case 0x08:  // CtrlH or BackSpace                     23/0E
            if(k == Key_BS)
                return k;
#if defined(__USE_NCURSES__)
            //  See the note under 0x7F: where the terminal erases with
            //  DEL, a bare ^H is what Ctrl-Backspace sends.
            if(erasechar() == 0x7F)
                return Key_C_BS;
#endif
            break;

#if defined(__USE_NCURSES__)
        case 0x7F:  // DEL
            //  Which byte the Backspace key sends is the terminal's
            //  business, and it says so: erasechar() reports what
            //  `stty erase' is set to. On nearly every terminal today
            //  that is DEL, not ^H.
            //
            //  The scancode table maps 0x7F to Ctrl-Backspace, which
            //  is only right for a terminal that erases with ^H. On the
            //  usual one it made the plain Backspace key delete a whole
            //  word - and in a one-word input field that looks exactly
            //  like the field being wiped.
            if(erasechar() == 0x7F)
                return Key_BS;
            break;
#endif

        case 0x09:  // CtrlI or Tab                           17/0F
            if(k == Key_Tab)
                return k;
            else
                break;

        case 0x0A:  // CtrlJ or CtrlEnter or GreyCtrlEnter    24/1C/E0
        case 0x0D:  // CtrlM or Enter or GreyEnter            32/1C/E0
            // First, translate Numpad-Enter to main Enter...
            if(k == Key_EntG)
                k = Key_Ent;
            else if(k == Key_C_EntG)
                k = Key_C_Ent;
            else if(k == Key_A_EntG)
                k = Key_A_Ent;
            // ...and now return if main Enter
            if((k == Key_Ent) or (k == Key_C_Ent) or (k == Key_A_Ent))
                return k;
            else
                break;

        case 0x1B:  // Ctrl[ or Esc                           1A/01
            if(k == Key_Esc)
                return k;
            else
                break;

        // asa: Not sure that the following case is required:
        // Key_S_3 == 0x0423, Key_C_U == 0x1615
        case 0x15: // CtrlU or Shift3 (on german keyboards)   16/04
            if(KCodScn(k) == 0x04)
                return k;
            break;

        case 0xE0: // Check for extended key and fix it if necessary
            if(KCodScn(k))
            {
                KCodAsc(k) = 0x00;
                return k;
            }
            break;
        }

        // Translate scancode of ASCII key to a known value
        if (KCodAsc(k) <= 127)
            return (gkey)(scancode_table[KCodAsc(k)] | KCodAsc(k));
        else
            return (gkey)(KCodAsc(k));
    }

    return k;
}


//  ------------------------------------------------------------------
//  The following tables map curses keyboard codes to BIOS keyboard
//  values.

#if defined(__USE_NCURSES__)

// This might not work with something other than ncurses... :-(
// If you ever port it to other curses implementation, remember
// that it might have to be changed to another data structure, or
// the array might have to be filled in another manner...

int gkbd_curstable[] =
{
    Key_C_Brk, //  KEY_BREAK
    Key_Dwn,   //  KEY_DOWN
    Key_Up,    //  KEY_UP
    Key_Lft,   //  KEY_LEFT
    Key_Rgt,   //  KEY_RIGHT
    Key_Home,  //  KEY_HOME
    Key_BS,    //  KEY_BACKSPACE
    -1,        //  KEY_F0
    Key_F1,    //  KEY_F(1)
    Key_F2,    //  KEY_F(2)
    Key_F3,    //  KEY_F(3)
    Key_F4,    //  KEY_F(4)
    Key_F5,    //  KEY_F(5)
    Key_F6,    //  KEY_F(6)
    Key_F7,    //  KEY_F(7)
    Key_F8,    //  KEY_F(8)
    Key_F9,    //  KEY_F(9)
    Key_F10,   //  KEY_F(10)
    Key_F11,   //  KEY_F(11)
    Key_F12,   //  KEY_F(12)
    Key_S_F3,  //  KEY_F(13)
    Key_S_F4,  //  KEY_F(14)
    Key_S_F5,  //  KEY_F(15)
    Key_S_F6,  //  KEY_F(16)
    Key_S_F7,  //  KEY_F(17)
    Key_S_F8,  //  KEY_F(18)
    Key_S_F9,  //  KEY_F(19)
    Key_S_F10, //  KEY_F(20)
    Key_S_F11, //  KEY_F(21)
    Key_S_F12, //  KEY_F(22)
    -1,        //  KEY_F(23)
    -1,        //  KEY_F(24)
    -1,        //  KEY_F(25)
    -1,        //  KEY_F(26)
    -1,        //  KEY_F(27)
    -1,        //  KEY_F(28)
    -1,        //  KEY_F(29)
    -1,        //  KEY_F(30)
    -1,        //  KEY_F(31)
    -1,        //  KEY_F(32)
    -1,        //  KEY_F(33)
    -1,        //  KEY_F(34)
    -1,        //  KEY_F(35)
    -1,        //  KEY_F(36)
    -1,        //  KEY_F(37)
    -1,        //  KEY_F(38)
    -1,        //  KEY_F(39)
    -1,        //  KEY_F(40)
    -1,        //  KEY_F(41)
    -1,        //  KEY_F(42)
    -1,        //  KEY_F(43)
    -1,        //  KEY_F(44)
    -1,        //  KEY_F(45)
    -1,        //  KEY_F(46)
    -1,        //  KEY_F(47)
    -1,        //  KEY_F(48)
    -1,        //  KEY_F(49)
    -1,        //  KEY_F(50)
    -1,        //  KEY_F(51)
    -1,        //  KEY_F(52)
    -1,        //  KEY_F(53)
    -1,        //  KEY_F(54)
    -1,        //  KEY_F(55)
    -1,        //  KEY_F(56)
    -1,        //  KEY_F(57)
    -1,        //  KEY_F(58)
    -1,        //  KEY_F(59)
    -1,        //  KEY_F(60)
    -1,        //  KEY_F(61)
    -1,        //  KEY_F(62)
    -1,        //  KEY_F(63)
    -1,        //  KEY_DL
    -1,        //  KEY_IL
    Key_Del,   //  KEY_DC
    Key_Ins,   //  KEY_IC
    Key_Ins,   //  KEY_EIC
    -1,        //  KEY_CLEAR
    -1,        //  KEY_EOS
    -1,        //  KEY_EOL
    Key_S_Dwn, //  KEY_SF - terminfo Scroll forward == <Shift>+<Down arrow>
    Key_S_Up,  //  KEY_SR - terminfo Scroll backward == <Shift>+<Up arrow>
    Key_PgDn,  //  KEY_NPAGE
    Key_PgUp,  //  KEY_PPAGE
    Key_Tab,   //  KEY_STAB
    -1,        //  KEY_CTAB
    -1,        //  KEY_CATAB
    Key_Ent,   //  KEY_ENTER
    -1,        //  KEY_SRESET
    -1,        //  KEY_RESET
    -1,        //  KEY_PRINT
    Key_End,   //  KEY_LL (hmm... this should be lower left)
    Key_Home,  //  KEY_A1
    Key_PgUp,  //  KEY_A3
    Key_5Num,  //  KEY_B2
    Key_End,   //  KEY_C1
    Key_PgDn,  //  KEY_C3
    Key_S_Tab, //  KEY_BTAB
    Key_Home,  //  KEY_BEG
    -1,        //  KEY_CANCEL
    -1,        //  KEY_CLOSE
    -1,        //  KEY_COMMAND
    -1,        //  KEY_COPY
    -1,        //  KEY_CREATE
    Key_End,   //  KEY_END
    -1,        //  KEY_EXIT
    -1,        //  KEY_FIND
    -1,        //  KEY_HELP
    -1,        //  KEY_MARK
    -1,        //  KEY_MESSAGE
    -1,        //  KEY_MOVE
    Key_PgDn,  //  KEY_NEXT
    -1,        //  KEY_OPEN
    -1,        //  KEY_OPTIONS
    Key_PgUp,  //  KEY_PREVIOUS
    -1,        //  KEY_REDO
    -1,        //  KEY_REFERENCE
    -1,        //  KEY_REFRESH
    -1,        //  KEY_REPLACE
    -1,        //  KEY_RESTART
    -1,        //  KEY_RESUME
    -1,        //  KEY_SAVE
    Key_S_Home,//  KEY_SBEG
    -1,        //  KEY_SCANCEL
    -1,        //  KEY_SCOMMAND
    -1,        //  KEY_SCOPY
    -1,        //  KEY_SCREATE
    Key_S_Del, //  KEY_SDC
    -1,        //  KEY_SDL
    -1,        //  KEY_SELECT
    Key_S_End, //  KEY_SEND
    -1,        //  KEY_SEOL
    -1,        //  KEY_SEXIT
    -1,        //  KEY_SFIND
    -1,        //  KEY_SHELP
    Key_S_Home,//  KEY_SHOME
    Key_S_Ins, //  KEY_SIC
    Key_S_Lft, //  KEY_SLEFT
    -1,        //  KEY_SMESSAGE
    -1,        //  KEY_SMOVE
    Key_S_PgDn,//  KEY_SNEXT
    -1,        //  KEY_SOPTIONS
    Key_S_PgUp,//  KEY_SPREVIOUS
    -1,        //  KEY_SPRINT
    -1,        //  KEY_SREDO
    -1,        //  KEY_SREPLACE
    Key_S_Rgt, //  KEY_SRIGHT
    -1,        //  KEY_SRSUME
    -1,        //  KEY_SSAVE
    -1,        //  KEY_SSUSPEND
    -1,        //  KEY_SUNDO
    -1,        //  KEY_SUSPEND
    -1,        //  KEY_UNDO
    -1,        //  KEY_MOUSE
    Key_Resize, // KEY_RESIZE - curses has already called resizeterm()
    -1,	     //  KEY_EVENT
    // Gap for future curses versions
    -1,	     //
    -1,	     //
    -1,	     //
    -1,	     //
    -1,	     //
    -1,	     //
    -1,	     //
    -1,	     //
    -1,	     //
    -1,	     //
    // xterm R7/DECFNK
    Key_A_Up,  //  L_KEY_AUP
    Key_A_Dwn, //  L_KEY_ADOWN
    Key_A_Rgt, //  L_KEY_ARIGHT
    Key_A_Lft, //  L_KEY_ALEFT
    Key_C_Up,  //  L_KEY_CUP
    Key_C_Dwn, //  L_KEY_CDOWN
    Key_C_Rgt, //  L_KEY_CRIGHT
    Key_C_Lft, //  L_KEY_CLEFT
    -1         //  L_KEY_UNUSED
};

void gkbd_setarrow(modifier_t m, arrow_t a, int k)
{
#ifdef HAVE_EXTENDED_NAMES
    // Modern terminfo have user-defined capabilities for modifier
    // arrow

    static const char * const enm[] = { "UP", "DN", "RIT", "LFT" };
    char udcapname[8];

    snprintf(udcapname, sizeof(udcapname), "k%s%d", enm[a], m);

    char *tc = tigetstr(udcapname);

    if(tc && tc != (char *)-1)
    {
        define_key(tc, k);
    }
#endif
#ifndef NO_MOD_DECFNK_XTERM_R7
    // Need to disable <Ctrl>+<Shift>+<arrow>, etc.

    // terminfo.src 1.554 2015/10/24 16:00:04
    // Don't have any conflicts with /\\E\[1;[3-9][^,]*[ABCD]/

    char decfnk[8];

    snprintf(decfnk, sizeof(decfnk), "\033[1;%d%c", m, 'A' + a);
    define_key(decfnk, k);
#endif
#ifndef NO_MOD_RXVT
    // Conflicts with /\\EO[a-d]/ exist, check terminal name first

    if(modifier(MOD_CTRL) == m && strstr(termname(), "rxvt"))
    {
        char rxvt[4];

        snprintf(rxvt, sizeof(rxvt), "\033O%c", 'a' + a);
        define_key(rxvt, k);
    }
#endif
}

void gkbd_setfnkeys()
{
#ifndef OLD_SHIFT_FN
    static const int mod_fn12_first = KEY_F(13) - KEY_MIN;

    static const int mod_fn12_keys[] =   // Modern Fn keys
    {
        // KEY_F(13-24) - <Shift>+Fn xterm R7/Linux/FreeBSD/DECFNK
        Key_S_F1,  //  KEY_F(13)
        Key_S_F2,  //  KEY_F(14)
        Key_S_F3,  //  KEY_F(15)
        Key_S_F4,  //  KEY_F(16)
        Key_S_F5,  //  KEY_F(17)
        Key_S_F6,  //  KEY_F(18)
        Key_S_F7,  //  KEY_F(19)
        Key_S_F8,  //  KEY_F(20)
        Key_S_F9,  //  KEY_F(21)
        Key_S_F10, //  KEY_F(22)
        Key_S_F11, //  KEY_F(23)
        Key_S_F12, //  KEY_F(24)

        // KEY_F(25-36) - <Ctrl>+Fn xterm R7/DECFNK
        Key_C_F1,  //  KEY_F(25)
        Key_C_F2,  //  KEY_F(26)
        Key_C_F3,  //  KEY_F(27)
        Key_C_F4,  //  KEY_F(28)
        Key_C_F5,  //  KEY_F(29)
        Key_C_F6,  //  KEY_F(30)
        Key_C_F7,  //  KEY_F(31)
        Key_C_F8,  //  KEY_F(32)
        Key_C_F9,  //  KEY_F(33)
        Key_C_F10, //  KEY_F(34)
        Key_C_F11, //  KEY_F(35)
        Key_C_F12, //  KEY_F(36)

        // KEY_F(37-48) - <Ctrl>+<Shift>+Fn xterm R7/DECFNK
        -1,        //  KEY_F(37)
        -1,        //  KEY_F(38)
        -1,        //  KEY_F(39)
        -1,        //  KEY_F(40)
        -1,        //  KEY_F(41)
        -1,        //  KEY_F(42)
        -1,        //  KEY_F(43)
        -1,        //  KEY_F(44)
        -1,        //  KEY_F(45)
        -1,        //  KEY_F(46)
        -1,        //  KEY_F(47)
        -1,        //  KEY_F(48)

        // KEY_F(49-60) - <Alt>+Fn xterm R7/DECFNK
        Key_A_F1,  //  KEY_F(49)
        Key_A_F2,  //  KEY_F(50)
        Key_A_F3,  //  KEY_F(51)
        Key_A_F4,  //  KEY_F(52)
        Key_A_F5,  //  KEY_F(53)
        Key_A_F6,  //  KEY_F(54)
        Key_A_F7,  //  KEY_F(55)
        Key_A_F8,  //  KEY_F(56)
        Key_A_F9,  //  KEY_F(57)
        Key_A_F10, //  KEY_F(58)
        Key_A_F11, //  KEY_F(59)
        Key_A_F12, //  KEY_F(60)

        // KEY_F(61-63) - <Alt>+<Shift>+Fn xterm R7/DECFNK
        -1,        //  KEY_F(61)
        -1,        //  KEY_F(62)
        -1         //  KEY_F(63)
    };

    static const char * const ort_fn10_term[] =
    {
        "xterm-old",
        "xterm-r5",
        "xterm-r6",
        "screen.xterm-r6",
        "rxvt",
        "screen.rxvt",
        "putty",
        "screen.putty",
        "linux",
        "screen.linux",
        0
    };

    static const int ort_fn10_first = KEY_F(11) - KEY_MIN;

    static const int ort_fn10_keys[] =   // Orthodox Fn keys
    {
        // <Shift> + F1-F10 generates F11-F20
        Key_S_F1,  //  KEY_F(11)
        Key_S_F2,  //  KEY_F(12)
        Key_S_F3,  //  KEY_F(13)
        Key_S_F4,  //  KEY_F(14)
        Key_S_F5,  //  KEY_F(15)
        Key_S_F6,  //  KEY_F(16)
        Key_S_F7,  //  KEY_F(17)
        Key_S_F8,  //  KEY_F(18)
        Key_S_F9,  //  KEY_F(19)
        Key_S_F10, //  KEY_F(20)

        Key_S_F11, //  KEY_F(21)
        Key_S_F12, //  KEY_F(22)

        Key_C_F1,  //  KEY_F(23)
        Key_C_F2,  //  KEY_F(24)
        Key_C_F3,  //  KEY_F(25)
        Key_C_F4,  //  KEY_F(26)
        Key_C_F5,  //  KEY_F(27)
        Key_C_F6,  //  KEY_F(28)
        Key_C_F7,  //  KEY_F(29)
        Key_C_F8,  //  KEY_F(30)
        Key_C_F9,  //  KEY_F(31)
        Key_C_F10, //  KEY_F(32)

        // <Ctrl>+<Shift> + F1-F10 generates  <Ctrl>+F11-F20
        -1,        //  KEY_F(33)
        -1,        //  KEY_F(34)
        -1,        //  KEY_F(35)
        -1,        //  KEY_F(36)
        -1,        //  KEY_F(37)
        -1,        //  KEY_F(38)
        -1,        //  KEY_F(39)
        -1,        //  KEY_F(40)
        -1,        //  KEY_F(41)
        -1,        //  KEY_F(42)

        -1,        //  KEY_F(43)
        -1,        //  KEY_F(44)

        Key_A_F1,  //  KEY_F(45)
        Key_A_F2,  //  KEY_F(46)
        Key_A_F3,  //  KEY_F(47)
        Key_A_F4,  //  KEY_F(48)
        Key_A_F5,  //  KEY_F(49)
        Key_A_F6,  //  KEY_F(50)
        Key_A_F7,  //  KEY_F(51)
        Key_A_F8,  //  KEY_F(52)
        Key_A_F9,  //  KEY_F(53)
        Key_A_F10, //  KEY_F(54)

        // <Alt>+<Shift> + F1-F10 generates  <Alt>+F11-F20
        -1,        //  KEY_F(55)
        -1,        //  KEY_F(56)
        -1,        //  KEY_F(57)
        -1,        //  KEY_F(58)
        -1,        //  KEY_F(59)
        -1,        //  KEY_F(60)
        -1,        //  KEY_F(61)
        -1,        //  KEY_F(62)
        -1         //  KEY_F(63)
    };

    const char *term = termname();

    for(int i = 0;
            i < sizeof(mod_fn12_keys)/sizeof(mod_fn12_keys[0]);
            i++)
    {
        gkbd_curstable[mod_fn12_first+i] = mod_fn12_keys[i];
    }
    for(int j = 0; ort_fn10_term[j]; j++)
    {
        if(0 == strncmp(ort_fn10_term[j], term, strlen(ort_fn10_term[j])))
        {
            for(int i = 0;
                    i < sizeof(ort_fn10_keys)/sizeof(ort_fn10_keys[0]);
                    i++)
            {
                gkbd_curstable[ort_fn10_first+i] = ort_fn10_keys[i];
            }
        }
    }
#endif
}

//  ------------------------------------------------------------------
//  Pushing a key back.
//
//  This cannot be left to curses. ungetch() takes a byte, which is no
//  longer enough once get_wch() has consumed every byte of a multibyte
//  character; and unget_wch(), which would be the obvious answer,
//  cannot be relied on either - NetBSD's curses remembers the value but
//  forgets that it was a character, so the next get_wch() reports it
//  with KEY_CODE_YES and the caller takes a typed letter for a function
//  key. Keeping the pushback here makes it behave the same everywhere.
//
//  A stack rather than a single slot: reading an escape sequence peeks
//  two keys and can push both back, and the last one pushed has to come
//  out first.

struct GKbdPush
{
    int      key;
    GKbdChar last;
};

static GKbdPush gkbd_pushstack[8];
static int      gkbd_pushed = 0;


//  ------------------------------------------------------------------
//  Saving and restoring the record of the last typed character.
//
//  Probing for modifier keys reads ahead and pushes what it finds back;
//  without this the probe would leave the record describing the key
//  after the one the caller is about to be handed.

static void gkbd_savelast(GKbdChar& sv)
{
    sv = gkbd_last;
}


static void gkbd_restorelast(const GKbdChar& sv)
{
    gkbd_last = sv;
}


//  ------------------------------------------------------------------
//  Push a key back so that the next read sees it again.

static void gkbd_ungetch(int key)
{
    if(gkbd_pushed >= (int)ARRAYSIZE(gkbd_pushstack))
    {
        //  Nothing sensible to do but drop it; the stack is far larger
        //  than the two entries the escape-sequence reader ever needs.
        return;
    }

    GKbdPush& p = gkbd_pushstack[gkbd_pushed++];
    p.key  = key;
    p.last = gkbd_last;
}


//  ------------------------------------------------------------------
//  Take back a pushed key, restoring what was known about it.

static bool gkbd_repeek(int* key)
{
    if(gkbd_pushed <= 0)
        return false;

    const GKbdPush& p = gkbd_pushstack[--gkbd_pushed];
    *key      = p.key;
    gkbd_last = p.last;
    return true;
}


//  ------------------------------------------------------------------
//  Getting the Escape key out of NetBSD's own curses
//
//  That curses will not hand back a lone Escape. It reads the byte,
//  decides it might begin a key sequence, and keeps it - and the wait it
//  is supposed to give up after never ends. Setting ESCDELAY, blocking,
//  timing out and retrying the read all leave it stuck; the key only
//  comes out when some other byte arrives and settles the question.
//
//  It is held, though, not thrown away, and that is enough to work
//  around. select() says whether the terminal had anything to read; if
//  it did and the read produced no key, this curses is sitting on an
//  Escape. Wait ESCDELAY for a byte that would complete a real sequence,
//  and when none comes, report the Escape ourselves. The one curses is
//  still holding is then swallowed when it finally appears, so the key
//  is delivered once.

#if defined(__USE_NCURSES__) && !defined(NCURSES_VERSION)
#define GOLD_CURSES_ESC_STUCK 1
#endif

#ifdef GOLD_CURSES_ESC_STUCK

static bool   gkbd_esc_held  = false;   // curses is holding an Escape
static double gkbd_esc_since = 0.0;     // since when
static int    gkbd_esc_owed  = 0;       // Escapes reported ahead of curses

static double gkbd_now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}


static bool gkbd_input_ready()
{
    fd_set fds;
    struct timeval tv;

    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    tv.tv_sec  = 0;
    tv.tv_usec = 0;

    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

#endif


int gkbd_cursgetch(eKeyModes mode)
{

    int key;

#ifdef GOLD_CURSES_ESC_STUCK
    bool _had_input = gkbd_input_ready();
#endif
#ifndef BUGGY_NCURSES
    nodelay(stdscr, mode);
#else
    wtimeout(stdscr, (mode != KeyMode_Wait) ? 0 : -1);
#endif
    //  A key put back earlier comes out before anything new is read.
    if(gkbd_repeek(&key))
    {
#ifndef BUGGY_NCURSES
        nodelay(stdscr, FALSE);
#else
        wtimeout(stdscr, -1);
#endif
        return key;
    }

#if defined(__USE_WIDE_NCURSES__)
    {
        wint_t wch = 0;
        int rc = get_wch(&wch);

        if(rc == ERR)
        {
            key = ERR;
        }
        else if(rc == KEY_CODE_YES)
        {
            //  A function key. These are the values above KEY_MIN that
            //  the caller looks up in its translation table.
            key = (int)wch;
        }
        else
        {
            //  A typed character. Its codepoint can be far above
            //  KEY_MIN - every Cyrillic letter is - so it must not be
            //  handed back as-is, or the caller would mistake it for a
            //  function key. The codepoint goes into the side-channel
            //  and the key carries the first byte of its encoding,
            //  which is all the caller needs to see that something was
            //  typed.
            gkbd_setlastcp((uint32_t)wch, true);
            key = gkbd_last.len ? (unsigned char)gkbd_last.buf[0] : (int)wch;
        }
    }
#else
    key = getch();
#endif
#ifndef BUGGY_NCURSES
    nodelay(stdscr, FALSE);
#else
    wtimeout(stdscr, -1);
#endif

#ifdef GOLD_CURSES_ESC_STUCK
    if(key == ERR)
    {
        if(_had_input)
        {
            //  Something was there to read and nothing came of it.
            gkbd_esc_held  = true;
            gkbd_esc_since = gkbd_now();
        }
        else if(gkbd_esc_held and
                ((gkbd_now() - gkbd_esc_since) * 1000.0 >= (double)ESCDELAY))
        {
            //  Long enough: no sequence is coming.
            gkbd_esc_held = false;
            gkbd_esc_owed++;
            key = 27;
        }
    }
    else
    {
        gkbd_esc_held = false;

        if((key == 27) and gkbd_esc_owed)
        {
            //  The one curses was holding, let go now that another key
            //  has arrived. It has been reported already.
            gkbd_esc_owed--;
            key = ERR;
        }
    }
#endif

    return key;
}


//  ------------------------------------------------------------------
//  The following table maps NT virtual keycodes to PC BIOS keyboard
//  values.  For each virtual keycode there are four possible BIOS
//  values: normal, shift, Ctrl, and ALT.  Key combinations that have
//  no BIOS equivalent have a value of -1, and are ignored.  Extended
//  (non-ASCII) key values have bit 8 set to 1 using the EXT macro.

#elif defined(__WIN32__)

#define EXT(key)    ((key)|0x10000)
#define ISEXT(val)  ((val)&0x10000)
#define EXTVAL(val) ((val)&0xFF)

struct kbd
{
    int keycode;            // virtual keycode
    int normal;             // BIOS keycode - normal
    int shift;              // BIOS keycode - Shift-
    int ctrl;               // BIOS keycode - Ctrl-
    int alt;                // BIOS keycode - Alt-
} __gkbd_nt2b_table [] =
{

//  ------------------------------------------------------------------
//  Virtual key   Normal      Shift       Control     Alt

    { VK_BACK,      Key_BS,     Key_BS,     Key_C_BS,   Key_A_BS },
    { VK_TAB,       Key_Tab,    Key_S_Tab,  Key_C_Tab,  Key_A_Tab },
    { VK_RETURN,    Key_Ent,    Key_Ent,    Key_C_Ent,  Key_A_Ent },
    { VK_ESCAPE,    Key_Esc,    Key_Esc,    Key_Esc,    Key_A_Esc },
    { VK_SPACE,     -1,         -1,         Key_Space,  Key_Space },
    { VK_APPS,      Key_S_F10,  Key_S_F10,  Key_S_F10,  -1        },

    { '0',          Key_0,      Key_S_0,    -1,         Key_A_0 },
    { '1',          Key_1,      Key_S_1,    -1,         Key_A_1 },
    { '2',          Key_2,      Key_S_2,    Key_C_2,    Key_A_2 },
    { '3',          Key_3,      Key_S_3,    -1,         Key_A_3 },
    { '4',          Key_4,      Key_S_4,    -1,         Key_A_4 },
    { '5',          Key_5,      Key_S_5,    -1,         Key_A_5 },
    { '6',          Key_6,      Key_S_6,    Key_C_6,    Key_A_6 },
    { '7',          Key_7,      Key_S_7,    -1,         Key_A_7 },
    { '8',          Key_8,      Key_S_8,    -1,         Key_A_8 },
    { '9',          Key_9,      Key_S_9,    -1,         Key_A_9 },
    { 'A',          Key_A,      Key_S_A,    Key_C_A,    Key_A_A },
    { 'B',          Key_B,      Key_S_B,    Key_C_B,    Key_A_B },
    { 'C',          Key_C,      Key_S_C,    Key_C_C,    Key_A_C },
    { 'D',          Key_D,      Key_S_D,    Key_C_D,    Key_A_D },
    { 'E',          Key_E,      Key_S_E,    Key_C_E,    Key_A_E },
    { 'F',          Key_F,      Key_S_F,    Key_C_F,    Key_A_F },
    { 'G',          Key_G,      Key_S_G,    Key_C_G,    Key_A_G },
    { 'H',          Key_H,      Key_S_H,    Key_C_H,    Key_A_H },
    { 'I',          Key_I,      Key_S_I,    Key_C_I,    Key_A_I },
    { 'J',          Key_J,      Key_S_J,    Key_C_J,    Key_A_J },
    { 'K',          Key_K,      Key_S_K,    Key_C_K,    Key_A_K },
    { 'L',          Key_L,      Key_S_L,    Key_C_L,    Key_A_L },
    { 'M',          Key_M,      Key_S_M,    Key_C_M,    Key_A_M },
    { 'N',          Key_N,      Key_S_N,    Key_C_N,    Key_A_N },
    { 'O',          Key_O,      Key_S_O,    Key_C_O,    Key_A_O },
    { 'P',          Key_P,      Key_S_P,    Key_C_P,    Key_A_P },
    { 'Q',          Key_Q,      Key_S_Q,    Key_C_Q,    Key_A_Q },
    { 'R',          Key_R,      Key_S_R,    Key_C_R,    Key_A_R },
    { 'S',          Key_S,      Key_S_S,    Key_C_S,    Key_A_S },
    { 'T',          Key_T,      Key_S_T,    Key_C_T,    Key_A_T },
    { 'U',          Key_U,      Key_S_U,    Key_C_U,    Key_A_U },
    { 'V',          Key_V,      Key_S_V,    Key_C_V,    Key_A_V },
    { 'W',          Key_W,      Key_S_W,    Key_C_W,    Key_A_W },
    { 'X',          Key_X,      Key_S_X,    Key_C_X,    Key_A_X },
    { 'Y',          Key_Y,      Key_S_Y,    Key_C_Y,    Key_A_Y },
    { 'Z',          Key_Z,      Key_S_Z,    Key_C_Z,    Key_A_Z },

    { VK_PRIOR,     Key_PgUp,   Key_S_PgUp, Key_C_PgUp, Key_A_PgUp },
    { VK_NEXT,      Key_PgDn,   Key_S_PgDn, Key_C_PgDn, Key_A_PgDn },
    { VK_END,       Key_End,    Key_S_End,  Key_C_End,  Key_A_End },
    { VK_HOME,      Key_Home,   Key_S_Home, Key_C_Home, Key_A_Home },
    { VK_LEFT,      Key_Lft,    Key_S_Lft,  Key_C_Lft,  Key_A_Lft },
    { VK_UP,        Key_Up,     Key_S_Up,   Key_C_Up,   Key_A_Up },
    { VK_RIGHT,     Key_Rgt,    Key_S_Rgt,  Key_C_Rgt,  Key_A_Rgt },
    { VK_DOWN,      Key_Dwn,    Key_S_Dwn,  Key_C_Dwn,  Key_A_Dwn },
    { VK_INSERT,    Key_Ins,    Key_S_Ins,  Key_C_Ins,  Key_A_Ins },
    { VK_DELETE,    Key_Del,    Key_S_Del,  Key_C_Del,  Key_A_Del },
    { VK_CLEAR,     Key_5Num,   Key_S_5Num, Key_C_5Num, Key_A_5Num },
    { VK_NUMPAD0,   Key_0,      Key_S_Ins,  Key_C_Ins,  -1 },
    { VK_NUMPAD1,   Key_1,      Key_S_End,  Key_C_End,  -1 },
    { VK_NUMPAD2,   Key_2,      Key_S_Dwn,  Key_C_Dwn,  -1 },
    { VK_NUMPAD3,   Key_3,      Key_S_PgDn, Key_C_PgDn, -1 },
    { VK_NUMPAD4,   Key_4,      Key_S_Lft,  Key_C_Lft,  -1 },
    { VK_NUMPAD5,   Key_5,      Key_S_5Num, Key_C_5Num, -1 },
    { VK_NUMPAD6,   Key_6,      Key_S_Rgt,  Key_C_Rgt,  -1 },
    { VK_NUMPAD7,   Key_7,      Key_S_Home, Key_C_Home, -1 },
    { VK_NUMPAD8,   Key_8,      Key_S_Up,   Key_C_Up,   -1 },
    { VK_NUMPAD9,   Key_9,      Key_S_PgUp, Key_C_PgUp, -1 },
    { VK_MULTIPLY,  Key_Multi,  Key_Multi,  Key_Multi,  Key_Multi },
    { VK_ADD,       Key_Plus,   Key_Plus,   Key_Plus,   Key_Plus },
    { VK_SUBTRACT,  Key_Minus,  Key_Minus,  Key_Minus,  Key_Minus },
    { VK_DECIMAL,   -1,         -1,         Key_C_Del,  Key_A_Del },
    { VK_DIVIDE,    Key_Sls,    Key_Sls,    Key_Sls,    Key_Sls },
    { VK_F1,        Key_F1,     Key_S_F1,   Key_C_F1,   Key_A_F1 },
    { VK_F2,        Key_F2,     Key_S_F2,   Key_C_F2,   Key_A_F2 },
    { VK_F3,        Key_F3,     Key_S_F3,   Key_C_F3,   Key_A_F3 },
    { VK_F4,        Key_F4,     Key_S_F4,   Key_C_F4,   Key_A_F4 },
    { VK_F5,        Key_F5,     Key_S_F5,   Key_C_F5,   Key_A_F5 },
    { VK_F6,        Key_F6,     Key_S_F6,   Key_C_F6,   Key_A_F6 },
    { VK_F7,        Key_F7,     Key_S_F7,   Key_C_F7,   Key_A_F7 },
    { VK_F8,        Key_F8,     Key_S_F8,   Key_C_F8,   Key_A_F8 },
    { VK_F9,        Key_F9,     Key_S_F9,   Key_C_F9,   Key_A_F9 },
    { VK_F10,       Key_F10,    Key_S_F10,  Key_C_F10,  Key_A_F10 },
    { VK_F11,       Key_F11,    Key_S_F11,  Key_C_F11,  Key_A_F11 },
    { VK_F12,       Key_F12,    Key_S_F12,  Key_C_F12,  Key_A_F12 },

    { -1,           -1,         -1,         -1,         -1 }  // THE END
};


//  ------------------------------------------------------------------

bool is_oem_key(int keycode)
{
    switch(keycode)
    {
    // OEM specific keys
    case 0x2a:
    case 0xba:
    case 0xbb:
    case 0xbc:
    case 0xbd:
    case 0xbe:
    case 0xbf:
    case 0xc0:
    case 0xdb:
    case 0xdc:
    case 0xdd:
    case 0xde:
    case 0xdf:
    case 0xe0:
    case 0xe1:
    case 0xe2:
    case 0xe3:
    case 0xe4:
    case 0xe6:
    case 0xe9:
    case 0xea:
    case 0xeb:
    case 0xec:
    case 0xed:
    case 0xef:
    case 0xf0:
    case 0xf1:
    case 0xf2:
    case 0xf3:
    case 0xf4:
    case 0xf5:
        return true;

    default:
        return false;
    }
}


//  ------------------------------------------------------------------

//  ------------------------------------------------------------------
//  Windows 9x has no working wide console input: PeekConsoleInputW and
//  ReadConsoleInputW are stubs there. The record is read through the
//  ANSI calls instead, and the byte it carries - in the OEM codepage -
//  is turned into the codepoint the rest of the keyboard layer expects
//  to find in the wide half of the union.

static void gkbd_oem_to_unicode(INPUT_RECORD& inp, DWORD nread)
{
    if(nread and (inp.EventType == KEY_EVENT))
    {
        char  c  = inp.Event.KeyEvent.uChar.AsciiChar;
        WCHAR wc = (unsigned char)c;
        if(c)
            MultiByteToWideChar(CP_OEMCP, 0, &c, 1, &wc, 1);
        inp.Event.KeyEvent.uChar.UnicodeChar = wc;
    }
}


static BOOL gkbd_peek(INPUT_RECORD& inp, DWORD& nread)
{
    if(gkbd_nt)
        return PeekConsoleInputW(gkbd_hin, &inp, 1, &nread);

    BOOL rc = PeekConsoleInputA(gkbd_hin, &inp, 1, &nread);
    gkbd_oem_to_unicode(inp, nread);
    return rc;
}


static BOOL gkbd_read(INPUT_RECORD& inp, DWORD& nread)
{
    if(gkbd_nt)
        return ReadConsoleInputW(gkbd_hin, &inp, 1, &nread);

    BOOL rc = ReadConsoleInputA(gkbd_hin, &inp, 1, &nread);
    gkbd_oem_to_unicode(inp, nread);
    return rc;
}


int gkbd_nt2bios(INPUT_RECORD& inp)
{

    int keycode = inp.Event.KeyEvent.wVirtualKeyCode;
    int state   = inp.Event.KeyEvent.dwControlKeyState;

    //  The console is read through the wide calls, so this is a
    //  codepoint. A gkey has one byte for the character, so the
    //  codepoint goes into the side-channel (see gkbd_lastchars) and
    //  what is returned here is the first byte of its encoding in the
    //  local charset - enough for the caller to see that a character
    //  was typed, and enough for every existing binding to keep working.
    uint32_t unicode = inp.Event.KeyEvent.uChar.UnicodeChar;
    int ascii = 0;

    if(unicode)
    {
        gkbd_setlastcp(unicode);
        int len = 0;
        const char* chars = gkbd_lastchars(&len);
        ascii = len ? (unsigned char)chars[0] : (int)(unicode & 0xFF);
    }
    else
    {
        gkbd_setlastcp(0);
    }

    // Look up the virtual keycode in the table. Ignore unrecognized keys.

    kbd* k = &__gkbd_nt2b_table[0];
    while((keycode != k->keycode) and (k->keycode != -1))
        k++;
    if(k->keycode == -1)    // value not in table
    {
        return ascii ? ascii : -1;
    }

    // Check the state of the shift keys. ALT has highest
    // priority, followed by Control, followed by Shift.
    // Select the appropriate table entry based on shift state.

    int c;
    if(state & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED))
        c = k->alt;
    else if(state & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))
        c = k->ctrl;
    else if(state & SHIFT_PRESSED)
    {
        if (k->shift == -1)
            c = ascii ? ascii : -1;
        else
            c = k->shift;
    }
    else
    {
        // If it is a letter key, use the ASCII value supplied
        // by NT to take into account the CapsLock state.
        if (g_isupper(keycode) or (k->normal == -1))
            c = ascii ? ascii : -1;
        else
            c = k->normal;
    }

    if (c != -1)
    {
        if (ascii and not (right_alt_same_as_left ? (state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) : (state & LEFT_ALT_PRESSED)))
            if (isalnum(keycode))
                return (ascii == ' ') ? Key_Space : ascii;
        if (ISEXT(c))
            return EXTVAL(c) << 8;
    }

    return c;
}

//  ------------------------------------------------------------------

bool is_numpad_key(const INPUT_RECORD& inp)
{

    if(not (inp.Event.KeyEvent.dwControlKeyState & ENHANCED_KEY))
    {
        switch(inp.Event.KeyEvent.wVirtualKeyCode)
        {
        case VK_CLEAR:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_END:
        case VK_HOME:
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN:
        case VK_INSERT:
        case VK_DELETE:
        case VK_NUMPAD0:
        case VK_NUMPAD1:
        case VK_NUMPAD2:
        case VK_NUMPAD3:
        case VK_NUMPAD4:
        case VK_NUMPAD5:
        case VK_NUMPAD6:
        case VK_NUMPAD7:
        case VK_NUMPAD8:
        case VK_NUMPAD9:
        case VK_ADD:            // Alt+'+' opens a hexadecimal entry
            return true;
        }
    }
    return false;
}


//  ------------------------------------------------------------------
//  Alt+numpad: the console composes the character itself - from the
//  decimal digits typed while Alt is held, or from hexadecimal ones
//  after numpad '+' where the EnableHexNumpad registry value allows it
//  - and hands it over on the release of Alt, as the character of that
//  key-up event. An astral character comes as two such events carrying
//  a surrogate pair. This is the one key-up event that means a
//  character was typed.

static bool gkbd_alt_composed(const INPUT_RECORD& inp)
{
    return (inp.EventType == KEY_EVENT) and not inp.Event.KeyEvent.bKeyDown
           and (inp.Event.KeyEvent.wVirtualKeyCode == VK_MENU)
           and (inp.Event.KeyEvent.uChar.UnicodeChar != 0);
}


//  Take the composed character out of that event - and the low half
//  of a surrogate pair out of the next - and put it in the side channel
//  the way a typed character goes there. Returns the key value that
//  stands for it, or 0 for nothing.

static gkey gkbd_take_composed(INPUT_RECORD& inp)
{
    uint32_t cp = inp.Event.KeyEvent.uChar.UnicodeChar;
    if((cp >= 0xD800) and (cp <= 0xDBFF))
    {
        INPUT_RECORD low;
        DWORD n = 0;
        gkbd_peek(low, n);
        if(n and gkbd_alt_composed(low)
           and (low.Event.KeyEvent.uChar.UnicodeChar >= 0xDC00)
           and (low.Event.KeyEvent.uChar.UnicodeChar <= 0xDFFF))
        {
            gkbd_read(low, n);
            cp = 0x10000 + ((cp - 0xD800) << 10) + (low.Event.KeyEvent.uChar.UnicodeChar - 0xDC00);
        }
    }
    gkbd_setlastcp(cp);
    int len = 0;
    const char* chars = gkbd_lastchars(&len);
    return len ? (gkey)(unsigned char)chars[0] : (gkey)(cp & 0xFF);
}


//  ------------------------------------------------------------------
//  Numpad translation table

#elif defined(__MSDOS__) || defined(__OS2__)

const word numpad_keys[] =
{
    0x4737, 0x4838, 0x4939, 0x0000,
    0x4B34, 0x0000, 0x4D36, 0x0000,
    0x4F31, 0x5032, 0x5133,
    0x5230, 0x532e
};

#endif

#if defined(GOLD_LINUX_TIOCL)
bool linux_cui_key(gkey k)
{
    switch(k)
    {
    case Key_Dwn:
    case Key_Up:
    case Key_Lft:
    case Key_Rgt:
    case Key_Home:
    case Key_Del:
    case Key_Ins:
    case Key_PgDn:
    case Key_PgUp:
    case Key_End:
        return true;
    }
    return false;
}
#endif

#if defined(__BEOS__)
int BeOSShiftState()
{
    int shift = 0;
    uint32 mods = modifiers();
    if(mods&B_LEFT_SHIFT_KEY)  shift |= LSHIFT;
    if(mods&B_RIGHT_SHIFT_KEY) shift |= RSHIFT;
    if(mods&B_CONTROL_KEY)     shift |= GCTRL;
    if(mods&B_OPTION_KEY)      shift |= ALT;
    return shift;
}
#endif

// TODO: move jcuken_koi8ru[] to configuration files

//       Apple OS X, non-standard or phonetic Belorussian, Russian and
//       Ukrainian keyboard need <Alt>+<national key> translation by
//       terminal application

//	 <Alt>+<national key> translation by terminal application does not
//	 conflict with this code, because it produces the ASCII
//	 Esc-sequence, and this code only handles KOI8-RU Esc-sequence

gkey gkbd_alt_secondary_keyboard(int key)
{
    const static char jcuken_koi8ru[] =
    {
#ifndef DONT_JCUKEN_ALT
        // КОИ-8: ёйцукенгшщзхъфывапролджэячсмитьбю
        0243, 0312, 0303, 0325, 0313, 0305, 0316, 0307,
        0333, 0335, 0332, 0310, 0337, 0306, 0331, 0327,
        0301, 0320, 0322, 0317, 0314, 0304, 0326, 0334,
        0321, 0336, 0323, 0315, 0311, 0324, 0330, 0302,
        0300,

        // КОИ-8: ЁЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮ
        0263, 0352, 0343, 0365, 0353, 0345, 0356, 0347,
        0373, 0375, 0372, 0350, 0377, 0346, 0371, 0367,
        0341, 0360, 0362, 0357, 0354, 0344, 0366, 0374,
        0361, 0376, 0363, 0355, 0351, 0364, 0370, 0342,
        0340,

        // KOI8-RU: ўЎіІ
        0256, 0276, 0246, 0266,

        // KOI8-RU: їЇґҐєЄ
        0247, 0267, 0255, 0275, 0244, 0264,
#endif
        0
    };
    const static char qwerty_ascii[] =
        // KOI8-R small keys
        "`qwertyuiop[]asdfghjkl;'zxcvbnm,."
        // KOI8-R keys with CapsLock
        "`qwertyuiop[]asdfghjkl;'zxcvbnm,."
        // KOI8-RU Belorussian
        "oobb"
        // KOI8-RU Ukranian
        "]]\\\\''"
        ;
    const char *pjk = strchr(jcuken_koi8ru, key);
    // TODO: assert(sizeof(qwerty_ascii) <= sizeof(jcuken_koi8ru));
    if(pjk and (pjk - jcuken_koi8ru < sizeof(qwerty_ascii)))
    {
        int ac = qwerty_ascii[pjk - jcuken_koi8ru]&0377;

        if(!g_isalpha(ac))
        {
            return 0;
        }
        return scancode_table[ac];
    }
    return 0;
}

//  ------------------------------------------------------------------
//  Get key stroke

gkey kbxget_raw(eKeyModes mode)
{
    gkey k;

//  TO_PORT_TAG: kbxget_raw(KeyMode_Control)
#if defined(__USE_NCURSES__)

    int key;
    if(mode == KeyMode_Shift)
    {
        // We can't do much but we can at least this :-)
        GKbdChar _saved;
        gkbd_savelast(_saved);
        k = kbxget_raw(KeyMode_Test);
        gkbd_restorelast(_saved);
        key = 0;
        switch(k)
        {
        case Key_A_Up:
        case Key_A_Dwn:
        case Key_A_Rgt:
        case Key_A_Lft:
            key = ALT;
            break;
        case Key_C_Brk:
        case Key_C_Up:
        case Key_C_Dwn:
        case Key_C_Rgt:
        case Key_C_Lft:
            key = GCTRL;
            break;
        case Key_S_Tab:
        case Key_S_Home:
        case Key_S_Del:
        case Key_S_Ins:
        case Key_S_Lft:
        case Key_S_Rgt:
        case Key_S_End:
        case Key_S_Dwn:
        case Key_S_Up:
            key = LSHIFT;
            break;
        }
        return key;
    }

    //  Get keystroke.
    //
    //  A read that finds nothing has to leave the last character alone.
    //  GoldED polls the keyboard while it idles - to move the clock on,
    //  among other things - and a poll landing between a key being read
    //  and being used would otherwise wipe the bytes it was typed as.
    //  Every field that asks gkbd_lastchars() what was typed then saw an
    //  empty answer and fell back to the single byte the key carries,
    //  which for anything outside ASCII is a fragment of a character.

    GKbdChar _lastsv;
    gkbd_savelast(_lastsv);

    gkbd_setlastcp(0);      // no character typed unless we say otherwise
    key = gkbd_cursgetch(mode);
    if(key == ERR)
    {
        gkbd_restorelast(_lastsv);
        return 0;
    }

    // Prefix for Meta-key or Alt-key sequences
    if(key == 27)
    {
        int key2 = gkbd_cursgetch(KeyMode_Test);
        // If no key follows, it is no Meta- or Alt- seq, but a single Esc
        if(key2 == ERR)
            k = Key_Esc;
        // Compute the right keycode for the alt sequence
        else if((key2 >= '1') and (key2 <= '9'))
            k = 0x7800 + ((key2 - '1') << 8);
        else if(key2 == '0')
            k = 0x8100;
        //  Range first, then the ctype call. key2 comes straight from
        //  curses and may be a key code rather than a character - an
        //  arrow is 0403, which is 259 - and on Unix g_isalpha() is
        //  isalpha() itself. NetBSD's ctype aborts the program on an
        //  argument outside -1..255 rather than reading past its table,
        //  so an Esc arriving on its own just before an arrow key ended
        //  the session. Reachable by moving between areas quickly
        //  enough to split an escape sequence across two reads.
        else if((0 <= key2)
                and (key2 < (int)(sizeof(scancode_table)/sizeof(scancode_table[0])))
                and g_isalpha(key2))
            k = (scancode_table[key2]);
        else if((key2 == '\010') or (key2 == KEY_BACKSPACE) or (key2 == '\177'))
            k = Key_A_BS;
        else if(key2 == '\011')
            k = Key_A_Tab;
        else if((key2 == '\015') or (key2 == KEY_ENTER))
            k = Key_A_Ent;
        else if(0 == (k = gkbd_alt_secondary_keyboard(key2)))
        {
            // No correct Alt-sequence; ungetch last key and return Esc
            if (mode != KeyMode_Test)
                gkbd_ungetch(key2);
            k = Key_Esc;
        }

        if((key2 != ERR) and (mode == KeyMode_Test))
            gkbd_ungetch(key2);
    }
    // Curses sequence; lookup in nice table above
    else if((key >= KEY_MIN)
            and (key <= KEY_MIN+sizeof(gkbd_curstable)/sizeof(int))
            and (0 <= gkbd_curstable[key - KEY_MIN]))
    {
        k = (gkbd_curstable[key - KEY_MIN]);
        if(k == Key_Resize)
        {
            if(gkbd_resize_real())
                gkbd.resize_pending = true;
            else
                k = 0;
        }
    }
    else if(key == '\015')
        k = Key_Ent;
    else if(key == '\011')
        k = Key_Tab;
    else if(key == '\000')
        k = Key_Space;
    else if(key < KEY_MIN)
        k = key;
    else
        return 0;	// Incorrect or unsupported key don't ungetch()

    if(mode == KeyMode_Test)
        gkbd_ungetch(key);

#elif defined(__MSDOS__)
    int dos_mode = (int)mode;
    if(gkbd.extkbd)
        dos_mode |= 0x10;

    i86 cpu;
    cpu.ah((byte)dos_mode);
    cpu.genint(0x16);
    if(dos_mode & 0x01)
        if(cpu.flags() & 0x40)   // if ZF is set, no key is available
            return 0;
    k = (gkey)cpu.ax();

    if((dos_mode & ~0x10) == 0)
    {
        if((KCodAsc(k) == 0xE0) and (KCodScn(k) != 0))
        {
            if(kbxget_raw(KeyMode_Shift) & (LSHIFT | RSHIFT))
            {
                KCodAsc(k) = 0;
                KCodScn(k) |= 0x80;
            }
        }
        else
            switch(KCodScn(k))
            {
            case 0x47:
            case 0x48:
            case 0x49:
            case 0x4B:
            case 0x4D:
            case 0x4F:
            case 0x50:
            case 0x51:
            case 0x52:
            case 0x53:
            {
                int shifts = kbxget_raw(KeyMode_Shift);
                if(shifts & (LSHIFT | RSHIFT))
                {
                    if(shifts & NUMLOCK)
                        KCodAsc(k) = 0;
                    else
                    {
                        KCodAsc(k) = 0;
                        KCodScn(k) |= 0x80;
                    }
                }
            }
            break;
            default:
                break;
            }
    }

    // If you test shift/alt/ctrl status with bios calls (e.g., using
    // bioskey (2) or bioskey (0x12)) then you should also use bios calls
    // for testing for keys.  This can be done with by bioskey (1) or
    // bioskey (0x11).  Failing to do so can cause trouble in multitasking
    // environments like DESQview/X. (Taken from DJGPP documentation)
    if((dos_mode & 0x02) == 1)
        kbxget_raw(KeyMode_Test);

#elif defined(__OS2__)

    KBDKEYINFO kb;
    if(mode == KeyMode_Wait)
        KbdCharIn(&kb, IO_WAIT, 0);
    else if(mode == KeyMode_Shift)
    {
        KbdPeek(&kb, 0);
        if(kb.fbStatus)
            return (gkey)(kb.fsState & (RSHIFT|LSHIFT|GCTRL|ALT));
        else
            return 0;
    }
    else
    {
        KbdPeek(&kb, 0);
        if(!(kb.fbStatus & 0x40))
            return 0;
    }
    KCodScn(k) = kb.chScan;
    KCodAsc(k) = kb.chChar;
    if(0x000 == KCodKey(k))
        return KEY_BRK;
    if(0xE0 == KCodScn(k))
        KCodScn(k) = 0x1C;
    else
    {
        if(0xE0 == KCodAsc(k))
        {
            // If key on the alphanumeric part then don't touch it.
            // This need to enter for example, russian 'p' char (code 0xe0)
            if(KCodScn(k) >= 0x38)
            {
                KCodAsc(k) = 0x00;
                if(kb.fsState & (LSHIFT | RSHIFT))
                    KCodScn(k) |= 0x80;
            }
            else
                KCodScn(k) = 0x00;
        }
        else
            switch(KCodScn(k))
            {
            case 0x47:
            case 0x48:
            case 0x49:
            case 0x4B:
            case 0x4D:
            case 0x4F:
            case 0x50:
            case 0x51:
            case 0x52:
            case 0x53:
                if(kb.fsState & (LSHIFT | RSHIFT))
                {
                    if(kb.fsState & NUMLOCK)
                        KCodAsc(k) = 0;
                    else
                    {
                        KCodAsc(k) = 0;
                        KCodScn(k) |= 0x80;
                    }
                }
                break;
            default:
                break;
            }
    }

#elif defined(__WIN32__)

    INPUT_RECORD inp;
    DWORD nread;
    static gkey KeyCtrlState = 0;

    if (mode == KeyMode_Control)
    {
        return KeyCtrlState;
    }
    else if(mode == KeyMode_Shift)
    {
        return 0;
    }
    else if(mode == KeyMode_Test)
    {

        // Peek at next key
        k = 0;
        gkbd_peek(inp, nread);
        if(nread)
        {
            if(inp.EventType == WINDOW_BUFFER_SIZE_EVENT)
            {
                gkbd_read(inp, nread);
                if(gkbd_resize_real())
                {
                    gkbd.resize_pending = true;
                    return Key_Resize;
                }
                return 0;
            }
            if(gkbd_nt and gkbd_alt_composed(inp))
            {
                //  Left in the queue for the read below; only the
                //  side channel is set, as the read will set it again.
                k = gkbd_take_composed(inp);
                if(k)
                    return k;
            }
            if((inp.EventType == KEY_EVENT) and inp.Event.KeyEvent.bKeyDown)
            {
                int kc = gkbd_nt2bios(inp);
                if((kc != -1) or is_oem_key(inp.Event.KeyEvent.wVirtualKeyCode))
                {
                    k = (gkey)kc;
                    return k;
                }
            }

            if ((inp.EventType != MOUSE_EVENT) || (WinVer.dwPlatformId == VER_PLATFORM_WIN32_NT))
            {
                // Discard other events
                gkbd_read(inp, nread);
            }
        }
    }
    else // KeyMode_Wait
    {

        DWORD &CKS = inp.Event.KeyEvent.dwControlKeyState;
        WORD &VKC = inp.Event.KeyEvent.wVirtualKeyCode;
        char &ascii = inp.Event.KeyEvent.uChar.AsciiChar;

        while(1)
        {

            gkbd_peek(inp, nread);
            if(not nread)
            {
                WaitForSingleObject(gkbd_hin, 1000);
                continue;
            }

            if(inp.EventType == WINDOW_BUFFER_SIZE_EVENT)
            {
                gkbd_read(inp, nread);
                if(not gkbd_resize_real())
                    continue;
                gkbd.resize_pending = true;
                k = Key_Resize;
                break;
            }
            if(gkbd_nt and gkbd_alt_composed(inp))
            {
                gkbd_read(inp, nread);
                k = gkbd_take_composed(inp);
                if(k)
                    break;
                continue;
            }

            if((inp.EventType == KEY_EVENT) and inp.Event.KeyEvent.bKeyDown)
            {
                bool alt_pressed = make_bool(CKS & (LEFT_ALT_PRESSED|RIGHT_ALT_PRESSED));
                bool ctrl_pressed = make_bool(CKS & (LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED));
                bool shift_pressed = make_bool(CKS & SHIFT_PRESSED);
                bool special_key = false;

                k = 0;

                if(alt_pressed and gkbd_nt and is_numpad_key(inp))
                {
                    //  A digit of an Alt+numpad entry, or the '+' that
                    //  opens a hexadecimal one. The character it builds
                    //  arrives with the release of Alt, handled above;
                    //  the digit itself is nothing. The cooked read that
                    //  used to compose the character here returned it as
                    //  one byte of the console codepage, which cannot
                    //  carry what a UTF-8 session types.
                    gkbd_read(inp, nread);
                    continue;
                }
                if(alt_pressed)
                    special_key = is_numpad_key(inp); // Alt-<numpad key> on Win9x: the cooked read composes
                else if(not gkbd_nt and not (CKS & ENHANCED_KEY) and not (VKC == VK_CLEAR) and (ascii and not ctrl_pressed) and not (iscntrl(ascii) and shift_pressed))
                    special_key = true; // It is alphanumeric key under Win9x
                if(special_key)
                {
                    ReadConsole(gkbd_hin, &ascii, 1, &nread, NULL);
                    if(alt_pressed)
                    {
                        k = (gkey)ascii;
                        break;
                    }
                    //  An OEM byte; gkbd_nt2bios() reads the wide half.
                    gkbd_oem_to_unicode(inp, 1);
                }
                else
                {
                    gkbd_read(inp, nread);
                }

                // Fix Win9x anomaly
                if((CKS & NUMLOCK_ON) and not (CKS & ENHANCED_KEY) and (VKC == VK_DELETE))
                    VKC = VK_DECIMAL;

                switch(VKC)
                {
                // Not meanful keys
                case VK_SHIFT:
                case VK_CONTROL:
                case VK_MENU:
                case VK_CAPITAL:
                case VK_NUMLOCK:
                case VK_SCROLL:
                    break;

                case VK_NUMPAD0:
                case VK_NUMPAD1:
                case VK_NUMPAD2:
                case VK_NUMPAD3:
                case VK_NUMPAD4:
                case VK_NUMPAD5:
                case VK_NUMPAD6:
                case VK_NUMPAD7:
                case VK_NUMPAD8:
                case VK_NUMPAD9:
                    if(shift_pressed)
                    {
                        WORD keytrans[10][2] =
                        {
                            {VK_NUMPAD0, VK_INSERT},
                            {VK_NUMPAD1, VK_END},
                            {VK_NUMPAD2, VK_DOWN},
                            {VK_NUMPAD3, VK_NEXT},
                            {VK_NUMPAD4, VK_LEFT},
                            {VK_NUMPAD5, VK_CLEAR},
                            {VK_NUMPAD6, VK_RIGHT},
                            {VK_NUMPAD7, VK_HOME},
                            {VK_NUMPAD8, VK_UP},
                            {VK_NUMPAD9, VK_PRIOR},
                        };
                        for(int i = 0; i < 10; i++)
                            if(VKC == keytrans[i][0])
                            {
                                VKC = keytrans[i][1];
                                break;
                            }
                    }
                // fall through
                default:
                {
                    int kc = gkbd_nt2bios(inp);
                    if(kc != -1)
                        k = (gkey)kc;
                }
                break;
                }
                if(k != 0)
                    break;
            }
            else
            {
                // Discard other events
                gkbd_read(inp, nread);
            }
        }
    }

#elif defined(__UNIX__)

    if(mode == KeyMode_Shift)
    {
#if defined(GOLD_LINUX_TIOCL)
        // Under Linux we could use TIOCLINUX ioctl_console with
        // TIOCL_GETSHIFTSTATE subcode to read the shift state variable.
        // Of course it is very unportable but should produce good results :-)
        // Note: valgrind complains with "Syscall param ioctl(TIOCLINUX) points
        // to uninitialised byte(s)".  This is expected. valgrind does not
        // understand how the ioctl works.  Try to run with a
        // --sim-hints=lax-ioctls option.
        char key = TIOCL_GETSHIFTSTATE;
        if(ioctl(fileno(stdin), TIOCLINUX, &key) == -1)
#else
        int key = 0;
#endif
#ifdef __BEOS__
        key = BeOSShiftState();
#endif
        return (int)key;
    }
    else if(mode == KeyMode_Test)
    {
        // Peek at next key
        return gkbd_input_pending() ? 0xFFFF : 0;
    }
    else
    {
        k = gkbd_getmappedkey();
    }

#endif

#ifdef GOLD_LINUX_TIOCL
    char shifts = TIOCL_GETSHIFTSTATE;
    if(linux_cui_key(k))
    {
        if(ioctl(fileno(stdin), TIOCLINUX, &shifts) == -1)
            shifts = 0;
        if(shifts & (LSHIFT | RSHIFT))
            KCodScn(k) |= 0x80;
        else if(shifts & GCTRL)
        {
            switch(k)
            {
            case Key_Ins:
                k = Key_C_Ins;
                break;
            case Key_Del:
                k = Key_C_Del;
                break;
            case Key_Dwn:
                k = Key_C_Dwn;
                break;
            case Key_Up:
                k = Key_C_Up;
                break;
            case Key_Lft:
                k = Key_C_Lft;
                break;
            case Key_Rgt:
                k = Key_C_Rgt;
                break;
            case Key_Home:
                k = Key_C_Home;
                break;
            case Key_PgDn:
                k = Key_C_PgDn;
                break;
            case Key_PgUp:
                k = Key_C_PgUp;
                break;
            case Key_End:
                k = Key_C_End;
                break;
            }
        }
    }
    else if(k == Key_BS)
    {
        if(ioctl(fileno(stdin), TIOCLINUX, &shifts) == -1)
            shifts = 0;
        if(shifts & ALT)
            k = Key_A_BS;
        else if(shifts & GCTRL)
            k = Key_C_BS;
    }
#elif __BEOS__
    {
        int shifts = BeOSShiftState();
        if(shifts & (ALT))
            switch(k)
            {
            case Key_0:
                k=Key_A_0;
                break;
            case Key_1:
                k=Key_A_1;
                break;
            case Key_2:
                k=Key_A_2;
                break;
            case Key_3:
                k=Key_A_3;
                break;
            case Key_4:
                k=Key_A_4;
                break;
            case Key_5:
                k=Key_A_5;
                break;
            case Key_6:
                k=Key_A_6;
                break;
            case Key_7:
                k=Key_A_7;
                break;
            case Key_8:
                k=Key_A_8;
                break;
            case Key_9:
                k=Key_A_9;
                break;
            case Key_A:
                k=Key_A_A;
                break;
            case Key_B:
                k=Key_A_B;
                break;
            case Key_C:
                k=Key_A_C;
                break;
            case Key_D:
                k=Key_A_D;
                break;
            case Key_E:
                k=Key_A_E;
                break;
            case Key_F:
                k=Key_A_F;
                break;
            case Key_G:
                k=Key_A_G;
                break;
            case Key_H:
                k=Key_A_H;
                break;
            case Key_I:
                k=Key_A_I;
                break;
            case Key_J:
                k=Key_A_J;
                break;
            case Key_K:
                k=Key_A_K;
                break;
            case Key_L:
                k=Key_A_L;
                break;
            case Key_M:
                k=Key_A_M;
                break;
            case Key_N:
                k=Key_A_N;
                break;
            case Key_O:
                k=Key_A_O;
                break;
            case Key_P:
                k=Key_A_P;
                break;
            case Key_Q:
                k=Key_A_Q;
                break;
            case Key_R:
                k=Key_A_R;
                break;
            case Key_S:
                k=Key_A_S;
                break;
            case Key_T:
                k=Key_A_T;
                break;
            case Key_U:
                k=Key_A_U;
                break;
            case Key_V:
                k=Key_A_V;
                break;
            case Key_W:
                k=Key_A_W;
                break;
            case Key_X:
                k=Key_A_X;
                break;
            case Key_Y:
                k=Key_A_Y;
                break;
            case Key_Z:
                k=Key_A_Z;
                break;
            case Key_F1:
                k=Key_A_F1;
                break;
            case Key_F2:
                k=Key_A_F2;
                break;
            case Key_F3:
                k=Key_A_F3;
                break;
            case Key_F4:
                k=Key_A_F4;
                break;
            case Key_F5:
                k=Key_A_F5;
                break;
            case Key_F6:
                k=Key_A_F6;
                break;
            case Key_F7:
                k=Key_A_F7;
                break;
            case Key_F8:
                k=Key_A_F8;
                break;
            case Key_F9:
                k=Key_A_F9;
                break;
            case Key_F10:
                k=Key_A_F10;
                break;
            case Key_F11:
                k=Key_A_F11;
                break;
            case Key_F12:
                k=Key_A_F12;
                break;
            case Key_BS:
                k=Key_A_BS;
                break;
            case Key_Ent:
                k=Key_A_Ent;
                break;
            case Key_Tab:
                k=Key_A_Tab;
                break;
            case Key_Dwn:
                k = Key_A_Dwn;
                break;
            case Key_Up:
                k = Key_A_Up;
                break;
            case Key_Lft:
                k = Key_A_Lft;
                break;
            case Key_Rgt:
                k = Key_A_Rgt;
                break;
            case Key_Home:
                k = Key_A_Home;
                break;
            case Key_PgDn:
                k = Key_A_PgDn;
                break;
            case Key_PgUp:
                k = Key_A_PgUp;
                break;
            case Key_End:
                k = Key_A_End;
                break;
            // case Key_: k=Key_A_; break;
            default:
                break;
            }
        else if(shifts & (LSHIFT | RSHIFT))
            switch(k)
            {
            case Key_F1:
                k=Key_S_F1;
                break;
            case Key_F2:
                k=Key_S_F2;
                break;
            case Key_F3:
                k=Key_S_F3;
                break;
            case Key_F4:
                k=Key_S_F4;
                break;
            case Key_F5:
                k=Key_S_F5;
                break;
            case Key_F6:
                k=Key_S_F6;
                break;
            case Key_F7:
                k=Key_S_F7;
                break;
            case Key_F8:
                k=Key_S_F8;
                break;
            case Key_F9:
                k=Key_S_F9;
                break;
            case Key_F10:
                k=Key_S_F10;
                break;
            case Key_F11:
                k=Key_S_F11;
                break;
            case Key_F12:
                k=Key_S_F12;
                break;
            case Key_Tab:
                k=Key_S_Tab;
                break;
            default:
                KCodScn(k) |= 0x80;
                break;
            }
        else if(shifts & GCTRL)
        {
            switch(k)
            {
            case Key_Ent:
                k = Key_C_Ent;
                break;
            case Key_Ins:
                k = Key_C_Ins;
                break;
            case Key_Del:
                k = Key_C_Del;
                break;
            case Key_Dwn:
                k = Key_C_Dwn;
                break;
            case Key_Up:
                k = Key_C_Up;
                break;
            case Key_Lft:
                k = Key_C_Lft;
                break;
            case Key_Rgt:
                k = Key_C_Rgt;
                break;
            case Key_Home:
                k = Key_C_Home;
                break;
            case Key_PgDn:
                k = Key_C_PgDn;
                break;
            case Key_PgUp:
                k = Key_C_PgUp;
                break;
            case Key_End:
                k = Key_C_End;
                break;
            case Key_BS:
                k = Key_C_BS;
                break;
            case Key_F1:
                k=Key_C_F1;
                break;
            case Key_F2:
                k=Key_C_F2;
                break;
            case Key_F3:
                k=Key_C_F3;
                break;
            case Key_F4:
                k=Key_C_F4;
                break;
            case Key_F5:
                k=Key_C_F5;
                break;
            case Key_F6:
                k=Key_C_F6;
                break;
            case Key_F7:
                k=Key_C_F7;
                break;
            case Key_F8:
                k=Key_C_F8;
                break;
            case Key_F9:
                k=Key_C_F9;
                break;
            case Key_F10:
                k=Key_C_F10;
                break;
            case Key_F11:
                k=Key_C_F11;
                break;
            case Key_F12:
                k=Key_C_F12;
                break;
            }
        }
    }
#endif

//  TO_PORT_TAG: kbxget_raw(KeyMode_Control)
#if defined(__WIN32__)
    KeyCtrlState = (gkey)(inp.Event.KeyEvent.dwControlKeyState & 0xFFFF);
#endif
    return k;
}


//  ------------------------------------------------------------------
//  Get key stroke

gkey kbxget(eKeyModes mode)
{

    return keyscanxlat(kbxget_raw(mode));
}


//  ------------------------------------------------------------------
//  Returns keycode of waiting key or zero if none

gkey kbxhit()
{

    return kbxget(KeyMode_Test);
}


//  ------------------------------------------------------------------
//  Clears internal keyboard buffer

void kbclear()
{

    while(gkbd.kbuf != NULL)
    {

        KBuf *kbuf = gkbd.kbuf->next;
        throw_free(gkbd.kbuf);
        gkbd.kbuf = kbuf;
    }
}


//  ------------------------------------------------------------------
//  Clear keyboard buffer

void clearkeys()
{

    while(kbxhit())
        kbxget(KeyMode_Wait);
}


//  ------------------------------------------------------------------
//  Puts a keystroke into the CXL keyboard "buffer"

bool gKeystacking = false;

int kbput(gkey xch)
{
    if (gKeystacking)
    {
        if (gkbd.kbuf != NULL)
            return -1;

        gKeystacking = false;
    }

    KBuf* kbuf;
    KBuf* temp;

    // allocate space for another keypress record
    kbuf=(KBuf*)throw_malloc(sizeof(KBuf));

    // find last record in linked list
    if((temp=gkbd.kbuf)!=NULL)
        for(; temp->next!=NULL; temp=temp->next);

    // add new record to end of linked list
    kbuf->next=NULL;
    kbuf->prev=temp;
    if(temp != NULL)
        temp->next=kbuf;

    // add keypress info to new record
    kbuf->xch=xch;

    //  Remember what was typed along with it.
    {
        kbuf->last = gkbd_last;
    }

    // if kbuf pointer was NULL, point it to new record
    if(gkbd.kbuf == NULL)
        gkbd.kbuf=kbuf;

    // return normally
    return 0;
}


//  ------------------------------------------------------------------
//  Put keys into the real keyboard buffer

gkey kbput_(gkey xch)
{

#if defined(__MSDOS__)

#if defined(__DJGPP__)
    if(gkbd.extkbd)
    {
        i86 cpu;

        cpu.ah(0x05);
        cpu.cx((word)xch);
        cpu.genint(0x16);
    }
    else
    {
#endif

#define BufStart (word)peek(0x40,0x80)
#define BufEnd   (word)peek(0x40,0x82)
#define BufHead  (word)peek(0x40,0x1A)
#define BufTail  (word)peek(0x40,0x1C)
#define BufTail_(a) poke(0x40,0x1C,(word)(a))

        word OldBufTail;

        OldBufTail = BufTail;
        if(BufTail == BufEnd-2)
            BufTail_(BufStart);
        else
            BufTail_(BufTail+2);

        if(BufTail == BufHead)
            BufTail_(OldBufTail);
        else
        {
            poke(0x40, OldBufTail, xch);
        }

#if defined(__DJGPP__)
    }
#endif

#endif

    return xch;
}


//  ------------------------------------------------------------------
//  Put keys into the real keyboard buffer

void kbputs_(char* str)
{

    char* p;

    for(p=str; *p ; p++)
        kbput_(gkey((scancode(*p)<<8)|*p));
}


//  ------------------------------------------------------------------
//  Change defined "on-key" list pointer

KBnd* chgonkey(KBnd* list)
{

    KBnd* temp;

    temp = gkbd.onkey;
    gkbd.onkey = list;

    return temp;
}


//  ------------------------------------------------------------------
//  Frees all active onkey definitions from memory

void freonkey()
{

    KBnd* temp;

    // free all onkey records in linked list
    while(gkbd.onkey!=NULL)
    {
        temp = gkbd.onkey->prev;
        throw_free(gkbd.onkey);
        gkbd.onkey = temp;
    }
}


//  ------------------------------------------------------------------
//  Attaches/detaches a key to a function

int setonkey(gkey keycode, VfvCP func, gkey pass)
{

    // search for a keycode that is already defined
    KBnd* onkey = gkbd.onkey;
    while(onkey)
    {
        if(onkey->keycode == keycode)
            break;
        onkey = onkey->prev;
    }

    // check to see if a key detachment is being requested
    if(func == NULL)
    {

        // if no defined onkey was found, then error
        if(onkey == NULL)
            return 2;

        // delete record from linked list
        KBnd* prev = onkey->prev;
        KBnd* next = onkey->next;
        if(prev)
            prev->next = next;
        if(next)
            next->prev = prev;
        if(onkey == gkbd.onkey)
            gkbd.onkey = prev;

        // free memory allocated for deleted record
        throw_free(onkey);
    }
    else
    {

        // if key was found, change func pointer
        // otherwise create a new onkey record
        if(onkey)
            onkey->func = func;
        else
        {

            // allocate memory for new record
            onkey = (KBnd*)throw_malloc(sizeof(KBnd));
            if(onkey == NULL)
                return 1;

            // add new record to linked list
            if(gkbd.onkey)
                gkbd.onkey->next = onkey;
            onkey->prev = gkbd.onkey;
            onkey->next = NULL;
            gkbd.onkey = onkey;

            // save info in onkey record
            gkbd.onkey->keycode = keycode;
            gkbd.onkey->func = func;
            gkbd.onkey->pass = pass;
        }
    }

    // return normally
    return 0;
}


//  ------------------------------------------------------------------

gkey key_tolower(gkey __keycode)
{

    byte &ascii = KCodAsc(__keycode);
    if(g_isupper(ascii))
        ascii = g_tolower(ascii);
    return __keycode;
}


//  ------------------------------------------------------------------
