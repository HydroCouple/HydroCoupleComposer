#include "ui/panels/profileplotpanel.h"

#include "layers/meshlayer.h"
#include "map/layerstackmodel.h"

#include <QChart>
#include <QChartView>
#include <QLabel>
#include <QLineSeries>
#include <QStackedLayout>
#include <QValueAxis>

#include <algorithm>
#include <cmath>
#include <limits>

namespace HydroCouple::Composer
{
  namespace
  {
    //! How many columns to profile before the chart is more ink than answer.
    constexpr int kMaximumProfiles = 12;
  }

  ProfilePlotPanel::ProfilePlotPanel(QWidget *parent) : QWidget(parent)
  {
    setObjectName(QStringLiteral("profilePlotPanel"));

    m_chart = new QChart;
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);

    m_valueAxis = new QValueAxis;
    m_valueAxis->setTitleText(tr("Value"));
    m_chart->addAxis(m_valueAxis, Qt::AlignBottom);

    m_elevationAxis = new QValueAxis;
    m_elevationAxis->setTitleText(tr("Elevation"));
    m_chart->addAxis(m_elevationAxis, Qt::AlignLeft);

    m_view = new QChartView(m_chart, this);
    m_view->setObjectName(QStringLiteral("profilePlotView"));
    m_view->setRenderHint(QPainter::Antialiasing);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("profilePlotStatus"));
    m_status->setAlignment(Qt::AlignCenter);
    m_status->setWordWrap(true);

    m_pages = new QStackedLayout(this);
    m_pages->setContentsMargins(0, 0, 0, 0);
    m_pages->addWidget(m_view);
    m_pages->addWidget(m_status);

    showMessage(tr("Select a column to profile its water column."));
  }

  ProfilePlotPanel::~ProfilePlotPanel() = default;

  void ProfilePlotPanel::setModel(LayerStackModel *model)
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
      connect(m_model, &LayerStackModel::renderChanged, this,
              [this] { refresh(); });
      connect(m_model, &QObject::destroyed, this,
              [this] { setModel(nullptr); });
    }

    refresh();
  }

  LayerStackModel *ProfilePlotPanel::model() const
  {
    return m_model;
  }

  int ProfilePlotPanel::profileCount() const
  {
    return static_cast<int>(m_profiles.size());
  }

  QString ProfilePlotPanel::statusText() const
  {
    return m_pages->currentWidget() == m_status ? m_status->text() : QString();
  }

  const MeshLayer *ProfilePlotPanel::layer() const
  {
    return m_layer;
  }

  const QVector<ExportSeries> &ProfilePlotPanel::profiles() const
  {
    return m_profiles;
  }

  MeshLayer *ProfilePlotPanel::profilableLayer(QString &reason) const
  {
    if (!m_model)
    {
      reason = tr("Select a column to profile its water column.");
      return nullptr;
    }

    for (int row = 0; row < m_model->rowCount(); ++row)
    {
      auto *candidate = dynamic_cast<MeshLayer *>(m_model->layerAt(row));

      if (!candidate || candidate->selection().isEmpty())
      {
        continue;
      }

      // Said apart from "nothing is selected": a flat mesh is a perfectly
      // good layer with nothing underneath it, and telling the user to
      // select something they have already selected explains nothing.
      if (!candidate->isLayered())
      {
        reason = tr("\"%1\" is a flat mesh, so there is no column to profile.")
                   .arg(candidate->name());
        return nullptr;
      }

      // A profile is a column of the mesh, and only a face is a column. A
      // selection of nodes or edges names something a water column does not
      // hang beneath.
      if (candidate->entity() != MeshEntity::Face)
      {
        reason = tr("\"%1\" is drawn by %2, and a column hangs under a face.")
                   .arg(candidate->name(),
                        candidate->entity() == MeshEntity::Edge
                          ? tr("edge")
                          : tr("node"));
        return nullptr;
      }

      return candidate;
    }

    reason = tr("Select a column to profile its water column.");
    return nullptr;
  }

  void ProfilePlotPanel::refresh()
  {
    m_chart->removeAllSeries();
    m_profiles.clear();
    m_layer = nullptr;

    QString reason;
    MeshLayer *source = profilableLayer(reason);

    if (!source)
    {
      showMessage(reason);
      return;
    }

    QList<int> columns(source->selection().begin(), source->selection().end());
    std::sort(columns.begin(), columns.end());

    if (columns.size() > kMaximumProfiles)
    {
      columns = columns.mid(0, kMaximumProfiles);
    }

    double lowestValue = std::numeric_limits<double>::max();
    double highestValue = std::numeric_limits<double>::lowest();
    double lowestElevation = std::numeric_limits<double>::max();
    double highestElevation = std::numeric_limits<double>::lowest();

    QString failure;

    for (int column : columns)
    {
      QVector<double> values;
      QVector<double> elevations;

      if (!source->columnProfile(column, values, elevations, failure))
      {
        continue;
      }

      auto *line = new QLineSeries;
      line->setName(tr("Column %1").arg(column));

      ExportSeries record;
      record.name = line->name();

      for (int layer = 0; layer < values.size() && layer < elevations.size();
           ++layer)
      {
        if (!std::isfinite(values.at(layer))
            || !std::isfinite(elevations.at(layer)))
        {
          continue;
        }

        // Value across, elevation up: the profile is read the way the water
        // column stands, not the way an array is indexed.
        line->append(values.at(layer), elevations.at(layer));

        record.julianDays.append(elevations.at(layer));
        record.values.append(values.at(layer));

        lowestValue = std::min(lowestValue, values.at(layer));
        highestValue = std::max(highestValue, values.at(layer));
        lowestElevation = std::min(lowestElevation, elevations.at(layer));
        highestElevation = std::max(highestElevation, elevations.at(layer));
      }

      if (line->count() == 0)
      {
        delete line;
        continue;
      }

      m_chart->addSeries(line);
      line->attachAxis(m_valueAxis);
      line->attachAxis(m_elevationAxis);
      m_profiles.append(record);
    }

    if (m_profiles.isEmpty())
    {
      showMessage(failure.isEmpty()
                    ? tr("Nothing selected on \"%1\" has a profile to draw.")
                        .arg(source->name())
                    : failure);
      return;
    }

    m_layer = source;

    // A column of one value would otherwise be drawn on a zero-width axis,
    // which renders pinned to the frame and reads as missing data.
    const auto padded = [](double low, double high)
    {
      if (!qFuzzyCompare(low + 1.0, high + 1.0))
      {
        return QPair<double, double>{low, high};
      }

      const double padding = std::abs(low) > 0.0 ? std::abs(low) * 0.05 : 0.5;

      return QPair<double, double>{low - padding, high + padding};
    };

    const QPair<double, double> values = padded(lowestValue, highestValue);
    const QPair<double, double> heights =
      padded(lowestElevation, highestElevation);

    m_valueAxis->setRange(values.first, values.second);
    m_elevationAxis->setRange(heights.first, heights.second);

    if (!source->valueAttribute().isEmpty())
    {
      m_valueAxis->setTitleText(source->valueAttribute());
    }

    m_chart->setTitle(source->name());
    m_status->clear();
    m_pages->setCurrentWidget(m_view);
  }

  void ProfilePlotPanel::showMessage(const QString &text)
  {
    m_layer = nullptr;
    m_profiles.clear();
    m_status->setText(text);
    m_pages->setCurrentWidget(m_status);
  }

} // namespace HydroCouple::Composer
