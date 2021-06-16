TEMPLATE     = app
CONFIG      += console
CONFIG      -= app_bundle
CONFIG      -= qt

DEFINES     += FFMPEGLIB
DEFINES     += CAUTILITIES
#DEFINES     += REMOVE_DEBUG_MESSAGE

include ($${PWD}/../../../../CiosAudio.pri)

INCLUDEPATH += $${PWD}/../../../..
INCLUDEPATH += $${PWD}/../../../../Platforms/Windows/FFmpeg/include

HEADERS     += $${PWD}/../../../../CiosAudio.hpp

SOURCES     += $${PWD}/../../CaPlay.cpp

win32 {
  RC_FILE      = $${PWD}/../../CaPlay.rc
  OTHER_FILES  = $${PWD}/../../CaPlay.rc
}

LIBS        += -L$${PWD}/../../../Windows/x86
LIBS        += -L$${PWD}/../../../../Platforms/Windows/FFmpeg/lib/x86
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
