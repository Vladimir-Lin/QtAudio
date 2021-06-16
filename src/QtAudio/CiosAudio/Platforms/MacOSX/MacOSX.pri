HEADERS += $${PWD}/CaMacOSX.hpp
SOURCES += $${PWD}/CaAllocator.cpp
SOURCES += $${PWD}/CaTimer.cpp
SOURCES += $${PWD}/CaMacOSX.cpp

include ($${PWD}/HostAPIs.pri)

contains (HostAPIs,CoreAudio) {
DEFINES     += ENABLE_HOST_COREAUDIO
HEADERS     += $${PWD}/CaCoreAudio.hpp
SOURCES     += $${PWD}/CaCoreAudio.cpp
LIBS        += -framework CoreAudio
LIBS        += -framework CoreServices
LIBS        += -framework AudioUnit
LIBS        += -framework AudioToolBox
}

contains (HostAPIs,FFMPEG) {
DEFINES     += ENABLE_HOST_FFMPEG
HEADERS     += $${PWD}/CaFFmpeg.hpp
SOURCES     += $${PWD}/CaFFmpeg.cpp
LIBS        += -lavcodec
LIBS        += -lavdevice
LIBS        += -lavfilter
LIBS        += -lavformat
LIBS        += -lavutil
LIBS        += -lswresample
LIBS        += -lswscale
LIBS        += -lpostproc
}

contains (HostAPIs,OpenAL) {
DEFINES     += ENABLE_HOST_OPENAL
HEADERS     += $${PWD}/CaOpenAL.hpp
SOURCES     += $${PWD}/CaOpenAL.cpp
LIBS        += -lopenal
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
