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

#ifndef MODELSTATUSCHANGEEVENTARG_H
#define MODELSTATUSCHANGEEVENTARG_H

#include "gmodelcomponent.h"

/*!
 * \brief The ModelStatusChangeEventArgsWrapper class.
 */
class ModelStatusChangeEventArg : public QObject
{
      Q_OBJECT
      Q_PROPERTY(HydroCouple::IModelComponent* Component READ component)
      Q_PROPERTY(QString PreviousStatus READ previousStatus )
      Q_PROPERTY(QString CurrentStatus READ currentStatus)
      Q_PROPERTY(QString Message READ message)

   public:
      ModelStatusChangeEventArg(QObject* parent);

      virtual ~ModelStatusChangeEventArg();

      HydroCouple::IModelComponent* component() const ;

      QString previousStatus() const ;

      QString currentStatus() const;

      HydroCouple::IModelComponent::ComponentStatus status() const;

      void setStatus(HydroCouple::IModelComponent::ComponentStatus status);

      QString message() const ;

      bool hasProgressMonitor() const ;

      float percentProgress() const ;

      void setStatus(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &status);

   private:
      bool m_hasProgress;
      QString m_message;
      float m_percentProgress;
      HydroCouple::IModelComponent* m_component;
      HydroCouple::IModelComponent::ComponentStatus m_currentStatus , m_previousStatus ;
};



#endif // MODELSTATUSCHANGEEVENTARG_H

