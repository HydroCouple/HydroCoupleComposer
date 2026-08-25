/*!
 * \file   test_styling.cpp
 * \brief  Phase C1c verification — ramps, classification and layer styles.
 *
 * Classification is checked against breaks worked out by hand rather than
 * against whatever the code produces: every method here returns *some*
 * plausible-looking set of numbers, so a test that only asserted "five
 * classes came back, ascending" would pass for all four methods swapped
 * around.
 */

#include "render/attributeprovider.h"
#include "render/classification.h"
#include "render/colorramp.h"
#include "render/layerstyle.h"

#include <gtest/gtest.h>

#include <QVariant>

#include <cmath>
#include <limits>

using namespace HydroCouple::Composer;

namespace
{
  //! A provider backed by a plain table, so the styling tests need no layer.
  class TableProvider : public IAttributeProvider
  {
    public:
      void add(const QVariant &numeric, const QVariant &category)
      {
        m_numeric.append(numeric);
        m_category.append(category);
      }

      [[nodiscard]] QVector<AttributeField> attributeFields() const override
      {
        AttributeField value;
        value.name = QStringLiteral("value");

        AttributeField kind;
        kind.name = QStringLiteral("kind");
        kind.type = QMetaType::QString;

        return {value, kind};
      }

      [[nodiscard]] int featureCount() const override
      {
        return static_cast<int>(m_numeric.size());
      }

      [[nodiscard]] QVariant attributeValue(int feature,
                                            const QString &field) const override
      {
        if (feature < 0 || feature >= m_numeric.size())
        {
          return {};
        }

        if (field == QStringLiteral("value"))
        {
          return m_numeric.at(feature);
        }

        return field == QStringLiteral("kind") ? m_category.at(feature)
                                               : QVariant();
      }

    private:
      QVector<QVariant> m_numeric;
      QVector<QVariant> m_category;
  };

  QVector<double> ramp(int count, double from, double step)
  {
    QVector<double> values;

    for (int i = 0; i < count; ++i)
    {
      values.append(from + step * i);
    }

    return values;
  }
}

// ── Colour ramps ────────────────────────────────────────────────────────────

TEST(ColorRampTest, InterpolatesBetweenStops)
{
  const ColorRamp ramp({{0.0, QColor(Qt::black)}, {1.0, QColor(Qt::white)}});

  EXPECT_EQ(ramp.colorAt(0.0), QColor(Qt::black));
  EXPECT_EQ(ramp.colorAt(1.0), QColor(Qt::white));

  const QColor middle = ramp.colorAt(0.5);

  EXPECT_NEAR(middle.redF(), 0.5, 0.01);
  EXPECT_NEAR(middle.greenF(), 0.5, 0.01);
  EXPECT_NEAR(middle.blueF(), 0.5, 0.01);
}

TEST(ColorRampTest, ClampsBeyondTheEnds)
{
  const ColorRamp ramp({{0.0, QColor(Qt::black)}, {1.0, QColor(Qt::white)}});

  EXPECT_EQ(ramp.colorAt(-5.0), QColor(Qt::black));
  EXPECT_EQ(ramp.colorAt(5.0), QColor(Qt::white));
}

TEST(ColorRampTest, SortsStopsGivenOutOfOrder)
{
  // A ramp authored back to front would otherwise read colours from the
  // wrong segment for every lookup.
  const ColorRamp ramp({{1.0, QColor(Qt::white)}, {0.0, QColor(Qt::black)}});

  EXPECT_EQ(ramp.colorAt(0.0), QColor(Qt::black));
  EXPECT_EQ(ramp.colorAt(1.0), QColor(Qt::white));
}

TEST(ColorRampTest, SamplesClassCentresNotEdges)
{
  const ColorRamp ramp({{0.0, QColor(Qt::black)}, {1.0, QColor(Qt::white)}});

  const QVector<QColor> colors = ramp.sample(4);

  ASSERT_EQ(colors.size(), 4);

  // Centres of four classes are at 1/8, 3/8, 5/8, 7/8 — so neither end is
  // pure black or pure white, which is the point.
  EXPECT_NEAR(colors.first().redF(), 0.125, 0.01);
  EXPECT_NEAR(colors.last().redF(), 0.875, 0.01);

  for (int i = 1; i < colors.size(); ++i)
  {
    EXPECT_GT(colors.at(i).redF(), colors.at(i - 1).redF());
  }
}

TEST(ColorRampTest, BuiltinsAreDistinctAndOrdered)
{
  const QStringList names = ColorRamp::builtinNames();
  ASSERT_FALSE(names.isEmpty());

  for (const QString &name : names)
  {
    const ColorRamp ramp = ColorRamp::builtin(name);

    EXPECT_FALSE(ramp.isEmpty()) << name.toStdString() << " has no stops";
    EXPECT_TRUE(ramp.colorAt(0.0).isValid());
    EXPECT_NE(ramp.colorAt(0.0), ramp.colorAt(1.0))
      << name.toStdString() << " starts and ends the same colour";
  }

  // Viridis is the default, and an unknown name must fall back to it rather
  // than to an empty ramp that would draw nothing.
  EXPECT_EQ(ColorRamp::builtin(QStringLiteral("no such ramp")).colorAt(0.5),
            ColorRamp::builtin(QStringLiteral("Viridis")).colorAt(0.5));
}

TEST(ColorRampTest, ReversesEndToEnd)
{
  const ColorRamp forward = ColorRamp::builtin(QStringLiteral("Viridis"));
  const ColorRamp backward = forward.reversed();

  EXPECT_EQ(backward.colorAt(0.0), forward.colorAt(1.0));
  EXPECT_EQ(backward.colorAt(1.0), forward.colorAt(0.0));
}

// ── Classification ──────────────────────────────────────────────────────────

TEST(ClassificationTest, EqualIntervalSplitsTheRangeEvenly)
{
  Classification classification;
  classification.setMethod(ClassificationMethod::EqualInterval);
  classification.setClassCount(4);

  // 0..100 in four classes: edges at 0, 25, 50, 75, 100 — regardless of how
  // the values are distributed inside that range.
  ASSERT_TRUE(classification.classify({0.0, 1.0, 2.0, 3.0, 99.0, 100.0}));

  const QVector<ClassBreak> &breaks = classification.breaks();
  ASSERT_EQ(breaks.size(), 4);

  EXPECT_NEAR(breaks.at(0).lower, 0.0, 1e-9);
  EXPECT_NEAR(breaks.at(0).upper, 25.0, 1e-9);
  EXPECT_NEAR(breaks.at(3).lower, 75.0, 1e-9);
  EXPECT_NEAR(breaks.at(3).upper, 100.0, 1e-9);
}

TEST(ClassificationTest, QuantilePutsEqualCountsInEachClass)
{
  Classification classification;
  classification.setMethod(ClassificationMethod::Quantile);
  classification.setClassCount(2);

  // The same skewed data as above. Quantile splits at the median — around 2.5
  // — not at the midpoint of the range, which is the whole difference between
  // the two methods.
  ASSERT_TRUE(classification.classify({0.0, 1.0, 2.0, 3.0, 99.0, 100.0}));

  const QVector<ClassBreak> &breaks = classification.breaks();
  ASSERT_EQ(breaks.size(), 2);

  EXPECT_NEAR(breaks.at(0).upper, 2.5, 1e-9)
    << "quantile split at the range midpoint, which is equal interval";
}

TEST(ClassificationTest, NaturalBreaksFindsTheGapInTheData)
{
  Classification classification;
  classification.setMethod(ClassificationMethod::NaturalBreaks);
  classification.setClassCount(2);

  // Two tight clusters with a wide gap between them. Any method that ignores
  // the data's shape puts the boundary somewhere else.
  const QVector<double> clustered = {1.0, 1.2, 1.4, 1.5,
                                     20.0, 20.3, 20.6, 21.0};

  ASSERT_TRUE(classification.classify(clustered));

  const QVector<ClassBreak> &breaks = classification.breaks();
  ASSERT_EQ(breaks.size(), 2);

  // Jenks boundaries are values from the data, so the split lands exactly on
  // the first member of the upper cluster. "Somewhere in the gap" would also
  // be true of equal interval's midpoint at 11, which is why that is not the
  // assertion.
  EXPECT_NEAR(breaks.at(0).upper, 20.0, 1e-9);

  // And the method genuinely differs from the others on this data — the whole
  // reason for offering more than one.
  Classification equal;
  equal.setMethod(ClassificationMethod::EqualInterval);
  equal.setClassCount(2);
  ASSERT_TRUE(equal.classify(clustered));

  EXPECT_GT(std::abs(equal.breaks().at(0).upper - breaks.at(0).upper), 1.0)
    << "natural breaks returned the equal-interval answer";
}

TEST(ClassificationTest, ClassIntervalsAreHalfOpenAndTheLastOneIsClosed)
{
  Classification classification;
  classification.setClassCount(2);

  ASSERT_TRUE(classification.classify(ramp(11, 0.0, 1.0)));

  // Edges at 0, 5, 10. A boundary value belongs to the class above it, so
  // adjacent classes cannot both claim it.
  EXPECT_EQ(classification.indexFor(0.0), 0);
  EXPECT_EQ(classification.indexFor(4.999), 0);
  EXPECT_EQ(classification.indexFor(5.0), 1);

  // The largest value in the layer must land somewhere; a purely half-open
  // last class would drop it off the map.
  EXPECT_EQ(classification.indexFor(10.0), 1);

  EXPECT_EQ(classification.indexFor(-1.0), -1);
  EXPECT_EQ(classification.indexFor(11.0), -1);
  EXPECT_EQ(classification.indexFor(std::nan("")), -1);
}

TEST(ClassificationTest, RefusesToInventClassesTheDataCannotFill)
{
  Classification classification;
  classification.setClassCount(8);

  // Three distinct values cannot support eight classes; five of them would
  // be legend entries matching no feature at all.
  ASSERT_TRUE(classification.classify({5.0, 5.0, 7.0, 9.0, 9.0}));
  EXPECT_EQ(classification.breaks().size(), 3);
}

TEST(ClassificationTest, HandlesDataWithNoRangeAtAll)
{
  Classification classification;
  classification.setClassCount(5);

  // Every value identical: one class is the honest answer, and dividing by
  // the zero range is not.
  ASSERT_TRUE(classification.classify({42.0, 42.0, 42.0}));

  ASSERT_EQ(classification.breaks().size(), 1);
  EXPECT_EQ(classification.indexFor(42.0), 0);
}

TEST(ClassificationTest, IgnoresNonFiniteValues)
{
  Classification classification;
  classification.setClassCount(2);

  // A NaN sorts unpredictably and would poison every comparison after it.
  ASSERT_TRUE(classification.classify(
    {0.0, std::nan(""), 5.0, 10.0, std::numeric_limits<double>::infinity()}));

  const QVector<ClassBreak> &breaks = classification.breaks();
  ASSERT_EQ(breaks.size(), 2);
  EXPECT_NEAR(breaks.first().lower, 0.0, 1e-9);
  EXPECT_NEAR(breaks.last().upper, 10.0, 1e-9);
}

TEST(ClassificationTest, RefusesEmptyData)
{
  Classification classification;

  EXPECT_FALSE(classification.classify({}));
  EXPECT_TRUE(classification.isEmpty());
  EXPECT_EQ(classification.indexFor(1.0), -1);
}

TEST(ClassificationTest, ManualBreaksSurviveAReclassify)
{
  Classification classification;

  ASSERT_TRUE(classification.setManualBreaks({0.0, 10.0, 100.0}));
  ASSERT_EQ(classification.breaks().size(), 2);

  // Recomputing from data would silently discard the user's boundaries.
  classification.classify(ramp(50, 0.0, 3.0));

  ASSERT_EQ(classification.breaks().size(), 2);
  EXPECT_NEAR(classification.breaks().at(0).upper, 10.0, 1e-9);
}

TEST(ClassificationTest, ColoursComeFromTheRampAndFollowIt)
{
  Classification classification;
  classification.setClassCount(3);
  classification.setRamp(
    ColorRamp({{0.0, QColor(Qt::black)}, {1.0, QColor(Qt::white)}}));

  ASSERT_TRUE(classification.classify(ramp(30, 0.0, 1.0)));

  const QVector<ClassBreak> &breaks = classification.breaks();
  ASSERT_EQ(breaks.size(), 3);

  for (int i = 1; i < breaks.size(); ++i)
  {
    EXPECT_GT(breaks.at(i).color.redF(), breaks.at(i - 1).color.redF())
      << "class colours do not follow the ramp";
  }

  // Copied, not referenced: breaks() hands back a reference into the object
  // about to be restyled, so comparing against it afterwards would compare
  // the new colours with themselves.
  const QColor beforeRamp = breaks.at(0).color;

  // Switching ramps must recolour what is already classified, or the legend
  // shows one palette and the map another.
  classification.setRamp(ColorRamp::builtin(QStringLiteral("Reds")));
  EXPECT_NE(classification.breaks().at(0).color, beforeRamp);
}

TEST(ClassificationTest, LabelsUseTheConfiguredPrecision)
{
  Classification classification;
  classification.setClassCount(2);
  classification.setLabelPrecision(0);

  ASSERT_TRUE(classification.classify(ramp(11, 0.0, 1.0)));

  EXPECT_EQ(classification.breaks().first().label, QStringLiteral("0 – 5"));
}

// ── Layer styles ────────────────────────────────────────────────────────────

TEST(LayerStyleTest, SingleModeColoursEveryFeatureTheSame)
{
  TableProvider provider;
  provider.add(1.0, QStringLiteral("a"));
  provider.add(99.0, QStringLiteral("b"));

  LayerStyle style;
  Symbol symbol;
  symbol.fill = QColor(Qt::magenta);
  style.setSymbol(symbol);

  ASSERT_TRUE(style.rebuild(provider));

  EXPECT_EQ(style.colorFor(provider, 0), QColor(Qt::magenta));
  EXPECT_EQ(style.colorFor(provider, 1), QColor(Qt::magenta));
  EXPECT_TRUE(style.legendItems().isEmpty())
    << "a single symbol needs no legend rows";
}

TEST(LayerStyleTest, GraduatedModeColoursByClass)
{
  TableProvider provider;

  for (int i = 0; i < 10; ++i)
    provider.add(static_cast<double>(i), QStringLiteral("x"));

  LayerStyle style;
  style.setMode(StyleMode::Graduated);
  style.setAttribute(QStringLiteral("value"));
  style.classification().setClassCount(2);

  ASSERT_TRUE(style.rebuild(provider));

  const QColor low = style.colorFor(provider, 0);
  const QColor high = style.colorFor(provider, 9);

  EXPECT_TRUE(low.isValid());
  EXPECT_TRUE(high.isValid());
  EXPECT_NE(low, high) << "both ends of the range got the same colour";

  const QVector<LegendItem> legend = style.legendItems();
  ASSERT_EQ(legend.size(), 2);
  EXPECT_EQ(legend.at(0).color, low);
  EXPECT_EQ(legend.at(1).color, high);
}

TEST(LayerStyleTest, CategorizedModeColoursByValue)
{
  TableProvider provider;
  provider.add(1.0, QStringLiteral("pipe"));
  provider.add(2.0, QStringLiteral("weir"));
  provider.add(3.0, QStringLiteral("pipe"));

  LayerStyle style;
  style.setMode(StyleMode::Categorized);
  style.setAttribute(QStringLiteral("kind"));

  ASSERT_TRUE(style.rebuild(provider));

  ASSERT_EQ(style.categories().size(), 2)
    << "distinct values were not de-duplicated";

  // Two features of the same kind must share a colour; that is the whole
  // claim a categorised map makes.
  EXPECT_EQ(style.colorFor(provider, 0), style.colorFor(provider, 2));
  EXPECT_NE(style.colorFor(provider, 0), style.colorFor(provider, 1));

  const QVector<LegendItem> legend = style.legendItems();
  ASSERT_EQ(legend.size(), 2);
  EXPECT_EQ(legend.at(0).label, QStringLiteral("pipe"));
}

TEST(LayerStyleTest, HidingALegendRowStopsThoseFeaturesBeingDrawn)
{
  TableProvider provider;
  provider.add(1.0, QStringLiteral("pipe"));
  provider.add(2.0, QStringLiteral("weir"));

  LayerStyle style;
  style.setMode(StyleMode::Categorized);
  style.setAttribute(QStringLiteral("kind"));
  ASSERT_TRUE(style.rebuild(provider));

  style.setLegendItemVisible(0, false);

  // An invalid colour is how the style says "do not draw this one"; giving
  // it the base symbol instead would leave the feature on the map.
  EXPECT_FALSE(style.colorFor(provider, 0).isValid());
  EXPECT_TRUE(style.colorFor(provider, 1).isValid());
  EXPECT_FALSE(style.legendItems().at(0).visible);
}

TEST(LayerStyleTest, HidingAGraduatedClassStopsThoseFeaturesBeingDrawn)
{
  TableProvider provider;

  for (int i = 0; i < 10; ++i)
    provider.add(static_cast<double>(i), QStringLiteral("x"));

  LayerStyle style;
  style.setMode(StyleMode::Graduated);
  style.setAttribute(QStringLiteral("value"));
  style.classification().setClassCount(2);
  ASSERT_TRUE(style.rebuild(provider));

  style.setLegendItemVisible(1, false);

  EXPECT_TRUE(style.colorFor(provider, 0).isValid());
  EXPECT_FALSE(style.colorFor(provider, 9).isValid());
}

TEST(LayerStyleTest, RebuildKeepsCategoriesTheUserSwitchedOff)
{
  TableProvider provider;
  provider.add(1.0, QStringLiteral("pipe"));
  provider.add(2.0, QStringLiteral("weir"));

  LayerStyle style;
  style.setMode(StyleMode::Categorized);
  style.setAttribute(QStringLiteral("kind"));
  ASSERT_TRUE(style.rebuild(provider));

  style.setLegendItemVisible(0, false);
  style.setCategoryColor(1, QColor(Qt::red));

  // Refreshing a layer must not switch categories back on behind the user.
  ASSERT_TRUE(style.rebuild(provider));

  EXPECT_FALSE(style.legendItems().at(0).visible);
  EXPECT_EQ(style.legendItems().at(1).color, QColor(Qt::red));
}

TEST(LayerStyleTest, LeavesValuesNoCategoryCoversUndrawn)
{
  TableProvider provider;
  provider.add(1.0, QStringLiteral("pipe"));

  LayerStyle style;
  style.setMode(StyleMode::Categorized);
  style.setAttribute(QStringLiteral("kind"));
  ASSERT_TRUE(style.rebuild(provider));

  // A value that arrived after the style was built — an edit, or a refreshed
  // dataset. Giving it the base symbol would put it on the map looking like a
  // category of its own that the legend never mentions.
  provider.add(2.0, QStringLiteral("outfall"));

  EXPECT_TRUE(style.colorFor(provider, 0).isValid());
  EXPECT_FALSE(style.colorFor(provider, 1).isValid())
    << "an unclassified value was drawn anyway";
}

TEST(LayerStyleTest, RefusesToThemeByAFieldTheLayerDoesNotHave)
{
  TableProvider provider;
  provider.add(1.0, QStringLiteral("pipe"));

  LayerStyle style;
  style.setMode(StyleMode::Graduated);
  style.setAttribute(QStringLiteral("depth_that_does_not_exist"));

  // Saying no is the point: colouring everything one colour would look like
  // a working map themed by a column that is not there.
  EXPECT_FALSE(style.rebuild(provider));
  EXPECT_TRUE(style.legendItems().isEmpty());
  EXPECT_FALSE(style.colorFor(provider, 0).isValid());
}

TEST(LayerStyleTest, SkipsFeaturesWithNoUsableValue)
{
  TableProvider provider;
  provider.add(1.0, QStringLiteral("pipe"));
  provider.add(QStringLiteral("not a number"), QStringLiteral("weir"));

  LayerStyle style;
  style.setMode(StyleMode::Graduated);
  style.setAttribute(QStringLiteral("value"));
  ASSERT_TRUE(style.rebuild(provider));

  EXPECT_TRUE(style.colorFor(provider, 0).isValid());
  EXPECT_FALSE(style.colorFor(provider, 1).isValid());
}

TEST(LayerStyleTest, MissingValuesDoNotDragTheBreaksTowardsZero)
{
  TableProvider provider;
  provider.add(100.0, QStringLiteral("a"));
  provider.add(QVariant(), QStringLiteral("b"));
  provider.add(200.0, QStringLiteral("c"));

  // A missing measurement read as zero would put the lower edge at 0 and
  // squeeze both real values into the top class.
  const QVector<double> values =
    provider.numericValues(QStringLiteral("value"));

  ASSERT_EQ(values.size(), 2);
  EXPECT_NEAR(values.first(), 100.0, 1e-9);
}
