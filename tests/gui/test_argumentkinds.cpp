/*!
 * \file   test_argumentkinds.cpp
 * \brief  U2a — which editor an argument gets, and in what order that is
 *         decided.
 *
 * describeArgument() needs a loaded component library and therefore a
 * build; chooseEditorKind() needs nothing, which is the whole reason the
 * two were split. What is checked here is the part that can actually be
 * wrong: the *order* of the chain. A mesh that also varies in time must
 * not arrive at the time-series editor and lose its geometry; a
 * categorical argument holding a hundred values must not be read as a
 * table; and — B5a's rule, which still stands — nothing may be decided
 * from an argument's id or caption, which is why neither appears in
 * ArgumentFacts at all.
 */

#include "configurator/argumentdescriptor.h"

#include <gtest/gtest.h>

#include <QSet>
#include <QString>

#include <iterator>

using namespace HydroCouple::Composer;
using HydroCouple::DataKind;

namespace
{
  //! A plain numeric scalar: the thing every other case is a departure from.
  ArgumentFacts number()
  {
    ArgumentFacts facts;
    facts.dataKind = DataKind::Float64;

    return facts;
  }
}

TEST(ArgumentKindTest, TheFallbackChainStillDecidesWhatItAlwaysDid)
{
  // The pre-U2a behaviour, unchanged. An argument that answers no typed
  // interface must land exactly where it used to, or this phase is a
  // regression wearing a feature's clothes.
  EXPECT_EQ(chooseEditorKind(number()), ArgumentEditorKind::Number);

  ArgumentFacts integer = number();
  integer.dataKind = DataKind::Int32;
  EXPECT_EQ(chooseEditorKind(integer), ArgumentEditorKind::Integer);

  ArgumentFacts boolean = number();
  boolean.dataKind = DataKind::Boolean;
  EXPECT_EQ(chooseEditorKind(boolean), ArgumentEditorKind::Boolean);

  ArgumentFacts text = number();
  text.dataKind = DataKind::String;
  EXPECT_EQ(chooseEditorKind(text), ArgumentEditorKind::Text);

  ArgumentFacts opaque = number();
  opaque.dataKind = DataKind::Opaque;
  EXPECT_EQ(chooseEditorKind(opaque), ArgumentEditorKind::Raw);

  ArgumentFacts unknown;
  EXPECT_EQ(chooseEditorKind(unknown), ArgumentEditorKind::Raw);

  ArgumentFacts grid = number();
  grid.isScalar = false;
  EXPECT_EQ(chooseEditorKind(grid), ArgumentEditorKind::Table);

  ArgumentFacts categorical = number();
  categorical.hasCategories = true;
  EXPECT_EQ(chooseEditorKind(categorical), ArgumentEditorKind::Categorical);

  ArgumentFacts path = number();
  path.hasFileFilters = true;
  EXPECT_EQ(chooseEditorKind(path), ArgumentEditorKind::FilePath);
}

TEST(ArgumentKindTest, ATypedInterfaceBeatsWhateverTheShapeSuggests)
{
  // The point of U2-S. A component that implements the interface has told
  // us what its argument is; deciding from rank instead calls a mesh a
  // table and hands the user a spreadsheet of coordinates. This is the
  // gate that fails if the chain is ever reordered to put shape first.
  const struct
  {
      void (*mark)(ArgumentFacts &);
      ArgumentEditorKind expected;
      const char *what;
  } cases[] = {
    { [](ArgumentFacts &f) { f.isTimeSeries = true; },
      ArgumentEditorKind::TimeSeries, "a time series" },
    { [](ArgumentFacts &f) { f.isPolyhedralSurface = true; },
      ArgumentEditorKind::Mesh, "a surface" },
    { [](ArgumentFacts &f) { f.isGeometry = true; },
      ArgumentEditorKind::Geometry, "geometry" },
    { [](ArgumentFacts &f) { f.isRaster = true; },
      ArgumentEditorKind::Raster, "a raster" },
    { [](ArgumentFacts &f) { f.isIdBased = true; },
      ArgumentEditorKind::IdTable, "an id table" },
  };

  for (const auto &one : cases)
  {
    // Shaped like a table, which is what it would otherwise be read as.
    ArgumentFacts facts = number();
    facts.isScalar = false;
    one.mark(facts);

    EXPECT_EQ(chooseEditorKind(facts), one.expected)
      << one.what << " was read as something else";

    // And with file filters too, which would otherwise make it a path.
    ArgumentFacts fromFile = number();
    fromFile.isScalar = false;
    fromFile.hasFileFilters = true;
    one.mark(fromFile);

    EXPECT_EQ(chooseEditorKind(fromFile), one.expected)
      << one.what << " read from a file was read as a path";
  }
}

TEST(ArgumentKindTest, SpaceBeatsTimeWhenAnArgumentIsBoth)
{
  // A time-varying mesh is a mesh whose values move. The geometry is the
  // hard thing to edit and a time axis can be a column inside that
  // editor; a time-series editor has nowhere to put twelve thousand
  // faces. HydroCouple has a named interface for exactly this
  // combination — ITimeSeriesPolyhedralSurfaceComponentDataItem — so it
  // is not a hypothetical.
  ArgumentFacts mesh = number();
  mesh.isScalar = false;
  mesh.isTimeSeries = true;
  mesh.isPolyhedralSurface = true;

  EXPECT_EQ(chooseEditorKind(mesh), ArgumentEditorKind::Mesh);

  ArgumentFacts raster = number();
  raster.isScalar = false;
  raster.isTimeSeries = true;
  raster.isRaster = true;

  EXPECT_EQ(chooseEditorKind(raster), ArgumentEditorKind::Raster);

  // But time beats identifiers: a table of series is worth plotting
  // against its time axis, and the ids are its rows.
  ArgumentFacts series = number();
  series.isScalar = false;
  series.isTimeSeries = true;
  series.isIdBased = true;

  EXPECT_EQ(chooseEditorKind(series), ArgumentEditorKind::TimeSeries);
}

TEST(ArgumentKindTest, AUnitOfPureTimeIsADurationAndAnythingElseIsAQuantity)
{
  ArgumentFacts duration = number();
  duration.hasUnit = true;
  duration.unitIsPureTime = true;

  EXPECT_EQ(chooseEditorKind(duration), ArgumentEditorKind::Duration);

  ArgumentFacts quantity = number();
  quantity.hasUnit = true;

  EXPECT_EQ(chooseEditorKind(quantity), ArgumentEditorKind::Quantity);

  // A unit does not make a grid into a scalar: a hundred discharges with
  // a unit are still a table, and the converting spin box has one box.
  ArgumentFacts many = number();
  many.isScalar = false;
  many.hasUnit = true;

  EXPECT_EQ(chooseEditorKind(many), ArgumentEditorKind::Table);

  // Nor does it make a string into a number.
  ArgumentFacts labelled = number();
  labelled.dataKind = DataKind::String;
  labelled.hasUnit = true;

  EXPECT_EQ(chooseEditorKind(labelled), ArgumentEditorKind::Text);
}

TEST(ArgumentKindTest, ACrsIsOneTheComponentWillOnlyTakeAsACrs)
{
  ArgumentFacts crs = number();
  crs.dataKind = DataKind::String;
  crs.acceptsSpatialReference = true;

  EXPECT_EQ(chooseEditorKind(crs), ArgumentEditorKind::Crs);

  // The signal is the component's declared type, not a guess. An argument
  // that accepts a spatial reference but holds a number is not a CRS
  // string, so it must not get the picker.
  ArgumentFacts numeric = number();
  numeric.acceptsSpatialReference = true;

  EXPECT_NE(chooseEditorKind(numeric), ArgumentEditorKind::Crs);

  // And a plain string is a plain string. This is the negative gate B5a
  // asked for, in its CRS form: nothing about a caption or an id can
  // promote text to a coordinate system.
  ArgumentFacts plain = number();
  plain.dataKind = DataKind::String;

  EXPECT_EQ(chooseEditorKind(plain), ArgumentEditorKind::Text);
}

TEST(ArgumentKindTest, TextReadFromAFileIsTextRatherThanAPath)
{
  // FVQual's kinetics block: the component takes a .rxn file, but what
  // the user edits is its contents. The text pane keeps a file chooser,
  // so this judgement costs nothing if it is wrong in either direction.
  ArgumentFacts kinetics = number();
  kinetics.dataKind = DataKind::String;
  kinetics.hasFileFilters = true;

  EXPECT_EQ(chooseEditorKind(kinetics), ArgumentEditorKind::LongText);

  // A file of anything else is a path: the component reads it and we have
  // no business showing its bytes in a text box.
  ArgumentFacts opaqueFile = number();
  opaqueFile.dataKind = DataKind::Opaque;
  opaqueFile.hasFileFilters = true;

  EXPECT_EQ(chooseEditorKind(opaqueFile), ArgumentEditorKind::FilePath);

  // Including a grid of text read from a file, which is a table with a
  // source rather than one long string.
  ArgumentFacts sheet = number();
  sheet.dataKind = DataKind::String;
  sheet.hasFileFilters = true;
  sheet.isScalar = false;

  EXPECT_EQ(chooseEditorKind(sheet), ArgumentEditorKind::FilePath);
}

TEST(ArgumentKindTest, ACategoricalGridIsATableNotAChoice)
{
  // The existing rule, preserved: a combo box holds one value, so a
  // hundred categorical values are a table whose cells happen to be
  // constrained. Losing this would silently drop ninety-nine of them.
  ArgumentFacts many = number();
  many.hasCategories = true;
  many.isScalar = false;

  EXPECT_EQ(chooseEditorKind(many), ArgumentEditorKind::Table);
}

TEST(ArgumentKindTest, EveryKindHasANameAndNoTwoShareOne)
{
  // The names reach messages and settings, so a duplicate would make two
  // kinds indistinguishable in a log, and an empty one would make a kind
  // invisible.
  const ArgumentEditorKind all[] = {
    ArgumentEditorKind::Categorical, ArgumentEditorKind::Number,
    ArgumentEditorKind::Integer,     ArgumentEditorKind::Boolean,
    ArgumentEditorKind::Text,        ArgumentEditorKind::FilePath,
    ArgumentEditorKind::Table,       ArgumentEditorKind::Quantity,
    ArgumentEditorKind::TimeSeries,  ArgumentEditorKind::Mesh,
    ArgumentEditorKind::Geometry,    ArgumentEditorKind::Raster,
    ArgumentEditorKind::IdTable,     ArgumentEditorKind::Duration,
    ArgumentEditorKind::Crs,         ArgumentEditorKind::LongText,
    ArgumentEditorKind::Raw,
  };

  QSet<QString> seen;

  for (ArgumentEditorKind kind : all)
  {
    const QString name = argumentEditorKindName(kind);

    EXPECT_FALSE(name.isEmpty());
    EXPECT_FALSE(seen.contains(name)) << name.toStdString() << " twice";

    seen.insert(name);
  }

  EXPECT_EQ(seen.size(), int(std::size(all)));
}

TEST(ArgumentKindTest, NoArgumentLosesTheEditorItUsedToHave)
{
  // The regression this phase could most easily have shipped. Widening
  // the enum means a newly-typed argument matches no case in the
  // configurator's switch, produces no widget, and disappears from the
  // component's form — a component's meteorology simply gone. The dock
  // draws inlineKind for that reason, and inlineKind must never be one of
  // the kinds the dock cannot draw.
  const ArgumentEditorKind drawable[] = {
    ArgumentEditorKind::Categorical, ArgumentEditorKind::Integer,
    ArgumentEditorKind::Number,      ArgumentEditorKind::Boolean,
    ArgumentEditorKind::FilePath,    ArgumentEditorKind::Text,
    ArgumentEditorKind::Table,       ArgumentEditorKind::Raw,
  };

  const auto canDraw = [&](ArgumentEditorKind kind)
  {
    for (ArgumentEditorKind one : drawable)
    {
      if (one == kind)
      {
        return true;
      }
    }

    return false;
  };

  // Every combination of the flags that matter, which is small enough to
  // enumerate rather than sample.
  for (int mask = 0; mask < 256; ++mask)
  {
    for (DataKind dataKind : { DataKind::Unknown, DataKind::Float64,
                               DataKind::Int32, DataKind::Boolean,
                               DataKind::String, DataKind::Opaque })
    {
      ArgumentFacts facts;
      facts.dataKind = dataKind;
      facts.isScalar = (mask & 1) != 0;
      facts.hasCategories = (mask & 2) != 0;
      facts.hasFileFilters = (mask & 4) != 0;
      facts.hasUnit = (mask & 8) != 0;
      facts.unitIsPureTime = (mask & 16) != 0;
      facts.isTimeSeries = (mask & 32) != 0;
      facts.isPolyhedralSurface = (mask & 64) != 0;
      facts.isRaster = (mask & 128) != 0;

      EXPECT_TRUE(canDraw(genericEditorKind(facts)))
        << "the dock would draw nothing for mask " << mask;
    }
  }
}

TEST(ArgumentKindTest, TheFallbackIsExactlyTheOldChain)
{
  // genericEditorKind is the pre-U2a behaviour, preserved. For every
  // argument that answers no typed interface and carries no unit, it and
  // the new chain must still agree — otherwise this phase changed what
  // untyped arguments look like, which it has no business doing.
  for (int mask = 0; mask < 8; ++mask)
  {
    for (DataKind dataKind : { DataKind::Unknown, DataKind::Float64,
                               DataKind::Int32, DataKind::Boolean,
                               DataKind::String, DataKind::Opaque })
    {
      ArgumentFacts facts;
      facts.dataKind = dataKind;
      facts.isScalar = (mask & 1) != 0;
      facts.hasCategories = (mask & 2) != 0;
      facts.hasFileFilters = (mask & 4) != 0;

      const ArgumentEditorKind typed = chooseEditorKind(facts);
      const ArgumentEditorKind generic = genericEditorKind(facts);

      // The one deliberate change: text read from a file is now text
      // rather than a path. Written as "wherever the new chain says
      // LongText" rather than by restating its condition — the first
      // version of this gate restated it, got it wrong, and failed,
      // because a categorical argument that also reads files is still
      // categorical: categories are tested first in both chains.
      if (typed == ArgumentEditorKind::LongText)
      {
        EXPECT_EQ(generic, ArgumentEditorKind::FilePath);

        continue;
      }

      EXPECT_EQ(typed, generic)
        << "an untyped argument changed editor, at mask " << mask;
    }
  }
}
