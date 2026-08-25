/*!
 * \file   layertreepanel.h
 * \author Caleb Buahin
 * \brief  LayerTreePanel — the layer list, with ordering and visibility.
 *
 * A thin view over LayerStackModel: it edits the model and reports what the
 * canvas alone can act on (framing a layer), holding no layer state itself.
 *
 * The view is a QTreeView over a currently flat model. Groups and per-symbol
 * sub-layers arrive with classification in C1c, and a tree view absorbs them
 * without the panel changing; a list view would have to be replaced.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_PANELS_LAYERTREEPANEL_H
#define HYDROCOUPLECOMPOSER_UI_PANELS_LAYERTREEPANEL_H

#include <QWidget>

class QToolButton;
class QTreeView;

namespace HydroCouple::Composer
{
  class LayerStackModel;
  class MapLayer;

  /*!
   * \brief Panel listing the map's layers.
   */
  class LayerTreePanel : public QWidget
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs the panel.
       * \param parent Parent widget.
       */
      explicit LayerTreePanel(QWidget *parent = nullptr);

      ~LayerTreePanel() override;

      /*!
       * \brief Sets the layer stack to show.
       * \param model The stack; the panel does not take ownership.
       */
      void setModel(LayerStackModel *model);

      /*!
       * \brief The layer stack being shown, or nullptr.
       */
      [[nodiscard]] LayerStackModel *model() const;

      /*!
       * \brief The selected layer, or nullptr.
       */
      [[nodiscard]] MapLayer *currentLayer() const;

      /*!
       * \brief The underlying view, for selection handling and tests.
       */
      [[nodiscard]] QTreeView *view() const;

      /*!
       * \brief Moves the selected layer one place up the stack.
       */
      void moveCurrentUp();

      /*!
       * \brief Moves the selected layer one place down the stack.
       */
      void moveCurrentDown();

      /*!
       * \brief Removes the selected layer from the stack.
       */
      void removeCurrent();

      /*!
       * \brief Asks for the selected layer's style editor.
       */
      void styleCurrent();

    Q_SIGNALS:
      /*!
       * \brief Emitted when the selected layer changes.
       * \param layer The newly selected layer, or nullptr.
       */
      void currentLayerChanged(HydroCouple::Composer::MapLayer *layer);

      /*!
       * \brief Emitted when the user asks to frame a layer.
       * \param layer The layer to frame.
       */
      void zoomToLayerRequested(HydroCouple::Composer::MapLayer *layer);

      /*!
       * \brief Emitted when the user asks to restyle a layer.
       * \param layer The layer to style.
       */
      void styleLayerRequested(HydroCouple::Composer::MapLayer *layer);

    private:
      void updateButtons();

      LayerStackModel *m_model = nullptr;
      QTreeView *m_view = nullptr;
      QToolButton *m_upButton = nullptr;
      QToolButton *m_downButton = nullptr;
      QToolButton *m_removeButton = nullptr;
      QToolButton *m_zoomButton = nullptr;
      QToolButton *m_styleButton = nullptr;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_PANELS_LAYERTREEPANEL_H
