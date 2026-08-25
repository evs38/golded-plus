# -*- makefile -*-

ifeq ($(TERM),cygwin)
MINGW=true
else
ifeq ($(OSTYPE),msys)
MINGW=true
endif
endif

#  PLATFORM=cyg is the mingw target - a native Win32 binary built with
#  the Cygwin toolchain - so say so outright instead of inferring it from
#  $TERM, which a non-interactive build does not set at all.
ifeq ($(PLATFORM),cyg)
MINGW=true
endif

#  Open Watcom wants the POSIX regex replacement on every target, and the
#  Win32 import libraries on its Win32 one. Two flags, because those are
#  two different questions.
ifneq ($(filter $(PLATFORM),wcn wco wcx wcl),)
WATCOMBUILD=true
endif
ifeq ($(PLATFORM),wcn)
WIN32BUILD=true
endif

TOP=..

include $(TOP)/Config.def
SHORTTARGET=ged
TARGET=golded3

GLIBS=gmb3 gcfg uulib gall gcui smblib
ifneq ($(findstring GCFG_NO_MYSPELL, $(CPPFLAGS)), GCFG_NO_MYSPELL)
#  The Watcom targets turn the spellchecker off in GNUmakef.def, which
#  is included further down - too late for the test above to see it.
ifeq ($(filter $(PLATFORM),wcn wco wcx wcl),)
GLIBS+=hunspell
endif
endif

INCS=-I. -I$(TOP)/goldlib/gall -I$(TOP)/goldlib/gcui -I$(TOP)/goldlib/gcfg -I$(TOP)/goldlib/gmb3 -I$(TOP)/goldlib/uulib -I$(TOP)/goldlib/smblib

ifeq ($(PLATFORM),emx)
#  os2me is MMPM/2's import library.  The two MCI entry points this
#  build uses are resolved at run time with DosQueryProcAddr - see
#  goldlib/gall/gutlos2.cpp - so the library is only wanted where the
#  toolchain has one.  EMX shipped it; kLIBC does not, and asking for
#  it there fails the link.
GOLD_OS2ME=$(wildcard $(EMXPATH)/lib/os2me.a /emx/lib/os2me.a /@unixroot/usr/lib/os2me.a /@unixroot/usr/lib/libos2me.a)
ifneq ($(GOLD_OS2ME),)
#  += , not = : GNUmakef.def has already put -los2_unicode_lazy here
#  for ULS, and an assignment would drop it on any host that does have
#  os2me.
STDLIBS+=-los2me
endif
GLIBS+=glibc
INCS+=-I$(TOP)/goldlib/glibc
else
ifdef MINGW
STDLIBS=-luser32 -lwinmm
GLIBS+=glibc
INCS+=-I$(TOP)/goldlib/glibc
endif
ifdef WIN32BUILD
STDLIBS=user32.lib winmm.lib
endif
ifdef WATCOMBUILD
GLIBS+=glibc
INCS+=-I$(TOP)/goldlib/glibc
endif
endif

include $(TOP)/GNUmakef.inc

ifeq ($(USE_NCURSES),1)
STDLIBS+= $(LIBCURSES)
endif

include $(TOP)/GNUmakef.prg
