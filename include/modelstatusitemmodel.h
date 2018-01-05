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

#ifndef MODELSTATUSITEMMODEL
#define MODELSTATUSITEMMODEL

#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include <QTimer>
#include "hydrocouple.h"
#include "modelstatusitem.h"


class ModelStatusItemStyledItemDelegate : public QStyledItemDelegate
{
      Q_OBJECT

   public:
      ModelStatusItemStyledItemDelegate(QObject* parent = nullptr);

      virtual ~ModelStatusItemStyledItemDelegate();

      void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

      QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

};

class ModelStatusItemModel : public QAbstractItemModel
{

      Q_OBJECT

   public:

      ModelStatusItemModel(QObject* parent = nullptr);

      virtual ~ModelStatusItemModel();

      int columnCount(const QModelIndex &parent = QModelIndex()) const override;

      QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

      QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;

      QModelIndex parent(const QModelIndex &index) const override;

      int rowCount(const QModelIndex &parent = QModelIndex()) const override;

      QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

      void addNewModel(HydroCouple::IModelComponent* modelComponent);

      void removeModel(HydroCouple::IModelComponent* modelComponent);

      bool useTimerUpdate() const;

      void setUserTimerUpdate(bool useTimerUpdate);

      int updateTimeOut() const;

      void setUpdateTimeOut(int timeout);

      QList<ModelStatusItem*> models() const;

   private slots:

      void onComponentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &statusChangedEvent);

      void onModelStatusItemPropertyChanged();

      void onChildrenChanging();
      
      void onChildrenChanged();

      void onUpdateStatus();

   private:
      void createSignalSlotConnections(ModelStatusItem *modelStatusItem);

      void disconnectSignalSlotConnections(ModelStatusItem *modelStatusItem);

   private:
      QList<ModelStatusItem*> m_models;
      bool m_useTimerUpdate;
      int m_timeOut;
      QTimer* m_timer;

};

#endif // MODELSTATUSITEMMODEL

