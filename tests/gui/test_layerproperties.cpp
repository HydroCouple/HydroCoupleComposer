/*!
 * \file   test_layerproperties.cpp
 * \brief  Phase C1c + C5a verification — the layer properties editor.
 *
 * The dialog edits the layer's own objects rather than copies, so these tests
 * check the layer, not the form: a dialog that faithfully recorded every
 * choice into a copy nobody reads would look identical from the inside and
 * change nothing on the map.
 *
 * C5a's addition is that the dialog opens for *every* layer and drops the tabs
 * a layer cannot fill, where its predecessor refused to open at all — so the
 * gates below are as much about which tabs exist as about what they write.
 */

#include "core/composerapplication.h"
#include "map/maplayer.h"
#include "probelayer.h"
#include "render/layerstyle.h"
#include "ui/dialogs/layerpropertiesdialog.h"

#include <gtest/gtest.h>

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTabWidget>

using namespace HydroCouple::Composer;
namespace Testing = HydroCouple::Composer::Testing;

namespace
{
  /*!
   * \brief A style-less layer that credits a provider, like a basemap.
   */
  class CreditedProbeLayer : public Testing::ProbeLayer
  {
    public:
      CreditedProbeLayer(const QString &name, const QRectF &extent)
        : ProbeLayer(name, extent)
      {
        // Recorded through the base's protected setter, which is how the
        // real layers do it — overriding the accessor instead would leave
        // that storage untested and a mutation to it invisible.
        setSourceDescription(
          QStringLiteral("https://tiles.example/{z}/{x}/{y}.png"));
      }

      [[nodiscard]] QString attribution() const override
      {
        return QStringLiteral("© Probe Tiles");
      }
  };

  class LayerPropertiesTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_layerproperties";
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

  ComposerApplication *LayerPropertiesTest::s_app = nullptr;
}

TEST_F(LayerPropertiesTest, OpensForEveryLayerAndOmitsTheTabsItCannotFill)
{
  const std::unique_ptr<Testing::ProbeFeatureLayer> styled = makeLayer();
  Testing::ProbeLayer plain(QStringLiteral("plain"),
                            QRectF(0.0, 0.0, 1.0, 1.0));

  EXPECT_TRUE(LayerPropertiesDialog::canEdit(styled.get()));
  EXPECT_TRUE(LayerPropertiesDialog::canEdit(&plain))
    << "a layer with no style was refused a properties dialog, which is what "
       "C5a exists to stop";
  EXPECT_FALSE(LayerPropertiesDialog::canEdit(nullptr));

  LayerPropertiesDialog styledDialog(styled.get());

  EXPECT_NE(styledDialog.findChild<QWidget *>(QStringLiteral("symbologyTab")),
            nullptr);
  EXPECT_NE(styledDialog.findChild<QWidget *>(QStringLiteral("labelsTab")),
            nullptr);

  LayerPropertiesDialog plainDialog(&plain);

  // The tabs every layer can answer for are there …
  EXPECT_NE(plainDialog.findChild<QWidget *>(QStringLiteral("informationTab")),
            nullptr);
  EXPECT_NE(plainDialog.findChild<QWidget *>(QStringLiteral("sourceTab")),
            nullptr);
  EXPECT_NE(plainDialog.findChild<QWidget *>(QStringLiteral("renderingTab")),
            nullptr);

  // … and the ones it cannot are absent rather than present and empty, which
  // is the failure the old canStyle() gate traded for no dialog at all.
  EXPECT_EQ(plainDialog.findChild<QWidget *>(QStringLiteral("symbologyTab")),
            nullptr);
  EXPECT_EQ(plainDialog.findChild<QWidget *>(QStringLiteral("labelsTab")),
            nullptr);

  auto *tabs =
    plainDialog.findChild<QTabWidget *>(QStringLiteral("layerPropertiesTabs"));
  ASSERT_NE(tabs, nullptr);
  EXPECT_EQ(tabs->count(), 3)
    << "a tab was added for a page the layer cannot fill";
}

TEST_F(LayerPropertiesTest, ListsTheLayersOwnAttributesWithUnits)
{
  const std::unique_ptr<Testing::ProbeFeatureLayer> layer = makeLayer();
  LayerPropertiesDialog dialog(layer.get());

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

TEST_F(LayerPropertiesTest, WritesTheFormOntoTheLayersOwnStyle)
{
  const std::unique_ptr<Testing::ProbeFeatureLayer> layer = makeLayer();
  LayerPropertiesDialog dialog(layer.get());

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

TEST_F(LayerPropertiesTest, ReopeningShowsWhatWasApplied)
{
  const std::unique_ptr<Testing::ProbeFeatureLayer> layer = makeLayer();

  layer->styleRef().setMode(StyleMode::Categorized);
  layer->styleRef().setAttribute(QStringLiteral("kind"));
  layer->styleRef().classification().setClassCount(7);
  ASSERT_TRUE(layer->restyle());

  LayerPropertiesDialog dialog(layer.get());

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

TEST_F(LayerPropertiesTest, DisablesControlsThatDoNotApplyToTheChosenMode)
{
  const std::unique_ptr<Testing::ProbeFeatureLayer> layer = makeLayer();
  LayerPropertiesDialog dialog(layer.get());

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

TEST_F(LayerPropertiesTest, SaysSoWhenTheChosenStyleCanDrawNothing)
{
  auto layer =
    std::make_unique<Testing::ProbeFeatureLayer>(QStringLiteral("empty"));

  AttributeField text;
  text.name = QStringLiteral("label");
  text.type = QMetaType::QString;
  layer->declareField(text);

  layer->addFeature(QPointF(0.0, 0.0),
                    {{QStringLiteral("label"), QStringLiteral("not a number")}});

  LayerPropertiesDialog dialog(layer.get());

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

TEST_F(LayerPropertiesTest, TurnsLabellingOnForTheChosenField)
{
  const std::unique_ptr<Testing::ProbeFeatureLayer> layer = makeLayer();
  LayerPropertiesDialog dialog(layer.get());

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

TEST_F(LayerPropertiesTest, AppliesIdentityAndRenderingToAStyllessLayer)
{
  Testing::ProbeLayer plain(QStringLiteral("plain"),
                            QRectF(0.0, 0.0, 1.0, 1.0));
  plain.setOpacity(1.0);

  LayerPropertiesDialog dialog(&plain);

  QSignalSpy appearanceSpy(&plain, &MapLayer::appearanceChanged);

  auto *name = dialog.findChild<QLineEdit *>(QStringLiteral("layerNameEdit"));
  auto *visible =
    dialog.findChild<QCheckBox *>(QStringLiteral("renderingVisibleCheck"));
  auto *opacity =
    dialog.findChild<QSpinBox *>(QStringLiteral("renderingOpacitySpin"));
  ASSERT_NE(name, nullptr);
  ASSERT_NE(visible, nullptr);
  ASSERT_NE(opacity, nullptr);

  // Loaded from the layer, not from a default: a form that always opened on
  // "visible, 100%" would apply those values to a layer the user had hidden.
  EXPECT_EQ(name->text(), QStringLiteral("plain"));
  EXPECT_TRUE(visible->isChecked());
  EXPECT_EQ(opacity->value(), 100);

  name->setText(QStringLiteral("renamed"));
  visible->setChecked(false);
  opacity->setValue(40);

  // True, not false: nothing failed. Its predecessor returned false here
  // simply because the layer had no style to rebuild.
  EXPECT_TRUE(dialog.apply());

  EXPECT_EQ(plain.name(), QStringLiteral("renamed"));
  EXPECT_FALSE(plain.isVisible());
  EXPECT_NEAR(plain.opacity(), 0.4, 1.0e-9);

  EXPECT_GE(appearanceSpy.count(), 1)
    << "the map was never told, so the change would sit invisible";
}

TEST_F(LayerPropertiesTest, ReportsWhereTheLayerCameFrom)
{
  const CreditedProbeLayer credited(QStringLiteral("basemap"),
                                    QRectF(0.0, 0.0, 1.0, 1.0));
  LayerPropertiesDialog creditedDialog(
    const_cast<CreditedProbeLayer *>(&credited));

  auto *source =
    creditedDialog.findChild<QLabel *>(QStringLiteral("layerSourceLabel"));
  ASSERT_NE(source, nullptr);
  EXPECT_EQ(source->text(),
            QStringLiteral("https://tiles.example/{z}/{x}/{y}.png"));

  // A layer built in memory says so, rather than showing an empty row that
  // reads as a missing value.
  Testing::ProbeLayer plain(QStringLiteral("plain"),
                            QRectF(0.0, 0.0, 1.0, 1.0));
  LayerPropertiesDialog plainDialog(&plain);

  auto *plainSource =
    plainDialog.findChild<QLabel *>(QStringLiteral("layerSourceLabel"));
  ASSERT_NE(plainSource, nullptr);
  EXPECT_FALSE(plainSource->text().isEmpty());
  EXPECT_FALSE(plainSource->text().contains(QStringLiteral("://")));
}

TEST_F(LayerPropertiesTest, InformationCountsFeaturesAndReportsTheExtent)
{
  const std::unique_ptr<Testing::ProbeFeatureLayer> layer = makeLayer();
  LayerPropertiesDialog dialog(layer.get());

  auto *count = dialog.findChild<QLabel *>(
    QStringLiteral("informationFeatureCountLabel"));
  ASSERT_NE(count, nullptr);
  EXPECT_EQ(count->text(), QStringLiteral("12"));

  auto *extent =
    dialog.findChild<QLabel *>(QStringLiteral("informationExtentLabel"));
  ASSERT_NE(extent, nullptr);

  // The features run from (0,0) to (11,11), so the extent has to mention 11 —
  // a label that printed the layer's name or four zeroes would pass a mere
  // "not empty" check.
  EXPECT_TRUE(extent->text().contains(QStringLiteral("11")))
    << "extent shown as: " << extent->text().toStdString();

  // A layer with no features at all has no extent, and says so.
  const Testing::ProbeLayer empty(QStringLiteral("empty"), QRectF());
  LayerPropertiesDialog emptyDialog(
    const_cast<Testing::ProbeLayer *>(&empty));

  auto *emptyExtent =
    emptyDialog.findChild<QLabel *>(QStringLiteral("informationExtentLabel"));
  ASSERT_NE(emptyExtent, nullptr);
  EXPECT_FALSE(emptyExtent->text().contains(QStringLiteral("0.000")));
}

TEST_F(LayerPropertiesTest, MetadataAppearsOnlyForALayerThatCreditsSomeone)
{
  const CreditedProbeLayer credited(QStringLiteral("basemap"),
                                    QRectF(0.0, 0.0, 1.0, 1.0));
  LayerPropertiesDialog creditedDialog(
    const_cast<CreditedProbeLayer *>(&credited));

  auto *attribution = creditedDialog.findChild<QLabel *>(
    QStringLiteral("metadataAttributionLabel"));
  ASSERT_NE(attribution, nullptr);
  EXPECT_EQ(attribution->text(), QStringLiteral("© Probe Tiles"));

  Testing::ProbeLayer plain(QStringLiteral("plain"),
                            QRectF(0.0, 0.0, 1.0, 1.0));
  LayerPropertiesDialog plainDialog(&plain);

  EXPECT_EQ(plainDialog.findChild<QWidget *>(QStringLiteral("metadataTab")),
            nullptr)
    << "an empty Metadata tab on a layer that credits nobody";
}
