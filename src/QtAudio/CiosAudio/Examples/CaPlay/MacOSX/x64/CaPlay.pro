TEMPLATE     = app
CONFIG      += console
CONFIG      -= app_bundle
CONFIG      -= qt

DEFINES     += FFMPEGLIB
DEFINES     += CAUTILITIES
# DEFINES     += REMOVE_DEBUG_MESSAGE

include ($${PWD}/../../../../CiosAudio.pri)

INCLUDEPATH += /usr/local/include
INCLUDEPATH += $${PWD}/../../../..
INCLUDEPATH += $${PWD}/../../../../Platforms/MacOSX/FFmpeg/include

HEADERS     += $${PWD}/../../../../CiosAudio.hpp

SOURCES     += $${PWD}/../../CaPlay.cpp

LIBS        += -L/usr/local/lib
#LIBS        += -L$${PWD}/../../../MacOSX/x64
#LIBS        += -L$${PWD}/../../../../Platforms/MacOSX/FFmpeg/lib/x64

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

OTHER_FILES += $${PWD}/CMakeLists.txt
