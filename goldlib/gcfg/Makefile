# -*- makefile -*-

ifeq ($(TERM),cygwin)
MINGW=true
else
ifeq ($(OSTYPE),msys)
MINGW=true
endif
endif

TOP=../..
TARGET=gcfg
INCS=-I$(TOP)/goldlib -I$(TOP)/goldlib/gall -I$(TOP)/goldlib/gcfg
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

include $(TOP)/GNUmakef.inc
include $(TOP)/GNUmakef.lib
