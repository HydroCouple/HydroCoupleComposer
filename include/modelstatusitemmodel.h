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

