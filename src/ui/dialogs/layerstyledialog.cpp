#include "ui/dialogs/layerstyledialog.h"

#include "map/maplayer.h"
#include "render/attributeprovider.h"
#include "render/layerstyle.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace HydroCouple::Composer
{
  namespace
  {
    //! Width of the ramp preview drawn into the ramp combo, in pixels.
    constexpr int kRampPreviewWidth = 96;
    constexpr int kRampPreviewHeight = 14;

    QIcon rampPreview(const ColorRamp &ramp)
    {
      QPixmap pixmap(kRampPreviewWidth, kRampPreviewHeight);
      QPainter painter(&pixmap);

      for (int x = 0; x < kRampPreviewWidth; ++x)
      {
        painter.setPen(ramp.colorAt(static_cast<double>(x)
                                    / (kRampPreviewWidth - 1)));
        painter.drawLine(x, 0, x, kRampPreviewHeight);
      }

      return QIcon(pixmap);
    }

    void setButtonColor(QPushButton *button, const QColor &color)
    {
      QPixmap pixmap(16, 16);
      pixmap.fill(color);

      button->setIcon(QIcon(pixmap));
      button->setText(color.name(QColor::HexRgb));
      button->setProperty("styleColor", color);
    }

    const IAttributeProvider *providerFor(const MapLayer *layer)
    {
      // A layer supplies attributes by also implementing the provider
      // interface; one that does not can still be drawn with a single symbol.
      return dynamic_cast<const IAttributeProvider *>(layer);
    }
  }

  LayerStyleDialog::LayerStyleDialog(MapLayer *layer, QWidget *parent)
    : QDialog(parent), m_layer(layer)
  {
    setObjectName(QStringLiteral("layerStyleDialog"));
    setWindowTitle(layer ? tr("Style — %1").arg(layer->name()) : tr("Style"));
    setModal(true);

    buildForm();
    loadFromLayer();
    updateEnabledState();
  }

  LayerStyleDialog::~LayerStyleDialog() = default;

  bool LayerStyleDialog::canStyle(const MapLayer *layer)
  {
    return layer && layer->style() != nullptr;
  }

  void LayerStyleDialog::buildForm()
  {
    auto *symbology = new QGroupBox(tr("Symbology"), this);
    auto *form = new QFormLayout(symbology);

    m_modeCombo = new QComboBox(symbology);
    m_modeCombo->setObjectName(QStringLiteral("styleModeCombo"));
    m_modeCombo->addItem(tr("Single symbol"),
                         QVariant::fromValue(static_cast<int>(StyleMode::Single)));
    m_modeCombo->addItem(
      tr("Graduated"), QVariant::fromValue(static_cast<int>(StyleMode::Graduated)));
    m_modeCombo->addItem(
      tr("Categorized"),
      QVariant::fromValue(static_cast<int>(StyleMode::Categorized)));
    form->addRow(tr("Colour by"), m_modeCombo);

    m_attributeCombo = new QComboBox(symbology);
    m_attributeCombo->setObjectName(QStringLiteral("styleAttributeCombo"));
    form->addRow(tr("Attribute"), m_attributeCombo);

    m_methodCombo = new QComboBox(symbology);
    m_methodCombo->setObjectName(QStringLiteral("styleMethodCombo"));

    for (const ClassificationMethod method :
         {ClassificationMethod::EqualInterval, ClassificationMethod::Quantile,
          ClassificationMethod::NaturalBreaks, ClassificationMethod::Manual})
    {
      m_methodCombo->addItem(Classification::methodName(method),
                             QVariant::fromValue(static_cast<int>(method)));
    }

    form->addRow(tr("Method"), m_methodCombo);

    m_classCountSpin = new QSpinBox(symbology);
    m_classCountSpin->setObjectName(QStringLiteral("styleClassCountSpin"));
    m_classCountSpin->setRange(1, 24);
    form->addRow(tr("Classes"), m_classCountSpin);

    m_rampCombo = new QComboBox(symbology);
    m_rampCombo->setObjectName(QStringLiteral("styleRampCombo"));
    m_rampCombo->setIconSize(QSize(kRampPreviewWidth, kRampPreviewHeight));

    for (const QString &name : ColorRamp::builtinNames())
    {
      m_rampCombo->addItem(rampPreview(ColorRamp::builtin(name)), name, name);
    }

    form->addRow(tr("Colour ramp"), m_rampCombo);

    m_fillButton = new QPushButton(symbology);
    m_fillButton->setObjectName(QStringLiteral("styleFillButton"));
    connect(m_fillButton, &QPushButton::clicked, this,
            [this]
            {
              // Opened from clicked(), a release — a modal opened from a
              // mouse press wedges input on macOS.
              const QColor chosen = QColorDialog::getColor(
                m_fillButton->property("styleColor").value<QColor>(), this,
                tr("Symbol colour"));

              if (chosen.isValid())
              {
                setButtonColor(m_fillButton, chosen);
              }
            });
    form->addRow(tr("Symbol colour"), m_fillButton);

    m_sizeSpin = new QDoubleSpinBox(symbology);
    m_sizeSpin->setObjectName(QStringLiteral("styleSizeSpin"));
    m_sizeSpin->setRange(0.5, 64.0);
    m_sizeSpin->setSuffix(tr(" px"));
    form->addRow(tr("Symbol size"), m_sizeSpin);

    auto *labelling = new QGroupBox(tr("Labels"), this);
    auto *labelForm = new QFormLayout(labelling);

    m_labelsCheck = new QCheckBox(tr("Label features"), labelling);
    m_labelsCheck->setObjectName(QStringLiteral("styleLabelsCheck"));
    labelForm->addRow(m_labelsCheck);

    m_labelFieldCombo = new QComboBox(labelling);
    m_labelFieldCombo->setObjectName(QStringLiteral("styleLabelFieldCombo"));
    labelForm->addRow(tr("Label with"), m_labelFieldCombo);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("styleStatusLabel"));
    m_statusLabel->setWordWrap(true);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                           | QDialogButtonBox::Apply
                                           | QDialogButtonBox::Cancel,
                                         this);

    connect(buttons, &QDialogButtonBox::accepted, this,
            [this]
            {
              apply();
              accept();
            });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, [this] { apply(); });

    connect(m_modeCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { updateEnabledState(); });
    connect(m_labelsCheck, &QCheckBox::toggled, this,
            [this](bool) { updateEnabledState(); });

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(symbology);
    layout->addWidget(labelling);
    layout->addWidget(m_statusLabel);
    layout->addStretch(1);
    layout->addWidget(buttons);
  }

  void LayerStyleDialog::loadFromLayer()
  {
    const LayerStyle *style = m_layer ? m_layer->style() : nullptr;

    if (!style)
    {
      return;
    }

    m_modeCombo->setCurrentIndex(
      m_modeCombo->findData(static_cast<int>(style->mode())));

    if (const IAttributeProvider *provider = providerFor(m_layer))
    {
      const QVector<AttributeField> fields = provider->attributeFields();

      m_attributeCombo->clear();
      m_labelFieldCombo->clear();

      for (const AttributeField &field : fields)
      {
        const QString label = field.displayName.isEmpty() ? field.name
                                                          : field.displayName;
        const QString shown =
          field.unit.isEmpty() ? label
                               : QStringLiteral("%1 (%2)").arg(label,
                                                               field.unit);

        m_attributeCombo->addItem(shown, field.name);
        m_labelFieldCombo->addItem(shown, field.name);
      }
    }

    const int attributeIndex = m_attributeCombo->findData(style->attribute());

    if (attributeIndex >= 0)
    {
      m_attributeCombo->setCurrentIndex(attributeIndex);
    }

    m_methodCombo->setCurrentIndex(m_methodCombo->findData(
      static_cast<int>(style->classification().method())));
    m_classCountSpin->setValue(style->classification().classCount());

    setButtonColor(m_fillButton, style->symbol().fill);
    m_sizeSpin->setValue(style->symbol().size);

    m_labelsCheck->setChecked(style->labels().enabled);

    const int labelIndex = m_labelFieldCombo->findData(
      style->labels().fieldName);

    if (labelIndex >= 0)
    {
      m_labelFieldCombo->setCurrentIndex(labelIndex);
    }
  }

  void LayerStyleDialog::updateEnabledState()
  {
    const auto mode =
      static_cast<StyleMode>(m_modeCombo->currentData().toInt());

    const bool themed = mode != StyleMode::Single;
    const bool graduated = mode == StyleMode::Graduated;

    // Classes and a method mean nothing to a categorised layer, whose classes
    // are the values themselves, and nothing at all to a single symbol.
    m_attributeCombo->setEnabled(themed);
    m_methodCombo->setEnabled(graduated);
    m_classCountSpin->setEnabled(graduated);
    m_rampCombo->setEnabled(themed);

    m_labelFieldCombo->setEnabled(m_labelsCheck->isChecked());
  }

  bool LayerStyleDialog::apply()
  {
    LayerStyle *style = m_layer ? m_layer->style() : nullptr;

    if (!style)
    {
      return false;
    }

    style->setMode(static_cast<StyleMode>(m_modeCombo->currentData().toInt()));
    style->setAttribute(m_attributeCombo->currentData().toString());

    style->classification().setMethod(static_cast<ClassificationMethod>(
      m_methodCombo->currentData().toInt()));
    style->classification().setClassCount(m_classCountSpin->value());
    style->classification().setRamp(
      ColorRamp::builtin(m_rampCombo->currentData().toString()));

    Symbol symbol = style->symbol();
    symbol.fill = m_fillButton->property("styleColor").value<QColor>();
    symbol.size = m_sizeSpin->value();
    style->setSymbol(symbol);

    style->labels().enabled = m_labelsCheck->isChecked();
    style->labels().fieldName = m_labelFieldCombo->currentData().toString();

    bool built = true;

    if (const IAttributeProvider *provider = providerFor(m_layer))
    {
      built = style->rebuild(*provider);
    }

    // Says so plainly when the style cannot colour anything, rather than
    // closing on a layer that has quietly stopped drawing.
    m_statusLabel->setText(
      built ? QString()
            : tr("This layer has no usable values for “%1”, so nothing is "
                 "drawn. Choose another attribute.")
                .arg(style->attribute()));

    m_layer->notifyAppearanceChanged();

    return built;
  }

} // namespace HydroCouple::Composer
