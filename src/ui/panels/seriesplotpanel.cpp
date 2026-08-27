#include "ui/panels/seriesplotpanel.h"

#include "layers/featurelayer.h"
#include "layers/timelayer.h"
#include "map/layerstackmodel.h"
#include "results/julianday.h"
#include "results/seriesexport.h"

#include <QCheckBox>
#include <QChart>
#include <QChartView>
#include <QDateTimeAxis>
#include <QLabel>
#include <QFileDialog>
#include <QLineSeries>
#include <QMessageBox>
#include <QStackedLayout>
#include <QToolButton>
#include <QVBoxLayout>
#include <QValueAxis>

namespace HydroCouple::Composer
{
  namespace
  {
    //! How many series to draw before the chart is more ink than information.
    constexpr int kMaximumSeries = 24;

    /*!
     * \brief \a julianDay positioned so a QDateTimeAxis labels it in UTC.
     *
     * The axis renders its labels in whatever zone the viewer sits in and
     * offers no way to change that, so the instant is handed over carrying
     * its UTC calendar fields in local spec. What that costs is the x
     * positions being offset from true epoch milliseconds by the viewer's
     * own offset, which nobody reads; what it buys is that two people
     * opening one run in two zones read the same times off the same plot,
     * and that the axis agrees with the transport readout underneath it.
     */
    QDateTime axisInstant(double julianDay)
    {
      const QDateTime utc = dateTimeFromJulianDay(julianDay);

      return utc.isValid() ? QDateTime(utc.date(), utc.time()) : QDateTime();
    }
  }

  namespace
  {
    /*!
     * \brief What \a layer calls the values it recorded.
     *
     * The component's own caption for the variable, which is the only label
     * it supplied; "Value" throws it away.
     */
    QString valueCaption(const FeatureLayer &layer, const ITimeLayer &recorded)
    {
      for (const AttributeField &field : layer.attributeFields())
      {
        if (field.name == recorded.valueAttribute())
        {
          return field.displayName;
        }
      }

      return {};
    }
  }

  SeriesPlotPanel::SeriesPlotPanel(QWidget *parent) : QWidget(parent)
  {
    setObjectName(QStringLiteral("seriesPlotPanel"));

    m_chart = new QChart;
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);

    m_timeAxis = new QDateTimeAxis;
    m_timeAxis->setTitleText(tr("Time"));
    m_timeAxis->setFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    m_chart->addAxis(m_timeAxis, Qt::AlignBottom);

    m_valueAxis = new QValueAxis;
    m_chart->addAxis(m_valueAxis, Qt::AlignLeft);

    m_view = new QChartView(m_chart, this);
    m_view->setObjectName(QStringLiteral("seriesPlotView"));
    m_view->setRenderHint(QPainter::Antialiasing);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("seriesPlotStatus"));
    m_status->setAlignment(Qt::AlignCenter);
    m_status->setWordWrap(true);

    // Stacked rather than hidden side by side: the message stands *where* the
    // plot would be, so an empty chart is never mistaken for a run that
    // recorded nothing.
    m_exportButton = new QToolButton(this);
    m_exportButton->setObjectName(QStringLiteral("exportSeriesButton"));
    m_exportButton->setText(tr("Export…"));
    m_exportButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_exportButton->setToolTip(tr("Write the plotted series to CSV or .dat"));
    m_exportButton->setEnabled(false);

    connect(m_exportButton, &QToolButton::clicked, this,
            &SeriesPlotPanel::onExportRequested);

    m_overlayCheck = new QCheckBox(tr("Overlay other runs"), this);
    m_overlayCheck->setObjectName(QStringLiteral("overlayRunsCheck"));
    m_overlayCheck->setChecked(true);
    m_overlayCheck->setToolTip(
      tr("Also draw the same feature from every other run on this mesh."));

    connect(m_overlayCheck, &QCheckBox::toggled, this,
            [this](bool) { refresh(); });

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->addWidget(m_exportButton);
    buttons->addWidget(m_overlayCheck);
    buttons->addStretch(1);

    m_pages = new QStackedLayout;
    m_pages->setContentsMargins(0, 0, 0, 0);
    m_pages->addWidget(m_view);
    m_pages->addWidget(m_status);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(buttons);
    layout->addLayout(m_pages, 1);

    showMessage(tr("Select a feature to plot what it recorded."));
  }

  SeriesPlotPanel::~SeriesPlotPanel() = default;

  void SeriesPlotPanel::setModel(LayerStackModel *model)
  {
    if (m_model == model)
    {
      return;
    }

    if (m_model)
    {
      m_model->disconnect(this);
    }

    m_model = model;

    if (m_model)
    {
      // The stack announces every change that would alter what is drawn,
      // and a selection is one of them. The attribute table listens to the
      // same signal for the same reason.
      connect(m_model, &LayerStackModel::renderChanged, this,
              [this] { refresh(); });
      connect(m_model, &QObject::destroyed, this,
              [this] { setModel(nullptr); });
    }

    refresh();
  }

  LayerStackModel *SeriesPlotPanel::model() const
  {
    return m_model;
  }

  int SeriesPlotPanel::seriesCount() const
  {
    return static_cast<int>(m_chart->series().size());
  }

  QString SeriesPlotPanel::statusText() const
  {
    // From which page is current, not from whether the label is visible: a
    // panel in a window that has never been shown reports every child as
    // invisible, and a test would then read "plotting fine" from a panel
    // that is explaining why it cannot.
    return m_pages->currentWidget() == m_status ? m_status->text() : QString();
  }

  bool SeriesPlotPanel::overlaysOtherRuns() const
  {
    return m_overlayCheck->isChecked();
  }

  void SeriesPlotPanel::setOverlaysOtherRuns(bool overlay)
  {
    m_overlayCheck->setChecked(overlay);
  }

  QVector<const FeatureLayer *> SeriesPlotPanel::plottedLayers() const
  {
    return m_layers;
  }

  QVector<double> SeriesPlotPanel::seriesValues(int index) const
  {
    const QList<QAbstractSeries *> series = m_chart->series();

    if (index < 0 || index >= series.size())
    {
      return {};
    }

    const auto *line = qobject_cast<const QLineSeries *>(series.at(index));

    if (!line)
    {
      return {};
    }

    QVector<double> values;
    values.reserve(static_cast<int>(line->count()));

    for (const QPointF &point : line->points())
    {
      values.append(point.y());
    }

    return values;
  }

  const QVector<ExportSeries> &SeriesPlotPanel::exportSeries() const
  {
    return m_plotted;
  }

  QStringList SeriesPlotPanel::exportTo(const QString &path,
                                       QString &message) const
  {
    const QVector<ExportSeries> &exportable = exportSeries();

    // Nothing plotted is refused by the writers themselves, which is where
    // it has to be refused anyway: a CSV of no series is a header and no
    // rows, and that is a file, not a failure to write one.
    if (path.endsWith(QStringLiteral(".dat"), Qt::CaseInsensitive))
    {
      return writeSeriesDat(path, exportable, message);
    }

    return writeSeriesCsv(path, exportable, message) ? QStringList{path}
                                                     : QStringList{};
  }

  void SeriesPlotPanel::onExportRequested()
  {
    // From clicked(), which is a release: a modal opened from a mouse press
    // wedges input on macOS.
    QString selectedFilter;
    QString path = QFileDialog::getSaveFileName(
      this, tr("Export plotted series"), QStringLiteral("series.csv"),
      tr("CSV file (*.csv);;SWMM time series (*.dat)"), &selectedFilter);

    if (path.isEmpty())
    {
      return;
    }

    // The native dialog does not always swap the suffix when the filter
    // changes, and the suffix is what chooses the format below.
    const bool wantsDat = selectedFilter.contains(QStringLiteral("*.dat"));

    if (wantsDat && !path.endsWith(QStringLiteral(".dat"), Qt::CaseInsensitive))
    {
      if (path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive))
      {
        path.chop(4);
      }

      path += QStringLiteral(".dat");
    }
    else if (!wantsDat
             && !path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)
             && !path.endsWith(QStringLiteral(".dat"), Qt::CaseInsensitive))
    {
      path += QStringLiteral(".csv");
    }

    QString message;
    const QStringList written = exportTo(path, message);

    if (written.isEmpty())
    {
      QMessageBox::warning(this, tr("Export failed"), message);
      return;
    }

    if (written.size() > 1)
    {
      // Said out loud, because the user asked for one file and got several
      // and would otherwise find only the last one they named.
      QMessageBox::information(
        this, tr("Export"),
        tr("A .dat file holds one series, so %1 files were written:\n%2")
          .arg(written.size())
          .arg(written.join(QLatin1Char('\n'))));
    }
  }

  QVector<FeatureLayer *> SeriesPlotPanel::plottableLayers(
    QString &reason) const
  {
    QVector<FeatureLayer *> plottable;

    if (!m_model)
    {
      reason = tr("Select a feature to plot what it recorded.");
      return plottable;
    }

    // Said separately from "nothing is selected": a selected feature on a
    // layer that was never recorded through time is a different answer, and
    // only one of the two is worth acting on.
    QString untimed;
    FeatureLayer *selected = nullptr;

    for (int row = 0; row < m_model->rowCount() && !selected; ++row)
    {
      auto *candidate = dynamic_cast<FeatureLayer *>(m_model->layerAt(row));

      if (!candidate || candidate->selection().isEmpty())
      {
        continue;
      }

      // Through the capability, not the class: a run's item and a difference
      // between two runs are both recorded through time, and a plot that
      // named one of them could only ever draw that one.
      const auto *recorded = dynamic_cast<const ITimeLayer *>(candidate);

      if (!recorded || recorded->timeCount() <= 0)
      {
        if (untimed.isEmpty())
        {
          untimed = candidate->name();
        }

        continue;
      }

      selected = candidate;
    }

    if (!selected)
    {
      reason = untimed.isEmpty()
                 ? tr("Select a feature to plot what it recorded.")
                 : tr("\"%1\" holds one instant, so there is no series to "
                      "plot.")
                     .arg(untimed);
      return plottable;
    }

    plottable.append(selected);

    if (!overlaysOtherRuns())
    {
      return plottable;
    }

    // Every other run at the same place. Feature N of one layer is feature N
    // of another only when the two were recorded on the same ground, so that
    // is asked outright rather than inferred from a matching count — which
    // is what would silently overlay a curve from somewhere else entirely.
    for (int row = 0; row < m_model->rowCount(); ++row)
    {
      auto *candidate = dynamic_cast<FeatureLayer *>(m_model->layerAt(row));

      if (!candidate || candidate == selected)
      {
        continue;
      }

      const auto *recorded = dynamic_cast<const ITimeLayer *>(candidate);

      if (!recorded || recorded->timeCount() <= 0)
      {
        continue;
      }

      QString elsewhere;

      if (sameGeometry(*selected, *candidate, elsewhere))
      {
        plottable.append(candidate);
      }
    }

    return plottable;
  }

  void SeriesPlotPanel::refresh()
  {
    m_chart->removeAllSeries();
    m_plotted.clear();
    m_layers.clear();

    QString reason;
    const QVector<FeatureLayer *> sources = plottableLayers(reason);

    if (sources.isEmpty())
    {
      showMessage(reason);
      return;
    }

    double lowest = std::numeric_limits<double>::max();
    double highest = std::numeric_limits<double>::lowest();
    double earliest = std::numeric_limits<double>::max();
    double latest = std::numeric_limits<double>::lowest();

    // Named by their layer only when there is more than one to tell apart.
    // On a single run "Feature 3" is what the user picked; prefixing it with
    // a layer name the chart title already carries is noise.
    const bool overlaid = sources.size() > 1;

    QString sharedCaption;
    bool oneCaption = true;

    for (FeatureLayer *source : sources)
    {
      const auto *recorded = dynamic_cast<const ITimeLayer *>(source);
      const QVector<double> instants = recorded->times();

      if (instants.isEmpty())
      {
        continue;
      }

      // The selected layer's features, read from every layer: the stack
      // holds one selection, and the overlay is the same place seen in each
      // run rather than a different place in each.
      QList<int> features(sources.first()->selection().begin(),
                          sources.first()->selection().end());
      std::sort(features.begin(), features.end());

      const QString caption = valueCaption(*source, *recorded);

      for (int feature : features)
      {
        if (m_plotted.size() >= kMaximumSeries)
        {
          break;
        }

        QVector<double> values;
        QString failure;

        if (!recorded->valuesOverTime(feature, values, failure))
        {
          continue;
        }

        auto *line = new QLineSeries;
        line->setName(overlaid
                        ? tr("%1 — feature %2").arg(source->name()).arg(feature)
                        : tr("Feature %1").arg(feature));

        ExportSeries record;
        record.name = line->name();

        for (int level = 0; level < values.size() && level < instants.size();
             ++level)
        {
          const QDateTime at = axisInstant(instants.at(level));

          // A value the model never produced has no place on an axis: a
          // non-finite one would collapse the range and take every real
          // value with it.
          if (!at.isValid() || !std::isfinite(values.at(level)))
          {
            continue;
          }

          line->append(static_cast<qreal>(at.toMSecsSinceEpoch()),
                       values.at(level));

          // The instant as recorded, not as positioned: the axis carries a
          // shifted one so its labels read in UTC, and an export written
          // from that would be off by the exporter's own time zone.
          record.julianDays.append(instants.at(level));
          record.values.append(values.at(level));

          lowest = std::min(lowest, values.at(level));
          highest = std::max(highest, values.at(level));
          earliest = std::min(earliest, instants.at(level));
          latest = std::max(latest, instants.at(level));
        }

        if (line->count() == 0)
        {
          delete line;
          continue;
        }

        m_chart->addSeries(line);
        line->attachAxis(m_timeAxis);
        line->attachAxis(m_valueAxis);
        m_plotted.append(record);

        if (!m_layers.contains(source))
        {
          m_layers.append(source);
        }
      }

      if (sharedCaption.isEmpty())
      {
        sharedCaption = caption;
      }
      else if (sharedCaption != caption)
      {
        oneCaption = false;
      }
    }

    if (m_plotted.isEmpty())
    {
      m_layers.clear();
      showMessage(tr("Nothing selected on \"%1\" has values to plot.")
                    .arg(sources.first()->name()));
      return;
    }

    // The span of what is actually drawn, across every layer: two runs of
    // different lengths overlaid on the first one's axis would cut the
    // longer one off at the point the shorter one stopped.
    m_timeAxis->setRange(axisInstant(earliest), axisInstant(latest));

    // A flat series would otherwise be drawn on a zero-height axis, which
    // renders as a line pinned to the frame and reads as missing data.
    if (qFuzzyCompare(lowest + 1.0, highest + 1.0))
    {
      const double padding = std::abs(lowest) > 0.0 ? std::abs(lowest) * 0.05
                                                    : 0.5;
      m_valueAxis->setRange(lowest - padding, highest + padding);
    }
    else
    {
      m_valueAxis->setRange(lowest, highest);
    }

    // One label for the axis only when every layer on it is showing the same
    // variable. Two different quantities sharing an axis is a chart that has
    // to say so rather than pick one of their names for both.
    m_valueAxis->setTitleText(oneCaption && !sharedCaption.isEmpty()
                                ? sharedCaption
                                : tr("Value"));

    // No title when several layers are overlaid: there is no one thing the
    // chart is of, and the legend already names every layer on it.
    m_chart->setTitle(m_layers.size() == 1 ? m_layers.first()->name()
                                           : QString());

    m_status->clear();
    m_pages->setCurrentWidget(m_view);
    m_exportButton->setEnabled(true);
  }

  void SeriesPlotPanel::showMessage(const QString &text)
  {
    m_layers.clear();
    m_plotted.clear();
    m_status->setText(text);
    m_pages->setCurrentWidget(m_status);
    m_exportButton->setEnabled(false);
  }

} // namespace HydroCouple::Composer
