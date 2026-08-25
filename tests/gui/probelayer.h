/*!
 * \file   probelayer.h
 * \brief  A minimal MapLayer for the C1b tests.
 *
 * The concrete layers arrive with C1d and C2, so the stack and canvas are
 * tested against a layer that exists only to be observed: it records the
 * painter state it was handed and draws one rectangle at a known place.
 *
 * No Q_OBJECT: it adds no signals or slots of its own, and declaring it would
 * require the header to be listed as a target source for AUTOMOC to reach it.
 */

#ifndef HYDROCOUPLECOMPOSER_TESTS_PROBELAYER_H
#define HYDROCOUPLECOMPOSER_TESTS_PROBELAYER_H

#include "map/maplayer.h"
#include "map/extentmath.h"
#include "map/maptransform.h"
#include "render/attributeprovider.h"
#include "render/layerstyle.h"

#include <QHash>
#include <QPainter>
#include <QRectF>
#include <QVariant>
#include <QVector>

namespace HydroCouple::Composer::Testing
{
  /*!
   * \brief A layer that records how it was drawn.
   */
  class ProbeLayer : public MapLayer
  {
    public:
      /*!
       * \brief Constructs a probe.
       * \param name Layer name.
       * \param extent The extent to report, in the layer's own CRS.
       * \param color Fill colour, so overlapping probes can be told apart.
       */
      ProbeLayer(const QString &name, const QRectF &extent,
                 const QColor &color = Qt::black)
        : MapLayer(name), m_extent(extent), m_color(color)
      {
      }

      [[nodiscard]] QRectF extent() const override { return m_extent; }

      /*!
       * \brief Reports a new extent.
       * \param extent The replacement extent.
       */
      void setExtent(const QRectF &extent)
      {
        m_extent = extent;
        notifyExtentChanged();
      }

      void render(QPainter &painter, const MapTransform &transform) override
      {
        ++renderCount;
        lastOpacity = painter.opacity();
        lastScale = transform.scale();

        if (order)
        {
          order->append(name());
        }

        painter.fillRect(QRectF(transform.toScreen(m_extent.topLeft()),
                                transform.toScreen(m_extent.bottomRight()))
                           .normalized(),
                         m_color);
      }

      //! Shared render-order log, when the test wants one.
      QVector<QString> *order = nullptr;

      int renderCount = 0;
      double lastOpacity = -1.0;
      double lastScale = -1.0;

    private:
      QRectF m_extent;
      QColor m_color;
  };

  /*!
   * \brief A layer with features and attributes, for the styling tests.
   *
   * Draws each feature as a dot in the colour its style chooses, so a test
   * can read the styling decision back out of the rendered image rather than
   * only out of the object that made it.
   */
  class ProbeFeatureLayer : public MapLayer, public IAttributeProvider
  {
    public:
      explicit ProbeFeatureLayer(const QString &name) : MapLayer(name) {}

      /*!
       * \brief Adds one feature.
       * \param position Where it sits, in world coordinates.
       * \param attributes Its attribute values, keyed by field name.
       */
      void addFeature(const QPointF &position,
                      const QHash<QString, QVariant> &attributes)
      {
        m_positions.append(position);

        for (auto it = attributes.constBegin(); it != attributes.constEnd();
             ++it)
        {
          QVector<QVariant> &column = m_attributes[it.key()];
          column.resize(m_positions.size());
          column[m_positions.size() - 1] = it.value();
        }

        notifyExtentChanged();
      }

      /*!
       * \brief Declares a themeable field.
       * \param field The field to declare.
       */
      void declareField(const AttributeField &field)
      {
        m_fields.append(field);
      }

      //! The style, for editing.
      LayerStyle &styleRef() { return m_style; }

      //! Recomputes the style from the data and announces the change.
      bool restyle()
      {
        const bool built = m_style.rebuild(*this);
        notifyAppearanceChanged();

        return built;
      }

      // ── MapLayer ─────────────────────────────────────────────────────
      [[nodiscard]] const LayerStyle *style() const override
      {
        return &m_style;
      }

      [[nodiscard]] LayerStyle *style() override { return &m_style; }

      [[nodiscard]] QRectF extent() const override
      {
        QRectF bounds;
        bool valid = false;

        // Through expandTo, because QRectF::united() drops the zero-area
        // rect of a single point and would leave the last one's extent.
        for (const QPointF &position : m_positions)
        {
          expandTo(bounds, valid, position);
        }

        return bounds;
      }

      void render(QPainter &painter, const MapTransform &transform) override
      {
        ++renderCount;
        drawnColors.clear();

        for (int i = 0; i < m_positions.size(); ++i)
        {
          const QColor color = m_style.colorFor(*this, i);

          // An invalid colour means the style declined to draw this feature,
          // which is not the same as drawing it in a default colour.
          if (!color.isValid())
          {
            continue;
          }

          drawnColors.append(color);

          painter.setPen(Qt::NoPen);
          painter.setBrush(color);
          painter.drawEllipse(transform.toScreen(m_positions.at(i)),
                              m_style.symbol().size, m_style.symbol().size);
        }
      }

      // ── IAttributeProvider ───────────────────────────────────────────
      [[nodiscard]] QVector<AttributeField> attributeFields() const override
      {
        return m_fields;
      }

      [[nodiscard]] int featureCount() const override
      {
        return static_cast<int>(m_positions.size());
      }

      [[nodiscard]] QVariant attributeValue(int feature,
                                            const QString &field) const override
      {
        const auto it = m_attributes.constFind(field);

        if (it == m_attributes.constEnd() || feature < 0
            || feature >= it->size())
        {
          return {};
        }

        return it->at(feature);
      }

      int renderCount = 0;
      QVector<QColor> drawnColors;

    private:
      QVector<QPointF> m_positions;
      QHash<QString, QVector<QVariant>> m_attributes;
      QVector<AttributeField> m_fields;
      LayerStyle m_style;
  };

} // namespace HydroCouple::Composer::Testing

#endif // HYDROCOUPLECOMPOSER_TESTS_PROBELAYER_H
