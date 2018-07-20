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

#ifndef GDEFAULTSELECTIONGRAPHIC_H
#define GDEFAULTSELECTIONGRAPHIC_H

#include <QRectF>
#include <QPainter>

class GDefaultSelectionGraphic
{

public:
	static void paint(const QRectF& rect, QPainter * painter, const QPen& line = QPen(QBrush(QColor(0, 150, 255), Qt::BrushStyle::SolidPattern), 1, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::RoundJoin));

	static QRectF getRectFrom(const QPointF& p1, const QPointF p2);
};

#endif // GDEFAULTSELECTIONGRAPHIC_H
