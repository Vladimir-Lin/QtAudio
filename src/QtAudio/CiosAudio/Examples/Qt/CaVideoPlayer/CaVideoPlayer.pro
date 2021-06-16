#-------------------------------------------------
#
# Project created by QtCreator 2014-12-22T01:30:37
#
#-------------------------------------------------

QT          += core
QT          += gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QT          += opengl

TARGET       = CaVideoPlayer
TEMPLATE     = app

DEFINES     += FFMPEGLIB
DEFINES     += CAUTILITIES
#DEFINES     += REMOVE_DEBUG_MESSAGE

include ($${PWD}/../../../CiosAudio.pri)

INCLUDEPATH += $${PWD}/../../..

LIBS        += -L$${PWD}/../../Windows/x64

HEADERS     += $${PWD}/../../../CiosAudio.hpp

SOURCES     += $${PWD}/main.cpp
SOURCES     += $${PWD}/CaVideoPlayer.cpp

HEADERS     += $${PWD}/CaVideoPlayer.hpp

FORMS       += $${PWD}/CaVideoPlayer.ui
