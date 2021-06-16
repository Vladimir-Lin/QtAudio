cmake_minimum_required(VERSION 2.8)
message("Compile CIOS Audio Core on Windows")
PROJECT(CiosAudio)
INCLUDE (CheckIncludeFile)

# add_definitions(-DCAC_NAMESPACE=CiosAudio)

set(CMAKE_BUILD_TYPE Release)

option(EnableDirectSound "DirectSound" ON)
option(EnableWMME "WMME" ON)
option(EnableWASAPI "WASAPI" ON)
option(EnableWDMKS "WDMKS" ON)
option(EnableOpenAL "OpenAL" OFF)
option(EnablePulseAudio "PulseAudio" OFF)
option(EnableVLC "VLC" OFF)
option(EnableDEBUG "DEBUG" ON)

set(LIBS ${LIBS} ${FFMPEG_LIBRARIES})
include_directories(${FFMPEG_INCLUDE_DIRS})
include_directories(../../Platforms/Windows/FFmpeg/include)
option(EnableFFMPEG "FFMPEG" ON)
message("Add FFmpeg into default module")

option(EnableSkeleton "SKELETON" ON)
option(EnableUtilities "Utilities" ON)

include_directories(../../ ../../Platforms/Windows ../../Platforms/Skeleton)

SET(CAHEADERS ../../CiosAudio.hpp)
SET(CACORES
    ../../CaAllocationGroup.cpp
    ../../CaAllocationLink.cpp
    ../../CaBuffer.cpp
    ../../CaStreamIO.cpp
    ../../CaConduit.cpp
    ../../CaLinearConduit.cpp
    ../../CaConduitFunction.cpp
    ../../CaConverters.cpp
    ../../CaCpuLoad.cpp
    ../../CaDebug.cpp
    ../../CaDeviceInfo.cpp
    ../../CaDither.cpp
    ../../CaHostApi.cpp
    ../../CaHostApiInfo.cpp
    ../../CaRingBuffer.cpp
    ../../CaLoopBuffer.cpp
    ../../CaStream.cpp
    ../../CiosAudio.cpp
    ../../CaStreamParameters.cpp
    ../../CaMediaCodec.cpp )
SET(CAPLAT
    ../../Platforms/CiosMachine.cpp
    ../../Platforms/CaPlatforms.cpp)
SET(CAWINDOWS
    ../../Platforms/Windows/CaAllocator.cpp
    ../../Platforms/Windows/CaWindows.cpp
    ../../Platforms/Windows/CaTimer.cpp)

SET(DSOUNDSRC ../../Platforms/Windows/CaDirectSound.cpp)
SET(WMMESRC ../../Platforms/Windows/CaWmme.cpp)
SET(WASAPISRC ../../Platforms/Windows/CaWaSAPI.cpp)
SET(WDMKSSRC ../../Platforms/Windows/CaWdmks.cpp)
SET(OPENALSRC ../../Platforms/Windows/CaOpenAL.cpp)
SET(PULSEAUDIOSRC ../../Platforms/Windows/CaPulseAudio.cpp)
SET(VLCSRC ../../Platforms/Windows/CaVLC.cpp)
SET(FFMPEGSRC ../../Platforms/Windows/CaFFmpeg.cpp)
SET(SKELETON ../../Platforms/Skeleton/CaSkeleton.cpp)
SET(UTILITIES
    ../../Utilities/BridgeConduit.cpp
    ../../Utilities/Windows/nMutex.cpp
    ../../Utilities/Windows/nThreadData.cpp
    ../../Utilities/Windows/nThread.cpp)

if (EnableDirectSound)
add_definitions(-DENABLE_HOST_DIRECTSOUND)
SET(HOSTAPIS ${HOSTAPIS} ${DSOUNDSRC})
endif(EnableDirectSound)

if (EnableWMME)
add_definitions(-DENABLE_HOST_WMME)
SET(HOSTAPIS ${HOSTAPIS} ${WMMESRC})
endif (EnableWMME)

if (EnableWASAPI)
add_definitions(-DENABLE_HOST_WASAPI)
SET(HOSTAPIS ${HOSTAPIS} ${WASAPISRC})
endif (EnableWASAPI)

if (EnableWDMKS)
add_definitions(-DENABLE_HOST_WDMKS)
SET(HOSTAPIS ${HOSTAPIS} ${WDMKSSRC})
endif (EnableWDMKS)

if (EnableOpenAL)
add_definitions(-DENABLE_HOST_OPENAL)
SET(HOSTAPIS ${HOSTAPIS} ${OPENALSRC})
endif (EnableOpenAL)

if (EnablePulseAudio)
add_definitions(-DENABLE_HOST_PULSEAUDIO)
SET(HOSTAPIS ${HOSTAPIS} ${PULSEAUDIOSRC})
endif (EnablePulseAudio)

if (EnableVLC)
add_definitions(-DENABLE_HOST_VLC)
SET(HOSTAPIS ${HOSTAPIS} ${VLCSRC})
endif (EnableVLC)

if (EnableFFMPEG)
add_definitions(-DENABLE_HOST_FFMPEG)
add_definitions(-DFFMPEGLIB)
SET(HOSTAPIS ${HOSTAPIS} ${FFMPEGSRC})
endif (EnableFFMPEG)

if (EnableSkeleton)
add_definitions(-DSkeletonAPI)
include_directories(../../Platforms/Skeleton)
SET(HOSTAPIS ${HOSTAPIS} ${SKELETON})
endif (EnableSkeleton)

if (EnableDEBUG)
else (EnableDEBUG)
add_definitions(-DREMOVE_DEBUG_MESSAGE)
endif (EnableDEBUG)

if (EnableUtilities)
add_definitions(-DCAUTILITIES)
SET(HOSTAPIS ${HOSTAPIS} ${UTILITIES})
if (EnableFFMPEG)
SET(HOSTAPIS ${HOSTAPIS} ../../Utilities/RunMedia.cpp)
endif (EnableFFMPEG)
endif (EnableUtilities)

SET(CALISTS ${CACORES} ${CAPLAT} ${CAWINDOWS} ${HOSTAPIS})
ADD_LIBRARY(CaCore ${CALISTS} ${CAHEADERS})

install(TARGETS CaCore DESTINATION lib)
install(FILES ../../CiosAudio.hpp DESTINATION include)
