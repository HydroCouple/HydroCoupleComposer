/*!
 * \file   maplayer.h
 * \author Caleb Buahin
 * \brief  MapLayer — one drawable entry in the map's layer stack.
 *
 * A layer knows its own extent and how to draw itself; it knows nothing about
 * the widget it is drawn into. Everything a layer needs about the view arrives
 * as a MapTransform argument, so the same layer can be drawn into a canvas, an
 * export image or a print device without change.
 *
 * The concrete layers (vector, raster, mesh, data item) arrive with phases C1d
 * and C2. This base carries only what the layer stack and the canvas need from
 * every layer, deliberately: openswmm.gui's equivalent grew labelling, masks,
 * joins and diagram configuration onto its base class, and each of those made
 * every layer type pay for a feature most of them do not have.
 */

#ifndef HYDROCOUPLECOMPOSER_MAP_MAPLAYER_H
#define HYDROCOUPLECOMPOSER_MAP_MAPLAYER_H

#include <QObject>
#include <QRectF>
#include <QString>

#include <memory>

class QPainter;

namespace HydroCouple::Composer
{
  class LayerStyle;
  class MapTransform;
  class SpatialReference;

  /*!
   * \brief Abstract base for everything the map can draw.
   */
  class MapLayer : public QObject
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs a layer.
       * \param name User-visible layer name.
       * \param parent Owning object.
       */
      explicit MapLayer(const QString &name, QObject *parent = nullptr);

      ~MapLayer() override;

      /*!
       * \brief Stable identifier, unique within the session.
       *
       * Distinct from the name, which the user may change or duplicate.
       */
      [[nodiscard]] QString id() const;

      /*!
       * \brief The user-visible name.
       */
      [[nodiscard]] QString name() const;

      /*!
       * \brief Renames the layer.
       * \param name The new name; ignored when empty.
       */
      void setName(const QString &name);

      /*!
       * \brief Whether the layer is drawn.
       */
      [[nodiscard]] bool isVisible() const;

      /*!
       * \brief Shows or hides the layer.
       * \param visible True to draw the layer.
       */
      void setVisible(bool visible);

      /*!
       * \brief Draw opacity, from 0 (invisible) to 1 (opaque).
       */
      [[nodiscard]] double opacity() const;

      /*!
       * \brief Sets the draw opacity.
       * \param opacity Clamped to [0, 1].
       */
      void setOpacity(double opacity);

      /*!
       * \brief The layer's coordinate reference system, or nullptr.
       */
      [[nodiscard]] const SpatialReference *crs() const;

      /*!
       * \brief The layer's coordinate reference system as a shared handle.
       */
      [[nodiscard]] std::shared_ptr<SpatialReference> crsHandle() const;

      /*!
       * \brief Sets the coordinate reference system.
       *
       * Shared rather than owned because layers read from the same source
       * routinely share one CRS, and building a CRS is a PROJ database lookup.
       *
       * \param crs The system the layer's coordinates are expressed in.
       */
      void setCrs(std::shared_ptr<SpatialReference> crs);

      /*!
       * \brief The layer's style, or nullptr when it has none.
       *
       * A virtual accessor rather than a member on the base: a style is what
       * a layer with features needs, and a basemap or an image overlay has
       * none. The legend asks every layer this one question and builds
       * nothing for the layers that answer nullptr.
       */
      [[nodiscard]] virtual const LayerStyle *style() const;

      /*!
       * \brief The layer's style for editing, or nullptr when it has none.
       *
       * The non-const half of the pair, so the legend and the style editor
       * can change a style without casting the const away at every call site.
       */
      [[nodiscard]] virtual LayerStyle *style();

      /*!
       * \brief Whether this layer is a backdrop rather than data.
       *
       * A basemap covers the whole world, so framing "everything" would
       * always frame the planet and never the data drawn on top of it. The
       * canvas therefore falls back to a basemap's extent only when nothing
       * else has one.
       */
      [[nodiscard]] virtual bool isBasemap() const;

      /*!
       * \brief Credit the map must display for this layer, or empty.
       *
       * Every free tile provider makes attribution a condition of use, so it
       * is data the layer carries rather than a nicety a caller might
       * remember to add. The canvas draws it over everything.
       */
      [[nodiscard]] virtual QString attribution() const;

      /*!
       * \brief The CRS the map is being drawn in, or nullptr.
       */
      [[nodiscard]] const SpatialReference *mapCrs() const;

      /*!
       * \brief Tells the layer what CRS the map is drawn in.
       *
       * Every layer being drawn into a map needs this — its own coordinates
       * mean nothing without knowing what they are being drawn alongside —
       * so it lives on the base rather than on each layer that reprojects.
       *
       * \param crs The map's coordinate reference system.
       */
      void setMapCrs(std::shared_ptr<SpatialReference> crs);

      /*!
       * \brief The layer's bounding rectangle, in the layer's own CRS.
       *
       * An empty rectangle means the layer has no geometry to zoom to.
       */
      [[nodiscard]] virtual QRectF extent() const = 0;

      /*!
       * \brief Draws the layer.
       *
       * The painter is already clipped to the viewport and its opacity set;
       * an implementation draws in world coordinates converted through
       * \a transform and must not alter painter state it does not restore.
       *
       * \param painter Painter to draw with.
       * \param transform World-to-pixel mapping for the current view.
       */
      virtual void render(QPainter &painter,
                          const MapTransform &transform) = 0;

    Q_SIGNALS:
      /*!
       * \brief Emitted when the name changes.
       * \param name The new name.
       */
      void nameChanged(const QString &name);

      /*!
       * \brief Emitted when the layer must be redrawn.
       *
       * One signal rather than one per property: every listener does the same
       * thing with it, which is to repaint.
       */
      void appearanceChanged();

      /*!
       * \brief Emitted when the layer's extent changes.
       */
      void extentChanged();

    public:
      /*!
       * \brief Announces that the layer must be redrawn.
       *
       * Public, and deliberately outside the Q_SIGNALS block above: a layer's
       * style is edited through style(), by the legend and the style editor,
       * so whoever changed it has to be able to say so — the layer itself
       * never sees the edit.
       */
      void notifyAppearanceChanged();

    protected:
      /*!
       * \brief Announces that the extent changed, for subclasses.
       */
      void notifyExtentChanged();

      /*!
       * \brief Called after the map's CRS changes, for subclasses.
       *
       * A layer that caches reprojected geometry rebuilds it here.
       */
      virtual void onMapCrsChanged();

    private:
      QString m_id;
      QString m_name;
      bool m_visible = true;
      double m_opacity = 1.0;
      std::shared_ptr<SpatialReference> m_crs;
      std::shared_ptr<SpatialReference> m_mapCrs;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_MAP_MAPLAYER_H
