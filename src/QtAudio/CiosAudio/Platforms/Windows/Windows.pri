INCLUDEPATH += $${PWD}

HEADERS     += $${PWD}/CaWindows.hpp

SOURCES     += $${PWD}/CaWindows.cpp
SOURCES     += $${PWD}/CaAllocator.cpp
SOURCES     += $${PWD}/CaTimer.cpp

LIBS        += -luser32
LIBS        += -lwsock32
LIBS        += -lAdvapi32
LIBS        += -lole32

include ($${PWD}/HostAPIs.pri)

contains (HostAPIs,DirectSound) {
DEFINES     += ENABLE_HOST_DIRECTSOUND
HEADERS     += $${PWD}/CaDirectSound.hpp
SOURCES     += $${PWD}/CaDirectSound.cpp
LIBS        += -ldsound
}

contains (HostAPIs,WaSAPI) {
DEFINES     += ENABLE_HOST_WASAPI
HEADERS     += $${PWD}/CaWaSAPI.hpp
SOURCES     += $${PWD}/CaWaSAPI.cpp
}

contains (HostAPIs,WDMKS) {
DEFINES     += ENABLE_HOST_WDMKS
HEADERS     += $${PWD}/CaWdmks.hpp
SOURCES     += $${PWD}/CaWdmks.cpp
LIBS        += -lsetupapi
}

contains (HostAPIs,MME) {
DEFINES     += ENABLE_HOST_WMME
HEADERS     += $${PWD}/CaWmme.hpp
SOURCES     += $${PWD}/CaWmme.cpp
LIBS        += -lwinmm
}

contains (HostAPIs,FFMPEG) {
DEFINES     += ENABLE_HOST_FFMPEG
HEADERS     += $${PWD}/CaFFmpeg.hpp
SOURCES     += $${PWD}/CaFFmpeg.cpp
INCLUDEPATH += $${PWD}/FFmpeg/include
LIBS        += -L$${PWD}/FFmpeg/lib/x64
LIBS        += -lavcodec
LIBS        += -lavdevice
LIBS        += -lavfilter
LIBS        += -lavformat
LIBS        += -lavutil
LIBS        += -lpostproc
LIBS        += -lswresample
LIBS        += -lswscale
}

contains (HostAPIs,OpenAL) {
DEFINES     += ENABLE_HOST_OPENAL
HEADERS     += $${PWD}/CaOpenAL.hpp
SOURCES     += $${PWD}/CaOpenAL.cpp
LIBS        += -lOpenAL32
}

contains (HostAPIs,PulseAudio) {
DEFINES     += ENABLE_HOST_PULSEAUDIO
INCLUDEPATH += $${PWD}/pulse
HEADERS     += $${PWD}/CaPulseAudio.hpp
SOURCES     += $${PWD}/CaPulseAudio.cpp
}

contains (HostAPIs,VLC) {
DEFINES     += ENABLE_HOST_VLC
HEADERS     += $${PWD}/CaVLC.hpp
SOURCES     += $${PWD}/CaVLC.cpp
}
