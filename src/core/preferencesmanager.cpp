#include "core/preferencesmanager.h"

#include <QSettings>

#include <algorithm>

namespace HydroCouple::Composer
{

  namespace
  {
    // Group names double as the dialog's category titles, so they are the
    // user-facing spelling; key names are the code's.
    const QString kGeneral = QStringLiteral("General");
    const QString kAppearance = QStringLiteral("Appearance");
    const QString kComponents = QStringLiteral("Components");
    const QString kSelection = QStringLiteral("Selection");
    const QString kMap = QStringLiteral("Map");
    const QString kScene = QStringLiteral("3D View");

    // The stored key keeps the pre-existing "appearance/mode" spelling so a
    // choice made before this manager existed is still honoured.
    QString storageName(const QString &group, const QString &name)
    {
      if (group == kAppearance && name == QLatin1String("themeMode"))
      {
        return QStringLiteral("appearance/mode");
      }

      // Under one prefix, because "[General]" is a reserved section in
      // Qt's ini format whose keys are read back without a prefix: a key
      // stored as "general/x" is written into it and comes back as "x"
      // after a restart, which is to say lost.
      QString folded = group.toLower();
      folded.remove(QLatin1Char(' '));

      return QStringLiteral("preferences/") + folded + QLatin1Char('/')
             + name;
    }
  } // namespace

  const QVector<PreferencesManager::Key> &PreferencesManager::keys()
  {
    // Defaults are the values the code hard-coded before they were
    // preferences, so a fresh install behaves exactly as it did.
    static const QVector<Key> table = {
      {kGeneral, QStringLiteral("showWelcomeOnStartUp"), true},
      {kGeneral, QStringLiteral("recentLimit"), 10},
      {kAppearance, QStringLiteral("themeMode"), QStringLiteral("System")},
      {kComponents, QStringLiteral("searchPaths"), QStringList()},
      {kComponents, QStringLiteral("rescanOnStartUp"), true},
      {kSelection, QStringLiteral("pickTolerancePixels"), 6.0},
      {kSelection, QStringLiteral("dragThresholdPixels"), 3},
      {kSelection, QStringLiteral("snapTolerancePixels"), 10.0},
      {kSelection, QStringLiteral("selectionColor"), QColor(0, 200, 255)},
      {kMap, QStringLiteral("defaultCrs"), QStringLiteral("EPSG:3857")},
      {kScene, QStringLiteral("backgroundColor"), QColor(0x1a, 0x1d, 0x21)},
      {kScene, QStringLiteral("defaultProjection"),
       QStringLiteral("Perspective")},
      {kScene, QStringLiteral("defaultVerticalExaggeration"), 1.0},
      {kScene, QStringLiteral("showAxisGizmo"), true},
      {kScene, QStringLiteral("axisGizmoSizePixels"), 96},
      {kScene, QStringLiteral("axisGizmoCorner"),
       QStringLiteral("BottomLeft")},
      {kScene, QStringLiteral("linkViews"),
       QStringLiteral("OnTabSwitch")},
      {kScene, QStringLiteral("orbitDegreesPerPixel"), 0.4},
      {kScene, QStringLiteral("invertWheel"), false},
      {kScene, QStringLiteral("panModifier"), QStringLiteral("MiddleDrag")},
    };

    return table;
  }

  PreferencesManager::PreferencesManager(QSettings *settings, QObject *parent)
    : QObject(parent), m_settings(settings)
  {
  }

  PreferencesManager *PreferencesManager::instance()
  {
    static PreferencesManager manager;

    return &manager;
  }

  const PreferencesManager::Key *PreferencesManager::find(const QString &group,
                                                          const QString &name)
  {
    for (const Key &key : keys())
    {
      if (key.group == group && key.name == name)
      {
        return &key;
      }
    }

    return nullptr;
  }

  QString PreferencesManager::settingsKey(const Key &key)
  {
    return storageName(key.group, key.name);
  }

  QVariant PreferencesManager::value(const QString &group,
                                     const QString &name) const
  {
    const Key *key = find(group, name);

    if (!key)
    {
      return {};
    }

    QSettings own;
    QSettings &settings = m_settings ? *m_settings : own;

    QVariant stored = settings.value(settingsKey(*key), key->fallback);

    // An ini store hands strings back; the default's type is what callers
    // expect, so the stored value is coerced to it. A value that cannot be
    // coerced is treated as absent rather than handed on as garbage.
    if (stored.userType() != key->fallback.userType()
        && !stored.convert(key->fallback.metaType()))
    {
      return key->fallback;
    }

    return stored;
  }

  bool PreferencesManager::setValue(const QString &group, const QString &name,
                                    const QVariant &value)
  {
    const Key *key = find(group, name);

    if (!key)
    {
      return false;
    }

    if (this->value(group, name) == value)
    {
      // Announcing an unchanged value would make every listener redo work
      // for nothing, and a dialog's Apply touches every key.
      return true;
    }

    QSettings own;
    QSettings &settings = m_settings ? *m_settings : own;

    // A colour is written as "#aarrggbb" rather than as a serialised
    // QVariant, so the file stays readable and editable by hand; value()
    // coerces the string back through QColor's own parser.
    settings.setValue(settingsKey(*key),
                      key->fallback.userType() == QMetaType::QColor
                        ? QVariant(value.value<QColor>().name(QColor::HexArgb))
                        : value);
    settings.sync();

    Q_EMIT preferenceChanged(group, name);

    return true;
  }

  void PreferencesManager::resetToDefaults()
  {
    for (const Key &key : keys())
    {
      const bool differed = value(key.group, key.name) != key.fallback;

      QSettings own;
      QSettings &settings = m_settings ? *m_settings : own;
      settings.remove(settingsKey(key));
      settings.sync();

      if (differed)
      {
        Q_EMIT preferenceChanged(key.group, key.name);
      }
    }
  }

  // ── General ────────────────────────────────────────────────────────────

  bool PreferencesManager::showWelcomeOnStartUp() const
  {
    return value(kGeneral, QStringLiteral("showWelcomeOnStartUp")).toBool();
  }

  void PreferencesManager::setShowWelcomeOnStartUp(bool shows)
  {
    setValue(kGeneral, QStringLiteral("showWelcomeOnStartUp"), shows);
  }

  int PreferencesManager::recentLimit() const
  {
    return value(kGeneral, QStringLiteral("recentLimit")).toInt();
  }

  void PreferencesManager::setRecentLimit(int limit)
  {
    setValue(kGeneral, QStringLiteral("recentLimit"), limit);
  }

  // ── Appearance ─────────────────────────────────────────────────────────

  QString PreferencesManager::themeMode() const
  {
    return value(kAppearance, QStringLiteral("themeMode")).toString();
  }

  void PreferencesManager::setThemeMode(const QString &mode)
  {
    setValue(kAppearance, QStringLiteral("themeMode"), mode);
  }

  // ── Components ─────────────────────────────────────────────────────────

  QStringList PreferencesManager::componentSearchPaths() const
  {
    return value(kComponents, QStringLiteral("searchPaths")).toStringList();
  }

  void PreferencesManager::setComponentSearchPaths(const QStringList &paths)
  {
    setValue(kComponents, QStringLiteral("searchPaths"), paths);
  }

  bool PreferencesManager::rescanComponentsOnStartUp() const
  {
    return value(kComponents, QStringLiteral("rescanOnStartUp")).toBool();
  }

  void PreferencesManager::setRescanComponentsOnStartUp(bool rescans)
  {
    setValue(kComponents, QStringLiteral("rescanOnStartUp"), rescans);
  }

  // ── Selection & picking ────────────────────────────────────────────────

  double PreferencesManager::pickTolerancePixels() const
  {
    return value(kSelection, QStringLiteral("pickTolerancePixels")).toDouble();
  }

  void PreferencesManager::setPickTolerancePixels(double pixels)
  {
    setValue(kSelection, QStringLiteral("pickTolerancePixels"), pixels);
  }

  int PreferencesManager::dragThresholdPixels() const
  {
    return value(kSelection, QStringLiteral("dragThresholdPixels")).toInt();
  }

  void PreferencesManager::setDragThresholdPixels(int pixels)
  {
    setValue(kSelection, QStringLiteral("dragThresholdPixels"), pixels);
  }

  double PreferencesManager::snapTolerancePixels() const
  {
    return value(kSelection, QStringLiteral("snapTolerancePixels")).toDouble();
  }

  void PreferencesManager::setSnapTolerancePixels(double pixels)
  {
    setValue(kSelection, QStringLiteral("snapTolerancePixels"), pixels);
  }

  QColor PreferencesManager::selectionColor() const
  {
    return value(kSelection, QStringLiteral("selectionColor")).value<QColor>();
  }

  void PreferencesManager::setSelectionColor(const QColor &color)
  {
    setValue(kSelection, QStringLiteral("selectionColor"), color);
  }

  // ── Map ────────────────────────────────────────────────────────────────

  QString PreferencesManager::defaultMapCrs() const
  {
    return value(kMap, QStringLiteral("defaultCrs")).toString();
  }

  void PreferencesManager::setDefaultMapCrs(const QString &crs)
  {
    setValue(kMap, QStringLiteral("defaultCrs"), crs);
  }

  // ── 3D view ────────────────────────────────────────────────────────────

  QColor PreferencesManager::sceneBackgroundColor() const
  {
    return value(kScene, QStringLiteral("backgroundColor")).value<QColor>();
  }

  void PreferencesManager::setSceneBackgroundColor(const QColor &color)
  {
    setValue(kScene, QStringLiteral("backgroundColor"), color);
  }

  QString PreferencesManager::defaultSceneProjection() const
  {
    return value(kScene, QStringLiteral("defaultProjection")).toString();
  }

  void PreferencesManager::setDefaultSceneProjection(const QString &projection)
  {
    setValue(kScene, QStringLiteral("defaultProjection"), projection);
  }

  double PreferencesManager::defaultVerticalExaggeration() const
  {
    return value(kScene, QStringLiteral("defaultVerticalExaggeration"))
      .toDouble();
  }

  void PreferencesManager::setDefaultVerticalExaggeration(double factor)
  {
    setValue(kScene, QStringLiteral("defaultVerticalExaggeration"), factor);
  }

  bool PreferencesManager::showAxisGizmo() const
  {
    return value(kScene, QStringLiteral("showAxisGizmo")).toBool();
  }

  void PreferencesManager::setShowAxisGizmo(bool show)
  {
    setValue(kScene, QStringLiteral("showAxisGizmo"), show);
  }

  int PreferencesManager::axisGizmoSizePixels() const
  {
    // Floored rather than trusted. The value reaches the renderer as a
    // viewport side, and a viewport of zero or of a negative width is not
    // an invisible gizmo — on some backends it is a validation error that
    // takes the whole frame down with it.
    return std::max(24, value(kScene, QStringLiteral("axisGizmoSizePixels"))
                          .toInt());
  }

  void PreferencesManager::setAxisGizmoSizePixels(int pixels)
  {
    setValue(kScene, QStringLiteral("axisGizmoSizePixels"), pixels);
  }

  QString PreferencesManager::axisGizmoCorner() const
  {
    return value(kScene, QStringLiteral("axisGizmoCorner")).toString();
  }

  void PreferencesManager::setAxisGizmoCorner(const QString &corner)
  {
    setValue(kScene, QStringLiteral("axisGizmoCorner"), corner);
  }

  QString PreferencesManager::linkViews() const
  {
    return value(kScene, QStringLiteral("linkViews")).toString();
  }

  void PreferencesManager::setLinkViews(const QString &link)
  {
    setValue(kScene, QStringLiteral("linkViews"), link);
  }

  double PreferencesManager::orbitDegreesPerPixel() const
  {
    // Not clamped here. The one place that knows what a usable turn rate
    // is, is the one that turns the camera — orbitStep() in
    // scene/navigation.h — and a second opinion held here would be a
    // second set of bounds to keep in step with it.
    return value(kScene, QStringLiteral("orbitDegreesPerPixel")).toDouble();
  }

  void PreferencesManager::setOrbitDegreesPerPixel(double degrees)
  {
    setValue(kScene, QStringLiteral("orbitDegreesPerPixel"), degrees);
  }

  bool PreferencesManager::invertWheel() const
  {
    return value(kScene, QStringLiteral("invertWheel")).toBool();
  }

  void PreferencesManager::setInvertWheel(bool invert)
  {
    setValue(kScene, QStringLiteral("invertWheel"), invert);
  }

  QString PreferencesManager::panModifier() const
  {
    return value(kScene, QStringLiteral("panModifier")).toString();
  }

  void PreferencesManager::setPanModifier(const QString &modifier)
  {
    setValue(kScene, QStringLiteral("panModifier"), modifier);
  }

} // namespace HydroCouple::Composer
