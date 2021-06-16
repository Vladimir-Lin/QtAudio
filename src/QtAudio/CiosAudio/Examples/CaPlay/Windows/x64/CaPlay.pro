TEMPLATE     = app
CONFIG      += console
CONFIG      -= app_bundle
CONFIG      -= qt

DEFINES     += FFMPEGLIB
DEFINES     += CAUTILITIES
#DEFINES     += REMOVE_DEBUG_MESSAGE

include ($${PWD}/../../../../CiosAudio.pri)

INCLUDEPATH += $${PWD}/../../../..

LIBS        += -L$${PWD}/../../../Windows/x64
LIBS        += -L$${PWD}/../../../../../../../lib/Windows/x64/ffmpeg

HEADERS     += $${PWD}/../../../../CiosAudio.hpp

SOURCES     += $${PWD}/../../CaPlay.cpp

win32 {
  RC_FILE      = $${PWD}/../../CaPlay.rc
  OTHER_FILES  = $${PWD}/../../CaPlay.rc
}

OTHER_FILES += $${PWD}/CMakeLists.txt
