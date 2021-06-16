#-------------------------------------------------
#
# Project created by QtCreator 2014-12-22T01:33:27
#
#-------------------------------------------------

QT          += core
QT          += gui
QT          += widgets

TARGET       = CaWaveFreqFFT
TEMPLATE     = app

DEFINES     += FFMPEGLIB
DEFINES     += CAUTILITIES
#DEFINES     += REMOVE_DEBUG_MESSAGE

include ($${PWD}/../../../CiosAudio.pri)
include ($${PWD}/../../../Bindings/Qt/CaQtWrapper.pri)

INCLUDEPATH += $${PWD}/../../..

win32 {
LIBS        += -L$${PWD}/../../Windows/x64
}

linux {
INCLUDEPATH += $${PWD}/../../../Platforms/Linux/FFmpeg/include
INCLUDEPATH += $${PWD}/../../../Platforms/Linux/OSS/include
DEFINES     += HAVE_LINUX_SOUNDCARD_H
LIBS        += -lpthread
LIBS        += -lSDL
LIBS        += -lX11
LIBS        += -lXext
LIBS        += -lz
#LIBS        += -llzma
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
#LIBS        += -lopencore-amrnb
#LIBS        += -lopencore-amrwb
LIBS        += -lsoxr
LIBS        += -lbs2b
LIBS        += -lcaca
LIBS        += -ldc1394
#LIBS        += -lva
}

macx {
INCLUDEPATH += $${PWD}/../../../Platforms/MacOSX/FFmpeg/include
LIBS        += -L/usr/local/lib
LIBS        += -liconv
LIBS        += -lz
LIBS        += -lbz2
LIBS        += -lpthread
LIBS        += -ltwolame
LIBS        += -lmp3lame
LIBS        += -lgsm
LIBS        += -lwavpack
LIBS        += -logg
LIBS        += -lvorbis
LIBS        += -lvorbisenc
LIBS        += -lfaac
LIBS        += -framework CoreFoundation
LIBS        += -framework CoreVideo
LIBS        += -framework VideoDecodeAcceleration
LIBS        += -framework Accelerate
}

contains(CONFIG,iphoneos) {
INCLUDEPATH += $${PWD}/../../../Platforms/iPhoneOS/FFmpeg/include
LIBS        += -L$${PWD}/../../../Platforms/iPhoneOS/FFmpeg/lib/ARMv7
LIBS        += -lmp3lame
LIBS        += -ltwolame
LIBS        += -lwavpack
LIBS        += -lcrypto
LIBS        += -liconv
LIBS        += -lz
}

HEADERS     += $${PWD}/../../../CiosAudio.hpp
HEADERS     += $${PWD}/CaWaveFreqFFT.hpp

SOURCES     += $${PWD}/CaWaveFreqFFT.cpp

FORMS       += $${PWD}/CaWaveFreqFFT.ui
FORMS       += $${PWD}/CiosDevices.ui

RESOURCES   += $${PWD}/CaWaveFreqFFT.qrc
