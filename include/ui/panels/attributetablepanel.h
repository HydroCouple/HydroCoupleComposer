/*!
 * \file   attributetablepanel.h
 * \author Caleb Buahin
 * \brief  AttributeTablePanel — a layer's features, and the selection.
 *
 * The third view of the one selection. It is not kept in step with the map
 * and the scene so much as it reads the same thing they do: the selection
 * lives on the layer, so this panel has no copy of it to get wrong. What it
 * does have is the translation between a *row* and a *feature*, and those are
 * the same number by construction — which is why there is no lookup here.
 *
 * The layer being shown follows the selection rather than only the chooser: a
 * click that lands on another layer switches to it, because a table that goes
 * on describing a layer nobody just clicked is a table describing the wrong
 * thing.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_PANELS_ATTRIBUTETABLEPANEL_H
#define HYDROCOUPLECOMPOSER_UI_PANELS_ATTRIBUTETABLEPANEL_H

#include <QWidget>

class QComboBox;
class QLabel;
class QTableView;

namespace HydroCouple::Composer
{
  class AttributeTableModel;
  class LayerStackModel;
  class MapLayer;

  /*!
   * \brief Panel showing one layer's attributes.
   */
  class AttributeTablePanel : public QWidget
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs the panel.
       * \param parent Parent widget.
       */
      explicit AttributeTablePanel(QWidget *parent = nullptr);

      ~AttributeTablePanel() override;

      /*!
       * \brief Sets the layer stack to read.
       * \param model The stack; the panel does not take ownership.
       */
      void setModel(LayerStackModel *model);

      /*!
       * \brief The layer stack being read, or nullptr.
       */
      [[nodiscard]] LayerStackModel *model() const;

      /*!
       * \brief The layer whose attributes are shown, or nullptr.
       */
      [[nodiscard]] MapLayer *currentLayer() const;

      /*!
       * \brief Shows \a layer's attributes.
       * \param layer A layer in the stack; anything else is ignored.
       */
      void showLayer(MapLayer *layer);

      /*!
       * \brief The table view, for tests and for the host's own wiring.
       */
      [[nodiscard]] QTableView *view() const;

    private:
      //! Rebuilds the layer chooser from the stack.
      void rebuildChooser();

      //! Copies the layer's selection onto the table's rows.
      void readSelection();

      //! Copies the table's rows onto the layer's selection.
      void writeSelection();

      LayerStackModel *m_model = nullptr;
      AttributeTableModel *m_table = nullptr;

      QComboBox *m_chooser = nullptr;
      QTableView *m_view = nullptr;
      QLabel *m_summary = nullptr;

      /*!
       * \brief True while one side of the selection is copying to the other.
       *
       * The two directions are the same wire read from both ends, so without
       * this the first change echoes back and forth. A flag rather than
       * blocking signals, because what has to be suppressed is the *round
       * trip*, not the notification — the summary below the table still needs
       * to update on both.
       */
      bool m_syncing = false;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_PANELS_ATTRIBUTETABLEPANEL_H
