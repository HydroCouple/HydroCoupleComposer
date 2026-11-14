/*!
 * \file   domainimport.h
 * \author Caleb Buahin
 * \brief  Turning features already on the map into part of a mesh domain.
 *
 * Import from a *layer*, not from a file. The composer can already add any
 * OGR dataset as a map layer, pick its sublayer and reproject it, and the
 * user can already select features on it — so an importer that opened its
 * own file dialog would be a second reader, a second reprojection path and
 * a second chance for the two to disagree about where the same file is.
 * Reading the layer's projected geometry means an imported boundary lands
 * exactly where the same file drawn as a layer does, and the user sees the
 * data before committing it.
 *
 * A polygon's rings are not all the same thing, and which is which is read
 * off the geometry rather than off the order it arrived in. The OGR reader
 * flattens a MultiPolygon's rings into one list, so "the first ring is the
 * outer one and the rest are its holes" would read the second square of a
 * two-square MultiPolygon as a hole in the first — silently, and shapefiles
 * are full of MultiPolygons. A ring is a hole when it lies inside an odd
 * number of the other rings of its own feature, which is true of a polygon
 * with holes, of a MultiPolygon, and of the two combined, and does not
 * depend on winding order — shapefiles and GeoJSON disagree about that.
 *
 * Guessing by area is needed only to choose between the outer rings of
 * different features, where nothing in the file says which is the domain.
 */

#ifndef HYDROCOUPLECOMPOSER_MESH_DOMAINIMPORT_H
#define HYDROCOUPLECOMPOSER_MESH_DOMAINIMPORT_H

#include "mesh/meshdomain.h"

#include <QString>

namespace HydroCouple::Composer
{
  class FeatureLayer;
  class MeshDomainModel;

  /*!
   * \brief What an import did, in enough detail to say so.
   *
   * Counted rather than merely succeeded or failed: an import that quietly
   * dropped nine of twelve rings looks exactly like a file that held three.
   */
  struct DomainImportResult
  {
      //! Whether anything reached the domain.
      bool ok = false;

      //! What to tell the user, whether or not it worked.
      QString message;

      int boundaries = 0;   //!< 0 or 1 — a domain has one.
      int holes = 0;
      int breaklines = 0;
      int points = 0;

      //! Rings and lines that describe no ground, so were left out.
      int skipped = 0;

      /*!
       * \brief Interior rings that could not be expressed.
       *
       * A hole's own hole is an island of ground inside it, and a domain
       * carries a flat list of rings cut out of one boundary — there is
       * nowhere for an island to go. Counted and reported rather than
       * added as another hole, which would cut away the ground it stands
       * on.
       */
      int islands = 0;
  };

  /*!
   * \brief Adds \a layer's selected features to \a model as \a part.
   *
   * The boundary is replaced, because there is exactly one; holes,
   * breaklines and points are added to what is already there. The whole
   * import is one change to the model, so one gesture repaints the map
   * once.
   *
   * \param layer The layer to read; its map-CRS geometry is used, so the
   *        result is in the same coordinates the domain is drawn in.
   * \param part What the selected features become.
   * \param model The domain to add them to.
   * \returns What was imported, or why nothing was.
   */
  [[nodiscard]] DomainImportResult importSelectionAsDomainPart(
    const FeatureLayer &layer, DomainPart part, MeshDomainModel &model);

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_MESH_DOMAINIMPORT_H
