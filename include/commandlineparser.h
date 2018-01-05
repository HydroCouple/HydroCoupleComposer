/*!
 * \file   hydrocouple.h
 * \author Caleb Amoa Buahin <caleb.buahin@gmail.com>
 * \version   1.0.0
 * \description
 * This header file contains the core interface definitions for the
 * HydroCouple component-based modeling definitions.
 * \license
 * This file and its associated files, and libraries are free software.
 * You can redistribute it and/or modify it under the terms of the
 * Lesser GNU General Public License as published by the Free Software Foundation;
 * either version 3 of the License, or (at your option) any later version.
 * This file and its associated files is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.(see <http://www.gnu.org/licenses/> for details)
 * \copyright Copyright 2014-2018, Caleb Buahin, All rights reserved.
 * \date 2014-2018
 * \pre
 * \bug
 * \warning
 * \todo
 */

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


