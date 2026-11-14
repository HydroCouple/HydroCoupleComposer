#include "layers/wfsfeaturelayer.h"

#include "gis/spatialreference.h"
#include "layers/ogrfeatureloader.h"

#include <hydrocoupleogc/wfscapabilities.h>

#include <cpl_vsi.h>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>

namespace HydroCouple::Composer
{
  namespace
  {
    /*!
     * \brief What the service said when it would not answer.
     *
     * A WFS refuses in the body, under an HTTP 200 as often as not, so a
     * response that will not open has to be read as a document before it
     * is reported as gibberish.
     */
    QString refusal(const QByteArray &body)
    {
      const HydroCouple::Ogc::WfsCapabilities report =
        HydroCouple::Ogc::parseWfsCapabilities(body);

      return report.ok ? QString() : report.message;
    }
  }

  WfsFeatureLayer::WfsFeatureLayer(const QString &name) : FeatureLayer(name)
  {
  }

  WfsFeatureLayer::~WfsFeatureLayer() = default;

  QString WfsFeatureLayer::typeName() const
  {
    return m_typeName;
  }

  void WfsFeatureLayer::setTypeName(const QString &typeName)
  {
    m_typeName = typeName;
    setSourceDescription(typeName);
  }

  std::unique_ptr<WfsFeatureLayer> WfsFeatureLayer::fromResponse(
    const QByteArray &body, const QString &name, QString &message)
  {
    if (body.isEmpty())
    {
      message = QObject::tr("The service sent an empty response.");

      return nullptr;
    }

    ensureOgrDriversRegistered();

    // /vsimem is a process-wide namespace, and this name is deliberately
    // fixed: decoding happens on the calling thread and the file is
    // removed before this returns, so reusing the name means a response
    // left behind is a bug the next fetch trips over immediately rather
    // than a leak that grows quietly. Decoding off the GUI thread would
    // need a name per call.
    const QString path = QStringLiteral("/vsimem/hydrocouple-wfs-response");

    VSILFILE *file = VSIFileFromMemBuffer(
      path.toUtf8().constData(),
      reinterpret_cast<GByte *>(const_cast<char *>(body.constData())),
      body.size(), FALSE);

    if (!file)
    {
      message = QObject::tr("The response could not be read.");

      return nullptr;
    }

    VSIFCloseL(file);

    auto *dataset = static_cast<GDALDataset *>(
      GDALOpenEx(path.toUtf8().constData(), GDAL_OF_VECTOR, nullptr, nullptr,
                 nullptr));

    if (!dataset)
    {
      const QString said = refusal(body);

      message = said.isEmpty()
                  ? QObject::tr("The service's answer was not features this "
                                "program can read.")
                  : said;
      VSIUnlink(path.toUtf8().constData());

      return nullptr;
    }

    OGRLayer *source = dataset->GetLayer(0);

    std::unique_ptr<WfsFeatureLayer> layer(new WfsFeatureLayer(name));

    OgrLayerContents contents;
    const int read = readOgrLayer(source, layer->geometryKind(), contents);

    GDALClose(dataset);
    VSIUnlink(path.toUtf8().constData());

    if (read == 0)
    {
      // An empty answer is not a failure of this program: the collection
      // may simply hold nothing over the ground that was asked about, and
      // saying so is more use than an error.
      message = QObject::tr("The service returned no features over that "
                            "ground.");

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

    layer->finishLoading();

    return layer;
  }

} // namespace HydroCouple::Composer
