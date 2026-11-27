/*!
 * \file   componentpalettemodel.h
 * \author Caleb Buahin
 * \brief  ComponentPaletteModel — the loaded components, as a draggable list.
 *
 * Presents a ComponentRegistry's entries for the palette panel and supplies the
 * MIME payload the canvas accepts on drop.
 */

#ifndef HYDROCOUPLECOMPOSER_CANVAS_COMPONENTPALETTEMODEL_H
#define HYDROCOUPLECOMPOSER_CANVAS_COMPONENTPALETTEMODEL_H

#include "plugins/componentregistry.h"

#include <QAbstractListModel>

namespace HydroCouple::Composer
{

  /*!
   * \brief Lists a registry's components for drag-and-drop onto the canvas.
   */
  class ComponentPaletteModel : public QAbstractListModel
  {
      Q_OBJECT

    public:
      enum Roles
      {
        ComponentIdRole = Qt::UserRole + 1,
        VersionRole,
        LibraryRole,
        KindRole,    //!< ComponentRegistry::ComponentKind as int; -1 headers.
        IsHeaderRole //!< true for a section-heading row.
      };

      /*!
       * \brief Builds a model over \a registry.
       * \param registry Source of components; refreshes with it.
       * \param parent Optional Qt parent.
       */
      explicit ComponentPaletteModel(ComponentRegistry *registry,
                                     QObject *parent = nullptr);

      [[nodiscard]] int rowCount(
        const QModelIndex &parent = QModelIndex()) const override;

      [[nodiscard]] QVariant data(const QModelIndex &index,
                                  int role = Qt::DisplayRole) const override;

      [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &index) const override;

      [[nodiscard]] QStringList mimeTypes() const override;

      [[nodiscard]] QMimeData *mimeData(
        const QModelIndexList &indexes) const override;

    private:
      /*!
       * \brief One palette row: a section heading (info == nullptr) or a
       *        loaded library's info.
       */
      struct Row
      {
          QString header;
          HydroCouple::IComponentInfo *info = nullptr;
      };

      void reload();

      ComponentRegistry *m_registry = nullptr;
      std::vector<Row> m_rows;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_CANVAS_COMPONENTPALETTEMODEL_H
