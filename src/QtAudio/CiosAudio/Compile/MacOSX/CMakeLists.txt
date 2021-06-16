cmake_minimum_required(VERSION 2.8)
message("Compile CIOS Audio Core on Mac OS X")
PROJECT(CiosAudio)
INCLUDE (CheckIncludeFile)

# add_definitions(-DCAC_NAMESPACE=CiosAudio)

set(CMAKE_BUILD_TYPE Release)

option(EnableCoreAudio "CoreAudio" ON)
option(EnableOpenAL "OpenAL" OFF)
option(EnablePulseAudio "PulseAudio" OFF)
option(EnableVLC "VLC" OFF)
option(EnableDEBUG "DEBUG" ON)

set(LIBS ${LIBS} ${FFMPEG_LIBRARIES})
include_directories(${FFMPEG_INCLUDE_DIRS})
include_directories(/usr/local/include)
include_directories(../../Platforms/Linux/FFmpeg/include)
option(EnableFFMPEG "FFMPEG" ON)
message("Add FFmpeg into default module")

option(EnableSkeleton "SKELETON" ON)
option(EnableUtilities "Utilities" ON)

include_directories(../../ ../../Platforms/MacOSX ../../Platforms/Skeleton)

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
SET(CAMACOSX
    ../../Platforms/MacOSX/CaAllocator.cpp
    ../../Platforms/MacOSX/CaMacOSX.cpp
    ../../Platforms/MacOSX/CaTimer.cpp)

SET(COREAUDIOSRC ../../Platforms/MacOSX/CaCoreAudio.cpp)
SET(OPENALSRC ../../Platforms/Windows/CaOpenAL.cpp)
SET(PULSEAUDIOSRC ../../Platforms/Windows/CaPulseAudio.cpp)
SET(VLCSRC ../../Platforms/Windows/CaVLC.cpp)
SET(FFMPEGSRC ../../Platforms/MacOSX/CaFFmpeg.cpp)
SET(SKELETON ../../Platforms/Skeleton/CaSkeleton.cpp)
SET(UTILITIES
    ../../Utilities/BridgeConduit.cpp
    ../../Utilities/RunMedia.cpp
    ../../Utilities/MacOSX/nMutex.cpp
    ../../Utilities/MacOSX/nThreadData.cpp
    ../../Utilities/MacOSX/nThread.cpp)

if (EnableCoreAudio)
add_definitions(-DENABLE_HOST_COREAUDIO)
SET(HOSTAPIS ${HOSTAPIS} ${COREAUDIOSRC})
endif(EnableCoreAudio)

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

SET(CALISTS ${CACORES} ${CAPLAT} ${CAMACOSX} ${HOSTAPIS})
ADD_LIBRARY(CaCore ${CALISTS} ${CAHEADERS})

install(TARGETS CaCore DESTINATION lib)
install(FILES ../../CiosAudio.hpp DESTINATION include)
