#ifndef GDEFAULTSELECTIONGRAPHIC_H
#define GDEFAULTSELECTIONGRAPHIC_H

#include <QRectF>

class GDefaultSelectionGraphic
{

public:
	static void paint(const QRectF& rect, QPainter * painter, const QPen& line = QPen(QBrush(QColor(0, 150, 255), Qt::BrushStyle::SolidPattern), 1, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::RoundJoin));

	static QRectF getRectFrom(const QPointF& p1, const QPointF p2);
};

#endif // GDEFAULTSELECTIONGRAPHIC_H
