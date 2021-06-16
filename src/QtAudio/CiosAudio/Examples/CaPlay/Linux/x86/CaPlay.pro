TEMPLATE     = app
CONFIG      += console
CONFIG      -= app_bundle
CONFIG      -= qt

DEFINES     += FFMPEGLIB
DEFINES     += CAUTILITIES
#DEFINES     += REMOVE_DEBUG_MESSAGE
DEFINES     += HAVE_LINUX_SOUNDCARD_H

include ($${PWD}/../../../../CiosAudio.pri)

INCLUDEPATH += $${PWD}/../../../..
INCLUDEPATH += $${PWD}/../../../../Platforms/Linux/FFmpeg/include

HEADERS     += $${PWD}/../../../../CiosAudio.hpp

SOURCES     += $${PWD}/../../CaPlay.cpp

LIBS        += -L$${PWD}/../../../Linux/x86
LIBS        += -L$${PWD}/../../../../Platforms/Linux/FFmpeg/lib/x86
#LIBS        += -lCaCore
LIBS        += -lavcodec
LIBS        += -lavdevice
LIBS        += -lavfilter
LIBS        += -lavformat
LIBS        += -lavutil
LIBS        += -lpostproc
LIBS        += -lswresample
LIBS        += -lswscale
LIBS        += -lwinmm
LIBS        += -luser32
LIBS        += -lwsock32
LIBS        += -lAdvapi32
LIBS        += -ldsound

OTHER_FILES += $${PWD}/CMakeLists.txt
