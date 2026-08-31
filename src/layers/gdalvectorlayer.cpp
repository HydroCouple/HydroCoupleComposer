#include "layers/gdalvectorlayer.h"

#include "gis/spatialreference.h"
#include "layers/ogrfeatureloader.h"

#include <QFileInfo>
#include <QJsonObject>

#include <gdal_priv.h>
#include <ogrsf_frmts.h>

namespace HydroCouple::Composer
{
  GdalVectorLayer::GdalVectorLayer(const QString &name, const QString &filePath)
    : FeatureLayer(name), m_filePath(filePath)
  {
    setSourceDescription(filePath);
  }

  GdalVectorLayer::~GdalVectorLayer() = default;

  QStringList GdalVectorLayer::sublayerNames(const QString &filePath)
  {
    ensureOgrDriversRegistered();

    QStringList names;

    auto *dataset = static_cast<GDALDataset *>(
      GDALOpenEx(filePath.toUtf8().constData(), GDAL_OF_VECTOR, nullptr,
                 nullptr, nullptr));

    if (!dataset)
    {
      return names;
    }

    for (int i = 0; i < dataset->GetLayerCount(); ++i)
    {
      names.append(QString::fromUtf8(dataset->GetLayer(i)->GetName()));
    }

    GDALClose(dataset);

    return names;
  }

  std::unique_ptr<GdalVectorLayer> GdalVectorLayer::open(
    const QString &filePath, QString &message, const QString &layerName)
  {
    ensureOgrDriversRegistered();

    auto *dataset = static_cast<GDALDataset *>(
      GDALOpenEx(filePath.toUtf8().constData(), GDAL_OF_VECTOR, nullptr,
                 nullptr, nullptr));

    if (!dataset)
    {
      message = QObject::tr("%1 could not be opened as a vector dataset.")
                  .arg(QFileInfo(filePath).fileName());

      return nullptr;
    }

    OGRLayer *ogrLayer = layerName.isEmpty()
                           ? dataset->GetLayer(0)
                           : dataset->GetLayerByName(
                               layerName.toUtf8().constData());

    if (!ogrLayer)
    {
      message = layerName.isEmpty()
                  ? QObject::tr("%1 holds no vector layers.")
                      .arg(QFileInfo(filePath).fileName())
                  : QObject::tr("%1 holds no layer named “%2”.")
                      .arg(QFileInfo(filePath).fileName(), layerName);
      GDALClose(dataset);

      return nullptr;
    }

    std::unique_ptr<GdalVectorLayer> layer(new GdalVectorLayer(
      QString::fromUtf8(ogrLayer->GetName()), filePath));

    OgrLayerContents contents;

    if (readOgrLayer(ogrLayer, layer->geometryKind(), contents) == 0)
    {
      message = QObject::tr("%1 holds no features with geometry.")
                  .arg(QFileInfo(filePath).fileName());
      GDALClose(dataset);

      return nullptr;
    }

    layer->setFields(contents.fields);

    if (!contents.crsWkt.isEmpty())
    {
      QString crsMessage;
      layer->setCrs(
        SpatialReference::fromDefinition(contents.crsWkt, crsMessage));
    }

    for (VectorFeature &feature : contents.features)
    {
      layer->addFeature(std::move(feature));
    }

    GDALClose(dataset);

    layer->finishLoading();

    QJsonObject state;
    state.insert(QStringLiteral("type"), QStringLiteral("gdal-vector"));
    state.insert(QStringLiteral("path"), filePath);
    state.insert(QStringLiteral("name"), layer->name());

    if (!layerName.isEmpty())
    {
      // A GeoPackage holds several, and reopening the first when the third
      // was chosen is a layer that looks right and is not.
      state.insert(QStringLiteral("layer"), layerName);
    }

    layer->setPersistentState(state);

    return layer;
  }

  QString GdalVectorLayer::filePath() const
  {
    return m_filePath;
  }

} // namespace HydroCouple::Composer
