/*!
 * \file   runrecording.h
 * \author Caleb Buahin
 * \brief  Turning a composition's `writers` block into real output writers.
 *
 * The composition document names writers by type — `csv`, `netcdf_ugrid`,
 * `hdf5_ugrid`, `geopackage` — and a path. This maps those names onto the
 * SDK's writer classes so a run records itself, which is what later lets a
 * finished run be reopened without the model's library (plan G2).
 *
 * \par What a generic host can and cannot construct
 * `CSVWriter` needs only a destination. The UGRID and GeoPackage writers need
 * a `MeshDefinition` describing the grid their values live on, and a
 * composition of non-spatial components simply has no mesh to give them. Those
 * writers therefore become available once spatial data items are wired up
 * (phases C and E); until then, asking for one is reported as an unsupported
 * writer rather than silently producing no output.
 */

#ifndef HYDROCOUPLECOMPOSER_SIMULATION_RUNRECORDING_H
#define HYDROCOUPLECOMPOSER_SIMULATION_RUNRECORDING_H

#include "hydrocouplesdk/io/compositionspec.h"
#include "hydrocouplesdk/io/outputwriter.h"

#include <QString>

#include <memory>

namespace HydroCouple::Composer
{

  /*!
   * \brief Builds the writer a WriterSpec asks for.
   * \param spec The writer block from the composition.
   * \param documentDirectory Directory relative paths resolve against.
   * \param[out] message Why the writer could not be built, when it could not.
   * \returns The writer, or nullptr with \a message set.
   */
  [[nodiscard]] std::shared_ptr<HydroCouple::SDK::IO::IOutputWriter>
  makeOutputWriter(const HydroCouple::SDK::IO::WriterSpec &spec,
                   const QString &documentDirectory, QString &message);

  /*!
   * \brief Whether this host can build a writer of the given type.
   * \param type A WriterSpec type string.
   */
  [[nodiscard]] bool isWriterTypeSupported(const std::string &type);

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_SIMULATION_RUNRECORDING_H
