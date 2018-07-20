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

#ifndef MODELSTATUSITEM_H
#define MODELSTATUSITEM_H

#include <QObject>
#include "modelstatuschangeeventarg.h"
#include "QModelIndex"

class ModelStatusItem : public QObject
{

      friend class ModelStatusItemModel;

      Q_OBJECT

   public:

      ModelStatusItem(HydroCouple::IModelComponent* component, ModelStatusItem* parent = nullptr);

      virtual ~ModelStatusItem();

      QString componenentId() const;

      QString componentCaption() const;

      HydroCouple::IModelComponent* component() const;

      ModelStatusChangeEventArg* status() const;

      QModelIndex index(int column = 0) const;

      QList<ModelStatusItem*> childModelStatusItems() const;

      ModelStatusItem* parentModelStatusItem() const;

   signals:

      /*!
       * \brief The componentStatusChanged() function must be implemented
       * as a signal and emitted when Status of the component changes.
       *
       * \details See HydroCouple::ComponentStatus for the possible states.
       */
      void componentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &statusChangedEvent);

      void propertyChanged();
      
      void childrenChanging();
      
      void childrenChanged();

   private:
      void resetChildren();


   private slots:
      void onComponentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &statusChangedEvent);

      void onPropertyChanged(const QString& propertyName);

   private:
      QModelIndex m_indexes[5];
      HydroCouple::IModelComponent* m_component;
      QList<ModelStatusItem*> m_children;
      ModelStatusChangeEventArg * m_status;
      ModelStatusItem* m_parent;
      QString m_componentId;
      QString m_componentCaption;
};




#endif // MODELSTATUSITEM_H

