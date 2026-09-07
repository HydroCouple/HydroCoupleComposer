/*!
 * \file   test_graphlayout.cpp
 * \brief  Automatic placement for the coupling graph.
 *
 * The layout is a pure function of the composition and the node sizes, so
 * these gates need no scene, no window and no mouse: they assert the shape
 * of the arrangement — who stands left of whom, what does not overlap —
 * rather than exact coordinates, which would break on any spacing change
 * without saying anything had gone wrong.
 */

#include "canvas/graphlayout.h"

#include <gtest/gtest.h>

#include <QRectF>

using namespace HydroCouple::Composer;
using CompositionSpec = HydroCouple::SDK::IO::CompositionSpec;
using ComponentSpec = HydroCouple::SDK::IO::ComponentSpec;
using ConnectionSpec = HydroCouple::SDK::IO::ConnectionSpec;

namespace
{
  void addComponent(CompositionSpec &spec, const std::string &id)
  {
    ComponentSpec component;
    component.id = id;
    spec.components.push_back(component);
  }

  void connect(CompositionSpec &spec, const std::string &from,
               const std::string &to)
  {
    ConnectionSpec connection;
    connection.fromComponent = from;
    connection.output = "out";
    connection.toComponent = to;
    connection.input = "in";
    spec.connections.push_back(connection);
  }

  //! Every component the same size, so the shape is the only variable.
  QHash<QString, QSizeF> uniformSizes(const CompositionSpec &spec)
  {
    QHash<QString, QSizeF> sizes;

    for (const ComponentSpec &component : spec.components)
    {
      sizes.insert(QString::fromStdString(component.id), QSizeF(160.0, 70.0));
    }

    return sizes;
  }
}

TEST(GraphLayoutTest, AProviderStandsLeftOfWhatItFeeds)
{
  CompositionSpec spec;
  addComponent(spec, "recorder");
  addComponent(spec, "gain");
  addComponent(spec, "source");

  // Declared in the WRONG order on purpose: the arrangement must come from
  // the connections, not from the order the document happens to list.
  connect(spec, "source", "gain");
  connect(spec, "gain", "recorder");

  const QHash<QString, QPointF> placed =
    layoutComposition(spec, uniformSizes(spec));

  ASSERT_EQ(placed.size(), 3);
  EXPECT_LT(placed[QStringLiteral("source")].x(),
            placed[QStringLiteral("gain")].x());
  EXPECT_LT(placed[QStringLiteral("gain")].x(),
            placed[QStringLiteral("recorder")].x());
}

TEST(GraphLayoutTest, ComponentsInOneColumnDoNotOverlap)
{
  CompositionSpec spec;
  addComponent(spec, "a");
  addComponent(spec, "b");
  addComponent(spec, "c");
  addComponent(spec, "sink");

  // Three providers into one consumer: the three share a column.
  connect(spec, "a", "sink");
  connect(spec, "b", "sink");
  connect(spec, "c", "sink");

  const QHash<QString, QSizeF> sizes = uniformSizes(spec);
  const QHash<QString, QPointF> placed = layoutComposition(spec, sizes);

  ASSERT_EQ(placed.size(), 4);

  const QStringList column{QStringLiteral("a"), QStringLiteral("b"),
                           QStringLiteral("c")};

  for (const QString &one : column)
  {
    EXPECT_LT(placed[one].x(), placed[QStringLiteral("sink")].x());

    for (const QString &other : column)
    {
      if (one == other)
      {
        continue;
      }

      const QRectF first(placed[one], sizes.value(one));
      const QRectF second(placed[other], sizes.value(other));

      EXPECT_FALSE(first.intersects(second))
        << one.toStdString() << " overlaps " << other.toStdString();
    }
  }
}

TEST(GraphLayoutTest, AFeedbackLoopIsArrangedRatherThanRefused)
{
  CompositionSpec spec;
  addComponent(spec, "ocean");
  addComponent(spec, "atmosphere");
  addComponent(spec, "observer");

  // A two-way coupling: normal, and no order of ranks can satisfy it.
  connect(spec, "ocean", "atmosphere");
  connect(spec, "atmosphere", "ocean");
  connect(spec, "ocean", "observer");

  const QHash<QString, QSizeF> sizes = uniformSizes(spec);
  const QHash<QString, QPointF> placed = layoutComposition(spec, sizes);

  // Every component placed, and the cycle's members not stacked on top of
  // one another — the layout must terminate and produce something usable.
  ASSERT_EQ(placed.size(), 3);

  const QRectF ocean(placed[QStringLiteral("ocean")],
                     sizes.value(QStringLiteral("ocean")));
  const QRectF atmosphere(placed[QStringLiteral("atmosphere")],
                          sizes.value(QStringLiteral("atmosphere")));

  EXPECT_FALSE(ocean.intersects(atmosphere));
}

TEST(GraphLayoutTest, AnUnconnectedComponentIsStillGivenAPlace)
{
  CompositionSpec spec;
  addComponent(spec, "lonely");
  addComponent(spec, "source");
  addComponent(spec, "sink");
  connect(spec, "source", "sink");

  const QHash<QString, QPointF> placed =
    layoutComposition(spec, uniformSizes(spec));

  ASSERT_EQ(placed.size(), 3);
  EXPECT_TRUE(placed.contains(QStringLiteral("lonely")))
    << "a component nothing connects to was dropped from the layout";
}

TEST(GraphLayoutTest, WiderComponentsGetAWiderColumn)
{
  CompositionSpec spec;
  addComponent(spec, "wide");
  addComponent(spec, "consumer");
  connect(spec, "wide", "consumer");

  QHash<QString, QSizeF> sizes = uniformSizes(spec);
  const QHash<QString, QPointF> narrow = layoutComposition(spec, sizes);

  sizes.insert(QStringLiteral("wide"), QSizeF(260.0, 70.0));
  const QHash<QString, QPointF> wide = layoutComposition(spec, sizes);

  // The next column starts clear of the widest box in this one, or a long
  // component name would run straight through the connection beside it.
  EXPECT_GT(wide[QStringLiteral("consumer")].x(),
            narrow[QStringLiteral("consumer")].x());
}
