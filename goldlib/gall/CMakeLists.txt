set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(gall_SOURCES
  gbmh.cpp
  gcharset.cpp
  gcrc16tb.cpp
  gcrc32tb.cpp
  gcrchash.cpp
  gcrckeyv.cpp
  gcrcm16.cpp
  gcrcm32.cpp
  gcrcs16.cpp
  gcrcs32.cpp
  gdbgerr.cpp
  gdbgexit.cpp
  gdbgtrk.cpp
  gdirposx.cpp
  gespell.cpp
  geval.cpp
  gevalhum.cpp
  gevalrpn.cpp
  gfile.cpp
  gfilport.cpp
  gfilutl1.cpp
  gfilutl2.cpp
  gftnaddr.cpp
  gftnnl.cpp
  gftnnlfd.cpp
  gftnnlfu.cpp
  gftnnlge.cpp
  gftnnlv7.cpp
  gfuzzy.cpp
  ghdrmime.cpp
  giniprsr.cpp
  glog.cpp
  gmemdbg.cpp
  gmemutil.cpp
  gmsgattr.cpp
  gprnutil.cpp
  grecode.cpp
  gregex.cpp
  gsearch.cpp
  gsnd.cpp
  gsndwrap.cpp
  gstrctyp.cpp
  gstrmail.cpp
  gstrname.cpp
  gstrutil.cpp
  gtimjuld.cpp
  gtimutil.cpp
  gtxtpara.cpp
  gutf8.cpp
  gusrbase.cpp
  gusrezyc.cpp
  gusrgold.cpp
  gusrhuds.cpp
  gusrmax.cpp
  gusrpcb.cpp
  gusrra2.cpp
  gusrxbbs.cpp
  gutlclip.cpp
  gutlcode.cpp
  gutlgrp.cpp
  gutlmisc.cpp
  gutlmtsk.cpp
  gutltag.cpp
  gutlvers.cpp
  gwildmat.cpp
)
if (UNIX)
  list(APPEND gall_SOURCES
    gkbdunix.cpp
    gsigunix.cpp
    gutlunix.cpp
  )
elseif (WIN32)
  #  WIN32, not MSVC: mingw builds a native Windows binary too, and it
  #  needs the same g_init_os()/clipboard/sound layer. Cygwin is not
  #  WIN32 for CMake, so it keeps taking the UNIX branch above.
  list(APPEND gall_SOURCES
    gutlwin.cpp
    gutlwinm.cpp
  )
endif()
ADD_LIBRARY(gall STATIC ${gall_SOURCES})
#  BEFORE, because gespell.cpp includes the bundled hunspell as
#  <hunspell/hunspell.hxx> and that is resolved against this directory's
#  parent. A system with hunspell of its own installed - FreeBSD ports
#  put one in /usr/local/include/hunspell - would otherwise supply the
#  header while we link the copy built here, which is a version mismatch
#  waiting to misbehave at run time.
include_directories(BEFORE .. ../glibc)
target_link_libraries(gall hunspell)
if (GOLD_ICONV_LIBS)
  target_link_libraries(gall ${GOLD_ICONV_LIBS})
endif()