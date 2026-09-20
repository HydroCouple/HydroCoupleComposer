#include "ui/dialogs/preferencesdialog.h"

#include "core/preferencesmanager.h"
#include "scene/axisgizmo.h"
#include "scene/navigation.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace HydroCouple::Composer
{

  namespace
  {
    const char *const kColorProperty = "color";

    //! Paints \a color onto \a button as its swatch and records it.
    void showColor(QToolButton *button, const QColor &color)
    {
      QPixmap swatch(24, 16);
      swatch.fill(color);
      button->setIcon(QIcon(swatch));
      button->setText(color.name());
      button->setProperty(kColorProperty, color);
    }

    [[nodiscard]] QColor colorOf(const QToolButton *button)
    {
      return button->property(kColorProperty).value<QColor>();
    }

    //! A button that shows a colour and asks for another when clicked.
    QToolButton *makeColorButton(const QString &objectName, QWidget *parent)
    {
      auto *button = new QToolButton(parent);
      button->setObjectName(objectName);
      button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
      showColor(button, Qt::black);

      QObject::connect(button, &QToolButton::clicked, button,
                       [button]
                       {
                         const QColor chosen = QColorDialog::getColor(
                           colorOf(button), button, {},
                           QColorDialog::DontUseNativeDialog);

                         if (chosen.isValid())
                         {
                           showColor(button, chosen);
                         }
                       });

      return button;
    }

    //! Every page scrolls rather than squeezing when the dialog shrinks.
    QWidget *scrolled(QWidget *page, QWidget *parent)
    {
      auto *area = new QScrollArea(parent);
      area->setWidgetResizable(true);
      area->setFrameShape(QFrame::NoFrame);
      area->setWidget(page);

      return area;
    }
  } // namespace

  PreferencesDialog::PreferencesDialog(PreferencesManager *preferences,
                                       QWidget *parent)
    : QDialog(parent), m_preferences(preferences)
  {
    setObjectName(QStringLiteral("preferencesDialog"));
    setWindowTitle(tr("Preferences"));
    resize(720, 480);

    auto *root = new QVBoxLayout(this);

    auto *split = new QHBoxLayout;
    root->addLayout(split, 1);

    m_categories = new QListWidget(this);
    m_categories->setObjectName(QStringLiteral("categories"));
    m_categories->setMinimumWidth(160);
    m_categories->setMaximumWidth(200);
    split->addWidget(m_categories);

    m_pages = new QStackedWidget(this);
    m_pages->setObjectName(QStringLiteral("pages"));
    split->addWidget(m_pages, 1);

    // Titles are the manager's group names, so openAtCategory() and the
    // manager's preferenceChanged() speak the same vocabulary.
    addCategory(QStringLiteral("General"), buildGeneralPage());
    addCategory(QStringLiteral("Appearance"), buildAppearancePage());
    addCategory(QStringLiteral("Components"), buildComponentsPage());
    addCategory(QStringLiteral("Selection"), buildSelectionPage());
    addCategory(QStringLiteral("Map"), buildMapPage());
    addCategory(QStringLiteral("3D View"), buildScenePage());

    connect(m_categories, &QListWidget::currentRowChanged, m_pages,
            &QStackedWidget::setCurrentIndex);
    m_categories->setCurrentRow(0);

    auto *buttonRow = new QHBoxLayout;
    root->addLayout(buttonRow);

    auto *reset = new QPushButton(tr("Reset to defaults"), this);
    reset->setObjectName(QStringLiteral("resetButton"));
    connect(reset, &QPushButton::clicked, this,
            &PreferencesDialog::resetToDefaults);
    buttonRow->addWidget(reset);
    buttonRow->addStretch(1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Apply
                                           | QDialogButtonBox::Cancel
                                           | QDialogButtonBox::Ok,
                                         this);
    buttons->setObjectName(QStringLiteral("buttons"));
    buttonRow->addWidget(buttons);

    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &PreferencesDialog::apply);
    connect(buttons, &QDialogButtonBox::accepted, this,
            [this]
            {
              apply();
              accept();
            });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    revert();
  }

  void PreferencesDialog::addCategory(const QString &title, QWidget *page)
  {
    m_categories->addItem(title);
    m_pages->addWidget(scrolled(page, m_pages));
  }

  void PreferencesDialog::openAtCategory(const QString &title)
  {
    for (int row = 0; row < m_categories->count(); ++row)
    {
      if (m_categories->item(row)->text() == title)
      {
        m_categories->setCurrentRow(row);

        return;
      }
    }
  }

  QStringList PreferencesDialog::categories() const
  {
    QStringList titles;

    for (int row = 0; row < m_categories->count(); ++row)
    {
      titles.append(m_categories->item(row)->text());
    }

    return titles;
  }

  void PreferencesDialog::setCrsChooser(
    std::function<QString(const QString &)> chooser)
  {
    m_crsChooser = std::move(chooser);
    m_chooseCrs->setVisible(static_cast<bool>(m_crsChooser));
  }

  // ── Pages ──────────────────────────────────────────────────────────────

  QWidget *PreferencesDialog::buildGeneralPage()
  {
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);

    m_showWelcome = new QCheckBox(tr("Show the welcome page on start up"), page);
    m_showWelcome->setObjectName(QStringLiteral("showWelcome"));
    form->addRow(QString(), m_showWelcome);

    m_recentLimit = new QSpinBox(page);
    m_recentLimit->setObjectName(QStringLiteral("recentLimit"));
    m_recentLimit->setRange(1, 50);
    form->addRow(tr("Recent compositions to remember"), m_recentLimit);

    return page;
  }

  QWidget *PreferencesDialog::buildAppearancePage()
  {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    auto *group = new QGroupBox(tr("Theme"), page);
    auto *choices = new QVBoxLayout(group);

    m_themeSystem = new QRadioButton(tr("Follow the system"), group);
    m_themeSystem->setObjectName(QStringLiteral("themeSystem"));
    m_themeLight = new QRadioButton(tr("Light"), group);
    m_themeLight->setObjectName(QStringLiteral("themeLight"));
    m_themeDark = new QRadioButton(tr("Dark"), group);
    m_themeDark->setObjectName(QStringLiteral("themeDark"));

    choices->addWidget(m_themeSystem);
    choices->addWidget(m_themeLight);
    choices->addWidget(m_themeDark);

    layout->addWidget(group);
    layout->addStretch(1);

    return page;
  }

  QWidget *PreferencesDialog::buildComponentsPage()
  {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    layout->addWidget(new QLabel(
      tr("Directories scanned for component libraries:"), page));

    m_searchPaths = new QListWidget(page);
    m_searchPaths->setObjectName(QStringLiteral("searchPaths"));
    layout->addWidget(m_searchPaths, 1);

    auto *row = new QHBoxLayout;
    layout->addLayout(row);

    auto *add = new QPushButton(tr("Add…"), page);
    add->setObjectName(QStringLiteral("addSearchPath"));
    connect(add, &QPushButton::clicked, this,
            [this]
            {
              const QString directory = QFileDialog::getExistingDirectory(
                this, tr("Component library directory"));

              if (!directory.isEmpty()
                  && m_searchPaths->findItems(directory, Qt::MatchExactly)
                       .isEmpty())
              {
                m_searchPaths->addItem(directory);
              }
            });
    row->addWidget(add);

    auto *remove = new QPushButton(tr("Remove"), page);
    remove->setObjectName(QStringLiteral("removeSearchPath"));
    connect(remove, &QPushButton::clicked, this,
            [this] { delete m_searchPaths->takeItem(m_searchPaths->currentRow()); });
    row->addWidget(remove);
    row->addStretch(1);

    m_rescanOnStartUp = new QCheckBox(tr("Scan these directories on start up"),
                                      page);
    m_rescanOnStartUp->setObjectName(QStringLiteral("rescanOnStartUp"));
    layout->addWidget(m_rescanOnStartUp);

    return page;
  }

  QWidget *PreferencesDialog::buildSelectionPage()
  {
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);

    m_pickTolerance = new QDoubleSpinBox(page);
    m_pickTolerance->setObjectName(QStringLiteral("pickTolerance"));
    m_pickTolerance->setRange(1.0, 40.0);
    m_pickTolerance->setDecimals(1);
    m_pickTolerance->setSuffix(tr(" px"));
    m_pickTolerance->setToolTip(
      tr("How near a click has to land to a feature to select it."));
    form->addRow(tr("Pick tolerance"), m_pickTolerance);

    m_dragThreshold = new QSpinBox(page);
    m_dragThreshold->setObjectName(QStringLiteral("dragThreshold"));
    m_dragThreshold->setRange(0, 20);
    m_dragThreshold->setSuffix(tr(" px"));
    m_dragThreshold->setToolTip(
      tr("How far the mouse may move and still count as a click."));
    form->addRow(tr("Drag threshold"), m_dragThreshold);

    m_snapTolerance = new QDoubleSpinBox(page);
    m_snapTolerance->setObjectName(QStringLiteral("snapTolerance"));
    m_snapTolerance->setRange(1.0, 40.0);
    m_snapTolerance->setDecimals(1);
    m_snapTolerance->setSuffix(tr(" px"));
    m_snapTolerance->setToolTip(
      tr("How close to a vertex or edge counts as on it when drawing."));
    form->addRow(tr("Snap tolerance"), m_snapTolerance);

    m_selectionColor = makeColorButton(QStringLiteral("selectionColor"), page);
    form->addRow(tr("Selection colour"), m_selectionColor);

    return page;
  }

  QWidget *PreferencesDialog::buildMapPage()
  {
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);

    auto *row = new QWidget(page);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);

    m_defaultCrs = new QLineEdit(row);
    m_defaultCrs->setObjectName(QStringLiteral("defaultCrs"));
    m_defaultCrs->setPlaceholderText(QStringLiteral("EPSG:3857"));
    m_defaultCrs->setToolTip(
      tr("The coordinate system a new map opens in, as AUTHORITY:CODE."));
    rowLayout->addWidget(m_defaultCrs, 1);

    m_chooseCrs = new QToolButton(row);
    m_chooseCrs->setObjectName(QStringLiteral("chooseCrs"));
    m_chooseCrs->setText(tr("Choose…"));
    m_chooseCrs->setVisible(false);
    connect(m_chooseCrs, &QToolButton::clicked, this,
            [this]
            {
              if (!m_crsChooser)
              {
                return;
              }

              const QString chosen = m_crsChooser(m_defaultCrs->text());

              if (!chosen.isEmpty())
              {
                m_defaultCrs->setText(chosen);
              }
            });
    rowLayout->addWidget(m_chooseCrs);

    form->addRow(tr("Default coordinate system"), row);

    return page;
  }

  QWidget *PreferencesDialog::buildScenePage()
  {
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);

    m_sceneBackground =
      makeColorButton(QStringLiteral("sceneBackground"), page);
    form->addRow(tr("Background colour"), m_sceneBackground);

    m_sceneProjection = new QComboBox(page);
    m_sceneProjection->setObjectName(QStringLiteral("sceneProjection"));
    m_sceneProjection->addItem(tr("Perspective"),
                               QStringLiteral("Perspective"));
    m_sceneProjection->addItem(tr("Orthographic"),
                               QStringLiteral("Orthographic"));
    form->addRow(tr("Default projection"), m_sceneProjection);

    m_exaggeration = new QDoubleSpinBox(page);
    m_exaggeration->setObjectName(QStringLiteral("exaggeration"));
    m_exaggeration->setRange(0.1, 100.0);
    m_exaggeration->setSingleStep(0.5);
    m_exaggeration->setPrefix(tr("×"));
    form->addRow(tr("Default vertical exaggeration"), m_exaggeration);

    m_showGizmo = new QCheckBox(tr("Show the orientation cue"), page);
    m_showGizmo->setObjectName(QStringLiteral("showGizmo"));
    form->addRow(QString(), m_showGizmo);

    m_gizmoSize = new QSpinBox(page);
    m_gizmoSize->setObjectName(QStringLiteral("gizmoSize"));
    // The floor is the manager's too: a viewport of zero pixels is not an
    // invisible gizmo, it is a validation error on some backends. Set it
    // here as well so the dialog cannot ask for one in the first place.
    m_gizmoSize->setRange(24, 256);
    m_gizmoSize->setSingleStep(8);
    m_gizmoSize->setSuffix(tr(" px"));
    form->addRow(tr("Orientation cue size"), m_gizmoSize);

    m_gizmoCorner = new QComboBox(page);
    m_gizmoCorner->setObjectName(QStringLiteral("gizmoCorner"));
    m_gizmoCorner->addItem(tr("Bottom left"),
                           gizmoCornerName(GizmoCorner::BottomLeft));
    m_gizmoCorner->addItem(tr("Bottom right"),
                           gizmoCornerName(GizmoCorner::BottomRight));
    m_gizmoCorner->addItem(tr("Top left"),
                           gizmoCornerName(GizmoCorner::TopLeft));
    m_gizmoCorner->addItem(tr("Top right"),
                           gizmoCornerName(GizmoCorner::TopRight));
    form->addRow(tr("Orientation cue corner"), m_gizmoCorner);

    m_linkViews = new QComboBox(page);
    m_linkViews->setObjectName(QStringLiteral("linkViews"));
    m_linkViews->addItem(tr("On tab switch"),
                         viewLinkName(ViewLink::OnTabSwitch));
    m_linkViews->addItem(tr("Never"), viewLinkName(ViewLink::Never));
    m_linkViews->setToolTip(
      tr("Whether arriving at a view frames it on what the other one was "
         "showing."));
    form->addRow(tr("Link 2D and 3D views"), m_linkViews);

    m_orbitSensitivity = new QDoubleSpinBox(page);
    m_orbitSensitivity->setObjectName(QStringLiteral("orbitSensitivity"));
    // The same bounds orbitStep() enforces. Stated here too so the spin
    // box cannot offer a value the camera will quietly refuse — a control
    // that accepts a number and then ignores it is worse than one that
    // will not accept it.
    m_orbitSensitivity->setRange(0.05, 1.2);
    m_orbitSensitivity->setSingleStep(0.05);
    m_orbitSensitivity->setDecimals(2);
    m_orbitSensitivity->setSuffix(tr("°/px"));
    form->addRow(tr("Orbit sensitivity"), m_orbitSensitivity);

    m_invertWheel = new QCheckBox(tr("Invert the 3D scroll wheel"), page);
    m_invertWheel->setObjectName(QStringLiteral("invertWheel"));
    form->addRow(QString(), m_invertWheel);

    m_panModifier = new QComboBox(page);
    m_panModifier->setObjectName(QStringLiteral("panModifier"));
    m_panModifier->addItem(tr("Middle or right drag"),
                           panModifierName(PanModifier::MiddleDrag));
    m_panModifier->addItem(tr("Middle, right, or Shift and left drag"),
                           panModifierName(PanModifier::ShiftDrag));
    m_panModifier->setToolTip(
      tr("Middle and right always pan. The second choice adds Shift with "
         "the left button, for trackpads and mice with no middle button."));
    form->addRow(tr("Pan the 3D view with"), m_panModifier);

    // The size and the corner say nothing while the cue is hidden.
    connect(m_showGizmo, &QCheckBox::toggled, m_gizmoSize,
            &QWidget::setEnabled);
    connect(m_showGizmo, &QCheckBox::toggled, m_gizmoCorner,
            &QWidget::setEnabled);

    return page;
  }

  // ── Manager ⟷ widgets ──────────────────────────────────────────────────

  void PreferencesDialog::revert()
  {
    const PreferencesManager &prefs = *m_preferences;

    m_showWelcome->setChecked(prefs.showWelcomeOnStartUp());
    m_recentLimit->setValue(prefs.recentLimit());

    // The one that applies is checked; the others follow, because an
    // exclusive radio button cannot be unchecked by setChecked(false) —
    // clearing the two that do not apply would leave the old choice lit.
    const QString theme = prefs.themeMode();
    (theme == QLatin1String("Light")  ? m_themeLight
     : theme == QLatin1String("Dark") ? m_themeDark
                                      : m_themeSystem)
      ->setChecked(true);

    m_searchPaths->clear();
    m_searchPaths->addItems(prefs.componentSearchPaths());
    m_rescanOnStartUp->setChecked(prefs.rescanComponentsOnStartUp());

    m_pickTolerance->setValue(prefs.pickTolerancePixels());
    m_dragThreshold->setValue(prefs.dragThresholdPixels());
    m_snapTolerance->setValue(prefs.snapTolerancePixels());
    showColor(m_selectionColor, prefs.selectionColor());

    m_defaultCrs->setText(prefs.defaultMapCrs());

    showColor(m_sceneBackground, prefs.sceneBackgroundColor());
    m_sceneProjection->setCurrentIndex(
      m_sceneProjection->findData(prefs.defaultSceneProjection()));
    m_exaggeration->setValue(prefs.defaultVerticalExaggeration());

    m_showGizmo->setChecked(prefs.showAxisGizmo());
    m_gizmoSize->setValue(prefs.axisGizmoSizePixels());

    // Through the enum and back, so that a stored name nobody recognises
    // shows the corner it will actually be drawn in rather than leaving
    // the box on whatever happened to be there.
    m_gizmoCorner->setCurrentIndex(m_gizmoCorner->findData(
      gizmoCornerName(gizmoCornerFromName(prefs.axisGizmoCorner()))));

    m_gizmoSize->setEnabled(m_showGizmo->isChecked());
    m_gizmoCorner->setEnabled(m_showGizmo->isChecked());

    // Each through its enum and back, so a stored name nobody recognises
    // shows the behaviour that will actually be used rather than leaving
    // the box on whatever happened to be there.
    m_linkViews->setCurrentIndex(m_linkViews->findData(
      viewLinkName(viewLinkFromName(prefs.linkViews()))));
    m_orbitSensitivity->setValue(prefs.orbitDegreesPerPixel());
    m_invertWheel->setChecked(prefs.invertWheel());
    m_panModifier->setCurrentIndex(m_panModifier->findData(
      panModifierName(panModifierFromName(prefs.panModifier()))));
  }

  void PreferencesDialog::apply()
  {
    PreferencesManager &prefs = *m_preferences;

    // Every key is written; the manager announces only the ones that
    // changed, so listeners are not asked to redo work for the rest.
    prefs.setShowWelcomeOnStartUp(m_showWelcome->isChecked());
    prefs.setRecentLimit(m_recentLimit->value());

    prefs.setThemeMode(m_themeLight->isChecked()  ? QStringLiteral("Light")
                       : m_themeDark->isChecked() ? QStringLiteral("Dark")
                                                  : QStringLiteral("System"));

    QStringList paths;

    for (int row = 0; row < m_searchPaths->count(); ++row)
    {
      paths.append(m_searchPaths->item(row)->text());
    }

    prefs.setComponentSearchPaths(paths);
    prefs.setRescanComponentsOnStartUp(m_rescanOnStartUp->isChecked());

    prefs.setPickTolerancePixels(m_pickTolerance->value());
    prefs.setDragThresholdPixels(m_dragThreshold->value());
    prefs.setSnapTolerancePixels(m_snapTolerance->value());
    prefs.setSelectionColor(colorOf(m_selectionColor));

    const QString crs = m_defaultCrs->text().trimmed();
    prefs.setDefaultMapCrs(crs.isEmpty() ? QStringLiteral("EPSG:3857") : crs);

    prefs.setSceneBackgroundColor(colorOf(m_sceneBackground));
    prefs.setDefaultSceneProjection(
      m_sceneProjection->currentData().toString());
    prefs.setDefaultVerticalExaggeration(m_exaggeration->value());

    prefs.setShowAxisGizmo(m_showGizmo->isChecked());
    prefs.setAxisGizmoSizePixels(m_gizmoSize->value());
    prefs.setAxisGizmoCorner(m_gizmoCorner->currentData().toString());

    prefs.setLinkViews(m_linkViews->currentData().toString());
    prefs.setOrbitDegreesPerPixel(m_orbitSensitivity->value());
    prefs.setInvertWheel(m_invertWheel->isChecked());
    prefs.setPanModifier(m_panModifier->currentData().toString());
  }

  void PreferencesDialog::resetToDefaults()
  {
    // Applied at once rather than staged: "reset" is a decision, and a
    // reset that Cancel could undo would be a reset nobody can trust.
    m_preferences->resetToDefaults();
    revert();
  }

} // namespace HydroCouple::Composer
