#ifndef GMODELCOMPONENT_H
#define GMODELCOMPONENT_H

#include <QXmlStreamReader>

#include "gnode.h"
#include "hydrocouple.h"
#include "hydrocoupleproject.h"
#include "gexchangeitems.h"

class GModelComponent : public GNode
{
    friend class SimulationManager;

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

    HydroCoupleProject* project() const;

    void setProject(HydroCoupleProject* project);

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

    QHash<QString,HydroCouple::IInput*> inputs() const;

    QHash<QString,HydroCouple::IOutput*> outputs() const ;

    QHash<QString, GInput*> inputGraphicObjects() const;

    QHash<QString, GOutput*> outputGraphicObjects() const;

    static QString modelComponentStatusAsString(HydroCouple::ComponentStatus status);

    static GModelComponent* readComponentFile(const QFileInfo& file, HydroCoupleProject* project, QList<QString>& errorMessages);

    static GModelComponent* readComponent(QXmlStreamReader &xmlReader, HydroCoupleProject* project , QList<QString>& errorMessages);

    void readComponentConnections(QXmlStreamReader& xmlReader, QList<QString>& errorMessages);

    void writeComponent(const QFileInfo& fileInfo);

    void writeComponent(QXmlStreamWriter& xmlWriter);

    void writeComponentConnections(QXmlStreamWriter& xmlWriter);

    QRectF boundingRect() const override;

    void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0) override;

    bool createConnection(GNode *consumer) override;

    bool deleteConnection(GConnection *connection) override;

    bool deleteConnection(GNode * consumer) override;

    void deleteConnections() override;

    void disestablishConnections();

    void reestablishConnections();

    //      void removeAdaptedOutputsByComponentInfo(const QString& id);

  protected:

    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

  private:

    void deleteExchangeItems();

    void createExchangeItems();

    static void readArgument(QXmlStreamReader& xmlReader, HydroCouple::IModelComponent* component);

  signals:

    void componentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &statusChangedEvent);

    void propertyChanged(const QString& propertyName) override;

    void setAsTrigger(GModelComponent* component);

    void postMessage(const QString& message);

    void doubleClicked(GModelComponent * component);

    void doubleClicked(GAdaptedOutput* adaptedOutput);

  public slots:

    void onDoubleClicked(GNode* node);

  private slots:

    void onComponentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &statusChangedEvent);

    void onPropertyChanged(const QString& propertyName);

    void onCreateTextItem() override;

  protected:
    HydroCouple::IModelComponent* m_modelComponent;
    HydroCoupleProject* m_parent;

    QHash<QString,HydroCouple::IInput*> m_inputs;
    QHash<QString,HydroCouple::IOutput*> m_outputs;

    QHash<QString,GOutput*> m_outputGraphicObjects;
    QHash<QString,GInput*> m_inputGraphicObjects;

    static const QString sc_descriptionHtml;
    static QPen s_triggerPen ;
    static QBrush s_triggerBrush ;
    bool m_isTrigger, m_moveExchangeItemsWhenMoved, m_readFromFile, m_ignoreSignalsFromComponent;
    QFileInfo m_exportFile;
};

Q_DECLARE_METATYPE(GModelComponent*)

#endif // GMODELCOMPONENT_H
