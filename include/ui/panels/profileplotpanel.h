/*!
 * \file   profileplotpanel.h
 * \author Caleb Buahin
 * \brief  ProfilePlotPanel — the water column under a picked face.
 *
 * The third view of one selection. The map shows a layer of the column from
 * above, the 3D scene shows the column as prisms, and this shows what the
 * column holds from top to bottom — value across, elevation up, which is how
 * a profile is read and the transpose of how a time series is.
 *
 * Elevation is the y axis and not an index, because layers are not evenly
 * thick: a sigma layering packs them towards the surface, and a profile
 * plotted against layer number hides exactly the gradient it was opened to
 * show.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_PANELS_PROFILEPLOTPANEL_H
#define HYDROCOUPLECOMPOSER_UI_PANELS_PROFILEPLOTPANEL_H

#include "results/seriesexport.h"

#include <QWidget>

class QChart;
class QChartView;
class QLabel;
class QStackedLayout;
class QToolButton;
class QValueAxis;

namespace HydroCouple::Composer
{
  class LayerStackModel;
  class MeshLayer;

  /*!
   * \brief Plots the vertical profile of whatever column is selected.
   */
  class ProfilePlotPanel : public QWidget
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs the panel.
       * \param parent Parent widget.
       */
      explicit ProfilePlotPanel(QWidget *parent = nullptr);

      ~ProfilePlotPanel() override;

      /*!
       * \brief Watches \a model's selection, or nothing when null.
       * \param model The stack to follow; not owned.
       */
      void setModel(LayerStackModel *model);

      //! \returns The stack being followed, or nullptr.
      [[nodiscard]] LayerStackModel *model() const;

      //! \returns How many columns are profiled.
      [[nodiscard]] int profileCount() const;

      /*!
       * \brief The message shown in place of a plot, or empty when plotting.
       */
      [[nodiscard]] QString statusText() const;

      //! \returns The layer being profiled, or nullptr.
      [[nodiscard]] const MeshLayer *layer() const;

      /*!
       * \brief The profiles on the chart, values against elevations.
       *
       * Reuses the export shape so the same writers serve both plots; the
       * "instants" of a profile are its elevations, which is what a reader
       * of the file needs beside each value.
       */
      [[nodiscard]] const QVector<ExportSeries> &profiles() const;

    private:
      //! Re-reads the selection and rebuilds the chart.
      void refresh();

      //! Shows \a text instead of a plot.
      void showMessage(const QString &text);

      /*!
       * \brief The selected layered mesh layer, or nullptr.
       * \param[out] reason Why there is nothing to profile.
       */
      [[nodiscard]] MeshLayer *profilableLayer(QString &reason) const;

      QChartView *m_view = nullptr;
      QChart *m_chart = nullptr;
      QValueAxis *m_valueAxis = nullptr;
      QValueAxis *m_elevationAxis = nullptr;
      QLabel *m_status = nullptr;
      QStackedLayout *m_pages = nullptr;

      LayerStackModel *m_model = nullptr;

      //! The layer the profiles came from; not owned.
      const MeshLayer *m_layer = nullptr;

      //! What is on the chart, in the order it was drawn.
      QVector<ExportSeries> m_profiles;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_PANELS_PROFILEPLOTPANEL_H
