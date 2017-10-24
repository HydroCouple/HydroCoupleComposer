#ifndef CPUGPUALLOCATION_H
#define CPUGPUALLOCATION_H

#include <QObject>
#include <QXmlStreamReader>

class GModelComponent;

class CPUGPUAllocation : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int MPIProcess READ mpiProcess WRITE setMPIProcess NOTIFY propertyChanged)
    Q_PROPERTY(int GPUPlatform READ gpuPlatform WRITE setGPUPlatform NOTIFY propertyChanged)
    Q_PROPERTY(int GPUDevice READ gpuDevice WRITE setGPUDevice NOTIFY propertyChanged)
    Q_PROPERTY(int GPUMaxNumBlocksOrWorkGroups READ gpuMaxNumBlocksOrWorkGrps WRITE setGPUMaxNumBlocksOrWorkGrps NOTIFY propertyChanged)

  public:

    Q_INVOKABLE CPUGPUAllocation(QObject *parent = 0);

    int mpiProcess() const;

    void setMPIProcess(int mpiProcess);

    int gpuPlatform() const;

    void setGPUPlatform(int gpuPlatform);

    int gpuDevice() const;

    void setGPUDevice(int gpuDevice);

    int gpuMaxNumBlocksOrWorkGrps() const;

    void setGPUMaxNumBlocksOrWorkGrps(int gpuMaxNumBlocksOrWorkGrps);

    static CPUGPUAllocation *readCPUGPUAllocation(QXmlStreamReader &xmlReader, GModelComponent* parent, QList<QString>& errorMessages);

    void writeCPUGPUAllocation(QXmlStreamWriter& xmlWriter);

  signals:

    void propertyChanged(const QString& propertyName);

  private:
    int m_mpiProcess = 0;
    int m_gpuPlatform = -1;
    int m_gpuDevice = -1;
    int m_gpuMaxNumBlocksOrWorkGrps = 0;
};

Q_DECLARE_METATYPE(CPUGPUAllocation*)
Q_DECLARE_METATYPE(QList<CPUGPUAllocation*>)


#endif // CPUGPUALLOCATION_H
