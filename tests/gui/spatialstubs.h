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
#include "hydrocouplesdk/spatial/geometryadapters.h"
#include "hydrocouplesdk/spatial/meshadapters.h"

#include <ogr_geometry.h>
#include <ogr_spatialref.h>

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
                           double spacingDays = 1.0)
        : AbstractComponentDataItem(id, {&m_timeDimension, &m_dimension},
                                    nullptr, nullptr),
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

} // namespace HydroCouple::Composer::Testing

#endif // HYDROCOUPLECOMPOSER_TESTS_SPATIALSTUBS_H
