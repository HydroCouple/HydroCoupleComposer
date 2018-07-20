/*!
 * \file   hydrocouple.h
 * \author Caleb Amoa Buahin <caleb.buahin@gmail.com>
 * \version   1.0.0
 * \description
 * This header file contains the core interface definitions for the
 * HydroCouple component-based modeling definitions.
 * \license
 * This file and its associated files, and libraries are free software.
 * You can redistribute it and/or modify it under the terms of the
 * Lesser GNU Lesser General Public License as published by the Free Software Foundation;
 * either version 3 of the License, or (at your option) any later version.
 * This file and its associated files is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.(see <http://www.gnu.org/licenses/> for details)
 * \copyright Copyright 2014-2018, Caleb Buahin, All rights reserved.
 * \date 2014-2018
 * \pre
 * \bug
 * \warning
 * \todo
 */

#ifndef GMODELCOMPONENT_H
#define GMODELCOMPONENT_H

#include <QXmlStreamReader>

#include "gnode.h"
#include "hydrocouple.h"
#include "hydrocoupleproject.h"
#include "gexchangeitems.h"

class CPUGPUAllocation;

class GModelComponent : public GNode
{
    friend class GAdaptedOutput;

    Q_OBJECT

    Q_PROPERTY(HydroCouple::IModelComponent* ModelComponent READ modelComponent)
    Q_PROPERTY(QString Status READ status)
    Q_PROPERTY(bool MoveExchangeItemsWithComponent READ moveExchangeItemsWhenMoved WRITE setMoveExchangeItemsWhenMoved)
    Q_PROPERTY(bool Trigger READ trigger WRITE setTrigger NOTIFY propertyChanged)
    Q_PROPERTY(QPen TriggerPen READ triggerPen WRITE setTriggerPen NOTIFY propertyChanged)
    Q_PROPERTY(QBrush TriggerBrush READ triggerBrush WRITE setTriggerBrush NOTIFY propertyChanged)
    Q_PROPERTY(QList<CPUGPUAllocation*> ComputeResources READ allocatedComputeResources WRITE allocateComputeResources NOTIFY propertyChanged)

  public:

    GModelComponent(HydroCouple::IModelComponent* model,  HydroCoupleProject *parent);

    virtual ~GModelComponent();

    HydroCoupleProject* project() const;

    void setProject(HydroCoupleProject* project);

    HydroCouple::IModelComponent* modelComponent() const ;

    QString id() const override;

    QString caption() const override;

    QString status() const ;

    bool trigger() const;

    void setTrigger(bool trigger);

    QPen triggerPen() const;

    void setTriggerPen(const QPen& triggerPen);

    QBrush triggerBrush() const;

    void setTriggerBrush(const QBrush& triggerBrush);

    QList<CPUGPUAllocation*> allocatedComputeResources() const;

    void allocateComputeResources(const QList<CPUGPUAllocation*> &allocatedComputeResources);

    bool moveExchangeItemsWhenMoved() const;

    void setMoveExchangeItemsWhenMoved(bool move);

    bool readFromFile() const;

    QHash<QString,HydroCouple::IInput*> inputs() const;

    QHash<QString,HydroCouple::IOutput*> outputs() const ;

    QHash<QString, GInput*> inputGraphicObjects() const;

    QHash<QString, GOutput*> outputGraphicObjects() const;

    static QString modelComponentStatusAsString(HydroCouple::IModelComponent::ComponentStatus status);

    static GModelComponent* readComponentFile(const QFileInfo& file, HydroCoupleProject* project,
                                              QList<QString>& errorMessages, bool initialize = true);

    static GModelComponent* readComponent(QXmlStreamReader &xmlReader, HydroCoupleProject* project,
                                          const QDir& referenceDir, QList<QString>& errorMessages, bool initialize = true);

    void readComponentConnections(QXmlStreamReader& xmlReader, QList<QString>& errorMessages);

    void writeComponent(const QFileInfo& fileInfo);

    void writeComponent(QXmlStreamWriter& xmlWriter);

    void writeComponentConnections(QXmlStreamWriter& xmlWriter);

    QRectF boundingRect() const override;

    void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0) override;

    bool createConnection(GNode *node) override;

    void deleteConnection(GConnection *connection) override;

    void deleteConnection(GNode *node) override;

    void deleteConnections() override;

    HydroCouple::IAdaptedOutputFactory *getAdaptedOutputFactory(const QString & id);

    HydroCouple::IAdaptedOutput *createAdaptedOutputInstance(const QString &adaptedOutputFactoryId, const QString &identity, HydroCouple::IOutput *provider, HydroCouple::IInput *consumer);

  protected:

    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

  private:

    void deleteExchangeItems();

    void createExchangeItems();

    static void readArgument(QXmlStreamReader& xmlReader, HydroCouple::IModelComponent* component);

    HydroCouple::IAdaptedOutput *createAdaptedOutput(const QString &adaptedOutputId, const QString &adaptedOutputFactoryId, GOutput *output, GInput *input);

  signals:

    void componentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &statusChangedEvent);

    void propertyChanged(const QString& propertyName) override;

    void setAsTrigger(GModelComponent* component);

    void postMessage(const QString& message);

    void doubleClicked(GModelComponent * component);

    void doubleClicked(GAdaptedOutput* adaptedOutput);

  public slots:

    void onDoubleClicked(GNode* node);

  private slots:

    void onComponentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &statusChangedEvent);

    void onPropertyChanged(const QString& propertyName);

    void onCreateTextItem() override;

  protected:

    HydroCouple::IModelComponent* m_modelComponent;
    HydroCoupleProject* m_parent;
    QHash<QString,HydroCouple::IInput*> m_inputs;
    QHash<QString,HydroCouple::IOutput*> m_outputs;
    QHash<QString,GOutput*> m_outputGraphicObjects;
    QHash<QString,GInput*> m_inputGraphicObjects;
    QList<CPUGPUAllocation*> m_computeResourceAllocations;
    static const QString sc_descriptionHtml;
    static QPen s_triggerPen ;
    static QBrush s_triggerBrush ;
    bool m_isTrigger, m_moveExchangeItemsWhenMoved, m_readFromFile, m_ignoreSignalsFromComponent;
    QFileInfo m_exportFile;
};

Q_DECLARE_METATYPE(GModelComponent*)

#endif // GMODELCOMPONENT_H
