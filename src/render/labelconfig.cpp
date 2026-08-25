#include "render/labelconfig.h"

#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>

namespace HydroCouple::Composer
{

  bool LabelPainter::scaleVisible(const LabelConfig &config,
                                  double scaleDenominator)
  {
    if (!(scaleDenominator > 0.0))
    {
      return true;
    }

    // A larger denominator is a smaller scale, i.e. zoomed further out.
    if (config.minScale > 0.0 && scaleDenominator > config.minScale)
    {
      return false;
    }

    return !(config.maxScale > 0.0 && scaleDenominator < config.maxScale);
  }

  QRectF LabelPainter::labelRect(const LabelConfig &config,
                                 const QPointF &anchor, const QSizeF &textSize)
  {
    const double gap = config.offset;

    QPointF topLeft;

    switch (config.placement)
    {
      case LabelPlacement::Above:
        topLeft = QPointF(anchor.x() - textSize.width() * 0.5,
                          anchor.y() - textSize.height() - gap);
        break;

      case LabelPlacement::Below:
        topLeft =
          QPointF(anchor.x() - textSize.width() * 0.5, anchor.y() + gap);
        break;

      case LabelPlacement::Left:
        topLeft = QPointF(anchor.x() - textSize.width() - gap,
                          anchor.y() - textSize.height() * 0.5);
        break;

      case LabelPlacement::Right:
        topLeft =
          QPointF(anchor.x() + gap, anchor.y() - textSize.height() * 0.5);
        break;

      case LabelPlacement::Centered:
        topLeft = QPointF(anchor.x() - textSize.width() * 0.5,
                          anchor.y() - textSize.height() * 0.5);
        break;

      case LabelPlacement::AboveRight:
      default:
        topLeft =
          QPointF(anchor.x() + gap, anchor.y() - textSize.height() - gap);
        break;
    }

    return QRectF(topLeft, textSize);
  }

  void LabelPainter::draw(QPainter &painter, const LabelConfig &config,
                          const QRectF &box, const QString &text)
  {
    painter.save();
    painter.setFont(config.font);

    if (config.haloWidth > 0.0 && config.haloColor.alpha() > 0)
    {
      // Stroked glyph outlines rather than text drawn four times offset by a
      // pixel: the cheap trick leaves gaps on diagonals and thickens the
      // letterforms unevenly.
      QPainterPath glyphs;
      glyphs.addText(box.left(),
                     box.top() + QFontMetricsF(config.font).ascent(),
                     config.font, text);

      QPainterPathStroker stroker;
      stroker.setWidth(config.haloWidth * 2.0);
      stroker.setJoinStyle(Qt::RoundJoin);

      painter.setPen(Qt::NoPen);
      painter.setBrush(config.haloColor);
      painter.drawPath(stroker.createStroke(glyphs));
    }

    painter.setPen(config.color);
    painter.drawText(box, Qt::AlignLeft | Qt::AlignVCenter, text);
    painter.restore();
  }

  LabelCollisionMap::LabelCollisionMap(double padding) : m_padding(padding)
  {
  }

  bool LabelCollisionMap::tryPlace(const QRectF &box)
  {
    if (box.isEmpty())
    {
      return false;
    }

    const QRectF padded =
      box.adjusted(-m_padding, -m_padding, m_padding, m_padding);

    for (const QRectF &placed : m_placed)
    {
      if (placed.intersects(padded))
      {
        return false;
      }
    }

    m_placed.append(padded);

    return true;
  }

  int LabelCollisionMap::placedCount() const
  {
    return static_cast<int>(m_placed.size());
  }

  void LabelCollisionMap::clear()
  {
    m_placed.clear();
  }

} // namespace HydroCouple::Composer
