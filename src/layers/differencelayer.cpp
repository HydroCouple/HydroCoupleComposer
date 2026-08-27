#include "layers/differencelayer.h"

#include "hydrocouple.h"

#include <QObject>

#include <algorithm>
#include <cmath>
#include <limits>

namespace HydroCouple::Composer
{
  namespace
  {
    //! The attribute the differences are offered under.
    const QString kDifferenceField = QStringLiteral("difference");
  }

  DifferenceLayer::DifferenceLayer(const QString &name,
                                   std::unique_ptr<DataItemLayer> base,
                                   std::unique_ptr<DataItemLayer> other)
    : FeatureLayer(name), m_base(std::move(base)), m_other(std::move(other))
  {
  }

  DifferenceLayer::~DifferenceLayer() = default;

  std::unique_ptr<DifferenceLayer> DifferenceLayer::create(
    HydroCouple::IComponentDataItem *base,
    HydroCouple::IComponentDataItem *other, QString &message)
  {
    std::unique_ptr<DataItemLayer> baseLayer =
      DataItemLayer::create(base, message);

    if (!baseLayer)
    {
      return nullptr;
    }

    std::unique_ptr<DataItemLayer> otherLayer =
      DataItemLayer::create(other, message);

    if (!otherLayer)
    {
      return nullptr;
    }

    if (!sameGeometry(*baseLayer, *otherLayer, message))
    {
      return nullptr;
    }

    const QString name =
      QObject::tr("%1 − %2").arg(baseLayer->name(), otherLayer->name());

    std::unique_ptr<DifferenceLayer> layer(new DifferenceLayer(
      name, std::move(baseLayer), std::move(otherLayer)));

    // The base's geometry, copied rather than referenced: this layer is a
    // layer, and everything that draws one reads its own features.
    for (const VectorFeature &feature : layer->m_base->features())
    {
      VectorFeature copy;
      copy.kind = feature.kind;
      copy.parts = feature.parts;
      layer->addFeature(std::move(copy));
    }

    layer->finishLoading();

    if (!layer->refreshValues())
    {
      message = QObject::tr("The two runs' values could not be read.");
      return nullptr;
    }

    return layer;
  }

  bool DifferenceLayer::sameGeometry(const FeatureLayer &left,
                                     const FeatureLayer &right,
                                     QString &message)
  {
    if (left.featureCount() != right.featureCount())
    {
      message =
        QObject::tr("The two runs recorded %1 and %2 features, so they are "
                    "not the same geometry.")
          .arg(left.featureCount())
          .arg(right.featureCount());
      return false;
    }

    if (left.featureCount() == 0)
    {
      message = QObject::tr("Neither run recorded any geometry to compare.");
      return false;
    }

    // Taken from the extent rather than fixed, so the same mesh compares the
    // same way whether it is measured in metres or in degrees.
    const QRectF extent = left.extent();
    const double diagonal =
      std::hypot(extent.width(), extent.height());
    const double tolerance =
      diagonal > 0.0 ? diagonal * 1e-9 : 1e-9;

    const QVector<VectorFeature> &here = left.features();
    const QVector<VectorFeature> &there = right.features();

    for (int index = 0; index < here.size(); ++index)
    {
      if (here.at(index).kind != there.at(index).kind
          || here.at(index).parts.size() != there.at(index).parts.size())
      {
        message = QObject::tr("The two runs' geometries differ at feature %1, "
                              "so they are not the same ground.")
                    .arg(index);
        return false;
      }

      for (int part = 0; part < here.at(index).parts.size(); ++part)
      {
        const QPolygonF &mine = here.at(index).parts.at(part);
        const QPolygonF &yours = there.at(index).parts.at(part);

        if (mine.size() != yours.size())
        {
          message =
            QObject::tr("The two runs' geometries differ at feature %1, so "
                        "they are not the same ground.")
              .arg(index);
          return false;
        }

        for (int vertex = 0; vertex < mine.size(); ++vertex)
        {
          if (std::abs(mine.at(vertex).x() - yours.at(vertex).x()) > tolerance
              || std::abs(mine.at(vertex).y() - yours.at(vertex).y())
                   > tolerance)
          {
            message =
              QObject::tr("The two runs' geometries differ at feature %1, so "
                          "they are not the same ground.")
                .arg(index);
            return false;
          }
        }
      }
    }

    message.clear();
    return true;
  }

  QString DifferenceLayer::valueAttribute() const
  {
    return m_valueAttribute;
  }

  int DifferenceLayer::timeCount() const
  {
    return m_base ? m_base->timeCount() : 0;
  }

  int DifferenceLayer::timeIndex() const
  {
    const int levels = timeCount();

    if (levels <= 0)
    {
      return -1;
    }

    return m_timeIndex < 0 ? levels - 1 : std::min(m_timeIndex, levels - 1);
  }

  double DifferenceLayer::timeAt(int index) const
  {
    return m_base ? m_base->timeAt(index) : 0.0;
  }

  int DifferenceLayer::nearestTime(double julianDay) const
  {
    return m_base ? m_base->nearestTime(julianDay) : -1;
  }

  QVector<double> DifferenceLayer::times() const
  {
    return m_base ? m_base->times() : QVector<double>{};
  }

  bool DifferenceLayer::setTimeIndex(int index)
  {
    const int levels = timeCount();

    if (levels <= 0)
    {
      return false;
    }

    const int wanted = std::clamp(index, 0, levels - 1);

    if (wanted == timeIndex())
    {
      return true;
    }

    m_timeIndex = wanted;

    return refreshValues();
  }

  int DifferenceLayer::otherLevelFor(double julianDay) const
  {
    if (!m_other)
    {
      return -1;
    }

    // Matched on the instant, not on the level. Two runs that reported at
    // different rates share nothing but the clock, and pairing level three
    // with level three would compare hour three against hour thirty. A run
    // with no time axis answers with its only level, so a steady baseline
    // differences against every instant of the other.
    return m_other->timeCount() > 0 ? m_other->nearestTime(julianDay)
                                    : m_other->timeIndex();
  }

  bool DifferenceLayer::differencesAt(int index, QVector<double> &values,
                                      QString &message) const
  {
    values.clear();

    if (!m_base || !m_other)
    {
      message = QObject::tr("This comparison has lost one of its runs.");
      return false;
    }

    QVector<double> mine;

    if (!m_base->valuesAtTime(index, mine, message))
    {
      return false;
    }

    QVector<double> theirs;

    if (!m_other->valuesAtTime(otherLevelFor(timeAt(index)), theirs, message))
    {
      return false;
    }

    // As far as both recorded. The features past the end keep no value
    // rather than a stale one, which is what a partly-filled item already
    // does on a single run.
    const int entities =
      static_cast<int>(std::min(mine.size(), theirs.size()));

    values.reserve(entities);

    for (int entity = 0; entity < entities; ++entity)
    {
      values.append(mine.at(entity) - theirs.at(entity));
    }

    message.clear();
    return true;
  }

  bool DifferenceLayer::refreshValues()
  {
    QVector<double> values;
    QString message;

    if (!differencesAt(timeIndex(), values, message))
    {
      return false;
    }

    QVector<AttributeField> fields = attributeFields();

    if (m_valueFieldIndex < 0)
    {
      AttributeField field;
      field.name = kDifferenceField;

      // Named from what is being differenced, so a legend on a comparison
      // says which variable disagreed rather than only that something did.
      // Taken from the base's own field rather than from its layer name:
      // the caption is what the component called the variable, and the
      // layer name is what the run browser called the layer.
      QString variable = QObject::tr("value");

      for (const AttributeField &existing : m_base->attributeFields())
      {
        if (existing.name == m_base->valueAttribute())
        {
          variable = existing.displayName;
          break;
        }
      }

      field.displayName = QObject::tr("Δ %1").arg(variable);
      field.isDynamic = true;

      m_valueFieldIndex = static_cast<int>(fields.size());
      m_valueAttribute = field.name;

      fields.append(field);
      setFields(fields);
    }

    const QVector<VectorFeature> &existing = features();

    for (int index = 0; index < existing.size(); ++index)
    {
      QVector<QVariant> attributes = existing.at(index).attributes;
      attributes.resize(m_valueFieldIndex + 1);

      attributes[m_valueFieldIndex] =
        index < values.size() && std::isfinite(values.at(index))
          ? QVariant(values.at(index))
          : QVariant();

      setFeatureAttributes(index, attributes);
    }

    // The pool grows as a comparison is stepped, exactly as a single run's
    // does, so the breaks are computed over the differences seen so far
    // rather than recomputed from the level on screen.
    m_acrossTime.clear();
    m_acrossTimeLevels = 0;

    restyle();

    return true;
  }

  bool DifferenceLayer::valuesOverTime(int feature, QVector<double> &values,
                                       QString &message) const
  {
    values.clear();

    const int levels = timeCount();

    if (levels <= 0)
    {
      message = QObject::tr("This comparison was not recorded through time.");
      return false;
    }

    if (feature < 0 || feature >= featureCount())
    {
      message = QObject::tr("There is no feature %1 to read.").arg(feature);
      return false;
    }

    QVector<double> mine;

    if (!m_base->valuesOverTime(feature, mine, message))
    {
      return false;
    }

    QVector<double> theirs;

    if (m_other->timeCount() > 0)
    {
      if (!m_other->valuesOverTime(feature, theirs, message))
      {
        return false;
      }
    }
    else
    {
      QVector<double> level;

      if (!m_other->valuesAtTime(m_other->timeIndex(), level, message))
      {
        return false;
      }

      if (feature < level.size())
      {
        theirs.append(level.at(feature));
      }
    }

    values.reserve(levels);

    for (int level = 0; level < levels && level < mine.size(); ++level)
    {
      // Through the same matching the map uses, so a series read beside a
      // difference map is the same subtraction rather than a second one.
      const int theirLevel =
        m_other->timeCount() > 0 ? otherLevelFor(timeAt(level)) : 0;

      if (theirLevel < 0 || theirLevel >= theirs.size())
      {
        message = QObject::tr("The other run has no value to subtract at %1.")
                    .arg(timeAt(level));
        return false;
      }

      values.append(mine.at(level) - theirs.at(theirLevel));
    }

    message.clear();
    return true;
  }

  QVector<double> DifferenceLayer::numericValues(const QString &field) const
  {
    const int levels = timeCount();

    if (field != m_valueAttribute || levels < 1)
    {
      return FeatureLayer::numericValues(field);
    }

    if (m_acrossTimeLevels < levels)
    {
      for (int level = m_acrossTimeLevels; level < levels; ++level)
      {
        QVector<double> values;
        QString message;

        if (!differencesAt(level, values, message))
        {
          break;
        }

        for (const double value : values)
        {
          if (std::isfinite(value))
          {
            m_acrossTime.append(value);
          }
        }

        m_acrossTimeLevels = level + 1;
      }
    }

    return m_acrossTime;
  }

} // namespace HydroCouple::Composer
