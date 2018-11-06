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

#ifndef GNODE_H
#define GNODE_H

#include <QGraphicsObject>
#include <QPen>
#include <QBrush>
#include <QFont>
#include "gconnection.h"

class HydroCoupleProject;

class GNode : public QGraphicsObject
{

    Q_OBJECT

    friend class GConnection;

  public:

    enum NodeType
    {
      Input,
      Output,
      AdaptedOutput,
      Component
    };

    Q_ENUMS(NodeType)

    Q_PROPERTY(GNode::NodeType NodeType READ nodeType)
    Q_PROPERTY(int Size READ size WRITE setSize)
    Q_PROPERTY(int Margin READ margin WRITE setMargin)
    Q_PROPERTY(int CornerRadius READ cornerRadius WRITE setCornerRadius)
    Q_PROPERTY(QPen Pen READ pen WRITE setPen NOTIFY propertyChanged)
    Q_PROPERTY(QBrush Brush READ brush WRITE setBrush NOTIFY propertyChanged)
    Q_PROPERTY(QPen SelectedPen READ selectedPen WRITE setSelectedPen NOTIFY propertyChanged)
    Q_PROPERTY(QBrush SelectedBrush READ selectedBrush WRITE setSelectedBrush NOTIFY propertyChanged)
    Q_PROPERTY(QFont Font READ font WRITE setFont NOTIFY propertyChanged)
    Q_PROPERTY(QGraphicsTextItem* LabelGraphics READ labeGraphicsObject)
    Q_PROPERTY(QList<GConnection*> Connections READ connections NOTIFY propertyChanged)

  public:

    GNode(const QString& id, const QString& caption, NodeType nodeType , HydroCoupleProject* project);

    virtual ~GNode();

    virtual QString id() const;

    void setId(const QString & id);

    virtual QString caption() const;

    void setCaption(const QString& caption);

    NodeType nodeType() const;

    int size() const;

    virtual void setSize(int size);

    int margin() const;

    virtual void setMargin(int margin);

    int cornerRadius() const;

    virtual void setCornerRadius(int cornerRadius);

    QPen pen() const;

    virtual void setPen(const QPen& pen);

    QBrush brush() const;

    virtual void setBrush(const QBrush& brush);

    QPen selectedPen() const;

    virtual void setSelectedPen(const QPen& selectedPen);

    QBrush selectedBrush() const;

    virtual void setSelectedBrush(const QBrush& selectedBrush);

    QFont font() const;

    virtual void setFont(const QFont& font);

    QGraphicsTextItem  *labeGraphicsObject() const;

    virtual bool isValid() const;

    virtual QRectF boundingRect() const override;

    QList<GConnection*> connections() const;

    HydroCoupleProject *project() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    virtual bool createConnection(GNode *node, QString &message) = 0;

    virtual void deleteConnection(GConnection *connection) = 0;

    virtual void deleteConnection(GNode *node) = 0;

    virtual void deleteConnections() = 0;

  signals:

    void doubleClicked(GNode* component);

    virtual void propertyChanged(const QString& propertyName);

    virtual void hasChanges();

    void connectionAdded(GConnection *connection);

  protected:

    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent * event) override;

  private slots:

    virtual void onCreateTextItem();

    virtual void onChildHasChanges();

  protected:

    QHash<GNode*,GConnection*> m_connections;
    int m_margin , m_size, m_cornerRadius , m_xmargin, m_ymargin , m_height , m_width;
    QString m_id, m_caption;
    NodeType m_nodeType;
    QGraphicsTextItem* m_textItem ;
    QRectF m_boundingRect;
    QPen m_pen ;
    static QPen s_selectedPen;
    QBrush m_brush;
    static QBrush s_selectedBrush;
    static const QString sc_captionHtml;
    QFont m_font;
    static int s_zindex;
    HydroCoupleProject *m_project;
    static QImage s_errorImage;
    static bool s_errorImageLoaded;
};

Q_DECLARE_METATYPE(GNode*)
Q_DECLARE_METATYPE(QGraphicsTextItem*)

#endif // GNODE_H

