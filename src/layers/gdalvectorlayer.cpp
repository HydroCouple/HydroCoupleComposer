#include "layers/gdalvectorlayer.h"

#include "gis/spatialreference.h"
#include "layers/ogrgeometryreader.h"

#include <QFileInfo>

#include <gdal_priv.h>
#include <ogrsf_frmts.h>

namespace HydroCouple::Composer
{
  namespace
  {
    //! Registered once; GDAL's driver registration is not re-entrant.
    void ensureDriversRegistered()
    {
      static const bool registered = []
      {
        GDALAllRegister();
        return true;
      }();

      Q_UNUSED(registered)
    }

    QMetaType::Type metaTypeFor(OGRFieldType type)
    {
      switch (type)
      {
        case OFTInteger:
          return QMetaType::Int;
        case OFTInteger64:
          return QMetaType::LongLong;
        case OFTReal:
          return QMetaType::Double;
        case OFTDate:
        case OFTDateTime:
          return QMetaType::QDateTime;
        default:
          return QMetaType::QString;
      }
    }

    QVariant fieldValue(OGRFeature *feature, int index)
    {
      // An unset field is not an empty string or a zero — treating it as one
      // would put it in a class, and classification is what reads these.
      if (!feature->IsFieldSetAndNotNull(index))
      {
        return {};
      }

      switch (feature->GetFieldDefnRef(index)->GetType())
      {
        case OFTInteger:
          return feature->GetFieldAsInteger(index);
        case OFTInteger64:
          return QVariant::fromValue(feature->GetFieldAsInteger64(index));
        case OFTReal:
          return feature->GetFieldAsDouble(index);
        default:
          return QString::fromUtf8(feature->GetFieldAsString(index));
      }
    }

  }

  GdalVectorLayer::GdalVectorLayer(const QString &name, const QString &filePath)
    : FeatureLayer(name), m_filePath(filePath)
  {
  }

  GdalVectorLayer::~GdalVectorLayer() = default;

  QStringList GdalVectorLayer::sublayerNames(const QString &filePath)
  {
    ensureDriversRegistered();

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
    ensureDriversRegistered();

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

    // ── Fields ───────────────────────────────────────────────────────────
    const OGRFeatureDefn *definition = ogrLayer->GetLayerDefn();
    QVector<AttributeField> fields;

    for (int i = 0; i < definition->GetFieldCount(); ++i)
    {
      const OGRFieldDefn *field =
        const_cast<OGRFeatureDefn *>(definition)->GetFieldDefn(i);

      AttributeField entry;
      entry.name = QString::fromUtf8(field->GetNameRef());
      entry.displayName = entry.name;
      entry.type = metaTypeFor(field->GetType());

      fields.append(entry);
    }

    layer->setFields(fields);

    // ── CRS ──────────────────────────────────────────────────────────────
    if (const OGRSpatialReference *reference = ogrLayer->GetSpatialRef())
    {
      char *wkt = nullptr;

      if (const_cast<OGRSpatialReference *>(reference)->exportToWkt(&wkt)
            == OGRERR_NONE
          && wkt)
      {
        QString crsMessage;
        layer->setCrs(SpatialReference::fromDefinition(
          QString::fromUtf8(wkt), crsMessage));
        CPLFree(wkt);
      }
    }

    // ── Features ─────────────────────────────────────────────────────────
    ogrLayer->ResetReading();

    while (OGRFeature *feature = ogrLayer->GetNextFeature())
    {
      VectorFeature entry;
      entry.kind = layer->geometryKind();

      if (collectOgrGeometry(feature->GetGeometryRef(), entry.parts,
                             entry.kind))
      {
        entry.attributes.reserve(fields.size());

        for (int i = 0; i < fields.size(); ++i)
        {
          entry.attributes.append(fieldValue(feature, i));
        }

        layer->addFeature(std::move(entry));
      }

      OGRFeature::DestroyFeature(feature);
    }

    GDALClose(dataset);

    if (layer->featureCount() == 0)
    {
      message = QObject::tr("%1 holds no features with geometry.")
                  .arg(QFileInfo(filePath).fileName());

      return nullptr;
    }

    layer->finishLoading();

    return layer;
  }

  QString GdalVectorLayer::filePath() const
  {
    return m_filePath;
  }

} // namespace HydroCouple::Composer
