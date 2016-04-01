#include "stdafx.h"
#include "gexchangeitems.h"

GExchangeItem::GExchangeItem(const QString& itemID, QGraphicsObject *parent)
   : QGraphicsObject(parent)
{
   m_id = itemID;
}

GExchangeItem::~GExchangeItem()
{

}

QString GExchangeItem::itemId() const
{
   return m_id;
}

QString GExchangeItem::toString() const
{
   return m_id;
}

//=====================================================================================

GInput::GInput(GModelComponent* model, const QString& inputId, bool isMultiInput)
   : GExchangeItem(inputId, model), m_isMultiInput(isMultiInput)
{
   m_model = model;
}

GInput::~GInput()
{

}

GModelComponent* GInput::model() const
{
   return m_model;
}

bool GInput::isMultiInput() const
{
   return m_isMultiInput;
}

//=====================================================================================


GOutputItem::GOutputItem(GModelComponent* model, const QString& outputId)
   : GExchangeItem(outputId, model)
{
   m_model = model;
}

GOutputItem::~GOutputItem()
{

}

GModelComponent* GOutputItem::model() const
{
   return m_model;
}

//=====================================================================================


