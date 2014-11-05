#-------------------------------------------------
#
# Project created by QtCreator 2012-11-01T18:30:28
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = configurer
TEMPLATE = app
RC_FILE = configurer.rc

QMAKE_LFLAGS += /MAP:CONF.MAP


SOURCES += main.cpp\
        mainwindow.cpp \
    editdetpresets.cpp \
    detparams.cpp \
    dswidget.cpp \
    logview.cpp \
    comm.cpp \
    cfgeditor.cpp \
    proto.cpp \
    portconfig.cpp \
    registeredit.cpp \
    camregisteredit.cpp

HEADERS  += mainwindow.h \
    editdetpresets.h \
    detparams.h \
    defs.h \
    qdswidget.h \
    logview.h \
    pchbarrier.h \
    ext_headers.h \
    errors.h \
    detpresetsmodel.h \
    comm.h \
    ext_qt_headers.h \
    cfgeditor.h \
    progress_dialog.h \
    portconfig.h \
    registeredit.h \
    proto.h \
    camregisteredit.h

INCLUDEPATH += ../filter/ball

INCLUDEPATH += ../../boost_1_48_0

TRANSLATIONS += configurer_ru.ts

FORMS    += mainwindow.ui \
    editdetpresets.ui \
    detparams.ui \
    logview.ui \
    cfgeditor.ui \
    portconfig.ui \
    registeredit.ui \
    camregisteredit.ui

LIBS += -L../../boost_1_48_0/stage/lib

LIBS += Strmiids.lib ole32.lib gdi32.lib user32.lib

VERSN = $$system(type build-number.txt)
VERSTR = '\\"$${VERSN}\\"'
DEFINES += __BUILD_NUMBER=$${VERSTR}
DATE = $$system(date /T)
DATESTR = '\\"$${DATE}\\"'
DEFINES += "__BUILD_DATE=\"\\\"$$system(date /T)\\\"\""
