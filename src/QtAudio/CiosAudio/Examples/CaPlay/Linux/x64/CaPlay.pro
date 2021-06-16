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
INCLUDEPATH += $${PWD}/../../../../Platforms/Linux/OSS/include

HEADERS     += $${PWD}/../../../../CiosAudio.hpp

SOURCES     += $${PWD}/../../CaPlay.cpp

LIBS        += -L$${PWD}/../../../Linux/x64
#LIBS        += -L$${PWD}/../../../../Platforms/Linux/FFmpeg/lib/x64
LIBS        += -L/home/Qt/Arena/lib
LIBS        += -lavcodec
LIBS        += -lavformat
LIBS        += -lavutil
LIBS        += -lswresample
LIBS        += -lswscale
LIBS        += -lpthread
LIBS        += -lSDL
LIBS        += -lasound
LIBS        += -ldl
LIBS        += -lX11
LIBS        += -lXext
LIBS        += -lz
LIBS        += -llzma
LIBS        += -lbz2
LIBS        += -lXv
LIBS        += -lm
LIBS        += -lrt
LIBS        += -ljack
LIBS        += -lfaac
LIBS        += -lgsm
LIBS        += -lmp3lame
LIBS        += -ltwolame
LIBS        += -lopus
LIBS        += -lspeex
LIBS        += -ltheora
LIBS        += -ltheoraenc
LIBS        += -ltheoradec
LIBS        += -lwavpack
LIBS        += -lbluray
LIBS        += -lschroedinger-1.0
LIBS        += -lopencore-amrnb
LIBS        += -lopencore-amrwb
LIBS        += -lsoxr
LIBS        += -lbs2b
LIBS        += -lcaca
LIBS        += -ldc1394
LIBS        += -lva
LIBS        += -lhpi
LIBS        += -lhpimux
LIBS        += -lhpiudp

OTHER_FILES += $${PWD}/CMakeLists.txt
