QT                        += core
QT                        += gui
QT                        += widgets

TARGET                     = CaPlayer
TEMPLATE                   = app

INCLUDEPATH               += $${PWD}/../../..
INCLUDEPATH               += $${PWD}/../../../Platforms/Android/FFmpeg/include
LIBS                      += -L$${PWD}/../../../Platforms/Android/FFmpeg/lib

DEFINES                   += __STDC_CONSTANT_MACROS

include ($${PWD}/../../../CiosAudio.pri)

HEADERS                   += $${PWD}/CaPlayerWindow.hpp

SOURCES                   += $${PWD}/CaPlayerWindow.cpp

FORMS                     += $${PWD}/CaPlayerWindow.ui

CONFIG                    += mobility
MOBILITY                   =

ANDROID_PACKAGE_SOURCE_DIR = $${PWD}/android

OTHER_FILES               += $${PWD}/android/AndroidManifest.xml
