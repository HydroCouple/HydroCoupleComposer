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

class GModelComponent;
class GOutput;
class GAdaptedOutput;

class GExchangeItem : public GNode
{
      Q_OBJECT

   public:
      GExchangeItem(HydroCouple::IExchangeItem* exchangeItem, GNode::NodeType type,  QGraphicsObject *parent = nullptr);

      virtual ~GExchangeItem();

      virtual HydroCouple::IExchangeItem* exchangeItem() const;

   private:
      HydroCouple::IExchangeItem* m_exchangeItem;
};


class GInput: public GExchangeItem
{
      Q_OBJECT

   public:
      GInput(HydroCouple::IInput* input, QGraphicsObject* parent = nullptr);

      virtual ~GInput();

      HydroCouple::IInput* input() const;

      GOutput* provider() const;

      void setProvider(GOutput* provider);

   private:
      HydroCouple::IInput* m_input;
      GOutput* m_provider;
};

class GMultiInput : public GInput
{
      Q_OBJECT

   public:
      GMultiInput(HydroCouple::IMultiInput* input, QGraphicsObject* parent = nullptr);

      virtual ~GMultiInput();

      HydroCouple::IMultiInput* multiInput() const;

      QList<GOutput*> providers() const;

      void addProvider(GOutput* provider);

      void removeProvider(GOutput* provider);

   private:
      HydroCouple::IMultiInput* m_multiInput;
      QList<GOutput*> m_providers;
};

class GOutput : public GExchangeItem
{
      Q_OBJECT

   public:
      GOutput(HydroCouple::IOutput* output, QGraphicsObject* parent = nullptr);

      virtual ~GOutput();

      HydroCouple::IOutput* output() const;

   protected:
      HydroCouple::IOutput* m_output;
};


class GAdaptedOutput : public GOutput
{
      Q_OBJECT

      Q_PROPERTY(HydroCouple::IAdaptedOutput AdaptedOutput READ adaptedOutput)

   public:
      GAdaptedOutput(HydroCouple::IAdaptedOutput* adaptedOutput, HydroCouple::IAdaptedOutputFactory* factory,
                         GOutput* adaptee, GInput* input = nullptr , QGraphicsObject* parent = nullptr);

      virtual ~GAdaptedOutput();

      HydroCouple::IAdaptedOutput* adaptedOutput() const;

      GOutput* adaptee() const;

      GInput* input() const;

      HydroCouple::IAdaptedOutputFactory* factory() const;

   private:
      GOutput* m_adaptee;
      GInput* m_input;
      HydroCouple::IAdaptedOutput * m_adaptedOutput;
      HydroCouple::IAdaptedOutputFactory* m_adaptedOutputFactory;
};


#endif // GEXCHANGEITEMS_H
