INCLUDEPATH += $${PWD}

HEADERS     += $${PWD}/CaLinux.hpp

SOURCES     += $${PWD}/CaAllocator.cpp
SOURCES     += $${PWD}/CaTimer.cpp
SOURCES     += $${PWD}/CaLinux.cpp

include ($${PWD}/HostAPIs.pri)

contains (HostAPIs,ALSA) {
DEFINES     += ENABLE_HOST_ALSA
HEADERS     += $${PWD}/CaALSA.hpp
SOURCES     += $${PWD}/CaALSA.cpp
LIBS        += -lasound
LIBS        += -ldl
}

contains (HostAPIs,OSS) {
DEFINES     += ENABLE_HOST_OSS
HEADERS     += $${PWD}/CaOSS.hpp
SOURCES     += $${PWD}/CaOSS.cpp
}

contains (HostAPIs,JACK) {
DEFINES     += ENABLE_HOST_JACK
HEADERS     += $${PWD}/CaJack.hpp
SOURCES     += $${PWD}/CaJack.cpp
}

contains (HostAPIs,ASIHPI) {
DEFINES     += ENABLE_HOST_ASIHPI
HEADERS     += $${PWD}/CaASIHPI.hpp
SOURCES     += $${PWD}/CaASIHPI.cpp
LIBS        += -lhpi
LIBS        += -lhpimux
LIBS        += -lhpiudp
}

contains (HostAPIs,FFMPEG) {
DEFINES     += ENABLE_HOST_FFMPEG
HEADERS     += $${PWD}/CaFFmpeg.hpp
SOURCES     += $${PWD}/CaFFmpeg.cpp
LIBS        += -lavcodec
LIBS        += -lavformat
LIBS        += -lavutil
LIBS        += -lswresample
LIBS        += -lswscale
}

contains (HostAPIs,OpenAL) {
DEFINES     += ENABLE_HOST_OPENAL
HEADERS     += $${PWD}/CaOpenAL.hpp
SOURCES     += $${PWD}/CaOpenAL.cpp
}

contains (HostAPIs,PulseAudio) {
DEFINES     += ENABLE_HOST_PULSEAUDIO
HEADERS     += $${PWD}/CaPulseAudio.hpp
SOURCES     += $${PWD}/CaPulseAudio.cpp
}

contains (HostAPIs,VLC) {
DEFINES     += ENABLE_HOST_VLC
HEADERS     += $${PWD}/CaVLC.hpp
SOURCES     += $${PWD}/CaVLC.cpp
}
