#ifndef EXPERIMENTALSIMULATION_H
#define EXPERIMENTALSIMULATION_H

#include <QFileInfo>
#include <unordered_map>

class ExperimentalSimulationIndividual;
class HydroCoupleProject;
class SimulationManager;

class ExperimentalSimulation  : public QObject
{
    Q_OBJECT

  public:

    ExperimentalSimulation(const QFileInfo &projectFile, QObject  *parent);

    ~ExperimentalSimulation();

    bool initialize(QList<QString> &errors);

    void execute();

    QFileInfo projectFile() const;

    QFileInfo inputFile() const;

    std::vector<QFileInfo> associatedFiles() const;

    std::vector<std::string> variables() const;

    std::string variable(int individual, int variableIndex);

    QFileInfo getAbsoluteFilePath(const QString &filePath);

  private:

    void createIndividuals();

    void processFile(int individualIndex, const QFileInfo &inputFile);

    QString processString(int individualIndex, const QString &inputString);

  private:
    QFileInfo m_projectFile;
    QFileInfo m_inputFile;
    std::vector<QFileInfo> m_associatedFiles;
    std::vector<std::string> m_variables;
    std::vector<std::vector<std::string>> m_variableValues;
    std::vector<ExperimentalSimulationIndividual*> m_individuals;

    static const std::unordered_map<std::string, int> m_inputFileFlags; //Input file flags

};

class ExperimentalSimulationIndividual : public QObject
{

    Q_OBJECT

  public:

    ExperimentalSimulationIndividual(const QFileInfo &inputFile, const std::vector<QFileInfo> &associatedFiles, ExperimentalSimulation *parentSimulation);

    ~ExperimentalSimulationIndividual();

    QFileInfo inputFile() const;

    std::vector<QFileInfo> associatedFiles() const;

    void execute(int mpiProcess = 0);

  private:

    QFileInfo m_inputFile;
    std::vector<QFileInfo> m_associatedFiles;
    ExperimentalSimulation *m_parentSimulation;
};

#endif // EXPERIMENTALSIMULATION_H
