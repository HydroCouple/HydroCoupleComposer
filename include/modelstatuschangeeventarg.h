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

