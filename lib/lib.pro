#-------------------------------------------------
#
# Project created by QtCreator 2016-02-25T09:25:58
#
#-------------------------------------------------

QT       += network

QT       -= gui

TARGET = ibqt
TEMPLATE = lib

DEFINES += LIB_LIBRARY

SOURCES += \
    ibqt.cpp

HEADERS += \
    ibbardata.h \
    ibcommissionreport.h \
    ibcontract.h \
    ibdefines.h \
    ibexecution.h \
    ibfadatatype.h \
    iborder.h \
    iborderstate.h \
    ibscandata.h \
    ibsocketerrors.h \
    ibtagvalue.h \
    ibticktype.h \
    ibqt.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}

DISTFILES += \
    LICENSE
