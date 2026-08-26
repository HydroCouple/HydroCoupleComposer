/*!
 * \file   vectorprobe.h
 * \brief  A vector layer a test can put features into.
 *
 * FeatureLayer's data entry points are protected, which is right — a layer's
 * features come from a loader — and leaves a test with nothing to build a
 * small, exactly-known dataset from. This is that.
 *
 * Shared rather than repeated in each suite: the drape tests and the picking
 * tests need the same three shapes, and two copies of a fixture drift into
 * two slightly different fixtures.
 *
 * No Q_OBJECT: it adds no signals of its own, and declaring one would require
 * the header to be listed as a target source for AUTOMOC to reach it.
 */

#ifndef HYDROCOUPLECOMPOSER_TESTS_VECTORPROBE_H
#define HYDROCOUPLECOMPOSER_TESTS_VECTORPROBE_H

#include "layers/featurelayer.h"

#include <QPointF>
#include <QPolygonF>
#include <QString>
#include <QVector>

namespace HydroCouple::Composer::Testing
{
  /*!
   * \brief A feature layer with public geometry entry.
   */
  class VectorProbe : public FeatureLayer
  {
    public:
      explicit VectorProbe(const QString &name) : FeatureLayer(name)
      {
        AttributeField field;
        field.name = QStringLiteral("name");
        field.type = QMetaType::QString;

        setFields({ field });
      }

      //! Adds one line through \a points, named \a name.
      void addLine(const QVector<QPointF> &points,
                   const QString &name = QString())
      {
        add(GeometryKind::Line, points, name);
      }

      //! Adds one ring through \a points.
      void addRing(const QVector<QPointF> &points,
                   const QString &name = QString())
      {
        add(GeometryKind::Polygon, points, name);
      }

      //! Adds one point.
      void addPoint(const QPointF &position, const QString &name = QString())
      {
        add(GeometryKind::Point, { position }, name);
      }

    private:
      void add(GeometryKind kind, const QVector<QPointF> &points,
               const QString &name)
      {
        VectorFeature feature;
        feature.kind = kind;
        feature.parts.append(QPolygonF(points));
        feature.attributes.append(
          name.isEmpty() ? QStringLiteral("F%1").arg(featureCount()) : name);

        addFeature(std::move(feature));
        finishLoading();
      }
  };

} // namespace HydroCouple::Composer::Testing

#endif // HYDROCOUPLECOMPOSER_TESTS_VECTORPROBE_H
