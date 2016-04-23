#ifndef GMODELCOMPONENT_H
#define GMODELCOMPONENT_H

#include <QXmlStreamReader>

#include "gnode.h"
#include "hydrocouple.h"
#include "hydrocoupleproject.h"
#include "gexchangeitems.h"

class GModelComponent : public GNode
{
      Q_OBJECT

      Q_PROPERTY(HydroCouple::IModelComponent* ModelComponent READ modelComponent)
      Q_PROPERTY(QString Status READ status)
      Q_PROPERTY(bool MoveExchangeItemsWithComponent READ moveExchangeItemsWhenMoved WRITE setMoveExchangeItemsWhenMoved)
      Q_PROPERTY(bool Trigger READ trigger WRITE setTrigger NOTIFY propertyChanged)
      Q_PROPERTY(QPen TriggerPen READ triggerPen WRITE setTriggerPen NOTIFY propertyChanged)
      Q_PROPERTY(QBrush TriggerBrush READ triggerBrush WRITE setTriggerBrush NOTIFY propertyChanged)

   public:

      GModelComponent(HydroCouple::IModelComponent* model,  HydroCoupleProject *parent);

      virtual ~GModelComponent();

      HydroCouple::IModelComponent* modelComponent() const ;

      QString status() const ;

      void deleteAllConnections();

      bool trigger() const;

      void setTrigger(bool trigger);

      QPen triggerPen() const;

      void setTriggerPen(const QPen& triggerPen);

      QBrush triggerBrush() const;

      void setTriggerBrush(const QBrush& triggerBrush);

      bool moveExchangeItemsWhenMoved() const;

      void setMoveExchangeItemsWhenMoved(bool move);

      QList<GInput*> inputExchangeItems() const;

      QList<GOutput*> outputExchangeItems() const;

      static QString modelComponentStatusAsString(HydroCouple::ComponentStatus status);

      static GModelComponent* readComponentFile(const QFileInfo& file);

      static GModelComponent* readComponentSection(const QXmlStreamReader &xmlReader);

      QRectF boundingRect() const override;

      void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0) override;

   protected:
      virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

   private:
      void createExchangeItems();

   signals:

      void componentStatusChanged(const HydroCouple::IComponentStatusChangeEventArgs& statusChangedEvent);

      void propertyChanged(const QString& propertyName, const QVariant& value) override;

      void setAsTrigger(GModelComponent* component);

      void postMessage(const QString& message);

      void doubleClicked(GModelComponent * component);

   private slots:

      void onComponentStatusChanged(const HydroCouple::IComponentStatusChangeEventArgs& statusChangedEvent);

      void onPropertyChanged(const QString& propertyName, const QVariant& value);

      void onDoubleClicked(GNode* node);

      void onCreateTextItem() override;

   protected:
      HydroCouple::IModelComponent* m_modelComponent;
      HydroCoupleProject* m_parent;
      QList<GOutput*> m_outputExchangeItems;
      QList<GInput*> m_inputExchangeItems;
      static const QString sc_descriptionHtml;
      static QPen s_triggerPen ;
      static QBrush s_triggerBrush ;
      bool m_isTrigger;
      bool m_moveExchangeItemsWhenMoved;
};

Q_DECLARE_METATYPE(GModelComponent*)

#endif // GMODELCOMPONENT_H
