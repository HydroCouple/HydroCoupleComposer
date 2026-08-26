/*!
 * \file   seriesplotpanel.h
 * \author Caleb Buahin
 * \brief  SeriesPlotPanel — what the picked features recorded, over time.
 *
 * The map answers "what did this look like at that instant"; the plot
 * answers "what did this one do". Both read the same item, one hyperslab
 * each, so neither holds a copy of the other's values and the two cannot
 * come to disagree about what the run recorded.
 *
 * A view of the layer stack's selection, like the attribute table: picking
 * on the map, banding in the 3D view and selecting rows in the table all
 * arrive here through the one selection they already share.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_PANELS_SERIESPLOTPANEL_H
#define HYDROCOUPLECOMPOSER_UI_PANELS_SERIESPLOTPANEL_H

#include "results/seriesexport.h"

#include <QWidget>

class QLabel;
class QStackedLayout;
class QToolButton;

class QChart;
class QChartView;
class QDateTimeAxis;
class QValueAxis;

namespace HydroCouple::Composer
{
  class DataItemLayer;
  class LayerStackModel;

  /*!
   * \brief Plots the recorded series of whatever is selected.
   */
  class SeriesPlotPanel : public QWidget
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs the panel.
       * \param parent Parent widget.
       */
      explicit SeriesPlotPanel(QWidget *parent = nullptr);

      ~SeriesPlotPanel() override;

      /*!
       * \brief Watches \a model's selection, or nothing when null.
       * \param model The stack to follow; not owned.
       */
      void setModel(LayerStackModel *model);

      //! \returns The stack being followed, or nullptr.
      [[nodiscard]] LayerStackModel *model() const;

      /*!
       * \brief How many series are plotted.
       *
       * One per selected feature that had a series to read.
       */
      [[nodiscard]] int seriesCount() const;

      /*!
       * \brief The message shown in place of a plot, or empty when plotting.
       *
       * Exposed so a test can check *what* the panel says when it draws
       * nothing — "nothing selected" and "this layer has no time axis" are
       * different answers and only one of them is worth acting on.
       */
      [[nodiscard]] QString statusText() const;

      /*!
       * \brief The layer being plotted, or nullptr.
       */
      [[nodiscard]] const DataItemLayer *layer() const;

      /*!
       * \brief The values of series \a index, in the order plotted.
       * \param index Series index, in [0, seriesCount).
       */
      [[nodiscard]] QVector<double> seriesValues(int index) const;

      /*!
       * \brief What is plotted, ready to be written out.
       *
       * Kept as the chart is built rather than reconstructed from it, so an
       * export holds exactly the series on screen — the same features, the
       * same instants, the same points dropped — instead of a second reading
       * that could differ from the picture it came from. Reading it back off
       * the axis would mean inverting the conversion that put it there,
       * which is a rounding no export should be built on.
       */
      [[nodiscard]] const QVector<ExportSeries> &exportSeries() const;

      /*!
       * \brief Writes what is plotted to \a path.
       *
       * The suffix chooses the format: `.dat` writes SWMM time series, one
       * file per series; anything else writes one CSV.
       *
       * \param path Destination file.
       * \param[out] message Diagnostic on failure.
       * \returns The files written, or empty on failure.
       */
      [[nodiscard]] QStringList exportTo(const QString &path,
                                         QString &message) const;

    private:
      //! Asks where to write, then writes.
      void onExportRequested();

      //! Re-reads the selection and rebuilds the chart.
      void refresh();

      //! Shows \a text instead of a plot.
      void showMessage(const QString &text);

      /*!
       * \brief The selected data-item layer with a time axis, or nullptr.
       * \param[out] reason Why there is nothing to plot.
       */
      [[nodiscard]] DataItemLayer *plottableLayer(QString &reason) const;

      QChartView *m_view = nullptr;
      QChart *m_chart = nullptr;
      QDateTimeAxis *m_timeAxis = nullptr;
      QValueAxis *m_valueAxis = nullptr;
      QLabel *m_status = nullptr;
      QStackedLayout *m_pages = nullptr;
      QToolButton *m_exportButton = nullptr;

      LayerStackModel *m_model = nullptr;

      //! The layer the plotted series came from; not owned.
      const DataItemLayer *m_layer = nullptr;

      //! What is on the chart, in the order it was drawn.
      QVector<ExportSeries> m_plotted;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_PANELS_SERIESPLOTPANEL_H
