/*!
 * \file wcscoveragelayer.cpp
 * \brief WcsCoverageLayer implementation.
 */

#include "layers/wcscoveragelayer.h"

#include "layers/ogrfeatureloader.h"

#include <hydrocoupleogc/wcscapabilities.h>

#include <cstring>

#include <cpl_conv.h>
#include <cpl_vsi.h>
#include <gdal_priv.h>

#include <QUuid>

namespace HydroCouple::Composer
{
  WcsCoverageLayer::WcsCoverageLayer(const QString &name,
                                     const QString &memoryPath)
    : GdalRasterLayer(name, memoryPath), m_memoryPath(memoryPath)
  {
  }

  WcsCoverageLayer::~WcsCoverageLayer()
  {
    // Only the name is this layer's to release; the base closes the dataset
    // after us. The order is safe either way, which is worth saying because
    // it looks as though it should not be: /vsimem is reference counted, so
    // unlinking a file that still has an open handle removes the name and
    // frees the buffer when that handle closes.
    if (!m_memoryPath.isEmpty())
    {
      VSIUnlink(m_memoryPath.toUtf8().constData());
    }
  }

  std::unique_ptr<WcsCoverageLayer> WcsCoverageLayer::fromResponse(
    const QByteArray &body, const QString &name, QString &message)
  {
    if (body.isEmpty())
    {
      message = QObject::tr("The service returned nothing for %1.").arg(name);

      return nullptr;
    }

    ensureOgrDriversRegistered();

    /*
     * A unique name, unlike the WFS layer beside this one, and for a reason
     * that is easy to get backwards. /vsimem is a process-wide namespace, so
     * the question is how long the name must stay reserved -- and that is
     * however long the dataset behind it is open. A WFS response is decoded
     * once into features and closed immediately, so one fixed name plus an
     * unconditional unlink is right there: a leftover becomes a bug the next
     * fetch trips over rather than a leak. A raster is not decoded once. Its
     * dataset stays open for the layer's whole life, read again on every
     * repaint, so two coverage layers sharing a name would be two datasets
     * over one buffer.
     */
    const QString path =
      QStringLiteral("/vsimem/hydrocouple-wcs-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    // GDAL takes the buffer over (the trailing TRUE), so it must be memory
    // GDAL can free -- not QByteArray's, which is reference counted and may
    // outlive or predecease this call.
    void *buffer = CPLMalloc(static_cast<size_t>(body.size()));
    std::memcpy(buffer, body.constData(), static_cast<size_t>(body.size()));

    VSILFILE *file = VSIFileFromMemBuffer(
      path.toUtf8().constData(), static_cast<GByte *>(buffer),
      static_cast<vsi_l_offset>(body.size()), TRUE);

    if (!file)
    {
      CPLFree(buffer);
      message = QObject::tr("%1 could not be held in memory to be read.")
                  .arg(name);

      return nullptr;
    }

    VSIFCloseL(file);

    auto *dataset =
      static_cast<GDALDataset *>(GDALOpen(path.toUtf8().constData(),
                                          GA_ReadOnly));

    if (!dataset)
    {
      VSIUnlink(path.toUtf8().constData());

      // Very often an exception report rather than a coverage: a WCS answers
      // a bad request with XML under a 200 as readily as under a 404.
      const QString reported =
        HydroCouple::Ogc::wcsExceptionText(body);

      message = reported.isEmpty()
                  ? QObject::tr("What %1 returned is not a coverage this "
                                "program can read.").arg(name)
                  : reported;

      return nullptr;
    }

    std::unique_ptr<WcsCoverageLayer> layer(new WcsCoverageLayer(name, path));

    if (!layer->adoptDataset(dataset, message))
    {
      GDALClose(dataset);
      VSIUnlink(path.toUtf8().constData());

      return nullptr;
    }

    layer->setName(name);

    return layer;
  }

  QString WcsCoverageLayer::serviceUrl() const
  {
    return m_serviceUrl;
  }

  void WcsCoverageLayer::setServiceUrl(const QString &url)
  {
    m_serviceUrl = url;
  }

  QString WcsCoverageLayer::coverageId() const
  {
    return m_coverageId;
  }

  void WcsCoverageLayer::setCoverageId(const QString &identifier)
  {
    m_coverageId = identifier;
  }

  const HydroCouple::Ogc::WcsCoverageDescription &
  WcsCoverageLayer::description() const
  {
    return m_description;
  }

  void WcsCoverageLayer::setDescription(
    const HydroCouple::Ogc::WcsCoverageDescription &description)
  {
    m_description = description;
  }

  QString WcsCoverageLayer::sourceDescription() const
  {
    if (m_serviceUrl.isEmpty())
    {
      return m_coverageId;
    }

    if (m_coverageId.isEmpty())
    {
      return m_serviceUrl;
    }

    return QStringLiteral("%1 (%2)").arg(m_serviceUrl, m_coverageId);
  }

} // namespace HydroCouple::Composer
