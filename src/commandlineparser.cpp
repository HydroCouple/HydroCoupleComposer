#include "stdafx.h"
#include "commandlineparser.h"
#include "hydrocoupleproject.h"
#include "simulationmanager.h"
#include "gmodelcomponent.h"
#include "hydrocouplecomposer.h"
#include "splashscreen.h"
#include "experimentalsimulation.h"

#ifdef USE_MPI
#include <mpi.h>
#endif

#ifdef USE_OPENMP
#include <omp.h>
#endif

void CommandLineParser::setApplicationStyle(QApplication* application)
{
  application->setStyle("fusion");
  QFile file(":/HydroCoupleComposer/Styles");
  file.open(QFile::ReadOnly);
  QString styleSheet = QLatin1String(file.readAll());
  application->setStyleSheet(styleSheet);

  //  QPalette palette;

  //  palette.setColor(QPalette::Window,QColor(53,53,53));
  //  palette.setColor(QPalette::WindowText,Qt::white);
  //  palette.setColor(QPalette::Disabled,QPalette::WindowText,QColor(127,127,127));
  //  palette.setColor(QPalette::Base,QColor(42,42,42));
  //  palette.setColor(QPalette::AlternateBase,QColor(66,66,66));
  //  palette.setColor(QPalette::ToolTipBase,Qt::white);
  //  palette.setColor(QPalette::ToolTipText,Qt::white);
  //  palette.setColor(QPalette::Text,Qt::white);
  //  palette.setColor(QPalette::Disabled,QPalette::Text,QColor(127,127,127));
  //  palette.setColor(QPalette::Dark,QColor(35,35,35));
  //  palette.setColor(QPalette::Shadow,QColor(20,20,20));
  //  palette.setColor(QPalette::Button,QColor(53,53,53));
  //  palette.setColor(QPalette::ButtonText,Qt::white);
  //  palette.setColor(QPalette::Disabled,QPalette::ButtonText,QColor(127,127,127));
  //  palette.setColor(QPalette::BrightText,Qt::red);
  //  palette.setColor(QPalette::Link,QColor(42,130,218));
  //  palette.setColor(QPalette::Highlight,QColor(42,130,218));
  //  palette.setColor(QPalette::Disabled,QPalette::Highlight,QColor(80,80,80));
  //  palette.setColor(QPalette::HighlightedText,Qt::white);
  //  palette.setColor(QPalette::Disabled,QPalette::HighlightedText,QColor(127,127,127));

  //  application->setPalette(palette);
}

QStringList CommandLineParser::applicationArgsToStringList(int argc, char *argv[])
{
  QStringList applicationArguments;

  for(int i = 0; i < argc ; i++)
  {
    applicationArguments << QString(argv[i]);
  }

  return applicationArguments;
}

bool CommandLineParser::initializeMPI(int argc, char *argv[], int &numMPIProcesses, int &mpiProcessorRank)
{
  bool mpiInitialized = false;

#ifdef USE_MPI
  {
    int allowed = 0;
    int mpiStatus = MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &allowed);

    if(allowed < MPI_THREAD_MULTIPLE)
    {
      printf("Warning: This MPI Implementation does not provide sufficient threading support for %i\n", MPI_THREAD_MULTIPLE);
    }


    if (mpiStatus != MPI_SUCCESS)
    {
      printf ("Error starting MPI program. Terminating.\n");
      MPI_Abort(MPI_COMM_WORLD, mpiStatus);
    }
    else
    {
      char processor_name[MPI_MAX_PROCESSOR_NAME];
      MPI_Comm_size(MPI_COMM_WORLD, &numMPIProcesses);
      MPI_Comm_rank(MPI_COMM_WORLD, &mpiProcessorRank);
      int procNameLength = 0;
      MPI_Get_processor_name(processor_name, &procNameLength);

      printf("MPI Hostname : %s | No. Processors : %i | MPI Proc. Rank : %i\n ",
             processor_name, numMPIProcesses, mpiProcessorRank);


      mpiInitialized = true;
    }
  }
#else
  {
    numMPIProcesses = 1;
    mpiProcessorRank = 0;
  }
#endif



  return mpiInitialized;
}

void CommandLineParser::initializeGUIApplication(QApplication *application, const QFileInfo &projectFile)
{
  printf("Starting GUI...\n");

  //set style
  setApplicationStyle(application);
  HydroCoupleComposer* mainWindow = new HydroCoupleComposer();
  HydroCoupleComposer::hydroCoupleComposerWindow = mainWindow;

  //splash screen
  SplashScreen *splashScreen = new SplashScreen(mainWindow,QPixmap(":/HydroCoupleComposer/hydrocouplecomposersplash"));
  splashScreen->setFont(QFont("Segoe UI Light", 10, 2));
  splashScreen->show();


  //!Read settings
  splashScreen->onShowMessage("Reading Application Settings");
  printf("Reading Application Settings\n");

  //!Load components
  splashScreen->onShowMessage("Loading Component Libraries");
  printf("Loading Component Libraries\n");

  //!Setup component manager
  ComponentManager* componentManager = mainWindow->project()->componentManager();
  componentManager->connect(componentManager, SIGNAL(postMessage(const QString&)), splashScreen, SLOT(onShowMessage(const QString &)));

#ifdef QT_DEBUG
  {
    printf("Debug loading Libraries\n");
#if __APPLE__
    componentManager->addComponentDirectory(QDir("./../../../../../../HydroCoupleSDK/build/"));
#else
    componentManager->addComponentDirectory(QDir("./../HydroCoupleSDK"));
#endif
  }
#endif

  splashScreen->finish(mainWindow);
  mainWindow->show();

  if(projectFile.isFile() && projectFile.exists())
  {
    printf("Opening file: %s\n", qPrintable(projectFile.absoluteFilePath()));

    mainWindow->openFile(projectFile);
  }
}

void CommandLineParser::deleteComponent(GModelComponent *&component)
{
  if(component)
  {
    component->modelComponent()->finish();

    if(component->project())
    {
      delete component->project();
      component = nullptr;
    }
    else
    {
      delete component;
      component = nullptr;
    }
  }
}

void CommandLineParser::initializeCommandLineParser(QCommandLineParser &parser)
{
  parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);

  QCommandLineOption helpOption = parser.addHelpOption();
  QCommandLineOption versionOption =  parser.addVersionOption();

  parser.addPositionalArgument("file","The composition file to open/run.");

  const QCommandLineOption runOption({"r","run","execute"},"Execute action. Executes the composition <file> specified to completion.","file");
  parser.addOption(runOption);

  const QCommandLineOption verboseOption({"vb","verbose"},"Verbose mode. Print out details of simulation.");
  parser.addOption(verboseOption);

  const QCommandLineOption GUIOption({"ng","nogui"},"A flag to indicate whether to show GUI or not.", "");
  parser.addOption(GUIOption);

  const QCommandLineOption ExpSimOption({"x", "ex","exp"},"A flag to indicate whether to execute supplied file in parallel as experimental simulation. The no gui option must be set", "file");
  parser.addOption(ExpSimOption);

  const QCommandLineOption OpenMPOption({"t", "nt"},"The maximum number of threads to use with OpenMP", "number of threads");
  parser.addOption(OpenMPOption);

}

int CommandLineParser::executeCommandLine(QCommandLineParser &parser, int argc, char *argv[])
{
  int returnVal = 0;

  //parse application arguments
  QStringList applicationArguments = applicationArgsToStringList(argc, argv);
  parser.parse(applicationArguments);

  bool isGUI = true;

  //try to initialize MPI
  initializeMPI(argc, argv, HydroCoupleProject::numMPIProcesses , HydroCoupleProject::mpiProcess);

  QCoreApplication *application = nullptr;

  //Check if GUI and initialize application
  if(!parser.isSet("ng") && HydroCoupleProject::mpiProcess == 0 &&
          !parser.isSet("version") && !parser.isSet("help") &&
          !parser.isSet("ex"))
  {
    application = new QApplication(argc, argv);
    isGUI = true;
  }
  else
  {
    application = new QCoreApplication(argc, argv);
    isGUI = false;
  }

  //Set application identifiers
  application->setOrganizationName("hydrocouple");
  application->setOrganizationDomain("hydrocouple.org");
  application->setApplicationName("hydrocouplecomposer");
  application->setApplicationVersion("1.0.0");

  if(parser.isSet("nt"))
  {
    int numThreads = parser.value("nt").toInt();

    //check for openmp
#ifdef USE_OPENMP
    if(numThreads >= 1 && numThreads <= omp_get_max_threads())
      omp_set_num_threads(numThreads);

    printf("OpenMP is enabled with %i Processors and %i Max Threads\n", omp_get_num_procs() , omp_get_max_threads());

#endif
  }
  else
  {
#ifdef USE_OPENMP
    printf("OpenMP is enabled with %i Processors and %i Max Threads\n", omp_get_num_procs() , omp_get_max_threads());
#endif
  }


  //version
  if(parser.isSet("version"))
  {
    if(HydroCoupleProject::mpiProcess == 0)
    {
      printf("Application Name: %s\nApplication Version: %s\n", "HydroCoupleComposer",
             qPrintable(QCoreApplication::applicationVersion()));

#ifdef USE_MPI
        MPI_Finalize();
#endif

    }
    else
    {
#ifdef USE_MPI
      MPI_Finalize();
#endif
    }

    application->quit();
  }
  //help
  else if (parser.isSet("help"))
  {
    if(HydroCoupleProject::mpiProcess == 0)
    {

#ifdef USE_MPI
        MPI_Finalize();
#endif

      parser.showHelp();
    }
    else
    {
#ifdef USE_MPI
        MPI_Finalize();
#endif
    }

    application->quit();

  }
  else
  {

    //Catch when quitting
    QObject::connect(application, &QCoreApplication::aboutToQuit,&CommandLineParser::applicationExiting);

    bool fileSpecified = false;
    QFileInfo inputFile;

    //Check if positional argument has been set
    if(!parser.positionalArguments().isEmpty() && QFile::exists(parser.positionalArguments().first()))
    {
      inputFile = QFileInfo(parser.positionalArguments().first());
      fileSpecified = true;
    }
    //Otherwise check if run argument file has been specified
    else if(parser.isSet("r") && parser.value("r").size() )
    {

      QFileInfo file = QFileInfo(parser.value("r"));

      if(file.isRelative())
      {
        QDir dir(QDir::currentPath());
        file = dir.absoluteFilePath(file.filePath());
      }

      if(file.isFile() && file.exists())
      {
        inputFile = file;
        fileSpecified = true;
      }
    }
    else if(parser.isSet("ex") && parser.value("ex").size() )
    {

      QFileInfo file = QFileInfo(parser.value("ex"));

      if(file.isRelative())
      {
        QDir dir(QDir::currentPath());
        file = dir.absoluteFilePath(file.filePath());
      }

      if(file.isFile() && file.exists())
      {
        inputFile = file;
        fileSpecified = true;
      }
    }

    bool verbose = false;

    if(parser.isSet("verbose"))
    {
      verbose = true;
    }

    if(isGUI)
    {
      initializeGUIApplication(dynamic_cast<QApplication*>(application),inputFile);
      returnVal = application->exec();
    }
    else
    {
      if(parser.isSet("ex"))
      {
        if(inputFile.exists())
        {
          if(HydroCoupleProject::mpiProcess == 0)
          {
            ExperimentalSimulation *experimentalSimulation = new ExperimentalSimulation(inputFile, nullptr);

            QList<QString> errors;

            if(experimentalSimulation->initialize(errors))
            {
              experimentalSimulation->execute();
            }
            else
            {
              for(QString error : errors)
              {
                printf("%s\n", error.toStdString().c_str());
              }
            }
          }
          else
          {
            executeMPIExperimentalSimulation();
          }
        }

        applicationExiting();
      }
      else
      {
        if(HydroCoupleProject::mpiProcess == 0)
        {
          QList<QString> messages;

          HydroCoupleProject* project = HydroCoupleProject::readProjectFile(inputFile, messages,true,false);

          if(messages.length())
          {
            printf("Error messages.\n");

            for(QString message : messages)
            {
              printf("%s\n", qPrintable(message));
            }
          }

          printf("Creating simulation manager...\n");
          SimulationManager simulationManager(project);
          simulationManager.setMonitorComponentMessages(true);
          printf("Running composition...\n");
          simulationManager.runComposition();

        }
        else
        {
          printf("Entering worker MPI process loop...\n");
          enterMPIWorkersLoop();
          printf("Quiting worker MPI process loop...\n");
        }

        applicationExiting();

      }

      application->quit();

    }
  }

  return returnVal;
}

void CommandLineParser::enterMPIWorkersLoop()
{

#ifdef USE_MPI
  {
    GModelComponent* component = nullptr;

    MPI_Status status;

    int result = 0;
    bool finalize = false;
    int loopCount = 0;

    while ((result = MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status)) == MPI_SUCCESS)
    {
      switch (status.MPI_TAG)
      {
        case CommandLineParser::CreateComponent:
          {
            deleteComponent(component);

            int dataSize = 0;
            MPI_Get_count(&status, MPI_CHAR, &dataSize);

            if(dataSize)
            {
              char* inputArgs = new char[dataSize];
              MPI_Recv(&inputArgs[0],dataSize,MPI_CHAR,status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);

              QString inputArgsString(inputArgs);
              QStringList splitArgs = inputArgsString.split(',', QString::SkipEmptyParts);

              QFileInfo inputFile(splitArgs[0]);
              int componentIndex = splitArgs[1].toInt();

              printf("Creating Worker Component: %i on MPI Processor: %i from Input File: %s\n", componentIndex, HydroCoupleProject::mpiProcess, qPrintable(inputFile.absoluteFilePath()));

              QList<QString> errorMessages;
              component = HydroCoupleProject::readModelComponent(inputFile, errorMessages, componentIndex);
              component->modelComponent()->mpiSetProcessRank(HydroCoupleProject::mpiProcess);

              printf("Finished Creating Worker Component: %s on MPI Processor: %i\n", qPrintable(component->modelComponent()->id()),HydroCoupleProject::mpiProcess);

              if(errorMessages.length())
              {
                printf("Error messages.\n");

                for(QString message : errorMessages)
                {
                  printf("%s\n", qPrintable(message));
                }
              }

              delete[] inputArgs;
            }
          }
          break;
        case CommandLineParser::InitializeAndPrepareComponent:
          {

            printf("Initializing and Preparing Worker Component on MPI Processor: %i\n", HydroCoupleProject::mpiProcess);

            int componentIndex = 0;
            MPI_Recv(&componentIndex,1,MPI_INT,status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);

            printf("Initializing and Preparing Worker Component: %s on MPI Processor: %i\n", qPrintable(component->modelComponent()->id()), HydroCoupleProject::mpiProcess);

            if(component->modelComponent()->index() == componentIndex)
            {
              component->modelComponent()->initialize();
              component->modelComponent()->prepare();
            }

            printf("Finished Initializing and Preparing Worker Component: %s on MPI Procesor: %i\n", qPrintable(component->modelComponent()->id()), HydroCoupleProject::mpiProcess);
          }
          break;
        case CommandLineParser::FinishComponent:
          {

            int index = 0;
            MPI_Recv(&index,1,MPI_INT,status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);

            if(component)
            {
              printf("Finishing Component...\n");
              deleteComponent(component);
              printf("Finished Finishing Component :) : %s | %s\n", qPrintable(component->id()), qPrintable(component->caption()));
            }
          }
          break;
        case CommandLineParser::Finalize:
          {
            int index = 0;
            MPI_Recv(&index,1,MPI_INT,status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
            deleteComponent(component);
            finalize = true;
          }
          break;
        default:
          {
            if(component)
            {
              component->modelComponent()->update();
            }
          }
          break;
      }

      loopCount++;

      if(finalize)
      {
        printf("Exiting worker loop\n");
        break;
      }
    }

    if(result != MPI_SUCCESS)
    {
      printf("Aborting...\n");
      processMPIStatus(result);
      MPI_Abort(MPI_COMM_WORLD, result);
    }

    if(component)
    {
      component->modelComponent()->finish();
      delete component;
    }
  }
#endif
}

void CommandLineParser::executeMPIExperimentalSimulation()
{
#ifdef USE_MPI
  {
    int result = 0;
    bool finalize = false;
    MPI_Status status;

    while ((result = MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status)) == MPI_SUCCESS)
    {
      switch (status.MPI_TAG)
      {
        case CommandLineParser::Finalize:
          {
            finalize = true;
          }
          break;
        default:
          {
            printf("Recieved message to execute\n");

            int dataSize = 0;
            MPI_Get_count(&status, MPI_CHAR, &dataSize);

            if(dataSize)
            {
              char* inputArgs = new char[dataSize];
              MPI_Recv(&inputArgs[0],dataSize,MPI_CHAR,status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);

              QString file(inputArgs);
              QFileInfo inputFile(file);

              QList<QString> messages;

              HydroCoupleProject* project = HydroCoupleProject::readProjectFile(inputFile, messages,true,false);

              if(messages.length())
              {
                printf("Error messages.\n");

                for(QString message : messages)
                {
                  printf("%s\n", qPrintable(message));
                }
              }

              printf("Creating simulation manager...\n");
              SimulationManager simulationManager(project);
              simulationManager.setMonitorComponentMessages(true);
              printf("Running composition...\n");
              simulationManager.runComposition();

              delete[] inputArgs;
            }
          }
          break;
      }

      if(finalize)
      {
        break;
      }
    }
  }
#endif
}

void CommandLineParser::processMPIStatus(int status)
{

#ifdef USE_MPI
  {
    switch (status)
    {
      case MPI_SUCCESS:
        {

        }
        break;
      case MPI_ERR_BUFFER:
        {
          printf("Invalid buffer pointer\n");
        }
        break;
      case MPI_ERR_COUNT:
        {
          printf("Invalid count argument\n");
        }
        break;
      case MPI_ERR_TYPE:
        {
          printf("Invalid datatype argument\n");
        }
        break;
      case  MPI_ERR_TAG:
        {
          printf("Invalid tag argument\n");
        }
        break;
      case  MPI_ERR_COMM:
        {
          printf("Invalid communicator\n");
        }
        break;
      case  MPI_ERR_RANK:
        {
          printf("Invalid rank\n");
        }
        break;
      case  MPI_ERR_ROOT:
        {
          printf("Invalid root\n");
        }
        break;
      case  MPI_ERR_GROUP:
        {
          printf("Invalid group\n");
        }
        break;
      case  MPI_ERR_OP:
        {
          printf("Invalid operation\n");
        }
        break;
      case   MPI_ERR_TOPOLOGY:
        {
          printf("Invalid topology\n");
        }
        break;
      case  MPI_ERR_DIMS:
        {
          printf("Invalid dimension argument\n");
        }
        break;
      case  MPI_ERR_ARG:
        {
          printf("Invalid argument\n");
        }
        break;
      case  MPI_ERR_UNKNOWN:
        {
          printf("Unknown error\n");
        }
        break;
      case  MPI_ERR_TRUNCATE:
        {
          printf("Message truncated on receive\n");
        }
        break;
      case  MPI_ERR_OTHER:
        {
          printf("Other error; use Error_string\n");
        }
        break;
      case  MPI_ERR_INTERN:
        {
          printf("Internal error code\n");
        }
        break;
      case  MPI_ERR_IN_STATUS:
        {
          printf("Error code is in status\n");
        }
        break;
      case   MPI_ERR_PENDING:
        {
          printf("Pending request\n");
        }
        break;
      case  MPI_ERR_REQUEST:
        {
          printf("Invalid request (handle)\n");
        }
        break;
      case  MPI_ERR_ACCESS:
        {
          printf("Permission denied\n");
        }
        break;
      case   MPI_ERR_AMODE:
        {
          printf("Error related to amode passed to MPI_File_open\n");
        }
        break;
      case   MPI_ERR_BAD_FILE:
        {
          printf("Invalid file name (e.g., path name too long)\n");
        }
        break;
      case   MPI_ERR_CONVERSION:
        {
          printf("Error in user data conversion function\n");
        }
        break;
      case   MPI_ERR_DUP_DATAREP:
        {
          printf("Data representation identifier already registered\n");
        }
        break;
      case   MPI_ERR_FILE_EXISTS:
        {
          printf("File exists\n");
        }
        break;
      case   MPI_ERR_FILE_IN_USE:
        {
          printf("File operation could not be completed, file in use\n");
        }
        break;
      case    MPI_ERR_FILE:
        {
          printf("Invalid file handle\n");
        }
        break;
      case    MPI_ERR_INFO:
        {
          printf("Invalid info argument\n");
        }
        break;
      case   MPI_ERR_INFO_KEY:
        {
          printf("Key longer than MPI_MAX_INFO_KEY\n");
        }
        break;
      case  MPI_ERR_INFO_VALUE:
        {
          printf("Value longer than MPI_MAX_INFO_VAL\n");
        }
        break;
      case   MPI_ERR_INFO_NOKEY:
        {
          printf("Invalid key passed to MPI_Info_delete\n");
        }
        break;
      case   MPI_ERR_IO:
        {
          printf("Other I/O error\n");
        }
        break;
      case   MPI_ERR_NAME:
        {
          printf("Invalid service name in MPI_Lookup_name\n");
        }
        break;
      case   MPI_ERR_NO_MEM:
        {
          printf("Alloc_mem could not allocate memory\n");
        }
        break;
      case   MPI_ERR_NOT_SAME:
        {
          printf("Collective argument/sequence not the same on all processes\n");
        }
        break;
      case   MPI_ERR_NO_SPACE:
        {
          printf("Not enough space\n");
        }
        break;
      case   MPI_ERR_NO_SUCH_FILE:
        {
          printf("File does not exist\n");
        }
        break;
      case   MPI_ERR_PORT:
        {
          printf("Invalid port name in MPI_comm_connect\n");
        }
        break;
      case    MPI_ERR_QUOTA:
        {
          printf("Quota exceeded\n");
        }
        break;
      case   MPI_ERR_READ_ONLY:
        {
          printf("Read-only file or file system\n");
        }
        break;
      case   MPI_ERR_SERVICE:
        {
          printf("Invalid service name in MPI_Unpublish_name\n");
        }
        break;
      case   MPI_ERR_SPAWN:
        {
          printf("Error in spawning processes\n");
        }
        break;
      case     MPI_ERR_UNSUPPORTED_DATAREP:
        {
          printf("Unsupported dararep in MPI_File_set_view\n");
        }
        break;
      case    MPI_ERR_UNSUPPORTED_OPERATION:
        {
          printf("Unsupported operation on file\n");
        }
        break;
      case    MPI_ERR_WIN:
        {
          printf("Invalid win argument\n");
        }
        break;
      case    MPI_ERR_BASE:
        {
          printf("Invalid base passed to MPI_Free_mem\n");
        }
        break;
      case    MPI_ERR_LOCKTYPE:
        {
          printf("Invalid locktype argument\n");
        }
        break;
      case    MPI_ERR_KEYVAL:
        {
          printf("Invalid keyval\n");
        }
        break;
      case   MPI_ERR_RMA_CONFLICT:
        {
          printf("Conflicting accesses to window\n");
        }
        break;
      case    MPI_ERR_RMA_SYNC:
        {
          printf("Wrong synchronization of RMA calls\n");
        }
        break;
      case    MPI_ERR_SIZE:
        {
          printf("Invalid size argument\n");
        }
        break;
      case    MPI_ERR_DISP:
        {
          printf("Invalid disp argument\n");
        }
        break;
      case    MPI_ERR_ASSERT:
        {
          printf("Invalid assert argument\n");
        }
        break;
      case    MPI_ERR_LASTCODE:
        {
          printf("Last valid error code for a predefined error class.\n");
        }
        break;
    }
  }
#endif
}

void CommandLineParser::applicationExiting()
{
  //finalize MPI
#ifdef USE_MPI
  {
    if (HydroCoupleProject::mpiProcess == 0)
    {

      if(HydroCoupleProject::numMPIProcesses > 1)
      {
        printf("Quitting all workers...\n");

        for (int i = 1; i < HydroCoupleProject::numMPIProcesses; i++)
        {
          int test = 0;
          MPI_Send(&test, 1, MPI_INT, i, CommandLineParser::Finalize, MPI_COMM_WORLD);
        }
      }

      printf("Quitting MPI Process %i/%i\n", HydroCoupleProject::mpiProcess, HydroCoupleProject::numMPIProcesses - 1);
      MPI_Finalize();
    }
    else
    {
      printf("Quitting Worker MPI Process %i/%i\n", HydroCoupleProject::mpiProcess, HydroCoupleProject::numMPIProcesses - 1);
      MPI_Finalize();
    }
  }
#endif
}
