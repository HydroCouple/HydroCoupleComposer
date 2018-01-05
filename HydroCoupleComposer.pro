#Author Caleb Amoa Buahin
#Email caleb.buahin@gmail.com
#Date 2016
#License GNU General Public License (see <http: //www.gnu.org/licenses/> for details).

TEMPLATE = app
VERSION = 1.0.0
TARGET = HydroCoupleComposer
QT += core widgets gui printsupport concurrent opengl

#DEFINES += GRAPHVIZ_LIBRARY
DEFINES += UTAH_CHPC
DEFINES += USE_MPI
DEFINES += USE_OPENMP

CONFIG += c++11
CONFIG += debug_and_release

PRECOMPILED_HEADER += ./include/stdafx.h

INCLUDEPATH += .\
               ./include \
               ../HydroCouple/include \
               ../HydroCoupleVis/include \
               ../../QPropertyModel/include

macx{
    INCLUDEPATH += /usr/local/include \
                   /usr/local/include/libiomp
}

linux{
   INCLUDEPATH += /usr/include
}

win32{
  INCLUDEPATH += $$PWD/graphviz/win32/include \
                 $$(MSMPI_INC)/
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
           ./include/cpugpuallocation.h \
           ./include/preferencesdialog.h

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
           ./src/cpugpuallocation.cpp \
           ./src/preferencesdialog.cpp

macx{

    contains(DEFINES,USE_OPENMP){

        QMAKE_CC = /usr/local/opt/llvm/bin/clang
        QMAKE_CXX = /usr/local/opt/llvm/bin/clang++
        QMAKE_LINK = /usr/local/opt/llvm/bin/clang++

        QMAKE_CFLAGS+= -fopenmp
        QMAKE_LFLAGS+= -fopenmp
        QMAKE_CXXFLAGS+= -fopenmp -g

        INCLUDEPATH += /usr/local/opt/llvm/lib/clang/5.0.0/include
        LIBS += -L /usr/local/opt/llvm/lib -lomp

        message("OpenMP enabled")
     } else {

        message("OpenMP disabled")
     }

    contains(DEFINES,USE_MPI){

        QMAKE_CC = /usr/local/bin/mpicc
        QMAKE_CXX = /usr/local/bin/mpicxx
        QMAKE_LINK = /usr/local/bin/mpicxx

        QMAKE_CFLAGS += $$system(mpicc --showme:compile)
        QMAKE_CXXFLAGS += $$system(mpic++ --showme:compile)
        QMAKE_LFLAGS += $$system(mpic++ --showme:link)

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


win32{

    contains(DEFINES,USE_OPENMP){

        QMAKE_CFLAGS += -openmp
        QMAKE_LFLAGS += -openmp
        QMAKE_CXXFLAGS += -openmp
        #QMAKE_LIBS += -liomp5
        QMAKE_CXXFLAGS_RELEASE = $$QMAKE_CXXFLAGS
        QMAKE_CXXFLAGS_DEBUG = $$QMAKE_CXXFLAGS

      message("OpenMP enabled")
     } else {

      message("OpenMP disabled")
     }

    contains(DEFINES,USE_MPI){

#        QMAKE_CC = mpicc
#        QMAKE_CXX = mpic++
#        QMAKE_LINK = mpic++
       LIBS += -L$$(MSMPI_LIB64)/ -lmsmpi
      # message($$(MSMPI_LIB64))
       message("MPI enabled")
     } else {

      message("MPI disabled")
     }
}

CONFIG(debug, debug|release) {

   macx{
   LIBS += -L./../../QPropertyModel/build/debug -lQPropertyModel.1.0.0 \
           -L./../HydroCoupleVis/build/debug -lHydroCoupleVis.1.0.0 \
           -L/usr/local/lib -lcgraph \
           -L/usr/local/lib -lgvc
     }

   linux{

    contains(DEFINES,UTAH_CHPC){

         INCLUDEPATH += /uufs/chpc.utah.edu/common/home/u0660135/Projects/HydroCouple/graphviz/build/usr/local/include

         LIBS += -L./../../QPropertyModel/build/debug -lQPropertyModel.so.1.0.0 \
                 -L./../HydroCoupleVis/build/debug -lHydroCoupleVis.so.1.0.0 \
                 -L./../graphviz/build/usr/local/lib -lcgraph \
                 -L./../graphviz/build/usr/local/lib -lgvc

         message("Compiling on CHPC")
          } else {

   LIBS += -L./../../QPropertyModel/build/debug -lQPropertyModel.so.1.0.0 \
           -L./../HydroCoupleVis/build/debug -lHydroCoupleVis.so.1.0.0 \
           -L/usr/lib -lcgraph \
           -L/usr/lib -lgvc
          }
     }

   win32{

   QMAKE_CXXFLAGS += -MDd

   LIBS += -L$$PWD/../../QPropertyModel/build/debug -lQPropertyModel1 \
           -L$$PWD/../HydroCoupleVis/build/debug -lHydroCoupleVis1


    contains(DEFINES,GRAPHVIZ_LIBRARY){

          LIBS += -L$$PWD/graphviz/win32/lib -lcgraphd \
                  -L$$PWD/graphviz/win32/lib -lgvcd
          }
     }

   DESTDIR = ./build/debug
   OBJECTS_DIR = $$DESTDIR/.obj
   MOC_DIR = $$DESTDIR/.moc
   RCC_DIR = $$DESTDIR/.qrc
   UI_DIR = $$DESTDIR/.ui
}

CONFIG(release, debug|release){

   macx{
      LIBS += -L./../../QPropertyModel/lib/macx -lQPropertyModel.1.0.0 \
              -L./../HydroCoupleVis/lib/macx -lHydroCoupleVis.1.0.0 \
              -L/usr/local/lib -lcgraph \
              -L/usr/local/lib -lgvc
     }

   linux{
    contains(DEFINES,UTAH_CHPC){

         INCLUDEPATH += ./../graphviz/build/usr/local/include

         LIBS += -L./../../QPropertyModel/lib/linux -lQPropertyModel.so.1.0.0 \
                 -L./../HydroCoupleVis/lib/linux -lHydroCoupleVis.so.1.0.0 \
                 -L./../graphviz/build/usr/local/lib -lcgraph \
                 -L./../graphviz/build/usr/local/lib -lgvc

         message("Compiling on CHPC")
          } else {

   LIBS += -L./../../QPropertyModel/build/debug -lQPropertyModel \
           -L./../HydroCoupleVis/build/debug -lHydroCoupleVis \
           -L/usr/lib -lcgraph \
           -L/usr/lib -lgvc
          }
     }

   win32{
      
      QMAKE_CXXFLAGS += -MD
     
      LIBS += -L$$PWD/../../QPropertyModel/lib/win32 -lQPropertyModel1 \
              -L$$PWD/../HydroCoupleVis/lib/win32 -lHydroCoupleVis1


    contains(DEFINES,GRAPHVIZ_LIBRARY){

          LIBS += -L$$PWD/graphviz/win32/lib -lcgraph \
                  -L$$PWD/graphviz/win32/lib -lgvc
          }
     }

         #MacOS
         macx{
             DESTDIR = $$PWD/bin/macx
     }

         #Linux
         linux{
             DESTDIR = $$PWD/bin/linux
     }

         #Windows
         win32{
             DESTDIR = $$PWD/bin/win32
     }

    RELEASE_EXTRAS = $$PWD/build/release
    OBJECTS_DIR = $$RELEASE_EXTRAS/.obj
    MOC_DIR = $$RELEASE_EXTRAS/.moc
    RCC_DIR = $$RELEASE_EXTRAS/.qrc
    UI_DIR = $$RELEASE_EXTRAS/.ui
}

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
         ./forms/argumentdialog.ui \
         ./forms/preferencesdialog.ui



