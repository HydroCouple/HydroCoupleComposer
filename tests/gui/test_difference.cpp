/*!
 * \file   test_difference.cpp
 * \brief  D4a verification — one variable, two runs, drawn as the difference.
 *
 * The gates are on signed numbers per feature per instant, because every way
 * this can be wrong still draws a map: subtracting the wrong way round gives
 * a plausible picture with every colour inverted, pairing by level index
 * instead of by instant gives a plausible picture of the wrong hours, and
 * differencing two meshes that merely have the same number of cells gives a
 * plausible picture of nothing at all.
 *
 * The self-comparison test is the plan's own gate: a run against itself is
 * zero everywhere, exactly, and anything that is nearly zero is a bug.
 */

#include "core/composerapplication.h"
#include "layers/differencelayer.h"
#include "map/layerstackmodel.h"
#include "render/classification.h"
#include "render/layerstyle.h"
#include "results/timecontroller.h"
#include "spatialstubs.h"

#include <gtest/gtest.h>

#include <cmath>
#include <memory>

using namespace HydroCouple::Composer;
namespace Testing = HydroCouple::Composer::Testing;
namespace Spatial = HydroCouple::Spatial;

namespace
{
  //! J2000, so the fixtures sit where real Julian days do.
  constexpr double kEpoch = 2451545.0;

  /*!
   * \brief A geometry item with a second, non-temporal axis.
   *
   * Shaped {layers, geometries} and recorded at no instant at all — a set of
   * initial conditions, or a steady field over a water column. It exists to
   * put a *slice* choice in front of the comparison: with no time axis there
   * is no level to ask for, and every other axis is meant to keep the
   * convention the map already uses, which is its last index. A run compared
   * against this must read the same slice the map draws.
   */
  class LayeredStaticItem
    : public HydroCouple::SDK::AbstractComponentDataItem,
      public HydroCouple::SDK::ComponentDataItem2D<double>,
      public virtual Spatial::IGeometryComponentDataItem
  {
      using Store = HydroCouple::SDK::ComponentDataItem2D<double>;

    public:
      LayeredStaticItem(std::string_view id, int layers,
                        std::vector<Spatial::IGeometry *> geometries)
        : AbstractComponentDataItem(id, {&m_layerDimension, &m_dimension},
                                    nullptr, nullptr),
          Store(layers, static_cast<int>(geometries.size()), 0.0),
          m_geometries(std::move(geometries)),
          m_layerDimension("layers", "Layer dimension"),
          m_dimension("geometries", "Geometry dimension")
      {
      }

      void setValue(int layer, int geometry, double value)
      {
        Store::rawData()[static_cast<size_t>(layer) * m_geometries.size()
                         + static_cast<size_t>(geometry)] = value;
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
      HydroCouple::SDK::Dimension m_layerDimension;
      HydroCouple::SDK::Dimension m_dimension;
      HydroCouple::SDK::Spatial::EnvelopeAdapter m_envelope;
  };

  class DifferenceTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_difference";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      void SetUp() override
      {
        m_crs = std::make_unique<Testing::StubCrs>(4326);
        m_a = Testing::makePoint(0.0, 0.0, 0, m_crs.get());
        m_b = Testing::makePoint(1.0, 1.0, 1, m_crs.get());

        // A third point somewhere else entirely, for the run that was not
        // recorded on the same ground.
        m_far = Testing::makePoint(50.0, 50.0, 1, m_crs.get());
      }

      [[nodiscard]] std::vector<Spatial::IGeometry *> shared() const
      {
        return {static_cast<Spatial::IGeometry *>(m_a.get()),
                static_cast<Spatial::IGeometry *>(m_b.get())};
      }

      /*!
       * \brief An item of \a steps levels, \a spacing days apart.
       *
       * Values are `offset + step * 100 + geometry`, so reading the wrong
       * level, the wrong entity or the wrong run produces a different number
       * rather than a plausible one.
       */
      std::unique_ptr<Testing::StubTimeGeometryItem> makeItem(
        const char *id, int steps, double first, double spacing,
        double offset, std::vector<Spatial::IGeometry *> geometries = {})
      {
        if (geometries.empty())
        {
          geometries = shared();
        }

        const int entities = static_cast<int>(geometries.size());

        auto item = std::make_unique<Testing::StubTimeGeometryItem>(
          id, steps, std::move(geometries), first, spacing);

        for (int step = 0; step < steps; ++step)
        {
          for (int entity = 0; entity < entities; ++entity)
          {
            item->setValue(step, entity, offset + step * 100.0 + entity);
          }
        }

        return item;
      }

      std::unique_ptr<Testing::StubCrs> m_crs;
      std::unique_ptr<Testing::StubPoint> m_a;
      std::unique_ptr<Testing::StubPoint> m_b;
      std::unique_ptr<Testing::StubPoint> m_far;

      static ComposerApplication *s_app;
  };

  ComposerApplication *DifferenceTest::s_app = nullptr;

  //! The value attribute of \a layer, feature by feature.
  QVector<QVariant> shown(const DifferenceLayer &layer)
  {
    QVector<QVariant> values;

    for (int feature = 0; feature < layer.featureCount(); ++feature)
    {
      values.append(layer.attributeValue(feature, layer.valueAttribute()));
    }

    return values;
  }
}

// ── the difference itself ───────────────────────────────────────────────────

TEST_F(DifferenceTest, ARunComparedAgainstItselfIsZeroEverywhere)
{
  // The plan's own gate. Exactly zero, not nearly: a comparison that read
  // one of the two through a different path — a different level, a different
  // axis, a rounded instant — lands near zero and looks entirely correct.
  const std::unique_ptr<Testing::StubTimeGeometryItem> run =
    makeItem("run", 4, kEpoch, 1.0, 0.0);

  QString message;
  const std::unique_ptr<DifferenceLayer> layer =
    DifferenceLayer::create(run.get(), run.get(), message);

  ASSERT_NE(layer, nullptr) << message.toStdString();
  ASSERT_EQ(layer->featureCount(), 2);

  for (int level = 0; level < layer->timeCount(); ++level)
  {
    ASSERT_TRUE(layer->setTimeIndex(level));

    for (const QVariant &value : shown(*layer))
    {
      ASSERT_TRUE(value.isValid());
      EXPECT_DOUBLE_EQ(value.toDouble(), 0.0)
        << "at level " << level;
    }
  }
}

TEST_F(DifferenceTest, TheDifferenceIsTheBaseMinusTheOtherAndNotTheReverse)
{
  // The base is 7 higher everywhere, so every difference is +7. Reversed, it
  // would be -7 — the same map with every colour on the other side of the
  // ramp, which is exactly as readable and exactly wrong.
  const std::unique_ptr<Testing::StubTimeGeometryItem> base =
    makeItem("base", 3, kEpoch, 1.0, 7.0);
  const std::unique_ptr<Testing::StubTimeGeometryItem> other =
    makeItem("other", 3, kEpoch, 1.0, 0.0);

  QString message;
  const std::unique_ptr<DifferenceLayer> layer =
    DifferenceLayer::create(base.get(), other.get(), message);

  ASSERT_NE(layer, nullptr) << message.toStdString();

  ASSERT_TRUE(layer->setTimeIndex(1));

  const QVector<QVariant> values = shown(*layer);
  ASSERT_EQ(values.size(), 2);
  EXPECT_DOUBLE_EQ(values.at(0).toDouble(), 7.0);
  EXPECT_DOUBLE_EQ(values.at(1).toDouble(), 7.0);
}

TEST_F(DifferenceTest, EachEntityIsSubtractedFromItsOwnCounterpart)
{
  // The two entities differ by different amounts, so a comparison that
  // subtracted entity zero from everything — or reversed the entity order —
  // gives numbers of the right magnitude in the wrong places.
  const std::unique_ptr<Testing::StubTimeGeometryItem> base =
    makeItem("base", 2, kEpoch, 1.0, 0.0);

  auto other = std::make_unique<Testing::StubTimeGeometryItem>(
    "other", 2, shared(), kEpoch, 1.0);

  for (int step = 0; step < 2; ++step)
  {
    other->setValue(step, 0, step * 100.0 + 0.0 - 1.0);
    other->setValue(step, 1, step * 100.0 + 1.0 - 5.0);
  }

  QString message;
  const std::unique_ptr<DifferenceLayer> layer =
    DifferenceLayer::create(base.get(), other.get(), message);

  ASSERT_NE(layer, nullptr) << message.toStdString();
  ASSERT_TRUE(layer->setTimeIndex(0));

  const QVector<QVariant> values = shown(*layer);
  ASSERT_EQ(values.size(), 2);
  EXPECT_DOUBLE_EQ(values.at(0).toDouble(), 1.0);
  EXPECT_DOUBLE_EQ(values.at(1).toDouble(), 5.0);
}

TEST_F(DifferenceTest, TwoRunsOnDifferentClocksAreMatchedByInstant)
{
  // The base reports daily, the other twice a day. Level 2 of the base is
  // day 2, which is level 4 of the other — pairing by level index would
  // subtract the other run's day 1 from the base's day 2 and produce a map
  // of the wrong hours, with no sign that anything is wrong.
  const std::unique_ptr<Testing::StubTimeGeometryItem> base =
    makeItem("base", 3, kEpoch, 1.0, 0.0);
  const std::unique_ptr<Testing::StubTimeGeometryItem> other =
    makeItem("other", 5, kEpoch, 0.5, 0.0);

  QString message;
  const std::unique_ptr<DifferenceLayer> layer =
    DifferenceLayer::create(base.get(), other.get(), message);

  ASSERT_NE(layer, nullptr) << message.toStdString();
  ASSERT_TRUE(layer->setTimeIndex(2));

  // base level 2 = 200 + entity; other level 4 = 400 + entity.
  const QVector<QVariant> values = shown(*layer);
  ASSERT_EQ(values.size(), 2);
  EXPECT_DOUBLE_EQ(values.at(0).toDouble(), -200.0);
  EXPECT_DOUBLE_EQ(values.at(1).toDouble(), -200.0);
}

TEST_F(DifferenceTest, ASteadyBaselineIsSubtractedFromEveryInstant)
{
  // A run with no time axis is not a failure to compare against: it is a
  // baseline, and it answers with its only level at every instant of the
  // other. The alternative — refusing, or reading level -1 as level 0 of a
  // series it does not have — loses a comparison people actually make.
  const std::unique_ptr<Testing::StubTimeGeometryItem> base =
    makeItem("base", 3, kEpoch, 1.0, 0.0);

  auto steady = std::make_unique<Testing::StubGeometryItem>(
    "steady", shared());
  steady->setValue(0, 10.0);
  steady->setValue(1, 20.0);

  QString message;
  const std::unique_ptr<DifferenceLayer> layer =
    DifferenceLayer::create(base.get(), steady.get(), message);

  ASSERT_NE(layer, nullptr) << message.toStdString();

  ASSERT_TRUE(layer->setTimeIndex(2));
  const QVector<QVariant> late = shown(*layer);
  ASSERT_EQ(late.size(), 2);
  EXPECT_DOUBLE_EQ(late.at(0).toDouble(), 200.0 - 10.0);
  EXPECT_DOUBLE_EQ(late.at(1).toDouble(), 201.0 - 20.0);

  ASSERT_TRUE(layer->setTimeIndex(0));
  const QVector<QVariant> early = shown(*layer);
  EXPECT_DOUBLE_EQ(early.at(0).toDouble(), 0.0 - 10.0);
}

// ── what is refused ─────────────────────────────────────────────────────────

TEST_F(DifferenceTest, TwoRunsOfDifferentSizesAreRefused)
{
  const std::unique_ptr<Testing::StubTimeGeometryItem> base =
    makeItem("base", 2, kEpoch, 1.0, 0.0);

  const std::unique_ptr<Testing::StubTimeGeometryItem> other = makeItem(
    "other", 2, kEpoch, 1.0, 0.0,
    {static_cast<Spatial::IGeometry *>(m_a.get())});

  QString message;
  EXPECT_EQ(DifferenceLayer::create(base.get(), other.get(), message),
            nullptr);
  EXPECT_TRUE(message.contains(QStringLiteral("2 and 1")))
    << message.toStdString();
}

TEST_F(DifferenceTest, TwoRunsThatMerelyMatchInCountAreRefused)
{
  // The one that matters. Two runs over different meshes of the same size
  // difference perfectly happily and mean nothing at all — and a check on
  // the count alone waves it straight through.
  const std::unique_ptr<Testing::StubTimeGeometryItem> base =
    makeItem("base", 2, kEpoch, 1.0, 0.0);

  const std::unique_ptr<Testing::StubTimeGeometryItem> other = makeItem(
    "other", 2, kEpoch, 1.0, 0.0,
    {static_cast<Spatial::IGeometry *>(m_a.get()),
     static_cast<Spatial::IGeometry *>(m_far.get())});

  QString message;
  EXPECT_EQ(DifferenceLayer::create(base.get(), other.get(), message),
            nullptr);
  EXPECT_TRUE(message.contains(QStringLiteral("not the same ground")))
    << message.toStdString();
  EXPECT_TRUE(message.contains(QStringLiteral("feature 1")))
    << message.toStdString();
}

TEST_F(DifferenceTest, AnItemWithNoGeometryIsRefused)
{
  const std::unique_ptr<Testing::StubTimeGeometryItem> base =
    makeItem("base", 2, kEpoch, 1.0, 0.0);

  QString message;
  EXPECT_EQ(DifferenceLayer::create(base.get(), nullptr, message), nullptr);
  EXPECT_FALSE(message.isEmpty());
}

// ── the difference as a layer ───────────────────────────────────────────────

TEST_F(DifferenceTest, TheComparisonStepsWithTheClockLikeAnyOtherLayer)
{
  // Through the controller, not by calling setTimeIndex: what is under test
  // is that a comparison is driven by the same clock as the runs it came
  // from, which is the point of it being a layer rather than a report.
  const std::unique_ptr<Testing::StubTimeGeometryItem> base =
    makeItem("base", 3, kEpoch, 1.0, 7.0);
  const std::unique_ptr<Testing::StubTimeGeometryItem> other =
    makeItem("other", 3, kEpoch, 1.0, 0.0);

  QString message;
  std::unique_ptr<DifferenceLayer> layer =
    DifferenceLayer::create(base.get(), other.get(), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  DifferenceLayer *kept = layer.get();

  LayerStackModel stack;
  stack.addLayer(layer.release());

  TimeController clock;
  clock.setModel(&stack);

  ASSERT_TRUE(clock.hasTime());
  EXPECT_DOUBLE_EQ(clock.first(), kEpoch);
  EXPECT_DOUBLE_EQ(clock.last(), kEpoch + 2.0);

  clock.setCurrent(kEpoch);
  EXPECT_EQ(kept->timeIndex(), 0);

  clock.setCurrent(kEpoch + 2.0);
  EXPECT_EQ(kept->timeIndex(), 2);
}

TEST_F(DifferenceTest, TheBreaksAreTakenFromEveryInstantNotTheOneOnScreen)
{
  // The base grows away from the other as the run goes on, so the range of
  // differences is wider than any single instant's. A comparison classified
  // from the level on screen recomputes its colours every step, and the one
  // thing a difference map is read for is which step disagreed most.
  const std::unique_ptr<Testing::StubTimeGeometryItem> base =
    makeItem("base", 3, kEpoch, 1.0, 0.0);

  auto other = std::make_unique<Testing::StubTimeGeometryItem>(
    "other", 3, shared(), kEpoch, 1.0);

  for (int step = 0; step < 3; ++step)
  {
    for (int entity = 0; entity < 2; ++entity)
    {
      // Falls behind by ten a step: differences run 0, 10, 20.
      other->setValue(step, entity, step * 100.0 + entity - step * 10.0);
    }
  }

  QString message;
  const std::unique_ptr<DifferenceLayer> layer =
    DifferenceLayer::create(base.get(), other.get(), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  ASSERT_TRUE(layer->setTimeIndex(0));

  const QVector<double> pooled = layer->numericValues(layer->valueAttribute());

  // Six values, not two: every entity at every instant.
  ASSERT_EQ(pooled.size(), 6);
  EXPECT_DOUBLE_EQ(*std::min_element(pooled.begin(), pooled.end()), 0.0);
  EXPECT_DOUBLE_EQ(*std::max_element(pooled.begin(), pooled.end()), 20.0);
}

TEST_F(DifferenceTest, TheLayerSaysWhichVariableItIsDifferencing)
{
  // The base's own caption, not the layer's name: a legend on a comparison
  // that says only "Δ value" has thrown away the one label the component
  // supplied.
  auto base = std::make_unique<Testing::StubTimeGeometryItem>(
    "base", 2, shared(), kEpoch, 1.0, "Water depth (m)");
  auto other = std::make_unique<Testing::StubTimeGeometryItem>(
    "other", 2, shared(), kEpoch, 1.0, "Water depth (m)");

  QString message;
  const std::unique_ptr<DifferenceLayer> layer =
    DifferenceLayer::create(base.get(), other.get(), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  bool named = false;

  for (const AttributeField &field : layer->attributeFields())
  {
    if (field.name == layer->valueAttribute())
    {
      EXPECT_TRUE(field.displayName.contains(QStringLiteral("Water depth")))
        << field.displayName.toStdString();
      named = true;
    }
  }

  EXPECT_TRUE(named) << "the difference has no value field at all";
}

TEST_F(DifferenceTest, OneFeaturesDifferenceSeriesIsTheTwoRunsSubtracted)
{
  // The plot's read, and it must agree with the map's: the same instants
  // matched the same way. A series that paired by level while the map paired
  // by instant would disagree with the map beside it.
  const std::unique_ptr<Testing::StubTimeGeometryItem> base =
    makeItem("base", 3, kEpoch, 1.0, 0.0);
  const std::unique_ptr<Testing::StubTimeGeometryItem> other =
    makeItem("other", 5, kEpoch, 0.5, 0.0);

  QString message;
  const std::unique_ptr<DifferenceLayer> layer =
    DifferenceLayer::create(base.get(), other.get(), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  QVector<double> series;
  ASSERT_TRUE(layer->valuesOverTime(1, series, message))
    << message.toStdString();

  // Base days 0, 1, 2 against the other's levels 0, 2, 4.
  ASSERT_EQ(series.size(), 3);
  EXPECT_DOUBLE_EQ(series.at(0), 0.0);
  EXPECT_DOUBLE_EQ(series.at(1), 100.0 - 200.0);
  EXPECT_DOUBLE_EQ(series.at(2), 200.0 - 400.0);

  const QVector<double> instants = layer->times();
  ASSERT_EQ(instants.size(), 3);
  EXPECT_DOUBLE_EQ(instants.first(), kEpoch);
  EXPECT_DOUBLE_EQ(instants.last(), kEpoch + 2.0);
}

TEST_F(DifferenceTest, ASteadyOtherRunIsReadOnTheSliceTheMapDraws)
{
  // A baseline with a second axis and no time axis at all. There is no level
  // to ask it for, so every other axis keeps the convention the map uses —
  // its last index. Asking for level zero instead is a comparison against
  // the wrong slice of a perfectly valid field, and every number it produces
  // is the right size.
  const std::unique_ptr<Testing::StubTimeGeometryItem> base =
    makeItem("base", 2, kEpoch, 1.0, 0.0);

  auto steady = std::make_unique<LayeredStaticItem>("steady", 3, shared());

  for (int entity = 0; entity < 2; ++entity)
  {
    steady->setValue(0, entity, 1000.0);
    steady->setValue(1, entity, 2000.0);
    steady->setValue(2, entity, 3.0);
  }

  QString message;
  const std::unique_ptr<DifferenceLayer> layer =
    DifferenceLayer::create(base.get(), steady.get(), message);

  ASSERT_NE(layer, nullptr) << message.toStdString();
  ASSERT_TRUE(layer->setTimeIndex(1));

  // Base level 1 is 100 + entity; the last layer of the baseline is 3.
  const QVector<QVariant> values = shown(*layer);
  ASSERT_EQ(values.size(), 2);
  EXPECT_DOUBLE_EQ(values.at(0).toDouble(), 100.0 - 3.0);
  EXPECT_DOUBLE_EQ(values.at(1).toDouble(), 101.0 - 3.0);
}
