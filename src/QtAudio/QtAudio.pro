NAME         = QtAudio
TARGET       = $${NAME}

QT           = core
QT          -= gui
QT          -= script
QT          += QtFFmpeg

# CONFIG      += staticlib

load(qt_module)

HEADERS     += $${PWD}/../../include/$${NAME}/qtaudio.h

include ($${PWD}/CiosAudio/CiosAudio.pri)

OTHER_FILES += $${PWD}/../../include/$${NAME}/headers.pri
include ($${PWD}/../../doc/Qt/Qt.pri)

TRNAME       = $${NAME}
include ($${PWD}/../../Translations.pri)

win32 {
LIBS        += -lavcodec
LIBS        += -lavdevice
LIBS        += -lavfilter
LIBS        += -lavformat
LIBS        += -lavutil
LIBS        += -lpostproc
LIBS        += -lswresample
LIBS        += -lswscale
}
