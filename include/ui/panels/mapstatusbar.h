/*!
 * \file   mapstatusbar.h
 * \author Caleb Buahin
 * \brief  MapStatusBar — where the map is, how big, and in what system.
 *
 * The three readings a GIS keeps permanently in view: the coordinate under
 * the pointer, the scale as a 1:N ratio, and the coordinate reference system
 * everything is drawn in. openswmm.gui's status bar carries the same three,
 * and this is deliberately the same shape.
 *
 * A widget rather than code inside the main window because all three are
 * views of one canvas and a test should be able to drive them without a
 * window around them.
 *
 * The scale is editable, not a readout. That is the reason a GIS puts scale
 * in the status bar rather than in a corner: "show me this at 1:2000" is a
 * command, and a label cannot take it.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_PANELS_MAPSTATUSBAR_H
#define HYDROCOUPLECOMPOSER_UI_PANELS_MAPSTATUSBAR_H

#include <QWidget>

class QComboBox;
class QLabel;
class QToolButton;

namespace HydroCouple::Composer
{
  class MapCanvas;

  /*!
   * \brief The map's coordinate, scale and CRS readouts.
   */
  class MapStatusBar : public QWidget
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs the bar.
       * \param parent Parent widget.
       */
      explicit MapStatusBar(QWidget *parent = nullptr);

      ~MapStatusBar() override;

      /*!
       * \brief Watches \a canvas, or nothing when null.
       *
       * Everything shown comes from the canvas; passing nullptr leaves the
       * readouts in place but stops them meaning anything, which is what
       * happens when a tab holding no map is in front.
       *
       * \param canvas The canvas to follow.
       */
      void setCanvas(MapCanvas *canvas);

      //! \returns The canvas being watched, or nullptr.
      [[nodiscard]] MapCanvas *canvas() const;

      /*!
       * \brief Turns the readouts on or off.
       *
       * A perspective camera has no single scale, so on the 3D tab the
       * controls go dead rather than show a number that quietly means
       * nothing.
       *
       * \param live True when the readouts apply to what is on screen.
       */
      void setLive(bool live);

      //! \returns The text of the scale field, e.g. "1:2,500".
      [[nodiscard]] QString scaleText() const;

    Q_SIGNALS:
      /*!
       * \brief Emitted when the CRS button is pressed.
       *
       * The bar shows the system; choosing one is the window's business,
       * since it owns the chooser and knows what else to tell.
       */
      void crsRequested();

    private:
      void buildForm();

      //! Applies a typed or chosen "1:N", ignoring what cannot be read.
      void applyScaleText(const QString &text);

      void refreshScale();
      void refreshCrs();

      MapCanvas *m_canvas = nullptr;

      QLabel *m_coordinateLabel = nullptr;
      QComboBox *m_scaleCombo = nullptr;
      QToolButton *m_crsButton = nullptr;

      //! Guards the two directions of the scale field against echoing.
      bool m_syncing = false;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_PANELS_MAPSTATUSBAR_H
