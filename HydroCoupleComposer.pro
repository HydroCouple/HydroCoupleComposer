TEMPLATE = app
TARGET = HydroCoupleComposer
VERSION = 1.0.0

QT += core \
      widgets \
      gui

INCLUDEPATH += .\
               ./include \
               ../HydroCouple/include \
               ../../QPropertyModel/QPropertyModel/include

macx {
  INCLUDEPATH += /usr/local/include
}

HEADERS += ./include/componentmanager.h \
           ./include/connectiondialog.h \
           ./include/gdefaultselectiongraphic.h \
           ./include/gmodelcomponent.h \
           ./include/graphicsview.h \
           ./include/hydrocouplecomposer.h \
           ./include/hydrocoupleproject.h \
           ./include/splashscreen.h \
           ./include/custompropertyitems.h \
           ./include/gmodelcomponentconnection.h

SOURCES += ./src/stdafx.cpp \
           ./src/main.cpp \
           ./src/hydrocoupleproject.cpp \
           ./src/componentmanager.cpp \
           ./src/connectiondialog.cpp \
           ./src/gdefaultselectiongraphic.cpp \
           ./src/gmodelcomponent.cpp \
           ./src/graphicsview.cpp \
           ./src/hydrocouplecomposer.cpp \
           ./src/splashscreen.cpp \
           ./src/custompropertyitems.cpp \
           ./src/gmodelcomponentconnection.cpp

PRECOMPILED_HEADER += ./include/stdafx.h

RESOURCES += ./resources/hydrocouplecomposer.qrc
RC_FILE = ./resources/HydroCoupleComposer.rc
ICON = ./resources/HydroCoupleComposer.icns

FORMS += ./forms/hydrocouplecomposer.ui \
         ./forms/connectiondialog.ui

macx {
LIBS += -L$$PWD/../../QPropertyModel/QPropertyModel/bin/debug/ -lQPropertyModel.1.0.0
LIBS += -L/usr/local/lib -lcgraph
LIBS += -L/usr/local/lib -lgvc
}


