HEADERS += $${PWD}/CaiPhoneOS.hpp
SOURCES += $${PWD}/CaAllocator.cpp
SOURCES += $${PWD}/CaTimer.cpp
SOURCES += $${PWD}/CaiPhoneOS.cpp

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
}
