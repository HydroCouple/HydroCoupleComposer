QT += core widgets gui printsupport concurrent

TEMPLATE = app
TARGET = HydroCoupleComposer

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


HEADERS += ./include/stdafx.h \
           ./include/componentmanager.h \
           ./include/gdefaultselectiongraphic.h \
           ./include/gmodelcomponent.h \
           ./include/graphicsview.h \
           ./include/hydrocouplecomposer.h \
           ./include/hydrocoupleproject.h \
           ./include/splashscreen.h \
           ./include/custompropertyitems.h \
           ./include/gexchangeitems.h \
           ./include/modelstatusitemmodel.h \
           ./include/modelstatuschangeeventarg.h \
           ./include/modelstatusitem.h \
           ./include/argumentdialog.h \
           ./include/gnode.h \
           ./include/gconnection.h \
           ./include/qxmlsyntaxhighlighter.h \
           ./include/simulationmanager.h

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
           ./src/modelstatuschangeeventarg.cpp \
           ./src/modelstatusitem.cpp \
           ./src/argumentdialog.cpp \
           ./src/gnode.cpp \
           ./src/gconnection.cpp \
           ./src/ginput.cpp \
           ./src/gmultiinput.cpp \
           ./src/goutput.cpp \
           ./src/gadaptedoutput.cpp \
           ./src/qxmlsyntaxhighlighter.cpp \
           ./src/simulationmanager.cpp

PRECOMPILED_HEADER += ./include/stdafx.h

RESOURCES += ./resources/hydrocouplecomposer.qrc
RC_FILE = ./resources/HydroCoupleComposer.rc

macx{
ICON = ./resources/HydroCoupleComposer.icns
}

FORMS += ./forms/hydrocouplecomposer.ui \
         ./forms/argumentdialog.ui


CONFIG(debug, debug|release) {

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

   DESTDIR = ./build/debug
   OBJECTS_DIR = $$DESTDIR/.obj
   MOC_DIR = $$DESTDIR/.moc
   RCC_DIR = $$DESTDIR/.qrc
   UI_DIR = $$DESTDIR/.ui
}

CONFIG(release, debug|release){

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

    DESTDIR = bin
    RELEASE_EXTRAS = ./build/release 
    OBJECTS_DIR = $$RELEASE_EXTRAS/.obj
    MOC_DIR = $$RELEASE_EXTRAS/.moc
    RCC_DIR = $$RELEASE_EXTRAS/.qrc
    UI_DIR = $$RELEASE_EXTRAS/.ui
}   


