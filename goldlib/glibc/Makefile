# -*- makefile -*-

TOP=../..
TARGET=glibc
INCS=-I$(TOP)/goldlib -I$(TOP)/goldlib/glibc -I$(TOP)/goldlib/gall
CFLAGS=-DHAVE_CONFIG_H

#  wcl borrows the lnx source lists everywhere else, but not here: Open
#  Watcom's Linux runtime has no POSIX regex, so it needs the same
#  replacement its other targets do.
ifeq ($(PLATFORM),wcl)
SRCTAG=wcn
endif

include $(TOP)/GNUmakef.inc
include $(TOP)/GNUmakef.lib
