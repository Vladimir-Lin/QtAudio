cmake_minimum_required(VERSION 2.8)
message("Compile CIOS Audio Player on Windows/x86")
project(CaPlay)
INCLUDE (CheckIncludeFile)

set(CMAKE_BUILD_TYPE Release)

add_definitions(-DCAC_NAMESPACE=CiosAudio)
add_definitions(-DCAUTILITIES)
add_definitions(-DFFMPEGLIB)

include_directories(${CMAKE_SOURCE_DIR}/../../../../)
include_directories(${CMAKE_SOURCE_DIR}/../../../../Platforms/Windows/FFmpeg/include)

link_directories(${CMAKE_SOURCE_DIR}/../../../../Platforms/Windows/FFmpeg/lib/x86/)
link_directories(${CMAKE_SOURCE_DIR}/../../../Windows/x86/)

set(CAHEADERS ../../../../CiosAudio.hpp)
set(CAPLAY ../../CaPlay.cpp)
set(CALIBS
    CaCore
    avcodec
    avdevice
    avfilter
    avformat
    avutil
    postproc
    swresample
    swscale
    winmm
    user32
    wsock32
    Advapi32
    dsound)

add_executable(CaPlay ${CAPLAY} ${CAHEADERS})
target_link_libraries(CaPlay ${CALIBS})

install(TARGETS CaPlay DESTINATION Bin)
