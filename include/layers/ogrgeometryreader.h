/*!
 * \file   ogrgeometryreader.h
 * \author Caleb Buahin
 * \brief  Flattening OGR geometry into drawable parts.
 *
 * Shared by the file loader and the data-item loader. The latter goes through
 * OGR deliberately: HydroCouple geometries can produce WKB, and reading that
 * back means every geometry type the standard admits — multi-parts and
 * collections included — arrives through one tested path instead of a switch
 * that grows a case each time a component uses a shape nobody anticipated.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_OGRGEOMETRYREADER_H
#define HYDROCOUPLECOMPOSER_LAYERS_OGRGEOMETRYREADER_H

#include "layers/featurelayer.h"

class OGRGeometry;

namespace HydroCouple::Composer
{

  /*!
   * \brief Flattens \a geometry into polygon parts, recursing collections.
   * \param geometry Geometry to read; null yields nothing.
   * \param[out] parts Receives one polygon per ring, line or point.
   * \param[out] kind Receives what the geometry turned out to be.
   * \returns True when anything was collected.
   */
  bool collectOgrGeometry(const OGRGeometry *geometry,
                          QVector<QPolygonF> &parts, GeometryKind &kind);

  /*!
   * \brief Flattens well-known binary into polygon parts.
   * \param wkb The WKB bytes.
   * \param[out] parts Receives the geometry's parts.
   * \param[out] kind Receives what the geometry turned out to be.
   * \returns True when the bytes parsed and held geometry.
   */
  bool collectWkbGeometry(const std::vector<unsigned char> &wkb,
                          QVector<QPolygonF> &parts, GeometryKind &kind);

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_OGRGEOMETRYREADER_H
