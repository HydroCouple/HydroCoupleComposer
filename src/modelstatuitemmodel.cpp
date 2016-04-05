#include "modelstatusitemmodel.h"


ModelStatusItemModel::ModelStatusItemModel(QObject *parent)
   :QAbstractItemModel(parent)
{

}

ModelStatusItemModel::~ModelStatusItemModel()
{

}

int ModelStatusItemModel::columnCount(const QModelIndex &parent) const
{
   return 4;
}

QVariant  ModelStatusItemModel::data(const QModelIndex &index, int role) const
{
   return QVariant();
}


QModelIndex ModelStatusItemModel::index(int row, int column, const QModelIndex &parent) const
{
   return QModelIndex();
}

QModelIndex ModelStatusItemModel::parent(const QModelIndex &index) const
{
   return QModelIndex();
}


int ModelStatusItemModel::rowCount(const QModelIndex &parent) const
{
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
                     return "Component";
                     break;
                  case 1:
                     return "Status";
                     break;
                  case 2:
                     return "Progress";
                     break;
                  case 3:
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
