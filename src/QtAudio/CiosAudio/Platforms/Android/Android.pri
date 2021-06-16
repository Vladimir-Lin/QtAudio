INCLUDEPATH += $${PWD}

HEADERS     += $${PWD}/CaAndroid.hpp

SOURCES     += $${PWD}/CaAndroid.cpp
SOURCES     += $${PWD}/CaAllocator.cpp
SOURCES     += $${PWD}/CaTimer.cpp

include ($${PWD}/HostAPIs.pri)

contains (HostAPIs,ALSA) {
DEFINES     += ENABLE_HOST_ALSA
HEADERS     += $${PWD}/CaALSA.hpp
SOURCES     += $${PWD}/CaALSA.cpp
LIBS        += -ldl
}

contains (HostAPIs,OSS) {
DEFINES     += ENABLE_HOST_OSS
DEFINES     += HAVE_LINUX_SOUNDCARD_H
HEADERS     += $${PWD}/CaOSS.hpp
SOURCES     += $${PWD}/CaOSS.cpp
}

contains (HostAPIs,FFMPEG) {
DEFINES     += ENABLE_HOST_FFMPEG
HEADERS     += $${PWD}/CaFFmpeg.hpp
SOURCES     += $${PWD}/CaFFmpeg.cpp
LIBS        += -lswresample
LIBS        += -lswscale
LIBS        += -lavfilter
LIBS        += -lavformat
LIBS        += -lavcodec
LIBS        += -lavutil
LIBS        += -liconv
LIBS        += -lcharset
LIBS        += -lfaac
LIBS        += -lgsm
LIBS        += -lmp3lame
LIBS        += -ltwolame
LIBS        += -lwavpack
LIBS        += -lvorbisenc
LIBS        += -lvorbisfile
LIBS        += -lvorbis
LIBS        += -logg
}
