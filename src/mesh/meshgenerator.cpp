#include "mesh/meshgenerator.h"

#include "hydrocouplesdk/tools/quadmesher.h"
#include "hydrocouplesdk/tools/triangulator.h"

#include <QObject>

namespace HydroCouple::Composer
{
  namespace
  {
    namespace IO = HydroCouple::SDK::IO;
    namespace Tools = HydroCouple::SDK::Tools;

    //! Counts \a mesh's faces by how many nodes each one uses.
    void countFaces(const IO::MeshDefinition &mesh, qint64 &triangles,
                    qint64 &quads)
    {
      triangles = 0;
      quads = 0;

      for (size_t face = 0; face + 1 < mesh.faceNodeOffsets.size(); ++face)
      {
        const int64_t width =
          mesh.faceNodeOffsets[face + 1] - mesh.faceNodeOffsets[face];

        if (width == 3)
        {
          ++triangles;
        }
        else if (width == 4)
        {
          ++quads;
        }
      }
    }
  }

  MeshGenerationResult generateMesh(const MeshDomain &domain,
                                    const MeshGenerationOptions &options,
                                    const Tools::MeshProgress &progress)
  {
    MeshGenerationResult result;

    // Asked first, so a domain with no boundary is answered in the words it
    // already has for that rather than by whatever the triangulator makes of
    // an empty ring.
    QString why;

    if (!domain.isValid(why))
    {
      result.message = why;

      return result;
    }

    std::string message;

    if (!Tools::Triangulator::triangulate(domain.toTriangulationInput(),
                                          result.mesh, message, progress))
    {
      // A run the user abandoned is not a run that failed, and telling them
      // the triangulation failed after they pressed Cancel would be a lie
      // about their own model.
      if (message == "cancelled")
      {
        result.cancelled = true;
        result.message = QObject::tr("Meshing was cancelled.");
        result.mesh = {};

        return result;
      }

      // The triangulator's own words: it is the one that knows what it could
      // not do with the ground it was given.
      result.message = QObject::tr("The triangulation failed: %1")
                         .arg(QString::fromStdString(message));
      result.mesh = {};

      return result;
    }

    if (options.quadDominant)
    {
      IO::MeshDefinition merged;

      if (!Tools::QuadMesher::triToQuadDominant(
            result.mesh, options.minQuadQuality, merged, message, progress))
      {
        if (message == "cancelled")
        {
          result.cancelled = true;
          result.message = QObject::tr("Meshing was cancelled.");
          result.mesh = {};

          return result;
        }

        result.message = QObject::tr("The quad merge failed: %1")
                           .arg(QString::fromStdString(message));
        result.mesh = {};

        return result;
      }

      result.mesh = std::move(merged);
    }

    countFaces(result.mesh, result.triangles, result.quads);

    result.ok = true;
    result.message =
      options.quadDominant
        ? QObject::tr("Generated %1 quads and %2 triangles over %3 nodes.")
            .arg(result.quads)
            .arg(result.triangles)
            .arg(result.mesh.nodeCount())
        : QObject::tr("Generated %1 triangles over %2 nodes.")
            .arg(result.triangles)
            .arg(result.mesh.nodeCount());

    return result;
  }

} // namespace HydroCouple::Composer
