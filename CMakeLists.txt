cmake_minimum_required (VERSION 3.2)
project (vdlm2dec C)

add_executable(vdlm2dec cJSON.c  crc.c  d8psk.c  label.c  main.c  outacars.c  out.c  outxid.c  rs.c  vdlm2.c  viterbi.c )

find_library(LIBACARS acars)
if(LIBACARS)
message ( STATUS "Using libacars")
add_definitions(-DHAVE_LIBACARS )
target_link_libraries( vdlm2dec acars)
else()
message ( STATUS "Not using libacars")
endif()

option(rtl "Compiling for rtl sdr" )
if(rtl)
find_library(LIBRTL rtl)
if(NOT LIBRTL)
message (FATAL_ERROR "librtl path not found")
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
