# -*- makefile -*-

ifeq ($(TERM),cygwin)
MINGW=true
else
ifeq ($(OSTYPE),msys)
MINGW=true
endif
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
