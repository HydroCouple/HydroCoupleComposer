#include "stdafx.h"
#include "gmodelcomponent.h"
#include "gdefaultselectiongraphic.h"
#include "cpugpuallocation.h"

#include <QGraphicsScene>
#include <QStyleOptionGraphicsItem>
#include <QTextDocument>

using namespace HydroCouple;

const QString GModelComponent::sc_descriptionHtml =
    "<h3><i>[CId] Instance</i></h3>"
    "<h2>[Caption]</h2>"
    "<h3>[Id]</h3>"
    "<h4><i>Status : [Status]</i></h4>"
    "<hr>"
    "<div>"
    "<img alt=\"icon\" src='[IconPath]' width=\"100\" align=\"left\" />"
    "<p align=\"center\">[Description]</p>"
    "</div>";

QBrush GModelComponent::s_triggerBrush(QColor(255, 255, 255), Qt::BrushStyle::SolidPattern);
QPen GModelComponent::s_triggerPen(QBrush(QColor(255, 0, 0), Qt::BrushStyle::SolidPattern), 5.0, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::RoundJoin);

GModelComponent::GModelComponent(IModelComponent* model, HydroCoupleProject *parent)
  : GNode(model->id(),model->caption(),GNode::Component, parent), m_isTrigger(false),
    m_moveExchangeItemsWhenMoved(true),
    m_readFromFile(false),
    m_ignoreSignalsFromComponent(false)
{

  setFlag(GraphicsItemFlag::ItemSendsScenePositionChanges, true);

  m_textItem->setFlag(GraphicsItemFlag::ItemIsMovable, false);
  m_textItem->setFlag(GraphicsItemFlag::ItemIsSelectable, false);
  m_textItem->setFlag(GraphicsItemFlag::ItemIsFocusable, false);

  m_parent = parent;
  m_modelComponent = model;

  qRegisterMetaType<QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs>>();
  qRegisterMetaType<QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs>>("QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs>");

  connect(dynamic_cast<QObject*>(m_modelComponent), SIGNAL(componentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &)),
          this, SLOT(onComponentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &)));

  connect(dynamic_cast<QObject*>(m_modelComponent), SIGNAL(propertyChanged(const QString &)), this, SLOT(onPropertyChanged(const QString&)));

  connect(this, &GNode::propertyChanged,this, &GModelComponent::onPropertyChanged);
  connect(this, &GNode::doubleClicked,this, &GModelComponent::onDoubleClicked);

  m_computeResourceAllocations.append(new CPUGPUAllocation(this));

  onCreateTextItem();
}

GModelComponent::~GModelComponent()
{
  deleteExchangeItems();

  project()->componentManager()->removeModelInstance(m_modelComponent->componentInfo()->id(), m_modelComponent);

  delete m_modelComponent;
}

HydroCoupleProject* GModelComponent::project() const
{
  return m_parent;
}

void GModelComponent::setProject(HydroCoupleProject *project)
{
  m_parent = project;
}

IModelComponent* GModelComponent::modelComponent() const
{
  return m_modelComponent;
}

QString GModelComponent::id() const
{
  return m_modelComponent->id();
}

QString GModelComponent::caption() const
{
  return m_modelComponent->caption();
}

QString GModelComponent::status() const
{
  return modelComponentStatusAsString( m_modelComponent->status());
}

bool GModelComponent::trigger() const
{
  return m_isTrigger;
}

void GModelComponent::setTrigger(bool trigger)
{
  if (m_isTrigger != trigger)
  {
    m_isTrigger = trigger;

    if (m_isTrigger)
    {
      emit setAsTrigger(this);
    }

    emit propertyChanged("Trigger");
    emit postMessage(m_modelComponent->id() + " has been set as the trigger" );
    emit hasChanges();
    update();
  }
}

QPen GModelComponent::triggerPen() const
{
  return s_triggerPen;
}

void GModelComponent::setTriggerPen(const QPen& triggerPen)
{
  s_triggerPen = triggerPen;
  emit propertyChanged("TriggerPen");
  emit hasChanges();
  update();
}

QBrush GModelComponent::triggerBrush() const
{
  return s_triggerBrush;
}

void GModelComponent::setTriggerBrush(const QBrush& triggerBrush)
{
  s_triggerBrush = triggerBrush;
  emit propertyChanged("TriggerBrush");
  emit hasChanges();
  update();
}

QList<CPUGPUAllocation*> GModelComponent::allocatedComputeResources() const
{
  return m_computeResourceAllocations;
}

void GModelComponent::allocateComputeResources(const QList<CPUGPUAllocation *> &allocatedComputeResources)
{
  m_computeResourceAllocations = allocatedComputeResources;
  emit propertyChanged("ComputeResources");
}

bool GModelComponent::moveExchangeItemsWhenMoved() const
{
  return m_moveExchangeItemsWhenMoved;
}

void GModelComponent::setMoveExchangeItemsWhenMoved(bool move)
{
  m_moveExchangeItemsWhenMoved = move;
}

bool GModelComponent::readFromFile() const
{
  return m_readFromFile;
}

QHash<QString,IInput*> GModelComponent::inputs() const
{
  return m_inputs;
}

QHash<QString, IOutput*> GModelComponent::outputs() const
{
  return m_outputs;
}

QHash<QString,GInput*> GModelComponent::inputGraphicObjects() const
{
  return m_inputGraphicObjects;
}

QHash<QString, GOutput*> GModelComponent::outputGraphicObjects() const
{
  return m_outputGraphicObjects;
}

QString GModelComponent::modelComponentStatusAsString(IModelComponent::ComponentStatus status)
{
  switch (status)
  {
    case IModelComponent::Created:
      return "Created";
      break;
    case IModelComponent::Initializing:
      return "Initializing";
      break;
    case IModelComponent::Initialized:
      return "Initialized";
      break;
    case IModelComponent::Validating:
      return "Validating";
      break;
    case IModelComponent::Valid:
      return "Valid";
      break;
    case IModelComponent::WaitingForData:
      return "WaitingForData";
      break;
    case IModelComponent::Invalid:
      return "Invalid";
      break;
    case IModelComponent::Preparing:
      return "Preparing";
      break;
    case IModelComponent::Updating:
      return "Updating";
      break;
    case IModelComponent::Updated:
      return "Updated";
      break;
    case IModelComponent::Done:
      return "Done";
      break;
    case IModelComponent::Finishing:
      return "Finishing";
      break;
    case IModelComponent::Finished:
      return "Finished";
      break;
    case IModelComponent::Failed:
      return "Failed";
      break;
    default:
      return	QString();
      break;
  }
}

GModelComponent* GModelComponent::readComponentFile(const QFileInfo & fileInfo, HydroCoupleProject* project,
                                                    QList<QString>& errorMessages, bool initialize)
{
  QFile file(fileInfo.absoluteFilePath());

  if(file.open(QIODevice::ReadOnly))
  {
    QXmlStreamReader xmlReader(&file);

    while(!xmlReader.isEndDocument() && !xmlReader.hasError())
    {
      if(!xmlReader.name().toString().compare("ModelComponent", Qt::CaseInsensitive) && !xmlReader.hasError() && xmlReader.tokenType() == QXmlStreamReader::StartElement)
      {
        while (!((xmlReader.isEndElement() || xmlReader.isEndDocument()) &&
                 !xmlReader.name().toString().compare("ModelComponent", Qt::CaseInsensitive)) && !xmlReader.hasError())
        {

          GModelComponent* modelComponent = readComponent(xmlReader, project, fileInfo.dir(), errorMessages, initialize);

          if(modelComponent)
          {
            modelComponent->m_readFromFile = true;
            modelComponent->m_exportFile = fileInfo;
            modelComponent->modelComponent()->setReferenceDirectory(fileInfo.dir().absolutePath());
            return modelComponent;
          }
        }
      }
      xmlReader.readNext();
    }
  }

  return nullptr;
}

GModelComponent* GModelComponent::readComponent(QXmlStreamReader &xmlReader, HydroCoupleProject* project,
                                                const QDir& referenceDir, QList<QString>& errorMessages, bool initialize)
{
  GModelComponent* modelComponent = nullptr;

  if(!xmlReader.name().toString().compare("ModelComponent", Qt::CaseInsensitive) &&
     !xmlReader.hasError() && xmlReader.tokenType() == QXmlStreamReader::StartElement)
  {

    QXmlStreamAttributes attributes = xmlReader.attributes();

    if(attributes.hasAttribute("ModelComponentFile"))
    {
      QStringRef value = attributes.value("ModelComponentFile");
      QFileInfo fileInfo(value.toString().trimmed());

      if(fileInfo.isRelative())
      {
        fileInfo = referenceDir.absoluteFilePath(fileInfo.filePath());
      }

      if(fileInfo.exists() && (modelComponent = readComponentFile(fileInfo, project, errorMessages, initialize)))
      {
        if(attributes.hasAttribute("IsTrigger"))
        {
          QString trigVal = attributes.value("IsTrigger").toString();

          if(!trigVal.compare("True" , Qt::CaseInsensitive))
          {
            modelComponent->setTrigger(true);
          }
        }

        if(attributes.hasAttribute("XPos") && attributes.hasAttribute("YPos"))
        {
          QString xposS = attributes.value("XPos").toString();
          QString yposS = attributes.value("YPos").toString();

          bool ok;

          double xloc = xposS.toDouble(&ok);
          double yloc = yposS.toDouble(&ok);

          if(ok)
          {
            modelComponent->setPos(xloc,yloc);
          }
        }

        if(attributes.hasAttribute("Caption"))
        {
          modelComponent->setCaption(attributes.value("Caption").toString());
        }
      }
      else
      {
        errorMessages.append("ModelComponentFile specified was not found : "+ fileInfo.absoluteFilePath() +" : Column " + QString::number(xmlReader.columnNumber()) + " Row " + QString::number(xmlReader.lineNumber()));
      }
    }
    else
    {
      if(attributes.hasAttribute("ModelComponentLibrary"))
      {
        QStringRef value = attributes.value("ModelComponentLibrary");
        QFileInfo fileInfo(value.toString().trimmed());

        if(fileInfo.isRelative() && project)
        {
          fileInfo = QFileInfo(referenceDir.absoluteFilePath(fileInfo.filePath()));
        }

        if(fileInfo.exists())
        {
          IModelComponentInfo* modelComponentInfo = dynamic_cast<IModelComponentInfo*>(project->m_componentManager->loadComponent(fileInfo));

          if(modelComponentInfo)
          {
            IModelComponent* component = modelComponentInfo->createComponentInstance();
            component->setReferenceDirectory(referenceDir.absolutePath());
            //printf("Process Rank: %i\n", component->mpiProcessRank());

            modelComponent = new GModelComponent(component, project);

            if(attributes.hasAttribute("IsTrigger"))
            {
              QString trigVal = attributes.value("IsTrigger").toString();

              if(!trigVal.compare("True" , Qt::CaseInsensitive))
              {
                modelComponent->setTrigger(true);
              }
            }

            if(attributes.hasAttribute("XPos") && attributes.hasAttribute("YPos"))
            {
              QString xposS = attributes.value("XPos").toString();
              QString yposS = attributes.value("YPos").toString();

              bool ok;

              double xloc = xposS.toDouble(&ok);
              double yloc = yposS.toDouble(&ok);

              if(ok)
              {
                modelComponent->setPos(xloc,yloc);
              }
            }

            if(attributes.hasAttribute("Caption"))
            {
              modelComponent->setCaption(attributes.value("Caption").toString());
            }

            while (!(xmlReader.isEndElement() && !xmlReader.name().toString().compare("ModelComponent", Qt::CaseInsensitive)) && !xmlReader.hasError())
            {
              if(!xmlReader.name().toString().compare("Arguments", Qt::CaseInsensitive) &&
                 !xmlReader.hasError() && xmlReader.tokenType() == QXmlStreamReader::StartElement)
              {
                while (!(xmlReader.isEndElement() && !xmlReader.name().toString().compare("Arguments", Qt::CaseInsensitive)) && !xmlReader.hasError())
                {
                  if(!xmlReader.name().toString().compare("Argument", Qt::CaseInsensitive) && !xmlReader.hasError()  &&
                     xmlReader.tokenType() == QXmlStreamReader::StartElement)
                  {
                    readArgument(xmlReader, component);

                    while (!(xmlReader.isEndElement() && !xmlReader.name().toString().compare("Argument", Qt::CaseInsensitive)) && !xmlReader.hasError())
                    {
                      xmlReader.readNext();
                    }
                  }
                  xmlReader.readNext();
                }

                //                if(initialize)
                //                {
                //                  //printf("Process Rank: %i\n", component->mpiProcessRank());
                //                  component->initialize();
                //                }
              }
              else if(!xmlReader.name().toString().compare("ComputeResourceAllocations", Qt::CaseInsensitive) &&
                      !xmlReader.hasError() && xmlReader.tokenType() == QXmlStreamReader::StartElement)
              {
                while (!(xmlReader.isEndElement() && !xmlReader.name().toString().compare("ComputeResourceAllocations", Qt::CaseInsensitive)) && !xmlReader.hasError())
                {
                  if(!xmlReader.name().toString().compare("ComputeResourceAllocation", Qt::CaseInsensitive) && !xmlReader.hasError()  &&
                     xmlReader.tokenType() == QXmlStreamReader::StartElement)
                  {
                    CPUGPUAllocation *cpugpuAllocation = CPUGPUAllocation::readCPUGPUAllocation(xmlReader, modelComponent, errorMessages);

                    if(cpugpuAllocation != nullptr && !modelComponent->m_computeResourceAllocations.contains(cpugpuAllocation))
                    {
                      modelComponent->m_computeResourceAllocations.push_back(cpugpuAllocation);
                    }
                  }
                  xmlReader.readNext();
                }
              }

              xmlReader.readNext();
            }
          }
          else
          {
            errorMessages.append("Unable to load component specified at: "+ fileInfo.absoluteFilePath() +" : Column " + QString::number(xmlReader.columnNumber()) + " Row " + QString::number(xmlReader.lineNumber()));
          }
        }
        else
        {
          errorMessages.append("ModelComponentFile specified was not found : "+ fileInfo.absoluteFilePath() +" : Column " + QString::number(xmlReader.columnNumber()) +  " Row " + QString::number(xmlReader.lineNumber()));
        }
      }
    }

    while (!(xmlReader.isEndElement() && !xmlReader.name().toString().compare("ModelComponent", Qt::CaseInsensitive)) && !xmlReader.hasError())
    {

      if(modelComponent)
      {
        if(!xmlReader.name().toString().compare("ExchangeItemPositions", Qt::CaseInsensitive) &&
           !xmlReader.hasError() && xmlReader.tokenType() == QXmlStreamReader::StartElement )
        {

          while (!(xmlReader.isEndElement() &&
                   !xmlReader.name().toString().compare("ExchangeItemPositions", Qt::CaseInsensitive)) && !xmlReader.hasError())
          {

            if(!xmlReader.name().toString().compare("Outputs", Qt::CaseInsensitive) &&
               !xmlReader.hasError() && xmlReader.tokenType() == QXmlStreamReader::StartElement )
            {
              while (!(xmlReader.isEndElement() &&
                       !xmlReader.name().toString().compare("Outputs", Qt::CaseInsensitive)) && !xmlReader.hasError())
              {

                if(!xmlReader.name().toString().compare("Output", Qt::CaseInsensitive) &&
                   !xmlReader.hasError() && xmlReader.tokenType() == QXmlStreamReader::StartElement )
                {
                  attributes = xmlReader.attributes();

                  if(attributes.hasAttribute("Id"))
                  {
                    QString id = attributes.value("Id").toString();

                    if(modelComponent->m_outputGraphicObjects.contains(id))
                    {
                      GOutput* output = modelComponent->m_outputGraphicObjects[id];

                      if(attributes.hasAttribute("XPos") && attributes.hasAttribute("YPos"))
                      {
                        QString xposS = attributes.value("XPos").toString();
                        QString yposS = attributes.value("YPos").toString();

                        bool ok;

                        double xloc = xposS.toDouble(&ok);
                        double yloc = yposS.toDouble(&ok);

                        if(ok)
                        {
                          output->setPos(xloc,yloc);
                        }
                      }

                      if(attributes.hasAttribute("Caption"))
                      {
                        output->setCaption(attributes.value("Caption").toString());
                      }
                    }
                    else
                    {
                      GOutput* output = new GOutput(id, modelComponent);
                      modelComponent->m_outputGraphicObjects[id] = output;
                      QString err;
                      if(!modelComponent->createConnection(output, err))
                        errorMessages.push_back(err);

                      if(attributes.hasAttribute("XPos") && attributes.hasAttribute("YPos"))
                      {
                        QString xposS = attributes.value("XPos").toString();
                        QString yposS = attributes.value("YPos").toString();

                        bool ok;

                        double xloc = xposS.toDouble(&ok);
                        double yloc = yposS.toDouble(&ok);

                        if(ok)
                        {
                          output->setPos(xloc,yloc);
                        }
                      }

                      if(attributes.hasAttribute("Caption"))
                      {
                        output->setCaption(attributes.value("Caption").toString());
                      }
                    }
                  }
                }

                xmlReader.readNext();
              }
            }
            else if(!xmlReader.name().toString().compare("Inputs", Qt::CaseInsensitive) &&
                    !xmlReader.hasError() && xmlReader.tokenType() == QXmlStreamReader::StartElement )
            {
              while (!(xmlReader.isEndElement() &&
                       !xmlReader.name().toString().compare("Inputs", Qt::CaseInsensitive)) && !xmlReader.hasError())
              {

                if(!xmlReader.name().toString().compare("Input", Qt::CaseInsensitive) &&
                   !xmlReader.hasError() && xmlReader.tokenType() == QXmlStreamReader::StartElement )
                {
                  attributes = xmlReader.attributes();

                  if(attributes.hasAttribute("Id"))
                  {
                    QString id = attributes.value("Id").toString();

                    if(modelComponent->m_inputGraphicObjects.contains(id))
                    {
                      GInput* input = modelComponent->m_inputGraphicObjects[id];

                      if(attributes.hasAttribute("XPos") && attributes.hasAttribute("YPos"))
                      {
                        QString xposS = attributes.value("XPos").toString();
                        QString yposS = attributes.value("YPos").toString();

                        bool ok;

                        double xloc = xposS.toDouble(&ok);
                        double yloc = yposS.toDouble(&ok);

                        if(ok)
                        {
                          input->setPos(xloc,yloc);
                        }
                      }

                      if(attributes.hasAttribute("Caption"))
                      {
                        input->setCaption(attributes.value("Caption").toString());
                      }
                    }
                    else
                    {
                      GInput* input = new GInput(id, modelComponent);
                      modelComponent->m_inputGraphicObjects[id] = input;
                      QString err;
                      if(!input->createConnection(modelComponent, err))
                        errorMessages.push_back(err);

                      if(attributes.hasAttribute("XPos") && attributes.hasAttribute("YPos"))
                      {
                        QString xposS = attributes.value("XPos").toString();
                        QString yposS = attributes.value("YPos").toString();

                        bool ok;

                        double xloc = xposS.toDouble(&ok);
                        double yloc = yposS.toDouble(&ok);

                        if(ok)
                        {
                          input->setPos(xloc,yloc);
                        }
                      }

                      if(attributes.hasAttribute("Caption"))
                      {
                        input->setCaption(attributes.value("Caption").toString());
                      }
                    }
                  }
                }

                xmlReader.readNext();
              }
            }

            xmlReader.readNext();
          }
        }
      }

      xmlReader.readNext();
    }
  }

  return modelComponent;
}

void GModelComponent::readComponentConnections(QXmlStreamReader & xmlReader, QList<QString>& errorMessages)
{
  if(!xmlReader.name().toString().compare("ModelComponentConnection",Qt::CaseInsensitive) && !xmlReader.hasError() && xmlReader.tokenType() == QXmlStreamReader::StartElement )
  {
    while(!(xmlReader.isEndElement() && !xmlReader.name().toString().compare("ModelComponentConnection", Qt::CaseInsensitive)) && !xmlReader.hasError())
    {
      if(!xmlReader.name().toString().compare("OutputExchangeItem",Qt::CaseInsensitive) && !xmlReader.hasError() && xmlReader.tokenType() == QXmlStreamReader::StartElement)
      {
        QXmlStreamAttributes attributes = xmlReader.attributes();

        if(attributes.hasAttribute("OutputExchangeItemId"))
        {
          QStringRef id = attributes.value("OutputExchangeItemId");

          if(m_outputGraphicObjects.contains(id.toString()))
          {
            GOutput* outputExchangeItem = m_outputGraphicObjects[id.toString()];

            //            if(attributes.hasAttribute("XPos") && attributes.hasAttribute("YPos"))
            //            {
            //              QString xposS = attributes.value("XPos").toString();
            //              QString yposS = attributes.value("YPos").toString();

            //              bool ok;

            //              double xloc = xposS.toDouble(&ok);
            //              double yloc = yposS.toDouble(&ok);

            //              if(ok)
            //              {
            //                outputExchangeItem->setPos(xloc,yloc);
            //              }
            //            }

            outputExchangeItem->readOutputExchangeItemConnections(xmlReader, errorMessages);

          }
          else
          {
            GOutput* outputExchangeItem = new GOutput(id.toString(), this);
            m_outputGraphicObjects[id.toString()] = outputExchangeItem;

            QString err;
            if(!this->createConnection(outputExchangeItem, err))
              errorMessages.push_back(err);

            outputExchangeItem->readOutputExchangeItemConnections(xmlReader, errorMessages);
          }
        }
      }
      xmlReader.readNext();
    }
  }
}

void GModelComponent::writeComponent(const QFileInfo &fileInfo)
{
  QFile file(fileInfo.absoluteFilePath());

  if(file.open(QIODevice::WriteOnly | QIODevice::Truncate))
  {

    QXmlStreamWriter xmlWriter(&file);
    xmlWriter.setAutoFormatting(true);
    xmlWriter.writeStartDocument();

    m_readFromFile = true;

    xmlWriter.writeStartElement("ModelComponent");
    {
      QFileInfo libFile(m_modelComponent->componentInfo()->libraryFilePath());
      QString relPath = "";

      if(libFile.isRelative())
      {
        //        if(m_readFromFile)
        //       {
        relPath = m_project->projectFile().dir().absoluteFilePath(libFile.filePath());
        relPath = fileInfo.dir().relativeFilePath(relPath);
        //        }
        //        else
        //        {
        //          relPath = m_project->m_projectFile.dir().absoluteFilePath(libFile.filePath());
        //          relPath = fileInfo.dir().relativeFilePath(relPath);
        //        }
      }
      else
      {
        relPath = fileInfo.dir().relativeFilePath(libFile.absoluteFilePath());
      }

      xmlWriter.writeAttribute("Name", m_modelComponent->componentInfo()->id());
      xmlWriter.writeAttribute("Caption", m_modelComponent->caption());
      xmlWriter.writeAttribute("ModelComponentLibrary",relPath);

      xmlWriter.writeStartElement("ComputeResourceAllocations");
      {
        for(CPUGPUAllocation *computeResourceAllocation : m_computeResourceAllocations)
        {
          computeResourceAllocation->writeCPUGPUAllocation(xmlWriter);
        }
      }
      xmlWriter.writeEndElement();


      xmlWriter.writeStartElement("Arguments");
      {
        for(IArgument* argument : m_modelComponent->arguments())
        {
          xmlWriter.writeStartElement("Argument");
          {
            xmlWriter.writeAttribute("Id",argument->id());
            argument->saveData();

            switch (argument->currentArgumentIOType())
            {
              case IArgument::File:
                {
                  xmlWriter.writeAttribute("ArgumentIOType","File");
                  xmlWriter.writeCharacters(argument->toString());
                }
                break;
              case IArgument::String:
                {
                  xmlWriter.writeAttribute("ArgumentIOType","String");

                  QString input = argument->toString();

                  if(!input.contains("</"))
                  {
                    xmlWriter.writeCharacters(argument->toString());
                  }
                  else
                  {
                    QXmlStreamReader xmlReader(input);

                    while (xmlReader.name().isEmpty() || xmlReader.name().isNull())
                    {
                      xmlReader.readNext();
                    }

                    while (!xmlReader.atEnd())
                    {
                      xmlWriter.writeCurrentToken(xmlReader);
                      xmlReader.readNext();
                    }
                  }
                }
                break;
            }
          }
          xmlWriter.writeEndElement();
        }
      }
      xmlWriter.writeEndElement();

    }
    xmlWriter.writeEndElement();

    xmlWriter.writeEndDocument();

    m_modelComponent->setReferenceDirectory(fileInfo.dir().absolutePath());
    m_readFromFile = true;
    m_exportFile = fileInfo;
  }
}

void GModelComponent::writeComponent(QXmlStreamWriter &xmlWriter)
{
  QDir projectDir = m_parent->projectFile().dir();
  xmlWriter.setAutoFormatting(true);

  xmlWriter.writeStartElement("ModelComponent");
  {
    if(m_readFromFile && m_exportFile.dir().exists())
    {
      writeComponent(m_exportFile);
      xmlWriter.writeAttribute("Name",m_modelComponent->componentInfo()->id());
      xmlWriter.writeAttribute("Caption" , m_modelComponent->caption());
      xmlWriter.writeAttribute("Description" , m_modelComponent->description());

      if(m_isTrigger)
      {
        xmlWriter.writeAttribute("IsTrigger" , "True");
      }

      xmlWriter.writeAttribute("ModelComponentFile", projectDir.relativeFilePath(m_exportFile.absoluteFilePath()));
      xmlWriter.writeAttribute("XPos" , QString::number(pos().x()));
      xmlWriter.writeAttribute("YPos" , QString::number(pos().y()));

    }
    else
    {
      QString relPath = projectDir.relativeFilePath(m_modelComponent->componentInfo()->libraryFilePath());
      xmlWriter.writeAttribute("Name",m_modelComponent->componentInfo()->id());
      xmlWriter.writeAttribute("Caption" , m_modelComponent->caption());
      xmlWriter.writeAttribute("Description" , m_modelComponent->description());

      if(m_isTrigger)
      {
        xmlWriter.writeAttribute("IsTrigger" , "True");
      }

      xmlWriter.writeAttribute("ModelComponentLibrary",relPath);
      xmlWriter.writeAttribute("XPos" , QString::number(pos().x()));
      xmlWriter.writeAttribute("YPos" , QString::number(pos().y()));

      xmlWriter.writeStartElement("ComputeResourceAllocations");
      {
        for(CPUGPUAllocation *computeResourceAllocation : m_computeResourceAllocations)
        {
          computeResourceAllocation->writeCPUGPUAllocation(xmlWriter);
        }
      }
      xmlWriter.writeEndElement();

      xmlWriter.writeStartElement("Arguments");
      {
        for(IArgument* argument : m_modelComponent->arguments())
        {
          xmlWriter.writeStartElement("Argument");
          {
            xmlWriter.writeAttribute("Id",argument->id());
            argument->saveData();

            switch (argument->currentArgumentIOType())
            {
              case IArgument::File:
                {
                  xmlWriter.writeAttribute("ArgumentIOType","File");
                  xmlWriter.writeCharacters(argument->toString());
                }
                break;
              case IArgument::String:
                {
                  xmlWriter.writeAttribute("ArgumentIOType","String");
                  QString input = argument->toString();

                  if(!input.contains("</"))
                  {
                    xmlWriter.writeCharacters(argument->toString());
                  }
                  else
                  {
                    QXmlStreamReader xmlReader(input);

                    while (xmlReader.name().isEmpty() || xmlReader.name().isNull())
                    {
                      xmlReader.readNext();
                    }

                    while (!xmlReader.atEnd())
                    {
                      xmlWriter.writeCurrentToken(xmlReader);
                      xmlReader.readNext();
                    }
                  }
                }
                break;
            }
          }
          xmlWriter.writeEndElement();
        }
      }
      xmlWriter.writeEndElement();

      m_readFromFile = false;
    }

    xmlWriter.writeStartElement("ExchangeItemPositions");
    {
      xmlWriter.writeStartElement("Outputs");
      {
        for(GOutput *output : m_outputGraphicObjects)
        {
          xmlWriter.writeStartElement("Output");
          {
            xmlWriter.writeAttribute("Id", output->id());
            xmlWriter.writeAttribute("XPos", QString::number(output->pos().x()));
            xmlWriter.writeAttribute("YPos", QString::number(output->pos().y()));
            xmlWriter.writeAttribute("Caption", output->caption());
          }
          xmlWriter.writeEndElement();
        }
      }
      xmlWriter.writeEndElement();


      xmlWriter.writeStartElement("Inputs");
      {
        for(GInput *input : m_inputGraphicObjects)
        {
          xmlWriter.writeStartElement("Input");
          {
            xmlWriter.writeAttribute("Id", input->id());
            xmlWriter.writeAttribute("XPos", QString::number(input->pos().x()));
            xmlWriter.writeAttribute("YPos", QString::number(input->pos().y()));
            xmlWriter.writeAttribute("Caption", input->caption());
          }
          xmlWriter.writeEndElement();
        }
      }
      xmlWriter.writeEndElement();

    }
    xmlWriter.writeEndElement();

  }
  xmlWriter.writeEndElement();

}

void GModelComponent::writeComponentConnections(QXmlStreamWriter &xmlWriter)
{
  bool found  = false;

  for(GOutput* output :  m_outputGraphicObjects.values())
  {
    if(output->connections().length())
      found = true;
  }

  if(!found)
    return ;

  int index = m_parent->modelComponents().indexOf(this);

  xmlWriter.writeStartElement("ModelComponentConnection");
  {
    xmlWriter.writeAttribute("SourceModelComponentIndex" , QString::number(index));

    for(GOutput* output :  m_outputGraphicObjects.values())
    {
      if(output->connections().length())
      {
        output->writeExchangeItemConnections(xmlWriter);
      }
    }
  }
  xmlWriter.writeEndElement();
}

QRectF GModelComponent::boundingRect() const
{
  QRectF m_boundingRect(0, 0, m_textItem->boundingRect().width() + 2 * m_xmargin, m_textItem->boundingRect().height() + 2 * m_ymargin);
  return m_boundingRect;
}

void GModelComponent::paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget)
{
  QPen pen = m_pen;
  QBrush brush = m_brush;
  QPainterPath m_path = QPainterPath();

  QRectF bRect = m_textItem->boundingRect();
  bRect.setCoords(0, 0, bRect.width() + 2 * m_margin, bRect.height() + 2 * m_margin);
  m_path.addRoundedRect(bRect, m_cornerRadius, m_cornerRadius, Qt::SizeMode::AbsoluteSize);

  if (option->state & QStyle::State_Selected)
  {
    pen = s_selectedPen;
    brush = s_selectedBrush;
  }
  else if (m_isTrigger)
  {
    pen = s_triggerPen;
    brush = s_triggerBrush;
  }

  painter->setPen(pen);
  painter->setBrush(brush);
  painter->drawPath(m_path);

  if (option->state & QStyle::State_Selected)
  {
    GDefaultSelectionGraphic::paint(bRect, painter);
  }
}

bool GModelComponent::createConnection(GNode *node, QString &message)
{
  if(node->nodeType() == GNode::NodeType::Output && !m_connections.contains(node))
  {
    GConnection *connection = new GConnection(this, node);
    m_connections[node] = connection;

    emit connectionAdded(connection);
    emit propertyChanged("Connections");
    emit hasChanges();

    return true;
  }

  message = "Connection is not valid!";

  return false;
}

void GModelComponent::deleteConnection(GConnection *connection)
{
  if(m_connections.contains(connection->consumer()))
  {
    delete connection;
  }
}

void GModelComponent::deleteConnection(GNode *node)
{
  if(m_connections.contains(node))
  {
    GConnection *connection  = m_connections[node];
    delete connection;
  }
}

void GModelComponent::deleteConnections()
{
  while (m_connections.size())
  {
    GConnection *connection = m_connections.begin().value();
    deleteConnection(connection);
  }
}

IAdaptedOutputFactory *GModelComponent::getAdaptedOutputFactory(const QString &id)
{
  for(IAdaptedOutputFactory *adaptedOutputFactory : m_modelComponent->componentInfo()->adaptedOutputFactories())
  {
    if(!QString::compare(id, adaptedOutputFactory->id()))
    {
      return adaptedOutputFactory;
    }
  }

  return nullptr;
}

IAdaptedOutput *GModelComponent::createAdaptedOutputInstance(const QString &adaptedOutputFactoryId, const QString &identity, IOutput *provider, IInput *consumer)
{

  for(IAdaptedOutputFactory *adaptedOutputFactory : m_modelComponent->componentInfo()->adaptedOutputFactories())
  {
    if(!QString::compare(adaptedOutputFactoryId, adaptedOutputFactory->id()))
    {
      QList<IIdentity*> availableAdaptors = adaptedOutputFactory->getAvailableAdaptedOutputIds(provider, consumer);

      for(IIdentity *adaptorId :  availableAdaptors)
      {
        if(!QString::compare(identity, adaptorId->id()))
        {
          IAdaptedOutput *adaptedOutput = adaptedOutputFactory->createAdaptedOutput(adaptorId, provider, consumer);
          return adaptedOutput;
        }
      }
    }
  }

  return nullptr;
}

QVariant GModelComponent::itemChange(GraphicsItemChange change, const QVariant &value)
{
  if(change == QGraphicsItem::ItemPositionChange && m_moveExchangeItemsWhenMoved)
  {
    QPointF move = value.toPointF() - pos();

    for(GInput* input : m_inputGraphicObjects.values())
    {
      input->moveBy(move.x() , move.y());
    }

    for(GOutput* output : m_outputGraphicObjects.values())
    {
      output->moveBy(move.x() , move.y());
    }
  }

  return GNode::itemChange(change, value);
}

void GModelComponent::deleteExchangeItems()
{
  m_inputs.clear();
  m_outputs.clear();

  for(GInput* input : m_inputGraphicObjects.values())
  {
    delete input;
  }

  m_inputGraphicObjects.clear();

  deleteConnections();

  for(GOutput *output : m_outputGraphicObjects)
  {
    delete output;
  }

  m_outputGraphicObjects.clear();

}

void GModelComponent::createExchangeItems()
{
  m_inputs.clear();
  m_outputs.clear();

  QPointF p = pos();
  QRectF bound = boundingRect();

  qreal xl = p.x() - 1.5*m_size;
  qreal xr = p.x() + m_size + 2* m_margin + 1.5*m_size;

  qreal lc = p.y() + bound.height()/2.0 - 40.0 ;
  bool sign = true;
  int count = 0;

  for(IOutput *output : m_modelComponent->outputs())
  {
    m_outputs[output->id()] = output;

    GOutput* goutput = nullptr;

    if(m_outputGraphicObjects.contains(output->id()))
    {
      goutput = m_outputGraphicObjects[output->id()];

      //      if(!goutput->caption().isEmpty())
      //        output->setCaption(goutput->caption());
      //      else
      goutput->setCaption(output->caption());
      goutput->reEstablishSignalSlotConnections();
    }
    else
    {
      GOutput* goutput = new GOutput(output->id(),this);
      goutput->setCaption(output->caption());

      m_outputGraphicObjects[output->id()] = goutput;

      if(sign)
      {
        goutput->setPos(xl, count * 180 + lc);
        sign = false;
        count++;
      }
      else
      {
        goutput->setPos(xl, -count * 180 + lc);
        sign = true;
      }

      if(scene())
      {
        scene()->addItem(goutput);
      }

      connect(goutput,SIGNAL(hasChanges()),this,SLOT(onChildHasChanges()));
      QString err;
      if(!createConnection(goutput, err))
        emit postMessage(err);
    }
  }

  QList<QString> missingOutputs;

  for(QHash<QString, GOutput*>::iterator it = m_outputGraphicObjects.begin();
      it != m_outputGraphicObjects.end(); it++)
  {
    GOutput *output = it.value();

    if(!m_outputs.contains(output->id()))
    {
      emit postMessage("Output exchangeitem with id " +  output->id() + " could not be found therefore it will be removed");

      for(auto itc = m_connections.begin(); itc != m_connections.end(); itc++)
      {
        if(itc.key() == output)
        {
          deleteConnection(itc.value());
          break;
        }
      }

      missingOutputs.push_back(output->id());
      delete output;
    }
  }

  for(const QString& id : missingOutputs)
  {
    QHash<QString, GOutput*>::iterator it = m_outputGraphicObjects.find(id);

    if(it != m_outputGraphicObjects.end())
    {
      m_outputGraphicObjects.erase(it);
    }
  }

  sign = true;
  count  = 0;


  for(IInput *input : m_modelComponent->inputs())
  {
    m_inputs[input->id()] = input;

    GInput* ginput = nullptr;

    if(m_inputGraphicObjects.contains(input->id()))
    {
      ginput = m_inputGraphicObjects[input->id()];
      ginput->reEstablishSignalSlotConnections();

      //      if(!ginput->caption().isEmpty())
      //        input->setCaption(ginput->caption());
      //      else
      ginput->setCaption(input->caption());
    }
    else
    {
      //      IMultiInput* minput = dynamic_cast<IMultiInput*>(input);

      //      if(minput)
      //      {
      //        ginput = new GMultiInput(minput->id(), this);
      //      }
      //      else
      //      {
      ginput = new GInput(input->id(),this);

      //      if(!ginput->caption().isEmpty())
      //        input->setCaption(ginput->caption());
      //      else
      ginput->setCaption(input->caption());

      //      ginput->setCaption(input->caption());

      m_inputGraphicObjects[ginput->id()] = ginput;

      if(sign)
      {
        ginput->setPos(xr, count * 180 + lc);
        sign = false;
        count++;
      }
      else
      {
        ginput->setPos(xr, -count * 180 + lc);
        sign = true;
      }

      if(scene())
      {
        scene()->addItem(ginput);
      }

      connect(ginput,SIGNAL(hasChanges()),this,SLOT(onChildHasChanges()));

      QString err;
      if(!ginput->createConnection(this, err))
        emit postMessage(err);
    }
  }

  QList<QString> missingInputs;

  for(QHash<QString, GInput*>::iterator it = m_inputGraphicObjects.begin();
      it != m_inputGraphicObjects.end(); it++)
  {
    GInput *input = it.value();

    if(!m_inputs.contains(input->id()))
    {
      emit postMessage("Input exchangeitem with id " +  input->id() + " could not be found therefore it will be removed");
      missingInputs.push_back(input->id());
      delete input;
    }
  }

  for(const QString& id : missingInputs)
  {
    QHash<QString, GInput*>::iterator it = m_inputGraphicObjects.find(id);

    if(it != m_inputGraphicObjects.end())
    {
      m_inputGraphicObjects.erase(it);
    }
  }


  emit hasChanges();
}

void GModelComponent::readArgument(QXmlStreamReader &xmlReader , IModelComponent* component)
{
  if(!xmlReader.name().toString().compare("Argument", Qt::CaseInsensitive) && !xmlReader.hasError() && xmlReader.tokenType() == QXmlStreamReader::StartElement )
  {
    QXmlStreamAttributes attributes = xmlReader.attributes();

    if(attributes.hasAttribute("Id") && attributes.hasAttribute("ArgumentIOType"))
    {
      QStringRef argumentId = attributes.value("Id");
      QStringRef argIOType = attributes.value("ArgumentIOType");
      IArgument* targument = nullptr;


      for(IArgument* argument : component->arguments())
      {
        if(!argumentId.toString().compare(argument->id() , Qt::CaseInsensitive))
        {
          targument = argument;

          QString value;
          QXmlStreamWriter writer(&value);

          while(!(xmlReader.isEndElement() && !xmlReader.name().toString().compare("Argument", Qt::CaseInsensitive)) && !xmlReader.hasError())
          {
            xmlReader.readNext();
            writer.writeCurrentToken(xmlReader);
          }

          QString message;

          if(!argIOType.toString().compare("File", Qt::CaseInsensitive))
          {
            targument->readValues(value, message, true);
          }
          else
          {
            targument->readValues(value, message);
          }

          break;
        }
      }
    }
  }
}

void GModelComponent::onComponentStatusChanged(const QSharedPointer<IComponentStatusChangeEventArgs> & statusChangedEvent)
{
  //#ifndef QT_DEBUG

  if(statusChangedEvent->status() ==  IModelComponent::Initialized)
  {
    createExchangeItems();
    onCreateTextItem();
  }
  else if(statusChangedEvent->status() == IModelComponent::Created)
  {
    m_inputs.clear();
    m_outputs.clear();
    onCreateTextItem();
  }

  if(!signalsBlocked())
    onCreateTextItem();

  emit componentStatusChanged(statusChangedEvent);
  emit postMessage(statusChangedEvent->message());

  //#endif
}

void GModelComponent::onPropertyChanged(const QString& propertyName)
{

  if (!propertyName.compare("Status", Qt::CaseInsensitive) ||
      !propertyName.compare("Id", Qt::CaseInsensitive) ||
      !propertyName.compare("Caption", Qt::CaseInsensitive) ||
      !propertyName.compare("Description", Qt::CaseInsensitive)
      )
  {
    onCreateTextItem();
  }

  emit propertyChanged(propertyName);

}

void GModelComponent::onDoubleClicked(GNode *node)
{
  if(dynamic_cast<GModelComponent*>(node))
  {
    emit doubleClicked(dynamic_cast<GModelComponent*>(node));
  }
  else if(dynamic_cast<GAdaptedOutput*>(node))
  {
    emit doubleClicked(dynamic_cast<GAdaptedOutput*>(node));
  }
}

void GModelComponent::onCreateTextItem()
{
  if(m_project && m_project->hasGraphics())
  {
    //    m_textItem->document()->setDefaultStyleSheet("img "
    //                                                 "{"
    //                                                 "margin-top: 10px;"
    //                                                 "margin-bottom: 10px;"
    //                                                 "margin-left: 10px;"
    //                                                 "margin-right: 10px;"
    //                                                 "}");
    m_textItem->setFont(m_font);
    m_textItem->setScale(1.0);
    QString desc(sc_descriptionHtml);
    QFileInfo iconFile(QFileInfo(m_modelComponent->componentInfo()->libraryFilePath()).dir(), m_modelComponent->componentInfo()->iconFilePath());

    desc.replace("[Id]", m_modelComponent->id())
        .replace("[Caption]", m_modelComponent->caption())
        .replace("[Description]", m_modelComponent->description())
        .replace("[Status]", modelComponentStatusAsString(m_modelComponent->status()))
        .replace("[IconPath]", iconFile.absoluteFilePath())
        .replace("[CId]", m_modelComponent->componentInfo()->caption());

    //    desc = "\t" + desc;

    setToolTip(desc);

    m_textItem->setHtml(desc);
    m_textItem->setTextWidth(m_size - 2 * m_margin);
    m_textItem->setPos(m_margin, m_margin);

    m_xmargin = m_ymargin = m_margin;
  }
}
