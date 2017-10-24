#ifndef COMMANDLINEPARSER_H
#define COMMANDLINEPARSER_H

#include <QCoreApplication>
#include <QtWidgets/QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include "hydrocouple.h"

class GModelComponent;

class CommandLineParser
{
  public:

    enum SpecialMPITags
    {
      CreateComponent = 10001,
      InitializeAndPrepareComponent = 1002,
      FinishComponent = 10003,
      Finalize = 1004
    };

    static void setApplicationStyle(QApplication* application);

    static QStringList applicationArgsToStringList(int argc, char *argv[]);

    static bool initializeMPI(int argc, char *argv[], int &numMPIProcesses, int &mpiProcessorRank);

    static void initializeGUIApplication(QApplication *application, const QFileInfo &projectFile);

    static void deleteComponent(GModelComponent *&component);

    static void initializeCommandLineParser(QCommandLineParser &parser);

    static int executeCommandLine(QCommandLineParser &parser, int argc, char *argv[]);

    static void enterMPIWorkersLoop();

    static void processMPIStatus(int status);

  public slots:

    static void applicationExiting();

  private:

    CommandLineParser(){}
};

#endif // COMMANDLINEPARSER_H


