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

macx{
  INCLUDEPATH += /usr/local/include
}

win32{
  INCLUDEPATH += ../../graphviz/lib/cgraph \
                 ../../graphviz/lib/gvc \
                 ../../graphviz/lib/cdt \
                 ../../graphviz/lib/common \
                 ../../graphviz/lib/pathplan
}


HEADERS += ./include/componentmanager.h \
           ./include/gdefaultselectiongraphic.h \
           ./include/gmodelcomponent.h \
           ./include/graphicsview.h \
           ./include/hydrocouplecomposer.h \
           ./include/hydrocoupleproject.h \
           ./include/splashscreen.h \
           ./include/custompropertyitems.h \
           ./include/gexchangeitems.h \
           ./include/modelstatusitemmodel.h \
           ./include/modelstatusitemstatuschangeeventargswrapper.h \
           ./include/modelstatusitem.h \
           ./include/argumentdialog.h \
           ./include/gnode.h \
           ./include/gconnection.h

SOURCES += ./src/stdafx.cpp \
           ./src/main.cpp \
           ./src/hydrocoupleproject.cpp \
           ./src/componentmanager.cpp \
           ./src/gdefaultselectiongraphic.cpp \
           ./src/gmodelcomponent.cpp \
           ./src/graphicsview.cpp \
           ./src/hydrocouplecomposer.cpp \
           ./src/splashscreen.cpp \
           ./src/custompropertyitems.cpp \
           ./src/gexchangeitem.cpp \
           ./src/modelstatuitemmodel.cpp \
           ./src/modelstatusitemstatuschangeeventargswrapper.cpp \
           ./src/modelstatusitem.cpp \
           ./src/argumentdialog.cpp \
           ./src/gnode.cpp \
           ./src/gconnection.cpp \
           ./src/ginput.cpp \
           ./src/gmultiinput.cpp \
           ./src/goutput.cpp \
           ./src/gadaptedoutput.cpp

PRECOMPILED_HEADER += ./include/stdafx.h

RESOURCES += ./resources/hydrocouplecomposer.qrc
RC_FILE = ./resources/HydroCoupleComposer.rc

macx{
ICON = ./resources/HydroCoupleComposer.icns
}

FORMS += ./forms/hydrocouplecomposer.ui \
         ./forms/argumentdialog.ui


CONFIG(debug, debug|release) {
   DESTDIR = ./build/debug
   OBJECTS_DIR = $$DESTDIR/.obj
   MOC_DIR = $$DESTDIR/.moc
   RCC_DIR = $$DESTDIR/.qrc
   UI_DIR = $$DESTDIR/.ui
   
   macx{
   LIBS += -L./../../QPropertyModel/QPropertyModel/build/debug -lQPropertyModel.1.0.0 \
           -L/usr/local/lib -lcgraph \
           -L/usr/local/lib -lgvc
   }
   
   win32{
   LIBS += -L./../../QPropertyModel/QPropertyModel/build/debug -lQPropertyModel1 \
           -L./graphviz/lib -lcgraph \
           -L./graphviz/lib -lgvc
   }
}

CONFIG(release, debug|release){

    DESTDIR = bin
    RELEASE_EXTRAS = ./build/release 
    OBJECTS_DIR = $$RELEASE_EXTRAS/.obj
    MOC_DIR = $$RELEASE_EXTRAS/.moc
    RCC_DIR = $$RELEASE_EXTRAS/.qrc
    UI_DIR = $$RELEASE_EXTRAS/.ui
    
   macx{
      LIBS += -L./../../QPropertyModel/QPropertyModel/lib -lQPropertyModel.1.0.0 \
              -L/usr/local/lib -lcgraph \
              -L/usr/local/lib -lgvc
   }
   
   win32{
      LIBS += -L./../../QPropertyModel/QPropertyModel/lib -lQPropertyModel1 \
              -L./graphviz/lib -lcgraph \
              -L./graphviz/lib -lgvc
   }
}   


