/*!
 * \file   screenshot.cpp
 * \brief  Renders the assembled main window to a PNG for visual review.
 *
 * Runs under the offscreen platform so it works headlessly and in CI. It
 * builds a small demo composition from whatever component library it is
 * given, so the capture shows the real canvas, palette and configurator
 * rather than an empty shell.
 *
 * Usage: composer_screenshot <output.png> [component-library-directory]
 *        composer_screenshot <output.png> --map
 *        composer_screenshot <output.png> --preferences [category]
 */

#include "canvas/compositionscene.h"
#include "core/composerapplication.h"
#include "core/preferencesmanager.h"
#include "map/layerstackmodel.h"
#include "map/mapcanvas.h"
#include "map/maplayer.h"
#include "map/maptransform.h"
#include "render/attributeprovider.h"
#include "layers/gdalvectorlayer.h"
#include "layers/layeredmesh.h"
#include "layers/meshlayer.h"
#include "render/layerstyle.h"
#include "scene/sceneview.h"
#include "ui/composermainwindow.h"
#include "ui/dialogs/preferencesdialog.h"
#include "ui/toolbars/ribbonbar.h"
#include "ui/theme/thememanager.h"
#include "ui/panels/layertreepanel.h"

#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QPainter>
#include <QPixmap>
#include <QTabWidget>
#include <QTimer>

#include <QTreeView>

#include <cmath>
#include <iostream>

using namespace HydroCouple::Composer;

namespace
{
  /*!
   * \brief A stand-in layer, so the map capture shows a map.
   *
   * Lives in the capture tool rather than the library on purpose: the real
   * vector, raster and mesh layers arrive with C1d and C2, and shipping a
   * decorative one in the meantime would be a class nothing ever removes.
   */
  class DemoLayer : public MapLayer
  {
    public:
      DemoLayer(const QString &name, const QRectF &extent, const QColor &color,
                bool filled)
        : MapLayer(name), m_extent(extent), m_color(color), m_filled(filled)
      {
      }

      [[nodiscard]] QRectF extent() const override { return m_extent; }

      void render(QPainter &painter, const MapTransform &transform) override
      {
        const QRectF box(transform.toScreen(m_extent.topLeft()),
                         transform.toScreen(m_extent.bottomRight()));

        if (m_filled)
        {
          painter.setPen(QPen(m_color.darker(140), 1.5));
          painter.setBrush(m_color);
          painter.drawRect(box.normalized());
          return;
        }

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(m_color, 2.0));

        // A meandering polyline across the extent — enough to read as
        // geometry rather than as a test pattern.
        QPolygonF line;
        const int steps = 24;

        for (int i = 0; i <= steps; ++i)
        {
          const double t = static_cast<double>(i) / steps;
          const double x = m_extent.left() + t * m_extent.width();
          const double y = m_extent.center().y()
                           + std::sin(t * 6.0) * m_extent.height() * 0.3;
          line << transform.toScreen(QPointF(x, y));
        }

        painter.drawPolyline(line);
      }

    private:
      QRectF m_extent;
      QColor m_color;
      bool m_filled = false;
  };

  /*!
   * \brief A styled point layer, so the capture shows a real legend.
   */
  class DemoPointLayer : public MapLayer, public IAttributeProvider
  {
    public:
      DemoPointLayer() : MapLayer(QStringLiteral("Monitoring points"))
      {
        for (int i = 0; i < 24; ++i)
        {
          const double t = i / 23.0;

          m_points.append(QPointF(80.0 + t * 840.0,
                                  350.0 + std::sin(t * 6.0) * 210.0));
          m_depths.append(0.2 + t * t * 4.8);
        }

        m_style.setMode(StyleMode::Graduated);
        m_style.setAttribute(QStringLiteral("depth"));
        m_style.classification().setMethod(ClassificationMethod::NaturalBreaks);
        m_style.classification().setClassCount(5);
        m_style.classification().setLabelPrecision(1);
        m_style.classification().setRamp(
          ColorRamp::builtin(QStringLiteral("Plasma")));

        Symbol symbol = m_style.symbol();
        symbol.size = 7.0;
        m_style.setSymbol(symbol);

        m_style.rebuild(*this);
      }

      [[nodiscard]] const LayerStyle *style() const override
      {
        return &m_style;
      }

      [[nodiscard]] LayerStyle *style() override { return &m_style; }

      [[nodiscard]] QRectF extent() const override
      {
        QRectF bounds;

        for (const QPointF &point : m_points)
        {
          bounds = bounds.isNull() ? QRectF(point, QSizeF(0.0, 0.0))
                                   : bounds.united(QRectF(point,
                                                          QSizeF(0.0, 0.0)));
        }

        return bounds;
      }

      void render(QPainter &painter, const MapTransform &transform) override
      {
        for (int i = 0; i < m_points.size(); ++i)
        {
          const QColor color = m_style.colorFor(*this, i);

          if (!color.isValid())
          {
            continue;
          }

          painter.setBrush(color);
          painter.setPen(QPen(color.darker(160), 1.0));
          painter.drawEllipse(transform.toScreen(m_points.at(i)),
                              m_style.symbol().size, m_style.symbol().size);
        }
      }

      [[nodiscard]] QVector<AttributeField> attributeFields() const override
      {
        AttributeField depth;
        depth.name = QStringLiteral("depth");
        depth.displayName = QStringLiteral("Depth");
        depth.unit = QStringLiteral("m");

        return {depth};
      }

      [[nodiscard]] int featureCount() const override
      {
        return static_cast<int>(m_points.size());
      }

      [[nodiscard]] QVariant attributeValue(int feature,
                                            const QString &field) const override
      {
        if (field != QStringLiteral("depth") || feature < 0
            || feature >= m_depths.size())
        {
          return {};
        }

        return m_depths.at(feature);
      }

    private:
      QVector<QPointF> m_points;
      QVector<double> m_depths;
      LayerStyle m_style;
  };
}

int main(int argc, char *argv[])
{
  ComposerApplication app(argc, argv);

  if (argc < 2)
  {
    std::cerr << "usage: composer_screenshot <output.png> [library-dir]\n";
    return 2;
  }

  const QString outputPath = QString::fromLocal8Bit(argv[1]);

  // --dark anywhere in the arguments: the chrome icons are recoloured per
  // theme, so both halves of that have to be reviewable.
  bool dark = false;

  for (int index = 2; index < argc; ++index)
  {
    dark = dark || QString::fromLocal8Bit(argv[index]) == QLatin1String("--dark");
  }
  const bool captureMap =
    argc > 2 && QString::fromLocal8Bit(argv[2]) == QLatin1String("--map");

  // --gis <dir>: load the real GeoJSON fixtures through GDAL, so the capture
  // shows imported data rather than a drawing.
  const bool captureGis =
    argc > 3 && QString::fromLocal8Bit(argv[2]) == QLatin1String("--gis");

  // --mesh: a UGRID mesh with per-face values, as a component or the SDK's
  // meshing tools would hand one over.
  const bool captureMesh =
    argc > 2 && QString::fromLocal8Bit(argv[2]) == QLatin1String("--mesh");

  // --layered: the radial mesh given a water column, shown in 3D as the
  // prismatic cells FVQual solves on, coloured by a stratified temperature.
  const bool captureLayered =
    argc > 2 && QString::fromLocal8Bit(argv[2]) == QLatin1String("--layered");

  // --peel <k>: show only layer k of the layered capture.
  int peelLayer = -1;

  for (int index = 2; index + 1 < argc; ++index)
  {
    if (QString::fromLocal8Bit(argv[index]) == QLatin1String("--peel"))
    {
      peelLayer = QString::fromLocal8Bit(argv[index + 1]).toInt();
    }
  }

  // --scene: the same mesh, shown in the 3D view. Its node elevations make
  // the bowl the depth field describes, which the map can only colour.
  const bool captureScene =
    argc > 2 && QString::fromLocal8Bit(argv[2]) == QLatin1String("--scene");

  if (dark)
  {
    // setMode only records the choice; apply() is what repaints.
    ThemeManager::instance()->setMode(ThemeManager::Mode::Dark);
    ThemeManager::instance()->apply();
  }

  // --preferences [category]: the dialog alone, over the real preferences,
  // opened at the named category. A dialog is the one piece of chrome the
  // window capture cannot show, since it is modal and opened on demand.
  if (argc > 2
      && QString::fromLocal8Bit(argv[2]) == QLatin1String("--preferences"))
  {
    PreferencesDialog dialog(PreferencesManager::instance());
    dialog.setCrsChooser([](const QString &current) { return current; });

    if (argc > 3 && !QString::fromLocal8Bit(argv[3]).startsWith(QLatin1String("--")))
    {
      dialog.openAtCategory(QString::fromLocal8Bit(argv[3]));
    }

    dialog.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    const QPixmap capture = dialog.grab();

    if (!capture.save(outputPath))
    {
      std::cerr << "could not write " << outputPath.toStdString() << '\n';
      return 1;
    }

    std::cout << "wrote " << QFileInfo(outputPath).absoluteFilePath().toStdString()
              << " (" << capture.width() << "x" << capture.height() << ")\n";

    return 0;
  }

  ComposerMainWindow window;
  window.resize(1400, 880);
  window.show();

  if (captureMesh || captureScene || captureLayered)
  {
    // A radial mesh: rings of quads around a centre, each face carrying a
    // value that falls off with distance — the shape a depth field takes.
    HydroCouple::SDK::IO::MeshDefinition mesh;
    mesh.meshName = "demo";

    constexpr int kRings = 9;
    constexpr int kSectors = 28;

    // The bed the depth field sits on: deepest at the centre, rising to the
    // rim. The map has nowhere to put this; the 3D view is what it is for.
    const auto bedAt = [](int ring) { return -120.0 * (1.0 - double(ring) / kRings); };

    mesh.nodeX.push_back(0.0);
    mesh.nodeY.push_back(0.0);
    mesh.nodeZ.push_back(bedAt(0));

    for (int ring = 1; ring <= kRings; ++ring)
    {
      for (int sector = 0; sector < kSectors; ++sector)
      {
        const double angle = 2.0 * M_PI * sector / kSectors;
        const double radius = 60.0 * ring;

        mesh.nodeX.push_back(radius * std::cos(angle));
        mesh.nodeY.push_back(radius * std::sin(angle));
        mesh.nodeZ.push_back(bedAt(ring));
      }
    }

    const auto nodeAt = [](int ring, int sector)
    { return 1 + (ring - 1) * kSectors + (sector % kSectors); };

    mesh.faceNodeOffsets.push_back(0);
    QVector<double> values;

    for (int sector = 0; sector < kSectors; ++sector)
    {
      mesh.faceNodes.push_back(0);
      mesh.faceNodes.push_back(nodeAt(1, sector));
      mesh.faceNodes.push_back(nodeAt(1, sector + 1));
      mesh.faceNodeOffsets.push_back(
        static_cast<int64_t>(mesh.faceNodes.size()));
      values.append(10.0);
    }

    for (int ring = 1; ring < kRings; ++ring)
    {
      for (int sector = 0; sector < kSectors; ++sector)
      {
        mesh.faceNodes.push_back(nodeAt(ring, sector));
        mesh.faceNodes.push_back(nodeAt(ring, sector + 1));
        mesh.faceNodes.push_back(nodeAt(ring + 1, sector + 1));
        mesh.faceNodes.push_back(nodeAt(ring + 1, sector));
        mesh.faceNodeOffsets.push_back(
          static_cast<int64_t>(mesh.faceNodes.size()));

        values.append(10.0 * std::exp(-0.28 * ring)
                      * (1.0 + 0.35 * std::sin(3.0 * 2.0 * M_PI * sector
                                               / kSectors)));
      }
    }

    QString message;
    std::unique_ptr<MeshLayer> layer =
      MeshLayer::create(QStringLiteral("Depth (m)"), mesh, MeshEntity::Face,
                        message);

    if (layer)
    {
      layer->setValues(QStringLiteral("depth"), values);

      layer->style()->setMode(StyleMode::Graduated);
      layer->style()->setAttribute(QStringLiteral("depth"));
      layer->style()->classification().setMethod(
        ClassificationMethod::EqualInterval);
      layer->style()->classification().setClassCount(6);
      layer->style()->classification().setLabelPrecision(1);
      layer->style()->classification().setRamp(
        ColorRamp::builtin(QStringLiteral("Viridis")));

      Symbol symbol = layer->style()->symbol();
      symbol.stroke = QColor(255, 255, 255, 90);
      symbol.strokeWidth = 0.5;
      layer->style()->setSymbol(symbol);

      layer->restyle();

      MeshLayer *added = layer.release();
      window.layerStack()->addLayer(added);

      if (captureLayered)
      {
        // Ten sigma layers over the bowl, with a thermocline: warm at the
        // surface, cold at the bed, and the gradient in between. This is the
        // shape a stratified reservoir takes, and the reason the 3D view has
        // to be able to cut into it at all.
        constexpr int kLayers = 10;

        std::vector<double> cfSigma(kLayers + 1);
        for (int k = 0; k <= kLayers; ++k)
          cfSigma[std::size_t(k)] = -double(k) / double(kLayers);

        const std::size_t columns = std::size_t(mesh.faceCount());
        std::vector<double> depth(columns), surface(columns, 0.0);

        for (std::size_t column = 0; column < columns; ++column)
        {
          // Bed depth from the mesh's own node elevations, so the water
          // column follows the bowl the map is showing.
          const std::int64_t from = mesh.faceNodeOffsets[column];
          const std::int64_t to = mesh.faceNodeOffsets[column + 1];

          double bed = 0.0;
          for (std::int64_t slot = from; slot < to; ++slot)
            bed += mesh.nodeZ[std::size_t(mesh.faceNodes[std::size_t(slot)])];

          depth[column] = -bed / double(to - from);
        }

        QString layeredMessage;
        const LayeredMesh layered = LayeredMesh::fromCfSigma(
          mesh, cfSigma, depth, surface, layeredMessage);

        if (added->setLayering(layered, layeredMessage))
        {
          QVector<double> temperature;
          temperature.reserve(int(layered.cellCount()));

          for (std::size_t column = 0; column < columns; ++column)
          {
            for (int k = 0; k < kLayers; ++k)
            {
              // A thermocline centred a third of the way down.
              const double fraction = (double(k) + 0.5) / double(kLayers);
              temperature.append(
                6.0 + 18.0 / (1.0 + std::exp((fraction - 0.33) * 14.0)));
            }
          }

          added->setLayeredValues(QStringLiteral("temperature"), temperature);
          added->setName(QObject::tr("Temperature (°C)"));

          added->style()->setAttribute(QStringLiteral("temperature"));
          added->style()->classification().classify(temperature);
          added->style()->classification().setRamp(
            ColorRamp::builtin(QStringLiteral("Inferno")));

          if (peelLayer >= 0)
            added->setVisibleLayers(peelLayer, peelLayer);
        }
        else
        {
          std::cerr << layeredMessage.toStdString() << '\n';
        }
      }
    }
    else
    {
      std::cerr << message.toStdString() << '\n';
    }

    if (auto *tabs =
          window.findChild<QTabWidget *>(QStringLiteral("workspaceTabs")))
    {
      tabs->setCurrentWidget((captureScene || captureLayered)
                               ? static_cast<QWidget *>(window.sceneView())
                               : static_cast<QWidget *>(window.mapCanvas()));
    }

    if ((captureScene || captureLayered) && window.sceneView())
    {
      Camera camera = window.sceneView()->camera();
      camera.setElevation(captureLayered ? 20.0 : 32.0);
      camera.setAzimuth(35.0);
      camera.setVerticalExaggeration(captureLayered ? 6.0 : 2.5);
      window.sceneView()->setCamera(camera);
      window.sceneView()->zoomToFullExtent();
    }

    if (auto *dock =
          window.findChild<QDockWidget *>(QStringLiteral("layerDock")))
    {
      dock->raise();
    }

    window.mapCanvas()->zoomToFullExtent();
    window.layerTree()->view()->expandAll();
    window.layerTree()->view()->setCurrentIndex(
      window.layerStack()->index(0, 0));
  }
  else if (captureGis)
  {
    const QDir fixtures(QString::fromLocal8Bit(argv[3]));

    struct Import
    {
        const char *file;
        StyleMode mode;
        const char *attribute;
        const char *ramp;
    };

    const Import imports[] = {
      {"catchments.geojson", StyleMode::Graduated, "area", "Blues"},
      {"conduits.geojson", StyleMode::Graduated, "flow", "Plasma"},
      {"network.geojson", StyleMode::Categorized, "kind", "Viridis"}};

    for (const Import &import : imports)
    {
      QString message;
      std::unique_ptr<GdalVectorLayer> layer = GdalVectorLayer::open(
        fixtures.filePath(QString::fromLatin1(import.file)), message);

      if (!layer)
      {
        std::cerr << message.toStdString() << '\n';
        continue;
      }

      layer->style()->setMode(import.mode);
      layer->style()->setAttribute(QString::fromLatin1(import.attribute));
      layer->style()->classification().setClassCount(3);
      layer->style()->classification().setLabelPrecision(1);
      layer->style()->classification().setRamp(
        ColorRamp::builtin(QString::fromLatin1(import.ramp)));

      layer->style()->labels().enabled = true;
      layer->style()->labels().fieldName = QStringLiteral("name");

      Symbol symbol = layer->style()->symbol();
      symbol.size = 11.0;
      symbol.strokeWidth = 2.0;
      layer->style()->setSymbol(symbol);

      layer->restyle();
      window.layerStack()->addLayer(layer.release());
    }

    if (auto *tabs =
          window.findChild<QTabWidget *>(QStringLiteral("workspaceTabs")))
    {
      tabs->setCurrentWidget(window.mapCanvas());
    }

    if (auto *dock =
          window.findChild<QDockWidget *>(QStringLiteral("layerDock")))
    {
      dock->raise();
    }

    window.mapCanvas()->zoomToFullExtent();
    window.layerTree()->view()->expandAll();
    window.layerTree()->view()->setCurrentIndex(
      window.layerStack()->index(0, 0));
  }
  else if (captureMap)
  {
    window.layerStack()->addLayer(new DemoLayer(
      QStringLiteral("Catchment"), QRectF(0.0, 0.0, 1000.0, 700.0),
      QColor(0x0A, 0x66, 0xC2, 0x33), true));
    window.layerStack()->addLayer(new DemoLayer(
      QStringLiteral("Reach"), QRectF(60.0, 100.0, 880.0, 500.0),
      QColor(0x1F, 0x77, 0xB4), false));
    window.layerStack()->addLayer(new DemoPointLayer());

    window.mapCanvas()->zoomToFullExtent();

    if (auto *tabs =
          window.findChild<QTabWidget *>(QStringLiteral("workspaceTabs")))
    {
      tabs->setCurrentWidget(window.mapCanvas());
    }

    if (auto *dock = window.findChild<QDockWidget *>(QStringLiteral("layerDock")))
    {
      dock->raise();
    }

    window.layerTree()->view()->setCurrentIndex(
      window.layerStack()->index(0, 0));

    // Expanded, so the capture shows the legend the style produced rather
    // than a collapsed row that hides it.
    window.layerTree()->view()->expandAll();
  }
  else if (argc > 2)
  {
    const QString directory = QString::fromLocal8Bit(argv[2]);
    const int loaded = window.loadComponentDirectory(directory);
    window.log(QStringLiteral("Loaded %1 component library(ies).").arg(loaded));

    // A small demo composition so the capture shows real nodes and an edge.
    CompositionScene *scene = window.canvas()->compositionScene();

    const QString upstream = scene->addComponentAt(
      QStringLiteral("composer.test.component"), QStringLiteral("upstream"),
      QPointF(-260.0, -60.0));
    const QString downstream = scene->addComponentAt(
      QStringLiteral("composer.test.component"), QStringLiteral("downstream"),
      QPointF(60.0, 40.0));

    if (!upstream.isEmpty() && !downstream.isEmpty())
    {
      scene->connectPorts(upstream, QStringLiteral("values"), downstream,
                          QStringLiteral("inflow"));

      if (ComponentNodeItem *node = scene->node(downstream))
      {
        node->setSelected(true);
      }
    }

    window.canvas()->fitInView(scene->itemsBoundingRect().adjusted(-60, -60, 60, 60),
                               Qt::KeepAspectRatio);
  }

  if (captureMap || captureGis || captureMesh || captureScene ||
      captureLayered)
  {
    if (auto *ribbon = window.findChild<RibbonBar *>(QStringLiteral("ribbonBar")))
    {
      ribbon->setCurrentTab(QStringLiteral("map"));
    }
  }

  // Let layout and the queued selection handling settle before capturing.
  QCoreApplication::processEvents();
  QCoreApplication::processEvents();

  const QPixmap capture = window.grab();

  if (!capture.save(outputPath))
  {
    std::cerr << "could not write " << outputPath.toStdString() << '\n';
    return 1;
  }

  std::cout << "wrote " << QFileInfo(outputPath).absoluteFilePath().toStdString()
            << " (" << capture.width() << "x" << capture.height() << ")\n";

  return 0;
}
