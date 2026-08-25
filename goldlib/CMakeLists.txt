add_subdirectory(hunspell)
add_subdirectory(gall)
add_subdirectory(gcfg)
add_subdirectory(gcui)
add_subdirectory(smblib)
add_subdirectory(gmb3)
add_subdirectory(uulib)
if (WIN32)
  #  The POSIX regex functions are missing from every Windows C runtime,
  #  mingw's as much as MSVC's, so this replacement is built for the
  #  native Windows targets. Cygwin has them and is not WIN32 here.
  add_subdirectory(glibc)
endif()
