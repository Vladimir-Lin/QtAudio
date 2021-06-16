cmake_minimum_required(VERSION 2.8)
message("Compile CIOS Audio Player on Mac OS X/x64")
project(CaPlay)
INCLUDE (CheckIncludeFile)

set(CMAKE_BUILD_TYPE Release)

add_definitions(-DCAC_NAMESPACE=CiosAudio)
add_definitions(-DCAUTILITIES)
add_definitions(-DFFMPEGLIB)

macro(ADD_FRAMEWORK fwname appname)
    find_library(FRAMEWORK_${fwname}
    NAMES ${fwname}
    PATHS ${CMAKE_OSX_SYSROOT}/System/Library
    PATH_SUFFIXES Frameworks
    NO_DEFAULT_PATH)
    if( ${FRAMEWORK_${fwname}} STREQUAL FRAMEWORK_${fwname}-NOTFOUND)
        MESSAGE(ERROR ": Framework ${fwname} not found")
    else()
        TARGET_LINK_LIBRARIES(${appname} "${FRAMEWORK_${fwname}}/${fwname}")
        MESSAGE(STATUS "Framework ${fwname} found at ${FRAMEWORK_${fwname}}")
    endif()
endmacro(ADD_FRAMEWORK)

include_directories(${CMAKE_SOURCE_DIR}/../../../../)
include_directories(/usr/include)
include_directories(/usr/local/include)
include_directories(${CMAKE_SOURCE_DIR}/../../../../Platforms/MacOSX/FFmpeg/include)

link_directories(/usr/lib)
link_directories(/usr/local/lib)
link_directories(${CMAKE_SOURCE_DIR}/../../../../Platforms/MacOSX/FFmpeg/lib/x64/)

set(CAHEADERS ../../../../CiosAudio.hpp)
set(CAPLAY ../../CaPlay.cpp)
set(CALIBS
    CaCore
    avcodec
    avdevice
    avfilter
    avformat
    avutil
    swresample
    swscale
    postproc
    iconv
    z
    bz2
    pthread
    twolame
    mp3lame
    gsm
    wavpack
    ogg
    vorbis
    vorbisenc
    vorbisfile
    faac)

add_executable(CaPlay ${CAPLAY} ${CAHEADERS})
target_link_libraries(CaPlay ${CALIBS})

ADD_FRAMEWORK(CoreAudio CaPlay)
ADD_FRAMEWORK(CoreServices CaPlay)
ADD_FRAMEWORK(AudioUnit CaPlay)
ADD_FRAMEWORK(AudioToolBox CaPlay)
ADD_FRAMEWORK(CoreFoundation CaPlay)
ADD_FRAMEWORK(CoreVideo CaPlay)
ADD_FRAMEWORK(VideoDecodeAcceleration CaPlay)
ADD_FRAMEWORK(Accelerate CaPlay)

install(TARGETS CaPlay DESTINATION bin)
