/*!
 * \file   layerstackmodel.h
 * \author Caleb Buahin
 * \brief  LayerStackModel — the ordered set of map layers, as a Qt model.
 *
 * The stack is the single owner of the session's layers, and the only place
 * their order and visibility live. The canvas draws from it and the layer tree
 * edits it; neither keeps a copy, so a layer toggled in the tree is redrawn
 * without either widget knowing about the other.
 *
 * Row 0 is the **top** of the stack, matching what the layer tree shows and
 * every other GIS application's convention. Drawing therefore walks the rows
 * backwards — see renderOrder().
 *
 * Two levels: layers, and beneath each one the legend rows its style produces.
 * The legend is derived from the style on every read rather than stored, which
 * is what stops a map and its legend from drifting apart; the classes are
 * checkable there, so a class switched off in the legend disappears from the
 * map without a separate visibility list to keep in step.
 */

#ifndef HYDROCOUPLECOMPOSER_MAP_LAYERSTACKMODEL_H
#define HYDROCOUPLECOMPOSER_MAP_LAYERSTACKMODEL_H

#include <QAbstractItemModel>
#include <QSet>
#include <QHash>
#include <QVector>

namespace HydroCouple::Composer
{
  class MapLayer;

  /*!
   * \brief An ordered, owning list of map layers.
   */
  class LayerStackModel : public QAbstractItemModel
  {
      Q_OBJECT

    public:
      /*!
       * \brief Model roles beyond the standard Qt ones.
       */
      enum Roles
      {
        LayerIdRole = Qt::UserRole + 1,  //!< Stable layer id, as a QString.
        OpacityRole,                     //!< Draw opacity, as a double.
        CrsDescriptionRole,              //!< CRS name, or an empty string.
        IsLegendRole,                    //!< True for a legend row.
        LegendIndexRole,                 //!< Class index, or -1.

        /*!
         * Whether the layer has any 3D form at all. Distinct from the
         * toggle: a point layer answers false here and no checkbox can
         * change that, which the tree must show as "impossible" rather
         * than "switched off".
         */
        HasSceneFormRole,

        //! Whether the layer joins the scene when it can. Writable.
        ShownIn3DRole
      };

      /*!
       * \brief Constructs an empty stack.
       * \param parent Owning object.
       */
      explicit LayerStackModel(QObject *parent = nullptr);

      ~LayerStackModel() override;

      // ── QAbstractItemModel ───────────────────────────────────────────────

      [[nodiscard]] QModelIndex index(
        int row, int column,
        const QModelIndex &parent = QModelIndex()) const override;

      [[nodiscard]] QModelIndex parent(const QModelIndex &child) const override;

      [[nodiscard]] int rowCount(
        const QModelIndex &parent = QModelIndex()) const override;

      [[nodiscard]] int columnCount(
        const QModelIndex &parent = QModelIndex()) const override;

      [[nodiscard]] QVariant data(const QModelIndex &index,
                                  int role) const override;

      bool setData(const QModelIndex &index, const QVariant &value,
                   int role) override;

      [[nodiscard]] Qt::ItemFlags flags(
        const QModelIndex &index) const override;

      [[nodiscard]] QVariant headerData(
        int section, Qt::Orientation orientation, int role) const override;

      bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                    const QModelIndex &destinationParent,
                    int destinationChild) override;

      bool removeRows(int row, int count,
                      const QModelIndex &parent = QModelIndex()) override;

      [[nodiscard]] Qt::DropActions supportedDropActions() const override;

      // ── Stack ────────────────────────────────────────────────────────────

      /*!
       * \brief Adds \a layer at the top of the stack, taking ownership.
       * \param layer The layer to add; ignored when null or already present.
       * \returns The row it was inserted at, or -1.
       */
      int addLayer(MapLayer *layer);

      /*!
       * \brief Inserts \a layer at \a row, taking ownership.
       * \param row Insertion row, clamped to the stack.
       * \param layer The layer to insert; ignored when null or present.
       * \returns The row it was inserted at, or -1.
       */
      int insertLayer(int row, MapLayer *layer);

      /*!
       * \brief Removes and destroys the layer at \a row.
       * \param row Row to remove.
       */
      bool removeLayer(int row);

      /*!
       * \brief The layer at \a row, or nullptr.
       * \param row Row to read.
       */
      [[nodiscard]] MapLayer *layerAt(int row) const;

      /*!
       * \brief The row holding \a layer, or -1.
       * \param layer Layer to find.
       */
      [[nodiscard]] int rowOf(const MapLayer *layer) const;

      /*!
       * \brief The layer with \a id, or nullptr.
       * \param id Stable layer id.
       */
      [[nodiscard]] MapLayer *layerById(const QString &id) const;

      /*!
       * \brief Every layer, top of the stack first.
       */
      [[nodiscard]] QVector<MapLayer *> layers() const;

      /*!
       * \brief Every layer in drawing order, bottom of the stack first.
       */
      [[nodiscard]] QVector<MapLayer *> renderOrder() const;

      /*!
       * \brief Removes and destroys every layer.
       */
      void clear();

      /*!
       * \brief The layer a legend row belongs to, or the layer itself.
       * \param index Any index in this model.
       */
      [[nodiscard]] MapLayer *layerFor(const QModelIndex &index) const;

      /*!
       * \brief Selects \a feature in \a layer and nothing anywhere else.
       *
       * The stack's job rather than a view's, because there is one selection
       * across the whole stack and two views doing this independently would
       * be two places for that rule to be spelled differently. A null layer
       * or a negative feature selects nothing, which is what a click on empty
       * space means.
       *
       * Layers without features are unaffected, having nothing to select.
       *
       * \param layer The layer the selection belongs to, or nullptr.
       * \param feature Index within \a layer, or -1.
       */
      void selectOnly(MapLayer *layer, int feature);

      /*!
       * \brief Gives \a layer the whole of \a features, clearing the rest.
       *
       * The rubber band's route in. One layer holds the selection at a time
       * — the same rule a click follows — because a table shows one layer's
       * rows and a selection spread across three of them is one the user
       * can only ever see a third of.
       *
       * \param layer The layer to select in; nullptr clears everything.
       * \param features Feature indices; out-of-range ones are dropped by
       *        the layer.
       */
      void selectOnly(MapLayer *layer, const QSet<int> &features);

      /*!
       * \brief Whether \a index is a legend row rather than a layer.
       * \param index Index to test.
       */
      [[nodiscard]] static bool isLegendIndex(const QModelIndex &index);

    Q_SIGNALS:
      /*!
       * \brief Emitted whenever the drawn result would differ.
       *
       * Covers order, membership, visibility and per-layer content alike: a
       * view that repaints on this signal cannot miss a change.
       */
      void renderChanged();

    private:
      void connectLayer(MapLayer *layer);
      void emitRowChanged(MapLayer *layer);
      [[nodiscard]] int legendCount(const MapLayer *layer) const;

      QVector<MapLayer *> m_layers;

      //! Legend row counts as the views last saw them. A style rebuild can
      //! change how many rows a layer has, and a view told only that the data
      //! changed would keep asking for rows that are no longer there.
      QHash<const MapLayer *, int> m_legendCounts;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_MAP_LAYERSTACKMODEL_H
