/*!
 * \file   transectpanel.h
 * \author Caleb Buahin
 * \brief  TransectPanel — the water column along a line drawn on the map.
 *
 * The section view. Where ProfilePlotPanel reads one column top to bottom,
 * this reads every column a line crosses: distance along the line across,
 * elevation up, one filled cell per finite volume the line passed through.
 *
 * Drawn rather than plotted. A section is a picture of cells, not of a
 * curve — the shape of the layering, where the bed rises, which cells the
 * line clipped a corner of — and a line chart of the same numbers throws
 * away all three. It is painted directly for the same reason the map is.
 *
 * Colours come from the layer's own classification, so the section, the map
 * and the 3D scene are three views of one theme and cannot disagree.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_PANELS_TRANSECTPANEL_H
#define HYDROCOUPLECOMPOSER_UI_PANELS_TRANSECTPANEL_H

#include "results/transect.h"

#include <QPolygonF>
#include <QRectF>
#include <QWidget>

class QLabel;
class QStackedLayout;

namespace HydroCouple::Composer
{
  class LayerStackModel;
  class MeshLayer;

  /*!
   * \brief Paints a section: cells in distance and elevation.
   */
  class TransectView : public QWidget
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs an empty view.
       * \param parent Parent widget.
       */
      explicit TransectView(QWidget *parent = nullptr);

      /*!
       * \brief Sets what to draw.
       * \param section The cells to paint.
       * \param layer The layer whose classification colours them; not owned,
       *        and must outlive the section it coloured.
       */
      void setSection(const TransectSection &section, const MeshLayer *layer);

      //! \returns The section being painted.
      [[nodiscard]] const TransectSection &section() const;

      /*!
       * \brief Where \a cell lands in widget pixels.
       *
       * Exposed because it is how the painting is done and how a test says
       * which pixel should be which colour — a section that draws its cells
       * in the wrong place is only observable somewhere.
       *
       * \param cell A cell of the current section.
       * \returns The rectangle, or an empty one when nothing is drawn.
       */
      [[nodiscard]] QRectF cellRect(const TransectCell &cell) const;

    protected:
      void paintEvent(QPaintEvent *event) override;

    private:
      //! The area the cells are drawn in, inside the axis margins.
      [[nodiscard]] QRectF plotArea() const;

      TransectSection m_section;
      const MeshLayer *m_layer = nullptr;
  };

  /*!
   * \brief Shows the section a line cuts, or why there is none.
   */
  class TransectPanel : public QWidget
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs the panel with no line drawn.
       * \param parent Parent widget.
       */
      explicit TransectPanel(QWidget *parent = nullptr);

      ~TransectPanel() override;

      /*!
       * \brief Watches \a model's layers, or nothing when null.
       * \param model The stack to cut through; not owned.
       */
      void setModel(LayerStackModel *model);

      //! \returns The stack being followed, or nullptr.
      [[nodiscard]] LayerStackModel *model() const;

      /*!
       * \brief Sets the section line.
       *
       * Held rather than consumed, so the section re-cuts itself as a run
       * steps: the line is where the user is looking, and re-drawing it
       * every step would be asking them to hold still.
       *
       * \param line The line in the map's CRS; empty clears the section.
       */
      void setLine(const QPolygonF &line);

      //! \returns The section line, in the map's CRS.
      [[nodiscard]] const QPolygonF &line() const;

      //! \returns How many cells the line cuts.
      [[nodiscard]] int cellCount() const;

      //! \returns The section on screen.
      [[nodiscard]] const TransectSection &section() const;

      //! \returns The layer being cut, or nullptr.
      [[nodiscard]] const MeshLayer *layer() const;

      //! \returns The view, for the pixels it paints.
      [[nodiscard]] TransectView *view() const;

      /*!
       * \brief The message shown in place of a section, or empty.
       */
      [[nodiscard]] QString statusText() const;

    private:
      //! Re-cuts the line and repaints.
      void refresh();

      //! Shows \a text instead of a section.
      void showMessage(const QString &text);

      /*!
       * \brief The topmost layer a line can be cut through, or nullptr.
       *
       * Topmost rather than selected: a section names its columns by where
       * the line goes, so requiring a selection first would make the user
       * pick the cells they drew a line to find.
       *
       * \param[out] reason Why there is nothing to cut.
       */
      [[nodiscard]] MeshLayer *cuttableLayer(QString &reason) const;

      TransectView *m_view = nullptr;
      QLabel *m_status = nullptr;
      QStackedLayout *m_pages = nullptr;

      LayerStackModel *m_model = nullptr;

      //! The layer the section came from; not owned.
      const MeshLayer *m_layer = nullptr;

      QPolygonF m_line;
      TransectSection m_section;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_PANELS_TRANSECTPANEL_H
