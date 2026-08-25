/*!
 * \file   layerstyledialog.h
 * \author Caleb Buahin
 * \brief  LayerStyleDialog — the editor for a layer's style and labels.
 *
 * Edits the layer's own LayerStyle rather than a copy, so the legend in the
 * layer tree, the map and this dialog are three views of one object and
 * cannot show three different answers. Apply rebuilds the classes from the
 * layer's current data and tells the layer to redraw; nothing is cached here.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_DIALOGS_LAYERSTYLEDIALOG_H
#define HYDROCOUPLECOMPOSER_UI_DIALOGS_LAYERSTYLEDIALOG_H

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QStackedWidget;

namespace HydroCouple::Composer
{
  class MapLayer;

  /*!
   * \brief Edits how one layer is drawn and labelled.
   */
  class LayerStyleDialog : public QDialog
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs the dialog for \a layer.
       * \param layer The layer to style; must outlive the dialog.
       * \param parent Parent widget.
       */
      explicit LayerStyleDialog(MapLayer *layer, QWidget *parent = nullptr);

      ~LayerStyleDialog() override;

      /*!
       * \brief Whether \a layer can be styled at all.
       *
       * A layer with no style — a basemap, an image overlay — has nothing
       * this dialog can edit, and offering it an empty form would be worse
       * than not offering it.
       *
       * \param layer The layer to test.
       */
      [[nodiscard]] static bool canStyle(const MapLayer *layer);

      /*!
       * \brief Writes the form back to the layer and redraws it.
       * \returns True when the style can colour features afterwards.
       */
      bool apply();

    private:
      void buildForm();
      void loadFromLayer();
      void updateEnabledState();

      MapLayer *m_layer = nullptr;

      QComboBox *m_modeCombo = nullptr;
      QComboBox *m_attributeCombo = nullptr;
      QComboBox *m_methodCombo = nullptr;
      QSpinBox *m_classCountSpin = nullptr;
      QComboBox *m_rampCombo = nullptr;
      QPushButton *m_fillButton = nullptr;
      QDoubleSpinBox *m_sizeSpin = nullptr;
      QCheckBox *m_labelsCheck = nullptr;
      QComboBox *m_labelFieldCombo = nullptr;
      QLabel *m_statusLabel = nullptr;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_DIALOGS_LAYERSTYLEDIALOG_H
