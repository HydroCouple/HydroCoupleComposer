#include "modelstatusitemmodel.h"
#include "gmodelcomponent.h"
#include <QDebug>
#include <QApplication>
#include <QProgressBar>

using namespace HydroCouple;

ModelStatusItemStyledItemDelegate::ModelStatusItemStyledItemDelegate(QObject *parent)
   :QStyledItemDelegate(parent)
{

}

ModelStatusItemStyledItemDelegate::~ModelStatusItemStyledItemDelegate()
{

}

void ModelStatusItemStyledItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   ModelStatusItem* item = nullptr;

   if(index.column() == 3 && (item = static_cast<ModelStatusItem*>(index.internalPointer())))
   {


      QStyleOptionProgressBar progressStyle;
      progressStyle.rect = option.rect; // Maybe some other initialization from option would be needed
      progressStyle.textVisible = true;

#ifdef QT_DEBUG
      progressStyle.minimum = 0;
      progressStyle.maximum = 100;
      progressStyle.progress = item->status()->percentProgress();
#else
      if(item->status()->status() == ComponentStatus::Initializing ||
            item->status()->status() == ComponentStatus::Validating ||
            item->status()->status() == ComponentStatus::WaitingForData ||
            item->status()->status() == ComponentStatus::Preparing ||
            item->status()->status() == ComponentStatus::Updating ||
            item->status()->status() == ComponentStatus::Updated ||
            item->status()->status() == ComponentStatus::Done ||
            item->status()->status() == ComponentStatus::Finishing
            )
      {
         if(item->status()->hasProgressMonitor())
         {
            progressStyle.minimum = 0;
            progressStyle.maximum = 100;
            progressStyle.progress = item->status()->percentProgress();
         }
         else
         {
            progressStyle.minimum = 0;
            progressStyle.maximum = 0;
            progressStyle.progress = 0;
            progressStyle.text = "Busy...";
         }
      }
      else
      {


         progressStyle.minimum = 0;
         progressStyle.maximum = 100;
         progressStyle.progress = 0;
         progressStyle.text = item->status()->currentStatus();
      }
#endif
      QApplication::style()->drawControl(QStyle::CE_ProgressBar, &progressStyle , painter);
      return;
   }

   QStyledItemDelegate::paint(painter , option , index);
}

QSize ModelStatusItemStyledItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   QSize size = QStyledItemDelegate::sizeHint(option,index);

   if(index.column() == 3 && size.width() < 500)
   {
      size.setWidth(500);
   }
   return size;
}

//=====================================================================================================================

ModelStatusItemModel::ModelStatusItemModel(QObject *parent)
   :QAbstractItemModel(parent) , m_timeOut(250) , m_useTimerUpdate(true)
{
   m_timer = new QTimer(this);
   connect(m_timer , SIGNAL(timeout()) , this , SLOT(onUpdateStatus()));
   m_timer->start(m_timeOut);
}

ModelStatusItemModel::~ModelStatusItemModel()
{
   // qDeleteAll(m_models);
   // m_models.clear();
}

int ModelStatusItemModel::columnCount(const QModelIndex &parent) const
{
   return 5;
}

QVariant ModelStatusItemModel::data(const QModelIndex &index, int role) const
{
   ModelStatusItem* status;

   if(index.isValid() && (status = static_cast<ModelStatusItem*>(index.internalPointer())) && role == Qt::DisplayRole)
   {
      switch (index.column())
      {
         case 0:
            return status->component()->id();
            break;
         case 1:
            return status->component()->caption();
            break;
         case 2:
            return status->status()->currentStatus();
            break;
         case 3:
            return status->status()->percentProgress();
            break;
         case 4:
            return status->status()->message();
            break;
      }
   }

   return QVariant();
}

QModelIndex ModelStatusItemModel::index(int row, int column, const QModelIndex &parent) const
{
   QModelIndex index;
   ModelStatusItem* status;

   if(parent.isValid())
   {
      if((status = static_cast<ModelStatusItem*>(parent.internalPointer())))
      {
         ModelStatusItem* child = status->childModelStatusItems()[row];
         index = createIndex(row , column , child);
         child->m_indexes[column] = index;
      }
   }
   else if(row < m_models.length())
   {
      ModelStatusItem* child =m_models[row];
      index = createIndex(row , column , child);
      child->m_indexes[column] = index;
   }

   return index;
}

QModelIndex ModelStatusItemModel::parent(const QModelIndex &index) const
{
   ModelStatusItem* item = nullptr;

   if(index.isValid())
   {
      if((item = static_cast<ModelStatusItem*>(index.internalPointer()))
            && item->parentModelStatusItem() && item->parentModelStatusItem()->m_indexes[0].isValid())
      {
         return item->parentModelStatusItem()->m_indexes[0];
      }
   }

   return QModelIndex();
}

int ModelStatusItemModel::rowCount(const QModelIndex &parent) const
{
   ModelStatusItem* item = nullptr;

   qDebug() << parent;

   if(parent.isValid())
   {
      if((item = static_cast<ModelStatusItem*>(parent.internalPointer())))
      {
         return item->childModelStatusItems().length();
      }
   }
   else
   {
      return m_models.length();
   }

   return 0;
}

QVariant ModelStatusItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   switch (orientation)
   {
      case Qt::Horizontal:
         switch (role)
         {
            case Qt::DisplayRole:

               switch (section)
               {
                  case 0:
                     return "Id";
                     break;
                  case 1:
                     return "Caption";
                     break;
                  case 2:
                     return "Status";
                     break;
                  case 3:
                     return "Progress";
                     break;
                  case 4:
                     return "Message";
                     break;
                  default:
                     break;
               }
               break;
            default:
               return QVariant();
               break;
         }
         break;
      default:
         return QVariant();
         break;
   }
}

void ModelStatusItemModel::addNewModel(HydroCouple::IModelComponent * modelComponent)
{

   beginResetModel();

   for(ModelStatusItem* item : m_models)
   {
      if(item->component() == modelComponent)
      {
         return ;
      }
   }

   ModelStatusItem* item = new ModelStatusItem(modelComponent , nullptr);
   m_models.append(item);

   createSignalSlotConnections(item);

   endResetModel();

}

void ModelStatusItemModel::removeModel(IModelComponent *modelComponent)
{
   int row = 0;

   for(ModelStatusItem* item : m_models)
   {
      if(item->component() == modelComponent)
      {

         beginResetModel();//QModelIndex() , row , row);

         m_models.removeAll(item);
         disconnectSignalSlotConnections(item);

         endResetModel();

         return ;
      }

      row ++;
   }
}

bool ModelStatusItemModel::useTimerUpdate() const
{
   return m_useTimerUpdate;
}

void ModelStatusItemModel::setUserTimerUpdate(bool useTimerUpdate)
{
   m_useTimerUpdate = useTimerUpdate;

   if(m_useTimerUpdate)
   {
      m_timer->start(m_timeOut);
   }
   else
   {
      m_timer->stop();
   }
}

int ModelStatusItemModel::updateTimeOut() const
{
   return m_timeOut;
}

void ModelStatusItemModel::setUpdateTimeOut(int timeout)
{
   m_timer->setInterval(timeout);
}

void ModelStatusItemModel::onComponentStatusChanged(const IComponentStatusChangeEventArgs &statusChangedEvent)
{
   if(!m_useTimerUpdate)
   {
      ModelStatusItem* senderItem = static_cast<ModelStatusItem*>(sender());
      emit dataChanged(senderItem->m_indexes[0], senderItem->m_indexes[4]);
   }
}

void ModelStatusItemModel::onModelStatusItemPropertyChanged()
{
   ModelStatusItem* senderItem = static_cast<ModelStatusItem*>(sender());
   emit dataChanged(senderItem->m_indexes[0], senderItem->m_indexes[4]);
}

void ModelStatusItemModel::onChildrenChanging()
{
   ModelStatusItem* senderItem = static_cast<ModelStatusItem*>(sender());
   beginRemoveRows(senderItem->m_indexes[0] , 0 , senderItem->m_children.length() - 1);
}

void ModelStatusItemModel::onChildrenChanged()
{
   ModelStatusItem* senderItem = static_cast<ModelStatusItem*>(sender());

   for(ModelStatusItem* item : senderItem->m_children)
   {
      createSignalSlotConnections(item);
   }
   endRemoveRows();
}

void ModelStatusItemModel::onUpdateStatus()
{
   emit dataChanged(QModelIndex() , QModelIndex());
}

void ModelStatusItemModel::createSignalSlotConnections(ModelStatusItem *modelStatusItem)
{
   connect(modelStatusItem , SIGNAL(componentStatusChanged(const HydroCouple::IComponentStatusChangeEventArgs &)) ,
           this , SLOT(onComponentStatusChanged(const HydroCouple::IComponentStatusChangeEventArgs &)));

   connect(modelStatusItem , SIGNAL(propertyChanged()) ,
           this , SLOT(onModelStatusItemPropertyChanged()));

   connect(modelStatusItem , SIGNAL(childrenChanging()) ,
           this , SLOT(onChildrenChanging()));

   connect(modelStatusItem , SIGNAL(childrenChanged()) ,
           this , SLOT(onChildrenChanged()));

   for(ModelStatusItem* item : modelStatusItem->m_children)
   {
      createSignalSlotConnections(item);
   }
}

void ModelStatusItemModel::disconnectSignalSlotConnections(ModelStatusItem *modelStatusItem)
{
   disconnect(modelStatusItem , SIGNAL(componentStatusChanged(const HydroCouple::IComponentStatusChangeEventArgs &)) ,
              this , SLOT(onComponentStatusChanged(const HydroCouple::IComponentStatusChangeEventArgs &)));

   disconnect(modelStatusItem , SIGNAL(propertyChanged()) ,
              this , SLOT(onModelStatusItemPropertyChanged()));

   disconnect(modelStatusItem , SIGNAL(childrenChanging()) ,
              this , SLOT(onChildrenChanging()));

   disconnect(modelStatusItem , SIGNAL(childrenChanged()) ,
              this , SLOT(onChildrenChanged()));

   for(ModelStatusItem* item : modelStatusItem->childModelStatusItems())
   {
      disconnectSignalSlotConnections(item);
   }
}
