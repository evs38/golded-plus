#  ------------------------------------------------------------------
#  GoldED+ built with a Borland C++ toolkit and Borland MAKE.
#
#  This is the Borland toolkit's own makefile - it drives bcc32, tlib
#  and tlink32 directly and needs nothing from the GNU side.
#
#      make -f gedbcc.mak                Win32, Borland C++ 5.5.1
#      make -f gedbcc.mak -DBC5=1        Win32, Borland C++ 5.02
#      make -f gedbcc.mak -DDOS32=1      32-bit DOS (DPMI), 5.02
#      make -f gedbcc.mak clean          remove what one of them built
#
#  Use gedbcc.bat instead of calling make directly: MAKE 5.0 aborts
#  with "Command arguments too long" when it inherits a present-day
#  Windows PATH, and the batch file trims it first.
#
#  5.5.1 is the one to reach for: it is the later compiler and the one
#  with the usable C++ library. 5.02 stays because it is what builds
#  the DOS target, and because it is the compiler this program was
#  written for.
#
#  BCROOT defaults to where each installer puts its own - C:\BC55 for
#  5.5.1, C:\BC5 for 5.02 - and is overridden on the command line
#  when they are somewhere else:
#
#      make -f gedbcc.mak -DBCROOT=C:\BORLAND\BCC55
#
#  The compiler switches live in gedbcc.rsp (5.02 Win32), gedbc55.rsp
#  (5.5 Win32) and gedbcx.rsp (DOS), not here, for the same reason: MAKE cannot hand bcc32 a
#  command line long enough to hold them. They are response files
#  rather than -config files on purpose: a response file adds to the
#  compiler's own BIN\bcc32.cfg, where the toolchain's include and
#  library paths live, whereas +cfg would replace it and we would have
#  to name BCROOT a second time.
#
#  The object rules track no header dependencies: bcc32 has no -MD, and
#  Borland MAKE both cannot work them out and quietly gives up on a
#  dependency list of every header in the tree. So after editing a
#  header, clean before building:
#
#      gedbcc clean
#      gedbcc
#
#  The source lists below come from the .all files - `wcn' for Win32,
#  which is the source set every native Win32 build uses, and `bcx' for
#  DOS, which is this compiler's own historical set. They are written
#  out by hand; when a source is added to a .all file, add it here too.
#  ------------------------------------------------------------------

!if $d(DOS32)
#  DOS is 5.02's alone: the 5.5 command-line compiler is Win32 only
#  and has neither the -WX target nor dpmi32.lib.
BCDEF  = C:\BC5
RSP    = gedbcx.rsp
OBJDIR = obj\bcx
LIBDIR = lib\bcx
SFX    = bcx
#  -WX is a target, not just a code-generation switch: at link time it
#  is what picks c0x32.obj and dpmi32.lib and puts the DOS/4GW-style
#  stub on the front, so it has to be on the link line as well as in
#  the response file the compiler reads.
LNKOPT = -WX
!elif $d(BC5)
BCDEF  = C:\BC5
RSP    = gedbcc.rsp
OBJDIR = obj\bcw
LIBDIR = lib\bcw
SFX    = bcw
LNKOPT =
!else
BCDEF  = C:\BC55
RSP    = gedbc55.rsp
OBJDIR = obj\bc55
LIBDIR = lib\bc55
SFX    = bc55
LNKOPT =
!endif

!if !$d(BCROOT)
BCROOT = $(BCDEF)
!endif

CC     = $(BCROOT)\BIN\bcc32
LIBR   = $(BCROOT)\BIN\tlib

#  /P128 raises the library page size. tlib's default of 16 cannot address
#  an archive this large, and says so. The duplicate-symbol warnings it
#  prints while building these are the same effect from the other end:
#  Borland instantiates every member of every class template that gets
#  used, so each object carries its own copy of the STL code it touched.
#  tlib keeps the first and the warnings are harmless.
LIBPAGE = /P128
BINDIR = bin

#  Where the implicit rules look for sources. Object base names are
#  unique across the whole tree, so one flat object directory is enough
#  and every library can share it.
.path.cpp = goldlib\gall;goldlib\gcui;goldlib\gcfg;goldlib\gmb3;goldlib\uulib;goldlib\smblib;goldlib\glibc;golded3;goldnode;rddt
.path.c   = goldlib\gall;goldlib\gcui;goldlib\gcfg;goldlib\gmb3;goldlib\uulib;goldlib\smblib;goldlib\glibc;golded3;goldnode;rddt
.path.obj = $(OBJDIR)

!if $d(DOS32)
GALL_OBJS = \
	$(OBJDIR)\gcrc16tb.obj $(OBJDIR)\gcrc32tb.obj $(OBJDIR)\gcrchash.obj $(OBJDIR)\gcrckeyv.obj \
	$(OBJDIR)\gcrcm16.obj $(OBJDIR)\gcrcm32.obj $(OBJDIR)\gcrcs16.obj $(OBJDIR)\gcrcs32.obj \
	$(OBJDIR)\gdbgerr.obj $(OBJDIR)\gdbgexit.obj $(OBJDIR)\gdbgtrk.obj $(OBJDIR)\gdirposx.obj \
	$(OBJDIR)\geval.obj $(OBJDIR)\gevalhum.obj $(OBJDIR)\gevalrpn.obj $(OBJDIR)\gfile.obj \
	$(OBJDIR)\gfilport.obj $(OBJDIR)\gfilutl1.obj $(OBJDIR)\gfilutl2.obj $(OBJDIR)\gftnaddr.obj \
	$(OBJDIR)\gftnnl.obj $(OBJDIR)\gftnnlfd.obj $(OBJDIR)\gftnnlfu.obj $(OBJDIR)\gftnnlge.obj \
	$(OBJDIR)\gftnnlv7.obj $(OBJDIR)\glog.obj $(OBJDIR)\gmemdbg.obj $(OBJDIR)\gmemutil.obj \
	$(OBJDIR)\gmsgattr.obj $(OBJDIR)\ghdrmime.obj $(OBJDIR)\gprnutil.obj $(OBJDIR)\gsnd.obj \
	$(OBJDIR)\gsndwrap.obj $(OBJDIR)\gstrctyp.obj $(OBJDIR)\gstrmail.obj $(OBJDIR)\gstrname.obj \
	$(OBJDIR)\gstrutil.obj $(OBJDIR)\gtimjuld.obj $(OBJDIR)\gtimutil.obj $(OBJDIR)\gbmh.obj \
	$(OBJDIR)\gfuzzy.obj $(OBJDIR)\gregex.obj $(OBJDIR)\gwildmat.obj $(OBJDIR)\gsearch.obj \
	$(OBJDIR)\gtxtpara.obj $(OBJDIR)\gusrbase.obj $(OBJDIR)\gusrezyc.obj $(OBJDIR)\gusrgold.obj \
	$(OBJDIR)\gusrhuds.obj $(OBJDIR)\gusrmax.obj $(OBJDIR)\gusrpcb.obj $(OBJDIR)\gusrra2.obj \
	$(OBJDIR)\gusrxbbs.obj $(OBJDIR)\gutlclip.obj $(OBJDIR)\gutlcode.obj $(OBJDIR)\gutlgrp.obj \
	$(OBJDIR)\gutlmisc.obj $(OBJDIR)\gutlmtsk.obj $(OBJDIR)\gutltag.obj $(OBJDIR)\gutlvers.obj \
	$(OBJDIR)\gcharset.obj $(OBJDIR)\gutf8.obj $(OBJDIR)\grecode.obj $(OBJDIR)\giniprsr.obj \
	$(OBJDIR)\gutldos.obj $(OBJDIR)\gutlbcx.obj $(OBJDIR)\gespell.obj

GCUI_OBJS = \
	$(OBJDIR)\gkbdbase.obj $(OBJDIR)\gkbdgetm.obj $(OBJDIR)\gkbdwait.obj $(OBJDIR)\gsrchmgr.obj \
	$(OBJDIR)\gmoubase.obj $(OBJDIR)\gvidbase.obj $(OBJDIR)\gvidinit.obj $(OBJDIR)\gwinbase.obj \
	$(OBJDIR)\gwindow.obj $(OBJDIR)\gwinhlp1.obj $(OBJDIR)\gwinhlp2.obj $(OBJDIR)\gwininit.obj \
	$(OBJDIR)\gwinline.obj $(OBJDIR)\gwinmenu.obj $(OBJDIR)\gwinmnub.obj $(OBJDIR)\gwinpckf.obj \
	$(OBJDIR)\gwinpcks.obj $(OBJDIR)\gwinpick.obj $(OBJDIR)\gwinput2.obj

GCFG_OBJS = \
	$(OBJDIR)\gedacfg.obj $(OBJDIR)\gxareas.obj $(OBJDIR)\gxcrash.obj $(OBJDIR)\gxdb.obj \
	$(OBJDIR)\gxdutch.obj $(OBJDIR)\gxezy102.obj $(OBJDIR)\gxezy110.obj $(OBJDIR)\gxfd.obj \
	$(OBJDIR)\gxfecho4.obj $(OBJDIR)\gxfecho5.obj $(OBJDIR)\gxfecho6.obj $(OBJDIR)\gxfidpcb.obj \
	$(OBJDIR)\gxfm092.obj $(OBJDIR)\gxfm100.obj $(OBJDIR)\gxfm116.obj $(OBJDIR)\gxgecho.obj \
	$(OBJDIR)\gxhpt.obj $(OBJDIR)\gximail4.obj $(OBJDIR)\gximail5.obj $(OBJDIR)\gximail6.obj \
	$(OBJDIR)\gxinter.obj $(OBJDIR)\gxlora.obj $(OBJDIR)\gxmax3.obj $(OBJDIR)\gxme2.obj \
	$(OBJDIR)\gxopus.obj $(OBJDIR)\gxpcb.obj $(OBJDIR)\gxportal.obj $(OBJDIR)\gxprobrd.obj \
	$(OBJDIR)\gxqfront.obj $(OBJDIR)\gxqecho.obj $(OBJDIR)\gxquick.obj $(OBJDIR)\gxra.obj \
	$(OBJDIR)\gxraecho.obj $(OBJDIR)\gxspace.obj $(OBJDIR)\gxsquish.obj $(OBJDIR)\gxsuper.obj \
	$(OBJDIR)\gxsync.obj $(OBJDIR)\gxtimed.obj $(OBJDIR)\gxtmail.obj $(OBJDIR)\gxts.obj \
	$(OBJDIR)\gxwmail.obj $(OBJDIR)\gxwtr.obj $(OBJDIR)\gxxbbs.obj $(OBJDIR)\gxxmail.obj

GMB3_OBJS = \
	$(OBJDIR)\gmoarea.obj $(OBJDIR)\gmohuds.obj $(OBJDIR)\gmoezyc1.obj $(OBJDIR)\gmoezyc2.obj \
	$(OBJDIR)\gmoezyc3.obj $(OBJDIR)\gmoezyc4.obj $(OBJDIR)\gmoezyc5.obj $(OBJDIR)\gmofido1.obj \
	$(OBJDIR)\gmofido2.obj $(OBJDIR)\gmofido3.obj $(OBJDIR)\gmofido4.obj $(OBJDIR)\gmofido5.obj \
	$(OBJDIR)\gmojamm1.obj $(OBJDIR)\gmojamm2.obj $(OBJDIR)\gmojamm3.obj $(OBJDIR)\gmojamm4.obj \
	$(OBJDIR)\gmojamm5.obj $(OBJDIR)\gmopcbd1.obj $(OBJDIR)\gmopcbd2.obj $(OBJDIR)\gmopcbd3.obj \
	$(OBJDIR)\gmopcbd4.obj $(OBJDIR)\gmopcbd5.obj $(OBJDIR)\gmosmb1.obj $(OBJDIR)\gmosmb2.obj \
	$(OBJDIR)\gmosqsh1.obj $(OBJDIR)\gmosqsh2.obj $(OBJDIR)\gmosqsh3.obj $(OBJDIR)\gmosqsh4.obj \
	$(OBJDIR)\gmosqsh5.obj $(OBJDIR)\gmowcat1.obj $(OBJDIR)\gmowcat2.obj $(OBJDIR)\gmowcat3.obj \
	$(OBJDIR)\gmowcat4.obj $(OBJDIR)\gmowcat5.obj $(OBJDIR)\gmoxbbs1.obj $(OBJDIR)\gmoxbbs2.obj \
	$(OBJDIR)\gmoxbbs3.obj $(OBJDIR)\gmoxbbs4.obj $(OBJDIR)\gmoxbbs5.obj

UULIB_OBJS = \
	$(OBJDIR)\fptools.obj $(OBJDIR)\uucheck.obj $(OBJDIR)\uuencode.obj $(OBJDIR)\uulib.obj \
	$(OBJDIR)\uunconc.obj $(OBJDIR)\uuscan.obj $(OBJDIR)\uustring.obj $(OBJDIR)\uuutil.obj

SMBLIB_OBJS = \
	$(OBJDIR)\lzh.obj $(OBJDIR)\smblib.obj

GLIBC_OBJS = \
	$(OBJDIR)\regex.obj $(OBJDIR)\dummy.obj

GOLDED3_OBJS = \
	$(OBJDIR)\gcalst.obj $(OBJDIR)\gcarea.obj $(OBJDIR)\gccfgg.obj $(OBJDIR)\gccfgg0.obj \
	$(OBJDIR)\gccfgg1.obj $(OBJDIR)\gccfgg2.obj $(OBJDIR)\gccfgg3.obj $(OBJDIR)\gccfgg4.obj \
	$(OBJDIR)\gccfgg5.obj $(OBJDIR)\gccfgg6.obj $(OBJDIR)\gccfgg7.obj $(OBJDIR)\gccfgg8.obj \
	$(OBJDIR)\gckeys.obj $(OBJDIR)\gclang.obj $(OBJDIR)\gcmisc.obj $(OBJDIR)\gealst.obj \
	$(OBJDIR)\gearea.obj $(OBJDIR)\gecarb.obj $(OBJDIR)\gecmfd.obj $(OBJDIR)\gectnr.obj \
	$(OBJDIR)\gectrl.obj $(OBJDIR)\gedoit.obj $(OBJDIR)\gedoss.obj $(OBJDIR)\geedit.obj \
	$(OBJDIR)\geedit2.obj $(OBJDIR)\geedit3.obj $(OBJDIR)\gefile.obj $(OBJDIR)\gefind.obj \
	$(OBJDIR)\geglob.obj $(OBJDIR)\gehdre.obj $(OBJDIR)\geinit.obj $(OBJDIR)\geline.obj \
	$(OBJDIR)\gelmsg.obj $(OBJDIR)\gemain.obj $(OBJDIR)\gemenu.obj $(OBJDIR)\gemlst.obj \
	$(OBJDIR)\gemnus.obj $(OBJDIR)\gemrks.obj $(OBJDIR)\gemsgs.obj $(OBJDIR)\genode.obj \
	$(OBJDIR)\geplay.obj $(OBJDIR)\gepost.obj $(OBJDIR)\geqwks.obj $(OBJDIR)\gerand.obj \
	$(OBJDIR)\geread.obj $(OBJDIR)\geread2.obj $(OBJDIR)\gescan.obj $(OBJDIR)\gesrch.obj \
	$(OBJDIR)\gesoup.obj $(OBJDIR)\getpls.obj $(OBJDIR)\geusrbse.obj $(OBJDIR)\geutil.obj \
	$(OBJDIR)\geutil2.obj $(OBJDIR)\geview.obj $(OBJDIR)\gmarea.obj $(OBJDIR)\gehtml.obj \
	$(OBJDIR)\golded3.obj

GOLDNODE_OBJS = \
	$(OBJDIR)\goldnode.obj

RDDT_OBJS = \
	$(OBJDIR)\rddt.obj
!else
GALL_OBJS = \
	$(OBJDIR)\gcrc16tb.obj $(OBJDIR)\gcrc32tb.obj $(OBJDIR)\gcrchash.obj $(OBJDIR)\gcrckeyv.obj \
	$(OBJDIR)\gcrcm16.obj $(OBJDIR)\gcrcm32.obj $(OBJDIR)\gcrcs16.obj $(OBJDIR)\gcrcs32.obj \
	$(OBJDIR)\gdbgerr.obj $(OBJDIR)\gdbgexit.obj $(OBJDIR)\gdbgtrk.obj $(OBJDIR)\gdirposx.obj \
	$(OBJDIR)\geval.obj $(OBJDIR)\gevalhum.obj $(OBJDIR)\gevalrpn.obj $(OBJDIR)\gfile.obj \
	$(OBJDIR)\gfilport.obj $(OBJDIR)\gfilutl1.obj $(OBJDIR)\gfilutl2.obj $(OBJDIR)\gftnaddr.obj \
	$(OBJDIR)\gftnnl.obj $(OBJDIR)\gftnnlfd.obj $(OBJDIR)\gftnnlfu.obj $(OBJDIR)\gftnnlge.obj \
	$(OBJDIR)\gftnnlv7.obj $(OBJDIR)\glog.obj $(OBJDIR)\gmemdbg.obj $(OBJDIR)\gmemutil.obj \
	$(OBJDIR)\gmsgattr.obj $(OBJDIR)\ghdrmime.obj $(OBJDIR)\gprnutil.obj $(OBJDIR)\gsnd.obj \
	$(OBJDIR)\gsndwrap.obj $(OBJDIR)\gstrctyp.obj $(OBJDIR)\gstrmail.obj $(OBJDIR)\gstrname.obj \
	$(OBJDIR)\gstrutil.obj $(OBJDIR)\gtimjuld.obj $(OBJDIR)\gtimutil.obj $(OBJDIR)\gbmh.obj \
	$(OBJDIR)\gfuzzy.obj $(OBJDIR)\gregex.obj $(OBJDIR)\gwildmat.obj $(OBJDIR)\gsearch.obj \
	$(OBJDIR)\gtxtpara.obj $(OBJDIR)\gusrbase.obj $(OBJDIR)\gusrezyc.obj $(OBJDIR)\gusrgold.obj \
	$(OBJDIR)\gusrhuds.obj $(OBJDIR)\gusrmax.obj $(OBJDIR)\gusrpcb.obj $(OBJDIR)\gusrra2.obj \
	$(OBJDIR)\gusrxbbs.obj $(OBJDIR)\gutlclip.obj $(OBJDIR)\gutlcode.obj $(OBJDIR)\gutlgrp.obj \
	$(OBJDIR)\gutlmisc.obj $(OBJDIR)\gutlmtsk.obj $(OBJDIR)\gutltag.obj $(OBJDIR)\gutlvers.obj \
	$(OBJDIR)\gcharset.obj $(OBJDIR)\gutf8.obj $(OBJDIR)\grecode.obj $(OBJDIR)\giniprsr.obj \
	$(OBJDIR)\gutlwin.obj $(OBJDIR)\gutlwinm.obj $(OBJDIR)\gespell.obj

GCUI_OBJS = \
	$(OBJDIR)\gkbdbase.obj $(OBJDIR)\gkbdgetm.obj $(OBJDIR)\gkbdwait.obj $(OBJDIR)\gsrchmgr.obj \
	$(OBJDIR)\gmoubase.obj $(OBJDIR)\gvidbase.obj $(OBJDIR)\gvidinit.obj $(OBJDIR)\gwinbase.obj \
	$(OBJDIR)\gwindow.obj $(OBJDIR)\gwinhlp1.obj $(OBJDIR)\gwinhlp2.obj $(OBJDIR)\gwininit.obj \
	$(OBJDIR)\gwinline.obj $(OBJDIR)\gwinmenu.obj $(OBJDIR)\gwinmnub.obj $(OBJDIR)\gwinpckf.obj \
	$(OBJDIR)\gwinpcks.obj $(OBJDIR)\gwinpick.obj $(OBJDIR)\gwinput2.obj

GCFG_OBJS = \
	$(OBJDIR)\gedacfg.obj $(OBJDIR)\gxareas.obj $(OBJDIR)\gxcrash.obj $(OBJDIR)\gxdb.obj \
	$(OBJDIR)\gxdutch.obj $(OBJDIR)\gxezy102.obj $(OBJDIR)\gxezy110.obj $(OBJDIR)\gxfd.obj \
	$(OBJDIR)\gxfecho4.obj $(OBJDIR)\gxfecho5.obj $(OBJDIR)\gxfecho6.obj $(OBJDIR)\gxfidpcb.obj \
	$(OBJDIR)\gxfm092.obj $(OBJDIR)\gxfm100.obj $(OBJDIR)\gxfm116.obj $(OBJDIR)\gxgecho.obj \
	$(OBJDIR)\gxhpt.obj $(OBJDIR)\gximail4.obj $(OBJDIR)\gximail5.obj $(OBJDIR)\gximail6.obj \
	$(OBJDIR)\gxinter.obj $(OBJDIR)\gxlora.obj $(OBJDIR)\gxmax3.obj $(OBJDIR)\gxme2.obj \
	$(OBJDIR)\gxopus.obj $(OBJDIR)\gxpcb.obj $(OBJDIR)\gxportal.obj $(OBJDIR)\gxprobrd.obj \
	$(OBJDIR)\gxqfront.obj $(OBJDIR)\gxqecho.obj $(OBJDIR)\gxquick.obj $(OBJDIR)\gxra.obj \
	$(OBJDIR)\gxraecho.obj $(OBJDIR)\gxspace.obj $(OBJDIR)\gxsquish.obj $(OBJDIR)\gxsuper.obj \
	$(OBJDIR)\gxsync.obj $(OBJDIR)\gxtimed.obj $(OBJDIR)\gxtmail.obj $(OBJDIR)\gxts.obj \
	$(OBJDIR)\gxwmail.obj $(OBJDIR)\gxwtr.obj $(OBJDIR)\gxxbbs.obj $(OBJDIR)\gxxmail.obj

GMB3_OBJS = \
	$(OBJDIR)\gmoarea.obj $(OBJDIR)\gmohuds.obj $(OBJDIR)\gmoezyc1.obj $(OBJDIR)\gmoezyc2.obj \
	$(OBJDIR)\gmoezyc3.obj $(OBJDIR)\gmoezyc4.obj $(OBJDIR)\gmoezyc5.obj $(OBJDIR)\gmofido1.obj \
	$(OBJDIR)\gmofido2.obj $(OBJDIR)\gmofido3.obj $(OBJDIR)\gmofido4.obj $(OBJDIR)\gmofido5.obj \
	$(OBJDIR)\gmojamm1.obj $(OBJDIR)\gmojamm2.obj $(OBJDIR)\gmojamm3.obj $(OBJDIR)\gmojamm4.obj \
	$(OBJDIR)\gmojamm5.obj $(OBJDIR)\gmopcbd1.obj $(OBJDIR)\gmopcbd2.obj $(OBJDIR)\gmopcbd3.obj \
	$(OBJDIR)\gmopcbd4.obj $(OBJDIR)\gmopcbd5.obj $(OBJDIR)\gmosmb1.obj $(OBJDIR)\gmosmb2.obj \
	$(OBJDIR)\gmosqsh1.obj $(OBJDIR)\gmosqsh2.obj $(OBJDIR)\gmosqsh3.obj $(OBJDIR)\gmosqsh4.obj \
	$(OBJDIR)\gmosqsh5.obj $(OBJDIR)\gmowcat1.obj $(OBJDIR)\gmowcat2.obj $(OBJDIR)\gmowcat3.obj \
	$(OBJDIR)\gmowcat4.obj $(OBJDIR)\gmowcat5.obj $(OBJDIR)\gmoxbbs1.obj $(OBJDIR)\gmoxbbs2.obj \
	$(OBJDIR)\gmoxbbs3.obj $(OBJDIR)\gmoxbbs4.obj $(OBJDIR)\gmoxbbs5.obj

UULIB_OBJS = \
	$(OBJDIR)\fptools.obj $(OBJDIR)\uucheck.obj $(OBJDIR)\uuencode.obj $(OBJDIR)\uulib.obj \
	$(OBJDIR)\uunconc.obj $(OBJDIR)\uuscan.obj $(OBJDIR)\uustring.obj $(OBJDIR)\uuutil.obj

SMBLIB_OBJS = \
	$(OBJDIR)\lzh.obj $(OBJDIR)\smblib.obj

GLIBC_OBJS = \
	$(OBJDIR)\regex.obj $(OBJDIR)\dummy.obj

GOLDED3_OBJS = \
	$(OBJDIR)\gcalst.obj $(OBJDIR)\gcarea.obj $(OBJDIR)\gccfgg.obj $(OBJDIR)\gccfgg0.obj \
	$(OBJDIR)\gccfgg1.obj $(OBJDIR)\gccfgg2.obj $(OBJDIR)\gccfgg3.obj $(OBJDIR)\gccfgg4.obj \
	$(OBJDIR)\gccfgg5.obj $(OBJDIR)\gccfgg6.obj $(OBJDIR)\gccfgg7.obj $(OBJDIR)\gccfgg8.obj \
	$(OBJDIR)\gckeys.obj $(OBJDIR)\gclang.obj $(OBJDIR)\gcmisc.obj $(OBJDIR)\gealst.obj \
	$(OBJDIR)\gearea.obj $(OBJDIR)\gecarb.obj $(OBJDIR)\gecmfd.obj $(OBJDIR)\gectnr.obj \
	$(OBJDIR)\gectrl.obj $(OBJDIR)\gedoit.obj $(OBJDIR)\gedoss.obj $(OBJDIR)\geedit.obj \
	$(OBJDIR)\geedit2.obj $(OBJDIR)\geedit3.obj $(OBJDIR)\gefile.obj $(OBJDIR)\gefind.obj \
	$(OBJDIR)\geglob.obj $(OBJDIR)\gehdre.obj $(OBJDIR)\geinit.obj $(OBJDIR)\geline.obj \
	$(OBJDIR)\gelmsg.obj $(OBJDIR)\gemain.obj $(OBJDIR)\gemenu.obj $(OBJDIR)\gemlst.obj \
	$(OBJDIR)\gemnus.obj $(OBJDIR)\gemrks.obj $(OBJDIR)\gemsgs.obj $(OBJDIR)\genode.obj \
	$(OBJDIR)\geplay.obj $(OBJDIR)\gepost.obj $(OBJDIR)\geqwks.obj $(OBJDIR)\gerand.obj \
	$(OBJDIR)\geread.obj $(OBJDIR)\geread2.obj $(OBJDIR)\gescan.obj $(OBJDIR)\gesrch.obj \
	$(OBJDIR)\gesoup.obj $(OBJDIR)\getpls.obj $(OBJDIR)\geusrbse.obj $(OBJDIR)\geutil.obj \
	$(OBJDIR)\geutil2.obj $(OBJDIR)\geview.obj $(OBJDIR)\gmarea.obj $(OBJDIR)\gehtml.obj \
	$(OBJDIR)\golded3.obj

GOLDNODE_OBJS = \
	$(OBJDIR)\goldnode.obj

RDDT_OBJS = \
	$(OBJDIR)\rddt.obj
!endif

LIBS = $(LIBDIR)\gall.lib $(LIBDIR)\gcui.lib $(LIBDIR)\gcfg.lib \
       $(LIBDIR)\gmb3.lib $(LIBDIR)\uulib.lib $(LIBDIR)\smblib.lib \
       $(LIBDIR)\glibc.lib

GED = $(BINDIR)\ged$(SFX).exe
GN  = $(BINDIR)\gn$(SFX).exe
RD  = $(BINDIR)\rddt$(SFX).exe

all: dirs $(GED) $(GN) $(RD)
	@echo Built $(GED), $(GN) and $(RD)

dirs:
	-@md $(OBJDIR)
	-@md $(LIBDIR)
	-@md $(BINDIR)

#  -n names the output directory; the object keeps the source's base
#  name, which is what .path.obj expects to find.
.cpp.obj:
	$(CC) @$(RSP) -n$(OBJDIR) $<

.c.obj:
	$(CC) @$(RSP) -n$(OBJDIR) $<

!if $d(DOS32)
$(LIBDIR)\gall.lib: $(GALL_OBJS)
	-@del $(LIBDIR)\gall.lib
	$(LIBR) $(LIBPAGE) $(LIBDIR)\gall.lib @&&|
+$(OBJDIR)\gcrc16tb.obj &
+$(OBJDIR)\gcrc32tb.obj &
+$(OBJDIR)\gcrchash.obj &
+$(OBJDIR)\gcrckeyv.obj &
+$(OBJDIR)\gcrcm16.obj &
+$(OBJDIR)\gcrcm32.obj &
+$(OBJDIR)\gcrcs16.obj &
+$(OBJDIR)\gcrcs32.obj &
+$(OBJDIR)\gdbgerr.obj &
+$(OBJDIR)\gdbgexit.obj &
+$(OBJDIR)\gdbgtrk.obj &
+$(OBJDIR)\gdirposx.obj &
+$(OBJDIR)\geval.obj &
+$(OBJDIR)\gevalhum.obj &
+$(OBJDIR)\gevalrpn.obj &
+$(OBJDIR)\gfile.obj &
+$(OBJDIR)\gfilport.obj &
+$(OBJDIR)\gfilutl1.obj &
+$(OBJDIR)\gfilutl2.obj &
+$(OBJDIR)\gftnaddr.obj &
+$(OBJDIR)\gftnnl.obj &
+$(OBJDIR)\gftnnlfd.obj &
+$(OBJDIR)\gftnnlfu.obj &
+$(OBJDIR)\gftnnlge.obj &
+$(OBJDIR)\gftnnlv7.obj &
+$(OBJDIR)\glog.obj &
+$(OBJDIR)\gmemdbg.obj &
+$(OBJDIR)\gmemutil.obj &
+$(OBJDIR)\gmsgattr.obj &
+$(OBJDIR)\ghdrmime.obj &
+$(OBJDIR)\gprnutil.obj &
+$(OBJDIR)\gsnd.obj &
+$(OBJDIR)\gsndwrap.obj &
+$(OBJDIR)\gstrctyp.obj &
+$(OBJDIR)\gstrmail.obj &
+$(OBJDIR)\gstrname.obj &
+$(OBJDIR)\gstrutil.obj &
+$(OBJDIR)\gtimjuld.obj &
+$(OBJDIR)\gtimutil.obj &
+$(OBJDIR)\gbmh.obj &
+$(OBJDIR)\gfuzzy.obj &
+$(OBJDIR)\gregex.obj &
+$(OBJDIR)\gwildmat.obj &
+$(OBJDIR)\gsearch.obj &
+$(OBJDIR)\gtxtpara.obj &
+$(OBJDIR)\gusrbase.obj &
+$(OBJDIR)\gusrezyc.obj &
+$(OBJDIR)\gusrgold.obj &
+$(OBJDIR)\gusrhuds.obj &
+$(OBJDIR)\gusrmax.obj &
+$(OBJDIR)\gusrpcb.obj &
+$(OBJDIR)\gusrra2.obj &
+$(OBJDIR)\gusrxbbs.obj &
+$(OBJDIR)\gutlclip.obj &
+$(OBJDIR)\gutlcode.obj &
+$(OBJDIR)\gutlgrp.obj &
+$(OBJDIR)\gutlmisc.obj &
+$(OBJDIR)\gutlmtsk.obj &
+$(OBJDIR)\gutltag.obj &
+$(OBJDIR)\gutlvers.obj &
+$(OBJDIR)\gcharset.obj &
+$(OBJDIR)\gutf8.obj &
+$(OBJDIR)\grecode.obj &
+$(OBJDIR)\giniprsr.obj &
+$(OBJDIR)\gutldos.obj &
+$(OBJDIR)\gutlbcx.obj &
+$(OBJDIR)\gespell.obj
|
!else
$(LIBDIR)\gall.lib: $(GALL_OBJS)
	-@del $(LIBDIR)\gall.lib
	$(LIBR) $(LIBPAGE) $(LIBDIR)\gall.lib @&&|
+$(OBJDIR)\gcrc16tb.obj &
+$(OBJDIR)\gcrc32tb.obj &
+$(OBJDIR)\gcrchash.obj &
+$(OBJDIR)\gcrckeyv.obj &
+$(OBJDIR)\gcrcm16.obj &
+$(OBJDIR)\gcrcm32.obj &
+$(OBJDIR)\gcrcs16.obj &
+$(OBJDIR)\gcrcs32.obj &
+$(OBJDIR)\gdbgerr.obj &
+$(OBJDIR)\gdbgexit.obj &
+$(OBJDIR)\gdbgtrk.obj &
+$(OBJDIR)\gdirposx.obj &
+$(OBJDIR)\geval.obj &
+$(OBJDIR)\gevalhum.obj &
+$(OBJDIR)\gevalrpn.obj &
+$(OBJDIR)\gfile.obj &
+$(OBJDIR)\gfilport.obj &
+$(OBJDIR)\gfilutl1.obj &
+$(OBJDIR)\gfilutl2.obj &
+$(OBJDIR)\gftnaddr.obj &
+$(OBJDIR)\gftnnl.obj &
+$(OBJDIR)\gftnnlfd.obj &
+$(OBJDIR)\gftnnlfu.obj &
+$(OBJDIR)\gftnnlge.obj &
+$(OBJDIR)\gftnnlv7.obj &
+$(OBJDIR)\glog.obj &
+$(OBJDIR)\gmemdbg.obj &
+$(OBJDIR)\gmemutil.obj &
+$(OBJDIR)\gmsgattr.obj &
+$(OBJDIR)\ghdrmime.obj &
+$(OBJDIR)\gprnutil.obj &
+$(OBJDIR)\gsnd.obj &
+$(OBJDIR)\gsndwrap.obj &
+$(OBJDIR)\gstrctyp.obj &
+$(OBJDIR)\gstrmail.obj &
+$(OBJDIR)\gstrname.obj &
+$(OBJDIR)\gstrutil.obj &
+$(OBJDIR)\gtimjuld.obj &
+$(OBJDIR)\gtimutil.obj &
+$(OBJDIR)\gbmh.obj &
+$(OBJDIR)\gfuzzy.obj &
+$(OBJDIR)\gregex.obj &
+$(OBJDIR)\gwildmat.obj &
+$(OBJDIR)\gsearch.obj &
+$(OBJDIR)\gtxtpara.obj &
+$(OBJDIR)\gusrbase.obj &
+$(OBJDIR)\gusrezyc.obj &
+$(OBJDIR)\gusrgold.obj &
+$(OBJDIR)\gusrhuds.obj &
+$(OBJDIR)\gusrmax.obj &
+$(OBJDIR)\gusrpcb.obj &
+$(OBJDIR)\gusrra2.obj &
+$(OBJDIR)\gusrxbbs.obj &
+$(OBJDIR)\gutlclip.obj &
+$(OBJDIR)\gutlcode.obj &
+$(OBJDIR)\gutlgrp.obj &
+$(OBJDIR)\gutlmisc.obj &
+$(OBJDIR)\gutlmtsk.obj &
+$(OBJDIR)\gutltag.obj &
+$(OBJDIR)\gutlvers.obj &
+$(OBJDIR)\gcharset.obj &
+$(OBJDIR)\gutf8.obj &
+$(OBJDIR)\grecode.obj &
+$(OBJDIR)\giniprsr.obj &
+$(OBJDIR)\gutlwin.obj &
+$(OBJDIR)\gutlwinm.obj &
+$(OBJDIR)\gespell.obj
|
!endif

$(LIBDIR)\gcui.lib: $(GCUI_OBJS)
	-@del $(LIBDIR)\gcui.lib
	$(LIBR) $(LIBPAGE) $(LIBDIR)\gcui.lib @&&|
+$(OBJDIR)\gkbdbase.obj &
+$(OBJDIR)\gkbdgetm.obj &
+$(OBJDIR)\gkbdwait.obj &
+$(OBJDIR)\gsrchmgr.obj &
+$(OBJDIR)\gmoubase.obj &
+$(OBJDIR)\gvidbase.obj &
+$(OBJDIR)\gvidinit.obj &
+$(OBJDIR)\gwinbase.obj &
+$(OBJDIR)\gwindow.obj &
+$(OBJDIR)\gwinhlp1.obj &
+$(OBJDIR)\gwinhlp2.obj &
+$(OBJDIR)\gwininit.obj &
+$(OBJDIR)\gwinline.obj &
+$(OBJDIR)\gwinmenu.obj &
+$(OBJDIR)\gwinmnub.obj &
+$(OBJDIR)\gwinpckf.obj &
+$(OBJDIR)\gwinpcks.obj &
+$(OBJDIR)\gwinpick.obj &
+$(OBJDIR)\gwinput2.obj
|

$(LIBDIR)\gcfg.lib: $(GCFG_OBJS)
	-@del $(LIBDIR)\gcfg.lib
	$(LIBR) $(LIBPAGE) $(LIBDIR)\gcfg.lib @&&|
+$(OBJDIR)\gedacfg.obj &
+$(OBJDIR)\gxareas.obj &
+$(OBJDIR)\gxcrash.obj &
+$(OBJDIR)\gxdb.obj &
+$(OBJDIR)\gxdutch.obj &
+$(OBJDIR)\gxezy102.obj &
+$(OBJDIR)\gxezy110.obj &
+$(OBJDIR)\gxfd.obj &
+$(OBJDIR)\gxfecho4.obj &
+$(OBJDIR)\gxfecho5.obj &
+$(OBJDIR)\gxfecho6.obj &
+$(OBJDIR)\gxfidpcb.obj &
+$(OBJDIR)\gxfm092.obj &
+$(OBJDIR)\gxfm100.obj &
+$(OBJDIR)\gxfm116.obj &
+$(OBJDIR)\gxgecho.obj &
+$(OBJDIR)\gxhpt.obj &
+$(OBJDIR)\gximail4.obj &
+$(OBJDIR)\gximail5.obj &
+$(OBJDIR)\gximail6.obj &
+$(OBJDIR)\gxinter.obj &
+$(OBJDIR)\gxlora.obj &
+$(OBJDIR)\gxmax3.obj &
+$(OBJDIR)\gxme2.obj &
+$(OBJDIR)\gxopus.obj &
+$(OBJDIR)\gxpcb.obj &
+$(OBJDIR)\gxportal.obj &
+$(OBJDIR)\gxprobrd.obj &
+$(OBJDIR)\gxqfront.obj &
+$(OBJDIR)\gxqecho.obj &
+$(OBJDIR)\gxquick.obj &
+$(OBJDIR)\gxra.obj &
+$(OBJDIR)\gxraecho.obj &
+$(OBJDIR)\gxspace.obj &
+$(OBJDIR)\gxsquish.obj &
+$(OBJDIR)\gxsuper.obj &
+$(OBJDIR)\gxsync.obj &
+$(OBJDIR)\gxtimed.obj &
+$(OBJDIR)\gxtmail.obj &
+$(OBJDIR)\gxts.obj &
+$(OBJDIR)\gxwmail.obj &
+$(OBJDIR)\gxwtr.obj &
+$(OBJDIR)\gxxbbs.obj &
+$(OBJDIR)\gxxmail.obj
|

$(LIBDIR)\gmb3.lib: $(GMB3_OBJS)
	-@del $(LIBDIR)\gmb3.lib
	$(LIBR) $(LIBPAGE) $(LIBDIR)\gmb3.lib @&&|
+$(OBJDIR)\gmoarea.obj &
+$(OBJDIR)\gmohuds.obj &
+$(OBJDIR)\gmoezyc1.obj &
+$(OBJDIR)\gmoezyc2.obj &
+$(OBJDIR)\gmoezyc3.obj &
+$(OBJDIR)\gmoezyc4.obj &
+$(OBJDIR)\gmoezyc5.obj &
+$(OBJDIR)\gmofido1.obj &
+$(OBJDIR)\gmofido2.obj &
+$(OBJDIR)\gmofido3.obj &
+$(OBJDIR)\gmofido4.obj &
+$(OBJDIR)\gmofido5.obj &
+$(OBJDIR)\gmojamm1.obj &
+$(OBJDIR)\gmojamm2.obj &
+$(OBJDIR)\gmojamm3.obj &
+$(OBJDIR)\gmojamm4.obj &
+$(OBJDIR)\gmojamm5.obj &
+$(OBJDIR)\gmopcbd1.obj &
+$(OBJDIR)\gmopcbd2.obj &
+$(OBJDIR)\gmopcbd3.obj &
+$(OBJDIR)\gmopcbd4.obj &
+$(OBJDIR)\gmopcbd5.obj &
+$(OBJDIR)\gmosmb1.obj &
+$(OBJDIR)\gmosmb2.obj &
+$(OBJDIR)\gmosqsh1.obj &
+$(OBJDIR)\gmosqsh2.obj &
+$(OBJDIR)\gmosqsh3.obj &
+$(OBJDIR)\gmosqsh4.obj &
+$(OBJDIR)\gmosqsh5.obj &
+$(OBJDIR)\gmowcat1.obj &
+$(OBJDIR)\gmowcat2.obj &
+$(OBJDIR)\gmowcat3.obj &
+$(OBJDIR)\gmowcat4.obj &
+$(OBJDIR)\gmowcat5.obj &
+$(OBJDIR)\gmoxbbs1.obj &
+$(OBJDIR)\gmoxbbs2.obj &
+$(OBJDIR)\gmoxbbs3.obj &
+$(OBJDIR)\gmoxbbs4.obj &
+$(OBJDIR)\gmoxbbs5.obj
|

$(LIBDIR)\uulib.lib: $(UULIB_OBJS)
	-@del $(LIBDIR)\uulib.lib
	$(LIBR) $(LIBPAGE) $(LIBDIR)\uulib.lib @&&|
+$(OBJDIR)\fptools.obj &
+$(OBJDIR)\uucheck.obj &
+$(OBJDIR)\uuencode.obj &
+$(OBJDIR)\uulib.obj &
+$(OBJDIR)\uunconc.obj &
+$(OBJDIR)\uuscan.obj &
+$(OBJDIR)\uustring.obj &
+$(OBJDIR)\uuutil.obj
|

$(LIBDIR)\smblib.lib: $(SMBLIB_OBJS)
	-@del $(LIBDIR)\smblib.lib
	$(LIBR) $(LIBPAGE) $(LIBDIR)\smblib.lib @&&|
+$(OBJDIR)\lzh.obj &
+$(OBJDIR)\smblib.obj
|

$(LIBDIR)\glibc.lib: $(GLIBC_OBJS)
	-@del $(LIBDIR)\glibc.lib
	$(LIBR) $(LIBPAGE) $(LIBDIR)\glibc.lib @&&|
+$(OBJDIR)\regex.obj &
+$(OBJDIR)\dummy.obj
|

$(GED): $(GOLDED3_OBJS) $(LIBS)
	$(CC) $(LNKOPT) -e$(GED) @&&|
$(GOLDED3_OBJS)
$(LIBS)
|

#  goldnode and rddt each need golded3.obj for the version banner.
$(GN): $(GOLDNODE_OBJS) $(OBJDIR)\golded3.obj $(LIBS)
	$(CC) $(LNKOPT) -e$(GN) @&&|
$(GOLDNODE_OBJS) $(OBJDIR)\golded3.obj
$(LIBS)
|

$(RD): $(RDDT_OBJS) $(OBJDIR)\golded3.obj $(LIBS)
	$(CC) $(LNKOPT) -e$(RD) @&&|
$(RDDT_OBJS) $(OBJDIR)\golded3.obj
$(LIBS)
|

clean:
	@-del $(OBJDIR)\*.obj
	@-del $(LIBDIR)\*.lib
	@-del $(GED)
	@-del $(GN)
	@-del $(RD)
