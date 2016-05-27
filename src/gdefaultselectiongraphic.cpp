#include "stdafx.h"
#include "gdefaultselectiongraphic.h"

void GDefaultSelectionGraphic::paint(const QRectF& rect, QPainter * painter, const QPen& line)
{
   painter->setPen(line);
   painter->setBrush(QBrush(QColor(0, 0, 0, 0)));
   painter->drawRect(rect);


   float width = 10.0;
   float hwidth = width / 2;

   float x1 = rect.left() - hwidth;
   float x2 = rect.left() + rect.width() / 2 - hwidth;
   float x3 = rect.right() - hwidth;
   float y1 = rect.top() - hwidth;
   float y2 = rect.top() + (rect.height() / 2) - hwidth;
   float y3 = rect.bottom() - hwidth;

   painter->drawEllipse(x2 + hwidth - hwidth / 2, y2 + hwidth - hwidth / 2, width / 2, width / 2);
   painter->setBrush(QBrush(QColor(255, 255, 255)));

   //top r
   painter->drawRect(x1, y1, width, width);
   //m r
   painter->drawRoundedRect(x1, y2, width, width,width,width);
   //bottom r
   painter->drawRect(x1, y3, width, width);

   //top m
   painter->drawRoundedRect(x2, y1, width, width , width, width);
   //m m
   //painter->drawRect(x2, y2, width, width);
   //bottom m
   painter->drawRoundedRect(x2, y3, width, width, width, width);

   //top l
   painter->drawRect(x3, y1, width, width);
   //m l
   painter->drawRoundedRect(x3, y2, width, width,width,width);
   //bottom l
   painter->drawRect(x3, y3, width, width);
}

QRectF GDefaultSelectionGraphic::getRectFrom(const QPointF& p1, const QPointF p2)
{
   double x1, x2, y1, y2;

   if (p1.x() < p2.x())
   {
      x1 = p1.x(); x2 = p2.x();
   }
   else
   {
      x1 = p2.x(); x2 = p1.x();
   }

   if (p1.y() < p2.y())
   {
      y1 = p1.y(); y2 = p2.y();
   }
   else
   {
      y1 = p2.y(); x2 = p1.y();
   }

   QRectF t = QRectF(x1, y1, x2 - x1, y2 - y1);
   return t;
}
