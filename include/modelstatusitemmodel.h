#ifndef MODELSTATUSITEMMODEL
#define MODELSTATUSITEMMODEL

#include <QAbstractItemModel>
#include "hydrocouple.h"

class ModelStatusItemModel : public QAbstractItemModel
{

   public:
      ModelStatusItemModel(QObject* parent = nullptr);

      virtual ~ModelStatusItemModel();

      int columnCount(const QModelIndex &parent = QModelIndex()) const override;

      QVariant	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

      QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;

      QModelIndex parent(const QModelIndex &index) const override;

      int rowCount(const QModelIndex &parent = QModelIndex()) const override;

      QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;


   private:
      QHash<HydroCouple::IModelComponentInfo*, HydroCouple::IComponentStatusChangeEventArgs &> m_models;


};

#endif // MODELSTATUSITEMMODEL

