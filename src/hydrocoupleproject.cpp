#include "stdafx.h"
#include "hydrocoupleproject.h"
#include "gmodelcomponent.h"

HydroCoupleProject::HydroCoupleProject(QObject *parent)
   : QObject(parent) , m_projectFile("Untitled Project.hcp"),m_hasChanges(false)
{
   m_componentManager = new ComponentManager(this);
}

HydroCoupleProject::~HydroCoupleProject()
{
   for(GModelComponent* component : m_modelComponents)
   {
      emit componentDeleting(component);
   }

   qDeleteAll(m_modelComponents);
   m_modelComponents.clear();
}

QFileInfo HydroCoupleProject::projectFile() const
{
   return m_projectFile;
}

ComponentManager* HydroCoupleProject::componentManager() const
{
   return m_componentManager;
}

QList<GModelComponent*> HydroCoupleProject::modelComponents() const
{
   return m_modelComponents;
}

GModelComponent* HydroCoupleProject::findModelComponentById(const QString &id)
{
   for(GModelComponent* modelComponent : m_modelComponents)
   {
      if(!modelComponent->modelComponent()->id().compare(id))
      {
         return modelComponent;
      }
   }

   return nullptr;
}

void HydroCoupleProject::addComponent(GModelComponent *component)
{

   if (!contains(component))
   {
      m_modelComponents.append(component);
      connect(component, SIGNAL(setAsTrigger(GModelComponent*)), this, SLOT(onTriggerChanged(GModelComponent*)));
      emit componentAdded(component);
      onSetHasChanges(true);
      postMessage("Added Component " +  component->modelComponent()->id() + "...");
      connect(component, SIGNAL(postMessage(QString)) , this , SLOT(onPostMessage(QString)));
      connect(component, SIGNAL(hasChanges()) , this , SLOT(onSetHasChanges()));
   }
}

bool HydroCoupleProject::deleteComponent(GModelComponent *component)
{
   if (m_modelComponents.removeAll(component))
   {
      disconnect(component, SIGNAL(postMessage(QString)) , this , SLOT(onPostMessage(QString)));
      emit componentDeleting(component);
      postMessage("Removed Component " +  component->modelComponent()->id() + "...");
      delete component;
      onSetHasChanges(true);
      return true;
   }
   else
   {
      return false;
   }
}

bool HydroCoupleProject::hasChanges() const
{
   return m_hasChanges;
}

HydroCoupleProject* HydroCoupleProject::readProjectFile(const QFileInfo &fileInfo , QList<QString>& errorMessages)
{
   HydroCoupleProject* project = new HydroCoupleProject(nullptr);
   project->m_projectFile = fileInfo;

   QFile file(fileInfo.absoluteFilePath());

   if(file.open(QIODevice::ReadOnly))
   {
      QXmlStreamReader xmlReader(&file);

      while(!xmlReader.isEndDocument() && !xmlReader.hasError())
      {
         if(!xmlReader.name().compare("HydroCoupleProject", Qt::CaseInsensitive) && !xmlReader.hasError() &&
               xmlReader.tokenType() == QXmlStreamReader::StartElement )
         {
            while (!(xmlReader.isEndElement() && !xmlReader.name().compare("HydroCoupleProject", Qt::CaseInsensitive)) && !xmlReader.hasError())
            {
               if(!xmlReader.name().compare("ModelComponents", Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
               {
                  while(!(xmlReader.isEndElement() && !xmlReader.name().compare("ModelComponents", Qt::CaseInsensitive)) && !xmlReader.hasError())
                  {
                     if(!xmlReader.name().compare("ModelComponent",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
                     {
                        GModelComponent* modelComponent = GModelComponent::readComponent(xmlReader, project, errorMessages);

                        if(modelComponent)
                        {
                           project->addComponent(modelComponent);
                        }
                     }
                     xmlReader.readNext();
                  }
               }
               else if(!xmlReader.name().compare("ModelComponentConnections", Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
               {
                  while(!(xmlReader.isEndElement() && !xmlReader.name().compare("ModelComponentConnections", Qt::CaseInsensitive)) && !xmlReader.hasError())
                  {
                     if(!xmlReader.name().compare("ModelComponentConnection",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
                     {
                        QXmlStreamAttributes attributes = xmlReader.attributes();

                        if(attributes.hasAttribute("SourceModelComponentIndex"))
                        {
                           QStringRef id = attributes.value("SourceModelComponentIndex");
                           bool idIsValid;
                           int componentIndex = id.toInt(&idIsValid);

                           if(idIsValid && componentIndex < project->m_modelComponents.length())
                           {
                              GModelComponent* modelComponent = project->m_modelComponents[componentIndex];

                              try
                              {
                                 modelComponent->readComponentConnections(xmlReader, errorMessages);
                              }
                              catch(std::exception e)
                              {
                                 errorMessages.append(QString(e.what()));
                                 return nullptr;
                              }
                              catch(std::string message)
                              {
                                errorMessages.append(QString::fromStdString( message));
                                return nullptr;
                              }
                              catch(...)
                              {

                                return nullptr;
                              }
                           }
                           else
                           {
                              errorMessages.append("ModelComponent was not found with specified index "+ id.toString() +" : Column " + QString::number(xmlReader.columnNumber()) + " Row " + QString::number(xmlReader.lineNumber()));
                           }
                        }
                        else
                        {
                           errorMessages.append("ModelComponentIndex attribute was not found : Column " + QString::number(xmlReader.columnNumber()) + " Row " + QString::number(xmlReader.lineNumber()));
                           return nullptr;
                        }

                     }
                     xmlReader.readNext();
                  }
               }
               xmlReader.readNext();
            }
         }
         xmlReader.readNext();
      }
   }

   project->onSetHasChanges(false);
   return project;
}

void HydroCoupleProject::onTriggerChanged(GModelComponent* triggerModelComponent)
{
   for (QList<GModelComponent*>::Iterator it = m_modelComponents.begin();
        it != m_modelComponents.end(); it++)
   {
      if (*it != triggerModelComponent)
      {
         (*it)->setTrigger(false);
      }
   }
}

void HydroCoupleProject::onSaveProject()
{

   QFile file(m_projectFile.absoluteFilePath());

   if(file.open(QIODevice::WriteOnly | QIODevice::Truncate))
   {
      QXmlStreamWriter writer(&file);
      writer.setAutoFormatting(true);

      writer.writeStartDocument();
      {
         writer.writeStartElement("HydroCoupleProject");
         {
            writer.writeAttribute("name", m_projectFile.fileName());

            writer.writeStartElement("ModelComponents");
            {
               for(GModelComponent* modelComponent : m_modelComponents)
               {
                  modelComponent->writeComponent(writer);
               }
            }
            writer.writeEndElement();

            writer.writeStartElement("ModelComponentConnections");
            {
               for(GModelComponent* modelComponent : m_modelComponents)
               {
                  modelComponent->writeComponentConnections(writer);
               }
            }
            writer.writeEndElement();
         }
         writer.writeEndElement();
      }
      writer.writeEndDocument();
      file.close();
   }

   onSetHasChanges(false);
}

void HydroCoupleProject::onSaveProjectAs(const QFileInfo &file)
{
   m_projectFile = file;
   onSaveProject();
}

void HydroCoupleProject::onSetHasChanges(bool hasChanges)
{
   m_hasChanges = hasChanges;
   emit stateModified(m_hasChanges);
}

void HydroCoupleProject::onReloadConnections()
{
   if(m_projectFile.exists())
   {
      QList<QString> errorMessages;

      QFile file(m_projectFile.absoluteFilePath());

      if(file.open(QIODevice::ReadOnly))
      {
         QXmlStreamReader xmlReader(&file);

         while(!xmlReader.isEndDocument() && !xmlReader.hasError())
         {
            if(!xmlReader.name().compare("ModelComponentConnections", Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
            {
               while(!(xmlReader.isEndElement() && !xmlReader.name().compare("ModelComponentConnections", Qt::CaseInsensitive)) && !xmlReader.hasError())
               {
                  if(!xmlReader.name().compare("ModelComponentConnection",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
                  {
                     QXmlStreamAttributes attributes = xmlReader.attributes();

                     if(attributes.hasAttribute("SourceModelComponentIndex"))
                     {
                        QStringRef id = attributes.value("SourceModelComponentIndex");
                        bool idIsValid;
                        int componentIndex = id.toInt(&idIsValid);

                        if(idIsValid && componentIndex < m_modelComponents.length())
                        {
                           GModelComponent* modelComponent = m_modelComponents[componentIndex];

                           try
                           {
                              modelComponent->readComponentConnections(xmlReader, errorMessages);
                           }
                           catch(std::exception e)
                           {
                              errorMessages.append(QString(e.what()));
                              return ;
                           }
                        }
                        else
                        {
                           errorMessages.append("ModelComponent was not found with specified index "+ id.toString() +" : Column " + QString::number(xmlReader.columnNumber()) + " Row " + QString::number(xmlReader.lineNumber()));
                        }
                     }
                     else
                     {
                        errorMessages.append("ModelComponentIndex attribute was not found : Column " + QString::number(xmlReader.columnNumber()) + " Row " + QString::number(xmlReader.lineNumber()));
                        return ;
                     }

                  }
                  xmlReader.readNext();
               }
            }
            xmlReader.readNext();
         }
      }
   }

   setProgress(false,0);
}

bool HydroCoupleProject::contains(GModelComponent* component) const
{
   for (int i = 0; i < m_modelComponents.count(); i++)
   {
      GModelComponent* mcomponent = m_modelComponents[i];

      if (mcomponent == component)
      {
         return true;
         break;
      }
   }

   return false;
}

void HydroCoupleProject::onPostMessage(const QString &message)
{
   emit postMessage(message);
}
