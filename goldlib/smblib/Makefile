# -*- makefile -*-

TOP=../..
TARGET=smblib
INCS=-I$(TOP)/goldlib -I$(TOP)/goldlib/smblib -I$(TOP)/goldlib/gall

#  Both sources here are .cpp, so the compiler picks C++ from the name.
#  Forcing it with `-x c++' did nothing for gcc and made Open Watcom's
#  driver read `c++' as a file to compile.

include $(TOP)/GNUmakef.inc
include $(TOP)/GNUmakef.lib
