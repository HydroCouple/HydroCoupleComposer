#Author Caleb Amoa Buahin
#Email caleb.buahin@gmail.com
#Date 2016
#License GNU General Public License (see <http://www.gnu.org/licenses/> for details).

TEMPLATE = app
TARGET = HydroCoupleComposer
QT += core widgets gui printsupport concurrent opengl

DEFINES += GRAPHVIZ_LIBRARY
DEFINES += UTAH_CHPC
DEFINES += USE_MPI
DEFINES += USE_OPENMP

CONFIG += c++11
CONFIG += debug_and_release

INCLUDEPATH += .\
               ./include \
               ../HydroCouple/include \
               ../HydroCoupleVis/include \
               ../../QPropertyModel/QPropertyModel/include

macx{
    INCLUDEPATH += /usr/local/include \
                   /usr/local/include/libiomp
}

linux{
   INCLUDEPATH += /usr/include/
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
           ./include/simulationmanager.h \
           ./include/commandlineparser.h \
           ./include/cpugpuallocation.h

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
           ./src/simulationmanager.cpp \
           ./src/commandlineparser.cpp \
           ./src/cpugpuallocation.cpp

macx{

    contains(DEFINES,USE_OPENMP){

        QMAKE_CC = /usr/local/opt/llvm/bin/clang
        QMAKE_CXX = /usr/local/opt/llvm/bin/clang++
        QMAKE_LINK = /usr/local/opt/llvm/bin/clang++

        QMAKE_CFLAGS+= -fopenmp
        QMAKE_LFLAGS+= -fopenmp
        QMAKE_CXXFLAGS+= -fopenmp

        INCLUDEPATH += /usr/local/opt/llvm/lib/clang/5.0.0/include
        LIBS += -L /usr/local/opt/llvm/lib -lomp

      message("OpenMP enabled")

    } else {

      message("OpenMP disabled")

    }

    contains(DEFINES,USE_MPI){

    # MPI Settings
    QMAKE_CC = /usr/local/bin/mpicc
    QMAKE_CXX = /usr/local/bin/mpicxx
    QMAKE_LINK = $$QMAKE_CXX

    LIBS += -L/usr/local/lib/ -lmpi

    message("MPI enabled")

    } else {

      message("MPI disabled")

    }
}


linux{

    contains(DEFINES,USE_OPENMP){

        QMAKE_CFLAGS += -fopenmp
        QMAKE_LFLAGS += -fopenmp
        QMAKE_CXXFLAGS += -fopenmp -g
        QMAKE_LIBS += -liomp5
        QMAKE_CXXFLAGS_RELEASE = $$QMAKE_CXXFLAGS
        QMAKE_CXXFLAGS_DEBUG = $$QMAKE_CXXFLAGS

      message("OpenMP enabled")

    } else {

      message("OpenMP disabled")

    }

    contains(DEFINES,USE_MPI){

        QMAKE_CC = mpicc
        QMAKE_CXX = mpic++
        QMAKE_LINK = mpic++

      message("MPI enabled")

    } else {

      message("MPI disabled")
    }


}

PRECOMPILED_HEADER += ./include/stdafx.h

RESOURCES += ./resources/hydrocouplecomposer.qrc
RC_FILE = ./resources/HydroCoupleComposer.rc

macx{
  ICON = ./resources/HydroCoupleComposer.icns
}

linux{
  ICON = ./resources/HydroCoupleComposer.ico
}

win32{
  ICON = ./resources/HydroCoupleComposer.ico
}

FORMS += ./forms/hydrocouplecomposer.ui \
         ./forms/argumentdialog.ui


CONFIG(debug, debug|release) {

   macx{
   LIBS += -L./../../QPropertyModel/QPropertyModel/build/debug -lQPropertyModel \
           -L./../HydroCoupleVis/build/debug -lHydroCoupleVis \
           -L/usr/local/lib -lcgraph \
           -L/usr/local/lib -lgvc
   }
   
   linux{

    contains(DEFINES,UTAH_CHPC){

         INCLUDEPATH += /uufs/chpc.utah.edu/common/home/u0660135/Projects/HydroCouple/graphviz/build/usr/local/include

         LIBS += -L./../../QPropertyModel/QPropertyModel/build/debug -lQPropertyModel \
                 -L./../HydroCoupleVis/build/debug -lHydroCoupleVis \
                 -L./../graphviz/build/usr/local/lib -lcgraph \
                 -L./../graphviz/build/usr/local/lib -lgvc

         message("Compiling on CHPC")


        } else {

   LIBS += -L./../../QPropertyModel/QPropertyModel/build/debug -lQPropertyModel \
           -L./../HydroCoupleVis/build/debug -lHydroCoupleVis \
           -L/usr/lib -lcgraph \
           -L/usr/lib -lgvc
       }
   }

   win32{
   LIBS += -L./../../QPropertyModel/QPropertyModel/build/debug -lQPropertyModel1 \
           -L./../HydroCoupleVis/build/debug -lHydroCoupleVis1 \
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
      LIBS += -L./../../QPropertyModel/QPropertyModel/lib -lQPropertyModel \
              -L./../HydroCoupleVis/lib -lHydroCoupleVis \
              -L/usr/local/lib -lcgraph \
              -L/usr/local/lib -lgvc
   }

   linux{
    contains(DEFINES,UTAH_CHPC){

         INCLUDEPATH += ./../graphviz/build/usr/local/include

         LIBS += -L./../../QPropertyModel/QPropertyModel/lib -lQPropertyModel \
                 -L./../HydroCoupleVis/lib -lHydroCoupleVis \
                 -L./../graphviz/build/usr/local/lib -lcgraph \
                 -L./../graphviz/build/usr/local/lib -lgvc

         message("Compiling on CHPC")


        } else {

   LIBS += -L./../../QPropertyModel/QPropertyModel/build/debug -lQPropertyModel \
           -L./../HydroCoupleVis/build/debug -lHydroCoupleVis \
           -L/usr/lib -lcgraph \
           -L/usr/lib -lgvc
       }
   }

   win32{
      LIBS += -L./../../QPropertyModel/QPropertyModel/lib -lQPropertyModel1 \
              -L./../HydroCoupleVis/lib -lHydroCoupleVis1 \
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

