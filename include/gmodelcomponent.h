#ifndef GMODELCOMPONENT_H
#define GMODELCOMPONENT_H


#include <QGraphicsObject>
#include <QPen>
#include <QBrush>
#include <QXmlStreamReader>

#include "hydrocouple.h"
#include "hydrocoupleproject.h"
#include "gmodelcomponentconnection.h"

class GModelComponent : public QGraphicsObject
{
      Q_OBJECT

      Q_PROPERTY(HydroCouple::IModelComponent* ModelComponent READ modelComponent)
      Q_PROPERTY(HydroCouple::ComponentStatus  Status READ status)
      Q_PROPERTY(QList<GModelComponentConnection*> ModelComponentConnections READ modelComponentConnections)
      Q_PROPERTY(bool Trigger READ trigger WRITE setTrigger NOTIFY propertyChanged)
      Q_PROPERTY(int Width READ width WRITE setWidth NOTIFY propertyChanged)
      Q_PROPERTY(int Margin READ margin WRITE setMargin NOTIFY propertyChanged)
      Q_PROPERTY(int CornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY propertyChanged)
      Q_PROPERTY(QPen Pen READ pen WRITE setPen NOTIFY propertyChanged)
      Q_PROPERTY(QGraphicsTextItem* GraphicsTextItem READ graphicsTextItem NOTIFY propertyChanged)
      Q_PROPERTY(QBrush Brush READ brush WRITE setBrush NOTIFY propertyChanged)
      Q_PROPERTY(QPen SelectedPen READ selectedPen WRITE setSelectedPen NOTIFY propertyChanged)
      Q_PROPERTY(QBrush SelectedBrush READ selectedBrush WRITE setSelectedBrush NOTIFY propertyChanged)
      Q_PROPERTY(QPen TriggerPen READ triggerPen WRITE setTriggerPen NOTIFY propertyChanged)
      Q_PROPERTY(QBrush TriggerBrush READ triggerBrush WRITE setTriggerBrush NOTIFY propertyChanged)


      //Q_ENUM(HydroCouple::ComponentStatus)

   public:
      explicit GModelComponent(HydroCouple::IModelComponent* model,  HydroCoupleProject *parent);

      virtual ~GModelComponent();

      HydroCouple::IModelComponent* modelComponent() const ;

      HydroCouple::ComponentStatus status() const ;

      //!Instantiated model connections
      QList<GModelComponentConnection*> modelComponentConnections() const;

      void deleteAllConnections();

      bool trigger() const;

      void setTrigger(bool trigger);

      //graphical items

      int width() const;

      void setWidth(int width);

      int margin() const;

      void setMargin(int margin);

      int cornerRadius() const;

      void setCornerRadius(int cornerRadius);

      QGraphicsTextItem* graphicsTextItem() const;

      QPen pen() const;

      void setPen(const QPen& pen);

      QBrush brush() const;

      void setBrush(const QBrush& brush);

      QPen selectedPen() const;

      void setSelectedPen(const QPen& selectedPen);

      QBrush selectedBrush() const;

      void setSelectedBrush(const QBrush& selectedBrush);

      QPen triggerPen() const;

      void setTriggerPen(const QPen& triggerPen);

      QBrush triggerBrush() const;

      void setTriggerBrush(const QBrush& triggerBrush);

      //!bounding rect for this component
      QRectF boundingRect() const override;

      //!paint component
      void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0) override;

      /*!
       * \brief QGraphicsItem::itemChange
       * \param change
       * \param value
       * \return
       */
      QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

      /*!
       * \brief createComponentModelConnection
       * \param component
       * \return
       */
      bool createComponentModelConnection(GModelComponent* component);

      /*!
       * \brief deleteComponentConnection
       * \param connection
       * \return
       */
      bool deleteComponentConnection(GModelComponentConnection * connection);

      static QString modelComponentStatusAsString(HydroCouple::ComponentStatus status);

      static GModelComponent* readComponentFile(const QFileInfo& file);

      static GModelComponent* readComponentSection(const QXmlStreamReader &xmlReader);

   signals:

      void componentStatusChanged(const HydroCouple::IComponentStatusChangeEventArgs& statusChangedEvent);

      void propertyChanged(const QString& propertyName, const QVariant& value);

      void componentConnectionAdded(GModelComponentConnection *connection);

      void componentConnectionDeleting(GModelComponentConnection *connection);

      void setAsTrigger(GModelComponent* component);

   private slots:

      void onComponentStatusChanged(const HydroCouple::IComponentStatusChangeEventArgs& statusChangedEvent);

      void onPropertyChanged(const QString& propertyName, const QVariant& value);

      void onReCreateGraphicObjects();

   private:
      HydroCouple::IModelComponent* m_modelComponent;
      QGraphicsTextItem* m_textItem;
      HydroCoupleProject* m_parent;
      QList<GModelComponentConnection*> m_modelComponentConnections;
      int m_margin , m_width, m_cornerRadius;
      static const QString sc_descriptionHtml;
      static QPen s_triggerPen, s_pen , s_selectedPen;
      static QBrush s_triggerBrush, s_brush , s_selectedBrush;
      bool m_isTrigger;
};





#endif // GMODELCOMPONENT_H
