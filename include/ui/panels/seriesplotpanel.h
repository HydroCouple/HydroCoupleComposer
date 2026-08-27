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
 *
 * The selection names one layer — the stack allows no more — but the plot
 * draws the same feature from every layer standing on the same ground.
 * Two runs of one model are two layers, and overlaying what each recorded
 * at one place is the comparison a plot is for: the one thing a difference
 * map cannot show, since it gives the gap and not the two curves that
 * produced it. Asking the user to select twice is not an option the stack
 * offers, and it is not the gesture anyone would make.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_PANELS_SERIESPLOTPANEL_H
#define HYDROCOUPLECOMPOSER_UI_PANELS_SERIESPLOTPANEL_H

#include "results/seriesexport.h"

#include <QVector>
#include <QWidget>

class QCheckBox;
class QLabel;
class QStackedLayout;
class QToolButton;

class QChart;
class QChartView;
class QDateTimeAxis;
class QValueAxis;

namespace HydroCouple::Composer
{
  class FeatureLayer;
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
       * \brief Whether other runs on the same ground are drawn too.
       *
       * On by default: the panel is a results panel, and a comparison
       * nobody switches on is a comparison nobody finds.
       */
      [[nodiscard]] bool overlaysOtherRuns() const;

      /*!
       * \brief Draws, or stops drawing, the other runs at the same place.
       * \param overlay True to overlay.
       */
      void setOverlaysOtherRuns(bool overlay);

      /*!
       * \brief How many series are plotted.
       *
       * One per selected feature that had a series to read, across every
       * layer that had a selection.
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
       * \brief The layers being plotted, topmost first.
       *
       * More than one whenever more than one has a selection, which is what
       * an overlay is: the same feature of two runs, read from the two
       * layers that carry them.
       */
      [[nodiscard]] QVector<const FeatureLayer *> plottedLayers() const;

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
       * \brief The selected layer, then every run standing beside it.
       *
       * The selected one first, because its selection is the feature every
       * other layer is read at; the rest in stack order, so the legend does
       * not reshuffle between two reads of one stack.
       *
       * \param[out] reason Why there is nothing to plot, when nothing is.
       */
      [[nodiscard]] QVector<FeatureLayer *> plottableLayers(
        QString &reason) const;

      QChartView *m_view = nullptr;
      QChart *m_chart = nullptr;
      QDateTimeAxis *m_timeAxis = nullptr;
      QValueAxis *m_valueAxis = nullptr;
      QLabel *m_status = nullptr;
      QStackedLayout *m_pages = nullptr;
      QToolButton *m_exportButton = nullptr;
      QCheckBox *m_overlayCheck = nullptr;

      LayerStackModel *m_model = nullptr;

      //! The layers the plotted series came from; not owned.
      QVector<const FeatureLayer *> m_layers;

      //! What is on the chart, in the order it was drawn.
      QVector<ExportSeries> m_plotted;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_PANELS_SERIESPLOTPANEL_H
