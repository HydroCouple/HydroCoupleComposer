#include "ui/panels/seriesplotpanel.h"

#include "layers/dataitemlayer.h"
#include "map/layerstackmodel.h"
#include "results/julianday.h"

#include <QChart>
#include <QChartView>
#include <QDateTimeAxis>
#include <QLabel>
#include <QLineSeries>
#include <QStackedLayout>
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
    m_pages = new QStackedLayout(this);
    m_pages->setContentsMargins(0, 0, 0, 0);
    m_pages->addWidget(m_view);
    m_pages->addWidget(m_status);

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

  const DataItemLayer *SeriesPlotPanel::layer() const
  {
    return m_layer;
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

  DataItemLayer *SeriesPlotPanel::plottableLayer(QString &reason) const
  {
    if (!m_model)
    {
      reason = tr("Select a feature to plot what it recorded.");
      return nullptr;
    }

    for (int row = 0; row < m_model->rowCount(); ++row)
    {
      auto *candidate = dynamic_cast<DataItemLayer *>(m_model->layerAt(row));

      if (!candidate || candidate->selection().isEmpty())
      {
        continue;
      }

      // Said separately from "nothing is selected", because a selected
      // feature on a layer that was never recorded through time is a
      // different answer and only one of the two is worth acting on.
      if (candidate->timeCount() <= 0)
      {
        reason = tr("\"%1\" holds one instant, so there is no series to plot.")
                   .arg(candidate->name());
        return nullptr;
      }

      return candidate;
    }

    reason = tr("Select a feature to plot what it recorded.");
    return nullptr;
  }

  void SeriesPlotPanel::refresh()
  {
    m_chart->removeAllSeries();
    m_layer = nullptr;

    QString reason;
    DataItemLayer *source = plottableLayer(reason);

    if (!source)
    {
      showMessage(reason);
      return;
    }

    const QVector<double> instants = source->times();

    if (instants.isEmpty())
    {
      showMessage(tr("\"%1\" records no instants to plot against.")
                    .arg(source->name()));
      return;
    }

    // Sorted, so the same selection always plots in the same order and the
    // legend does not reshuffle itself between two reads of one run.
    QList<int> features(source->selection().begin(), source->selection().end());
    std::sort(features.begin(), features.end());

    if (features.size() > kMaximumSeries)
    {
      features = features.mid(0, kMaximumSeries);
    }

    double lowest = std::numeric_limits<double>::max();
    double highest = std::numeric_limits<double>::lowest();
    bool any = false;

    for (int feature : features)
    {
      QVector<double> values;
      QString failure;

      if (!source->valuesOverTime(feature, values, failure))
      {
        continue;
      }

      auto *line = new QLineSeries;
      line->setName(tr("Feature %1").arg(feature));

      for (int level = 0; level < values.size() && level < instants.size();
           ++level)
      {
        const QDateTime at = axisInstant(instants.at(level));

        // A value the model never produced has no place on an axis: a
        // non-finite one would collapse the range and take every real value
        // with it.
        if (!at.isValid() || !std::isfinite(values.at(level)))
        {
          continue;
        }

        line->append(static_cast<qreal>(at.toMSecsSinceEpoch()),
                     values.at(level));

        lowest = std::min(lowest, values.at(level));
        highest = std::max(highest, values.at(level));
      }

      if (line->count() == 0)
      {
        delete line;
        continue;
      }

      m_chart->addSeries(line);
      line->attachAxis(m_timeAxis);
      line->attachAxis(m_valueAxis);
      any = true;
    }

    if (!any)
    {
      showMessage(tr("Nothing selected on \"%1\" has values to plot.")
                    .arg(source->name()));
      return;
    }

    m_layer = source;

    m_timeAxis->setRange(axisInstant(instants.first()),
                         axisInstant(instants.last()));

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

    // The component's own caption for the values, which is the only label it
    // supplied; "Value" throws it away.
    const QVector<AttributeField> fields = source->attributeFields();

    for (const AttributeField &field : fields)
    {
      if (field.name == source->valueAttribute())
      {
        m_valueAxis->setTitleText(field.displayName);
        break;
      }
    }

    m_chart->setTitle(source->name());
    m_status->clear();
    m_pages->setCurrentWidget(m_view);
  }

  void SeriesPlotPanel::showMessage(const QString &text)
  {
    m_layer = nullptr;
    m_status->setText(text);
    m_pages->setCurrentWidget(m_status);
  }

} // namespace HydroCouple::Composer
