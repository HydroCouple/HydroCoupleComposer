#include "layers/ogrfeatureloader.h"

#include "layers/ogrgeometryreader.h"

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

  void ensureOgrDriversRegistered()
  {
    ensureDriversRegistered();
  }

  int readOgrLayer(OGRLayer *source, GeometryKind kind,
                   OgrLayerContents &contents)
  {
    if (!source)
    {
      return 0;
    }

    // ── Fields ───────────────────────────────────────────────────────────
    const OGRFeatureDefn *definition = source->GetLayerDefn();

    for (int i = 0; i < definition->GetFieldCount(); ++i)
    {
      const OGRFieldDefn *field =
        const_cast<OGRFeatureDefn *>(definition)->GetFieldDefn(i);

      AttributeField entry;
      entry.name = QString::fromUtf8(field->GetNameRef());
      entry.displayName = entry.name;
      entry.type = metaTypeFor(field->GetType());

      contents.fields.append(entry);
    }

    // ── CRS ──────────────────────────────────────────────────────────────
    if (const OGRSpatialReference *reference = source->GetSpatialRef())
    {
      char *wkt = nullptr;

      if (const_cast<OGRSpatialReference *>(reference)->exportToWkt(&wkt)
            == OGRERR_NONE
          && wkt)
      {
        contents.crsWkt = QString::fromUtf8(wkt);
        CPLFree(wkt);
      }
    }

    // ── Features ─────────────────────────────────────────────────────────
    source->ResetReading();

    while (OGRFeature *feature = source->GetNextFeature())
    {
      VectorFeature entry;
      entry.kind = kind;

      if (collectOgrGeometry(feature->GetGeometryRef(), entry.parts,
                             entry.kind))
      {
        entry.attributes.reserve(contents.fields.size());

        for (int i = 0; i < contents.fields.size(); ++i)
        {
          entry.attributes.append(fieldValue(feature, i));
        }

        contents.features.append(std::move(entry));
      }

      OGRFeature::DestroyFeature(feature);
    }

    return int(contents.features.size());
  }

} // namespace HydroCouple::Composer
