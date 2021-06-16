cmake_minimum_required(VERSION 2.8)
message("Compile CIOS Audio Core on Linux")
PROJECT(CiosAudio)
INCLUDE (CheckIncludeFile)

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
CHECK_INCLUDE_FILE("/usr/include/linux/soundcard.h" HAVE_LINUX_SOUNDCARD_H)
if (HAVE_LINUX_SOUNDCARD_H)
option(EnableOSS "OSS" ON)
add_definitions(-DHAVE_LINUX_SOUNDCARD_H)
message("Add definition HAVE_LINUX_SOUNDCARD_H")
else (HAVE_LINUX_SOUNDCARD_H)
CHECK_INCLUDE_FILE("/usr/include/sys/soundcard.h" HAVE_SYS_SOUNDCARD_H)
if (HAVE_SYS_SOUNDCARD_H)
option(EnableOSS "OSS" ON)
add_definitions(-DHAVE_SYS_SOUNDCARD_H)
message("Add definition HAVE_SYS_SOUNDCARD_H")
else (HAVE_SYS_SOUNDCARD_H)
option(EnableOSS "OSS" OFF)
endif  (HAVE_SYS_SOUNDCARD_H)
endif  (HAVE_LINUX_SOUNDCARD_H)

message("Looking for JACK Audio Connection Kit")
CHECK_INCLUDE_FILE("/usr/include/jack/jack.h" Jack_FOUND)
if (Jack_FOUND)
option(EnableJack "JACK" ON)
message("Add Jack into default module")
else (Jack_FOUND)
option(EnableJack "JACK" OFF)
endif  (Jack_FOUND)

message("Looking for AudioScience HPI API")
CHECK_INCLUDE_FILE("/usr/include/asihpi/hpi.h" ASIHPI_FOUND)
if (ASIHPI_FOUND)
option(EnableASIHPI "ASIHPI" ON)
message("Add ASIHPI into default module")
else (ASIHPI_FOUND)
option(EnableASIHPI "ASIHPI" OFF)
endif  (ASIHPI_FOUND)

find_package(FFmpeg)
include_directories(${FFMPEG_INCLUDE_DIRS})
set(LIBS ${LIBS} ${FFMPEG_LIBRARIES})

if (FFmpeg_FOUND)
option(EnableFFMPEG "FFMPEG" ON)
else (FFmpeg_FOUND)
option(EnableFFMPEG "FFMPEG" OFF)
endif  (FFmpeg_FOUND)

option(EnableSkeleton "SKELETON" ON)

include_directories(../../ ../../Platforms/Linux ../../Platforms/Skeleton)

SET(CAHEADERS ../../CiosAudio.hpp)
SET(CACORES
    ../../CaAllocationGroup.cpp
    ../../CaAllocationLink.cpp
    ../../CaBuffer.cpp
    ../../CaConduit.cpp
    ../../CaConduitFunction.cpp
    ../../CaConverters.cpp
    ../../CaCpuLoad.cpp
    ../../CaDebug.cpp
    ../../CaDeviceInfo.cpp
    ../../CaDither.cpp
    ../../CaHostApi.cpp
    ../../CaHostApiInfo.cpp
    ../../CaRingBuffer.cpp
    ../../CaStream.cpp
    ../../CiosAudio.cpp
    ../../CaStreamParameters.cpp )
SET(CAPLAT ../../Platforms/CiosMachine.cpp ../../Platforms/CaPlatforms.cpp)
SET(CALINUX ../../Platforms/Linux/CaAllocator.cpp ../../Platforms/Linux/CaLinux.cpp ../../Platforms/Linux/CaTimer.cpp)

SET(ALSASRC ../../Platforms/Linux/CaALSA.cpp)
SET(ASIHPISRC ../../Platforms/Linux/CaASIHPI.cpp)
SET(OSSSRC ../../Platforms/Linux/CaOSS.cpp)
SET(JACKSRC ../../Platforms/Linux/CaJack.cpp)
SET(FFMPEGSRC ../../Platforms/Linux/CaFFmpeg.cpp)
SET(SKELETON ../../Platforms/Skeleton/CaSkeleton.cpp)

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

if (EnableFFMPEG)
add_definitions(-DENABLE_HOST_FFMPEG)
SET(HOSTAPIS ${HOSTAPIS} ${FFMPEGSRC})
endif (EnableFFMPEG)

if (EnableSkeleton)
add_definitions(-DSkeletonAPI)
include_directories(../../Platforms/Skeleton)
SET(HOSTAPIS ${HOSTAPIS} ${SKELETON})
endif (EnableSkeleton)

SET(CALISTS ${CACORES} ${CAPLAT} ${CALINUX} ${HOSTAPIS})
ADD_LIBRARY(CaCore ${CALISTS} ${CAHEADERS})
