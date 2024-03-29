cmake_minimum_required (VERSION 3.2)
project (vdlm2dec C)

add_compile_options(-Ofast -march=native )

add_executable(vdlm2dec cJSON.c  crc.c  d8psk.c  label.c  main.c  outacars.c  arincpos.c out.c  outxid.c  rs.c  vdlm2.c  viterbi.c )

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
pkg_check_modules(LIBACARS libacars-2>=2.0.0)
if(LIBACARS_FOUND)
message ( STATUS "Using libacars")
add_definitions(-DHAVE_LIBACARS )
target_sources(vdlm2dec PRIVATE arincpos.c )
target_link_libraries(vdlm2dec ${LIBACARS_LIBRARIES})
target_include_directories(vdlm2dec PUBLIC ${LIBACARS_INCLUDE_DIRS})
link_directories(${LIBACARS_LIBRARY_DIRS})
else()
message ( STATUS "Not using libacars")
endif()
endif()

option(rtl "Compiling for rtl sdr" )
if(rtl)
find_library(LIBRTL rtlsdr)
if(NOT LIBRTL)
message (FATAL_ERROR "librtlsdr path not found")
endif()
add_definitions(-DWITH_RTL )
target_sources( vdlm2dec PRIVATE rtl.c)
target_link_libraries( vdlm2dec ${LIBRTL})
endif()

option(airspy "Compiling for airspy sdr" )
if(airspy)
find_library(LIBAIR airspy)
if(NOT LIBAIR)
message ( FATAL_ERROR "libairspy path not found")
endif()
add_definitions(-DWITH_AIR )
target_sources( vdlm2dec PRIVATE air.c)
target_link_libraries( vdlm2dec ${LIBAIR})
endif()

if(NOT rtl AND NOT airspy )
message ("No sdr option set ! are you sure ?")
endif()

target_link_libraries( vdlm2dec pthread m )

install(TARGETS vdlm2dec
	RUNTIME DESTINATION bin
)
