/*!
 * \file   meshgenerator.h
 * \author Caleb Buahin
 * \brief  Generating a mesh over a domain, by handing it to the SDK.
 *
 * A delegation, not an implementation. The triangulation and the quad merge
 * are the SDK's, and the composer's job is to describe the ground, pass the
 * parameters through unaltered, and say what came back — so a mesh built
 * here and one built by calling the SDK directly are the same mesh, and a
 * model run against either gets the same answer.
 *
 * Kept apart from the dialog that will drive it and from the layer that
 * will draw it, because it is the part that has to be right: a preview and
 * a progress bar are visible when they are wrong, and a silently different
 * mesh is not.
 */

#ifndef HYDROCOUPLECOMPOSER_MESH_MESHGENERATOR_H
#define HYDROCOUPLECOMPOSER_MESH_MESHGENERATOR_H

#include "mesh/meshdomain.h"

#include "hydrocouplesdk/io/meshdefinition.h"

#include <QString>

namespace HydroCouple::Composer
{
  /*!
   * \brief What to build beyond the ground the domain already describes.
   *
   * The edge-length limit is not here: it belongs to the domain, which has
   * carried it since the domain could be drawn, and a second copy on the
   * options would be a second answer to one question.
   */
  struct MeshGenerationOptions
  {
      //! Whether to merge triangle pairs into quads after triangulating.
      bool quadDominant = false;

      /*!
       * \brief The quality below which a pair is left as two triangles.
       *
       * The SDK's threshold, passed through: 1 is a perfect rectangle and 0
       * accepts anything convex, so this is the only control over how
       * quad-dominant "quad-dominant" turns out to be.
       */
      double minQuadQuality = 0.5;
  };

  /*!
   * \brief The mesh, or why there is not one.
   */
  struct MeshGenerationResult
  {
      bool ok = false;

      //! What to tell the user, whether or not it worked.
      QString message;

      //! The mesh; empty unless \a ok.
      HydroCouple::SDK::IO::MeshDefinition mesh;

      //! Counted from the faces, so the two always describe the same mesh.
      qint64 triangles = 0;
      qint64 quads = 0;
  };

  /*!
   * \brief Meshes \a domain.
   *
   * \param domain The ground to mesh; refused before the SDK is troubled if
   *        it is not one a mesh can be built over.
   * \param options What to do after triangulating.
   * \returns The mesh, or the first refusal that stopped it.
   */
  [[nodiscard]] MeshGenerationResult generateMesh(
    const MeshDomain &domain, const MeshGenerationOptions &options = {});

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_MESH_MESHGENERATOR_H
