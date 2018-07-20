#include "stdafx.h"
#include "experimentalsimulation.h"
#include "hydrocoupleproject.h"
#include "simulationmanager.h"

#include <QDir>
#include <QTextStream>

#ifdef USE_MPI
#include <mpi.h>
#endif

#ifdef USE_OPENMP
#include <omp.h>
#endif

ExperimentalSimulation::ExperimentalSimulation(const QFileInfo &projectFile, QObject *parent)
  : QObject(parent),
    m_projectFile(projectFile)
{
}

ExperimentalSimulation::~ExperimentalSimulation()
{
  for(size_t i = 0; i < m_individuals.size(); i++)
  {
    delete m_individuals[i];
  }

  m_individuals.clear();
}

bool ExperimentalSimulation::initialize(QList<QString> &errors)
{
  m_associatedFiles.clear();
  m_variables.clear();
  m_variableValues.clear();

  for(size_t i = 0; i < m_individuals.size(); i++)
  {
    delete m_individuals[i];
  }

  m_individuals.clear();

  if(!m_projectFile.isDir())
  {
    m_projectFile = getAbsoluteFilePath(m_projectFile.filePath());

    if(QFile::exists(m_projectFile.absoluteFilePath()))
    {
      QFile file(m_projectFile.absoluteFilePath());

      if (file.open(QIODevice::ReadOnly))
      {
        QRegExp delimiters = QRegExp("(\\,|\\t|\\;|\\s+)");
        int currentFlag = -1;

        QTextStream streamReader(&file);
        int lineCount = 0;

        while (!streamReader.atEnd())
        {
          QString line = streamReader.readLine().trimmed();
          lineCount++;

          if (!line.isEmpty() && !line.isNull())
          {
            bool readSuccess = true;
            auto it = m_inputFileFlags.find(line.toStdString());

            if (it != m_inputFileFlags.cend())
            {
              currentFlag = it->second;
            }
            else if (!QStringRef::compare(QStringRef(&line, 0, 2), ";;"))
            {
              //commment do nothing
            }
            else
            {
              switch (currentFlag)
              {
                case 1:
                  {
                    m_inputFile = QFileInfo(line);

                    if(m_inputFile.isRelative())
                    {
                      m_inputFile = m_projectFile.absoluteDir().absoluteFilePath(line);
                    }

                    if(!QFile::exists(m_inputFile.absoluteFilePath()))
                    {
                      errors.push_back("Input file does not exist");
                      readSuccess = false;
                    }
                  }
                  break;
                case 2:
                  {
                    QFileInfo file = QFileInfo(line);

                    if(file.isRelative())
                    {
                      file = m_projectFile.absoluteDir().absoluteFilePath(line);
                    }


                    if(!QFile::exists(file.absoluteFilePath()))
                    {
                      errors.push_back("Associated Input file does not exist");
                      readSuccess = false;
                    }

                    m_associatedFiles.push_back(file);

                  }
                  break;
                case 3:
                  {
                    m_variables.push_back(line.toStdString());
                  }
                  break;
                case 4:
                  {
                    QStringList individual = line.split(delimiters, QString::SkipEmptyParts);

                    if(individual.size() == (int)m_variables.size())
                    {
                      std::vector<std::string> values;

                      for(QString row : individual)
                        values.push_back(row.toStdString());

                      m_variableValues.push_back(values);
                    }
                    else
                    {
                      errors.push_back("Individual row has error");
                      readSuccess = false;
                    }
                  }
                  break;
              }
            }

            if (!readSuccess)
            {
              errors.push_back("Error on Line " + QString::number(lineCount));
              file.close();
              return false;
            }
          }
        }

        file.close();
      }

      for(size_t i = 0; i < m_associatedFiles.size(); i++)
      {
        QFileInfo associatedFile = m_associatedFiles[i];
        associatedFile = getAbsoluteFilePath(associatedFile.filePath());
        m_associatedFiles[i] = associatedFile;

        if(!associatedFile.exists())
        {
          errors.push_back("Associated file does not exist");
          return false;
        }
      }

      createIndividuals();

      return true;
    }
    else
    {
      errors.push_back("Project file is invalid");
    }
  }
  else
  {
    errors.push_back("Project file is invalid");
  }


  return false;
}

void ExperimentalSimulation::execute()
{
  div_t inds_per_mpi_task = div(m_individuals.size(), HydroCoupleProject::numMPIProcesses);

#ifdef USE_OPENMP
#pragma omp parallel for
#endif
  for(int p = 1; p < HydroCoupleProject::numMPIProcesses; p++)
  {
    int start = inds_per_mpi_task.quot + inds_per_mpi_task.rem + (p - 1) * inds_per_mpi_task.quot;

    for(int j = start; j < start + inds_per_mpi_task.quot; j++)
    {
      m_individuals[j]->execute(p);
    }
  }

#ifdef USE_OPENMP
#pragma omp parallel for
#endif
  for(int i = 0; i < inds_per_mpi_task.quot + inds_per_mpi_task.rem; i++)
  {
    m_individuals[i]->execute();
  }
}

QFileInfo ExperimentalSimulation::inputFile() const
{
  return m_inputFile;
}

std::vector<QFileInfo> ExperimentalSimulation::associatedFiles() const
{
  return m_associatedFiles;
}

std::vector<std::string> ExperimentalSimulation::variables() const
{
  return m_variables;
}

std::string ExperimentalSimulation::variable(int individual, int variableIndex)
{
  return m_variableValues[individual][variableIndex];
}

QFileInfo ExperimentalSimulation::getAbsoluteFilePath(const QString &filePath)
{
  QFileInfo inputFile(filePath);

  if(inputFile.isRelative())
  {
    QDir dir(QDir::currentPath());
    inputFile = dir.absoluteFilePath(inputFile.filePath());
  }

  return inputFile;
}

void ExperimentalSimulation::createIndividuals()
{
  for(size_t i = 0; i < m_variableValues.size(); i++)
  {
    QString inputFileString = processString(i, m_inputFile.absoluteFilePath());
    QFileInfo inputFile(inputFileString);
    QString suffix = "." + inputFile.completeSuffix();
    inputFileString = inputFile.absoluteFilePath().replace(suffix,"") + "_" + QString::number(i) + suffix;
    inputFile = QFileInfo(inputFileString);

    if(QFile::exists(inputFile.absoluteFilePath()))
    {
      QFile::remove(inputFile.absoluteFilePath());
    }

    QFile::copy(m_inputFile.absoluteFilePath(), inputFile.absoluteFilePath());
    processFile(i, inputFile);


    std::vector<QFileInfo> values;

    for(size_t j = 0 ; j < m_associatedFiles.size(); j++)
    {
      QString assocInputFileString = processString(i, m_associatedFiles[j].absoluteFilePath());
      QFileInfo assocInputFile(assocInputFileString);
      assocInputFile = QFileInfo(assocInputFileString);

      if(QFile::exists(assocInputFile.absoluteFilePath()))
      {
        QFile::remove(assocInputFile.absoluteFilePath());
      }

      QFile::copy(m_associatedFiles[j].absoluteFilePath(), assocInputFile.absoluteFilePath());
      values.push_back(assocInputFile);
      processFile(i, assocInputFile);
    }

    ExperimentalSimulationIndividual *expInd = new ExperimentalSimulationIndividual(inputFile, values, this);
    m_individuals.push_back(expInd);
  }
}

void ExperimentalSimulation::processFile(int individualIndex, const QFileInfo &inputFile)
{
  QByteArray fileData;
  QFile file(inputFile.absoluteFilePath());
  file.open(QIODevice::ReadOnly); // open for read and write
  fileData = file.readAll(); // read all the data into the byte array
  file.close();

  QString text(fileData); // add to text string for easy string replace
  QString replaced = processString(individualIndex, text);

  QFile fileOutput(inputFile.absoluteFilePath());
  fileOutput.open(QIODevice::WriteOnly | QIODevice::Truncate);
  fileOutput.write(replaced.toUtf8()); // write the new text back to the file

  fileOutput.close(); // close the file handle.

}

QString ExperimentalSimulation::processString(int individualIndex, const QString &inputString)
{
  QString outputString = inputString;
  std::vector<std::string> & individualValues = m_variableValues[individualIndex];

  for(size_t i = 0; i < m_variables.size(); i++)
  {
    QString flag = QString::fromStdString(m_variables[i]);
    QString replacement = QString::fromStdString(individualValues[i]);
    outputString = outputString.replace(flag,replacement);
  }

  return outputString;
}

ExperimentalSimulationIndividual::ExperimentalSimulationIndividual(const QFileInfo &inputFile,
                                                                   const std::vector<QFileInfo> &associatedFiles,
                                                                   ExperimentalSimulation *parentSimulation)
  : QObject(parentSimulation),
    m_inputFile(inputFile),
    m_associatedFiles(associatedFiles),
    m_parentSimulation(parentSimulation)
{

}

ExperimentalSimulationIndividual::~ExperimentalSimulationIndividual()
{

}

QFileInfo ExperimentalSimulationIndividual::inputFile() const
{
  return m_inputFile;
}

std::vector<QFileInfo> ExperimentalSimulationIndividual::associatedFiles() const
{
  return m_associatedFiles;
}

void ExperimentalSimulationIndividual::execute(int mpiProcess)
{
  if(mpiProcess == 0)
  {
    QList<QString> messages;

    HydroCoupleProject *hydroCoupleProject = nullptr;


#ifdef USE_OPENMP
#pragma omp critical (ReadingHydroCoupleProject)
#endif
    {
      hydroCoupleProject = HydroCoupleProject::readProjectFile(m_inputFile.absoluteFilePath(), messages,true,false);
    }

    if(messages.length())
    {
      printf("Error messages.\n");

      for(QString message : messages)
      {
        printf("%s\n", qPrintable(message));
      }
    }

    printf("Creating simulation manager...\n");
    SimulationManager *simulationManager = new SimulationManager(hydroCoupleProject);
    simulationManager->setMonitorComponentMessages(true);

    printf("Running composition...\n");
    simulationManager->runComposition();
  }
  else
  {
#ifdef USE_MPI
    std::vector<char>initializeArgs(m_inputFile.absoluteFilePath().size() + 1);
    std::strcpy (initializeArgs.data(), m_inputFile.absoluteFilePath().toStdString().c_str());
    MPI_Send(initializeArgs.data(), initializeArgs.size() + 1,MPI_CHAR, mpiProcess, -1, MPI_COMM_WORLD);
#endif
  }
}

const std::unordered_map<std::string, int> ExperimentalSimulation::m_inputFileFlags({
                                                                                      {"[INPUTFILE]", 1},
                                                                                      {"[ASSOCIATEDFILES]", 2},
                                                                                      {"[VARIABLES]", 3},
                                                                                      {"[INDIVIDUALS]", 4},
                                                                                    });

