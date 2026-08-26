/*!
 * \file   attributetablemodel.h
 * \author Caleb Buahin
 * \brief  AttributeTableModel — a layer's attributes as rows and columns.
 *
 * A view over IAttributeProvider, holding nothing of its own. Values are read
 * through on every request rather than copied in, so a layer whose values
 * change during a run — which is every results layer — shows the current ones
 * without anybody remembering to invalidate a cache. What that costs is a
 * virtual call per cell, which for the few hundred cells a table can show at
 * once is nothing.
 *
 * Rows are feature indices, and deliberately so: that is the same number the
 * selection speaks in and the same number picking returns, which is what lets
 * a row and a feature refer to each other without a lookup table between them.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_PANELS_ATTRIBUTETABLEMODEL_H
#define HYDROCOUPLECOMPOSER_UI_PANELS_ATTRIBUTETABLEMODEL_H

#include "render/attributeprovider.h"

#include <QAbstractTableModel>
#include <QVector>

namespace HydroCouple::Composer
{
  class MapLayer;

  /*!
   * \brief One layer's attributes, as a table.
   */
  class AttributeTableModel : public QAbstractTableModel
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs an empty model.
       * \param parent Owning object.
       */
      explicit AttributeTableModel(QObject *parent = nullptr);

      ~AttributeTableModel() override;

      /*!
       * \brief Shows \a layer's attributes.
       *
       * A layer that carries none — a basemap, a raster — shows an empty
       * table rather than being refused: "this layer has no attributes" is an
       * answer, and an empty table says it.
       *
       * \param layer The layer to show; may be nullptr.
       */
      void setLayer(MapLayer *layer);

      /*!
       * \brief The layer being shown, or nullptr.
       */
      [[nodiscard]] MapLayer *layer() const;

      /*!
       * \brief Re-reads the layer's shape and values.
       *
       * For when a layer gains or loses features, which it does while a run
       * is loading. Values alone need no call: they are read through.
       */
      void refresh();

      [[nodiscard]] int rowCount(
        const QModelIndex &parent = QModelIndex()) const override;

      [[nodiscard]] int columnCount(
        const QModelIndex &parent = QModelIndex()) const override;

      [[nodiscard]] QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override;

      [[nodiscard]] QVariant headerData(
        int section, Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

    private:
      MapLayer *m_layer = nullptr;

      //! The provider half of the same object, or nullptr. Resolved once
      //! rather than cast on every cell.
      const IAttributeProvider *m_provider = nullptr;

      QVector<AttributeField> m_fields;

      /*!
       * \brief Watches the shown layer for its own destruction.
       *
       * The model holds the layer by pointer and reads through it on every
       * cell, so it has to let go the moment the layer does. Nothing else
       * guarantees the order: a window tears its layer stack down before the
       * docks that show it, and a header view re-laid-out during that
       * teardown asks a destroyed layer how many features it has.
       */
      QMetaObject::Connection m_watch;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_PANELS_ATTRIBUTETABLEMODEL_H
