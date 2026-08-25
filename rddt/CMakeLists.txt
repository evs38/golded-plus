set(rddt_SOURCES
  rddt.cpp
  ../golded3/golded3.cpp
)
add_executable(rddt ${rddt_SOURCES})
target_link_libraries(rddt gall)
if (GOLD_ICONV_LIBS)
  target_link_libraries(rddt ${GOLD_ICONV_LIBS})
endif()
include_directories(../goldlib/gall ../golded3)

INSTALL(TARGETS rddt
  RUNTIME DESTINATION bin
)
