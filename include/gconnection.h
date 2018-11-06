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

#ifndef GCONNECTION_H
#define GCONNECTION_H

#include "hydrocouple.h"
#include <QGraphicsObject>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QMimeData>

class GNode;
class GOutput;
class GInput;
class GAdaptedOutput;
class GExchangeItem;
class GModelComponent;

class GConnection : public QGraphicsObject
{

    friend class GModelComponent;
    friend class GInput;
    friend class GOutput;
    friend class GAdaptedOutput;

    Q_OBJECT

    Q_PROPERTY(GNode* Producer READ producer)
    Q_PROPERTY(GNode* Consumer READ consumer)
    Q_PROPERTY(QPen Pen READ pen WRITE setPen)
    Q_PROPERTY(QBrush Brush READ brush WRITE setBrush)
    Q_PROPERTY(QPen SelectedPen READ selectedPen WRITE setSelectedPen)
    Q_PROPERTY(QFont Font READ font WRITE setFont)
    Q_PROPERTY(float ArrowLength READ arrowLength WRITE setArrowLength)
    Q_PROPERTY(float ArrowWidth READ arrowWidth WRITE setArrowWidth)

  private:

    GConnection();

    GConnection(GNode* producer, GNode* consumer , QGraphicsObject* parent = nullptr);

  public:

    virtual ~GConnection();

    GNode* producer() const;

    GNode* consumer() const;

    float arrowLength() const;

    void setArrowLength(float value);

    float arrowWidth() const;

    void setArrowWidth(float value);

    QPen pen() const;

    void setPen(const QPen& pen);

    QBrush brush() const;

    void setBrush(const QBrush& brush);

    QPen selectedPen() const;

    void setSelectedPen(const QPen& pen);

    QFont font() const;

    void setFont(const QFont& font);

    virtual QRectF boundingRect() const override;

    QPainterPath shape() const override;

    bool isValid() const;

    virtual void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0) override;

    bool insertAdaptedOutput(const QString& adaptedOutputId, const QString& adaptedOutputFactoryId, bool fromComponentLibrary = false);

  signals:

    void doubleClicked(GConnection* connection);

  protected:

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    void dragEnterEvent(QGraphicsSceneDragDropEvent *event) override;

    void dragMoveEvent(QGraphicsSceneDragDropEvent *event) override;

    void dropEvent(QGraphicsSceneDragDropEvent *event) override;

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent * event) override;

  private:

    bool canAcceptDrop(const QMimeData* data);

  private slots:

    void parentLocationOrSizeChanged();

    QPointF minPosition(const QList<QPointF> & points);

    QPointF maxPosition(const QList<QPointF> & points);

    void onNodePropertyChanged(const QString &propertyName);

  protected:

    GNode* m_producer, *m_consumer;
    QPointF m_start, m_end , m_mid , m_c1, m_c2, m_errorLocation;
    QPointF m_arrowPoint[3];
    QRectF m_boundary;
    QPainterPath m_path;
    QPen m_pen;
    QGraphicsTextItem* m_numConnectionsText;
    static QPen m_selectedPen;
    QBrush m_brush;
    QPointF m_labelLocation;
    static QFont m_font;
    static float m_arrowLength , m_arrowWidth;
    static int s_zindex;

};


Q_DECLARE_METATYPE(GConnection*)

#endif // GCONNECTION_H

