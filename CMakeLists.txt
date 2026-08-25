CMAKE_MINIMUM_REQUIRED(VERSION 2.8...3.19)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

project(golded-plus C CXX)

#  The Windows builds compile a resource script. Declaring RC here lets
#  CMake set up the right tool for the compiler in use - rc.exe under
#  MSVC, windres under mingw - instead of handing one of them the
#  other's command line.
if (WIN32)
  enable_language(RC)
endif()

#  What turns on the wide-character curses API.
#
#  Everywhere else that is _XOPEN_SOURCE_EXTENDED, the X/Open macro. Not
#  on Solaris: there it selects a strict X/Open namespace, and one of the
#  things that namespace leaves out is the wide-character part of the C
#  library, so <cwchar> stops declaring btowc() and half of its
#  neighbours and nothing builds at all. ncurses has its own switch for
#  the same purpose, and Solaris ships ncurses.
if (CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  set(GOLD_WIDE_DEFS -DNCURSES_WIDECHAR=1)
else()
  set(GOLD_WIDE_DEFS -D_XOPEN_SOURCE_EXTENDED)
endif()



include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckCXXSourceCompiles)

#  wcwidth() is how we ask the C library whether it thinks a character
#  takes two columns. It is XSI, not ISO C, and Cygwin/MSYS hide it unless
#  the right feature-test macro is set - so check rather than assume.
check_symbol_exists(wcwidth "wchar.h" GOLD_HAVE_WCWIDTH)
if (GOLD_HAVE_WCWIDTH)
  add_definitions(-DGOLD_HAVE_WCWIDTH=1)
endif()

CHECK_SYMBOL_EXISTS(snprintf stdio.h HAVE_SNPRINTF)
if (${HAVE_SNPRINTF})
  add_definitions(-DHAVE_SNPRINTF=1)
endif()
CHECK_SYMBOL_EXISTS(vsnprintf stdio.h HAVE_VSNPRINTF)
if (${HAVE_VSNPRINTF})
  add_definitions(-DHAVE_VSNPRINTF=1)
endif()
CHECK_INCLUDE_FILE("stdarg.h" HAVE_STDARG_H)
if (${HAVE_STDARG_H})
  add_definitions(-DHAVE_STDARG_H=1)
endif()
CHECK_INCLUDE_FILE("string.h" HAVE_STRING_H)
if (${HAVE_STRING_H})
  add_definitions(-DHAVE_STRING_H=1)
endif()
CHECK_INCLUDE_FILE("malloc.h" HAVE_MALLOC_H)
if (${HAVE_MALLOC_H})
  add_definitions(-DHAVE_MALLOC_H=1)
endif()
CHECK_INCLUDE_FILE("time.h" HAVE_TIME_H)
if (${HAVE_TIME_H})
  add_definitions(-DHAVE_TIME_H=1)
endif()
CHECK_INCLUDE_FILE("errno.h" HAVE_ERRNO_H)
if (${HAVE_ERRNO_H})
  add_definitions(-DHAVE_ERRNO_H=1)
endif()
CHECK_INCLUDE_FILE("fcntl.h" HAVE_FCNTL_H)
if (${HAVE_FCNTL_H})
  add_definitions(-DHAVE_FCNTL_H=1)
endif()
CHECK_INCLUDE_FILE("io.h" HAVE_IO_H)
if (${HAVE_IO_H})
  add_definitions(-DHAVE_IO_H=1)
endif()
CHECK_INCLUDE_FILE("unistd.h" HAVE_UNISTD_H)
if (${HAVE_UNISTD_H})
  add_definitions(-DHAVE_UNISTD_H=1)
endif()
CHECK_INCLUDE_FILES("stdarg.h;stdlib.h;stddef.h;string.h" HAVE_STDC_HEADERS)
if (${HAVE_STDC_HEADERS})
  add_definitions(-DSTDC_HEADERS=1)
endif()
check_include_files("time.h;sys/time.h" TIME_WITH_SYS_TIME)
if (${TIME_WITH_SYS_TIME})
  add_definitions(-DTIME_WITH_SYS_TIME=1)
endif()
#  ------------------------------------------------------------------
#  ------------------------------------------------------------------
#  Curses.
#
#  Two things vary between systems: which library carries the
#  wide-character API - the one that makes UTF-8 output possible - and
#  where its header lives.
#
#    Linux           libncursesw   <ncursesw/curses.h> or <curses.h>
#    NetBSD pkgsrc   libncurses    <ncurses/curses.h>   (no ncursesw any more)
#    NetBSD base     libcurses     <curses.h>
#    macOS           libncurses    <curses.h>
#
#  Both NetBSD flavours can do wide characters, so either will build.
#  Rather than guess, each candidate library is paired with a header
#  from its own directory and the pair is checked by actually compiling
#  and linking against it: pairing one implementation's header with
#  another's library is the kind of mismatch that only shows up later.

option(WIDE_NCURSES "Use the wide-character (UTF-8 capable) curses API" ON)
option(GOLD_UTF8 "Build GoldED+ with an UTF-8 internal representation" ON)
set(GOLD_CURSES "auto" CACHE STRING
    "Curses flavour to build against: auto, ncursesw, ncurses, curses or none")

#  ------------------------------------------------------------------
#  Where curses and iconv come from
#
#  On unix the package manager's copy comes first - pkgsrc, ports,
#  Homebrew, MacPorts, OpenCSW - and the base system's is the fallback.
#  A packaged ncurses is newer and wide-capable where a base curses often
#  is not, and a packaged libiconv knows charsets the C library's does
#  not. Where a system has only its own, that is what gets used, and only
#  a system with neither falls back to the built-in tables.
#
#  The rule is the same everywhere, with no toolchain singled out: look,
#  and use what is found. Where nothing is found the platform's own way
#  catches the build - the Win32 console and codepage API on Windows,
#  ULS and VIO on OS/2, and only on DOS the compiled-in charset tables.
#  MSVC normally has neither library installed, so it finds neither and
#  lands on the native path it would have taken anyway; Cygwin and MSYS2
#  usually have both, and there they are the better answer.
#
#  Whichever side is chosen, header and library must come from the same
#  one. Several of these systems ship curses and iconv twice and the two
#  copies are not interchangeable: GNU libiconv renames iconv_open() to
#  libiconv_open() in its header, so its header paired with the C
#  library's iconv links to nothing, and a pkgsrc ncurses header paired
#  with the base libcurses compiles and then misbehaves at runtime. The
#  search below pairs them by prefix for exactly that reason.
option(GOLD_EXTERNAL_CURSES
       "Search package prefixes (/usr/pkg, /usr/local, ...) for curses before the system's own" ON)
option(GOLD_EXTERNAL_ICONV
       "Search package prefixes for GNU libiconv before the iconv the system provides" ON)

#  Prefixes that belong to a package manager rather than to the OS.
set(GOLD_PACKAGE_PREFIXES /usr/pkg /usr/local /opt/local /opt/homebrew
                          /opt/csw /usr/gnu /usr/sfw)

#  MSYS2 has two library trees and only one of them belongs to a mingw
#  build. /mingw64 is the native Win32 side; the unix-looking /usr is
#  the MSYS runtime - its libraries pull in msys-2.0.dll and its headers
#  are a different C library altogether. Left in the search, curses is
#  found there and `C:/msys64/usr/include' lands on the compile line
#  ahead of mingw64's own, which does not merely produce a hybrid
#  binary: the build stops in gdefs.h. So the package prefix here is
#  MSYSTEM's own, and the MSYS tree is kept out.
if (MINGW)
  set(GOLD_PACKAGE_PREFIXES "")

  #  MINGW_PREFIX is not always in the environment - an ssh session runs
  #  no login shell - so the compiler's own location is the reliable
  #  answer: C:/msys64/mingw64/bin/gcc.exe gives both the mingw prefix
  #  and, one level up again, the MSYS root to keep out.
  set(_gold_mingw_prefix "$ENV{MINGW_PREFIX}")
  if (NOT _gold_mingw_prefix AND CMAKE_C_COMPILER)
    get_filename_component(_gold_mingw_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    get_filename_component(_gold_mingw_prefix "${_gold_mingw_bin}" DIRECTORY)
  endif()

  if (_gold_mingw_prefix)
    list(APPEND GOLD_PACKAGE_PREFIXES "${_gold_mingw_prefix}")
    get_filename_component(_gold_msys_root "${_gold_mingw_prefix}" DIRECTORY)
    #  Spelled out natively: MSYS2's cmake resolves /usr to
    #  C:/msys64/usr, and an ignore list of "/usr" never matches it.
    list(APPEND CMAKE_IGNORE_PREFIX_PATH
         "${_gold_msys_root}/usr" "${_gold_msys_root}/usr/local")
    list(APPEND CMAKE_IGNORE_PATH
         "${_gold_msys_root}/usr/lib" "${_gold_msys_root}/usr/include"
         "${_gold_msys_root}/usr/local/lib" "${_gold_msys_root}/usr/local/include")
    message(STATUS "MinGW: package prefix ${_gold_mingw_prefix}, "
                   "MSYS tree under ${_gold_msys_root}/usr kept out")
  endif()
endif()

#  Take those out of the search path for the duration of a lookup, so
#  find_library()/find_path() can only see what the system itself has.
macro(gold_system_paths_only)
  set(_gold_saved_system_prefix ${CMAKE_SYSTEM_PREFIX_PATH})
  set(_gold_saved_prefix ${CMAKE_PREFIX_PATH})
  if (GOLD_PACKAGE_PREFIXES)
    list(REMOVE_ITEM CMAKE_SYSTEM_PREFIX_PATH ${GOLD_PACKAGE_PREFIXES})
    list(REMOVE_ITEM CMAKE_PREFIX_PATH ${GOLD_PACKAGE_PREFIXES})
  endif()
endmacro()

#  Put them at the front instead, so a package copy is preferred and the
#  system's own is still reachable behind it.
macro(gold_package_paths_first)
  set(_gold_saved_system_prefix ${CMAKE_SYSTEM_PREFIX_PATH})
  set(_gold_saved_prefix ${CMAKE_PREFIX_PATH})
  if (GOLD_PACKAGE_PREFIXES)
    list(INSERT CMAKE_PREFIX_PATH 0 ${GOLD_PACKAGE_PREFIXES})
  endif()
endmacro()

macro(gold_restore_paths)
  set(CMAKE_SYSTEM_PREFIX_PATH ${_gold_saved_system_prefix})
  set(CMAKE_PREFIX_PATH ${_gold_saved_prefix})
endmacro()

if (GOLD_EXTERNAL_CURSES)
  message(STATUS "Curses: package prefixes searched first, system's own behind them")
  gold_package_paths_first()
else()
  gold_system_paths_only()
endif()

#  A native Windows build draws through the Win32 console layer in
#  gvidbase.cpp, which speaks wide characters directly and needs no
#  library at all - so a Windows build that finds no curses loses
#  nothing and lands there.
#
#  No toolchain is singled out. MSYS2 and Cygwin both ship an ncurses
#  and where one is present it is the better screen; MSVC normally has
#  none, finds none, and ends up on the Win32 layer it would have used
#  anyway. GOLD_CURSES=none turns the search off by hand.
if (GOLD_CURSES STREQUAL "none")
  set(_gold_cands "")
elseif (GOLD_CURSES STREQUAL "auto")
  set(_gold_cands ncursesw ncurses curses)
else()
  set(_gold_cands ${GOLD_CURSES})
endif()

set(GOLD_CURSES_LIB "")
set(GOLD_CURSES_INC "")
set(GOLD_CURSES_WIDE FALSE)

foreach(_cand ${_gold_cands})
  if (GOLD_CURSES_LIB)
    break()
  endif()

  unset(_cand_lib CACHE)
  unset(_cand_inc CACHE)

  find_library(_cand_lib NAMES ${_cand})
  if (NOT _cand_lib)
    continue()
  endif()

  #  Take the header from the same prefix as the library. Systems that
  #  ship curses twice - FreeBSD has ncurses 6.5 in the base and 6.6 in
  #  ports, both under names like libncursesw - will otherwise hand out
  #  one version's header and the other's library, which links but is
  #  not the same thing at all.
  get_filename_component(_cand_libdir "${_cand_lib}" DIRECTORY)
  get_filename_component(_cand_prefix "${_cand_libdir}" DIRECTORY)

  find_path(_cand_inc NAMES curses.h
            HINTS ${_cand_prefix}/include
            PATH_SUFFIXES ${_cand} ncursesw ncurses ""
            NO_DEFAULT_PATH)

  if (NOT _cand_inc)
    #  Nothing under that prefix; fall back to the usual search.
    find_path(_cand_inc NAMES curses.h PATH_SUFFIXES ${_cand} "")
  endif()

  if (NOT _cand_inc)
    continue()
  endif()

  #  A header sitting in a subdirectory may include its siblings by that
  #  qualified path - ncurses opens with
  #  "#include <ncurses/ncurses_dll.h>" - so the parent has to be
  #  reachable as well as the subdirectory itself. The subdirectory is
  #  not always named after the library that was found: FreeBSD ports
  #  put the headers for both libncurses and libncursesw under
  #  include/ncurses.
  set(_cand_incs ${_cand_inc})
  get_filename_component(_cand_leaf "${_cand_inc}" NAME)
  if (NOT _cand_leaf STREQUAL "include")
    get_filename_component(_cand_parent "${_cand_inc}" DIRECTORY)
    list(APPEND _cand_incs ${_cand_parent})
  endif()

  set(CMAKE_REQUIRED_INCLUDES ${_cand_incs})
  set(CMAKE_REQUIRED_LIBRARIES ${_cand_lib})
  set(CMAKE_REQUIRED_DEFINITIONS ${GOLD_WIDE_DEFS})
  check_cxx_source_compiles("
#include <curses.h>
int main(void)
{
    cchar_t c;
    wchar_t w[2] = { 'x', 0 };
    wint_t k;
    setcchar(&c, w, 0, 0, 0);
    add_wch(&c);
    get_wch(&k);
    return 0;
}
" GOLD_WIDE_${_cand})
  unset(CMAKE_REQUIRED_INCLUDES)
  unset(CMAKE_REQUIRED_LIBRARIES)
  unset(CMAKE_REQUIRED_DEFINITIONS)

  if (GOLD_WIDE_${_cand})
    set(GOLD_CURSES_LIB ${_cand_lib})
    set(GOLD_CURSES_INC ${_cand_incs})
    set(GOLD_CURSES_WIDE TRUE)
  else()
    set(CMAKE_REQUIRED_INCLUDES ${_cand_incs})
    set(CMAKE_REQUIRED_LIBRARIES ${_cand_lib})
    check_cxx_source_compiles("
#include <curses.h>
int main(void) { initscr(); return 0; }
" GOLD_PLAIN_${_cand})
    unset(CMAKE_REQUIRED_INCLUDES)
    unset(CMAKE_REQUIRED_LIBRARIES)
    if (GOLD_PLAIN_${_cand})
      set(GOLD_CURSES_LIB ${_cand_lib})
      set(GOLD_CURSES_INC ${_cand_incs})
    endif()
  endif()
endforeach()

unset(_cand_lib CACHE)
unset(_cand_inc CACHE)

gold_restore_paths()

if (GOLD_CURSES_LIB)
  message(STATUS "Curses: ${GOLD_CURSES_LIB} (headers in ${GOLD_CURSES_INC})")
  add_definitions(-D__USE_NCURSES__)
  include_directories(${GOLD_CURSES_INC})
  set(CURSES_FOUND TRUE)
  set(CURSES_LIBRARIES ${GOLD_CURSES_LIB})
  set(CURSES_INCLUDE_DIRS ${GOLD_CURSES_INC})

  if (WIDE_NCURSES AND GOLD_CURSES_WIDE)
    #  The macro above exposes cchar_t, add_wch(), get_wch() and the
    #  rest of the wide API.
    add_definitions(${GOLD_WIDE_DEFS} -D__USE_WIDE_NCURSES__)
    message(STATUS "Curses: using the wide-character API")
  else()
    if (WIDE_NCURSES)
      message(WARNING "Wide curses requested but unavailable here; "
                      "falling back to the 8-bit curses API.")
    endif()
    set(WIDE_NCURSES OFF)
  endif()
else()
  set(CURSES_FOUND FALSE)
  if (WIN32)
    message(STATUS "curses: not used - the Win32 console layer handles the screen")
  else()
    message(WARNING "No usable curses found: falling back to raw terminal output.")
  endif()
endif()

#  ------------------------------------------------------------------
#  iconv. Used for all charset recoding; the built-in .chs tables are
#  only a fallback for platforms without it (DOS and the like).
#
#  The system's own iconv comes first - the C library's on unix, the
#  Win32 codepage API on Windows, neither of which needs a library on
#  the link line. GNU libiconv is looked for only when GOLD_EXTERNAL_ICONV
#  is on, or when the system turns out to have no iconv at all.
#
#  The reason for the ordering: GNU libiconv's header renames iconv_open()
#  to libiconv_open(). If its iconv.h is reachable - and it will be, as
#  soon as a package prefix joins the include path, which is what pkgsrc
#  ncurses does on NetBSD - then that is the header we compile against,
#  while the link line still names the C library's iconv, and nothing
#  resolves. Asking the system first, with no extra include directories,
#  cannot run into it.

set(_gold_iconv_test "
#include <iconv.h>
int main(void)
{
    iconv_t cd = iconv_open(\"UTF-8\", \"CP866\");
    if(cd != (iconv_t)-1)
        iconv_close(cd);
    return 0;
}
")

set(GOLD_ICONV_LIBS "")
set(GOLD_ICONV_WORKS FALSE)

#  Some platforms can recode without iconv at all: the recoder has a
#  Win32 codepage path and an OS/2 ULS path. Not finding an iconv there
#  is not a fall back to the built-in tables, and must not be reported
#  as one - it just means the operating system does the work.
if (WIN32 OR OS2)
  set(GOLD_ICONV_NATIVE_ONLY TRUE)
else()
  set(GOLD_ICONV_NATIVE_ONLY FALSE)
endif()

#  The search itself runs everywhere. Where an iconv is installed it is
#  the better answer - it knows charsets the Win32 API and the tables do
#  not - and where there is none, nothing is lost: MSVC in particular
#  normally has neither iconv nor curses, finds nothing, and ends up on
#  the native path it would have taken anyway.
if (NOT GOLD_EXTERNAL_ICONV)
  #  Probe against the include path the real compile will have. When
  #  curses came from a package prefix that prefix is on the include
  #  path too, and if it ships GNU libiconv then its iconv.h - which
  #  renames iconv_open() to libiconv_open() - is the header we will
  #  actually get. Testing without it would report that the C library's
  #  iconv is fine and then fail at link.
  set(CMAKE_REQUIRED_INCLUDES ${GOLD_CURSES_INC})
  check_cxx_source_compiles("${_gold_iconv_test}" GOLD_ICONV_IN_SYSTEM)
  unset(CMAKE_REQUIRED_INCLUDES)
  if (GOLD_ICONV_IN_SYSTEM)
    set(GOLD_ICONV_WORKS TRUE)
    message(STATUS "iconv: built in to the C library")
  else()
    #  Not in the C library. That does not mean there is none: macOS
    #  and the BSDs keep theirs in a separate libiconv that is still
    #  part of the system.
    message(STATUS "iconv: not in the C library - looking for a separate one")
  endif()
endif()

if (NOT GOLD_ICONV_WORKS)

  #  Look where the system keeps its own first. If that turns up nothing
  #  the search is repeated without the restriction: preferring the
  #  system's copy must not become refusing the only one there is.
  #  OpenBSD is that case - it has no iconv in base at all, and the port
  #  installs into /usr/local, which is a package prefix by the rule
  #  above.
  if (GOLD_EXTERNAL_ICONV)
    gold_package_paths_first()
  else()
    gold_system_paths_only()
  endif()

  #  Work out which iconv.h we would really see, find the library that
  #  belongs with it, and prove the pair by linking.
  set(_gold_iconv_prefix "")
  foreach(_dir ${GOLD_CURSES_INC})
    if (EXISTS "${_dir}/iconv.h")
      get_filename_component(_gold_iconv_prefix "${_dir}" DIRECTORY)
      break()
    endif()
  endforeach()

  if (_gold_iconv_prefix)
    set(Iconv_ROOT ${_gold_iconv_prefix})
  endif()

  find_package(Iconv QUIET)

  if (Iconv_FOUND AND NOT Iconv_IS_BUILT_IN)
    set(GOLD_ICONV_LIBS ${Iconv_LIBRARIES})
  endif()

  set(CMAKE_REQUIRED_INCLUDES ${GOLD_CURSES_INC} ${Iconv_INCLUDE_DIRS})
  set(CMAKE_REQUIRED_LIBRARIES ${GOLD_ICONV_LIBS})
  check_cxx_source_compiles("${_gold_iconv_test}" GOLD_ICONV_WORKS_LIB)
  unset(CMAKE_REQUIRED_INCLUDES)
  unset(CMAKE_REQUIRED_LIBRARIES)
  if (GOLD_ICONV_WORKS_LIB)
    set(GOLD_ICONV_WORKS TRUE)
  endif()

  if (NOT GOLD_ICONV_WORKS AND _gold_iconv_prefix)
    #  The header we see belongs to that prefix; link its library.
    find_library(GOLD_ICONV_LIB NAMES iconv
                 PATHS ${_gold_iconv_prefix}/lib NO_DEFAULT_PATH)
    if (GOLD_ICONV_LIB)
      set(CMAKE_REQUIRED_INCLUDES ${GOLD_CURSES_INC} ${Iconv_INCLUDE_DIRS})
      set(CMAKE_REQUIRED_LIBRARIES ${GOLD_ICONV_LIB})
      check_cxx_source_compiles("${_gold_iconv_test}" GOLD_ICONV_WORKS_PREFIXED)
      unset(CMAKE_REQUIRED_INCLUDES)
      unset(CMAKE_REQUIRED_LIBRARIES)
      if (GOLD_ICONV_WORKS_PREFIXED)
        set(GOLD_ICONV_LIBS ${GOLD_ICONV_LIB})
        set(GOLD_ICONV_WORKS TRUE)
      endif()
    endif()
  endif()

  gold_restore_paths()

  #  Second pass, with every prefix in play, only when the first found
  #  nothing at all.
  if (NOT GOLD_ICONV_WORKS AND NOT GOLD_EXTERNAL_ICONV)
    message(STATUS "iconv: none where the system keeps its own - widening the search")
    unset(Iconv_FOUND CACHE)
    unset(GOLD_ICONV_LIB CACHE)
    find_package(Iconv QUIET)
    if (Iconv_FOUND AND NOT Iconv_IS_BUILT_IN)
      set(GOLD_ICONV_LIBS ${Iconv_LIBRARIES})
    endif()
    set(CMAKE_REQUIRED_INCLUDES ${GOLD_CURSES_INC} ${Iconv_INCLUDE_DIRS})
    set(CMAKE_REQUIRED_LIBRARIES ${GOLD_ICONV_LIBS})
    check_cxx_source_compiles("${_gold_iconv_test}" GOLD_ICONV_WORKS_ANYWHERE)
    unset(CMAKE_REQUIRED_INCLUDES)
    unset(CMAKE_REQUIRED_LIBRARIES)
    if (GOLD_ICONV_WORKS_ANYWHERE)
      set(GOLD_ICONV_WORKS TRUE)
    endif()
  endif()

  if (GOLD_ICONV_WORKS AND Iconv_INCLUDE_DIRS)
    include_directories(${Iconv_INCLUDE_DIRS})
  endif()

endif()

if (GOLD_ICONV_WORKS)
  add_definitions(-DHAS_ICONV=1)
  if (GOLD_ICONV_LIBS)
    message(STATUS "iconv: ${GOLD_ICONV_LIBS}")
  endif()
elseif (NOT GOLD_ICONV_NATIVE_ONLY)
  set(GOLD_ICONV_LIBS "")
  message(WARNING "No usable iconv: falling back to the built-in charset tables.")
endif()

if (GOLD_UTF8)
  add_definitions(-DGOLD_UTF8=1)
endif()

if (MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /J /MP")
  add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS -D_ALLOW_RTCc_IN_STL  -D_CONSOLE -D__INCLUDE_NEW_KEYWORDS__)
  set(CompilerFlags
    CMAKE_CXX_FLAGS
    CMAKE_CXX_FLAGS_DEBUG
    CMAKE_CXX_FLAGS_RELEASE
    CMAKE_C_FLAGS
    CMAKE_C_FLAGS_DEBUG
    CMAKE_C_FLAGS_RELEASE
  )
  foreach(CompilerFlag ${CompilerFlags})
    string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
  endforeach()
else()
  #  The defines belong to every language, the warning and codegen
  #  switches only to C and C++. add_definitions() would hand the whole
  #  lot to the resource compiler as well, and windres rejects
  #  -funsigned-char with "invalid option -f", which breaks the mingw
  #  build. Putting them in the per-language flags keeps RC out of it and
  #  needs no generator expression, so it works with old CMake too.
  add_definitions(-D__INCLUDE_NEW_KEYWORDS__ -DPRAGMA_PACK)
  set(_gold_cc_flags "-funsigned-char -Wno-sign-compare -pedantic -Wno-write-strings")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_gold_cc_flags}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_gold_cc_flags}")
endif()

#  Tell the program where this copy was configured to be installed, so
#  it can look for golded.cfg in that tree's etc as a last resort.
#  GNUInstallDirs is what works sysconfdir out - /etc when the prefix is
#  /usr, <prefix>/etc otherwise, which is the convention a packager
#  expects. It has to be included here and not beside the install rules
#  below: add_definitions only reaches targets added after it.
include(GNUInstallDirs)
add_definitions(-DGOLD_SYSCONFDIR=\"${CMAKE_INSTALL_FULL_SYSCONFDIR}/golded/\")
message(STATUS "Config: last-resort directory ${CMAKE_INSTALL_FULL_SYSCONFDIR}/golded")

add_subdirectory(goldlib)
add_subdirectory(golded3)
add_subdirectory(goldnode)
add_subdirectory(rddt)

set (CPACK_STRIP_FILES TRUE)

include(GNUInstallDirs)
install(FILES
  docs/copying
  docs/copying.lib
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
install(FILES
  docs/golded.1
  docs/goldnode.1
  docs/rddt.1
  DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
)
install(DIRECTORY cfgs DESTINATION ${CMAKE_INSTALL_DOCDIR})
if (MSVC)
  set(CPACK_GENERATOR ${CPACK_GENERATOR} 7Z)
else()
  set(CPACK_GENERATOR ${CPACK_GENERATOR} TGZ)
endif()

set (CPACK_PACKAGE_VERSION_MAJOR "1")
set (CPACK_PACKAGE_VERSION_MINOR "1")
file(READ srcdate.h srcdate)
string(REPLACE "\n" ";" LINES ${srcdate})
list(GET LINES 1 LINETWO)
string(SUBSTRING ${LINETWO} 21 8 CPACK_PACKAGE_VERSION_PATCH)
set(CPACK_PACKAGE_VERSION_PATCH "5-${CPACK_PACKAGE_VERSION_PATCH}")
INCLUDE(CPack)
