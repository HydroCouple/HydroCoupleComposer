/*!
 * \file layerrestorer.cpp
 * \brief LayerRestorer implementation.
 */

#include "layers/layerrestorer.h"

#include "layers/gdalrasterlayer.h"
#include "layers/meshlayer.h"
#include "layers/gdalvectorlayer.h"
#include "layers/ogctilesource.h"
#include "layers/tilelayer.h"
#include "layers/wcscoveragelayer.h"
#include "layers/wfsfeaturelayer.h"

#include <hydrocoupleogc/servicediscovery.h>
#include <hydrocoupleogc/wcsrequest.h>
#include <hydrocoupleogc/wfsrequest.h>
#include <hydrocoupleogc/wmscapabilities.h>
#include <hydrocoupleogc/wmtscapabilities.h>

#include <QFileInfo>
#include <QJsonValue>
#include <QUrl>

namespace HydroCouple::Composer
{
  namespace
  {
    QString typeOf(const QJsonObject &entry)
    {
      return entry.value(QStringLiteral("type")).toString();
    }

    QString textOf(const QJsonObject &entry, const QString &key)
    {
      return entry.value(key).toString();
    }

    QStringList listOf(const QJsonObject &entry, const QString &key)
    {
      QStringList values;

      for (const QJsonValue &value : entry.value(key).toArray())
      {
        values.append(value.toString());
      }

      return values;
    }

    /*!
     * \brief The ground a fetched layer was asked for over.
     *
     * Saved because a feature service holds a country: reopening without it
     * would quietly return a different, arbitrary few thousand features
     * from wherever the service starts counting.
     */
    QRectF extentOf(const QJsonObject &entry)
    {
      const QJsonArray box = entry.value(QStringLiteral("extent")).toArray();

      if (box.size() != 4)
      {
        return {};
      }

      return QRectF(QPointF(box.at(0).toDouble(), box.at(1).toDouble()),
                    QPointF(box.at(2).toDouble(), box.at(3).toDouble()))
        .normalized();
    }

    //! Puts back what every layer carries, whatever kind it is.
    void applyCommon(MapLayer *layer, const QJsonObject &entry)
    {
      if (!layer)
      {
        return;
      }

      const QString name = textOf(entry, QStringLiteral("name"));

      if (!name.isEmpty())
      {
        layer->setName(name);
      }

      if (entry.contains(QStringLiteral("visible")))
      {
        layer->setVisible(entry.value(QStringLiteral("visible")).toBool(true));
      }

      if (entry.contains(QStringLiteral("opacity")))
      {
        layer->setOpacity(
          entry.value(QStringLiteral("opacity")).toDouble(1.0));
      }

      // Every fallback is the value the factory just gave the layer, so a
      // missing key -- an old composition, a hand-edited file -- keeps what
      // it has rather than being written back to a hard-coded default that
      // could drift from the real one. It also means no emptiness guard: an
      // absent block is just a block whose every key is missing.
      const QJsonObject sceneState =
        entry.value(QStringLiteral("scene")).toObject();

      layer->setShownIn3D(sceneState.value(QStringLiteral("shownIn3D"))
                            .toBool(layer->isShownIn3D()));

      if (ISceneSource *scene = layer->sceneSource())
      {
        scene->setTerrainEnabled(
          sceneState.value(QStringLiteral("terrainEnabled"))
            .toBool(scene->terrainEnabled()));

        ZPolicy policy = scene->zPolicy();
        policy.mode = static_cast<ZMode>(
          sceneState.value(QStringLiteral("zMode"))
            .toInt(static_cast<int>(policy.mode)));
        policy.constant = sceneState.value(QStringLiteral("constant"))
                            .toDouble(policy.constant);
        policy.field =
          sceneState.value(QStringLiteral("field")).toString(policy.field);
        policy.offset = sceneState.value(QStringLiteral("offset"))
                          .toDouble(policy.offset);

        scene->setZPolicy(policy);
        scene->setExtrusionHeight(
          sceneState.value(QStringLiteral("extrusion"))
            .toDouble(scene->extrusionHeight()));
      }

      if (auto *mesh = dynamic_cast<MeshLayer *>(layer))
      {
        mesh->setFlatShading(sceneState.value(QStringLiteral("flatShading"))
                               .toBool(mesh->flatShading()));
      }

      const QJsonObject shading =
        entry.value(QStringLiteral("shading")).toObject();

      if (auto *raster = dynamic_cast<GdalRasterLayer *>(layer))
      {
        raster->setRampName(shading.value(QStringLiteral("ramp"))
                              .toString(raster->rampName()));

        double low = 0.0;
        double high = 1.0;
        raster->valueRange(low, high);
        raster->setValueRange(
          shading.value(QStringLiteral("stretchFrom")).toDouble(low),
          shading.value(QStringLiteral("stretchTo")).toDouble(high));
      }

      // The recipe travels with the layer, so a composition saved, reopened
      // and saved again writes the same entry rather than losing it.
      layer->setPersistentState(entry);
    }
  } // namespace

  LayerRestorer::LayerRestorer(QObject *parent) : QObject(parent)
  {
    m_client = new HydroCouple::Ogc::HttpClient(this);
  }

  LayerRestorer::~LayerRestorer() = default;

  int LayerRestorer::pendingCount() const
  {
    return m_pending;
  }

  void LayerRestorer::restore(const QJsonArray &layers,
                              const LayerReady &onLayer,
                              const LayerFailed &onFailed)
  {
    for (const QJsonValue &value : layers)
    {
      if (value.isObject())
      {
        restoreOne(value.toObject(), onLayer, onFailed);
      }
    }
  }

  void LayerRestorer::restoreOne(const QJsonObject &entry,
                                 const LayerReady &onLayer,
                                 const LayerFailed &onFailed)
  {
    const QString type = typeOf(entry);

    if (type.isEmpty())
    {
      return;
    }

    if (type == QLatin1String("gdal-raster")
        || type == QLatin1String("gdal-vector"))
    {
      QString message;

      if (MapLayer *layer = restoreFile(entry, message))
      {
        applyCommon(layer, entry);
        onLayer(layer);
      }
      else
      {
        onFailed(message);
      }

      return;
    }

    restoreService(entry, onLayer, onFailed);
  }

  MapLayer *LayerRestorer::restoreFile(const QJsonObject &entry,
                                       QString &message) const
  {
    const QString path = textOf(entry, QStringLiteral("path"));

    if (path.isEmpty())
    {
      message = tr("A saved layer does not say what file it came from.");

      return nullptr;
    }

    if (!QFileInfo::exists(path))
    {
      // Named, because "a layer could not be restored" sends the user
      // looking in the wrong place. Files move between machines far more
      // often than they are deleted.
      message = tr("%1 is no longer where this composition left it: %2")
                  .arg(textOf(entry, QStringLiteral("name")), path);

      return nullptr;
    }

    if (typeOf(entry) == QLatin1String("gdal-raster"))
    {
      return GdalRasterLayer::open(path, message).release();
    }

    return GdalVectorLayer::open(path, message,
                                 textOf(entry, QStringLiteral("layer")))
      .release();
  }

  void LayerRestorer::restoreService(const QJsonObject &entry,
                                     const LayerReady &onLayer,
                                     const LayerFailed &onFailed)
  {
    const QString type = typeOf(entry);
    const QString service = textOf(entry, QStringLiteral("url"));
    const QString name = textOf(entry, QStringLiteral("name"));

    if (service.isEmpty())
    {
      onFailed(tr("A saved layer does not say what service it came from."));

      return;
    }

    // A tile template is the whole recipe: there is no capabilities
    // document behind it, so this is the one saved layer that is rebuilt
    // without asking anything of the network.
    if (type == QLatin1String("xyz"))
    {
      auto source = std::make_unique<XyzTileSource>(service);

      if (!source->isUsable())
      {
        onFailed(tr("%1: %2").arg(name, source->reason()));

        return;
      }

      source->setAttribution(name);

      auto *layer = new TileLayer(name, std::move(source));
      applyCommon(layer, entry);
      onLayer(layer);

      return;
    }

    HydroCouple::Ogc::ServiceKind kind =
      HydroCouple::Ogc::ServiceKind::Unknown;

    if (type == QLatin1String("wms"))
    {
      kind = HydroCouple::Ogc::ServiceKind::Wms;
    }
    else if (type == QLatin1String("wmts"))
    {
      kind = HydroCouple::Ogc::ServiceKind::Wmts;
    }
    else if (type == QLatin1String("wfs"))
    {
      kind = HydroCouple::Ogc::ServiceKind::Wfs;
    }
    else if (type == QLatin1String("wcs"))
    {
      kind = HydroCouple::Ogc::ServiceKind::Wcs;
    }
    else
    {
      onFailed(tr("%1 was saved as a kind of layer this version does not "
                  "know: %2.").arg(name, type));

      return;
    }

    const QString url = HydroCouple::Ogc::buildCapabilitiesUrl(service, kind);

    if (url.isEmpty())
    {
      onFailed(tr("%1 was saved with an address that is not one: %2")
                 .arg(name, service));

      return;
    }

    ++m_pending;

    // No credentials. A service that has since started asking for them will
    // refuse, and the layer is reported rather than silently missing -- which
    // is the honest outcome, because the password was deliberately never
    // written into the project file.
    m_client->get(
      QUrl(url), {},
      [this, entry, type, name, service, onLayer,
       onFailed](const HydroCouple::Ogc::HttpResponse &response) {
        --m_pending;

        MapLayer *layer = nullptr;
        QString message;

        if (type == QLatin1String("wms"))
        {
          const HydroCouple::Ogc::WmsCapabilities capabilities =
            HydroCouple::Ogc::parseWmsCapabilities(response.body);

          if (capabilities.ok)
          {
            auto source = std::make_unique<WmsTileSource>(
              capabilities, listOf(entry, QStringLiteral("layers")),
              textOf(entry, QStringLiteral("format")),
              textOf(entry, QStringLiteral("style")));

            if (source->isUsable())
            {
              layer = new TileLayer(name, std::move(source));
            }
            else
            {
              message = source->reason();
            }
          }
          else
          {
            message = capabilities.message;
          }
        }
        else if (type == QLatin1String("wmts"))
        {
          const HydroCouple::Ogc::WmtsCapabilities capabilities =
            HydroCouple::Ogc::parseWmtsCapabilities(response.body);

          if (capabilities.ok)
          {
            auto source = std::make_unique<WmtsTileSource>(
              capabilities, textOf(entry, QStringLiteral("layer")),
              textOf(entry, QStringLiteral("matrixSet")),
              textOf(entry, QStringLiteral("style")),
              textOf(entry, QStringLiteral("format")));

            if (source->isUsable())
            {
              layer = new TileLayer(name, std::move(source));
            }
            else
            {
              message = source->reason();
            }
          }
          else
          {
            message = capabilities.message;
          }
        }
        else if (type == QLatin1String("wfs"))
        {
          const HydroCouple::Ogc::WfsCapabilities capabilities =
            HydroCouple::Ogc::parseWfsCapabilities(response.body);

          if (!capabilities.ok)
          {
            onFailed(tr("%1: %2").arg(name, capabilities.message));

            return;
          }

          // A second request, because what was saved is which collection
          // over which ground, not the features themselves. Storing those
          // would put a country's buildings in a project file.
          fetchFeatures(capabilities, entry, onLayer, onFailed);

          return;
        }
        else if (type == QLatin1String("wcs"))
        {
          const HydroCouple::Ogc::WcsCapabilities capabilities =
            HydroCouple::Ogc::parseWcsCapabilities(response.body);

          if (!capabilities.ok)
          {
            onFailed(tr("%1: %2").arg(name, capabilities.message));

            return;
          }

          // Two more requests. A coverage cannot be asked for until it has
          // been described, because its axis names are its own.
          describeThenFetchCoverage(capabilities, entry, onLayer, onFailed);

          return;
        }

        if (layer)
        {
          applyCommon(layer, entry);
          onLayer(layer);

          return;
        }

        onFailed(message.isEmpty()
                   ? tr("%1 could not be rebuilt from %2.").arg(name, service)
                   : tr("%1: %2").arg(name, message));
      });
  }

  void LayerRestorer::fetchFeatures(
    const HydroCouple::Ogc::WfsCapabilities &capabilities,
    const QJsonObject &entry, const LayerReady &onLayer,
    const LayerFailed &onFailed)
  {
    const QString name = textOf(entry, QStringLiteral("name"));
    const QString typeName = textOf(entry, QStringLiteral("typeName"));

    const HydroCouple::Ogc::WfsFeatureType *featureType =
      capabilities.featureType(typeName);

    if (!featureType)
    {
      onFailed(tr("%1 is no longer published by that service.").arg(name));

      return;
    }

    HydroCouple::Ogc::WfsGetFeatureRequest request;
    request.typeName = featureType->name;
    request.outputFormat = HydroCouple::Ogc::preferredOutputFormat(
      *featureType, capabilities.outputFormats);
    request.crs = featureType->spellingOf(QStringLiteral("EPSG:4326"));
    request.extent = extentOf(entry);

    const QString url =
      HydroCouple::Ogc::buildGetFeatureUrl(capabilities, request);

    if (url.isEmpty())
    {
      onFailed(tr("%1 cannot be asked for any more.").arg(name));

      return;
    }

    ++m_pending;

    m_client->get(QUrl(url), {},
                  [this, entry, name, onLayer,
                   onFailed](const HydroCouple::Ogc::HttpResponse &response) {
                    --m_pending;

                    QString message;

                    std::unique_ptr<WfsFeatureLayer> layer =
                      WfsFeatureLayer::fromResponse(response.body, name,
                                                    message);

                    if (!layer)
                    {
                      onFailed(tr("%1: %2").arg(
                        name, message.isEmpty() ? response.error : message));

                      return;
                    }

                    applyCommon(layer.get(), entry);
                    onLayer(layer.release());
                  });
  }

  void LayerRestorer::describeThenFetchCoverage(
    const HydroCouple::Ogc::WcsCapabilities &capabilities,
    const QJsonObject &entry, const LayerReady &onLayer,
    const LayerFailed &onFailed)
  {
    const QString name = textOf(entry, QStringLiteral("name"));
    const QString service = textOf(entry, QStringLiteral("url"));
    const QString coverageId = textOf(entry, QStringLiteral("coverageId"));

    if (!capabilities.coverage(coverageId))
    {
      onFailed(tr("%1 is no longer published by that service.").arg(name));

      return;
    }

    const QString describeUrl = HydroCouple::Ogc::buildDescribeCoverageUrl(
      service, capabilities.version, coverageId);

    if (describeUrl.isEmpty())
    {
      onFailed(tr("%1 cannot be asked about any more.").arg(name));

      return;
    }

    ++m_pending;

    m_client->get(
      QUrl(describeUrl), {},
      [this, capabilities, entry, name, service, coverageId, onLayer,
       onFailed](const HydroCouple::Ogc::HttpResponse &response) {
        --m_pending;

        const HydroCouple::Ogc::WcsCoverageDescription description =
          HydroCouple::Ogc::parseWcsCoverageDescription(response.body);

        if (!description.ok)
        {
          onFailed(tr("%1: %2").arg(name, description.message.isEmpty()
                                            ? response.error
                                            : description.message));

          return;
        }

        HydroCouple::Ogc::WcsGetCoverageRequest request;
        request.coverageId = coverageId;
        request.extent = extentOf(entry);

        if (request.extent.isNull())
        {
          request.extent = description.boundsAsRect();
        }

        request.size = QSize(entry.value(QStringLiteral("width")).toInt(1024),
                             entry.value(QStringLiteral("height")).toInt(1024));

        const QString url = HydroCouple::Ogc::buildGetCoverageUrl(
          service, capabilities.version, request, description);

        if (url.isEmpty())
        {
          onFailed(tr("%1 cannot be asked for any more.").arg(name));

          return;
        }

        ++m_pending;

        m_client->get(
          QUrl(url), {},
          [this, entry, name, service, coverageId, description, onLayer,
           onFailed](const HydroCouple::Ogc::HttpResponse &coverage) {
            --m_pending;

            QString message;

            std::unique_ptr<WcsCoverageLayer> layer =
              WcsCoverageLayer::fromResponse(coverage.body, name, message);

            if (!layer)
            {
              onFailed(tr("%1: %2").arg(
                name, message.isEmpty() ? coverage.error : message));

              return;
            }

            layer->setServiceUrl(service);
            layer->setCoverageId(coverageId);
            layer->setDescription(description);

            applyCommon(layer.get(), entry);
            onLayer(layer.release());
          });
      });
  }

} // namespace HydroCouple::Composer
