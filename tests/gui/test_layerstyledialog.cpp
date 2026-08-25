/*!
 * \file   test_layerstyledialog.cpp
 * \brief  Phase C1c verification — the style editor.
 *
 * The dialog edits the layer's own style object rather than a copy, so these
 * tests check the layer, not the form: a dialog that faithfully recorded
 * every choice into a copy nobody reads would look identical from the inside
 * and change nothing on the map.
 */

#include "core/composerapplication.h"
#include "map/maplayer.h"
#include "probelayer.h"
#include "render/layerstyle.h"
#include "ui/dialogs/layerstyledialog.h"

#include <gtest/gtest.h>

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSignalSpy>
#include <QSpinBox>

using namespace HydroCouple::Composer;
namespace Testing = HydroCouple::Composer::Testing;

namespace
{
  class StyleDialogTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_layerstyledialog";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      //! A layer with one numeric field and one categorical field.
      static std::unique_ptr<Testing::ProbeFeatureLayer> makeLayer()
      {
        auto layer =
          std::make_unique<Testing::ProbeFeatureLayer>(QStringLiteral("probe"));

        AttributeField depth;
        depth.name = QStringLiteral("depth");
        depth.displayName = QStringLiteral("Depth");
        depth.unit = QStringLiteral("m");
        layer->declareField(depth);

        AttributeField kind;
        kind.name = QStringLiteral("kind");
        kind.type = QMetaType::QString;
        layer->declareField(kind);

        for (int i = 0; i < 12; ++i)
        {
          layer->addFeature(
            QPointF(i, i),
            {{QStringLiteral("depth"), static_cast<double>(i)},
             {QStringLiteral("kind"), i % 2 == 0 ? QStringLiteral("pipe")
                                                 : QStringLiteral("weir")}});
        }

        return layer;
      }

      static ComposerApplication *s_app;
  };

  ComposerApplication *StyleDialogTest::s_app = nullptr;
}

TEST_F(StyleDialogTest, OffersItselfOnlyForLayersWithAStyle)
{
  const std::unique_ptr<Testing::ProbeFeatureLayer> styled = makeLayer();
  const Testing::ProbeLayer plain(QStringLiteral("plain"),
                                  QRectF(0.0, 0.0, 1.0, 1.0));

  EXPECT_TRUE(LayerStyleDialog::canStyle(styled.get()));
  EXPECT_FALSE(LayerStyleDialog::canStyle(&plain))
    << "a layer with no style was offered a style editor";
  EXPECT_FALSE(LayerStyleDialog::canStyle(nullptr));
}

TEST_F(StyleDialogTest, ListsTheLayersOwnAttributesWithUnits)
{
  const std::unique_ptr<Testing::ProbeFeatureLayer> layer = makeLayer();
  LayerStyleDialog dialog(layer.get());

  auto *attributes =
    dialog.findChild<QComboBox *>(QStringLiteral("styleAttributeCombo"));
  ASSERT_NE(attributes, nullptr);

  ASSERT_EQ(attributes->count(), 2)
    << "the attribute list was not taken from the layer";

  // The label carries the unit; the data carries the canonical field name the
  // style stores, and those must not be confused.
  EXPECT_EQ(attributes->itemText(0), QStringLiteral("Depth (m)"));
  EXPECT_EQ(attributes->itemData(0).toString(), QStringLiteral("depth"));
}

TEST_F(StyleDialogTest, WritesTheFormOntoTheLayersOwnStyle)
{
  const std::unique_ptr<Testing::ProbeFeatureLayer> layer = makeLayer();
  LayerStyleDialog dialog(layer.get());

  QSignalSpy appearanceSpy(layer.get(), &MapLayer::appearanceChanged);

  auto *mode = dialog.findChild<QComboBox *>(QStringLiteral("styleModeCombo"));
  auto *attributes =
    dialog.findChild<QComboBox *>(QStringLiteral("styleAttributeCombo"));
  auto *classes =
    dialog.findChild<QSpinBox *>(QStringLiteral("styleClassCountSpin"));
  ASSERT_NE(mode, nullptr);
  ASSERT_NE(attributes, nullptr);
  ASSERT_NE(classes, nullptr);

  mode->setCurrentIndex(
    mode->findData(static_cast<int>(StyleMode::Graduated)));
  attributes->setCurrentIndex(
    attributes->findData(QStringLiteral("depth")));
  classes->setValue(3);

  ASSERT_TRUE(dialog.apply());

  // The layer's own style, not a copy the dialog kept to itself.
  const LayerStyle *style = layer->style();
  ASSERT_NE(style, nullptr);

  EXPECT_EQ(style->mode(), StyleMode::Graduated);
  EXPECT_EQ(style->attribute(), QStringLiteral("depth"));
  EXPECT_EQ(style->classification().breaks().size(), 3);

  // And the map was told, or the change would sit invisible until something
  // else happened to trigger a repaint.
  EXPECT_GE(appearanceSpy.count(), 1);
}

TEST_F(StyleDialogTest, ReopeningShowsWhatWasApplied)
{
  const std::unique_ptr<Testing::ProbeFeatureLayer> layer = makeLayer();

  layer->styleRef().setMode(StyleMode::Categorized);
  layer->styleRef().setAttribute(QStringLiteral("kind"));
  layer->styleRef().classification().setClassCount(7);
  ASSERT_TRUE(layer->restyle());

  LayerStyleDialog dialog(layer.get());

  auto *mode = dialog.findChild<QComboBox *>(QStringLiteral("styleModeCombo"));
  auto *attributes =
    dialog.findChild<QComboBox *>(QStringLiteral("styleAttributeCombo"));
  auto *classes =
    dialog.findChild<QSpinBox *>(QStringLiteral("styleClassCountSpin"));

  EXPECT_EQ(mode->currentData().toInt(),
            static_cast<int>(StyleMode::Categorized));
  EXPECT_EQ(attributes->currentData().toString(), QStringLiteral("kind"));
  EXPECT_EQ(classes->value(), 7);
}

TEST_F(StyleDialogTest, DisablesControlsThatDoNotApplyToTheChosenMode)
{
  const std::unique_ptr<Testing::ProbeFeatureLayer> layer = makeLayer();
  LayerStyleDialog dialog(layer.get());

  auto *mode = dialog.findChild<QComboBox *>(QStringLiteral("styleModeCombo"));
  auto *method =
    dialog.findChild<QComboBox *>(QStringLiteral("styleMethodCombo"));
  auto *classes =
    dialog.findChild<QSpinBox *>(QStringLiteral("styleClassCountSpin"));
  auto *attributes =
    dialog.findChild<QComboBox *>(QStringLiteral("styleAttributeCombo"));

  mode->setCurrentIndex(mode->findData(static_cast<int>(StyleMode::Single)));
  EXPECT_FALSE(attributes->isEnabled());
  EXPECT_FALSE(classes->isEnabled());

  // Categories are the values themselves, so a break method and a class count
  // mean nothing there.
  mode->setCurrentIndex(
    mode->findData(static_cast<int>(StyleMode::Categorized)));
  EXPECT_TRUE(attributes->isEnabled());
  EXPECT_FALSE(method->isEnabled());
  EXPECT_FALSE(classes->isEnabled());

  mode->setCurrentIndex(
    mode->findData(static_cast<int>(StyleMode::Graduated)));
  EXPECT_TRUE(method->isEnabled());
  EXPECT_TRUE(classes->isEnabled());
}

TEST_F(StyleDialogTest, SaysSoWhenTheChosenStyleCanDrawNothing)
{
  auto layer =
    std::make_unique<Testing::ProbeFeatureLayer>(QStringLiteral("empty"));

  AttributeField text;
  text.name = QStringLiteral("label");
  text.type = QMetaType::QString;
  layer->declareField(text);

  layer->addFeature(QPointF(0.0, 0.0),
                    {{QStringLiteral("label"), QStringLiteral("not a number")}});

  LayerStyleDialog dialog(layer.get());

  auto *mode = dialog.findChild<QComboBox *>(QStringLiteral("styleModeCombo"));
  mode->setCurrentIndex(
    mode->findData(static_cast<int>(StyleMode::Graduated)));

  // Graduated on a text column: nothing can be classified, and the dialog has
  // to say so rather than close on a layer that quietly stopped drawing.
  EXPECT_FALSE(dialog.apply());

  auto *status = dialog.findChild<QLabel *>(QStringLiteral("styleStatusLabel"));
  ASSERT_NE(status, nullptr);
  EXPECT_FALSE(status->text().isEmpty());
}

TEST_F(StyleDialogTest, TurnsLabellingOnForTheChosenField)
{
  const std::unique_ptr<Testing::ProbeFeatureLayer> layer = makeLayer();
  LayerStyleDialog dialog(layer.get());

  auto *labels =
    dialog.findChild<QCheckBox *>(QStringLiteral("styleLabelsCheck"));
  auto *field =
    dialog.findChild<QComboBox *>(QStringLiteral("styleLabelFieldCombo"));
  ASSERT_NE(labels, nullptr);
  ASSERT_NE(field, nullptr);

  EXPECT_FALSE(field->isEnabled()) << "a label field with labels switched off";

  labels->setChecked(true);
  EXPECT_TRUE(field->isEnabled());

  field->setCurrentIndex(field->findData(QStringLiteral("kind")));
  ASSERT_TRUE(dialog.apply());

  EXPECT_TRUE(layer->style()->labels().enabled);
  EXPECT_EQ(layer->style()->labels().fieldName, QStringLiteral("kind"));
}
