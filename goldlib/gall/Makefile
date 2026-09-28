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
#  $TERM, which a non-interactive build does not set at all. The
#  mingw.org toolchain has no <regex.h> of its own and takes it from
#  goldlib/glibc, as the -mno-cygwin build always did.
ifeq ($(PLATFORM),cyg)
MINGW=true
endif

TOP=../..
TARGET=gall
INCS=-I$(TOP)/goldlib -I$(TOP)/goldlib/gall
ifdef MINGW
INCS+=-I$(TOP)/goldlib/glibc
else
ifneq ($(filter $(PLATFORM),wcn wco wcx wcl),)
#  Open Watcom ships no POSIX regex on any of its targets;
#  goldlib/glibc supplies it.
INCS+=-I$(TOP)/goldlib/glibc
else
ifeq ($(PLATFORM),emx)
INCS+=-I$(TOP)/goldlib/glibc
endif
endif
endif

include $(TOP)/Config.def
include $(TOP)/GNUmakef.inc

ifeq ($(PLATFORM),emx)
ifeq ($(AR),emxomfar)
AR += -p32
endif
endif

include $(TOP)/GNUmakef.lib

#  ICU, for the transliterator only - see ICU_CFLAGS in GNUmakef.def.
$(FOBJPATH)/gtranslit$(OBJEXT): CPPFLAGS+=$(ICU_CFLAGS)
