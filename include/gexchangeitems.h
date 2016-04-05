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

#include <QObject>
#include <QGraphicsObject>


class GModelComponent;

class GExchangeItem : public QGraphicsObject
{
      Q_OBJECT
      Q_PROPERTY(QString Id READ itemId)

   public:
      GExchangeItem(const QString& itemID, QGraphicsObject *parent = nullptr);

      virtual ~GExchangeItem();

      virtual QString itemId() const;

      virtual QString toString() const;

   private:
      QString m_id;
};



//class GInput: public GExchangeItem
//{
//      Q_OBJECT
//      Q_PROPERTY(GModelComponent* Model READ model)
//      Q_PROPERTY(bool IsMultiConsumer READ isMultiConsumer)

//   public:
//      GInput(GModelComponent* model, const QString& inputId,
//             bool isMultiInput = false);

//      ~GInput();

//      GModelComponent* model() const;

//      bool isMultiInput() const;

//   private:
//      GModelComponent* m_model;
//      bool m_isMultiInput;
//};



//class GOutputItem : public GExchangeItem
//{
//      Q_OBJECT
//      Q_PROPERTY(GModelComponent* Model READ model)

//   public:
//      GOutputItem(GModelComponent* model, const QString& outputId);

//      ~GOutputItem();

//      GModelComponent* model() const;

//   protected:
//      GModelComponent* m_model;

//};



//class GAdaptedOutputItem : public GOutputItem
//{
//      Q_OBJECT
//      Q_PROPERTY(GOutputItem* ParentProvider READ parentProvider)

//   public:
//      GAdaptedOutputItem(GOutputItem* parentProvider);

//      ~GAdaptedOutputItem();

//      GOutputItem* parentProvider() const;

//   private:
//     GOutputItem* parentProvider;
//};


//class GAdaptedOutputItemFromAdaptedOutputFactory : public GAdaptedOutputItem
//{
//      Q_OBJECT
//      //Q_PROPERTY(type name READ name WRITE setName NOTIFY nameChanged)

//   public:


//};

#endif // GEXCHANGEITEMS_H
