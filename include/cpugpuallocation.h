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
 * Lesser GNU Lesser General Public License as published by the Free Software Foundation;
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
