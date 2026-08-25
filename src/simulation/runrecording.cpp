#include "simulation/runrecording.h"

#include "hydrocouplesdk/io/csvwriter.h"

#include <QDir>
#include <QFileInfo>

namespace HydroCouple::Composer
{

  namespace
  {
    std::filesystem::path resolve(const std::string &path,
                                  const QString &documentDirectory)
    {
      const QString candidate = QString::fromStdString(path);

      if (documentDirectory.isEmpty() || QFileInfo(candidate).isAbsolute())
      {
        return std::filesystem::path(candidate.toStdString());
      }

      return std::filesystem::path(
        QDir(documentDirectory).absoluteFilePath(candidate).toStdString());
    }
  } // namespace

  bool isWriterTypeSupported(const std::string &type)
  {
    return type == "csv";
  }

  std::shared_ptr<HydroCouple::SDK::IO::IOutputWriter> makeOutputWriter(
    const HydroCouple::SDK::IO::WriterSpec &spec,
    const QString &documentDirectory, QString &message)
  {
    if (spec.path.empty())
    {
      message = QStringLiteral("writer '%1' has no path")
                  .arg(QString::fromStdString(spec.type));
      return nullptr;
    }

    const std::filesystem::path path = resolve(spec.path, documentDirectory);

    // Make sure the destination directory exists; a writer that fails only at
    // finalize() would lose a whole run's output.
    const QFileInfo destination(QString::fromStdString(path.string()));
    QDir().mkpath(destination.absolutePath());

    if (spec.type == "csv")
    {
      return std::make_shared<HydroCouple::SDK::IO::CSVWriter>(path);
    }

    if (spec.type == "netcdf_ugrid" || spec.type == "hdf5_ugrid" ||
        spec.type == "geopackage")
    {
      // These writers are constructed with a MeshDefinition describing the
      // grid their values live on. A composition of non-spatial components has
      // no mesh to supply, so this is reported rather than guessed at; the
      // capability arrives with spatial data items.
      message =
        QStringLiteral("the '%1' writer needs a mesh definition, which this "
                       "composition does not declare — supported once spatial "
                       "data items are wired up")
          .arg(QString::fromStdString(spec.type));
      return nullptr;
    }

    message = QStringLiteral("unknown writer type '%1'")
                .arg(QString::fromStdString(spec.type));

    return nullptr;
  }

} // namespace HydroCouple::Composer
