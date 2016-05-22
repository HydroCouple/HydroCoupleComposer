#include "stdafx.h"
#include "modelstatusitemstatuschangeeventargswrapper.h"

using namespace HydroCouple;

ModelStatusItemStatusChangeEventArgsWrapper::ModelStatusItemStatusChangeEventArgsWrapper(QObject* parent)
   :QObject(parent) , m_hasProgress(false), m_message(""), m_percentProgress(0),
     m_currentStatus(HydroCouple::Created),
     m_previousStatus(HydroCouple::Created)

{


}

ModelStatusItemStatusChangeEventArgsWrapper::~ModelStatusItemStatusChangeEventArgsWrapper()
{

}

IModelComponent* ModelStatusItemStatusChangeEventArgsWrapper::component() const
{
   return m_component;
}

QString ModelStatusItemStatusChangeEventArgsWrapper::previousStatus() const
{
   return GModelComponent::modelComponentStatusAsString(m_previousStatus);
}

QString ModelStatusItemStatusChangeEventArgsWrapper::currentStatus() const
{
   return GModelComponent::modelComponentStatusAsString( m_currentStatus);
}

ComponentStatus ModelStatusItemStatusChangeEventArgsWrapper::status() const
{
   return m_currentStatus;
}


void ModelStatusItemStatusChangeEventArgsWrapper::setStatus(ComponentStatus status)
{
   m_currentStatus = status;
}

QString ModelStatusItemStatusChangeEventArgsWrapper::message() const
{
   return m_message;
}

bool ModelStatusItemStatusChangeEventArgsWrapper::hasProgressMonitor() const
{
   return m_hasProgress;
}

float ModelStatusItemStatusChangeEventArgsWrapper::percentProgress() const
{
   return m_percentProgress;
}

void ModelStatusItemStatusChangeEventArgsWrapper::setStatus(const std::shared_ptr<IComponentStatusChangeEventArgs> &status)
{
   m_hasProgress = status->hasProgressMonitor();
   m_percentProgress = status->percentProgress();
   m_message = status->message();
   m_previousStatus = status->previousStatus();
   m_currentStatus = status->status();
   m_component = status->component();
}
