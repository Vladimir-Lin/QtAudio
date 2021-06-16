DEFINES     += SkeletonAPI
DEFINES     += FFMPEGLIB
#DEFINES     += CAUTILITIES

INCLUDEPATH += $${PWD}

!contains (DEFINES,CIOSLIB) {
HEADERS     += $${PWD}/CiosAudio.hpp
}

HEADERS     += $${PWD}/CiosAudioPrivate.hpp

SOURCES     += $${PWD}/CiosAudio.cpp
SOURCES     += $${PWD}/CaCpuLoad.cpp
SOURCES     += $${PWD}/CaAllocationLink.cpp
SOURCES     += $${PWD}/CaAllocationGroup.cpp
SOURCES     += $${PWD}/CaDebug.cpp
SOURCES     += $${PWD}/CaDither.cpp
SOURCES     += $${PWD}/CaRingBuffer.cpp
SOURCES     += $${PWD}/CaLoopBuffer.cpp
SOURCES     += $${PWD}/CaConverters.cpp
SOURCES     += $${PWD}/CaStreamIO.cpp
SOURCES     += $${PWD}/CaConduit.cpp
SOURCES     += $${PWD}/CaLinearConduit.cpp
SOURCES     += $${PWD}/CaConduitFunction.cpp
SOURCES     += $${PWD}/CaBuffer.cpp
SOURCES     += $${PWD}/CaStream.cpp
SOURCES     += $${PWD}/CaHostApi.cpp
SOURCES     += $${PWD}/CaHostApiInfo.cpp
SOURCES     += $${PWD}/CaDeviceInfo.cpp
SOURCES     += $${PWD}/CaStreamParameters.cpp
SOURCES     += $${PWD}/CaMediaCodec.cpp

include ($${PWD}/Platforms/Platforms.pri)
include ($${PWD}/Documents/Documents.pri)
include ($${PWD}/Compile/Compile.pri)
include ($${PWD}/Bindings/Bindings.pri)
include ($${PWD}/Utilities/Utilities.pri)

OTHER_FILES += $${PWD}/Examples/CaPlay/CaPlay.cpp
OTHER_FILES += $${PWD}/Examples/CaPlay/Windows/x64/CMakeLists.txt
OTHER_FILES += $${PWD}/README.txt
OTHER_FILES += $${PWD}/Windows.txt
OTHER_FILES += $${PWD}/MacOSX.txt
OTHER_FILES += $${PWD}/Linux.txt
OTHER_FILES += $${PWD}/Solaris.txt
OTHER_FILES += $${PWD}/FreeBSD.txt
OTHER_FILES += $${PWD}/iPhoneOS.txt
OTHER_FILES += $${PWD}/Android.txt
