#include "ui/dialogs/layerpropertiesdialog.h"

#include "gis/spatialreference.h"
#include "map/maplayer.h"
#include "ui/dialogs/crsselectiondialog.h"
#include "render/attributeprovider.h"
#include "scene/scenesource.h"
#include "render/layerstyle.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
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

    /*!
     * \brief The layer's class, without the namespace it lives in.
     *
     * The class name is the one description that is always available and
     * always true, which is what the Information page is for.
     */
    QString typeNameOf(const MapLayer *layer)
    {
      const QString className =
        QString::fromLatin1(layer->metaObject()->className());

      return className.section(QStringLiteral("::"), -1);
    }

    QString formatExtent(const QRectF &extent)
    {
      if (extent.isEmpty())
      {
        // Not an error: a layer that has been created but not populated has
        // no extent, and saying so beats printing four zeroes.
        return QObject::tr("empty");
      }

      return QObject::tr("%1, %2 to %3, %4")
        .arg(extent.left(), 0, 'f', 3)
        .arg(extent.top(), 0, 'f', 3)
        .arg(extent.right(), 0, 'f', 3)
        .arg(extent.bottom(), 0, 'f', 3);
    }

    QString describeCrs(const SpatialReference *crs)
    {
      if (!crs)
      {
        // Distinct from "the file said EPSG:4326": a layer with no CRS is
        // drawn in whatever units it happens to hold, and C5b's Assign is
        // the answer to it.
        return QObject::tr("None — coordinates are taken as they are");
      }

      const QString authority = QString::fromStdString(crs->authName());
      const QString description = crs->description();

      if (authority.isEmpty())
      {
        return description;
      }

      return QStringLiteral("%1:%2 — %3")
        .arg(authority)
        .arg(crs->authSRID())
        .arg(description);
    }

    //! A read-only row that can still be copied out of.
    QLabel *factLabel(const QString &text, QWidget *parent,
                      const QString &objectName)
    {
      auto *label = new QLabel(text, parent);
      label->setObjectName(objectName);
      label->setTextInteractionFlags(Qt::TextSelectableByMouse);
      label->setWordWrap(true);

      return label;
    }
  }

  LayerPropertiesDialog::LayerPropertiesDialog(MapLayer *layer,
                                               QWidget *parent)
    : QDialog(parent), m_layer(layer)
  {
    setObjectName(QStringLiteral("layerPropertiesDialog"));
    setWindowTitle(layer ? tr("Layer Properties — %1").arg(layer->name())
                         : tr("Layer Properties"));
    setModal(true);

    buildTabs();
    loadFromLayer();
    updateEnabledState();
  }

  LayerPropertiesDialog::~LayerPropertiesDialog() = default;

  bool LayerPropertiesDialog::canEdit(const MapLayer *layer)
  {
    return layer != nullptr;
  }

  bool LayerPropertiesDialog::hasStyle() const
  {
    return m_layer && m_layer->style() != nullptr;
  }

  void LayerPropertiesDialog::buildTabs()
  {
    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("layerPropertiesTabs"));

    const struct
    {
        QWidget *page;
        QString title;
    } pages[] = {
      {buildInformationTab(), tr("Information")},
      {buildSourceTab(), tr("Source")},
      {buildSymbologyTab(), tr("Symbology")},

      // "(2D)" in the title, because labels are drawn only on the map and
      // nothing else in this dialog says which view a tab speaks for.
      {buildLabelsTab(), tr("Labels (2D)")},
      {buildRenderingTab(), tr("Rendering")},
      {buildSceneTab(), tr("3D")},
      {buildMetadataTab(), tr("Metadata")},
    };

    for (const auto &page : pages)
    {
      // A null page is a tab this layer cannot answer for. Adding it anyway
      // would offer an empty form, which is what the old dialog avoided by
      // refusing to open at all — the wrong half of the choice.
      if (page.page)
      {
        m_tabs->addTab(page.page, page.title);
      }
    }

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

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_tabs, 1);
    layout->addWidget(m_statusLabel);
    layout->addWidget(buttons);
  }

  QWidget *LayerPropertiesDialog::buildInformationTab()
  {
    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("informationTab"));

    auto *form = new QFormLayout(page);

    form->addRow(tr("Type"),
                 factLabel(m_layer ? typeNameOf(m_layer) : QString(), page,
                           QStringLiteral("informationTypeLabel")));

    if (const IAttributeProvider *provider = providerFor(m_layer))
    {
      form->addRow(tr("Features"),
                   factLabel(QString::number(provider->featureCount()), page,
                             QStringLiteral("informationFeatureCountLabel")));
    }

    form->addRow(tr("Extent"),
                 factLabel(m_layer ? formatExtent(m_layer->extent())
                                   : QString(),
                           page,
                           QStringLiteral("informationExtentLabel")));

    form->addRow(tr("Coordinate system"),
                 factLabel(describeCrs(m_layer ? m_layer->crs() : nullptr),
                           page, QStringLiteral("informationCrsLabel")));

    return page;
  }

  QWidget *LayerPropertiesDialog::buildSourceTab()
  {
    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("sourceTab"));

    auto *form = new QFormLayout(page);

    m_nameEdit = new QLineEdit(page);
    m_nameEdit->setObjectName(QStringLiteral("layerNameEdit"));
    form->addRow(tr("Layer name"), m_nameEdit);

    const QString source = m_layer ? m_layer->sourceDescription() : QString();

    form->addRow(tr("Data source"),
                 factLabel(source.isEmpty() ? tr("Built in memory") : source,
                           page, QStringLiteral("layerSourceLabel")));

    m_crsLabel = factLabel(QString(), page, QStringLiteral("layerCrsLabel"));

    auto *assign = new QPushButton(tr("Assign…"), page);
    assign->setObjectName(QStringLiteral("assignCrsButton"));
    assign->setToolTip(
      tr("Declare what system this layer's coordinates are already in. This "
         "does not move them."));

    // From clicked(), a release — a modal opened from a mouse press wedges
    // input on macOS.
    connect(assign, &QPushButton::clicked, this,
            [this] { assignCrs(); });

    auto *crsRow = new QWidget(page);
    auto *crsLayout = new QHBoxLayout(crsRow);
    crsLayout->setContentsMargins(0, 0, 0, 0);
    crsLayout->addWidget(m_crsLabel, 1);
    crsLayout->addWidget(assign);

    form->addRow(tr("Coordinate system"), crsRow);

    refreshCrsRow();

    return page;
  }

  void LayerPropertiesDialog::refreshCrsRow()
  {
    if (!m_crsLabel)
    {
      return;
    }

    if (m_pendingCrs)
    {
      // Marked as pending rather than shown plainly, because until Apply the
      // layer still holds the other one and the map still draws it there.
      m_crsLabel->setText(tr("%1 — will be assigned")
                            .arg(describeCrs(m_pendingCrs.get())));

      return;
    }

    m_crsLabel->setText(describeCrs(m_layer ? m_layer->crs() : nullptr));
  }

  void LayerPropertiesDialog::assignCrs()
  {
    if (!m_layer)
    {
      return;
    }

    CrsSelectionDialog chooser(this);
    chooser.setCurrentCrs(m_pendingCrs ? m_pendingCrs.get() : m_layer->crs());

    if (chooser.exec() != QDialog::Accepted)
    {
      return;
    }

    QString message;
    std::shared_ptr<SpatialReference> chosen = chooser.selectedCrs(message);

    if (!chosen)
    {
      QMessageBox::warning(this, tr("Cannot use that system"), message);

      return;
    }

    // A layer that already declares a system is the case worth stopping on:
    // whoever is here usually means "put this in that system", which assigning
    // does not do. The map already reprojects on the fly, so there is nothing
    // to move — but saying that once is cheaper than a map silently landing
    // in the wrong hemisphere.
    if (m_layer->crs() && !m_layer->crs()->isSameAs(*chosen))
    {
      const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Assign a different coordinate system?"),
        tr("%1 is currently declared as %2.\n\n"
           "Assigning %3 does not move its coordinates — it changes what they "
           "are taken to mean, which is a correction to make when the file "
           "declares the wrong system. The map already draws every layer in "
           "the map's own system, so nothing needs converting to see them "
           "together.")
          .arg(m_layer->name(), describeCrs(m_layer->crs()),
               describeCrs(chosen.get())),
        QMessageBox::Cancel | QMessageBox::Ok, QMessageBox::Cancel);

      if (answer != QMessageBox::Ok)
      {
        return;
      }
    }

    m_pendingCrs = std::move(chosen);

    refreshCrsRow();
  }

  QWidget *LayerPropertiesDialog::buildSymbologyTab()
  {
    if (!hasStyle())
    {
      return nullptr;
    }

    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("symbologyTab"));

    auto *form = new QFormLayout(page);

    m_modeCombo = new QComboBox(page);
    m_modeCombo->setObjectName(QStringLiteral("styleModeCombo"));
    m_modeCombo->addItem(
      tr("Single symbol"),
      QVariant::fromValue(static_cast<int>(StyleMode::Single)));
    m_modeCombo->addItem(
      tr("Graduated"),
      QVariant::fromValue(static_cast<int>(StyleMode::Graduated)));
    m_modeCombo->addItem(
      tr("Categorized"),
      QVariant::fromValue(static_cast<int>(StyleMode::Categorized)));
    form->addRow(tr("Colour by"), m_modeCombo);

    m_attributeCombo = new QComboBox(page);
    m_attributeCombo->setObjectName(QStringLiteral("styleAttributeCombo"));
    form->addRow(tr("Attribute"), m_attributeCombo);

    m_methodCombo = new QComboBox(page);
    m_methodCombo->setObjectName(QStringLiteral("styleMethodCombo"));

    for (const ClassificationMethod method :
         {ClassificationMethod::EqualInterval, ClassificationMethod::Quantile,
          ClassificationMethod::NaturalBreaks, ClassificationMethod::Manual})
    {
      m_methodCombo->addItem(Classification::methodName(method),
                             QVariant::fromValue(static_cast<int>(method)));
    }

    form->addRow(tr("Method"), m_methodCombo);

    m_classCountSpin = new QSpinBox(page);
    m_classCountSpin->setObjectName(QStringLiteral("styleClassCountSpin"));
    m_classCountSpin->setRange(1, 24);
    form->addRow(tr("Classes"), m_classCountSpin);

    m_rampCombo = new QComboBox(page);
    m_rampCombo->setObjectName(QStringLiteral("styleRampCombo"));
    m_rampCombo->setIconSize(QSize(kRampPreviewWidth, kRampPreviewHeight));

    for (const QString &name : ColorRamp::builtinNames())
    {
      m_rampCombo->addItem(rampPreview(ColorRamp::builtin(name)), name, name);
    }

    form->addRow(tr("Colour ramp"), m_rampCombo);

    m_fillButton = new QPushButton(page);
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

    m_sizeSpin = new QDoubleSpinBox(page);
    m_sizeSpin->setObjectName(QStringLiteral("styleSizeSpin"));
    m_sizeSpin->setRange(0.5, 64.0);
    m_sizeSpin->setSuffix(tr(" px"));
    // "(2D)": the scene has no point or line width, so this is the one
    // symbology setting that stops at the map.
    form->addRow(tr("Symbol size (2D)"), m_sizeSpin);

    connect(m_modeCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { updateEnabledState(); });

    return page;
  }

  QWidget *LayerPropertiesDialog::buildLabelsTab()
  {
    // Labelling lives inside LayerStyle (D19), so a layer with no style has
    // nothing to label with either.
    if (!hasStyle())
    {
      return nullptr;
    }

    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("labelsTab"));

    auto *form = new QFormLayout(page);

    m_labelsCheck = new QCheckBox(tr("Label features"), page);
    m_labelsCheck->setObjectName(QStringLiteral("styleLabelsCheck"));
    form->addRow(m_labelsCheck);

    m_labelFieldCombo = new QComboBox(page);
    m_labelFieldCombo->setObjectName(QStringLiteral("styleLabelFieldCombo"));
    form->addRow(tr("Label with"), m_labelFieldCombo);

    connect(m_labelsCheck, &QCheckBox::toggled, this,
            [this](bool) { updateEnabledState(); });

    return page;
  }

  QWidget *LayerPropertiesDialog::buildRenderingTab()
  {
    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("renderingTab"));

    auto *form = new QFormLayout(page);

    m_visibleCheck = new QCheckBox(tr("Draw this layer"), page);
    m_visibleCheck->setObjectName(QStringLiteral("renderingVisibleCheck"));
    form->addRow(m_visibleCheck);

    m_opacitySpin = new QSpinBox(page);
    m_opacitySpin->setObjectName(QStringLiteral("renderingOpacitySpin"));
    m_opacitySpin->setRange(0, 100);
    m_opacitySpin->setSuffix(tr(" %"));
    form->addRow(tr("Opacity"), m_opacitySpin);

    return page;
  }

  QWidget *LayerPropertiesDialog::buildSceneTab()
  {
    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("sceneTab"));

    auto *form = new QFormLayout(page);

    ISceneSource *scene = sceneSource();

    // The tab exists either way. A point layer's user checked "Draw this
    // layer", saw nothing in 3D, and was told nothing; an absent tab is the
    // same silence with better manners. The explanation is the control.
    if (!scene)
    {
      auto *why = new QLabel(
        tr("This layer has no 3D form. The map draws it, but there is "
           "nothing the scene could place against the terrain — point "
           "layers, for instance, are not drawn in 3D yet."),
        page);
      why->setObjectName(QStringLiteral("sceneAbsenceLabel"));
      why->setWordWrap(true);
      form->addRow(why);

      return page;
    }

    m_shownIn3dCheck = new QCheckBox(tr("Show this layer in 3D"), page);
    m_shownIn3dCheck->setObjectName(QStringLiteral("sceneShownIn3dCheck"));
    form->addRow(m_shownIn3dCheck);

    m_drapeCombo = new QComboBox(page);
    m_drapeCombo->setObjectName(QStringLiteral("renderingDrapeCombo"));
    m_drapeCombo->addItem(
      tr("Flat — at zero elevation"),
      QVariant::fromValue(static_cast<int>(SceneDrape::Flat)));
    m_drapeCombo->addItem(
      tr("Draped over the terrain"),
      QVariant::fromValue(static_cast<int>(SceneDrape::Terrain)));

    // Offered only where it does something. A surface is already a surface,
    // so extruding it would silently behave as draping and read as a control
    // that does not work.
    if (scene->supportsExtrusion())
    {
      m_drapeCombo->addItem(
        tr("Extruded above the terrain"),
        QVariant::fromValue(static_cast<int>(SceneDrape::Extruded)));
    }

    form->addRow(tr("3D placement"), m_drapeCombo);

    if (scene->supportsExtrusion())
    {
      m_extrusionSpin = new QDoubleSpinBox(page);
      m_extrusionSpin->setObjectName(
        QStringLiteral("renderingExtrusionSpin"));

      // Wide, and in map units: the unit is whatever the map's CRS measures
      // in, so the same number is metres in one system and degrees in
      // another and no range narrower than this fits both.
      m_extrusionSpin->setRange(-100000.0, 100000.0);
      m_extrusionSpin->setDecimals(3);
      form->addRow(tr("Extrusion height"), m_extrusionSpin);
    }

    connect(m_drapeCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { updateEnabledState(); });

    return page;
  }

  ISceneSource *LayerPropertiesDialog::sceneSource() const
  {
    return m_layer ? m_layer->sceneSource() : nullptr;
  }

  QWidget *LayerPropertiesDialog::buildMetadataTab()
  {
    const QString attribution = m_layer ? m_layer->attribution() : QString();

    if (attribution.isEmpty())
    {
      return nullptr;
    }

    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("metadataTab"));

    auto *form = new QFormLayout(page);

    form->addRow(tr("Attribution"),
                 factLabel(attribution, page,
                           QStringLiteral("metadataAttributionLabel")));

    return page;
  }

  void LayerPropertiesDialog::loadFromLayer()
  {
    if (!m_layer)
    {
      return;
    }

    m_nameEdit->setText(m_layer->name());
    m_visibleCheck->setChecked(m_layer->isVisible());
    m_opacitySpin->setValue(qRound(m_layer->opacity() * 100.0));

    if (m_shownIn3dCheck)
    {
      m_shownIn3dCheck->setChecked(m_layer->isShownIn3D());
    }

    if (const ISceneSource *scene = sceneSource(); scene && m_drapeCombo)
    {
      const int index =
        m_drapeCombo->findData(static_cast<int>(scene->drape()));

      // A layer left on Extruded whose combo no longer offers it cannot be
      // shown truthfully, so it falls back rather than showing the first row
      // as though that were the setting.
      m_drapeCombo->setCurrentIndex(index >= 0 ? index : 0);

      if (m_extrusionSpin)
      {
        m_extrusionSpin->setValue(scene->extrusionHeight());
      }
    }

    const LayerStyle *style = m_layer->style();

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

  void LayerPropertiesDialog::updateEnabledState()
  {
    if (m_extrusionSpin && m_drapeCombo)
    {
      // A height only means something to a curtain; on a flat or draped
      // layer it is a number that changes nothing.
      m_extrusionSpin->setEnabled(
        static_cast<SceneDrape>(m_drapeCombo->currentData().toInt())
        == SceneDrape::Extruded);
    }

    if (!hasStyle())
    {
      return;
    }

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

  bool LayerPropertiesDialog::apply()
  {
    if (!m_layer)
    {
      return false;
    }

    if (m_pendingCrs)
    {
      m_layer->setCrs(m_pendingCrs);
      m_pendingCrs.reset();
      refreshCrsRow();
    }

    m_layer->setName(m_nameEdit->text());
    m_layer->setVisible(m_visibleCheck->isChecked());
    m_layer->setOpacity(m_opacitySpin->value() / 100.0);

    if (m_shownIn3dCheck)
    {
      m_layer->setShownIn3D(m_shownIn3dCheck->isChecked());
    }

    if (ISceneSource *scene = sceneSource(); scene && m_drapeCombo)
    {
      scene->setDrape(
        static_cast<SceneDrape>(m_drapeCombo->currentData().toInt()));

      if (m_extrusionSpin)
      {
        scene->setExtrusionHeight(m_extrusionSpin->value());
      }
    }

    LayerStyle *style = m_layer->style();

    if (!style)
    {
      // Nothing failed: a basemap has no style, and its name, visibility and
      // opacity have all just been applied. No notifyAppearanceChanged() here
      // — setVisible() and setOpacity() announce themselves, and a rename is
      // not a repaint. The styled path below needs it because a style is
      // edited through style(), where the layer never sees the change.
      return true;
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
