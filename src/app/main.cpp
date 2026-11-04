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
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUuid>
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
                  const QStringList &componentDirectories, bool jsonProgress)
  {
    QTextStream out(stdout);
    QTextStream err(stderr);
    const QString runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    qint64 sequence = 0;
    auto event = [&](const QString &name, QJsonObject payload = {}) {
      if (!jsonProgress) return;
      payload.insert(QStringLiteral("schema"), QStringLiteral("hydrocouple.run-events/1"));
      payload.insert(QStringLiteral("run_id"), runId);
      payload.insert(QStringLiteral("sequence"), sequence++);
      payload.insert(QStringLiteral("event"), name);
      // Foreign libraries may log to stdout; hosts select only this prefix.
      out << "HYDROCOUPLE_EVENT " << QJsonDocument(payload).toJson(QJsonDocument::Compact) << '\n';
      out.flush();
    };
    QFile compositionFile(compositionPath);
    const bool readable = jsonProgress && compositionFile.open(QIODevice::ReadOnly);
    event(QStringLiteral("started"), {{QStringLiteral("composition"), QFileInfo(compositionPath).absoluteFilePath()},
      {QStringLiteral("composition_sha256"), readable
          ? QString::fromLatin1(QCryptographicHash::hash(compositionFile.readAll(), QCryptographicHash::Sha256).toHex())
          : QString()}});

    ComponentRegistry registry;

    const QStringList paths = ComponentRegistry::defaultSearchPaths(componentDirectories);
    for (const QString &directory : paths)
    {
      const int loaded = registry.scanDirectory(directory);
      if (!jsonProgress) out << "loaded " << loaded << " component library(ies) from "
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
      event(QStringLiteral("finished"), {{QStringLiteral("succeeded"), false}, {QStringLiteral("message"), message}});
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
                       event(QStringLiteral("finished"), {{QStringLiteral("succeeded"), ok},
                         {QStringLiteral("message"), summary},
                         {QStringLiteral("diagnostics"), QJsonArray::fromStringList(simulation.errors())},
                         {QStringLiteral("manifest"), simulation.runManifestPath()}});
                       if (!jsonProgress) out << (ok ? "run finished: " : "run failed: ")
                           << summary << '\n';
                       loop.quit();
                     });

    QObject::connect(&simulation, &SimulationManager::stepCompleted, &loop,
                     [&out, jsonProgress](int step)
                     {
                       if (!jsonProgress && step % 50 == 0)
                       {
                         out << "  step " << step << '\n';
                       }
                     });
    if (jsonProgress)
    {
      QObject::connect(&simulation, &SimulationManager::progressSnapshot, &loop,
                       [&](const QJsonObject &snapshot) { event(QStringLiteral("progress"), snapshot); });
      QObject::connect(&simulation, &SimulationManager::initializationProgressed, &loop,
                       [&](int stage, int count, const QString &component, const QString &activity) {
                         event(QStringLiteral("initializing"), {{QStringLiteral("stage"), stage},
                           {QStringLiteral("stage_count"), count}, {QStringLiteral("component"), component},
                           {QStringLiteral("activity"), activity}});
                       });
    }

    if (!simulation.start(document, message))
    {
      err << "cannot run: " << message << '\n';
      event(QStringLiteral("finished"), {{QStringLiteral("succeeded"), false}, {QStringLiteral("message"), message}});
      return 1;
    }

    loop.exec();

    for (const QString &error : simulation.errors())
    {
      err << "  " << error << '\n';
    }

    if (!jsonProgress && !simulation.runManifestPath().isEmpty())
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
  const QCommandLineOption jsonProgressOption(QStringLiteral("json-progress"),
    QCoreApplication::translate("main", "Emit prefixed versioned JSON-line progress for a headless run."));
  parser.addOption(jsonProgressOption);


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
    return runHeadless(parser.value(runOption), componentDirectories, parser.isSet(jsonProgressOption));
  }
  if (parser.isSet(jsonProgressOption)) parser.showHelp(1);

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
