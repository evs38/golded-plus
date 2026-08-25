#  ------------------------------------------------------------------
#  GoldED+ built with Open Watcom's own toolkit.
#
#  This one drives wpp386, wcc386, wlib and wlink directly and needs
#  nothing from the GNU side. The four targets are the same four the
#  GNU makefile builds:
#
#      wmake -f gedwcc.mak                Win32
#      wmake -f gedwcc.mak -h DOS32=1     32-bit DOS, DOS/4GW
#      wmake -f gedwcc.mak -h OS2=1       OS/2 32-bit
#      wmake -f gedwcc.mak -h LINUX=1     Linux 32-bit (cross)
#      wmake -f gedwcc.mak clean          remove what one of them built
#
#  gedwcc.bat sets the environment and passes these through, which is
#  the easier way in:  gedwcc dos
#
#  Each target keeps its own object and library directories, so they do
#  not tread on each other and none needs cleaning before the next.
#
#  Set WATCOM if the compiler is not where its installer puts it.
#
#  The source lists below come from the .all files, by the tag each
#  target has carried since the 1990s - wcn, wco and wcx - except the
#  Linux one, which borrows the `lnx' lists: which compiler builds a
#  Linux binary does not change which sources it needs. They are
#  written out by hand; when a source is added to a .all file, add it
#  here too.
#
#  The object rules track no header dependencies. Watcom can do it with
#  .AUTODEPEND, but only for objects it has already built once, so it
#  would be right on the second build and wrong on the first. After
#  editing a header, clean before building.
#  ------------------------------------------------------------------

!ifndef WATCOM
WATCOM = C:\WATCOM
!endif

#  Where OS/2 keeps its system DLLs, for the import library below.
!ifndef OS2DLLPATH
OS2DLLPATH = C:\OS2\DLL
!endif

#  Per-target extras; empty for most.
ULSLIB  =
TGTDEFS =

!ifdef DOS32
BT      = dos
LSYSTEM = dos4g
OBJDIR  = obj\wcx
LIBDIR  = lib\wcx
SFX     = wcx
SYSINC  = -i=$(WATCOM)\h
SYSLIBS =
EXEEXT  = .exe
!else
!ifdef OS2
BT      = os2
LSYSTEM = os2v2
OBJDIR  = obj\wco
LIBDIR  = lib\wco
SFX     = wco
SYSINC  = -i=$(WATCOM)\h -i=$(WATCOM)\h\os2
#  Charset conversion here is OS/2's own, ULS, so this target never
#  reaches the built-in tables.  Watcom has the headers but ships no
#  import library for UCONV.DLL, so one is made below.
ULSLIB  = $(LIBDIR)\uconv.lib
TGTDEFS = -dHAS_ULS
SYSLIBS = $(ULSLIB)
EXEEXT  = .exe
!else
!ifdef LINUX
BT      = linux
LSYSTEM = linux
OBJDIR  = obj\wcl
LIBDIR  = lib\wcl
SFX     = wcl
SYSINC  = -i=$(WATCOM)\lh
SYSLIBS =
EXEEXT  =
!else
BT      = nt
LSYSTEM = nt
OBJDIR  = obj\wcn
LIBDIR  = lib\wcn
SFX     = wcn
SYSINC  = -i=$(WATCOM)\h -i=$(WATCOM)\h\nt
SYSLIBS = user32.lib winmm.lib
EXEEXT  = .exe
!endif
!endif
!endif

BINDIR = bin

#  Which directory the tools themselves live in depends on the host
#  running wmake, not on the target being built: binnt on Windows,
#  binp on OS/2.  Set WHOST by hand for a host not named here.
!ifndef WHOST
!ifdef __OS2__
WHOST = binp
!else
WHOST = binnt
!endif
!endif

CXX  = $(WATCOM)\$(WHOST)\wpp386
CC   = $(WATCOM)\$(WHOST)\wcc386
LIBR = $(WATCOM)\$(WHOST)\wlib
LINK = $(WATCOM)\$(WHOST)\wlink

INCS = -i=goldlib -i=goldlib\gall -i=goldlib\gcui -i=goldlib\gcfg &
       -i=goldlib\gmb3 -i=goldlib\smblib -i=goldlib\uulib &
       -i=goldlib\glibc -i=golded3 -i=. $(SYSINC)

#  -DNOMINMAX: Watcom's <windows.h> makes min() and max() macros, which
#  wrecks <limits> - std::numeric_limits has members by those names.
#
#  No internal spellchecker on these targets: the bundled hunspell wants
#  parts of the C++ library Open Watcom has not got, and the MS Office
#  one wants COM. Everything else GoldED+ does is here.
DEFS = -dNDEBUG -dNOMINMAX -dGOLD_UTF8=1 &
       -dHAVE_CONFIG_H -dHAVE_STDARG_H -dHAVE_SNPRINTF -dHAVE_VSNPRINTF &
       -dGCFG_NO_MYSPELL -dGCFG_NO_MSSPELL

#  -zq quiet, -bt= target, -xs exception handling (owcc spells that -feh)
CXXFLAGS = -zq -bt=$(BT) -xs $(INCS) $(DEFS) $(TGTDEFS)
CFLAGS   = -zq -bt=$(BT) $(INCS) $(DEFS) $(TGTDEFS)

.EXTENSIONS:
.EXTENSIONS: .exe .lib .obj .cpp .c

.cpp: goldlib\gall;goldlib\gcui;goldlib\gcfg;goldlib\gmb3;goldlib\uulib;goldlib\smblib;goldlib\glibc;golded3;goldnode;rddt
.c:   goldlib\gall;goldlib\gcui;goldlib\gcfg;goldlib\gmb3;goldlib\uulib;goldlib\smblib;goldlib\glibc;golded3;goldnode;rddt

LIBS = $(LIBDIR)\gall.lib $(LIBDIR)\gcui.lib $(LIBDIR)\gcfg.lib &
       $(LIBDIR)\gmb3.lib $(LIBDIR)\uulib.lib $(LIBDIR)\smblib.lib &
       $(LIBDIR)\glibc.lib

GED = $(BINDIR)\ged$(SFX)$(EXEEXT)
GN  = $(BINDIR)\gn$(SFX)$(EXEEXT)
RD  = $(BINDIR)\rddt$(SFX)$(EXEEXT)

all: dirs $(GED) $(GN) $(RD) .SYMBOLIC
	@echo Built $(GED), $(GN) and $(RD)

dirs: .SYMBOLIC
#  obj\ and lib\ are made first: OS/2's mkdir, unlike the one cmd.exe
#  carries on Windows, will not create a missing parent directory.
	@if not exist obj mkdir obj
	@if not exist lib mkdir lib
	@if not exist $(OBJDIR) mkdir $(OBJDIR)
	@if not exist $(LIBDIR) mkdir $(LIBDIR)
	@if not exist $(BINDIR) mkdir $(BINDIR)

.cpp.obj:
	$(CXX) $(CXXFLAGS) -fo=$^@ $[@

.c.obj:
	$(CC) $(CFLAGS) -fo=$^@ $[@

!ifdef OS2
GALL_OBJS = &
	$(OBJDIR)\gcrc16tb.obj $(OBJDIR)\gcrc32tb.obj $(OBJDIR)\gcrchash.obj $(OBJDIR)\gcrckeyv.obj &
	$(OBJDIR)\gcrcm16.obj $(OBJDIR)\gcrcm32.obj $(OBJDIR)\gcrcs16.obj $(OBJDIR)\gcrcs32.obj &
	$(OBJDIR)\gdbgerr.obj $(OBJDIR)\gdbgexit.obj $(OBJDIR)\gdbgtrk.obj $(OBJDIR)\gdirposx.obj &
	$(OBJDIR)\geval.obj $(OBJDIR)\gevalhum.obj $(OBJDIR)\gevalrpn.obj $(OBJDIR)\gfile.obj &
	$(OBJDIR)\gfilport.obj $(OBJDIR)\gfilutl1.obj $(OBJDIR)\gfilutl2.obj $(OBJDIR)\gftnaddr.obj &
	$(OBJDIR)\gftnnl.obj $(OBJDIR)\gftnnlfd.obj $(OBJDIR)\gftnnlfu.obj $(OBJDIR)\gftnnlge.obj &
	$(OBJDIR)\gftnnlv7.obj $(OBJDIR)\glog.obj $(OBJDIR)\gmemdbg.obj $(OBJDIR)\gmemutil.obj &
	$(OBJDIR)\gmsgattr.obj $(OBJDIR)\ghdrmime.obj $(OBJDIR)\gprnutil.obj $(OBJDIR)\gsnd.obj &
	$(OBJDIR)\gsndwrap.obj $(OBJDIR)\gstrctyp.obj $(OBJDIR)\gstrmail.obj $(OBJDIR)\gstrname.obj &
	$(OBJDIR)\gstrutil.obj $(OBJDIR)\gtimjuld.obj $(OBJDIR)\gtimutil.obj $(OBJDIR)\gbmh.obj &
	$(OBJDIR)\gfuzzy.obj $(OBJDIR)\gregex.obj $(OBJDIR)\gwildmat.obj $(OBJDIR)\gsearch.obj &
	$(OBJDIR)\gtxtpara.obj $(OBJDIR)\gusrbase.obj $(OBJDIR)\gusrezyc.obj $(OBJDIR)\gusrgold.obj &
	$(OBJDIR)\gusrhuds.obj $(OBJDIR)\gusrmax.obj $(OBJDIR)\gusrpcb.obj $(OBJDIR)\gusrra2.obj &
	$(OBJDIR)\gusrxbbs.obj $(OBJDIR)\gutlclip.obj $(OBJDIR)\gutlcode.obj $(OBJDIR)\gutlgrp.obj &
	$(OBJDIR)\gutlmisc.obj $(OBJDIR)\gutlmtsk.obj $(OBJDIR)\gutltag.obj $(OBJDIR)\gutlvers.obj &
	$(OBJDIR)\gcharset.obj $(OBJDIR)\gutf8.obj $(OBJDIR)\grecode.obj $(OBJDIR)\giniprsr.obj &
	$(OBJDIR)\gutlos2.obj $(OBJDIR)\gutlos2m.obj $(OBJDIR)\gespell.obj

GALL_LIBOBJS = &
	+$(OBJDIR)\gcrc16tb.obj +$(OBJDIR)\gcrc32tb.obj +$(OBJDIR)\gcrchash.obj +$(OBJDIR)\gcrckeyv.obj &
	+$(OBJDIR)\gcrcm16.obj +$(OBJDIR)\gcrcm32.obj +$(OBJDIR)\gcrcs16.obj +$(OBJDIR)\gcrcs32.obj &
	+$(OBJDIR)\gdbgerr.obj +$(OBJDIR)\gdbgexit.obj +$(OBJDIR)\gdbgtrk.obj +$(OBJDIR)\gdirposx.obj &
	+$(OBJDIR)\geval.obj +$(OBJDIR)\gevalhum.obj +$(OBJDIR)\gevalrpn.obj +$(OBJDIR)\gfile.obj &
	+$(OBJDIR)\gfilport.obj +$(OBJDIR)\gfilutl1.obj +$(OBJDIR)\gfilutl2.obj +$(OBJDIR)\gftnaddr.obj &
	+$(OBJDIR)\gftnnl.obj +$(OBJDIR)\gftnnlfd.obj +$(OBJDIR)\gftnnlfu.obj +$(OBJDIR)\gftnnlge.obj &
	+$(OBJDIR)\gftnnlv7.obj +$(OBJDIR)\glog.obj +$(OBJDIR)\gmemdbg.obj +$(OBJDIR)\gmemutil.obj &
	+$(OBJDIR)\gmsgattr.obj +$(OBJDIR)\ghdrmime.obj +$(OBJDIR)\gprnutil.obj +$(OBJDIR)\gsnd.obj &
	+$(OBJDIR)\gsndwrap.obj +$(OBJDIR)\gstrctyp.obj +$(OBJDIR)\gstrmail.obj +$(OBJDIR)\gstrname.obj &
	+$(OBJDIR)\gstrutil.obj +$(OBJDIR)\gtimjuld.obj +$(OBJDIR)\gtimutil.obj +$(OBJDIR)\gbmh.obj &
	+$(OBJDIR)\gfuzzy.obj +$(OBJDIR)\gregex.obj +$(OBJDIR)\gwildmat.obj +$(OBJDIR)\gsearch.obj &
	+$(OBJDIR)\gtxtpara.obj +$(OBJDIR)\gusrbase.obj +$(OBJDIR)\gusrezyc.obj +$(OBJDIR)\gusrgold.obj &
	+$(OBJDIR)\gusrhuds.obj +$(OBJDIR)\gusrmax.obj +$(OBJDIR)\gusrpcb.obj +$(OBJDIR)\gusrra2.obj &
	+$(OBJDIR)\gusrxbbs.obj +$(OBJDIR)\gutlclip.obj +$(OBJDIR)\gutlcode.obj +$(OBJDIR)\gutlgrp.obj &
	+$(OBJDIR)\gutlmisc.obj +$(OBJDIR)\gutlmtsk.obj +$(OBJDIR)\gutltag.obj +$(OBJDIR)\gutlvers.obj &
	+$(OBJDIR)\gcharset.obj +$(OBJDIR)\gutf8.obj +$(OBJDIR)\grecode.obj +$(OBJDIR)\giniprsr.obj &
	+$(OBJDIR)\gutlos2.obj +$(OBJDIR)\gutlos2m.obj +$(OBJDIR)\gespell.obj

GCUI_OBJS = &
	$(OBJDIR)\gkbdbase.obj $(OBJDIR)\gkbdgetm.obj $(OBJDIR)\gkbdwait.obj $(OBJDIR)\gsrchmgr.obj &
	$(OBJDIR)\gmoubase.obj $(OBJDIR)\gvidbase.obj $(OBJDIR)\gvidinit.obj $(OBJDIR)\gwinbase.obj &
	$(OBJDIR)\gwindow.obj $(OBJDIR)\gwinhlp1.obj $(OBJDIR)\gwinhlp2.obj $(OBJDIR)\gwininit.obj &
	$(OBJDIR)\gwinline.obj $(OBJDIR)\gwinmenu.obj $(OBJDIR)\gwinmnub.obj $(OBJDIR)\gwinpckf.obj &
	$(OBJDIR)\gwinpcks.obj $(OBJDIR)\gwinpick.obj $(OBJDIR)\gwinput2.obj

GCUI_LIBOBJS = &
	+$(OBJDIR)\gkbdbase.obj +$(OBJDIR)\gkbdgetm.obj +$(OBJDIR)\gkbdwait.obj +$(OBJDIR)\gsrchmgr.obj &
	+$(OBJDIR)\gmoubase.obj +$(OBJDIR)\gvidbase.obj +$(OBJDIR)\gvidinit.obj +$(OBJDIR)\gwinbase.obj &
	+$(OBJDIR)\gwindow.obj +$(OBJDIR)\gwinhlp1.obj +$(OBJDIR)\gwinhlp2.obj +$(OBJDIR)\gwininit.obj &
	+$(OBJDIR)\gwinline.obj +$(OBJDIR)\gwinmenu.obj +$(OBJDIR)\gwinmnub.obj +$(OBJDIR)\gwinpckf.obj &
	+$(OBJDIR)\gwinpcks.obj +$(OBJDIR)\gwinpick.obj +$(OBJDIR)\gwinput2.obj

GCFG_OBJS = &
	$(OBJDIR)\gedacfg.obj $(OBJDIR)\gxareas.obj $(OBJDIR)\gxcrash.obj $(OBJDIR)\gxdb.obj &
	$(OBJDIR)\gxdutch.obj $(OBJDIR)\gxezy102.obj $(OBJDIR)\gxezy110.obj $(OBJDIR)\gxfd.obj &
	$(OBJDIR)\gxfecho4.obj $(OBJDIR)\gxfecho5.obj $(OBJDIR)\gxfecho6.obj $(OBJDIR)\gxfidpcb.obj &
	$(OBJDIR)\gxfm092.obj $(OBJDIR)\gxfm100.obj $(OBJDIR)\gxfm116.obj $(OBJDIR)\gxgecho.obj &
	$(OBJDIR)\gxhpt.obj $(OBJDIR)\gximail4.obj $(OBJDIR)\gximail5.obj $(OBJDIR)\gximail6.obj &
	$(OBJDIR)\gxinter.obj $(OBJDIR)\gxlora.obj $(OBJDIR)\gxmax3.obj $(OBJDIR)\gxme2.obj &
	$(OBJDIR)\gxopus.obj $(OBJDIR)\gxpcb.obj $(OBJDIR)\gxportal.obj $(OBJDIR)\gxprobrd.obj &
	$(OBJDIR)\gxqfront.obj $(OBJDIR)\gxqecho.obj $(OBJDIR)\gxquick.obj $(OBJDIR)\gxra.obj &
	$(OBJDIR)\gxraecho.obj $(OBJDIR)\gxspace.obj $(OBJDIR)\gxsquish.obj $(OBJDIR)\gxsuper.obj &
	$(OBJDIR)\gxsync.obj $(OBJDIR)\gxtimed.obj $(OBJDIR)\gxtmail.obj $(OBJDIR)\gxts.obj &
	$(OBJDIR)\gxwmail.obj $(OBJDIR)\gxwtr.obj $(OBJDIR)\gxxbbs.obj $(OBJDIR)\gxxmail.obj

GCFG_LIBOBJS = &
	+$(OBJDIR)\gedacfg.obj +$(OBJDIR)\gxareas.obj +$(OBJDIR)\gxcrash.obj +$(OBJDIR)\gxdb.obj &
	+$(OBJDIR)\gxdutch.obj +$(OBJDIR)\gxezy102.obj +$(OBJDIR)\gxezy110.obj +$(OBJDIR)\gxfd.obj &
	+$(OBJDIR)\gxfecho4.obj +$(OBJDIR)\gxfecho5.obj +$(OBJDIR)\gxfecho6.obj +$(OBJDIR)\gxfidpcb.obj &
	+$(OBJDIR)\gxfm092.obj +$(OBJDIR)\gxfm100.obj +$(OBJDIR)\gxfm116.obj +$(OBJDIR)\gxgecho.obj &
	+$(OBJDIR)\gxhpt.obj +$(OBJDIR)\gximail4.obj +$(OBJDIR)\gximail5.obj +$(OBJDIR)\gximail6.obj &
	+$(OBJDIR)\gxinter.obj +$(OBJDIR)\gxlora.obj +$(OBJDIR)\gxmax3.obj +$(OBJDIR)\gxme2.obj &
	+$(OBJDIR)\gxopus.obj +$(OBJDIR)\gxpcb.obj +$(OBJDIR)\gxportal.obj +$(OBJDIR)\gxprobrd.obj &
	+$(OBJDIR)\gxqfront.obj +$(OBJDIR)\gxqecho.obj +$(OBJDIR)\gxquick.obj +$(OBJDIR)\gxra.obj &
	+$(OBJDIR)\gxraecho.obj +$(OBJDIR)\gxspace.obj +$(OBJDIR)\gxsquish.obj +$(OBJDIR)\gxsuper.obj &
	+$(OBJDIR)\gxsync.obj +$(OBJDIR)\gxtimed.obj +$(OBJDIR)\gxtmail.obj +$(OBJDIR)\gxts.obj &
	+$(OBJDIR)\gxwmail.obj +$(OBJDIR)\gxwtr.obj +$(OBJDIR)\gxxbbs.obj +$(OBJDIR)\gxxmail.obj

GMB3_OBJS = &
	$(OBJDIR)\gmoarea.obj $(OBJDIR)\gmohuds.obj $(OBJDIR)\gmoezyc1.obj $(OBJDIR)\gmoezyc2.obj &
	$(OBJDIR)\gmoezyc3.obj $(OBJDIR)\gmoezyc4.obj $(OBJDIR)\gmoezyc5.obj $(OBJDIR)\gmofido1.obj &
	$(OBJDIR)\gmofido2.obj $(OBJDIR)\gmofido3.obj $(OBJDIR)\gmofido4.obj $(OBJDIR)\gmofido5.obj &
	$(OBJDIR)\gmojamm1.obj $(OBJDIR)\gmojamm2.obj $(OBJDIR)\gmojamm3.obj $(OBJDIR)\gmojamm4.obj &
	$(OBJDIR)\gmojamm5.obj $(OBJDIR)\gmopcbd1.obj $(OBJDIR)\gmopcbd2.obj $(OBJDIR)\gmopcbd3.obj &
	$(OBJDIR)\gmopcbd4.obj $(OBJDIR)\gmopcbd5.obj $(OBJDIR)\gmosmb1.obj $(OBJDIR)\gmosmb2.obj &
	$(OBJDIR)\gmosqsh1.obj $(OBJDIR)\gmosqsh2.obj $(OBJDIR)\gmosqsh3.obj $(OBJDIR)\gmosqsh4.obj &
	$(OBJDIR)\gmosqsh5.obj $(OBJDIR)\gmowcat1.obj $(OBJDIR)\gmowcat2.obj $(OBJDIR)\gmowcat3.obj &
	$(OBJDIR)\gmowcat4.obj $(OBJDIR)\gmowcat5.obj $(OBJDIR)\gmoxbbs1.obj $(OBJDIR)\gmoxbbs2.obj &
	$(OBJDIR)\gmoxbbs3.obj $(OBJDIR)\gmoxbbs4.obj $(OBJDIR)\gmoxbbs5.obj

GMB3_LIBOBJS = &
	+$(OBJDIR)\gmoarea.obj +$(OBJDIR)\gmohuds.obj +$(OBJDIR)\gmoezyc1.obj +$(OBJDIR)\gmoezyc2.obj &
	+$(OBJDIR)\gmoezyc3.obj +$(OBJDIR)\gmoezyc4.obj +$(OBJDIR)\gmoezyc5.obj +$(OBJDIR)\gmofido1.obj &
	+$(OBJDIR)\gmofido2.obj +$(OBJDIR)\gmofido3.obj +$(OBJDIR)\gmofido4.obj +$(OBJDIR)\gmofido5.obj &
	+$(OBJDIR)\gmojamm1.obj +$(OBJDIR)\gmojamm2.obj +$(OBJDIR)\gmojamm3.obj +$(OBJDIR)\gmojamm4.obj &
	+$(OBJDIR)\gmojamm5.obj +$(OBJDIR)\gmopcbd1.obj +$(OBJDIR)\gmopcbd2.obj +$(OBJDIR)\gmopcbd3.obj &
	+$(OBJDIR)\gmopcbd4.obj +$(OBJDIR)\gmopcbd5.obj +$(OBJDIR)\gmosmb1.obj +$(OBJDIR)\gmosmb2.obj &
	+$(OBJDIR)\gmosqsh1.obj +$(OBJDIR)\gmosqsh2.obj +$(OBJDIR)\gmosqsh3.obj +$(OBJDIR)\gmosqsh4.obj &
	+$(OBJDIR)\gmosqsh5.obj +$(OBJDIR)\gmowcat1.obj +$(OBJDIR)\gmowcat2.obj +$(OBJDIR)\gmowcat3.obj &
	+$(OBJDIR)\gmowcat4.obj +$(OBJDIR)\gmowcat5.obj +$(OBJDIR)\gmoxbbs1.obj +$(OBJDIR)\gmoxbbs2.obj &
	+$(OBJDIR)\gmoxbbs3.obj +$(OBJDIR)\gmoxbbs4.obj +$(OBJDIR)\gmoxbbs5.obj

UULIB_OBJS = &
	$(OBJDIR)\fptools.obj $(OBJDIR)\uucheck.obj $(OBJDIR)\uuencode.obj $(OBJDIR)\uulib.obj &
	$(OBJDIR)\uunconc.obj $(OBJDIR)\uuscan.obj $(OBJDIR)\uustring.obj $(OBJDIR)\uuutil.obj

UULIB_LIBOBJS = &
	+$(OBJDIR)\fptools.obj +$(OBJDIR)\uucheck.obj +$(OBJDIR)\uuencode.obj +$(OBJDIR)\uulib.obj &
	+$(OBJDIR)\uunconc.obj +$(OBJDIR)\uuscan.obj +$(OBJDIR)\uustring.obj +$(OBJDIR)\uuutil.obj

SMBLIB_OBJS = &
	$(OBJDIR)\lzh.obj $(OBJDIR)\smblib.obj

SMBLIB_LIBOBJS = &
	+$(OBJDIR)\lzh.obj +$(OBJDIR)\smblib.obj

GLIBC_OBJS = &
	$(OBJDIR)\regex.obj $(OBJDIR)\dummy.obj

GLIBC_LIBOBJS = &
	+$(OBJDIR)\regex.obj +$(OBJDIR)\dummy.obj

GOLDED3_OBJS = &
	$(OBJDIR)\gcalst.obj $(OBJDIR)\gcarea.obj $(OBJDIR)\gccfgg.obj $(OBJDIR)\gccfgg0.obj &
	$(OBJDIR)\gccfgg1.obj $(OBJDIR)\gccfgg2.obj $(OBJDIR)\gccfgg3.obj $(OBJDIR)\gccfgg4.obj &
	$(OBJDIR)\gccfgg5.obj $(OBJDIR)\gccfgg6.obj $(OBJDIR)\gccfgg7.obj $(OBJDIR)\gccfgg8.obj &
	$(OBJDIR)\gckeys.obj $(OBJDIR)\gclang.obj $(OBJDIR)\gcmisc.obj $(OBJDIR)\gealst.obj &
	$(OBJDIR)\gearea.obj $(OBJDIR)\gecarb.obj $(OBJDIR)\gecmfd.obj $(OBJDIR)\gectnr.obj &
	$(OBJDIR)\gectrl.obj $(OBJDIR)\gedoit.obj $(OBJDIR)\gedoss.obj $(OBJDIR)\geedit.obj &
	$(OBJDIR)\geedit2.obj $(OBJDIR)\geedit3.obj $(OBJDIR)\gefile.obj $(OBJDIR)\gefind.obj &
	$(OBJDIR)\geglob.obj $(OBJDIR)\gehdre.obj $(OBJDIR)\geinit.obj $(OBJDIR)\geline.obj &
	$(OBJDIR)\gelmsg.obj $(OBJDIR)\gemain.obj $(OBJDIR)\gemenu.obj $(OBJDIR)\gemlst.obj &
	$(OBJDIR)\gemnus.obj $(OBJDIR)\gemrks.obj $(OBJDIR)\gemsgs.obj $(OBJDIR)\genode.obj &
	$(OBJDIR)\geplay.obj $(OBJDIR)\gepost.obj $(OBJDIR)\geqwks.obj $(OBJDIR)\gerand.obj &
	$(OBJDIR)\geread.obj $(OBJDIR)\geread2.obj $(OBJDIR)\gescan.obj $(OBJDIR)\gesrch.obj &
	$(OBJDIR)\gesoup.obj $(OBJDIR)\getpls.obj $(OBJDIR)\geusrbse.obj $(OBJDIR)\geutil.obj &
	$(OBJDIR)\geutil2.obj $(OBJDIR)\geview.obj $(OBJDIR)\gmarea.obj $(OBJDIR)\gehtml.obj &
	$(OBJDIR)\golded3.obj

GOLDNODE_OBJS = &
	$(OBJDIR)\goldnode.obj

RDDT_OBJS = &
	$(OBJDIR)\rddt.obj
!else
!ifdef DOS32
GALL_OBJS = &
	$(OBJDIR)\gcrc16tb.obj $(OBJDIR)\gcrc32tb.obj $(OBJDIR)\gcrchash.obj $(OBJDIR)\gcrckeyv.obj &
	$(OBJDIR)\gcrcm16.obj $(OBJDIR)\gcrcm32.obj $(OBJDIR)\gcrcs16.obj $(OBJDIR)\gcrcs32.obj &
	$(OBJDIR)\gdbgerr.obj $(OBJDIR)\gdbgexit.obj $(OBJDIR)\gdbgtrk.obj $(OBJDIR)\gdirposx.obj &
	$(OBJDIR)\geval.obj $(OBJDIR)\gevalhum.obj $(OBJDIR)\gevalrpn.obj $(OBJDIR)\gfile.obj &
	$(OBJDIR)\gfilport.obj $(OBJDIR)\gfilutl1.obj $(OBJDIR)\gfilutl2.obj $(OBJDIR)\gftnaddr.obj &
	$(OBJDIR)\gftnnl.obj $(OBJDIR)\gftnnlfd.obj $(OBJDIR)\gftnnlfu.obj $(OBJDIR)\gftnnlge.obj &
	$(OBJDIR)\gftnnlv7.obj $(OBJDIR)\glog.obj $(OBJDIR)\gmemdbg.obj $(OBJDIR)\gmemutil.obj &
	$(OBJDIR)\gmsgattr.obj $(OBJDIR)\ghdrmime.obj $(OBJDIR)\gprnutil.obj $(OBJDIR)\gsnd.obj &
	$(OBJDIR)\gsndwrap.obj $(OBJDIR)\gstrctyp.obj $(OBJDIR)\gstrmail.obj $(OBJDIR)\gstrname.obj &
	$(OBJDIR)\gstrutil.obj $(OBJDIR)\gtimjuld.obj $(OBJDIR)\gtimutil.obj $(OBJDIR)\gbmh.obj &
	$(OBJDIR)\gfuzzy.obj $(OBJDIR)\gregex.obj $(OBJDIR)\gwildmat.obj $(OBJDIR)\gsearch.obj &
	$(OBJDIR)\gtxtpara.obj $(OBJDIR)\gusrbase.obj $(OBJDIR)\gusrezyc.obj $(OBJDIR)\gusrgold.obj &
	$(OBJDIR)\gusrhuds.obj $(OBJDIR)\gusrmax.obj $(OBJDIR)\gusrpcb.obj $(OBJDIR)\gusrra2.obj &
	$(OBJDIR)\gusrxbbs.obj $(OBJDIR)\gutlclip.obj $(OBJDIR)\gutlcode.obj $(OBJDIR)\gutlgrp.obj &
	$(OBJDIR)\gutlmisc.obj $(OBJDIR)\gutlmtsk.obj $(OBJDIR)\gutltag.obj $(OBJDIR)\gutlvers.obj &
	$(OBJDIR)\gcharset.obj $(OBJDIR)\gutf8.obj $(OBJDIR)\grecode.obj $(OBJDIR)\giniprsr.obj &
	$(OBJDIR)\gutldos.obj $(OBJDIR)\gespell.obj

GALL_LIBOBJS = &
	+$(OBJDIR)\gcrc16tb.obj +$(OBJDIR)\gcrc32tb.obj +$(OBJDIR)\gcrchash.obj +$(OBJDIR)\gcrckeyv.obj &
	+$(OBJDIR)\gcrcm16.obj +$(OBJDIR)\gcrcm32.obj +$(OBJDIR)\gcrcs16.obj +$(OBJDIR)\gcrcs32.obj &
	+$(OBJDIR)\gdbgerr.obj +$(OBJDIR)\gdbgexit.obj +$(OBJDIR)\gdbgtrk.obj +$(OBJDIR)\gdirposx.obj &
	+$(OBJDIR)\geval.obj +$(OBJDIR)\gevalhum.obj +$(OBJDIR)\gevalrpn.obj +$(OBJDIR)\gfile.obj &
	+$(OBJDIR)\gfilport.obj +$(OBJDIR)\gfilutl1.obj +$(OBJDIR)\gfilutl2.obj +$(OBJDIR)\gftnaddr.obj &
	+$(OBJDIR)\gftnnl.obj +$(OBJDIR)\gftnnlfd.obj +$(OBJDIR)\gftnnlfu.obj +$(OBJDIR)\gftnnlge.obj &
	+$(OBJDIR)\gftnnlv7.obj +$(OBJDIR)\glog.obj +$(OBJDIR)\gmemdbg.obj +$(OBJDIR)\gmemutil.obj &
	+$(OBJDIR)\gmsgattr.obj +$(OBJDIR)\ghdrmime.obj +$(OBJDIR)\gprnutil.obj +$(OBJDIR)\gsnd.obj &
	+$(OBJDIR)\gsndwrap.obj +$(OBJDIR)\gstrctyp.obj +$(OBJDIR)\gstrmail.obj +$(OBJDIR)\gstrname.obj &
	+$(OBJDIR)\gstrutil.obj +$(OBJDIR)\gtimjuld.obj +$(OBJDIR)\gtimutil.obj +$(OBJDIR)\gbmh.obj &
	+$(OBJDIR)\gfuzzy.obj +$(OBJDIR)\gregex.obj +$(OBJDIR)\gwildmat.obj +$(OBJDIR)\gsearch.obj &
	+$(OBJDIR)\gtxtpara.obj +$(OBJDIR)\gusrbase.obj +$(OBJDIR)\gusrezyc.obj +$(OBJDIR)\gusrgold.obj &
	+$(OBJDIR)\gusrhuds.obj +$(OBJDIR)\gusrmax.obj +$(OBJDIR)\gusrpcb.obj +$(OBJDIR)\gusrra2.obj &
	+$(OBJDIR)\gusrxbbs.obj +$(OBJDIR)\gutlclip.obj +$(OBJDIR)\gutlcode.obj +$(OBJDIR)\gutlgrp.obj &
	+$(OBJDIR)\gutlmisc.obj +$(OBJDIR)\gutlmtsk.obj +$(OBJDIR)\gutltag.obj +$(OBJDIR)\gutlvers.obj &
	+$(OBJDIR)\gcharset.obj +$(OBJDIR)\gutf8.obj +$(OBJDIR)\grecode.obj +$(OBJDIR)\giniprsr.obj &
	+$(OBJDIR)\gutldos.obj +$(OBJDIR)\gespell.obj

GCUI_OBJS = &
	$(OBJDIR)\gkbdbase.obj $(OBJDIR)\gkbdgetm.obj $(OBJDIR)\gkbdwait.obj $(OBJDIR)\gsrchmgr.obj &
	$(OBJDIR)\gmoubase.obj $(OBJDIR)\gvidbase.obj $(OBJDIR)\gvidinit.obj $(OBJDIR)\gwinbase.obj &
	$(OBJDIR)\gwindow.obj $(OBJDIR)\gwinhlp1.obj $(OBJDIR)\gwinhlp2.obj $(OBJDIR)\gwininit.obj &
	$(OBJDIR)\gwinline.obj $(OBJDIR)\gwinmenu.obj $(OBJDIR)\gwinmnub.obj $(OBJDIR)\gwinpckf.obj &
	$(OBJDIR)\gwinpcks.obj $(OBJDIR)\gwinpick.obj $(OBJDIR)\gwinput2.obj

GCUI_LIBOBJS = &
	+$(OBJDIR)\gkbdbase.obj +$(OBJDIR)\gkbdgetm.obj +$(OBJDIR)\gkbdwait.obj +$(OBJDIR)\gsrchmgr.obj &
	+$(OBJDIR)\gmoubase.obj +$(OBJDIR)\gvidbase.obj +$(OBJDIR)\gvidinit.obj +$(OBJDIR)\gwinbase.obj &
	+$(OBJDIR)\gwindow.obj +$(OBJDIR)\gwinhlp1.obj +$(OBJDIR)\gwinhlp2.obj +$(OBJDIR)\gwininit.obj &
	+$(OBJDIR)\gwinline.obj +$(OBJDIR)\gwinmenu.obj +$(OBJDIR)\gwinmnub.obj +$(OBJDIR)\gwinpckf.obj &
	+$(OBJDIR)\gwinpcks.obj +$(OBJDIR)\gwinpick.obj +$(OBJDIR)\gwinput2.obj

GCFG_OBJS = &
	$(OBJDIR)\gedacfg.obj $(OBJDIR)\gxareas.obj $(OBJDIR)\gxcrash.obj $(OBJDIR)\gxdb.obj &
	$(OBJDIR)\gxdutch.obj $(OBJDIR)\gxezy102.obj $(OBJDIR)\gxezy110.obj $(OBJDIR)\gxfd.obj &
	$(OBJDIR)\gxfecho4.obj $(OBJDIR)\gxfecho5.obj $(OBJDIR)\gxfecho6.obj $(OBJDIR)\gxfidpcb.obj &
	$(OBJDIR)\gxfm092.obj $(OBJDIR)\gxfm100.obj $(OBJDIR)\gxfm116.obj $(OBJDIR)\gxgecho.obj &
	$(OBJDIR)\gxhpt.obj $(OBJDIR)\gximail4.obj $(OBJDIR)\gximail5.obj $(OBJDIR)\gximail6.obj &
	$(OBJDIR)\gxinter.obj $(OBJDIR)\gxlora.obj $(OBJDIR)\gxmax3.obj $(OBJDIR)\gxme2.obj &
	$(OBJDIR)\gxopus.obj $(OBJDIR)\gxpcb.obj $(OBJDIR)\gxportal.obj $(OBJDIR)\gxprobrd.obj &
	$(OBJDIR)\gxqfront.obj $(OBJDIR)\gxqecho.obj $(OBJDIR)\gxquick.obj $(OBJDIR)\gxra.obj &
	$(OBJDIR)\gxraecho.obj $(OBJDIR)\gxspace.obj $(OBJDIR)\gxsquish.obj $(OBJDIR)\gxsuper.obj &
	$(OBJDIR)\gxsync.obj $(OBJDIR)\gxtimed.obj $(OBJDIR)\gxtmail.obj $(OBJDIR)\gxts.obj &
	$(OBJDIR)\gxwmail.obj $(OBJDIR)\gxwtr.obj $(OBJDIR)\gxxbbs.obj $(OBJDIR)\gxxmail.obj

GCFG_LIBOBJS = &
	+$(OBJDIR)\gedacfg.obj +$(OBJDIR)\gxareas.obj +$(OBJDIR)\gxcrash.obj +$(OBJDIR)\gxdb.obj &
	+$(OBJDIR)\gxdutch.obj +$(OBJDIR)\gxezy102.obj +$(OBJDIR)\gxezy110.obj +$(OBJDIR)\gxfd.obj &
	+$(OBJDIR)\gxfecho4.obj +$(OBJDIR)\gxfecho5.obj +$(OBJDIR)\gxfecho6.obj +$(OBJDIR)\gxfidpcb.obj &
	+$(OBJDIR)\gxfm092.obj +$(OBJDIR)\gxfm100.obj +$(OBJDIR)\gxfm116.obj +$(OBJDIR)\gxgecho.obj &
	+$(OBJDIR)\gxhpt.obj +$(OBJDIR)\gximail4.obj +$(OBJDIR)\gximail5.obj +$(OBJDIR)\gximail6.obj &
	+$(OBJDIR)\gxinter.obj +$(OBJDIR)\gxlora.obj +$(OBJDIR)\gxmax3.obj +$(OBJDIR)\gxme2.obj &
	+$(OBJDIR)\gxopus.obj +$(OBJDIR)\gxpcb.obj +$(OBJDIR)\gxportal.obj +$(OBJDIR)\gxprobrd.obj &
	+$(OBJDIR)\gxqfront.obj +$(OBJDIR)\gxqecho.obj +$(OBJDIR)\gxquick.obj +$(OBJDIR)\gxra.obj &
	+$(OBJDIR)\gxraecho.obj +$(OBJDIR)\gxspace.obj +$(OBJDIR)\gxsquish.obj +$(OBJDIR)\gxsuper.obj &
	+$(OBJDIR)\gxsync.obj +$(OBJDIR)\gxtimed.obj +$(OBJDIR)\gxtmail.obj +$(OBJDIR)\gxts.obj &
	+$(OBJDIR)\gxwmail.obj +$(OBJDIR)\gxwtr.obj +$(OBJDIR)\gxxbbs.obj +$(OBJDIR)\gxxmail.obj

GMB3_OBJS = &
	$(OBJDIR)\gmoarea.obj $(OBJDIR)\gmohuds.obj $(OBJDIR)\gmoezyc1.obj $(OBJDIR)\gmoezyc2.obj &
	$(OBJDIR)\gmoezyc3.obj $(OBJDIR)\gmoezyc4.obj $(OBJDIR)\gmoezyc5.obj $(OBJDIR)\gmofido1.obj &
	$(OBJDIR)\gmofido2.obj $(OBJDIR)\gmofido3.obj $(OBJDIR)\gmofido4.obj $(OBJDIR)\gmofido5.obj &
	$(OBJDIR)\gmojamm1.obj $(OBJDIR)\gmojamm2.obj $(OBJDIR)\gmojamm3.obj $(OBJDIR)\gmojamm4.obj &
	$(OBJDIR)\gmojamm5.obj $(OBJDIR)\gmopcbd1.obj $(OBJDIR)\gmopcbd2.obj $(OBJDIR)\gmopcbd3.obj &
	$(OBJDIR)\gmopcbd4.obj $(OBJDIR)\gmopcbd5.obj $(OBJDIR)\gmosmb1.obj $(OBJDIR)\gmosmb2.obj &
	$(OBJDIR)\gmosqsh1.obj $(OBJDIR)\gmosqsh2.obj $(OBJDIR)\gmosqsh3.obj $(OBJDIR)\gmosqsh4.obj &
	$(OBJDIR)\gmosqsh5.obj $(OBJDIR)\gmowcat1.obj $(OBJDIR)\gmowcat2.obj $(OBJDIR)\gmowcat3.obj &
	$(OBJDIR)\gmowcat4.obj $(OBJDIR)\gmowcat5.obj $(OBJDIR)\gmoxbbs1.obj $(OBJDIR)\gmoxbbs2.obj &
	$(OBJDIR)\gmoxbbs3.obj $(OBJDIR)\gmoxbbs4.obj $(OBJDIR)\gmoxbbs5.obj

GMB3_LIBOBJS = &
	+$(OBJDIR)\gmoarea.obj +$(OBJDIR)\gmohuds.obj +$(OBJDIR)\gmoezyc1.obj +$(OBJDIR)\gmoezyc2.obj &
	+$(OBJDIR)\gmoezyc3.obj +$(OBJDIR)\gmoezyc4.obj +$(OBJDIR)\gmoezyc5.obj +$(OBJDIR)\gmofido1.obj &
	+$(OBJDIR)\gmofido2.obj +$(OBJDIR)\gmofido3.obj +$(OBJDIR)\gmofido4.obj +$(OBJDIR)\gmofido5.obj &
	+$(OBJDIR)\gmojamm1.obj +$(OBJDIR)\gmojamm2.obj +$(OBJDIR)\gmojamm3.obj +$(OBJDIR)\gmojamm4.obj &
	+$(OBJDIR)\gmojamm5.obj +$(OBJDIR)\gmopcbd1.obj +$(OBJDIR)\gmopcbd2.obj +$(OBJDIR)\gmopcbd3.obj &
	+$(OBJDIR)\gmopcbd4.obj +$(OBJDIR)\gmopcbd5.obj +$(OBJDIR)\gmosmb1.obj +$(OBJDIR)\gmosmb2.obj &
	+$(OBJDIR)\gmosqsh1.obj +$(OBJDIR)\gmosqsh2.obj +$(OBJDIR)\gmosqsh3.obj +$(OBJDIR)\gmosqsh4.obj &
	+$(OBJDIR)\gmosqsh5.obj +$(OBJDIR)\gmowcat1.obj +$(OBJDIR)\gmowcat2.obj +$(OBJDIR)\gmowcat3.obj &
	+$(OBJDIR)\gmowcat4.obj +$(OBJDIR)\gmowcat5.obj +$(OBJDIR)\gmoxbbs1.obj +$(OBJDIR)\gmoxbbs2.obj &
	+$(OBJDIR)\gmoxbbs3.obj +$(OBJDIR)\gmoxbbs4.obj +$(OBJDIR)\gmoxbbs5.obj

UULIB_OBJS = &
	$(OBJDIR)\fptools.obj $(OBJDIR)\uucheck.obj $(OBJDIR)\uuencode.obj $(OBJDIR)\uulib.obj &
	$(OBJDIR)\uunconc.obj $(OBJDIR)\uuscan.obj $(OBJDIR)\uustring.obj $(OBJDIR)\uuutil.obj

UULIB_LIBOBJS = &
	+$(OBJDIR)\fptools.obj +$(OBJDIR)\uucheck.obj +$(OBJDIR)\uuencode.obj +$(OBJDIR)\uulib.obj &
	+$(OBJDIR)\uunconc.obj +$(OBJDIR)\uuscan.obj +$(OBJDIR)\uustring.obj +$(OBJDIR)\uuutil.obj

SMBLIB_OBJS = &
	$(OBJDIR)\lzh.obj $(OBJDIR)\smblib.obj

SMBLIB_LIBOBJS = &
	+$(OBJDIR)\lzh.obj +$(OBJDIR)\smblib.obj

GLIBC_OBJS = &
	$(OBJDIR)\regex.obj $(OBJDIR)\dummy.obj

GLIBC_LIBOBJS = &
	+$(OBJDIR)\regex.obj +$(OBJDIR)\dummy.obj

GOLDED3_OBJS = &
	$(OBJDIR)\gcalst.obj $(OBJDIR)\gcarea.obj $(OBJDIR)\gccfgg.obj $(OBJDIR)\gccfgg0.obj &
	$(OBJDIR)\gccfgg1.obj $(OBJDIR)\gccfgg2.obj $(OBJDIR)\gccfgg3.obj $(OBJDIR)\gccfgg4.obj &
	$(OBJDIR)\gccfgg5.obj $(OBJDIR)\gccfgg6.obj $(OBJDIR)\gccfgg7.obj $(OBJDIR)\gccfgg8.obj &
	$(OBJDIR)\gckeys.obj $(OBJDIR)\gclang.obj $(OBJDIR)\gcmisc.obj $(OBJDIR)\gealst.obj &
	$(OBJDIR)\gearea.obj $(OBJDIR)\gecarb.obj $(OBJDIR)\gecmfd.obj $(OBJDIR)\gectnr.obj &
	$(OBJDIR)\gectrl.obj $(OBJDIR)\gedoit.obj $(OBJDIR)\gedoss.obj $(OBJDIR)\geedit.obj &
	$(OBJDIR)\geedit2.obj $(OBJDIR)\geedit3.obj $(OBJDIR)\gefile.obj $(OBJDIR)\gefind.obj &
	$(OBJDIR)\geglob.obj $(OBJDIR)\gehdre.obj $(OBJDIR)\geinit.obj $(OBJDIR)\geline.obj &
	$(OBJDIR)\gelmsg.obj $(OBJDIR)\gemain.obj $(OBJDIR)\gemenu.obj $(OBJDIR)\gemlst.obj &
	$(OBJDIR)\gemnus.obj $(OBJDIR)\gemrks.obj $(OBJDIR)\gemsgs.obj $(OBJDIR)\genode.obj &
	$(OBJDIR)\geplay.obj $(OBJDIR)\gepost.obj $(OBJDIR)\geqwks.obj $(OBJDIR)\gerand.obj &
	$(OBJDIR)\geread.obj $(OBJDIR)\geread2.obj $(OBJDIR)\gescan.obj $(OBJDIR)\gesrch.obj &
	$(OBJDIR)\gesoup.obj $(OBJDIR)\getpls.obj $(OBJDIR)\geusrbse.obj $(OBJDIR)\geutil.obj &
	$(OBJDIR)\geutil2.obj $(OBJDIR)\geview.obj $(OBJDIR)\gmarea.obj $(OBJDIR)\gehtml.obj &
	$(OBJDIR)\golded3.obj

GOLDNODE_OBJS = &
	$(OBJDIR)\goldnode.obj

RDDT_OBJS = &
	$(OBJDIR)\rddt.obj
!else
!ifdef LINUX
GALL_OBJS = &
	$(OBJDIR)\gcrc16tb.obj $(OBJDIR)\gcrc32tb.obj $(OBJDIR)\gcrchash.obj $(OBJDIR)\gcrckeyv.obj &
	$(OBJDIR)\gcrcm16.obj $(OBJDIR)\gcrcm32.obj $(OBJDIR)\gcrcs16.obj $(OBJDIR)\gcrcs32.obj &
	$(OBJDIR)\gdbgerr.obj $(OBJDIR)\gdbgexit.obj $(OBJDIR)\gdbgtrk.obj $(OBJDIR)\gdirposx.obj &
	$(OBJDIR)\geval.obj $(OBJDIR)\gevalhum.obj $(OBJDIR)\gevalrpn.obj $(OBJDIR)\gfile.obj &
	$(OBJDIR)\gfilport.obj $(OBJDIR)\gfilutl1.obj $(OBJDIR)\gfilutl2.obj $(OBJDIR)\gftnaddr.obj &
	$(OBJDIR)\gftnnl.obj $(OBJDIR)\gftnnlfd.obj $(OBJDIR)\gftnnlfu.obj $(OBJDIR)\gftnnlge.obj &
	$(OBJDIR)\gftnnlv7.obj $(OBJDIR)\gkbdunix.obj $(OBJDIR)\glog.obj $(OBJDIR)\gmemdbg.obj &
	$(OBJDIR)\gmemutil.obj $(OBJDIR)\gmsgattr.obj $(OBJDIR)\ghdrmime.obj $(OBJDIR)\gprnutil.obj &
	$(OBJDIR)\gsigunix.obj $(OBJDIR)\gsnd.obj $(OBJDIR)\gsndwrap.obj $(OBJDIR)\gstrctyp.obj &
	$(OBJDIR)\gstrmail.obj $(OBJDIR)\gstrname.obj $(OBJDIR)\gstrutil.obj $(OBJDIR)\gtimjuld.obj &
	$(OBJDIR)\gtimutil.obj $(OBJDIR)\gbmh.obj $(OBJDIR)\gfuzzy.obj $(OBJDIR)\gregex.obj &
	$(OBJDIR)\gwildmat.obj $(OBJDIR)\gsearch.obj $(OBJDIR)\gtxtpara.obj $(OBJDIR)\gusrbase.obj &
	$(OBJDIR)\gusrezyc.obj $(OBJDIR)\gusrgold.obj $(OBJDIR)\gusrhuds.obj $(OBJDIR)\gusrmax.obj &
	$(OBJDIR)\gusrpcb.obj $(OBJDIR)\gusrra2.obj $(OBJDIR)\gusrxbbs.obj $(OBJDIR)\gutlclip.obj &
	$(OBJDIR)\gutlcode.obj $(OBJDIR)\gutlgrp.obj $(OBJDIR)\gutlmisc.obj $(OBJDIR)\gutlmtsk.obj &
	$(OBJDIR)\gutltag.obj $(OBJDIR)\gutlvers.obj $(OBJDIR)\gcharset.obj $(OBJDIR)\gutf8.obj &
	$(OBJDIR)\grecode.obj $(OBJDIR)\giniprsr.obj $(OBJDIR)\gutlunix.obj $(OBJDIR)\gespell.obj

GALL_LIBOBJS = &
	+$(OBJDIR)\gcrc16tb.obj +$(OBJDIR)\gcrc32tb.obj +$(OBJDIR)\gcrchash.obj +$(OBJDIR)\gcrckeyv.obj &
	+$(OBJDIR)\gcrcm16.obj +$(OBJDIR)\gcrcm32.obj +$(OBJDIR)\gcrcs16.obj +$(OBJDIR)\gcrcs32.obj &
	+$(OBJDIR)\gdbgerr.obj +$(OBJDIR)\gdbgexit.obj +$(OBJDIR)\gdbgtrk.obj +$(OBJDIR)\gdirposx.obj &
	+$(OBJDIR)\geval.obj +$(OBJDIR)\gevalhum.obj +$(OBJDIR)\gevalrpn.obj +$(OBJDIR)\gfile.obj &
	+$(OBJDIR)\gfilport.obj +$(OBJDIR)\gfilutl1.obj +$(OBJDIR)\gfilutl2.obj +$(OBJDIR)\gftnaddr.obj &
	+$(OBJDIR)\gftnnl.obj +$(OBJDIR)\gftnnlfd.obj +$(OBJDIR)\gftnnlfu.obj +$(OBJDIR)\gftnnlge.obj &
	+$(OBJDIR)\gftnnlv7.obj +$(OBJDIR)\gkbdunix.obj +$(OBJDIR)\glog.obj +$(OBJDIR)\gmemdbg.obj &
	+$(OBJDIR)\gmemutil.obj +$(OBJDIR)\gmsgattr.obj +$(OBJDIR)\ghdrmime.obj +$(OBJDIR)\gprnutil.obj &
	+$(OBJDIR)\gsigunix.obj +$(OBJDIR)\gsnd.obj +$(OBJDIR)\gsndwrap.obj +$(OBJDIR)\gstrctyp.obj &
	+$(OBJDIR)\gstrmail.obj +$(OBJDIR)\gstrname.obj +$(OBJDIR)\gstrutil.obj +$(OBJDIR)\gtimjuld.obj &
	+$(OBJDIR)\gtimutil.obj +$(OBJDIR)\gbmh.obj +$(OBJDIR)\gfuzzy.obj +$(OBJDIR)\gregex.obj &
	+$(OBJDIR)\gwildmat.obj +$(OBJDIR)\gsearch.obj +$(OBJDIR)\gtxtpara.obj +$(OBJDIR)\gusrbase.obj &
	+$(OBJDIR)\gusrezyc.obj +$(OBJDIR)\gusrgold.obj +$(OBJDIR)\gusrhuds.obj +$(OBJDIR)\gusrmax.obj &
	+$(OBJDIR)\gusrpcb.obj +$(OBJDIR)\gusrra2.obj +$(OBJDIR)\gusrxbbs.obj +$(OBJDIR)\gutlclip.obj &
	+$(OBJDIR)\gutlcode.obj +$(OBJDIR)\gutlgrp.obj +$(OBJDIR)\gutlmisc.obj +$(OBJDIR)\gutlmtsk.obj &
	+$(OBJDIR)\gutltag.obj +$(OBJDIR)\gutlvers.obj +$(OBJDIR)\gcharset.obj +$(OBJDIR)\gutf8.obj &
	+$(OBJDIR)\grecode.obj +$(OBJDIR)\giniprsr.obj +$(OBJDIR)\gutlunix.obj +$(OBJDIR)\gespell.obj

GCUI_OBJS = &
	$(OBJDIR)\gkbdbase.obj $(OBJDIR)\gkbdgetm.obj $(OBJDIR)\gkbdwait.obj $(OBJDIR)\gsrchmgr.obj &
	$(OBJDIR)\gmoubase.obj $(OBJDIR)\gvidbase.obj $(OBJDIR)\gvidinit.obj $(OBJDIR)\gwinbase.obj &
	$(OBJDIR)\gwindow.obj $(OBJDIR)\gwinhlp1.obj $(OBJDIR)\gwinhlp2.obj $(OBJDIR)\gwininit.obj &
	$(OBJDIR)\gwinline.obj $(OBJDIR)\gwinmenu.obj $(OBJDIR)\gwinmnub.obj $(OBJDIR)\gwinpckf.obj &
	$(OBJDIR)\gwinpcks.obj $(OBJDIR)\gwinpick.obj $(OBJDIR)\gwinput2.obj

GCUI_LIBOBJS = &
	+$(OBJDIR)\gkbdbase.obj +$(OBJDIR)\gkbdgetm.obj +$(OBJDIR)\gkbdwait.obj +$(OBJDIR)\gsrchmgr.obj &
	+$(OBJDIR)\gmoubase.obj +$(OBJDIR)\gvidbase.obj +$(OBJDIR)\gvidinit.obj +$(OBJDIR)\gwinbase.obj &
	+$(OBJDIR)\gwindow.obj +$(OBJDIR)\gwinhlp1.obj +$(OBJDIR)\gwinhlp2.obj +$(OBJDIR)\gwininit.obj &
	+$(OBJDIR)\gwinline.obj +$(OBJDIR)\gwinmenu.obj +$(OBJDIR)\gwinmnub.obj +$(OBJDIR)\gwinpckf.obj &
	+$(OBJDIR)\gwinpcks.obj +$(OBJDIR)\gwinpick.obj +$(OBJDIR)\gwinput2.obj

GCFG_OBJS = &
	$(OBJDIR)\gedacfg.obj $(OBJDIR)\gxareas.obj $(OBJDIR)\gxcrash.obj $(OBJDIR)\gxdb.obj &
	$(OBJDIR)\gxdutch.obj $(OBJDIR)\gxezy102.obj $(OBJDIR)\gxezy110.obj $(OBJDIR)\gxfd.obj &
	$(OBJDIR)\gxfecho4.obj $(OBJDIR)\gxfecho5.obj $(OBJDIR)\gxfecho6.obj $(OBJDIR)\gxfidpcb.obj &
	$(OBJDIR)\gxfm092.obj $(OBJDIR)\gxfm100.obj $(OBJDIR)\gxfm116.obj $(OBJDIR)\gxgecho.obj &
	$(OBJDIR)\gxhpt.obj $(OBJDIR)\gximail4.obj $(OBJDIR)\gximail5.obj $(OBJDIR)\gximail6.obj &
	$(OBJDIR)\gxinter.obj $(OBJDIR)\gxlora.obj $(OBJDIR)\gxmax3.obj $(OBJDIR)\gxme2.obj &
	$(OBJDIR)\gxopus.obj $(OBJDIR)\gxpcb.obj $(OBJDIR)\gxportal.obj $(OBJDIR)\gxprobrd.obj &
	$(OBJDIR)\gxqfront.obj $(OBJDIR)\gxqecho.obj $(OBJDIR)\gxquick.obj $(OBJDIR)\gxra.obj &
	$(OBJDIR)\gxraecho.obj $(OBJDIR)\gxspace.obj $(OBJDIR)\gxsquish.obj $(OBJDIR)\gxsuper.obj &
	$(OBJDIR)\gxsync.obj $(OBJDIR)\gxtimed.obj $(OBJDIR)\gxtmail.obj $(OBJDIR)\gxts.obj &
	$(OBJDIR)\gxwmail.obj $(OBJDIR)\gxwtr.obj $(OBJDIR)\gxxbbs.obj $(OBJDIR)\gxxmail.obj

GCFG_LIBOBJS = &
	+$(OBJDIR)\gedacfg.obj +$(OBJDIR)\gxareas.obj +$(OBJDIR)\gxcrash.obj +$(OBJDIR)\gxdb.obj &
	+$(OBJDIR)\gxdutch.obj +$(OBJDIR)\gxezy102.obj +$(OBJDIR)\gxezy110.obj +$(OBJDIR)\gxfd.obj &
	+$(OBJDIR)\gxfecho4.obj +$(OBJDIR)\gxfecho5.obj +$(OBJDIR)\gxfecho6.obj +$(OBJDIR)\gxfidpcb.obj &
	+$(OBJDIR)\gxfm092.obj +$(OBJDIR)\gxfm100.obj +$(OBJDIR)\gxfm116.obj +$(OBJDIR)\gxgecho.obj &
	+$(OBJDIR)\gxhpt.obj +$(OBJDIR)\gximail4.obj +$(OBJDIR)\gximail5.obj +$(OBJDIR)\gximail6.obj &
	+$(OBJDIR)\gxinter.obj +$(OBJDIR)\gxlora.obj +$(OBJDIR)\gxmax3.obj +$(OBJDIR)\gxme2.obj &
	+$(OBJDIR)\gxopus.obj +$(OBJDIR)\gxpcb.obj +$(OBJDIR)\gxportal.obj +$(OBJDIR)\gxprobrd.obj &
	+$(OBJDIR)\gxqfront.obj +$(OBJDIR)\gxqecho.obj +$(OBJDIR)\gxquick.obj +$(OBJDIR)\gxra.obj &
	+$(OBJDIR)\gxraecho.obj +$(OBJDIR)\gxspace.obj +$(OBJDIR)\gxsquish.obj +$(OBJDIR)\gxsuper.obj &
	+$(OBJDIR)\gxsync.obj +$(OBJDIR)\gxtimed.obj +$(OBJDIR)\gxtmail.obj +$(OBJDIR)\gxts.obj &
	+$(OBJDIR)\gxwmail.obj +$(OBJDIR)\gxwtr.obj +$(OBJDIR)\gxxbbs.obj +$(OBJDIR)\gxxmail.obj

GMB3_OBJS = &
	$(OBJDIR)\gmoarea.obj $(OBJDIR)\gmohuds.obj $(OBJDIR)\gmoezyc1.obj $(OBJDIR)\gmoezyc2.obj &
	$(OBJDIR)\gmoezyc3.obj $(OBJDIR)\gmoezyc4.obj $(OBJDIR)\gmoezyc5.obj $(OBJDIR)\gmofido1.obj &
	$(OBJDIR)\gmofido2.obj $(OBJDIR)\gmofido3.obj $(OBJDIR)\gmofido4.obj $(OBJDIR)\gmofido5.obj &
	$(OBJDIR)\gmojamm1.obj $(OBJDIR)\gmojamm2.obj $(OBJDIR)\gmojamm3.obj $(OBJDIR)\gmojamm4.obj &
	$(OBJDIR)\gmojamm5.obj $(OBJDIR)\gmopcbd1.obj $(OBJDIR)\gmopcbd2.obj $(OBJDIR)\gmopcbd3.obj &
	$(OBJDIR)\gmopcbd4.obj $(OBJDIR)\gmopcbd5.obj $(OBJDIR)\gmosmb1.obj $(OBJDIR)\gmosmb2.obj &
	$(OBJDIR)\gmosqsh1.obj $(OBJDIR)\gmosqsh2.obj $(OBJDIR)\gmosqsh3.obj $(OBJDIR)\gmosqsh4.obj &
	$(OBJDIR)\gmosqsh5.obj $(OBJDIR)\gmowcat1.obj $(OBJDIR)\gmowcat2.obj $(OBJDIR)\gmowcat3.obj &
	$(OBJDIR)\gmowcat4.obj $(OBJDIR)\gmowcat5.obj $(OBJDIR)\gmoxbbs1.obj $(OBJDIR)\gmoxbbs2.obj &
	$(OBJDIR)\gmoxbbs3.obj $(OBJDIR)\gmoxbbs4.obj $(OBJDIR)\gmoxbbs5.obj

GMB3_LIBOBJS = &
	+$(OBJDIR)\gmoarea.obj +$(OBJDIR)\gmohuds.obj +$(OBJDIR)\gmoezyc1.obj +$(OBJDIR)\gmoezyc2.obj &
	+$(OBJDIR)\gmoezyc3.obj +$(OBJDIR)\gmoezyc4.obj +$(OBJDIR)\gmoezyc5.obj +$(OBJDIR)\gmofido1.obj &
	+$(OBJDIR)\gmofido2.obj +$(OBJDIR)\gmofido3.obj +$(OBJDIR)\gmofido4.obj +$(OBJDIR)\gmofido5.obj &
	+$(OBJDIR)\gmojamm1.obj +$(OBJDIR)\gmojamm2.obj +$(OBJDIR)\gmojamm3.obj +$(OBJDIR)\gmojamm4.obj &
	+$(OBJDIR)\gmojamm5.obj +$(OBJDIR)\gmopcbd1.obj +$(OBJDIR)\gmopcbd2.obj +$(OBJDIR)\gmopcbd3.obj &
	+$(OBJDIR)\gmopcbd4.obj +$(OBJDIR)\gmopcbd5.obj +$(OBJDIR)\gmosmb1.obj +$(OBJDIR)\gmosmb2.obj &
	+$(OBJDIR)\gmosqsh1.obj +$(OBJDIR)\gmosqsh2.obj +$(OBJDIR)\gmosqsh3.obj +$(OBJDIR)\gmosqsh4.obj &
	+$(OBJDIR)\gmosqsh5.obj +$(OBJDIR)\gmowcat1.obj +$(OBJDIR)\gmowcat2.obj +$(OBJDIR)\gmowcat3.obj &
	+$(OBJDIR)\gmowcat4.obj +$(OBJDIR)\gmowcat5.obj +$(OBJDIR)\gmoxbbs1.obj +$(OBJDIR)\gmoxbbs2.obj &
	+$(OBJDIR)\gmoxbbs3.obj +$(OBJDIR)\gmoxbbs4.obj +$(OBJDIR)\gmoxbbs5.obj

UULIB_OBJS = &
	$(OBJDIR)\fptools.obj $(OBJDIR)\uucheck.obj $(OBJDIR)\uuencode.obj $(OBJDIR)\uulib.obj &
	$(OBJDIR)\uunconc.obj $(OBJDIR)\uuscan.obj $(OBJDIR)\uustring.obj $(OBJDIR)\uuutil.obj

UULIB_LIBOBJS = &
	+$(OBJDIR)\fptools.obj +$(OBJDIR)\uucheck.obj +$(OBJDIR)\uuencode.obj +$(OBJDIR)\uulib.obj &
	+$(OBJDIR)\uunconc.obj +$(OBJDIR)\uuscan.obj +$(OBJDIR)\uustring.obj +$(OBJDIR)\uuutil.obj

SMBLIB_OBJS = &
	$(OBJDIR)\lzh.obj $(OBJDIR)\smblib.obj

SMBLIB_LIBOBJS = &
	+$(OBJDIR)\lzh.obj +$(OBJDIR)\smblib.obj

GLIBC_OBJS = &
	$(OBJDIR)\regex.obj $(OBJDIR)\dummy.obj

GLIBC_LIBOBJS = &
	+$(OBJDIR)\regex.obj +$(OBJDIR)\dummy.obj

GOLDED3_OBJS = &
	$(OBJDIR)\gcalst.obj $(OBJDIR)\gcarea.obj $(OBJDIR)\gccfgg.obj $(OBJDIR)\gccfgg0.obj &
	$(OBJDIR)\gccfgg1.obj $(OBJDIR)\gccfgg2.obj $(OBJDIR)\gccfgg3.obj $(OBJDIR)\gccfgg4.obj &
	$(OBJDIR)\gccfgg5.obj $(OBJDIR)\gccfgg6.obj $(OBJDIR)\gccfgg7.obj $(OBJDIR)\gccfgg8.obj &
	$(OBJDIR)\gckeys.obj $(OBJDIR)\gclang.obj $(OBJDIR)\gcmisc.obj $(OBJDIR)\gealst.obj &
	$(OBJDIR)\gearea.obj $(OBJDIR)\gecarb.obj $(OBJDIR)\gecmfd.obj $(OBJDIR)\gectnr.obj &
	$(OBJDIR)\gectrl.obj $(OBJDIR)\gedoit.obj $(OBJDIR)\gedoss.obj $(OBJDIR)\geedit.obj &
	$(OBJDIR)\geedit2.obj $(OBJDIR)\geedit3.obj $(OBJDIR)\gefile.obj $(OBJDIR)\gefind.obj &
	$(OBJDIR)\geglob.obj $(OBJDIR)\gehdre.obj $(OBJDIR)\geinit.obj $(OBJDIR)\geline.obj &
	$(OBJDIR)\gelmsg.obj $(OBJDIR)\gemain.obj $(OBJDIR)\gemenu.obj $(OBJDIR)\gemlst.obj &
	$(OBJDIR)\gemnus.obj $(OBJDIR)\gemrks.obj $(OBJDIR)\gemsgs.obj $(OBJDIR)\genode.obj &
	$(OBJDIR)\geplay.obj $(OBJDIR)\gepost.obj $(OBJDIR)\geqwks.obj $(OBJDIR)\gerand.obj &
	$(OBJDIR)\geread.obj $(OBJDIR)\geread2.obj $(OBJDIR)\gescan.obj $(OBJDIR)\gesrch.obj &
	$(OBJDIR)\gesoup.obj $(OBJDIR)\getpls.obj $(OBJDIR)\geusrbse.obj $(OBJDIR)\geutil.obj &
	$(OBJDIR)\geutil2.obj $(OBJDIR)\geview.obj $(OBJDIR)\gmarea.obj $(OBJDIR)\gehtml.obj &
	$(OBJDIR)\golded3.obj

GOLDNODE_OBJS = &
	$(OBJDIR)\goldnode.obj

RDDT_OBJS = &
	$(OBJDIR)\rddt.obj
!else
GALL_OBJS = &
	$(OBJDIR)\gcrc16tb.obj $(OBJDIR)\gcrc32tb.obj $(OBJDIR)\gcrchash.obj $(OBJDIR)\gcrckeyv.obj &
	$(OBJDIR)\gcrcm16.obj $(OBJDIR)\gcrcm32.obj $(OBJDIR)\gcrcs16.obj $(OBJDIR)\gcrcs32.obj &
	$(OBJDIR)\gdbgerr.obj $(OBJDIR)\gdbgexit.obj $(OBJDIR)\gdbgtrk.obj $(OBJDIR)\gdirposx.obj &
	$(OBJDIR)\geval.obj $(OBJDIR)\gevalhum.obj $(OBJDIR)\gevalrpn.obj $(OBJDIR)\gfile.obj &
	$(OBJDIR)\gfilport.obj $(OBJDIR)\gfilutl1.obj $(OBJDIR)\gfilutl2.obj $(OBJDIR)\gftnaddr.obj &
	$(OBJDIR)\gftnnl.obj $(OBJDIR)\gftnnlfd.obj $(OBJDIR)\gftnnlfu.obj $(OBJDIR)\gftnnlge.obj &
	$(OBJDIR)\gftnnlv7.obj $(OBJDIR)\glog.obj $(OBJDIR)\gmemdbg.obj $(OBJDIR)\gmemutil.obj &
	$(OBJDIR)\gmsgattr.obj $(OBJDIR)\ghdrmime.obj $(OBJDIR)\gprnutil.obj $(OBJDIR)\gsnd.obj &
	$(OBJDIR)\gsndwrap.obj $(OBJDIR)\gstrctyp.obj $(OBJDIR)\gstrmail.obj $(OBJDIR)\gstrname.obj &
	$(OBJDIR)\gstrutil.obj $(OBJDIR)\gtimjuld.obj $(OBJDIR)\gtimutil.obj $(OBJDIR)\gbmh.obj &
	$(OBJDIR)\gfuzzy.obj $(OBJDIR)\gregex.obj $(OBJDIR)\gwildmat.obj $(OBJDIR)\gsearch.obj &
	$(OBJDIR)\gtxtpara.obj $(OBJDIR)\gusrbase.obj $(OBJDIR)\gusrezyc.obj $(OBJDIR)\gusrgold.obj &
	$(OBJDIR)\gusrhuds.obj $(OBJDIR)\gusrmax.obj $(OBJDIR)\gusrpcb.obj $(OBJDIR)\gusrra2.obj &
	$(OBJDIR)\gusrxbbs.obj $(OBJDIR)\gutlclip.obj $(OBJDIR)\gutlcode.obj $(OBJDIR)\gutlgrp.obj &
	$(OBJDIR)\gutlmisc.obj $(OBJDIR)\gutlmtsk.obj $(OBJDIR)\gutltag.obj $(OBJDIR)\gutlvers.obj &
	$(OBJDIR)\gcharset.obj $(OBJDIR)\gutf8.obj $(OBJDIR)\grecode.obj $(OBJDIR)\giniprsr.obj &
	$(OBJDIR)\gutlwin.obj $(OBJDIR)\gutlwinm.obj $(OBJDIR)\gespell.obj

GALL_LIBOBJS = &
	+$(OBJDIR)\gcrc16tb.obj +$(OBJDIR)\gcrc32tb.obj +$(OBJDIR)\gcrchash.obj +$(OBJDIR)\gcrckeyv.obj &
	+$(OBJDIR)\gcrcm16.obj +$(OBJDIR)\gcrcm32.obj +$(OBJDIR)\gcrcs16.obj +$(OBJDIR)\gcrcs32.obj &
	+$(OBJDIR)\gdbgerr.obj +$(OBJDIR)\gdbgexit.obj +$(OBJDIR)\gdbgtrk.obj +$(OBJDIR)\gdirposx.obj &
	+$(OBJDIR)\geval.obj +$(OBJDIR)\gevalhum.obj +$(OBJDIR)\gevalrpn.obj +$(OBJDIR)\gfile.obj &
	+$(OBJDIR)\gfilport.obj +$(OBJDIR)\gfilutl1.obj +$(OBJDIR)\gfilutl2.obj +$(OBJDIR)\gftnaddr.obj &
	+$(OBJDIR)\gftnnl.obj +$(OBJDIR)\gftnnlfd.obj +$(OBJDIR)\gftnnlfu.obj +$(OBJDIR)\gftnnlge.obj &
	+$(OBJDIR)\gftnnlv7.obj +$(OBJDIR)\glog.obj +$(OBJDIR)\gmemdbg.obj +$(OBJDIR)\gmemutil.obj &
	+$(OBJDIR)\gmsgattr.obj +$(OBJDIR)\ghdrmime.obj +$(OBJDIR)\gprnutil.obj +$(OBJDIR)\gsnd.obj &
	+$(OBJDIR)\gsndwrap.obj +$(OBJDIR)\gstrctyp.obj +$(OBJDIR)\gstrmail.obj +$(OBJDIR)\gstrname.obj &
	+$(OBJDIR)\gstrutil.obj +$(OBJDIR)\gtimjuld.obj +$(OBJDIR)\gtimutil.obj +$(OBJDIR)\gbmh.obj &
	+$(OBJDIR)\gfuzzy.obj +$(OBJDIR)\gregex.obj +$(OBJDIR)\gwildmat.obj +$(OBJDIR)\gsearch.obj &
	+$(OBJDIR)\gtxtpara.obj +$(OBJDIR)\gusrbase.obj +$(OBJDIR)\gusrezyc.obj +$(OBJDIR)\gusrgold.obj &
	+$(OBJDIR)\gusrhuds.obj +$(OBJDIR)\gusrmax.obj +$(OBJDIR)\gusrpcb.obj +$(OBJDIR)\gusrra2.obj &
	+$(OBJDIR)\gusrxbbs.obj +$(OBJDIR)\gutlclip.obj +$(OBJDIR)\gutlcode.obj +$(OBJDIR)\gutlgrp.obj &
	+$(OBJDIR)\gutlmisc.obj +$(OBJDIR)\gutlmtsk.obj +$(OBJDIR)\gutltag.obj +$(OBJDIR)\gutlvers.obj &
	+$(OBJDIR)\gcharset.obj +$(OBJDIR)\gutf8.obj +$(OBJDIR)\grecode.obj +$(OBJDIR)\giniprsr.obj &
	+$(OBJDIR)\gutlwin.obj +$(OBJDIR)\gutlwinm.obj +$(OBJDIR)\gespell.obj

GCUI_OBJS = &
	$(OBJDIR)\gkbdbase.obj $(OBJDIR)\gkbdgetm.obj $(OBJDIR)\gkbdwait.obj $(OBJDIR)\gsrchmgr.obj &
	$(OBJDIR)\gmoubase.obj $(OBJDIR)\gvidbase.obj $(OBJDIR)\gvidinit.obj $(OBJDIR)\gwinbase.obj &
	$(OBJDIR)\gwindow.obj $(OBJDIR)\gwinhlp1.obj $(OBJDIR)\gwinhlp2.obj $(OBJDIR)\gwininit.obj &
	$(OBJDIR)\gwinline.obj $(OBJDIR)\gwinmenu.obj $(OBJDIR)\gwinmnub.obj $(OBJDIR)\gwinpckf.obj &
	$(OBJDIR)\gwinpcks.obj $(OBJDIR)\gwinpick.obj $(OBJDIR)\gwinput2.obj

GCUI_LIBOBJS = &
	+$(OBJDIR)\gkbdbase.obj +$(OBJDIR)\gkbdgetm.obj +$(OBJDIR)\gkbdwait.obj +$(OBJDIR)\gsrchmgr.obj &
	+$(OBJDIR)\gmoubase.obj +$(OBJDIR)\gvidbase.obj +$(OBJDIR)\gvidinit.obj +$(OBJDIR)\gwinbase.obj &
	+$(OBJDIR)\gwindow.obj +$(OBJDIR)\gwinhlp1.obj +$(OBJDIR)\gwinhlp2.obj +$(OBJDIR)\gwininit.obj &
	+$(OBJDIR)\gwinline.obj +$(OBJDIR)\gwinmenu.obj +$(OBJDIR)\gwinmnub.obj +$(OBJDIR)\gwinpckf.obj &
	+$(OBJDIR)\gwinpcks.obj +$(OBJDIR)\gwinpick.obj +$(OBJDIR)\gwinput2.obj

GCFG_OBJS = &
	$(OBJDIR)\gedacfg.obj $(OBJDIR)\gxareas.obj $(OBJDIR)\gxcrash.obj $(OBJDIR)\gxdb.obj &
	$(OBJDIR)\gxdutch.obj $(OBJDIR)\gxezy102.obj $(OBJDIR)\gxezy110.obj $(OBJDIR)\gxfd.obj &
	$(OBJDIR)\gxfecho4.obj $(OBJDIR)\gxfecho5.obj $(OBJDIR)\gxfecho6.obj $(OBJDIR)\gxfidpcb.obj &
	$(OBJDIR)\gxfm092.obj $(OBJDIR)\gxfm100.obj $(OBJDIR)\gxfm116.obj $(OBJDIR)\gxgecho.obj &
	$(OBJDIR)\gxhpt.obj $(OBJDIR)\gximail4.obj $(OBJDIR)\gximail5.obj $(OBJDIR)\gximail6.obj &
	$(OBJDIR)\gxinter.obj $(OBJDIR)\gxlora.obj $(OBJDIR)\gxmax3.obj $(OBJDIR)\gxme2.obj &
	$(OBJDIR)\gxopus.obj $(OBJDIR)\gxpcb.obj $(OBJDIR)\gxportal.obj $(OBJDIR)\gxprobrd.obj &
	$(OBJDIR)\gxqfront.obj $(OBJDIR)\gxqecho.obj $(OBJDIR)\gxquick.obj $(OBJDIR)\gxra.obj &
	$(OBJDIR)\gxraecho.obj $(OBJDIR)\gxspace.obj $(OBJDIR)\gxsquish.obj $(OBJDIR)\gxsuper.obj &
	$(OBJDIR)\gxsync.obj $(OBJDIR)\gxtimed.obj $(OBJDIR)\gxtmail.obj $(OBJDIR)\gxts.obj &
	$(OBJDIR)\gxwmail.obj $(OBJDIR)\gxwtr.obj $(OBJDIR)\gxxbbs.obj $(OBJDIR)\gxxmail.obj

GCFG_LIBOBJS = &
	+$(OBJDIR)\gedacfg.obj +$(OBJDIR)\gxareas.obj +$(OBJDIR)\gxcrash.obj +$(OBJDIR)\gxdb.obj &
	+$(OBJDIR)\gxdutch.obj +$(OBJDIR)\gxezy102.obj +$(OBJDIR)\gxezy110.obj +$(OBJDIR)\gxfd.obj &
	+$(OBJDIR)\gxfecho4.obj +$(OBJDIR)\gxfecho5.obj +$(OBJDIR)\gxfecho6.obj +$(OBJDIR)\gxfidpcb.obj &
	+$(OBJDIR)\gxfm092.obj +$(OBJDIR)\gxfm100.obj +$(OBJDIR)\gxfm116.obj +$(OBJDIR)\gxgecho.obj &
	+$(OBJDIR)\gxhpt.obj +$(OBJDIR)\gximail4.obj +$(OBJDIR)\gximail5.obj +$(OBJDIR)\gximail6.obj &
	+$(OBJDIR)\gxinter.obj +$(OBJDIR)\gxlora.obj +$(OBJDIR)\gxmax3.obj +$(OBJDIR)\gxme2.obj &
	+$(OBJDIR)\gxopus.obj +$(OBJDIR)\gxpcb.obj +$(OBJDIR)\gxportal.obj +$(OBJDIR)\gxprobrd.obj &
	+$(OBJDIR)\gxqfront.obj +$(OBJDIR)\gxqecho.obj +$(OBJDIR)\gxquick.obj +$(OBJDIR)\gxra.obj &
	+$(OBJDIR)\gxraecho.obj +$(OBJDIR)\gxspace.obj +$(OBJDIR)\gxsquish.obj +$(OBJDIR)\gxsuper.obj &
	+$(OBJDIR)\gxsync.obj +$(OBJDIR)\gxtimed.obj +$(OBJDIR)\gxtmail.obj +$(OBJDIR)\gxts.obj &
	+$(OBJDIR)\gxwmail.obj +$(OBJDIR)\gxwtr.obj +$(OBJDIR)\gxxbbs.obj +$(OBJDIR)\gxxmail.obj

GMB3_OBJS = &
	$(OBJDIR)\gmoarea.obj $(OBJDIR)\gmohuds.obj $(OBJDIR)\gmoezyc1.obj $(OBJDIR)\gmoezyc2.obj &
	$(OBJDIR)\gmoezyc3.obj $(OBJDIR)\gmoezyc4.obj $(OBJDIR)\gmoezyc5.obj $(OBJDIR)\gmofido1.obj &
	$(OBJDIR)\gmofido2.obj $(OBJDIR)\gmofido3.obj $(OBJDIR)\gmofido4.obj $(OBJDIR)\gmofido5.obj &
	$(OBJDIR)\gmojamm1.obj $(OBJDIR)\gmojamm2.obj $(OBJDIR)\gmojamm3.obj $(OBJDIR)\gmojamm4.obj &
	$(OBJDIR)\gmojamm5.obj $(OBJDIR)\gmopcbd1.obj $(OBJDIR)\gmopcbd2.obj $(OBJDIR)\gmopcbd3.obj &
	$(OBJDIR)\gmopcbd4.obj $(OBJDIR)\gmopcbd5.obj $(OBJDIR)\gmosmb1.obj $(OBJDIR)\gmosmb2.obj &
	$(OBJDIR)\gmosqsh1.obj $(OBJDIR)\gmosqsh2.obj $(OBJDIR)\gmosqsh3.obj $(OBJDIR)\gmosqsh4.obj &
	$(OBJDIR)\gmosqsh5.obj $(OBJDIR)\gmowcat1.obj $(OBJDIR)\gmowcat2.obj $(OBJDIR)\gmowcat3.obj &
	$(OBJDIR)\gmowcat4.obj $(OBJDIR)\gmowcat5.obj $(OBJDIR)\gmoxbbs1.obj $(OBJDIR)\gmoxbbs2.obj &
	$(OBJDIR)\gmoxbbs3.obj $(OBJDIR)\gmoxbbs4.obj $(OBJDIR)\gmoxbbs5.obj

GMB3_LIBOBJS = &
	+$(OBJDIR)\gmoarea.obj +$(OBJDIR)\gmohuds.obj +$(OBJDIR)\gmoezyc1.obj +$(OBJDIR)\gmoezyc2.obj &
	+$(OBJDIR)\gmoezyc3.obj +$(OBJDIR)\gmoezyc4.obj +$(OBJDIR)\gmoezyc5.obj +$(OBJDIR)\gmofido1.obj &
	+$(OBJDIR)\gmofido2.obj +$(OBJDIR)\gmofido3.obj +$(OBJDIR)\gmofido4.obj +$(OBJDIR)\gmofido5.obj &
	+$(OBJDIR)\gmojamm1.obj +$(OBJDIR)\gmojamm2.obj +$(OBJDIR)\gmojamm3.obj +$(OBJDIR)\gmojamm4.obj &
	+$(OBJDIR)\gmojamm5.obj +$(OBJDIR)\gmopcbd1.obj +$(OBJDIR)\gmopcbd2.obj +$(OBJDIR)\gmopcbd3.obj &
	+$(OBJDIR)\gmopcbd4.obj +$(OBJDIR)\gmopcbd5.obj +$(OBJDIR)\gmosmb1.obj +$(OBJDIR)\gmosmb2.obj &
	+$(OBJDIR)\gmosqsh1.obj +$(OBJDIR)\gmosqsh2.obj +$(OBJDIR)\gmosqsh3.obj +$(OBJDIR)\gmosqsh4.obj &
	+$(OBJDIR)\gmosqsh5.obj +$(OBJDIR)\gmowcat1.obj +$(OBJDIR)\gmowcat2.obj +$(OBJDIR)\gmowcat3.obj &
	+$(OBJDIR)\gmowcat4.obj +$(OBJDIR)\gmowcat5.obj +$(OBJDIR)\gmoxbbs1.obj +$(OBJDIR)\gmoxbbs2.obj &
	+$(OBJDIR)\gmoxbbs3.obj +$(OBJDIR)\gmoxbbs4.obj +$(OBJDIR)\gmoxbbs5.obj

UULIB_OBJS = &
	$(OBJDIR)\fptools.obj $(OBJDIR)\uucheck.obj $(OBJDIR)\uuencode.obj $(OBJDIR)\uulib.obj &
	$(OBJDIR)\uunconc.obj $(OBJDIR)\uuscan.obj $(OBJDIR)\uustring.obj $(OBJDIR)\uuutil.obj

UULIB_LIBOBJS = &
	+$(OBJDIR)\fptools.obj +$(OBJDIR)\uucheck.obj +$(OBJDIR)\uuencode.obj +$(OBJDIR)\uulib.obj &
	+$(OBJDIR)\uunconc.obj +$(OBJDIR)\uuscan.obj +$(OBJDIR)\uustring.obj +$(OBJDIR)\uuutil.obj

SMBLIB_OBJS = &
	$(OBJDIR)\lzh.obj $(OBJDIR)\smblib.obj

SMBLIB_LIBOBJS = &
	+$(OBJDIR)\lzh.obj +$(OBJDIR)\smblib.obj

GLIBC_OBJS = &
	$(OBJDIR)\regex.obj $(OBJDIR)\dummy.obj

GLIBC_LIBOBJS = &
	+$(OBJDIR)\regex.obj +$(OBJDIR)\dummy.obj

GOLDED3_OBJS = &
	$(OBJDIR)\gcalst.obj $(OBJDIR)\gcarea.obj $(OBJDIR)\gccfgg.obj $(OBJDIR)\gccfgg0.obj &
	$(OBJDIR)\gccfgg1.obj $(OBJDIR)\gccfgg2.obj $(OBJDIR)\gccfgg3.obj $(OBJDIR)\gccfgg4.obj &
	$(OBJDIR)\gccfgg5.obj $(OBJDIR)\gccfgg6.obj $(OBJDIR)\gccfgg7.obj $(OBJDIR)\gccfgg8.obj &
	$(OBJDIR)\gckeys.obj $(OBJDIR)\gclang.obj $(OBJDIR)\gcmisc.obj $(OBJDIR)\gealst.obj &
	$(OBJDIR)\gearea.obj $(OBJDIR)\gecarb.obj $(OBJDIR)\gecmfd.obj $(OBJDIR)\gectnr.obj &
	$(OBJDIR)\gectrl.obj $(OBJDIR)\gedoit.obj $(OBJDIR)\gedoss.obj $(OBJDIR)\geedit.obj &
	$(OBJDIR)\geedit2.obj $(OBJDIR)\geedit3.obj $(OBJDIR)\gefile.obj $(OBJDIR)\gefind.obj &
	$(OBJDIR)\geglob.obj $(OBJDIR)\gehdre.obj $(OBJDIR)\geinit.obj $(OBJDIR)\geline.obj &
	$(OBJDIR)\gelmsg.obj $(OBJDIR)\gemain.obj $(OBJDIR)\gemenu.obj $(OBJDIR)\gemlst.obj &
	$(OBJDIR)\gemnus.obj $(OBJDIR)\gemrks.obj $(OBJDIR)\gemsgs.obj $(OBJDIR)\genode.obj &
	$(OBJDIR)\geplay.obj $(OBJDIR)\gepost.obj $(OBJDIR)\geqwks.obj $(OBJDIR)\gerand.obj &
	$(OBJDIR)\geread.obj $(OBJDIR)\geread2.obj $(OBJDIR)\gescan.obj $(OBJDIR)\gesrch.obj &
	$(OBJDIR)\gesoup.obj $(OBJDIR)\getpls.obj $(OBJDIR)\geusrbse.obj $(OBJDIR)\geutil.obj &
	$(OBJDIR)\geutil2.obj $(OBJDIR)\geview.obj $(OBJDIR)\gmarea.obj $(OBJDIR)\gehtml.obj &
	$(OBJDIR)\golded3.obj

GOLDNODE_OBJS = &
	$(OBJDIR)\goldnode.obj

RDDT_OBJS = &
	$(OBJDIR)\rddt.obj

!endif
!endif
!endif

$(LIBDIR)\gall.lib: $(GALL_OBJS)
	$(LIBR) -q -b -n $^@ $(GALL_LIBOBJS)

$(LIBDIR)\gcui.lib: $(GCUI_OBJS)
	$(LIBR) -q -b -n $^@ $(GCUI_LIBOBJS)

$(LIBDIR)\gcfg.lib: $(GCFG_OBJS)
	$(LIBR) -q -b -n $^@ $(GCFG_LIBOBJS)

$(LIBDIR)\gmb3.lib: $(GMB3_OBJS)
	$(LIBR) -q -b -n $^@ $(GMB3_LIBOBJS)

$(LIBDIR)\uulib.lib: $(UULIB_OBJS)
	$(LIBR) -q -b -n $^@ $(UULIB_LIBOBJS)

$(LIBDIR)\smblib.lib: $(SMBLIB_OBJS)
	$(LIBR) -q -b -n $^@ $(SMBLIB_LIBOBJS)

$(LIBDIR)\glibc.lib: $(GLIBC_OBJS)
	$(LIBR) -q -b -n $^@ $(GLIBC_LIBOBJS)

#  wlib reads the DLL's export table and writes the import library
#  from it; nothing else is needed to call into UCONV.
$(LIBDIR)\uconv.lib:
	$(LIBR) -q $^@ +$(OS2DLLPATH)\UCONV.DLL

$(GED): $(GOLDED3_OBJS) $(LIBS) $(ULSLIB)
	@%create $(LIBDIR)\ged.lnk
	@%append $(LIBDIR)\ged.lnk system $(LSYSTEM)
	@%append $(LIBDIR)\ged.lnk name $^@
	@%append $(LIBDIR)\ged.lnk option quiet, caseexact
	@%append $(LIBDIR)\ged.lnk file { $(GOLDED3_OBJS) }
	@%append $(LIBDIR)\ged.lnk library { $(LIBS) $(SYSLIBS) }
	$(LINK) @$(LIBDIR)\ged.lnk
	@if exist $^@.elf move /y $^@.elf $^@ > nul

#  goldnode and rddt each need golded3.obj for the version banner.
$(GN): $(GOLDNODE_OBJS) $(OBJDIR)\golded3.obj $(LIBS) $(ULSLIB)
	@%create $(LIBDIR)\gn.lnk
	@%append $(LIBDIR)\gn.lnk system $(LSYSTEM)
	@%append $(LIBDIR)\gn.lnk name $^@
	@%append $(LIBDIR)\gn.lnk option quiet, caseexact
	@%append $(LIBDIR)\gn.lnk file { $(GOLDNODE_OBJS) $(OBJDIR)\golded3.obj }
	@%append $(LIBDIR)\gn.lnk library { $(LIBS) $(SYSLIBS) }
	$(LINK) @$(LIBDIR)\gn.lnk
	@if exist $^@.elf move /y $^@.elf $^@ > nul

$(RD): $(RDDT_OBJS) $(OBJDIR)\golded3.obj $(LIBS) $(ULSLIB)
	@%create $(LIBDIR)\rd.lnk
	@%append $(LIBDIR)\rd.lnk system $(LSYSTEM)
	@%append $(LIBDIR)\rd.lnk name $^@
	@%append $(LIBDIR)\rd.lnk option quiet, caseexact
	@%append $(LIBDIR)\rd.lnk file { $(RDDT_OBJS) $(OBJDIR)\golded3.obj }
	@%append $(LIBDIR)\rd.lnk library { $(LIBS) $(SYSLIBS) }
	$(LINK) @$(LIBDIR)\rd.lnk
	@if exist $^@.elf move /y $^@.elf $^@ > nul

clean: .SYMBOLIC
	@if exist $(OBJDIR)\*.obj del /q $(OBJDIR)\*.obj
	@if exist $(LIBDIR)\*.lib del /q $(LIBDIR)\*.lib
	@if exist $(LIBDIR)\*.lst del /q $(LIBDIR)\*.lst
	@if exist $(LIBDIR)\*.lnk del /q $(LIBDIR)\*.lnk
	@if exist $(GED) del /q $(GED)
	@if exist $(GN) del /q $(GN)
	@if exist $(RD) del /q $(RD)
