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

win32 {
  INCLUDEPATH += 'C:/Program Files (x86)/Graphviz2.38/include'
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
           ./include/gmodelcomponentconnection.h \
    include/gcomponentitemconnection.h \
    include/gexchangeitems.h

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
           ./src/gmodelcomponentconnection.cpp \
    src/gexchangeitems.cpp

PRECOMPILED_HEADER += ./include/stdafx.h

RESOURCES += ./resources/hydrocouplecomposer.qrc
RC_FILE = ./resources/HydroCoupleComposer.rc
ICON = ./resources/HydroCoupleComposer.icns

FORMS += ./forms/hydrocouplecomposer.ui \
         ./forms/connectiondialog.ui


macx {
CONFIG(release , debug|release): LIBS += -L./../../../../QPropertyModel/QPropertyModel/bin/release -lQPropertyModel.1.0.0
CONFIG(debug , debug|release): LIBS += -L./../../../../QPropertyModel/QPropertyModel/bin/debug -lQPropertyModel.1.0.0
LIBS += -L/usr/local/lib -lcgraph
LIBS += -L/usr/local/lib -lgvc
}

win32 {
CONFIG(release, debug|release): LIBS += -L$$PWD/../../QPropertyModel/QPropertyModel/bin/release/ -lQPropertyModel1
CONFIG(debug, debug|release): LIBS += -L$$PWD/../../QPropertyModel/QPropertyModel/bin/debug/ -lQPropertyModel1

#CONFIG(release, debug|release): LIBS += -L'C:/Program Files (x86)/Graphviz2.38/lib/release/lib' -cgraph
#CONFIG(debug, debug|release): LIBS += -L'C:/Program Files (x86)/Graphviz2.38/lib/debug/lib' -cgraph

#CONFIG(release, debug|release):LIBS += -L'C:/Program Files (x86)/Graphviz2.38/lib/release/lib' -gvc
#CONFIG(debug, debug|release):LIBS += -L'C:/Program Files (x86)/Graphviz2.38/lib/debug/lib' -gvc
}

