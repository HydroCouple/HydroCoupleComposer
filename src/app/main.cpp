/*!
 * \file   main.cpp
 * \author Caleb Buahin
 * \brief  HydroCoupleComposer entry point, windowed or headless.
 *
 * The headless path exists so a composition can be executed by CI, a cluster
 * job or a script without a display, reusing exactly the pieces the window
 * uses — the same registry, the same document, the same SimulationManager. A
 * separate runner would be a second implementation to keep honest.
 *
 * Lives under src/app/ so it does not collide with the retired v1 entry point
 * at src/main.cpp, which stays until the legacy qmake project is removed.
 */

#include "core/composerapplication.h"
#include "plugins/componentregistry.h"
#include "project/compositiondocument.h"
#include "simulation/simulationmanager.h"
#include "ui/composermainwindow.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTextStream>

using namespace HydroCouple::Composer;

namespace
{
  /*!
   * \brief Runs a composition without a window.
   * \param compositionPath Composition document to run.
   * \param componentDirectories Directories to scan for component libraries.
   * \returns Process exit code: 0 on success.
   */
  int runHeadless(const QString &compositionPath,
                  const QStringList &componentDirectories)
  {
    QTextStream out(stdout);
    QTextStream err(stderr);

    ComponentRegistry registry;

    for (const QString &directory : componentDirectories)
    {
      const int loaded = registry.scanDirectory(directory);
      out << "loaded " << loaded << " component library(ies) from "
          << directory << '\n';
    }

    for (const ComponentLoadFailure &failure : registry.failures())
    {
      // Reported, not fatal: a plugin directory legitimately holds
      // dependencies beside components.
      err << "skipped " << failure.filePath << ": " << failure.message << '\n';
    }

    CompositionDocument document;
    QString message;

    if (!document.load(compositionPath, message))
    {
      err << "cannot open composition: " << message << '\n';
      return 1;
    }

    SimulationManager simulation(&registry);

    bool succeeded = false;
    bool done = false;

    QEventLoop loop;

    QObject::connect(&simulation, &SimulationManager::finished, &loop,
                     [&](bool ok, const QString &summary)
                     {
                       succeeded = ok;
                       done = true;
                       out << (ok ? "run finished: " : "run failed: ")
                           << summary << '\n';
                       loop.quit();
                     });

    QObject::connect(&simulation, &SimulationManager::stepCompleted, &loop,
                     [&out](int step)
                     {
                       if (step % 50 == 0)
                       {
                         out << "  step " << step << '\n';
                       }
                     });

    if (!simulation.start(document, message))
    {
      err << "cannot run: " << message << '\n';
      return 1;
    }

    loop.exec();

    for (const QString &error : simulation.errors())
    {
      err << "  " << error << '\n';
    }

    if (!simulation.runManifestPath().isEmpty())
    {
      out << "run manifest: " << simulation.runManifestPath() << '\n';
    }

    return (done && succeeded) ? 0 : 1;
  }
} // namespace

int main(int argc, char *argv[])
{
  ComposerApplication app(argc, argv);

  QCommandLineParser parser;
  parser.setApplicationDescription(
    QCoreApplication::translate(
      "main",
      "Composition editor, configurator and results viewer for HydroCouple 2.0."));
  parser.addHelpOption();
  parser.addVersionOption();

  const QCommandLineOption runOption(
    {QStringLiteral("r"), QStringLiteral("run")},
    QCoreApplication::translate("main",
                                "Run <composition> headlessly and exit."),
    QStringLiteral("composition"));
  parser.addOption(runOption);

  const QCommandLineOption componentsOption(
    {QStringLiteral("c"), QStringLiteral("components")},
    QCoreApplication::translate(
      "main", "Directory of component libraries; may be repeated."),
    QStringLiteral("directory"));
  parser.addOption(componentsOption);

  parser.addPositionalArgument(
    QStringLiteral("composition"),
    QCoreApplication::translate("main",
                                "Composition document to open on startup."),
    QStringLiteral("[composition]"));

  parser.process(app);

  const QStringList componentDirectories = parser.values(componentsOption);

  if (parser.isSet(runOption))
  {
    return runHeadless(parser.value(runOption), componentDirectories);
  }

  ComposerMainWindow window;

  for (const QString &directory : componentDirectories)
  {
    const int loaded = window.loadComponentDirectory(directory);
    window.log(QCoreApplication::translate("main",
                                           "Loaded %1 component library(ies) "
                                           "from %2")
                 .arg(loaded)
                 .arg(directory));
  }

  const QStringList positional = parser.positionalArguments();

  if (!positional.isEmpty())
  {
    QString message;

    if (!window.openComposition(positional.first(), message))
    {
      window.log(message);
    }
  }

  window.show();

  return app.exec();
}
