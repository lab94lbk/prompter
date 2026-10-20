QT += core gui quick testlib
CONFIG += c++17 testcase link_pkgconfig
PKGCONFIG += libdrm
TARGET = prompter_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/drmoutput.h ../src/renderer.h ../src/theme.h
SOURCES += prompter_tests.cpp ../src/backend.cpp ../src/drmoutput.cpp ../src/renderer.cpp ../src/theme.cpp
