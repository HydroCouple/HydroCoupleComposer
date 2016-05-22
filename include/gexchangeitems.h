/*! \file   gexchangeitems.h
 *  \author Caleb Amoa Buahin <caleb.buahin@gmail.com>
 *  \version   1.0.0.0
 *  \section   Description definitions for the exchange items
 *  \section License
 *  hydrocouple.h, associated files and libraries are free software;
 *  you can redistribute it and/or modify it under the terms of the
 *  Lesser GNU General Public License as published by the Free Software Foundation;
 *  either version 3 of the License, or (at your option) any later version.
 *  hydrocouple.h its associated files is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.(see <http://www.gnu.org/licenses/> for details)
 *  \date 2014-2016
 *  \pre
 *  \bug
 *  \todo Make the coupling graphical someday
 *  \warning
 */

#ifndef GEXCHANGEITEMS_H
#define GEXCHANGEITEMS_H

#include <QGraphicsObject>
#include "hydrocouple.h"
#include "gnode.h"
#include "gconnection.h"
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

class GModelComponent;
class GOutput;
class GAdaptedOutput;

class GExchangeItem : public GNode
{
      Q_OBJECT
      Q_PROPERTY(HydroCouple::IExchangeItem* ExchangeItem READ exchangeItem)
      Q_PROPERTY(GModelComponent* ModelComponent READ modelComponent)

   public:
      GExchangeItem(const QString &exchangeItemId, GNode::NodeType type,  GModelComponent *parent);

      virtual ~GExchangeItem();

      virtual HydroCouple::IExchangeItem* exchangeItem() const = 0;

      GModelComponent* modelComponent() const;

      virtual void writeExchangeItemConnections(QXmlStreamWriter & xmlWriter) = 0;

   protected slots:
      void onPropertyChanged(const QString &propertyName);

   protected:
      GModelComponent *m_component;

   private:
      QString m_exchangeItemId;

};


class GInput: public GExchangeItem
{
      Q_OBJECT
      Q_PROPERTY(HydroCouple::IInput* Input READ input)
      Q_PROPERTY(GOutput* Provider READ provider)

   public:
      GInput(const QString &inputId, GModelComponent* parent);

      virtual ~GInput();

      HydroCouple::IExchangeItem* exchangeItem() const override;

      HydroCouple::IInput* input() const;

      GOutput* provider() const;

      void setProvider(GOutput* provider);

      void writeExchangeItemConnections(QXmlStreamWriter &xmlWriter) override;

      bool createConnection(GNode* consumer) override;

      bool deleteConnection(GConnection* connection) override;

      bool deleteConnection(GNode* consumer) override;

      void deleteConnections() override;

   private:
      QString m_input;
      GOutput* m_provider;
};

class GMultiInput : public GInput
{
      Q_OBJECT
      Q_PROPERTY(HydroCouple::IMultiInput* MultiInput READ multiInput)
      Q_PROPERTY(QList<GOutput*> Providers READ providers)

   public:
      GMultiInput(const QString &multiInputId, GModelComponent* parent);

      virtual ~GMultiInput();

      HydroCouple::IMultiInput* multiInput() const;

      QList<GOutput*> providers() const;

      void addProvider(GOutput* provider);

      void removeProvider(GOutput* provider);

   private:
      QString m_multiInputId;
      QList<GOutput*> m_providers;
};

class GOutput : public GExchangeItem
{
      Q_OBJECT
      Q_PROPERTY(HydroCouple::IOutput* Output READ output)

   public:
      GOutput(const QString &outputId, GModelComponent *parent);

      virtual ~GOutput();

      HydroCouple::IExchangeItem* exchangeItem() const override;

      HydroCouple::IOutput* output() const;

      virtual void writeExchangeItemConnections(QXmlStreamWriter &xmlWriter) override;

      virtual void readOutputExchangeItemConnections(QXmlStreamReader &xmlReader, QList<QString> &errorMessages);

      virtual void readAdaptedOutputExchangeItem(QXmlStreamReader &xmlReader, QList<QString> &errorMessages);

      bool createConnection(GNode *consumer) override;

      bool deleteConnection(GConnection *connection) override;

      bool deleteConnection(GNode *consumer) override;

      void deleteConnections() override;

   protected:
      QString  m_outputId;
};


class GAdaptedOutput : public GOutput
{
      Q_OBJECT
      Q_PROPERTY(HydroCouple::IAdaptedOutput* AdaptedOutput READ adaptedOutput)

   public:
      GAdaptedOutput(HydroCouple::IAdaptedOutput * adaptedOutput, HydroCouple::IAdaptedOutputFactory *factory,
                     GOutput *adaptee, GInput *input);

      virtual ~GAdaptedOutput();

      HydroCouple::IAdaptedOutput* adaptedOutput() const;

      GOutput* adaptee() const;

      GInput* input() const;

      HydroCouple::IAdaptedOutputFactory* factory() const;

      void writeExchangeItemConnections(QXmlStreamWriter &xmlWriter) override;

      void readAdaptedOutputExchangeItemConnections(QXmlStreamReader &xmlReader, QList<QString> &errorMessages);

   private:
      GOutput *m_adaptee;
      GInput *m_input;
      HydroCouple::IAdaptedOutput *m_adaptedOutput;
      HydroCouple::IAdaptedOutputFactory *m_adaptedOutputFactory;
      QList<HydroCouple::IAdaptedOutput*> m_childAdaptedOutputs;
};


#endif // GEXCHANGEITEMS_H
