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
#include "layers/meshlayer.h"
#include "layers/gdalrasterlayer.h"

#include <gtest/gtest.h>
#include <gdal_priv.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTabWidget>

using namespace HydroCouple::Composer;
using HydroCouple::SDK::IO::MeshDefinition;
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

  // Four now, not three: the 3D tab is deliberately present for every layer
  // -- for one with no 3D form it holds the explanation, because an absent
  // tab is the old silence with better manners (V9).
  EXPECT_EQ(tabs->count(), 4)
    << "a tab was added for a page the layer cannot fill";
  EXPECT_NE(plainDialog.findChild<QWidget *>(QStringLiteral("sceneTab")),
            nullptr);
  EXPECT_NE(plainDialog.findChild<QLabel *>(
              QStringLiteral("sceneAbsenceLabel")),
            nullptr);
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

// ── The 3D tab (coherence plan V9) ────────────────────────────────────────

// A point layer's user checked "Draw this layer", saw nothing in 3D, and
// was told nothing. The tab now exists for every layer, and for one with no
// 3D form the explanation IS the control.
TEST_F(LayerPropertiesTest, APointLayers3dTabExplainsTheAbsence)
{
  const std::unique_ptr<Testing::ProbeFeatureLayer> layer = makeLayer();
  ASSERT_EQ(layer->sceneSource(), nullptr)
    << "the fixture stopped being the case under test";

  LayerPropertiesDialog dialog(layer.get());

  auto *why =
    dialog.findChild<QLabel *>(QStringLiteral("sceneAbsenceLabel"));
  ASSERT_NE(why, nullptr);
  EXPECT_TRUE(why->text().contains(QStringLiteral("point layers")))
    << why->text().toStdString();

  // And no dead controls beside the explanation.
  EXPECT_EQ(dialog.findChild<QComboBox *>(
              QStringLiteral("renderingDrapeCombo")),
            nullptr);
}

TEST_F(LayerPropertiesTest, SceneControlsLiveOnTheDedicated3dTabAndApply)
{
  MeshDefinition mesh;
  mesh.meshName = "surface";
  mesh.nodeX = {0.0, 10.0, 10.0, 0.0};
  mesh.nodeY = {0.0, 0.0, 10.0, 10.0};
  mesh.nodeZ = {0.0, 0.0, 5.0, 5.0};
  mesh.faceNodeOffsets = {0, 3, 6};
  mesh.faceNodes = {0, 1, 2, 0, 2, 3};

  QString message;
  std::unique_ptr<MeshLayer> layer = MeshLayer::create(
    QStringLiteral("surface"), mesh, MeshEntity::Face, message);
  ASSERT_TRUE(layer) << message.toStdString();

  LayerPropertiesDialog dialog(layer.get());

  // The controls live on the 3D tab, and the Rendering tab keeps only what
  // both views read.
  auto *sceneTab = dialog.findChild<QWidget *>(QStringLiteral("sceneTab"));
  ASSERT_NE(sceneTab, nullptr);

  auto *drape =
    dialog.findChild<QComboBox *>(QStringLiteral("renderingDrapeCombo"));
  ASSERT_NE(drape, nullptr);
  EXPECT_TRUE(sceneTab->isAncestorOf(drape))
    << "the drape control is still on the Rendering tab";

  auto *shown =
    dialog.findChild<QCheckBox *>(QStringLiteral("sceneShownIn3dCheck"));
  ASSERT_NE(shown, nullptr);
  ASSERT_TRUE(shown->isChecked());

  shown->setChecked(false);
  ASSERT_TRUE(dialog.apply());

  EXPECT_FALSE(layer->isShownIn3D())
    << "the dialog's 3D toggle did not reach the layer";
  EXPECT_TRUE(layer->isVisible());
}

// The mesh inherited supportsExtrusion() = true from FeatureLayer and
// ignored the setting entirely, so the dialog offered "Extruded above the
// terrain" and a height spin that did nothing.
TEST_F(LayerPropertiesTest, AMeshIsNotOfferedTheExtrusionItWouldIgnore)
{
  MeshDefinition mesh;
  mesh.meshName = "surface";
  mesh.nodeX = {0.0, 10.0, 10.0, 0.0};
  mesh.nodeY = {0.0, 0.0, 10.0, 10.0};
  mesh.nodeZ = {0.0, 0.0, 5.0, 5.0};
  mesh.faceNodeOffsets = {0, 3, 6};
  mesh.faceNodes = {0, 1, 2, 0, 2, 3};

  QString message;
  std::unique_ptr<MeshLayer> layer = MeshLayer::create(
    QStringLiteral("surface"), mesh, MeshEntity::Face, message);
  ASSERT_TRUE(layer) << message.toStdString();

  EXPECT_FALSE(layer->supportsExtrusion());

  LayerPropertiesDialog dialog(layer.get());

  auto *drape =
    dialog.findChild<QComboBox *>(QStringLiteral("renderingDrapeCombo"));
  ASSERT_NE(drape, nullptr);

  for (int i = 0; i < drape->count(); ++i)
  {
    EXPECT_FALSE(drape->itemText(i).contains(QStringLiteral("Extruded")))
      << "the combo offers a placement the mesh ignores";
  }

  EXPECT_EQ(dialog.findChild<QDoubleSpinBox *>(
              QStringLiteral("renderingExtrusionSpin")),
            nullptr);
}

// The terrain consent checkbox lives where heights could come from and
// nowhere else (coherence plan T2).
TEST_F(LayerPropertiesTest, TheTerrainCheckboxAppearsWhereHeightsCouldComeFrom)
{
  MeshDefinition mesh;
  mesh.meshName = "surface";
  mesh.nodeX = {0.0, 10.0, 10.0, 0.0};
  mesh.nodeY = {0.0, 0.0, 10.0, 10.0};
  mesh.nodeZ = {0.0, 0.0, 5.0, 5.0};
  mesh.faceNodeOffsets = {0, 3, 6};
  mesh.faceNodes = {0, 1, 2, 0, 2, 3};

  QString message;
  std::unique_ptr<MeshLayer> layer = MeshLayer::create(
    QStringLiteral("surface"), mesh, MeshEntity::Face, message);
  ASSERT_TRUE(layer) << message.toStdString();

  LayerPropertiesDialog dialog(layer.get());

  auto *consent = dialog.findChild<QCheckBox *>(
    QStringLiteral("sceneTerrainEnabledCheck"));
  ASSERT_NE(consent, nullptr);
  EXPECT_TRUE(consent->isChecked()) << "a surveyed mesh consents by default";

  consent->setChecked(false);
  ASSERT_TRUE(dialog.apply());

  EXPECT_FALSE(layer->terrainEnabled())
    << "the dialog's consent checkbox did not reach the layer";
}

// Flat shading is the mesh's opt-out (coherence plan T3).
TEST_F(LayerPropertiesTest, TheFlatShadingCheckboxReachesTheMesh)
{
  MeshDefinition mesh;
  mesh.meshName = "surface";
  mesh.nodeX = {0.0, 10.0, 10.0, 0.0};
  mesh.nodeY = {0.0, 0.0, 10.0, 10.0};
  mesh.nodeZ = {0.0, 0.0, 5.0, 5.0};
  mesh.faceNodeOffsets = {0, 3, 6};
  mesh.faceNodes = {0, 1, 2, 0, 2, 3};

  QString message;
  std::unique_ptr<MeshLayer> layer = MeshLayer::create(
    QStringLiteral("surface"), mesh, MeshEntity::Face, message);
  ASSERT_TRUE(layer) << message.toStdString();

  LayerPropertiesDialog dialog(layer.get());

  auto *flat = dialog.findChild<QCheckBox *>(
    QStringLiteral("sceneFlatShadingCheck"));
  ASSERT_NE(flat, nullptr);
  EXPECT_FALSE(flat->isChecked()) << "smooth is the default";

  flat->setChecked(true);
  ASSERT_TRUE(dialog.apply());

  EXPECT_TRUE(layer->flatShading());
}

// ── Raster symbology (coherence plan T5) ──────────────────────────────────
//
// setRamp() sat on the layer with no UI caller at all, which is why every
// raster wore Viridis for life.

namespace
{
  QString writeSingleBandRaster(const QString &name)
  {
    const QString path =
      QDir(QStringLiteral(COMPOSER_GIS_FIXTURE_DIR)).filePath(name);

    GDALAllRegister();
    GDALDriver *driver = GetGDALDriverManager()->GetDriverByName("GTiff");

    if (!driver)
    {
      return QString();
    }

    GDALDataset *dataset = driver->Create(path.toUtf8().constData(), 8, 8, 1,
                                          GDT_Float32, nullptr);

    if (!dataset)
    {
      return QString();
    }

    double geotransform[6] = {0.0, 1.0, 0.0, 8.0, 0.0, -1.0};
    dataset->SetGeoTransform(geotransform);

    std::vector<float> values(64);

    for (int i = 0; i < 64; ++i)
    {
      values[size_t(i)] = float(i);
    }

    dataset->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, 8, 8, values.data(),
                                        8, 8, GDT_Float32, 0, 0);
    GDALClose(dataset);

    return path;
  }
}

TEST_F(LayerPropertiesTest, ARasterGetsARampAndAStretchNotSilence)
{
  const QString path =
    writeSingleBandRaster(QStringLiteral("generated-props-band.tif"));
  ASSERT_FALSE(path.isEmpty());

  QString message;
  std::unique_ptr<GdalRasterLayer> raster =
    GdalRasterLayer::open(path, message);
  ASSERT_TRUE(raster) << message.toStdString();

  LayerPropertiesDialog dialog(raster.get());

  auto *ramp =
    dialog.findChild<QComboBox *>(QStringLiteral("rasterRampCombo"));
  auto *minimum =
    dialog.findChild<QDoubleSpinBox *>(QStringLiteral("rasterMinimumSpin"));
  auto *maximum =
    dialog.findChild<QDoubleSpinBox *>(QStringLiteral("rasterMaximumSpin"));

  ASSERT_NE(ramp, nullptr) << "a raster still has no symbology at all";
  ASSERT_NE(minimum, nullptr);
  ASSERT_NE(maximum, nullptr);

  // Loaded from the layer, not defaults.
  EXPECT_EQ(ramp->currentText(), raster->rampName());
  EXPECT_NEAR(minimum->value(), 0.0, 1e-6);
  EXPECT_NEAR(maximum->value(), 63.0, 1e-6);
}

TEST_F(LayerPropertiesTest, ApplyingRampAndStretchReachesTheRaster)
{
  const QString path =
    writeSingleBandRaster(QStringLiteral("generated-props-band2.tif"));
  ASSERT_FALSE(path.isEmpty());

  QString message;
  std::unique_ptr<GdalRasterLayer> raster =
    GdalRasterLayer::open(path, message);
  ASSERT_TRUE(raster) << message.toStdString();

  QSignalSpy repainted(raster.get(), &MapLayer::appearanceChanged);

  LayerPropertiesDialog dialog(raster.get());

  auto *ramp =
    dialog.findChild<QComboBox *>(QStringLiteral("rasterRampCombo"));
  auto *minimum =
    dialog.findChild<QDoubleSpinBox *>(QStringLiteral("rasterMinimumSpin"));
  auto *maximum =
    dialog.findChild<QDoubleSpinBox *>(QStringLiteral("rasterMaximumSpin"));
  ASSERT_NE(ramp, nullptr);

  const QString before = raster->rampName();

  for (int i = 0; i < ramp->count(); ++i)
  {
    if (ramp->itemText(i) != before)
    {
      ramp->setCurrentIndex(i);
      break;
    }
  }

  minimum->setValue(10.0);
  maximum->setValue(50.0);

  ASSERT_TRUE(dialog.apply());

  EXPECT_NE(raster->rampName(), before);

  double low = 0.0;
  double high = 0.0;
  raster->valueRange(low, high);
  EXPECT_NEAR(low, 10.0, 1e-9);
  EXPECT_NEAR(high, 50.0, 1e-9);
  EXPECT_GT(repainted.count(), 0) << "restyling never asked for a repaint";
}

// The stretch announces itself on its own: the previous gate let a ramp
// change's repaint hide a silent stretch.
TEST_F(LayerPropertiesTest, AStretchChangeAloneAsksForARepaint)
{
  const QString path =
    writeSingleBandRaster(QStringLiteral("generated-props-band3.tif"));
  ASSERT_FALSE(path.isEmpty());

  QString message;
  std::unique_ptr<GdalRasterLayer> raster =
    GdalRasterLayer::open(path, message);
  ASSERT_TRUE(raster) << message.toStdString();

  QSignalSpy repainted(raster.get(), &MapLayer::appearanceChanged);

  raster->setValueRange(5.0, 40.0);
  EXPECT_EQ(repainted.count(), 1)
    << "the picture is stretched differently and nothing repaints";

  // And what it already has is not an announcement.
  raster->setValueRange(5.0, 40.0);
  EXPECT_EQ(repainted.count(), 1);
}

// "Ignored unless above the minimum", as the setter documents: an
// upside-down stretch would divide the shading by a non-positive span.
TEST_F(LayerPropertiesTest, AnUpsideDownStretchIsRefused)
{
  const QString path =
    writeSingleBandRaster(QStringLiteral("generated-props-band4.tif"));
  ASSERT_FALSE(path.isEmpty());

  QString message;
  std::unique_ptr<GdalRasterLayer> raster =
    GdalRasterLayer::open(path, message);
  ASSERT_TRUE(raster) << message.toStdString();

  double low = 0.0;
  double high = 0.0;
  raster->valueRange(low, high);

  raster->setValueRange(50.0, 10.0);

  double lowAfter = 0.0;
  double highAfter = 0.0;
  raster->valueRange(lowAfter, highAfter);

  EXPECT_EQ(lowAfter, low);
  EXPECT_EQ(highAfter, high);
}
