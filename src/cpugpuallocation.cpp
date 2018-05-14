#include "cpugpuallocation.h"
#include "gmodelcomponent.h"

CPUGPUAllocation::CPUGPUAllocation(QObject *parent)
  :QObject(parent)
{

}

int CPUGPUAllocation::mpiProcess() const
{
  return m_mpiProcess;
}

void CPUGPUAllocation::setMPIProcess(int mpiProcess)
{
  m_mpiProcess = mpiProcess;
  emit propertyChanged("MPIProcess");
}

int CPUGPUAllocation::gpuPlatform() const
{
  return m_gpuPlatform;
}

void CPUGPUAllocation::setGPUPlatform(int gpuPlatform)
{
  m_gpuPlatform = gpuPlatform;
  emit propertyChanged("GPUPlatform");
}

int CPUGPUAllocation::gpuDevice() const
{
  return m_gpuDevice;
}

void CPUGPUAllocation::setGPUDevice(int gpuDevice)
{
  m_gpuDevice = gpuDevice;
  emit propertyChanged("GPUDevice");
}

int CPUGPUAllocation::gpuMaxNumBlocksOrWorkGrps() const
{
  return m_gpuMaxNumBlocksOrWorkGrps;
}

void CPUGPUAllocation::setGPUMaxNumBlocksOrWorkGrps(int gpuMaxNumBlocksOrWorkGrps)
{
  m_gpuMaxNumBlocksOrWorkGrps = gpuMaxNumBlocksOrWorkGrps;
  emit propertyChanged("GPUMaxNumBlocksOrWorkGroups");
}

CPUGPUAllocation *CPUGPUAllocation::readCPUGPUAllocation(QXmlStreamReader &xmlReader, GModelComponent *parent, QList<QString> &errorMessages)
{
  CPUGPUAllocation *allocation = nullptr;

  if(!xmlReader.name().toString().compare("ComputeResourceAllocation",Qt::CaseInsensitive) && !xmlReader.hasError() && xmlReader.tokenType() == QXmlStreamReader::StartElement )
  {
    int tempValue = 0;
    bool convertible = false;

    QXmlStreamAttributes attributes = xmlReader.attributes();

    if(attributes.hasAttribute("MPI_Process"))
    {
      tempValue = attributes.value("MPI_Process").toInt(&convertible);

      if(convertible)
      {

        if(parent)
        {
          QList<CPUGPUAllocation*> parentAllocations = parent->allocatedComputeResources();

          for(CPUGPUAllocation *parentAllocation : parentAllocations)
          {
            if(parentAllocation->m_mpiProcess == tempValue)
            {
              allocation = parentAllocation;
              break;
            }
          }
        }

        if(allocation == nullptr)
        {
          allocation = new CPUGPUAllocation(parent);
          allocation->m_mpiProcess = tempValue;
        }

        if(attributes.hasAttribute("GPU_Platform"))
        {
          tempValue = attributes.value("GPU_Platform").toInt(&convertible);
          allocation->m_gpuPlatform = convertible ? tempValue : allocation->m_gpuPlatform;
        }

        if(attributes.hasAttribute("GPU_Device"))
        {
          tempValue = attributes.value("GPU_Device").toInt(&convertible);
          allocation->m_gpuDevice = convertible ? tempValue : allocation->m_gpuDevice;
        }

        if(attributes.hasAttribute("Maximum_Number_GPU_Blocks_Or_Workgroups"))
        {
          tempValue = attributes.value("Maximum_Number_GPU_Blocks_Or_Workgroups").toInt(&convertible);
          allocation->m_gpuMaxNumBlocksOrWorkGrps = convertible ? tempValue : allocation->m_gpuMaxNumBlocksOrWorkGrps;
        }
      }
    }
    else
    {
      errorMessages.append("MPI_Process needs to be specified");
    }

    while(!(xmlReader.isEndElement() && !xmlReader.name().toString().compare("ComputeResourceAllocation", Qt::CaseInsensitive)) && !xmlReader.hasError())
    {
      xmlReader.readNext();
    }
  }

  return allocation;
}

void CPUGPUAllocation::writeCPUGPUAllocation(QXmlStreamWriter &xmlWriter)
{
  xmlWriter.writeStartElement("ComputeResourceAllocation");
  {
    xmlWriter.writeAttribute("MPI_Process", QString::number(m_mpiProcess));
    xmlWriter.writeAttribute("GPU_Platform", QString::number(m_gpuPlatform));
    xmlWriter.writeAttribute("GPU_Device", QString::number(m_gpuDevice));
    xmlWriter.writeAttribute("Maximum_Number_GPU_Blocks_Or_Workgroups", QString::number(m_gpuMaxNumBlocksOrWorkGrps));
  }
  xmlWriter.writeEndElement();
}
