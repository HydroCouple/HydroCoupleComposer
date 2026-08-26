#include "layers/dataitemlayer.h"

#include "gis/spatialreference.h"
#include "layers/ogrgeometryreader.h"

#include "hydrocouple.h"
#include "hydrocouplespatial.h"
#include "hydrocoupletemporal.h"

#include <QObject>

#include <algorithm>
#include <cmath>
#include <vector>

namespace HydroCouple::Composer
{
  namespace
  {
    using HydroCouple::DataKind;
    using HydroCouple::IComponentDataItem;

    namespace Spatial = HydroCouple::Spatial;

    //! The attribute a data item's values are offered under.
    const QString kValueField = QStringLiteral("value");

    QString itemName(const IComponentDataItem *item)
    {
      if (!item)
      {
        return QObject::tr("Data item");
      }

      const auto *identity = dynamic_cast<const HydroCouple::IIdentity *>(item);

      return identity ? QString::fromStdString(identity->caption())
                      : QObject::tr("Data item");
    }

    /*!
     * \brief The index of \a dimension within the item's dimension list.
     *
     * By pointer identity, which is what the standard's accessors hand back.
     * \returns The axis index, or -1 when the item does not list it.
     */
    int axisOf(const IComponentDataItem &item,
               const HydroCouple::IDimension *dimension)
    {
      if (!dimension)
      {
        return -1;
      }

      const std::vector<HydroCouple::IDimension *> dimensions =
        item.dimensions();

      for (size_t i = 0; i < dimensions.size(); ++i)
      {
        if (dimensions[i] == dimension)
        {
          return static_cast<int>(i);
        }
      }

      return -1;
    }

    /*!
     * \brief Reads one value per entity as a double.
     *
     * The entity axis is asked for by name rather than assumed to be
     * dimension 0. It usually is for a purely spatial item, but the SDK's
     * spatiotemporal items put **time** first — reading the leading
     * dimension there would colour every feature by a time index and look
     * entirely plausible doing it.
     *
     * Every other axis is read at its **last** index: for the time axis that
     * is the most recent step, which is what a map of a running model should
     * show, and for any other axis it is no more arbitrary than the first.
     * Choosing a step explicitly is the results viewer's job.
     */
    //! Axis zero is where the interface puts time — "the time dimension is
    //! dimension 0 of shape(); any additional dimensions follow" — and an
    //! item with no time axis answers -1 from timeIndex(), so the axis is
    //! never consulted for one. A function to work out which axis was written
    //! first and deleted: it could only ever return zero or contradict a
    //! documented invariant.
    constexpr int kTimeAxis = 0;

    bool readEntityValues(const IComponentDataItem &item, int entityAxis,
                          int timeAxis, int timeIndex,
                          QVector<double> &values, QString &message)
    {
      const std::vector<int64_t> shape = item.shape();

      if (shape.empty())
      {
        message = QObject::tr("The data item has no values to read.");
        return false;
      }

      const size_t axis =
        entityAxis >= 0 && entityAxis < static_cast<int>(shape.size())
          ? static_cast<size_t>(entityAxis)
          : 0;

      const int64_t count = shape[axis];

      if (count <= 0)
      {
        message = QObject::tr("The data item has no values to read.");
        return false;
      }

      std::vector<int64_t> start(shape.size(), 0);
      std::vector<int64_t> extent(shape.size(), 1);
      extent[axis] = count;

      for (size_t i = 0; i < shape.size(); ++i)
      {
        if (i == axis || shape[i] <= 0)
        {
          continue;
        }

        // The time axis goes where it was asked for; every other axis is
        // pinned to its last index, which for a layer index or a component
        // of a vector is the convention this had before time was a choice.
        start[i] = int(i) == timeAxis && timeIndex >= 0
                     ? std::min<int64_t>(timeIndex, shape[i] - 1)
                     : shape[i] - 1;
      }

      std::vector<double> buffer(static_cast<size_t>(count), 0.0);
      const int64_t bufferShape = count;

      HydroCouple::BufferDescriptor destination;
      destination.data = buffer.data();
      destination.kind = DataKind::Float64;
      destination.rank = 1;
      destination.shape = &bufferShape;

      std::string reason;

      // Float64 is asked for regardless of how the item stores its values:
      // the standard's copy converts, and the map has nothing to do with an
      // integer that a double cannot hold.
      if (!item.getValuesInto(destination, start, extent, &reason))
      {
        message = QString::fromStdString(reason);
        return false;
      }

      values.resize(static_cast<int>(count));

      for (int64_t i = 0; i < count; ++i)
      {
        values[static_cast<int>(i)] = buffer[static_cast<size_t>(i)];
      }

      return true;
    }

    void appendPointFeature(QVector<VectorFeature> &features,
                            const Spatial::IPoint *point)
    {
      if (!point)
      {
        return;
      }

      VectorFeature feature;
      feature.kind = GeometryKind::Point;

      QPolygonF part;
      part.append(QPointF(point->x(), point->y()));
      feature.parts.append(part);

      features.append(feature);
    }
  }

  DataItemLayer::DataItemLayer(const QString &name, IComponentDataItem *item)
    : FeatureLayer(name), m_item(item)
  {
  }

  DataItemLayer::~DataItemLayer() = default;

  bool DataItemLayer::isSpatial(const IComponentDataItem *item)
  {
    return dynamic_cast<const Spatial::IGeometryComponentDataItem *>(item)
           || dynamic_cast<const Spatial::INetworkComponentDataItem *>(item)
           || dynamic_cast<const Spatial::IPolyhedralSurfaceComponentDataItem *>(
             item);
  }

  IComponentDataItem *DataItemLayer::dataItem() const
  {
    return m_item;
  }

  QString DataItemLayer::valueAttribute() const
  {
    return m_valueAttribute;
  }

  std::unique_ptr<DataItemLayer> DataItemLayer::create(IComponentDataItem *item,
                                                       QString &message)
  {
    if (!isSpatial(item))
    {
      message = QObject::tr("This data item carries no geometry.");
      return nullptr;
    }

    std::unique_ptr<DataItemLayer> layer(
      new DataItemLayer(itemName(item), item));

    if (!layer->loadGeometry(message))
    {
      return nullptr;
    }

    layer->finishLoading();
    layer->refreshValues();

    return layer;
  }

  int DataItemLayer::entityAxis() const
  {
    // Asked of the item, never assumed: the entity axis is dimension 0 for a
    // plain spatial item and dimension 1 for a spatiotemporal one.
    if (const auto *geometries =
          dynamic_cast<const Spatial::IGeometryComponentDataItem *>(m_item))
    {
      return axisOf(*m_item, geometries->geometryDimension());
    }

    if (const auto *network =
          dynamic_cast<const Spatial::INetworkComponentDataItem *>(m_item))
    {
      return axisOf(*m_item,
                    network->networkDataObjectType()
                        == Spatial::NetworkDataObjectType::Edge
                      ? network->edgeDimension()
                      : network->vertexDimension());
    }

    if (const auto *surface =
          dynamic_cast<const Spatial::IPolyhedralSurfaceComponentDataItem *>(
            m_item))
    {
      switch (surface->meshDataObjectType())
      {
        case Spatial::MeshDataObjectType::Vertex:
          return axisOf(*m_item, surface->vertexDimension());
        case Spatial::MeshDataObjectType::Edge:
          return axisOf(*m_item, surface->edgeDimension());
        default:
          return axisOf(*m_item, surface->patchDimension());
      }
    }

    return -1;
  }

  bool DataItemLayer::loadGeometry(QString &message)
  {
    QVector<VectorFeature> collected;
    const Spatial::ISpatialReferenceSystem *reference = nullptr;

    // ── Geometry item ────────────────────────────────────────────────────
    if (const auto *geometries =
          dynamic_cast<const Spatial::IGeometryComponentDataItem *>(m_item))
    {
      for (int64_t i = 0; i < geometries->geometryCount(); ++i)
      {
        Spatial::IGeometry *geometry = geometries->geometry(i);

        if (!geometry)
        {
          continue;
        }

        if (!reference)
        {
          reference = geometry->spatialReferenceSystem();
        }

        VectorFeature feature;

        // Through WKB rather than a type switch: every geometry the standard
        // admits, multi-parts and collections included, arrives one way.
        if (collectWkbGeometry(geometry->getWKB(), feature.parts,
                               feature.kind))
        {
          collected.append(feature);
        }
      }
    }
    // ── Network item ─────────────────────────────────────────────────────
    else if (const auto *network =
               dynamic_cast<const Spatial::INetworkComponentDataItem *>(m_item))
    {
      const Spatial::INetwork *graph = network->network();

      if (!graph)
      {
        message = QObject::tr("The network data item has no network.");
        return false;
      }

      // Values sit on either the edges or the nodes, and the layer must draw
      // whichever they describe — colouring edges by node values would put
      // each value on the wrong thing.
      const bool onEdges =
        network->networkDataObjectType()
        == Spatial::NetworkDataObjectType::Edge;

      if (onEdges)
      {
        for (int64_t i = 0; i < graph->edgeCount(); ++i)
        {
          const Spatial::IEdge *edge = graph->edge(i);

          if (!edge || !edge->orig() || !edge->dest())
          {
            continue;
          }

          VectorFeature feature;
          feature.kind = GeometryKind::Line;

          QPolygonF part;
          part.append(QPointF(edge->orig()->x(), edge->orig()->y()));
          part.append(QPointF(edge->dest()->x(), edge->dest()->y()));
          feature.parts.append(part);

          collected.append(feature);
        }
      }
      else
      {
        for (int64_t i = 0; i < graph->vertexCount(); ++i)
        {
          appendPointFeature(collected, graph->vertex(i));
        }
      }
    }
    // ── Polyhedral surface / TIN item ────────────────────────────────────
    else if (const auto *surfaceItem =
               dynamic_cast<const Spatial::IPolyhedralSurfaceComponentDataItem *>(
                 m_item))
    {
      const Spatial::IPolyhedralSurface *surface =
        surfaceItem->polyhedralSurface();

      if (!surface)
      {
        message = QObject::tr("The mesh data item has no surface.");
        return false;
      }

      const bool onVertices =
        surfaceItem->meshDataObjectType()
        == Spatial::MeshDataObjectType::Vertex;

      if (onVertices)
      {
        for (int64_t i = 0; i < surface->vertexCount(); ++i)
        {
          appendPointFeature(collected, surface->vertex(i));
        }
      }
      else
      {
        for (int64_t i = 0; i < surface->patchCount(); ++i)
        {
          const Spatial::IPolygon *patch = surface->patch(i);

          if (!patch)
          {
            continue;
          }

          if (!reference)
          {
            reference = patch->spatialReferenceSystem();
          }

          VectorFeature feature;

          if (collectWkbGeometry(patch->getWKB(), feature.parts, feature.kind))
          {
            collected.append(feature);
          }
        }
      }
    }

    if (collected.isEmpty())
    {
      message = QObject::tr("The data item holds no geometry to draw.");
      return false;
    }

    for (VectorFeature &feature : collected)
    {
      addFeature(std::move(feature));
    }

    // The item's own CRS, when it declares one. Rebuilt through Composer's
    // GDAL-backed type so the map has one kind of CRS to reason about.
    if (reference && !reference->srText().empty())
    {
      QString crsMessage;
      setCrs(SpatialReference::fromDefinition(
        QString::fromStdString(reference->srText()), crsMessage));
    }

    return true;
  }

  int DataItemLayer::timeCount() const
  {
    const auto *series =
      dynamic_cast<const HydroCouple::Temporal::ITimeSeriesComponentDataItem *>(
        m_item);

    return series ? int(series->timeCount()) : 0;
  }

  int DataItemLayer::timeIndex() const
  {
    const int count = timeCount();

    if (count <= 0)
    {
      return -1;
    }

    // Unchosen means the last level, which for a component still running is
    // "now" — and is what a layer showed before time was a choice at all.
    // Resolved here rather than in the constructor, where the item has not
    // been asked how many levels it has.
    return m_timeIndex < 0 ? count - 1 : std::min(m_timeIndex, count - 1);
  }

  bool DataItemLayer::setTimeIndex(int index)
  {
    const int count = timeCount();

    if (count <= 0)
    {
      return false;
    }

    const int wanted = std::clamp(index, 0, count - 1);

    if (wanted == timeIndex())
    {
      return true;
    }

    m_timeIndex = wanted;

    return refreshValues();
  }

  double DataItemLayer::timeAt(int index) const
  {
    const auto *series =
      dynamic_cast<const HydroCouple::Temporal::ITimeSeriesComponentDataItem *>(
        m_item);

    if (!series || index < 0 || index >= int(series->timeCount()))
    {
      return 0.0;
    }

    const HydroCouple::Temporal::IDateTime *at = series->time(index);

    return at ? at->julianDay() : 0.0;
  }

  int DataItemLayer::nearestTime(double julianDay) const
  {
    const int count = timeCount();

    if (count <= 0)
    {
      return -1;
    }

    int nearest = 0;
    double best = std::abs(timeAt(0) - julianDay);

    for (int index = 1; index < count; ++index)
    {
      const double distance = std::abs(timeAt(index) - julianDay);

      // Strictly closer, so the earliest of two equally near levels wins
      // rather than the later one — a tie should not depend on which way
      // the loop happens to run.
      if (distance < best)
      {
        best = distance;
        nearest = index;
      }
    }

    return nearest;
  }

  const QVector<double> &DataItemLayer::valuesAcrossTime() const
  {
    const int levels = timeCount();

    // Only the levels not read yet: a component still running records more of
    // them as it goes, and the ones already pooled do not change.
    for (int level = m_acrossTimeLevels; level < levels; ++level)
    {
      QVector<double> values;
      QString message;

      if (readEntityValues(*m_item, entityAxis(), kTimeAxis, level, values,
                           message))
      {
        m_acrossTime.append(values);
      }
    }

    m_acrossTimeLevels = levels;

    return m_acrossTime;
  }

  QVector<double> DataItemLayer::numericValues(const QString &field) const
  {
    // The value attribute is the only one recorded through time; anything
    // else belongs to the geometry, which does not move between levels. A
    // static item has no levels to pool, and asking it for one would answer
    // with nothing rather than with what it holds.
    if (!m_item || field != m_valueAttribute || timeCount() < 1)
    {
      return FeatureLayer::numericValues(field);
    }

    return valuesAcrossTime();
  }

  bool DataItemLayer::refreshValues()
  {
    if (!m_item)
    {
      return false;
    }

    QVector<double> values;
    QString message;

    if (!readEntityValues(*m_item, entityAxis(), kTimeAxis, timeIndex(),
                          values, message))
    {
      return false;
    }

    QVector<AttributeField> fields = attributeFields();

    if (m_valueFieldIndex < 0)
    {
      AttributeField field;
      field.name = kValueField;
      field.displayName = QObject::tr("Value");

      // The value definition is an IDescription, so it names itself — a
      // legend reading "Value" where the component said "Water depth (m)"
      // throws away the only label the component supplied.
      if (const HydroCouple::IValueDefinition *definition =
            m_item->valueDefinition())
      {
        const QString caption = QString::fromStdString(definition->caption());

        if (!caption.isEmpty())
        {
          field.displayName = caption;
        }
      }

      // Values change every time step, which is what tells the classifier
      // that breaks computed once will go stale.
      field.isDynamic = true;

      m_valueFieldIndex = static_cast<int>(fields.size());
      m_valueAttribute = field.name;

      fields.append(field);
      setFields(fields);
    }

    // Written straight into the features, so the style reads them through the
    // same attribute path as any other column.
    const QVector<VectorFeature> &existing = features();

    for (int i = 0; i < existing.size(); ++i)
    {
      QVector<QVariant> attributes = existing.at(i).attributes;
      attributes.resize(m_valueFieldIndex + 1);

      // Fewer values than features is a partitioned or partly-filled item;
      // the features past the end keep no value rather than a stale one.
      attributes[m_valueFieldIndex] =
        i < values.size() ? QVariant(values.at(i)) : QVariant();

      setFeatureAttributes(i, attributes);
    }

    restyle();

    return true;
  }

} // namespace HydroCouple::Composer
