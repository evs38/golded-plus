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
//  GCUI: Golded+ Character-oriented User Interface.
//  Keyboard functions (declarations).
//  ------------------------------------------------------------------

#ifndef __gkbdbase_h
#define __gkbdbase_h


//  ------------------------------------------------------------------

#include <gtimall.h>


//  ------------------------------------------------------------------
//  Simple types

typedef word gkey;

// Operation modes for reading keystrokes, using in the following functions
// kbxget, kbxget_raw and gkbd_cursgetch.
enum eKeyModes
{
    // Waits for a pressed key and returns a code
    KeyMode_Wait = 0,
    // Returns a keystroke if available, otherwise 0
    KeyMode_Test,
    // Returns Shifts key status
    KeyMode_Shift,
    // Returns Control key status
    KeyMode_Control
};


//  ------------------------------------------------------------------
//  Keycode object

inline gkey& KCodKey(gkey &key)
{
    return key;
}
inline byte& KCodAsc(gkey &key)
{
    return *(((byte *)&key)+0);
}
inline byte& KCodScn(gkey &key)
{
    return *(((byte *)&key)+1);
}


//  ------------------------------------------------------------------
//  What a key was typed as, in the local charset.
//
//  A gkey has one byte for the character, which was enough while a
//  character was a byte. Rather than widening it - and every table and
//  binding built on it - the text travels alongside, and the few places
//  that actually insert into a message pick it up here.
//
//  One record rather than loose variables: it is saved, restored,
//  pushed back and buffered in four places, and as separate fields
//  those copies drifted - one of the buffers ended up smaller than the
//  global it was copied from, so restoring read past its end.

struct GKbdChar
{
    //  Longer than one character on purpose: converting a codepoint
    //  into a single-byte charset may transliterate it into several -
    //  the German sharp s becomes "ss".
    int  len;           // bytes in buf, 0 if the key was not a character
    char buf[16];       // the character in the local charset
};

//  The last key as text in the local charset - its UTF-8 encoding in
//  UTF-8 mode, the single byte otherwise. Empty when the last key was
//  not a character.

const char* gkbd_lastchars(int* len);

//  Put a character back into that side channel - used when a key
//  comes out of GoldED's own buffer rather than from the keyboard.
void gkbd_setlastchars(const char* chars, int len);

//  The bytes 'key' stands for, or NULL when it was not a whole
//  character - see the note on the definition.
const char* gkbd_keychars(gkey key, int* len, bool checkfirst = true);


//  ------------------------------------------------------------------
//  Definition of kbuf record

struct KBuf
{
    KBuf* prev;         // previous record
    KBuf* next;         // next record
    gkey   xch;         // keypress
    //  What the key was typed as, so that a key put back here and read
    //  again is still the character it was. Without this a Cyrillic
    //  letter that started an edit arrived as one stray byte.
    GKbdChar last;
};


//  ------------------------------------------------------------------
//  Definition of onkey record

struct KBnd
{
    KBnd* prev;         // pointer to previous record
    KBnd* next;         // pointer to next record
    gkey  keycode;      // Scan/ASCII code of trap key
    VfvCP func;         // address of onkey function
    gkey  pass;         // key to pass back, 0=don't pass
};


//  ------------------------------------------------------------------
//  Definition of keyboard info record

class GKbd
{

public:

    KBuf*  kbuf;           // Pointer to head record in key buffer
    KBnd*  onkey;          // Pointer to head record in onkey list
    KBnd*  curronkey;      // Pointer to current onkey record
    int    inmenu;         // In-menu flag used by menuing functions
    int    source;         // Source of keypress 0=kb, 1=kbuf, 2=mouse
    int    extkbd;         // Extended keyboard 0=none, 1=yes
    int    polling;        // Keyboard polling enabled
    Clock  tickinterval;   // Minimum interval between ticks
    Clock  tickvalue;      // Value from last tick
    VfvCP  tickfunc;       // Function to call when a tick is generated
    Clock  tickpress;      // Tick value at last keypress
    bool   inidle;         // In-idle flag used by tickfunc
    bool   quitall;        // Quit-all flag for menus etc.
    //  The terminal changed size and the screen has not been laid out
    //  again since. Raised with every Key_Resize; whoever owns the
    //  screen clears it once the new size has been applied.
    bool   resize_pending;

    void Init();
    GKbd();
    ~GKbd();
};

extern GKbd gkbd;


//  ------------------------------------------------------------------
//  Keyboard status codes returned from kbstat()

#define RSHIFT      1       // right shift pressed
#define LSHIFT      2       // left shift pressed
#define GCTRL       4       // [Ctrl] pressed
#define ALT         8       // [Alt] pressed
#define SCRLOCK     16      // [Scroll Lock] toggled
#define NUMLOCK     32      // [Num Lock] toggled
#define CAPSLOCK    64      // [Caps Lock] toggled
#define INS         128     // [Ins] toggled


//  ------------------------------------------------------------------

extern gkey scancode_table[];
extern bool right_alt_same_as_left;


//  ------------------------------------------------------------------
//  Function prototypes

gkey  kbxget_raw(eKeyModes mode);
KBnd* chgonkey  (KBnd* kblist);
void  clearkeys ();
void  freonkey  ();
int   setonkey  (gkey keycode, VfvCP func, gkey pass);
gkey  getxch    (int __tick=false);
void  kbclear   ();
gkey  kbmhit    ();
gkey  kbxget    (eKeyModes mode=KeyMode_Wait);
gkey  kbxhit    ();
int   kbput     (gkey xch);
word  kbput_    (gkey xch);
void  kbputs_   (char* str);
byte  scancode  (gkey ch);
gkey  waitkey   ();
gkey  waitkeyt  (int duration);

gkey  key_tolower(gkey __keycode);

gkey  keyscanxlat(gkey k);

gkey  __kbxget(int __mode=0, long __ticks=0, VfvCP __idlefunc=NULL);

void gkbdtickpressreset();
void gkbdtickvaluereset();


//  ------------------------------------------------------------------
//  Inline functions

inline gkey getxchtick()
{
    return getxch(true);
}
inline void kbdsettickfunc(VfvCP func)
{
    gkbd.tickfunc = func;
}


//  ------------------------------------------------------------------
//  Shorthand definitions of keyboard scancodes

#define  KEY_BRK  0xFFFF     // ^Break return from _KeyHit()/_KeyGet()


//  ------------------------------------------------------------------


#if defined(__USE_NCURSES__)
    // TODO: move L_KEY_BASE то KEY_MAX-9,
    // TODO: change array gkbd_curstable to std::map
    #define L_KEY_BASE	(KEY_RESIZE+12)
    #define L_KEY_AUP     (L_KEY_BASE+0)
    #define L_KEY_ADOWN   (L_KEY_BASE+1)
    #define L_KEY_ARIGHT  (L_KEY_BASE+2)
    #define L_KEY_ALEFT   (L_KEY_BASE+3)
    #define L_KEY_CUP     (L_KEY_BASE+4)
    #define L_KEY_CDOWN   (L_KEY_BASE+5)
    #define L_KEY_CRIGHT  (L_KEY_BASE+6)
    #define L_KEY_CLEFT   (L_KEY_BASE+7)
    #define L_KEY_UNUSED  (L_KEY_BASE+8)
#endif

#endif

//  ------------------------------------------------------------------
