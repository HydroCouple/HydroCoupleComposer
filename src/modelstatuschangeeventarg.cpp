#include "stdafx.h"
#include "modelstatuschangeeventarg.h"

using namespace HydroCouple;

ModelStatusChangeEventArg::ModelStatusChangeEventArg(QObject* parent)
   :QObject(parent) , m_hasProgress(false), m_message(""), m_percentProgress(0),
     m_currentStatus(HydroCouple::Created),
     m_previousStatus(HydroCouple::Created)

{


}

ModelStatusChangeEventArg::~ModelStatusChangeEventArg()
{

}

IModelComponent* ModelStatusChangeEventArg::component() const
{
   return m_component;
}

QString ModelStatusChangeEventArg::previousStatus() const
{
   return GModelComponent::modelComponentStatusAsString(m_previousStatus);
}

QString ModelStatusChangeEventArg::currentStatus() const
{
   return GModelComponent::modelComponentStatusAsString( m_currentStatus);
}

ComponentStatus ModelStatusChangeEventArg::status() const
{
   return m_currentStatus;
}


void ModelStatusChangeEventArg::setStatus(ComponentStatus status)
{
   m_currentStatus = status;
}

QString ModelStatusChangeEventArg::message() const
{
   return m_message;
}

bool ModelStatusChangeEventArg::hasProgressMonitor() const
{
   return m_hasProgress;
}

float ModelStatusChangeEventArg::percentProgress() const
{
   return m_percentProgress;
}

void ModelStatusChangeEventArg::setStatus(const QSharedPointer<IComponentStatusChangeEventArgs> &status)
{
   m_hasProgress = status->hasProgressMonitor();
   m_percentProgress = status->percentProgress();
   m_message = status->message();
   m_previousStatus = status->previousStatus();
   m_currentStatus = status->status();
   m_component = status->component();
}
