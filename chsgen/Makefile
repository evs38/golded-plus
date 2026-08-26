TOP=..

include $(TOP)/GNUmakef.def

TARGET=chsgn$(PLATFORM)$(EXE)
UWIDGEN=uwidgen$(PLATFORM)$(EXE)
UWIDTH=$(TOP)/goldlib/gall/guwidth.inc

all: $(TARGET)

$(TARGET):  chsgen.cc
	$(CXX) $(LNKFLAGS) $(CPPFLAGS) -o $@ $<

$(UWIDGEN): uwidgen.cc
	$(CXX) $(LNKFLAGS) $(CPPFLAGS) -o $@ $<

#  Rebuild the character-width tables from the Unicode Character
#  Database files in this directory. See README.txt.
uwidth: $(UWIDGEN)
	./$(UWIDGEN) . $(UWIDTH)
