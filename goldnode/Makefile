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
SHORTTARGET=gn
TARGET=goldnode
INCS=-I$(TOP)/goldlib -I$(TOP)/goldlib/gall -I$(TOP)/goldlib/gcfg -I$(TOP)/golded3/
ifdef MINGW
STDLIBS=-luser32
endif
ifdef WIN32BUILD
STDLIBS=user32.lib
endif
ifdef WATCOMBUILD
INCS+=-I$(TOP)/goldlib/glibc
endif
GLIBS=gall
ifdef WATCOMBUILD
GLIBS+=glibc
endif
CLEANS=
ADDS+=$(TOP)/$(OBJPATH)/$(PLATFORM)/golded3/golded3$(OBJEXT)

include $(TOP)/GNUmakef.inc
include $(TOP)/GNUmakef.prg
