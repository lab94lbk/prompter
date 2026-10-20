QT += core gui qml quick quickcontrols2

CONFIG += c++17 release link_pkgconfig
PKGCONFIG += libdrm
TARGET = prompter
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/drmoutput.h \
    src/preview.h \
    src/renderer.h \
    src/theme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/drmoutput.cpp \
    src/preview.cpp \
    src/renderer.cpp \
    src/theme.cpp

RESOURCES += src/resources.qrc
