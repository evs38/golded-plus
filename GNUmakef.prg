# -*- makefile -*-

.PHONY: $(TARGET) $(GLIBS)

$(TARGET): $(TOP)/$(BIN)/$(SHORTTARGET)$(PLATFORM)$(EXEEXT)

ifeq ($(PLATFORM),emx)
LIBS=$(addprefix -llib,$(GLIBS))
else
ifneq ($(filter $(PLATFORM),wcn wco wcx wcl),)
#  Watcom libraries are named in full: -l would have owcc translate the
#  name into lib<name>.a on its own search path, and these are ours, in
#  our tree, with the extension wlib gave them.
LIBS=$(addprefix $(FLIBPATH)/lib,$(addsuffix $(LIBEXT),$(GLIBS)))
else
LIBS=$(addprefix -l,$(GLIBS))
endif
endif
LIBS+=$(STDLIBS)
FGLIBS=$(addprefix $(FLIBPATH)/lib, $(addsuffix $(LIBEXT), $(GLIBS)))

$(TOP)/$(BIN)/$(SHORTTARGET)$(PLATFORM)$(EXEEXT): $(OBJS) $(FGLIBS) $(ADDS)
	@echo -n Linking $(TARGET)...
	@$(CXX) $(LNKFLAGS) -o $@ $(FOBJPATH)/*$(OBJEXT) $(ADDS) $(LIBS) -L$(FLIBPATH)
	@echo done
ifeq ($(PLATFORM),emx)
#  A .res is not something the linker reads, so the icon is bound onto
#  the executable here, once it exists. Only wrc is asked to do it:
#  that is the resource compiler the cross toolchain ships and it has
#  been tried, while a native build is left exactly as it always
#  behaved. Programs without a .rc of their own - goldnode, rddt -
#  have no .res and fall straight through.
	@res=`ls $(FOBJPATH)/*.res 2>/dev/null | head -1` ;			\
	if [ -n "$(GOLD_OS2WRC)" ] && [ -s "$$res" ] ; then			\
		echo -n "Binding resources..." ;				\
		$(GOLD_OS2WRC) -bt=os2 -q "$$res" $@ && echo done ;		\
	fi
endif

$(FGLIBS): $(GLIBS)

$(GLIBS):
	@cd $(TOP)/goldlib/$@; $(MAKE); cd `pwd`
