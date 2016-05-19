QT += core
QT -= gui

TARGET = NtClient
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    ntripclient.cpp

HEADERS += \
    ntripclient.h

QMAKE_CXXFLAGS += -std=c++0x -pthread
LIBS += -pthread

