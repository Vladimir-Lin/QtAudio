INCLUDEPATH += $${PWD}

HEADERS     += $${PWD}/CaSolaris.hpp

SOURCES     += $${PWD}/CaSolaris.cpp
SOURCES     += $${PWD}/CaAllocator.cpp
SOURCES     += $${PWD}/CaTimer.cpp

include ($${PWD}/HostAPIs.pri)

contains (HostAPIs,OSS) {
DEFINES     += ENABLE_HOST_OSS
HEADERS     += $${PWD}/CaOSS.hpp
SOURCES     += $${PWD}/CaOSS.cpp
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
