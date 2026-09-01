/*!
 * \file   spatialstubs.h
 * \brief  Minimal implementations of HydroCouple's spatial interfaces.
 *
 * The geometry itself now comes from the SDK: `PointAdapter`,
 * `PolygonAdapter` and friends serve the standard's IGeometry family over the
 * SDK's value types, so these layers are verified against what a real
 * component would publish rather than against a viewer-shaped imitation of
 * it.
 *
 * What remains here is the data-item shell — the SDK provides spatiotemporal
 * geometry items but no purely spatial one, so the plain-geometry case is
 * still built on AbstractComponentDataItem here, exactly as a component would.
 */

#ifndef HYDROCOUPLECOMPOSER_TESTS_SPATIALSTUBS_H
#define HYDROCOUPLECOMPOSER_TESTS_SPATIALSTUBS_H

#include "hydrocouplesdk/temporal/timedata.h"

#include "hydrocouplespatial.h"
#include "hydrocoupletemporal.h"

#include "hydrocouplesdk/data/abstractcomponentdataitem.h"
#include "hydrocouplesdk/data/componentdataitem.h"
#include "hydrocouplesdk/core/dimension.h"
#include "hydrocouplesdk/core/identity.h"
#include "hydrocouplesdk/core/valuedefinition.h"
#include "hydrocouplesdk/spatial/geometryadapters.h"
#include "hydrocouplesdk/io/meshdefinition.h"
#include "hydrocouplesdk/spatial/meshadapters.h"

#include <ogr_geometry.h>
#include <ogr_spatialref.h>

#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace HydroCouple::Composer::Testing
{
  namespace Spatial = HydroCouple::Spatial;

  //! A CRS that reports its own WKT, which is all a layer reads.
  class StubCrs : public Spatial::ISpatialReferenceSystem
  {
    public:
      explicit StubCrs(int epsg)
      {
        OGRSpatialReference reference;
        reference.importFromEPSG(epsg);
        reference.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        char *wkt = nullptr;
        reference.exportToWkt(&wkt);
        m_wkt = wkt ? wkt : "";
        CPLFree(wkt);

        m_srid = epsg;
      }

      [[nodiscard]] int authSRID() const override { return m_srid; }
      [[nodiscard]] const std::string &authName() const override
      {
        return m_authority;
      }
      [[nodiscard]] const std::string &srText() const override
      {
        return m_wkt;
      }
      [[nodiscard]] HydroCouple::IUnit::DistanceUnits distanceUnits()
        const override
      {
        return HydroCouple::IUnit::DistanceUnits::Degrees;
      }

    private:
      std::string m_authority = "EPSG";
      std::string m_wkt;
      int m_srid = 0;
  };

  //! The SDK's own adapters, which is what a component publishes.
  using StubPoint = HydroCouple::SDK::Spatial::PointAdapter;
  using StubPolygonBase = HydroCouple::SDK::Spatial::PolygonAdapter;

  //! Builds an SDK PointAdapter positioned at (x, y).
  inline std::unique_ptr<StubPoint> makePoint(
    double x, double y, unsigned int index,
    Spatial::ISpatialReferenceSystem *crs)
  {
    return std::make_unique<StubPoint>(
      HydroCouple::SDK::Spatial::Point(x, y), "p" + std::to_string(index),
      index, crs);
  }

  //! Builds an SDK PolygonAdapter over a closed ring of coordinates.
  inline std::unique_ptr<StubPolygonBase> makePolygon(
    const std::vector<std::pair<double, double>> &ring, unsigned int index,
    Spatial::ISpatialReferenceSystem *crs)
  {
    HydroCouple::SDK::Spatial::LineString exterior;

    for (const auto &vertex : ring)
    {
      exterior.addPoint(
        HydroCouple::SDK::Spatial::Point(vertex.first, vertex.second));
    }

    // Closed explicitly: a ring left open is a line, and the polygon that
    // results has a side missing.
    if (!ring.empty() && ring.front() != ring.back())
    {
      exterior.addPoint(
        HydroCouple::SDK::Spatial::Point(ring.front().first,
                                         ring.front().second));
    }

    return std::make_unique<StubPolygonBase>(
      HydroCouple::SDK::Spatial::Polygon(exterior),
      "poly" + std::to_string(index), index, crs);
  }

  /*!
   * \brief A geometry data item over borrowed geometries.
   *
   * Built on the SDK's own bases, exactly as a component would: the identity,
   * dimensions and hyperslab plumbing come from AbstractComponentDataItem and
   * ComponentDataItem1D, and only the spatial half is written here.
   */
  class StubGeometryItem
    : public HydroCouple::SDK::AbstractComponentDataItem,
      public HydroCouple::SDK::ComponentDataItem1D<double>,
      public virtual Spatial::IGeometryComponentDataItem
  {
      using Store = HydroCouple::SDK::ComponentDataItem1D<double>;

    public:
      StubGeometryItem(std::string_view id,
                       std::vector<Spatial::IGeometry *> geometries)
        : AbstractComponentDataItem(id, {&m_dimension}, nullptr, nullptr),
          Store(static_cast<int>(geometries.size()), 0.0),
          m_geometries(std::move(geometries)),
          m_dimension("geometries", "Geometry dimension")
      {
      }

      //! Sets the value carried by one geometry.
      void setValue(int index, double value)
      {
        Store::rawData()[static_cast<size_t>(index)] = value;
      }

      // ── IComponentDataItem ───────────────────────────────────────────
      [[nodiscard]] std::vector<int64_t> shape() const override
      {
        return Store::storageShape();
      }

      [[nodiscard]] HydroCouple::DataKind dataKind() const override
      {
        return Store::storageKind();
      }

      [[nodiscard]] bool getValuesInto(
        const HydroCouple::BufferDescriptor &destination,
        std::span<const int64_t> start, std::span<const int64_t> count,
        std::string *message = nullptr) const override
      {
        return Store::getSlab(destination, start, count, message);
      }

      [[nodiscard]] bool setValuesFrom(
        const HydroCouple::BufferDescriptor &source,
        std::span<const int64_t> start, std::span<const int64_t> count,
        std::string *message = nullptr) override
      {
        return Store::setSlab(source, start, count, message);
      }

      // ── IGeometryComponentDataItem ───────────────────────────────────
      [[nodiscard]] Spatial::IGeometry::GeometryType geometryType()
        const override
      {
        return m_geometries.empty()
                 ? Spatial::IGeometry::GeometryType::Geometry
                 : m_geometries.front()->geometryType();
      }

      [[nodiscard]] int64_t geometryCount() const override
      {
        return static_cast<int64_t>(m_geometries.size());
      }

      [[nodiscard]] Spatial::IGeometry *geometry(
        int64_t geometryIndex) const override
      {
        ++geometryReads;

        return geometryIndex >= 0
                   && geometryIndex < static_cast<int64_t>(m_geometries.size())
                 ? m_geometries[static_cast<size_t>(geometryIndex)]
                 : nullptr;
      }

      //! How often geometry has been asked for, so a test can tell whether a
      //! value refresh re-read the mesh.
      mutable int geometryReads = 0;

      [[nodiscard]] HydroCouple::IDimension *geometryDimension() const override
      {
        return const_cast<HydroCouple::SDK::Dimension *>(&m_dimension);
      }

      [[nodiscard]] Spatial::IEnvelope *envelope() const override
      {
        return const_cast<HydroCouple::SDK::Spatial::EnvelopeAdapter *>(
          &m_envelope);
      }

    private:
      std::vector<Spatial::IGeometry *> m_geometries;
      HydroCouple::SDK::Dimension m_dimension;
      HydroCouple::SDK::Spatial::EnvelopeAdapter m_envelope;
  };

  /*!
   * \brief A geometry item shaped like the SDK's spatiotemporal ones.
   *
   * Dimensions are {time, geometries} — **time first**, exactly as
   * TimeGeometryComponentDataItem declares them. A viewer that assumed the
   * entity axis was dimension 0 would read one value per time step here and
   * colour every feature by a time index.
   */
  class StubTimeGeometryItem
    : public HydroCouple::SDK::AbstractComponentDataItem,
      public HydroCouple::SDK::ComponentDataItem2D<double>,
      public virtual Spatial::IGeometryComponentDataItem,
      public virtual HydroCouple::Temporal::ITimeSeriesComponentDataItem
  {
      using Store = HydroCouple::SDK::ComponentDataItem2D<double>;

    public:
      /*!
       * \brief Builds an item of \a timeSteps over \a geometries.
       *
       * \param firstJulianDay The instant of step zero.
       * \param spacingDays How far apart the steps are; the two together
       *        are what lets a test put two items on different axes and
       *        check that a shared clock still lines them up.
       */
      StubTimeGeometryItem(std::string_view id, int timeSteps,
                           std::vector<Spatial::IGeometry *> geometries,
                           double firstJulianDay = 2451545.0,
                           double spacingDays = 1.0,
                           std::string_view valueCaption = {})
        : AbstractComponentDataItem(
            id, {&m_timeDimension, &m_dimension},
            valueCaption.empty()
              ? nullptr
              : HydroCouple::SDK::Quantity::unitLess(valueCaption),
            nullptr),
          Store(timeSteps, static_cast<int>(geometries.size()), 0.0),
          m_geometries(std::move(geometries)),
          m_timeDimension("time", "Time dimension"),
          m_dimension("geometries", "Geometry dimension"),
          m_span("span", firstJulianDay,
                 timeSteps > 0 ? (timeSteps - 1) * spacingDays : 0.0)
      {
        m_times.reserve(static_cast<size_t>(timeSteps));
        m_julianDays.reserve(static_cast<size_t>(timeSteps));

        for (int step = 0; step < timeSteps; ++step)
        {
          const double instant = firstJulianDay + step * spacingDays;

          m_times.push_back(std::make_unique<HydroCouple::SDK::Temporal::TimeData>(
            "t" + std::to_string(step), instant));
          m_julianDays.push_back(instant);
        }

      }

      // ── ITimeSeriesComponentDataItem ─────────────────────────────────
      [[nodiscard]] const HydroCouple::Temporal::IDateTime *time(
        int64_t timeIndex) const override
      {
        return timeIndex >= 0
                   && timeIndex < static_cast<int64_t>(m_times.size())
                 ? m_times[static_cast<size_t>(timeIndex)].get()
                 : nullptr;
      }

      [[nodiscard]] int64_t timeCount() const override
      {
        return static_cast<int64_t>(m_times.size());
      }

      [[nodiscard]] HydroCouple::IDimension *timeDimension() const override
      {
        return const_cast<HydroCouple::SDK::Dimension *>(&m_timeDimension);
      }

      [[nodiscard]] std::span<const double> times() const override
      {
        return {m_julianDays.data(), m_julianDays.size()};
      }

      [[nodiscard]] HydroCouple::Temporal::ITimeSpan *timeSpan()
        const override
      {
        return const_cast<HydroCouple::SDK::Temporal::TimeSpan *>(&m_span);
      }

      //! Sets the value of one geometry at one time step.
      void setValue(int step, int index, double value)
      {
        Store::operator()(step, index) = value;
      }

      // ── IComponentDataItem ───────────────────────────────────────────
      [[nodiscard]] std::vector<int64_t> shape() const override
      {
        return Store::storageShape();
      }

      [[nodiscard]] HydroCouple::DataKind dataKind() const override
      {
        return Store::storageKind();
      }

      [[nodiscard]] bool getValuesInto(
        const HydroCouple::BufferDescriptor &destination,
        std::span<const int64_t> start, std::span<const int64_t> count,
        std::string *message = nullptr) const override
      {
        ++m_reads;

        return Store::getSlab(destination, start, count, message);
      }

      //! How many slabs have been read out of this item.
      [[nodiscard]] int reads() const
      {
        return m_reads;
      }

      //! Forgets the count, so a test can measure one operation.
      void resetReads()
      {
        m_reads = 0;
      }

      [[nodiscard]] bool setValuesFrom(
        const HydroCouple::BufferDescriptor &source,
        std::span<const int64_t> start, std::span<const int64_t> count,
        std::string *message = nullptr) override
      {
        return Store::setSlab(source, start, count, message);
      }

      // ── IGeometryComponentDataItem ───────────────────────────────────
      [[nodiscard]] Spatial::IGeometry::GeometryType geometryType()
        const override
      {
        return m_geometries.empty()
                 ? Spatial::IGeometry::GeometryType::Geometry
                 : m_geometries.front()->geometryType();
      }

      [[nodiscard]] int64_t geometryCount() const override
      {
        return static_cast<int64_t>(m_geometries.size());
      }

      [[nodiscard]] Spatial::IGeometry *geometry(
        int64_t geometryIndex) const override
      {
        return geometryIndex >= 0
                   && geometryIndex < static_cast<int64_t>(m_geometries.size())
                 ? m_geometries[static_cast<size_t>(geometryIndex)]
                 : nullptr;
      }

      [[nodiscard]] HydroCouple::IDimension *geometryDimension() const override
      {
        return const_cast<HydroCouple::SDK::Dimension *>(&m_dimension);
      }

      [[nodiscard]] Spatial::IEnvelope *envelope() const override
      {
        return const_cast<HydroCouple::SDK::Spatial::EnvelopeAdapter *>(
          &m_envelope);
      }

    private:
      std::vector<Spatial::IGeometry *> m_geometries;
      std::vector<std::unique_ptr<HydroCouple::SDK::Temporal::TimeData>> m_times;
      std::vector<double> m_julianDays;
      HydroCouple::SDK::Temporal::TimeSpan m_span;

      //! Slab reads served, so a test can tell a cached answer from a
      //! re-read one. Mutable because reading does not change the item.
      mutable int m_reads = 0;
      HydroCouple::SDK::Dimension m_timeDimension;
      HydroCouple::SDK::Dimension m_dimension;
      HydroCouple::SDK::Spatial::EnvelopeAdapter m_envelope;
  };

  /*!
   * \brief A uniform regular grid, with an optional inactive cell.
   *
   * Through the SDK's Identity, as the SDK's own grid class does:
   * IRegularGrid2D is an IIdentity, which carries the description and signal
   * plumbing no test wants to reimplement.
   */
  /*!
   * \brief A recorded mesh item: values through time on one mesh entity.
   *
   * The surface is the SDK's own adapter over a MeshDefinition, so a test of
   * how a layer draws a mesh is a test against what a component actually
   * publishes -- including the bulk view, which is the only place a
   * surface's edges exist.
   */
  class StubTimeSurfaceItem
    : public HydroCouple::SDK::AbstractComponentDataItem,
      public HydroCouple::SDK::ComponentDataItem2D<double>,
      public virtual Spatial::IPolyhedralSurfaceComponentDataItem,
      public virtual HydroCouple::Temporal::ITimeSeriesComponentDataItem
  {
      using Store = HydroCouple::SDK::ComponentDataItem2D<double>;

    public:
      /*!
       * \brief Builds an item of \a timeSteps over \a mesh.
       *
       * \param attachedTo Which entity the values belong to; the entity
       *        count follows from it, because that is the relationship a
       *        layer has to get right.
       */
      StubTimeSurfaceItem(std::string_view id,
                          const HydroCouple::SDK::IO::MeshDefinition &mesh,
                          Spatial::MeshDataObjectType attachedTo,
                          int timeSteps)
        : AbstractComponentDataItem(id, {&m_timeDimension, &m_entityDimension},
                                    nullptr, nullptr),
          Store(timeSteps, static_cast<int>(entityCount(mesh, attachedTo)),
                0.0),
          m_surface(
            std::make_unique<HydroCouple::SDK::Spatial::PolyhedralSurfaceAdapter>(
              HydroCouple::SDK::Spatial::polyhedralSurfaceFromMesh(mesh),
              mesh)),
          m_attachedTo(attachedTo),
          m_timeDimension("time", "Time dimension"),
          m_entityDimension("entity", "Mesh entity dimension"),
          m_span("span", 2451545.0, timeSteps > 0 ? timeSteps - 1 : 0)
      {
        for (int step = 0; step < timeSteps; ++step)
        {
          const double instant = 2451545.0 + step;

          m_times.push_back(
            std::make_unique<HydroCouple::SDK::Temporal::TimeData>(
              "t" + std::to_string(step), instant));
          m_julianDays.push_back(instant);
        }
      }

      //! How many entities of \a attachedTo the mesh holds.
      [[nodiscard]] static int64_t entityCount(
        const HydroCouple::SDK::IO::MeshDefinition &mesh,
        Spatial::MeshDataObjectType attachedTo)
      {
        switch (attachedTo)
        {
          case Spatial::MeshDataObjectType::Vertex:
            return mesh.nodeCount();
          case Spatial::MeshDataObjectType::Edge:
            return mesh.edgeCount();
          default:
            return mesh.faceCount();
        }
      }

      //! Sets the value of one entity at one time step.
      void setValue(int step, int entity, double value)
      {
        Store::operator()(step, entity) = value;
      }

      // ── IPolyhedralSurfaceComponentDataItem ──────────────────────────
      [[nodiscard]] Spatial::MeshDataObjectType meshDataObjectType()
        const override
      {
        return m_attachedTo;
      }

      [[nodiscard]] Spatial::SpatialDataType meshDataType() const override
      {
        return Spatial::SpatialDataType::Scalar;
      }

      [[nodiscard]] Spatial::IPolyhedralSurface *polyhedralSurface()
        const override
      {
        return m_surface.get();
      }

      [[nodiscard]] HydroCouple::IDimension *patchDimension() const override
      {
        return m_attachedTo == Spatial::MeshDataObjectType::Cell
                 ? const_cast<HydroCouple::SDK::Dimension *>(&m_entityDimension)
                 : nullptr;
      }

      [[nodiscard]] HydroCouple::IDimension *edgeDimension() const override
      {
        return m_attachedTo == Spatial::MeshDataObjectType::Edge
                 ? const_cast<HydroCouple::SDK::Dimension *>(&m_entityDimension)
                 : nullptr;
      }

      [[nodiscard]] HydroCouple::IDimension *vertexDimension() const override
      {
        return m_attachedTo == Spatial::MeshDataObjectType::Vertex
                 ? const_cast<HydroCouple::SDK::Dimension *>(&m_entityDimension)
                 : nullptr;
      }

      // ── ITimeSeriesComponentDataItem ─────────────────────────────────
      [[nodiscard]] const HydroCouple::Temporal::IDateTime *time(
        int64_t timeIndex) const override
      {
        return timeIndex >= 0
                   && timeIndex < static_cast<int64_t>(m_times.size())
                 ? m_times[static_cast<size_t>(timeIndex)].get()
                 : nullptr;
      }

      [[nodiscard]] int64_t timeCount() const override
      {
        return static_cast<int64_t>(m_times.size());
      }

      [[nodiscard]] HydroCouple::IDimension *timeDimension() const override
      {
        return const_cast<HydroCouple::SDK::Dimension *>(&m_timeDimension);
      }

      [[nodiscard]] std::span<const double> times() const override
      {
        return {m_julianDays.data(), m_julianDays.size()};
      }

      [[nodiscard]] HydroCouple::Temporal::ITimeSpan *timeSpan()
        const override
      {
        return const_cast<HydroCouple::SDK::Temporal::TimeSpan *>(&m_span);
      }

      // ── IComponentDataItem ───────────────────────────────────────────
      [[nodiscard]] std::vector<int64_t> shape() const override
      {
        return Store::storageShape();
      }

      [[nodiscard]] HydroCouple::DataKind dataKind() const override
      {
        return Store::storageKind();
      }

      [[nodiscard]] bool getValuesInto(
        const HydroCouple::BufferDescriptor &destination,
        std::span<const int64_t> start, std::span<const int64_t> count,
        std::string *message = nullptr) const override
      {
        return Store::getSlab(destination, start, count, message);
      }

      [[nodiscard]] bool setValuesFrom(
        const HydroCouple::BufferDescriptor &source,
        std::span<const int64_t> start, std::span<const int64_t> count,
        std::string *message = nullptr) override
      {
        return Store::setSlab(source, start, count, message);
      }

    private:
      std::unique_ptr<HydroCouple::SDK::Spatial::PolyhedralSurfaceAdapter>
        m_surface;

      Spatial::MeshDataObjectType m_attachedTo;

      HydroCouple::SDK::Dimension m_timeDimension;
      HydroCouple::SDK::Dimension m_entityDimension;

      std::vector<std::unique_ptr<HydroCouple::SDK::Temporal::TimeData>> m_times;
      std::vector<double> m_julianDays;
      HydroCouple::SDK::Temporal::TimeSpan m_span;
  };

  class StubGrid : public HydroCouple::SDK::Identity,
                   public virtual Spatial::IRegularGrid2D
  {
    public:
      StubGrid(int xNodes, int yNodes, double spacing,
               Spatial::ISpatialReferenceSystem *crs)
        : HydroCouple::SDK::Identity("grid", "Grid"), m_xNodes(xNodes),
          m_yNodes(yNodes), m_spacing(spacing), m_crs(crs)
      {
        m_active.assign(
          static_cast<size_t>((xNodes - 1) * (yNodes - 1)), 1);
      }

      //! Marks one cell inactive — a hole in the model's domain.
      void deactivate(int x, int y)
      {
        m_active[static_cast<size_t>(y) * (m_xNodes - 1) + x] = 0;
      }

      [[nodiscard]] Spatial::ISpatialReferenceSystem *spatialReferenceSystem()
        const override
      {
        return m_crs;
      }

      [[nodiscard]] Spatial::RegularGridType gridType() const override
      {
        return Spatial::RegularGridType::Rectilinear;
      }

      [[nodiscard]] int numXNodes() const override { return m_xNodes; }
      [[nodiscard]] int numYNodes() const override { return m_yNodes; }

      [[nodiscard]] double xNodeLocation(int xNodeIndex, int) const override
      {
        return xNodeIndex * m_spacing;
      }

      [[nodiscard]] double yNodeLocation(int, int yNodeIndex) const override
      {
        return yNodeIndex * m_spacing;
      }

      [[nodiscard]] std::span<const double> nodeXs() const override
      {
        return {};
      }

      [[nodiscard]] std::span<const double> nodeYs() const override
      {
        return {};
      }

      [[nodiscard]] bool isActive(int xCellIndex, int yCellIndex) const override
      {
        return m_active[static_cast<size_t>(yCellIndex) * (m_xNodes - 1)
                        + xCellIndex]
               != 0;
      }

      [[nodiscard]] std::span<const uint8_t> activeCells() const override
      {
        return {m_active.data(), m_active.size()};
      }

    private:
      int m_xNodes = 0;
      int m_yNodes = 0;
      double m_spacing = 1.0;
      Spatial::ISpatialReferenceSystem *m_crs = nullptr;
      std::vector<uint8_t> m_active;
  };

  //! A regular-grid data item carrying one value per cell.
  class StubGridItem
    : public HydroCouple::SDK::AbstractComponentDataItem,
      public HydroCouple::SDK::ComponentDataItem1D<double>,
      public virtual Spatial::IRegularGrid2DComponentDataItem
  {
      using Store = HydroCouple::SDK::ComponentDataItem1D<double>;

    public:
      StubGridItem(std::string_view id, StubGrid *grid)
        : AbstractComponentDataItem(id, {&m_cellDimension}, nullptr, nullptr),
          Store((grid->numXNodes() - 1) * (grid->numYNodes() - 1), 0.0),
          m_grid(grid), m_cellDimension("cells", "Cell dimension"),
          m_xDimension("x", "X"), m_yDimension("y", "Y"),
          m_edgeDimension("edges", "Edges"),
          m_vertexDimension("vertices", "Vertices")
      {
      }

      void setValue(int cell, double value)
      {
        Store::rawData()[static_cast<size_t>(cell)] = value;
      }

      [[nodiscard]] std::vector<int64_t> shape() const override
      {
        return Store::storageShape();
      }

      [[nodiscard]] HydroCouple::DataKind dataKind() const override
      {
        return Store::storageKind();
      }

      [[nodiscard]] bool getValuesInto(
        const HydroCouple::BufferDescriptor &destination,
        std::span<const int64_t> start, std::span<const int64_t> count,
        std::string *message = nullptr) const override
      {
        return Store::getSlab(destination, start, count, message);
      }

      [[nodiscard]] bool setValuesFrom(
        const HydroCouple::BufferDescriptor &source,
        std::span<const int64_t> start, std::span<const int64_t> count,
        std::string *message = nullptr) override
      {
        return Store::setSlab(source, start, count, message);
      }

      [[nodiscard]] Spatial::IRegularGrid2D *grid() const override
      {
        return m_grid;
      }

      [[nodiscard]] Spatial::MeshDataObjectType meshDataObjectType()
        const override
      {
        return Spatial::MeshDataObjectType::Cell;
      }

      [[nodiscard]] HydroCouple::IDimension *xCellDimension() const override
      {
        return const_cast<HydroCouple::SDK::Dimension *>(&m_xDimension);
      }

      [[nodiscard]] HydroCouple::IDimension *yCellDimension() const override
      {
        return const_cast<HydroCouple::SDK::Dimension *>(&m_yDimension);
      }

      [[nodiscard]] HydroCouple::IDimension *cellEdgeDimension() const override
      {
        return const_cast<HydroCouple::SDK::Dimension *>(&m_edgeDimension);
      }

      [[nodiscard]] HydroCouple::IDimension *cellVertexDimension()
        const override
      {
        return const_cast<HydroCouple::SDK::Dimension *>(&m_vertexDimension);
      }

    private:
      StubGrid *m_grid = nullptr;
      HydroCouple::SDK::Dimension m_cellDimension;
      HydroCouple::SDK::Dimension m_xDimension;
      HydroCouple::SDK::Dimension m_yDimension;
      HydroCouple::SDK::Dimension m_edgeDimension;
      HydroCouple::SDK::Dimension m_vertexDimension;
  };

  // ── A raster data item ────────────────────────────────────────────────────
  //
  // Written against the published interface rather than any implementation.
  // The SDK's own RasterComponentDataItem does not implement
  // IRasterComponentDataItem -- it is a single band with shape {y, x} and a
  // GDAL-backed Raster that is not an IRaster -- so nothing shipped can be
  // drawn by RasterDataItemLayer yet. That is a gap in the SDK, not in the
  // layer, and it is why this stub exists.

  class StubRaster;

  class StubRasterBand : public HydroCouple::SDK::Identity,
                         public virtual Spatial::IRasterBand
  {
    public:
      StubRasterBand(StubRaster *raster, int width, int height)
        : HydroCouple::SDK::Identity("band", "Band"), m_raster(raster),
          m_width(width), m_height(height)
      {
      }

      [[nodiscard]] int xSize() const override { return m_width; }
      [[nodiscard]] int ySize() const override { return m_height; }

      [[nodiscard]] Spatial::IRaster *raster() const override;

      [[nodiscard]] Spatial::IRaster::RasterDataType dataType() const override
      {
        return Spatial::IRaster::RasterDataType::Float64;
      }

      void read(int, int, int, int, void *) const override {}
      void write(int, int, int, int, const void *) override {}

      [[nodiscard]] double noData() const override
      {
        return std::numeric_limits<double>::quiet_NaN();
      }

    private:
      StubRaster *m_raster = nullptr;
      int m_width = 0;
      int m_height = 0;
  };

  class StubRaster : public HydroCouple::SDK::Identity,
                     public virtual Spatial::IRaster
  {
    public:
      StubRaster(int width, int height, double originX, double originY,
                 double cellSize, Spatial::ISpatialReferenceSystem *crs,
                 int bands = 1)
        : HydroCouple::SDK::Identity("raster", "Raster"), m_width(width),
          m_height(height), m_crs(crs)
      {
        // North up: the origin is the upper-left corner and rows run down,
        // which is what the negative fifth coefficient says.
        m_transform = {originX, cellSize, 0.0, originY, 0.0, -cellSize};

        for (int band = 0; band < bands; ++band)
        {
          m_bands.push_back(
            std::make_unique<StubRasterBand>(this, width, height));
        }
      }

      [[nodiscard]] int xSize() const override { return m_width; }
      [[nodiscard]] int ySize() const override { return m_height; }

      [[nodiscard]] int rasterBandCount() const override
      {
        return static_cast<int>(m_bands.size());
      }

      void addRasterBand(RasterDataType) override
      {
        m_bands.push_back(
          std::make_unique<StubRasterBand>(this, m_width, m_height));
      }

      [[nodiscard]] Spatial::ISpatialReferenceSystem *spatialReferenceSystem()
        const override
      {
        return m_crs;
      }

      void geoTransformation(double *transformationMatrix) override
      {
        std::copy(m_transform.begin(), m_transform.end(), transformationMatrix);
      }

      [[nodiscard]] Spatial::IRasterBand *getRasterBand(
        int bandIndex) const override
      {
        return bandIndex >= 0 && bandIndex < static_cast<int>(m_bands.size())
                 ? m_bands[static_cast<size_t>(bandIndex)].get()
                 : nullptr;
      }

    private:
      int m_width = 0;
      int m_height = 0;
      Spatial::ISpatialReferenceSystem *m_crs = nullptr;
      std::array<double, 6> m_transform{};
      std::vector<std::unique_ptr<StubRasterBand>> m_bands;
  };

  inline Spatial::IRaster *StubRasterBand::raster() const
  {
    return m_raster;
  }

  class StubRasterItem : public HydroCouple::SDK::AbstractComponentDataItem,
                         public virtual Spatial::IRasterComponentDataItem
  {
    public:
      StubRasterItem(std::string_view id, StubRaster *raster)
        : AbstractComponentDataItem(id, {&m_bandDimension, &m_yDimension,
                                         &m_xDimension},
                                    nullptr, nullptr),
          m_raster(raster), m_bandDimension("bands", "Bands"),
          m_yDimension("y", "Rows"), m_xDimension("x", "Columns")
      {
        m_values.assign(static_cast<size_t>(raster->rasterBandCount())
                          * raster->ySize() * raster->xSize(),
                        0.0);
      }

      void setValue(int band, int row, int column, double value)
      {
        m_values[index(band, row, column)] = value;
      }

      /*!
       * \brief Reports {y, x} rather than {band, y, x}.
       *
       * Which is what HydroCoupleSDK's own RasterComponentDataItem does: it
       * is one band, and its shape has no band axis. An item shaped like
       * that must be refused rather than read as though its rows were bands.
       */
      void reportShapeWithoutABandAxis() { m_twoDimensional = true; }

      [[nodiscard]] std::vector<int64_t> shape() const override
      {
        if (m_twoDimensional)
        {
          return {m_raster->ySize(), m_raster->xSize()};
        }

        return {m_raster->rasterBandCount(), m_raster->ySize(),
                m_raster->xSize()};
      }

      [[nodiscard]] HydroCouple::DataKind dataKind() const override
      {
        return HydroCouple::DataKind::Float64;
      }

      [[nodiscard]] bool getValuesInto(
        const HydroCouple::BufferDescriptor &destination,
        std::span<const int64_t> start, std::span<const int64_t> count,
        std::string *message = nullptr) const override
      {
        if (start.size() != 3 || count.size() != 3)
        {
          if (message)
          {
            *message = "a raster is read band, row and column";
          }

          return false;
        }

        // Refused rather than wrapped into the next band's rows. Without
        // this a reader that muddles the band and row axes reads plausible
        // numbers off the end of one band and no test can see it.
        const std::vector<int64_t> extents = shape();

        for (size_t axis = 0; axis < 3; ++axis)
        {
          if (start[axis] < 0 || count[axis] < 0
              || start[axis] + count[axis] > extents[axis])
          {
            if (message)
            {
              *message = "that block is not inside this raster";
            }

            return false;
          }
        }

        auto *out = static_cast<double *>(destination.data);
        int64_t written = 0;

        for (int64_t band = start[0]; band < start[0] + count[0]; ++band)
        {
          for (int64_t row = start[1]; row < start[1] + count[1]; ++row)
          {
            for (int64_t column = start[2]; column < start[2] + count[2];
                 ++column)
            {
              out[written++] = m_values[index(static_cast<int>(band),
                                              static_cast<int>(row),
                                              static_cast<int>(column))];
            }
          }
        }

        return true;
      }

      [[nodiscard]] bool setValuesFrom(const HydroCouple::BufferDescriptor &,
                                       std::span<const int64_t>,
                                       std::span<const int64_t>,
                                       std::string * = nullptr) override
      {
        return false;
      }

      [[nodiscard]] Spatial::IRaster *raster() const override
      {
        return m_raster;
      }

      [[nodiscard]] HydroCouple::IDimension *xDimension() const override
      {
        return const_cast<HydroCouple::SDK::Dimension *>(&m_xDimension);
      }

      [[nodiscard]] HydroCouple::IDimension *yDimension() const override
      {
        return const_cast<HydroCouple::SDK::Dimension *>(&m_yDimension);
      }

      [[nodiscard]] HydroCouple::IDimension *bandDimension() const override
      {
        return const_cast<HydroCouple::SDK::Dimension *>(&m_bandDimension);
      }

    private:
      [[nodiscard]] size_t index(int band, int row, int column) const
      {
        return (static_cast<size_t>(band) * m_raster->ySize() + row)
                 * m_raster->xSize()
               + column;
      }

      StubRaster *m_raster = nullptr;
      bool m_twoDimensional = false;
      HydroCouple::SDK::Dimension m_bandDimension;
      HydroCouple::SDK::Dimension m_yDimension;
      HydroCouple::SDK::Dimension m_xDimension;
      std::vector<double> m_values;
  };

} // namespace HydroCouple::Composer::Testing

#endif // HYDROCOUPLECOMPOSER_TESTS_SPATIALSTUBS_H
