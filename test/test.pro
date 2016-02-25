QT += core network
QT -= gui

CONFIG += c++11

TARGET = test
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    testibqt.cpp

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../lib/release/ -libqt
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../lib/debug/ -libqt
else:unix: LIBS += -L$$OUT_PWD/../lib/ -libqt

INCLUDEPATH += $$PWD/../lib
DEPENDPATH += $$PWD/../lib

HEADERS += \
    testibqt.h
