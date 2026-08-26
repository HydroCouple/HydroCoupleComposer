/*!
 * \file   layerpropertiesdialog.h
 * \author Caleb Buahin
 * \brief  LayerPropertiesDialog — everything about one layer, in one place.
 *
 * Edits the layer's own objects rather than copies, so the legend in the layer
 * tree, the map, the 3D scene and this dialog are views of one state and
 * cannot show four different answers. Apply writes the form back and tells the
 * layer to redraw; nothing is cached here.
 *
 * Tabs are conditional, the dialog is not. Its predecessor withheld itself
 * whole from any layer without a style, which left a basemap with no
 * properties at all — and a basemap still has a name, a source, an opacity and
 * a drape. A layer that cannot answer for a tab simply does not get that tab.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_DIALOGS_LAYERPROPERTIESDIALOG_H
#define HYDROCOUPLECOMPOSER_UI_DIALOGS_LAYERPROPERTIESDIALOG_H

#include <QDialog>

#include <memory>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;

namespace HydroCouple::Composer
{
  class MapLayer;
  class SpatialReference;

  /*!
   * \brief Edits one layer's identity, appearance and rendering.
   */
  class LayerPropertiesDialog : public QDialog
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs the dialog for \a layer.
       * \param layer The layer to edit; must outlive the dialog.
       * \param parent Parent widget.
       */
      explicit LayerPropertiesDialog(MapLayer *layer,
                                     QWidget *parent = nullptr);

      ~LayerPropertiesDialog() override;

      /*!
       * \brief Whether \a layer has properties to edit.
       *
       * Every layer does; this exists so callers have one place to ask, and
       * so a null layer is refused somewhere other than in the constructor.
       *
       * \param layer The layer to test.
       */
      [[nodiscard]] static bool canEdit(const MapLayer *layer);

      /*!
       * \brief Writes the form back to the layer and redraws it.
       * \returns True unless a chosen style can colour nothing, in which case
       *          the dialog says why and stays open.
       */
      bool apply();

    private:
      void buildTabs();

      //! \returns The Information page — read-only facts about the layer.
      QWidget *buildInformationTab();

      //! \returns The Source page — name and provenance.
      QWidget *buildSourceTab();

      //! \returns The Symbology page, or nullptr when the layer has no style.
      QWidget *buildSymbologyTab();

      //! \returns The Labels page, or nullptr when the layer has no style.
      QWidget *buildLabelsTab();

      //! \returns The Rendering page — visibility and opacity.
      QWidget *buildRenderingTab();

      //! \returns The Metadata page, or nullptr when there is nothing to show.
      QWidget *buildMetadataTab();

      void loadFromLayer();
      void updateEnabledState();

      /*!
       * \brief Asks for a system to assign to this layer.
       *
       * Assigning changes what the layer's coordinates are taken to *mean*;
       * it does not move them. On a layer that already declares a system that
       * is almost always a correction to a wrong declaration, and almost
       * never what someone wanting to reproject is after — so it is confirmed
       * before it is recorded.
       */
      void assignCrs();

      //! Shows the assigned system, or the layer's own when none is pending.
      void refreshCrsRow();

      //! \returns True when the layer carries a style to edit.
      [[nodiscard]] bool hasStyle() const;

      MapLayer *m_layer = nullptr;

      QTabWidget *m_tabs = nullptr;

      QLineEdit *m_nameEdit = nullptr;
      QLabel *m_crsLabel = nullptr;

      /*!
       * \brief The system chosen but not yet applied.
       *
       * Held rather than assigned on the spot so that Cancel means what it
       * says — a CRS written straight through would survive a dialog the user
       * then dismissed.
       */
      std::shared_ptr<SpatialReference> m_pendingCrs;

      QCheckBox *m_visibleCheck = nullptr;
      QSpinBox *m_opacitySpin = nullptr;

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

#endif // HYDROCOUPLECOMPOSER_UI_DIALOGS_LAYERPROPERTIESDIALOG_H
