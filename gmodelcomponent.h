#ifndef GMODELCOMPONENT_H
#define GMODELCOMPONENT_H

#include "imodelcomponent.h"
#include "hydrocoupleproject.h"
#include <QGraphicsObject>

class GModelComponent : public QGraphicsObject , public virtual HydroCouple::IModelComponent
{
	Q_OBJECT;
	Q_PROPERTY(QString Id READ id NOTIFY propertyChanged);
	Q_PROPERTY(QString Caption READ getCaption WRITE setCaption NOTIFY propertyChanged);
	Q_PROPERTY(QString Description READ getDescription WRITE setDescription NOTIFY propertyChanged);

	Q_PROPERTY(bool Trigger READ trigger WRITE setTrigger NOTIFY propertyChanged);

	Q_PROPERTY(int Width READ width WRITE setWidth NOTIFY propertyChanged);
	Q_PROPERTY(int Margin READ margin WRITE setMargin NOTIFY propertyChanged);
	Q_PROPERTY(int CornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY propertyChanged);
	Q_PROPERTY(QPen Pen READ pen WRITE setPen NOTIFY propertyChanged);
	Q_PROPERTY(QBrush Brush READ brush WRITE setBrush NOTIFY propertyChanged);
	Q_PROPERTY(QPen SelectedPen READ selectedPen WRITE setSelectedPen NOTIFY propertyChanged);
	Q_PROPERTY(QBrush SelectedBrush READ selectedBrush WRITE setSelectedBrush NOTIFY propertyChanged);
	Q_PROPERTY(QPen TriggerPen READ triggerPen WRITE setTriggerPen NOTIFY propertyChanged);
	Q_PROPERTY(QBrush TriggerBrush READ triggerBrush WRITE setTriggerBrush NOTIFY propertyChanged);
	
	Q_INTERFACES(HydroCouple::IModelComponent);

public:
	explicit GModelComponent(HydroCouple::IModelComponent* &  model,  HydroCoupleProject *parent);

	virtual ~GModelComponent();

	QString id() const override;

	virtual QString getCaption() const override;

	virtual void setCaption(const QString & caption) override;

	virtual QString getDescription() const override;

	virtual void setDescription(const QString& decription) override;

    const HydroCouple::IModelComponentInfo* componentInfo() const override;

	HydroCouple::IModelComponent* parent() const override;

	HydroCouple::IModelComponent* modelComponent() const ;

	IModelComponent* clone() const override;

	QList<IModelComponent*> children() const override;

	QList<HydroCouple::Data::IArgument*> arguments() const override;

	HydroCouple::ComponentStatus status() const override;

	virtual QList<HydroCouple::Data::IInput*> inputs() const override;

	virtual QList<HydroCouple::Data::IOutput*> outputs() const override;

	virtual QList<HydroCouple::Data::IAdaptedOutputFactory*> adaptedOutputFactories() const override;

	virtual void initialize() override;

	virtual QList<QString> validate() override;

	virtual void prepare() override;

	void update(const QList<HydroCouple::Data::IOutput*> & requiredOutputs = QList<HydroCouple::Data::IOutput*>()) override;

	void finish() override;

	bool trigger() const;

	void setTrigger(bool trigger);

	//graphical items

	int width() const;

	void setWidth(int width);

	int margin() const;

	void setMargin(int margin);

	int cornerRadius() const;

	void setCornerRadius(int cornerRadius);

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
	QRectF	boundingRect() const override;

	//!paint component
	void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0) override;

	static QString modelComponentStatusAsString(HydroCouple::ComponentStatus status);

signals:

   void componentStatusChanged(const HydroCouple::IComponentStatusChangeEventArgs& statusChangedEvent) override;

   void propertyChanged(const QString& propertyName, const QVariant& value) override;

   void setAsTrigger(GModelComponent* component);

private slots:
    
   void onComponentStatusChanged(const HydroCouple::IComponentStatusChangeEventArgs& statusChangedEvent);

   void onPropertyChanged(const QString& propertyName, const QVariant& value);

   void onReCreateGraphicObjects();

private:
	HydroCouple::IModelComponent* m_modelComponent;
	QGraphicsTextItem* m_textItem;
	HydroCoupleProject* m_parent;
	int m_margin , m_width, m_cornerRadius;
	static const QString sc_descriptionHtml;
	static QPen s_triggerPen, s_pen , s_selectedPen;
	static QBrush s_triggerBrush, s_brush , s_selectedBrush;
	bool m_isTrigger;
};



#endif // GMODELCOMPONENT_H
