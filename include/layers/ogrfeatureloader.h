/*!
 * \file   ogrfeatureloader.h
 * \author Caleb Buahin
 * \brief  Filling a FeatureLayer from an OGR layer.
 *
 * Shared because a file on disk and a feature service arrive at the same
 * place by different roads. GDAL cannot fetch anything in this build — it
 * is compiled without curl, so its WFS driver is not registered and
 * CPLHTTPFetch is a stub — but it decodes bytes Qt fetched exactly as it
 * decodes bytes that were on disk. The only difference between the two is
 * where the bytes came from, so only that difference belongs in the
 * callers.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_OGRFEATURELOADER_H
#define HYDROCOUPLECOMPOSER_LAYERS_OGRFEATURELOADER_H

#include "layers/featurelayer.h"

class OGRLayer;

namespace HydroCouple::Composer
{

  /*!
   * \brief Registers GDAL's drivers, once.
   */
  void ensureOgrDriversRegistered();

  /*!
   * \brief What an OGR layer holds, in this program's own types.
   */
  struct OgrLayerContents
  {
      QVector<AttributeField> fields;
      QVector<VectorFeature> features;

      //! The layer's coordinate system as well-known text, or empty.
      QString crsWkt;
  };

  /*!
   * \brief Reads \a source into \a contents.
   *
   * Returns plain data rather than filling a layer, because a layer's
   * fields and features are its own to set — the base class keeps those
   * protected so that only a subclass may load one. Each subclass applies
   * what this read, and only the OGR half is shared.
   *
   * Features whose geometry cannot be read are skipped rather than kept
   * empty.
   *
   * \param source OGR layer to read; null yields nothing.
   * \param kind The geometry kind features should be recorded as.
   * \param[out] contents Receives everything read.
   * \returns How many features were read.
   */
  int readOgrLayer(OGRLayer *source, GeometryKind kind,
                   OgrLayerContents &contents);

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_OGRFEATURELOADER_H
