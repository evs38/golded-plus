# -*- makefile -*-
# $Id$
# Include file for GNU C++ build

.PHONY: all clean sourcelist

all: $(TARGET)

include $(TOP)/GNUmakef.def

# Messagebase formats
include $(TOP)/MSGBase.def

# Config files formats
include $(TOP)/Config.def

FOBJPATH=$(TOP)/$(OBJPATH)/$(PLATFORM)/$(TARGET)
FDEPPATH=$(TOP)/$(OBJPATH)/$(PLATFORM)/$(TARGET)
FLIBPATH=$(TOP)/$(LIBPATH)/$(PLATFORM)

.SUFFIXES: .c .cpp .all .rc .cxx

#  Which tag in the .all lists selects this platform's sources. Normally
#  the platform's own name; a target that shares another's source set -
#  Watcom for Linux, say - points SRCTAG at that one instead.
SRCTAG?=$(PLATFORM)

sourcelist: bld$(PLATFORM).inc

bld$(PLATFORM).inc: $(TARGET).all
	@echo making sourcelist
	@$(GREP) -w $(SRCTAG) $< \
		| $(SED) 's;^\([[:alnum:]_]*\)[ ]*\([[:alnum:]_]*\).*;SOURCES+=\1.\2;' > $@; [ -s $@ ] || rm -f $@

$(FOBJPATH)/%$(OBJEXT): %.cpp
	@echo building $(basename $<)$(OBJEXT)
	@$(CXX) -c -MD $(CPPFLAGS) -o $@ $<
	@$(SHELL) -c "[ -s $(patsubst %.cpp,%.d,$<) ] &&					\
		echo -n $(FOBJPATH)/>$(FDEPPATH)/$(patsubst %.cpp,%.d,$<) &&		\
		$(CAT) $(patsubst %.cpp,%.d,$<)>>$(FDEPPATH)/$(patsubst %.cpp,%.d,$<) &&\
		rm $(patsubst %.cpp,%.d,$<) || true"

$(FOBJPATH)/%$(OBJEXT): %.cxx
	@echo building $(basename $<)$(OBJEXT)
	@$(CXX) -c -MD $(CPPFLAGS) -o $@ $<
	@$(SHELL) -c "[ -s $(patsubst %.cxx,%.d,$<) ] &&					\
		echo -n $(FOBJPATH)/>$(FDEPPATH)/$(patsubst %.cxx,%.d,$<) &&		\
		$(CAT) $(patsubst %.cxx,%.d,$<)>>$(FDEPPATH)/$(patsubst %.cxx,%.d,$<) &&\
		rm $(patsubst %.cxx,%.d,$<) || true"

$(FOBJPATH)/%$(OBJEXT): %.c
	@echo building $(basename $<)$(OBJEXT)
	@$(CC) -c -MD $(CFLAGS) -o $@ $<
	@$(SHELL) -c "[ -s $(patsubst %.c,%.d,$<) ] &&					\
		echo -n $(FOBJPATH)/>$(FDEPPATH)/$(patsubst %.c,%.d,$<) &&		\
		$(CAT) $(patsubst %.c,%.d,$<)>>$(FDEPPATH)/$(patsubst %.c,%.d,$<) &&	\
		rm $(patsubst %.c,%.d,$<) || true"

ifeq ($(PLATFORM),cyg)
$(FOBJPATH)/%$(OBJEXT): %.rc
	@echo creating resources
	@$(WINDRES) --use-temp-file -o $@ $<
endif

ifeq ($(PLATFORM),emx)
#  The icon goes on in two steps: a resource compiler turns the .rc
#  into a .res, and the .res is bound onto the finished executable
#  afterwards - the linker never reads one, it is handed the object
#  files and nothing else.
#
#  Two programs can compile it. OS/2's own `rc' is what a native build
#  has, and is called exactly as it always was. Open Watcom's `wrc'
#  is a host binary the cross toolchain ships, so a build off the
#  machine can have the icon too; it needs telling where the target's
#  headers are, since the .rc includes <os2.h>, and the compiler is
#  asked for that the way liblocal.a is looked for elsewhere.
#
#  Where neither is on PATH the step is skipped with a note. Nothing
#  is lost but the icon.
GOLD_OS2RC=$(shell command -v rc 2>/dev/null)
GOLD_OS2WRC=$(shell command -v wrc 2>/dev/null)
GOLD_OS2WRCINC=$(shell $(CC) -print-sysroot 2>/dev/null)/include
$(FOBJPATH)/%.res: %.rc
	@echo creating resources
	@if [ -n "$(GOLD_OS2RC)" ] ; then					\
		$(GOLD_OS2RC) $(subst -I,-i ,$(INCS)) -x1 -r $< ;		\
		touch $@ ;							\
	elif [ -n "$(GOLD_OS2WRC)" ] ; then					\
		$(GOLD_OS2WRC) -bt=os2 -q -r -fo=$@ -i=$(GOLD_OS2WRCINC)	\
			$(if $(GOLD_OS2TK_INC),-i=$(GOLD_OS2TK_INC))		\
			$(subst -I,-i=,$(INCS)) $< ;				\
	else									\
		echo "  no resource compiler on this host - icon left out" ;	\
		touch $@ ;							\
	fi
endif

SOURCES=
ifeq ($(wildcard $(TOP)/$(OBJPATH)/$(PLATFORM)/source.ls?),$(TOP)/$(OBJPATH)/$(PLATFORM)/source.lst)
include bld$(PLATFORM).inc
endif

ifeq ($(PLATFORM),cyg)
OBJS=$(addprefix $(FOBJPATH)/,$(patsubst %.rc,%$(OBJEXT),$(patsubst %.c,%$(OBJEXT),$(patsubst %.cpp,%$(OBJEXT),$(patsubst %.cxx,%$(OBJEXT),$(filter %.c %.cpp %.cxx %.rc,$(SOURCES)))))))
else
ifeq ($(PLATFORM),emx)
OBJS=$(addprefix $(FOBJPATH)/,$(patsubst %.rc,%.res,$(patsubst %.c,%$(OBJEXT),$(patsubst %.cpp,%$(OBJEXT),$(patsubst %.cxx,%$(OBJEXT),$(filter %.c %.cpp %.cxx %.rc,$(SOURCES)))))))
else
OBJS=$(addprefix $(FOBJPATH)/,$(patsubst %.c,%$(OBJEXT),$(patsubst %.cpp,%$(OBJEXT),$(patsubst %.cxx,%$(OBJEXT),$(filter %.c %.cpp %.cxx %.rc,$(SOURCES))))))
endif
endif
DEPS = $(wildcard $(FDEPPATH)/*.d)
ALLOBJS = $(wildcard $(FOBJPATH)/*.o)

clean:
	@echo cleaning...
	@-rm -f $(ALLOBJS) $(DEPS) bld$(PLATFORM).inc

ifneq ($(DEPS),)
-include $(DEPS)
endif
