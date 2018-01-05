#include "stdafx.h"
#include "hydrocouplecomposer.h"
#include "splashscreen.h"
#include "simulationmanager.h"
#include "commandlineparser.h"

#include <iostream>
#include <QCoreApplication>
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
  QCommandLineParser parser;
  parser.setApplicationDescription("HydroCoupleComposer CommandLine");
  CommandLineParser::initializeCommandLineParser(parser);
  return CommandLineParser::executeCommandLine(parser, argc, argv);
}


