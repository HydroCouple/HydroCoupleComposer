#ifndef MODELSTATUSITEMSTATUSCHANGEEVENTARGSWRAPPER_H
#define MODELSTATUSITEMSTATUSCHANGEEVENTARGSWRAPPER_H

#include "gmodelcomponent.h"

/*!
 * \brief The ComponentStatusChangeEventArgs class.
 */
class ModelStatusItemStatusChangeEventArgsWrapper : public QObject
{
      Q_OBJECT
      Q_PROPERTY(HydroCouple::IModelComponent* Component READ component)
      Q_PROPERTY(QString PreviousStatus READ previousStatus )
      Q_PROPERTY(QString CurrentStatus READ currentStatus)
      Q_PROPERTY(QString Message READ message)

   public:
      ModelStatusItemStatusChangeEventArgsWrapper(QObject* parent);

      virtual ~ModelStatusItemStatusChangeEventArgsWrapper();

      HydroCouple::IModelComponent* component() const ;

      QString previousStatus() const ;

      QString currentStatus() const;

      HydroCouple::ComponentStatus status() const;

      QString message() const ;

      bool hasProgressMonitor() const ;

      float percentProgress() const ;

      void setStatus(const HydroCouple::IComponentStatusChangeEventArgs & status);

   private:
      bool m_hasProgress;
      QString m_message;
      float m_percentProgress;
      HydroCouple::IModelComponent* m_component;
      HydroCouple::ComponentStatus m_previousStatus, m_currentStatus;
};



#endif // MODELSTATUSITEMSTATUSCHANGEEVENTARGSWRAPPER_H

