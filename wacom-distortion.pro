#-------------------------------------------------
#
# Project created by QtCreator 2015-08-26T18:56:42
#
#-------------------------------------------------

QT       += core gui
CONFIG   += c++11

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = wacom-distortion
TEMPLATE = app


SOURCES += main.cc\
    lmath.c \
    calibrationwidget.cpp

HEADERS  += \
    lmath.h \
    calibrationwidget.h

DISTFILES += \
    README.md
