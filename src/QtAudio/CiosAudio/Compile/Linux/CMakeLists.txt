cmake_minimum_required(VERSION 2.8)
message("Compile CIOS Audio Core on Linux")
PROJECT(CiosAudio)
INCLUDE (CheckIncludeFile)

# add_definitions(-DCAC_NAMESPACE=CiosAudio)

set(CMAKE_BUILD_TYPE Release)

option(EnableDEBUG "DEBUG" ON)

include_directories(usr/local/include)

message("Looking for Advanced Linux Sound Architecture")
find_package(ALSA)
include_directories(${ALSA_INCLUDE_DIRS})
set(LIBS ${LIBS} ${ALSA_LIBRARIES})
if (ALSA_FOUND)
option(EnableAlsa "ALSA" ON)
else (ALSA_FOUND)
option(EnableAlsa "ALSA" OFF)
endif (ALSA_FOUND)

message("Looking for Open Sound System")
CHECK_INCLUDE_FILE("linux/soundcard.h" HAVE_LINUX_SOUNDCARD_H)
if (HAVE_LINUX_SOUNDCARD_H)
option(EnableOSS "OSS" ON)
add_definitions(-DHAVE_LINUX_SOUNDCARD_H)
message("Add definition HAVE_LINUX_SOUNDCARD_H")
else (HAVE_LINUX_SOUNDCARD_H)
CHECK_INCLUDE_FILE("sys/soundcard.h" HAVE_SYS_SOUNDCARD_H)
if (HAVE_SYS_SOUNDCARD_H)
option(EnableOSS "OSS" ON)
add_definitions(-DHAVE_SYS_SOUNDCARD_H)
message("Add definition HAVE_SYS_SOUNDCARD_H")
else (HAVE_SYS_SOUNDCARD_H)
option(EnableOSS "OSS" OFF)
endif  (HAVE_SYS_SOUNDCARD_H)
endif  (HAVE_LINUX_SOUNDCARD_H)

message("Looking for JACK Audio Connection Kit")
CHECK_INCLUDE_FILE("jack/jack.h" Jack_FOUND)
if (Jack_FOUND)
option(EnableJack "JACK" ON)
message("Add Jack into default module")
else (Jack_FOUND)
option(EnableJack "JACK" OFF)
endif  (Jack_FOUND)

message("Looking for AudioScience HPI API")
CHECK_INCLUDE_FILE("asihpi/hpi.h" ASIHPI_FOUND)
if (ASIHPI_FOUND)
option(EnableASIHPI "ASIHPI" ON)
message("Add ASIHPI into default module")
else (ASIHPI_FOUND)
option(EnableASIHPI "ASIHPI" OFF)
endif  (ASIHPI_FOUND)

option(EnableOpenAL "OpenAL" OFF)
option(EnablePulseAudio "PulseAudio" OFF)
option(EnableVLC "VLC" OFF)

set(LIBS ${LIBS} ${FFMPEG_LIBRARIES})
include_directories(${FFMPEG_INCLUDE_DIRS})
include_directories(../../Platforms/Linux/FFmpeg/include)
option(EnableFFMPEG "FFMPEG" ON)
message("Add FFmpeg into default module")

option(EnableSkeleton "SKELETON" ON)
option(EnableUtilities "Utilities" ON)

include_directories(../../ ../../Platforms/Linux ../../Platforms/Skeleton)

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
SET(CALINUX
    ../../Platforms/Linux/CaAllocator.cpp
    ../../Platforms/Linux/CaLinux.cpp
    ../../Platforms/Linux/CaTimer.cpp)

SET(ALSASRC ../../Platforms/Linux/CaALSA.cpp)
SET(ASIHPISRC ../../Platforms/Linux/CaASIHPI.cpp)
SET(OSSSRC ../../Platforms/Linux/CaOSS.cpp)
SET(JACKSRC ../../Platforms/Linux/CaJack.cpp)
SET(OPENALSRC ../../Platforms/Windows/CaOpenAL.cpp)
SET(PULSEAUDIOSRC ../../Platforms/Windows/CaPulseAudio.cpp)
SET(VLCSRC ../../Platforms/Windows/CaVLC.cpp)
SET(FFMPEGSRC ../../Platforms/Linux/CaFFmpeg.cpp)
SET(SKELETON ../../Platforms/Skeleton/CaSkeleton.cpp)
SET(UTILITIES
    ../../Utilities/BridgeConduit.cpp
    ../../Utilities/Linux/nMutex.cpp
    ../../Utilities/Linux/nThreadData.cpp
    ../../Utilities/Linux/nThread.cpp)

if (EnableAlsa)
add_definitions(-DENABLE_HOST_ALSA)
SET(HOSTAPIS ${HOSTAPIS} ${ALSASRC})
endif(EnableAlsa)

if (EnableJack)
add_definitions(-DENABLE_HOST_JACK)
SET(HOSTAPIS ${HOSTAPIS} ${JACKSRC})
endif (EnableJack)

if (EnableOSS)
add_definitions(-DENABLE_HOST_OSS)
SET(HOSTAPIS ${HOSTAPIS} ${OSSSRC})
endif (EnableOSS)

if (EnableASIHPI)
add_definitions(-DENABLE_HOST_ASIHPI)
SET(HOSTAPIS ${HOSTAPIS} ${ASIHPISRC})
endif (EnableASIHPI)

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

SET(CALISTS ${CACORES} ${CAPLAT} ${CALINUX} ${HOSTAPIS})
ADD_LIBRARY(CaCore ${CALISTS} ${CAHEADERS})

install(TARGETS CaCore DESTINATION lib)
install(FILES ../../CiosAudio.hpp DESTINATION include)
