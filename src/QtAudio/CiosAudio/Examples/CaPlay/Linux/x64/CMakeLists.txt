cmake_minimum_required(VERSION 2.8)
message("Compile CIOS Audio Player on Linux/x64")
project(CaPlay)
INCLUDE (CheckIncludeFile)

set(CMAKE_BUILD_TYPE Release)

add_definitions(-DCAC_NAMESPACE=CiosAudio)
add_definitions(-DCAUTILITIES)
add_definitions(-DFFMPEGLIB)

include_directories(${CMAKE_SOURCE_DIR}/../../../../)
include_directories(/usr/include)
include_directories(/usr/local/include)
include_directories(${CMAKE_SOURCE_DIR}/../../../../Platforms/Linux/FFmpeg/include)

link_directories(/usr/lib)
#link_directories(/usr/local/lib)
#link_directories(${CMAKE_SOURCE_DIR}/../../../../Platforms/Linux/FFmpeg/lib/x64/)

if(EXISTS "/etc/lsb-release")
execute_process(
    COMMAND cat /etc/lsb-release
    COMMAND grep DISTRIB_ID
    COMMAND awk -F= "{ print $2 }"
    COMMAND tr "\n" " "
    COMMAND sed "s/ //"
    OUTPUT_VARIABLE LSBID
    RESULT_VARIABLE LSBID_RESULT
)
else()
set (LSBID "Fedora")
endif()

if(NOT ${LSBID} STREQUAL "")

  if(${LSBID} STREQUAL "Fedora")
    option(EnableFEDORA "Fedora" ON)
    message("Fedora/x64 detected")
  else(${LSBID} STREQUAL "Fedora")
    option(EnableFEDORA "Fedora" OFF)
  endif(${LSBID} STREQUAL "Fedora")

  if(${LSBID} STREQUAL "Ubuntu")
    option(EnableUBUNTU "Ubuntu" ON)
    message("Ubuntu/x64 detected")
  else(${LSBID} STREQUAL "Ubuntu")
    option(EnableUBUNTU "Ubuntu" OFF)
  endif(${LSBID} STREQUAL "Ubuntu")

  if(${LSBID} STREQUAL "Debian")
    option(EnableDEBIAN "Debian" ON)
    message("Debian/x64 detected")
  else(${LSBID} STREQUAL "Debian")
    option(EnableDEBIAN "Debian" OFF)
  endif(${LSBID} STREQUAL "Debian")

else(NOT ${LSBID} STREQUAL "")

    option(EnableFEDORA "Fedora" OFF)
    option(EnableUBUNTU "Ubuntu" OFF)
    option(EnableDEBIAN "Debian" OFF)

endif(NOT ${LSBID} STREQUAL "")

set(CAHEADERS ../../../../CiosAudio.hpp)
set(CAPLAY ../../CaPlay.cpp)
set(CALIBS
    CaCore
    avcodec
    avformat
    avutil
    swresample
    swscale
    z
    bz2
    pthread
    dl)

if (EnableFEDORA)
include (Fedora.txt)
endif(EnableFEDORA)

if (EnableUBUNTU)
include (Ubuntu.txt)
endif(EnableUBUNTU)

if (EnableDEBIAN)
include (Debian.txt)
endif(EnableDEBIAN)

add_executable(CaPlay ${CAPLAY} ${CAHEADERS})
target_link_libraries(CaPlay ${CALIBS})

install(TARGETS CaPlay DESTINATION bin)
