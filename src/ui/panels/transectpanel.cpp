#include "ui/panels/transectpanel.h"

#include "layers/meshlayer.h"
#include "map/layerstackmodel.h"

#include <QLabel>
#include <QPainter>
#include <QStackedLayout>

#include <algorithm>
#include <cmath>

namespace HydroCouple::Composer
{
  namespace
  {
    //! Room for the axis labels, in pixels.
    constexpr int kLeftMargin = 56;
    constexpr int kBottomMargin = 28;
    constexpr int kTopMargin = 10;
    constexpr int kRightMargin = 12;
  }

  // ── TransectView ──────────────────────────────────────────────────────────

  TransectView::TransectView(QWidget *parent) : QWidget(parent)
  {
    setObjectName(QStringLiteral("transectView"));
    setMinimumSize(200, 120);
  }

  void TransectView::setSection(const TransectSection &section,
                                const MeshLayer *layer)
  {
    m_section = section;
    m_layer = layer;
    update();
  }

  const TransectSection &TransectView::section() const
  {
    return m_section;
  }

  QRectF TransectView::plotArea() const
  {
    return QRectF(rect()).adjusted(kLeftMargin, kTopMargin, -kRightMargin,
                                   -kBottomMargin);
  }

  QRectF TransectView::cellRect(const TransectCell &cell) const
  {
    const QRectF area = plotArea();

    if (m_section.isEmpty() || m_section.length <= 0.0 || area.width() <= 0.0
        || area.height() <= 0.0)
    {
      return {};
    }

    const auto [lowest, highest] = m_section.elevationRange();
    const double span = highest - lowest;

    if (span <= 0.0)
    {
      return {};
    }

    // The x axis runs the whole line, not the extent of the cells: a line
    // that starts off the mesh has to show that it did.
    const auto across = [&](double distance)
    {
      return area.left() + area.width() * (distance / m_section.length);
    };

    // Up, so the bed is at the bottom, which is the only orientation a
    // section is ever read in.
    const auto up = [&](double elevation)
    {
      return area.bottom() - area.height() * ((elevation - lowest) / span);
    };

    const double left = across(cell.startDistance);
    const double right = across(cell.endDistance);
    const double top = up(cell.topElevation);
    const double bottom = up(cell.bottomElevation);

    return QRectF(QPointF(std::min(left, right), std::min(top, bottom)),
                  QPointF(std::max(left, right), std::max(top, bottom)));
  }

  void TransectView::paintEvent(QPaintEvent *event)
  {
    Q_UNUSED(event)

    QPainter painter(this);
    painter.fillRect(rect(), palette().base());

    const QRectF area = plotArea();

    if (m_section.isEmpty() || area.width() <= 0.0 || area.height() <= 0.0)
    {
      return;
    }

    // Antialiasing off for the cells: neighbouring cells share an edge, and
    // blending across it invents colours that are in neither of them — which
    // on a classified section reads as a class that does not exist.
    painter.setRenderHint(QPainter::Antialiasing, false);

    for (const TransectCell &cell : m_section.cells)
    {
      const QRectF box = cellRect(cell);

      if (box.isEmpty())
      {
        continue;
      }

      const QColor color =
        m_layer ? m_layer->colorForCellValue(cell.value) : QColor(Qt::gray);

      // An invalid colour is the classification saying the value falls
      // outside it. Left unpainted, as the map leaves such a feature
      // undrawn, rather than filled with a colour that means nothing.
      if (!color.isValid())
      {
        continue;
      }

      painter.fillRect(box, color);
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(palette().color(QPalette::WindowText));
    painter.drawLine(area.bottomLeft(), area.bottomRight());
    painter.drawLine(area.topLeft(), area.bottomLeft());

    const auto [lowest, highest] = m_section.elevationRange();

    painter.drawText(
      QRectF(0.0, area.top() - kTopMargin, kLeftMargin - 4.0, 20.0),
      Qt::AlignRight | Qt::AlignVCenter,
      QString::number(highest, 'g', 4));
    painter.drawText(
      QRectF(0.0, area.bottom() - 10.0, kLeftMargin - 4.0, 20.0),
      Qt::AlignRight | Qt::AlignVCenter, QString::number(lowest, 'g', 4));

    painter.drawText(QRectF(area.left(), area.bottom() + 4.0, 80.0, 20.0),
                     Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("0"));
    painter.drawText(QRectF(area.right() - 80.0, area.bottom() + 4.0, 80.0,
                            20.0),
                     Qt::AlignRight | Qt::AlignVCenter,
                     QString::number(m_section.length, 'g', 4));
  }

  // ── TransectPanel ─────────────────────────────────────────────────────────

  TransectPanel::TransectPanel(QWidget *parent) : QWidget(parent)
  {
    setObjectName(QStringLiteral("transectPanel"));

    m_view = new TransectView(this);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("transectStatus"));
    m_status->setAlignment(Qt::AlignCenter);
    m_status->setWordWrap(true);

    m_pages = new QStackedLayout(this);
    m_pages->setContentsMargins(0, 0, 0, 0);
    m_pages->addWidget(m_view);
    m_pages->addWidget(m_status);

    showMessage(tr("Draw a section line on the map to cut through the "
                   "water column."));
  }

  TransectPanel::~TransectPanel() = default;

  void TransectPanel::setModel(LayerStackModel *model)
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

  LayerStackModel *TransectPanel::model() const
  {
    return m_model;
  }

  void TransectPanel::setLine(const QPolygonF &line)
  {
    m_line = line;
    refresh();
  }

  const QPolygonF &TransectPanel::line() const
  {
    return m_line;
  }

  int TransectPanel::cellCount() const
  {
    return static_cast<int>(m_section.cells.size());
  }

  const TransectSection &TransectPanel::section() const
  {
    return m_section;
  }

  const MeshLayer *TransectPanel::layer() const
  {
    return m_layer;
  }

  TransectView *TransectPanel::view() const
  {
    return m_view;
  }

  QString TransectPanel::statusText() const
  {
    return m_pages->currentWidget() == m_status ? m_status->text() : QString();
  }

  MeshLayer *TransectPanel::cuttableLayer(QString &reason) const
  {
    if (!m_model)
    {
      reason = tr("Draw a section line on the map to cut through the "
                  "water column.");
      return nullptr;
    }

    for (int row = 0; row < m_model->rowCount(); ++row)
    {
      auto *candidate = dynamic_cast<MeshLayer *>(m_model->layerAt(row));

      // Layering rather than visibility decides: a flat mesh above a layered
      // one is not a refusal, it is simply not the thing being cut.
      if (candidate && candidate->isLayered())
      {
        return candidate;
      }
    }

    reason = tr("No layered mesh is loaded, so there is nothing to cut "
                "through.");
    return nullptr;
  }

  void TransectPanel::refresh()
  {
    m_section = TransectSection{};

    QString reason;
    MeshLayer *source = cuttableLayer(reason);

    if (!source)
    {
      showMessage(reason);
      return;
    }

    if (m_line.size() < 2)
    {
      showMessage(tr("Draw a section line across \"%1\" to cut through it.")
                    .arg(source->name()));
      return;
    }

    QString failure;

    if (!source->transect(m_line, m_section, failure))
    {
      showMessage(failure);
      return;
    }

    m_layer = source;
    m_view->setSection(m_section, source);
    m_status->clear();
    m_pages->setCurrentWidget(m_view);
  }

  void TransectPanel::showMessage(const QString &text)
  {
    m_layer = nullptr;
    m_section = TransectSection{};
    m_view->setSection(m_section, nullptr);
    m_status->setText(text);
    m_pages->setCurrentWidget(m_status);
  }

} // namespace HydroCouple::Composer
