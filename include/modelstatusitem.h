#ifndef MODELSTATUSITEM_H
#define MODELSTATUSITEM_H

#include <QObject>
#include "modelstatusitemstatuschangeeventargswrapper.h"
#include "QModelIndex"

class ModelStatusItem : public QObject
{

      friend class ModelStatusItemModel;

      Q_OBJECT

   public:

      ModelStatusItem(HydroCouple::IModelComponent* component, ModelStatusItem* parent = nullptr);

      virtual ~ModelStatusItem();

      HydroCouple::IModelComponent* component() const;

      ModelStatusItemStatusChangeEventArgsWrapper* status() const;

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
      void componentStatusChanged(const HydroCouple::IComponentStatusChangeEventArgs& statusChangedEvent);

      void propertyChanged();
      
      void childrenChanging();
      
      void childrenChanged();

   private:
      void resetChildren();


   private slots:
      void onComponentStatusChanged(const HydroCouple::IComponentStatusChangeEventArgs& statusChangedEvent);

      void onPropertyChanged(const QString& propertyName, const QVariant& value);

   private:
      QModelIndex m_indexes[5];
      HydroCouple::IModelComponent* m_component;
      QList<ModelStatusItem*> m_children;
      ModelStatusItemStatusChangeEventArgsWrapper * m_status;
      ModelStatusItem* m_parent;
};




#endif // MODELSTATUSITEM_H

