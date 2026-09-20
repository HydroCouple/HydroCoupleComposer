/*!
 * \file   preferencesmanager.h
 * \author Caleb Buahin
 * \brief  PreferencesManager — the application's global preferences.
 *
 * One place where a setting lives, read at the point of use. Before this
 * every tolerance was a constexpr copied into whichever file needed it —
 * the click slop existed three times — and the three things that were
 * persisted (appearance, recent list, welcome flag) each wrote QSettings by
 * hand. A preference nobody re-reads is decoration, so consumers read
 * through the accessors on every use rather than caching at construction,
 * and anything that holds derived state listens to preferenceChanged().
 *
 * Table-driven: every key is declared once with its group, name and
 * default, and the typed accessors, the dialog, resetToDefaults() and the
 * tests all walk the same table. Adding a preference is adding a row.
 */

#ifndef HYDROCOUPLECOMPOSER_CORE_PREFERENCESMANAGER_H
#define HYDROCOUPLECOMPOSER_CORE_PREFERENCESMANAGER_H

#include <QColor>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

class QSettings;

namespace HydroCouple::Composer
{
  /*!
   * \brief The global preferences, over QSettings.
   */
  class PreferencesManager : public QObject
  {
      Q_OBJECT

    public:
      /*!
       * \brief One row of the preference table.
       */
      struct Key
      {
          QString group;       //!< "General", "Selection", ...
          QString name;        //!< "pickTolerancePixels", ...
          QVariant fallback;   //!< What value() answers when nothing is stored.
      };

      /*!
       * \brief Builds a manager over \a settings.
       * \param settings The store, or nullptr for the application default.
       *   Not owned; must outlive this.
       * \param parent Optional Qt parent.
       */
      explicit PreferencesManager(QSettings *settings = nullptr,
                                  QObject *parent = nullptr);

      /*!
       * \brief The application-wide manager over the default QSettings.
       */
      static PreferencesManager *instance();

      /*!
       * \brief Every declared preference, in table order.
       */
      [[nodiscard]] static const QVector<Key> &keys();

      /*!
       * \brief The stored value of \a group / \a name, or its default.
       *
       * An undeclared key answers an invalid QVariant.
       */
      [[nodiscard]] QVariant value(const QString &group,
                                   const QString &name) const;

      /*!
       * \brief Stores \a value and announces it.
       *
       * Emits preferenceChanged() only when the stored value differs; an
       * undeclared key is refused and nothing is written.
       * \returns Whether the value was stored.
       */
      bool setValue(const QString &group, const QString &name,
                    const QVariant &value);

      /*!
       * \brief Forgets every stored value, so defaults apply again.
       *
       * Announces each key whose value that changed.
       */
      void resetToDefaults();

      // ── General ──────────────────────────────────────────────────────────
      [[nodiscard]] bool showWelcomeOnStartUp() const;
      void setShowWelcomeOnStartUp(bool shows);
      [[nodiscard]] int recentLimit() const;
      void setRecentLimit(int limit);

      // ── Appearance ───────────────────────────────────────────────────────
      //! "System", "Light" or "Dark" — ThemeManager's spelling.
      [[nodiscard]] QString themeMode() const;
      void setThemeMode(const QString &mode);

      // ── Components ───────────────────────────────────────────────────────
      [[nodiscard]] QStringList componentSearchPaths() const;
      void setComponentSearchPaths(const QStringList &paths);
      [[nodiscard]] bool rescanComponentsOnStartUp() const;
      void setRescanComponentsOnStartUp(bool rescans);

      // ── Selection & picking ──────────────────────────────────────────────
      //! How near a click has to land to a feature, in pixels.
      [[nodiscard]] double pickTolerancePixels() const;
      void setPickTolerancePixels(double pixels);
      //! How far the mouse may move and still count as a click.
      [[nodiscard]] int dragThresholdPixels() const;
      void setDragThresholdPixels(int pixels);
      //! How close to a vertex or edge, in pixels, counts as on it.
      [[nodiscard]] double snapTolerancePixels() const;
      void setSnapTolerancePixels(double pixels);
      [[nodiscard]] QColor selectionColor() const;
      void setSelectionColor(const QColor &color);

      // ── Map ──────────────────────────────────────────────────────────────
      //! "AUTHORITY:CODE", the CRS a new map opens in.
      [[nodiscard]] QString defaultMapCrs() const;
      void setDefaultMapCrs(const QString &crs);

      // ── 3D view ──────────────────────────────────────────────────────────
      [[nodiscard]] QColor sceneBackgroundColor() const;
      void setSceneBackgroundColor(const QColor &color);
      //! "Perspective" or "Orthographic".
      [[nodiscard]] QString defaultSceneProjection() const;
      void setDefaultSceneProjection(const QString &projection);
      [[nodiscard]] double defaultVerticalExaggeration() const;
      void setDefaultVerticalExaggeration(double factor);

      //! Whether the orientation cue is drawn over the 3D view.
      [[nodiscard]] bool showAxisGizmo() const;
      void setShowAxisGizmo(bool show);
      //! The side of the gizmo's square viewport, in logical pixels.
      [[nodiscard]] int axisGizmoSizePixels() const;
      void setAxisGizmoSizePixels(int pixels);
      /*!
       * \brief Which corner of the 3D view the gizmo occupies.
       *
       * Stored and returned by name — "BottomLeft" and the rest — rather
       * than as scene/axisgizmo.h's enum, so that preferences, which
       * every layer reads, does not drag the scene layer in behind it.
       * gizmoCornerFromName() does the translation, next to the enum it
       * translates into.
       */
      [[nodiscard]] QString axisGizmoCorner() const;
      void setAxisGizmoCorner(const QString &corner);

      /*!
       * \brief Whether switching tabs hands the framing over.
       *
       * Stored by name — "OnTabSwitch" or "Never" — for the same reason
       * the gizmo's corner is: scene/navigation.h owns the enum and the
       * translation, and preferences is read by every layer.
       */
      [[nodiscard]] QString linkViews() const;
      void setLinkViews(const QString &link);
      //! Degrees the 3D camera turns per pixel of orbit drag.
      [[nodiscard]] double orbitDegreesPerPixel() const;
      void setOrbitDegreesPerPixel(double degrees);
      //! Whether the 3D wheel zooms the opposite way.
      [[nodiscard]] bool invertWheel() const;
      void setInvertWheel(bool invert);
      //! "MiddleDrag" or "ShiftDrag"; see scene/navigation.h.
      [[nodiscard]] QString panModifier() const;
      void setPanModifier(const QString &modifier);

    Q_SIGNALS:
      /*!
       * \brief A stored value changed.
       * \param group The key's group, as in keys().
       * \param name The key's name.
       */
      void preferenceChanged(const QString &group, const QString &name);

    private:
      [[nodiscard]] static const Key *find(const QString &group,
                                           const QString &name);
      [[nodiscard]] static QString settingsKey(const Key &key);

      QSettings *m_settings = nullptr;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_CORE_PREFERENCESMANAGER_H
