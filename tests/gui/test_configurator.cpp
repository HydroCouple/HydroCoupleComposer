/*!
 * \file   test_configurator.cpp
 * \brief  Phase B2 verification — the argument configurator.
 *
 * The central test is a hydration contract: for every argument the fixture
 * component publishes, the descriptor must reflect what the component says
 * about it, an edit must survive a serialize/initialize round trip, and the
 * recorded document payload must equal what the component itself serialises.
 * A form that merely renders is not enough — it has to round-trip.
 */

#include "configurator/argumentdescriptor.h"
#include "configurator/componentconfigurator.h"
#include "core/composerapplication.h"
#include "plugins/componentregistry.h"
#include "project/componentinstances.h"
#include "project/compositiondocument.h"

#include <gtest/gtest.h>

#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>
#include <QUrl>
#include <QTableWidget>
#include <QUndoStack>

using namespace HydroCouple::Composer;

namespace
{
  QString dataPath(const QString &name)
  {
    return QDir(QStringLiteral(COMPOSER_CONFIGURATOR_FIXTURE_DIR))
      .absoluteFilePath(name);
  }

  QString fixturePath(const QString &stem)
  {
    return QDir(QStringLiteral(COMPOSER_FIXTURE_DIR))
      .absoluteFilePath(QStringLiteral("lib") + stem +
                        ComponentLibrary::librarySuffix());
  }

  class ConfiguratorTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_configurator";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      void SetUp() override
      {
        QString message;
        ASSERT_NE(registry.loadLibrary(fixturePath(QStringLiteral("testcomponent")),
                                       message),
                  nullptr)
          << message.toStdString();

        HydroCouple::SDK::IO::ComponentSpec spec;
        spec.id = "unit";
        spec.info.componentInfoId = "composer.test.component";
        ASSERT_TRUE(document.addComponent(spec, {}));

        instances = std::make_unique<ComponentInstances>(&document, &registry);
        configurator =
          std::make_unique<ComponentConfigurator>(&document, instances.get());
        configurator->setComponent(QStringLiteral("unit"));
      }

      void TearDown() override
      {
        configurator.reset();
        instances.reset();
      }

      [[nodiscard]] ArgumentDescriptor descriptorFor(const QString &id) const
      {
        for (const ArgumentDescriptor &descriptor : configurator->descriptors())
        {
          if (descriptor.id == id)
          {
            return descriptor;
          }
        }

        return {};
      }

      ComponentRegistry registry;
      CompositionDocument document;
      std::unique_ptr<ComponentInstances> instances;
      std::unique_ptr<ComponentConfigurator> configurator;

      static ComposerApplication *s_app;
  };

  ComposerApplication *ConfiguratorTest::s_app = nullptr;
}

// ── Introspection ─────────────────────────────────────────────────────────

TEST_F(ConfiguratorTest, DescribesEveryArgumentTheComponentPublishes)
{
  const QList<ArgumentDescriptor> descriptors = configurator->descriptors();
  ASSERT_FALSE(descriptors.isEmpty());

  QStringList ids;
  for (const ArgumentDescriptor &descriptor : descriptors)
  {
    ids.append(descriptor.id);
  }

  EXPECT_TRUE(ids.contains(QStringLiteral("scale")));
  EXPECT_TRUE(ids.contains(QStringLiteral("iterations")));
  EXPECT_TRUE(ids.contains(QStringLiteral("label")));
  EXPECT_TRUE(ids.contains(QStringLiteral("regime")));
  EXPECT_TRUE(ids.contains(QStringLiteral("grid")));
}

// The editor is chosen from what the component advertises, not from a schema.
TEST_F(ConfiguratorTest, ChoosesEditorsFromComponentMetadata)
{
  EXPECT_EQ(descriptorFor(QStringLiteral("iterations")).kind,
            ArgumentEditorKind::Integer);
  EXPECT_EQ(descriptorFor(QStringLiteral("label")).kind,
            ArgumentEditorKind::Text);

  // A value definition that enumerates categories becomes a choice.
  const ArgumentDescriptor regime = descriptorFor(QStringLiteral("regime"));
  EXPECT_EQ(regime.kind, ArgumentEditorKind::Categorical);
  EXPECT_EQ(regime.categories,
            QStringList({QStringLiteral("steady"), QStringLiteral("dynamic"),
                         QStringLiteral("kinematic")}));

  // Rank decides the table, and the shape comes from the component.
  const ArgumentDescriptor grid = descriptorFor(QStringLiteral("grid"));
  EXPECT_EQ(grid.kind, ArgumentEditorKind::Table);
  EXPECT_EQ(grid.rows, 2);
  EXPECT_EQ(grid.columns, 3);

  const ArgumentDescriptor scale = descriptorFor(QStringLiteral("scale"));
  EXPECT_EQ(scale.kind, ArgumentEditorKind::Table) << "rank-1 of length 3";
  EXPECT_EQ(scale.rows, 3);
}

TEST_F(ConfiguratorTest, BuildsAWidgetForEachEditableArgument)
{
  EXPECT_NE(configurator->findChild<QSpinBox *>(QStringLiteral("argument_iterations")),
            nullptr);
  EXPECT_NE(configurator->findChild<QLineEdit *>(QStringLiteral("argument_label")),
            nullptr);
  EXPECT_NE(configurator->findChild<QComboBox *>(QStringLiteral("argument_regime")),
            nullptr);
  EXPECT_NE(configurator->findChild<QTableWidget *>(QStringLiteral("argument_grid")),
            nullptr);
}

// ── The hydration contract ────────────────────────────────────────────────

TEST_F(ConfiguratorTest, EveryEditedArgumentRoundTripsThroughTheComponent)
{
  struct Case
  {
      QString id;
      nlohmann::json payload;
  };

  const std::vector<Case> cases{
    {QStringLiteral("iterations"), nlohmann::json{{"values", {7}}}},
    {QStringLiteral("label"), nlohmann::json{{"values", {"upper basin"}}}},
    {QStringLiteral("regime"), nlohmann::json{{"values", {"dynamic"}}}},
    {QStringLiteral("scale"), nlohmann::json{{"values", {1.5, 2.5, 3.5}}}},
    // Rank-2 values are rows, not a flattened run — the component enforces it.
    {QStringLiteral("grid"),
     nlohmann::json{{"values", {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}}}},
  };

  for (const Case &testCase : cases)
  {
    QString message;
    ASSERT_TRUE(configurator->applyArgument(testCase.id, testCase.payload,
                                            message))
      << testCase.id.toStdString() << ": " << message.toStdString();

    // What the document recorded must equal what the component now reports —
    // otherwise the form and the model have quietly diverged.
    const std::optional<CompositionDocument::ComponentSpec> spec =
      document.component(QStringLiteral("unit"));
    ASSERT_TRUE(spec.has_value());

    const std::string key = testCase.id.toStdString();
    ASSERT_TRUE(spec->arguments.contains(key))
      << testCase.id.toStdString() << " was not recorded";

    HydroCouple::IModelComponent *component =
      instances->instance(QStringLiteral("unit"));
    ASSERT_NE(component, nullptr);

    HydroCouple::IArgument *live = nullptr;
    for (HydroCouple::IArgument *candidate : component->arguments())
    {
      if (QString::fromStdString(candidate->id()) == testCase.id)
      {
        live = candidate;
        break;
      }
    }
    ASSERT_NE(live, nullptr);

    QString readMessage;
    const nlohmann::json reserialised = readArgumentPayload(live, readMessage);
    ASSERT_FALSE(reserialised.is_null()) << readMessage.toStdString();

    // The component's own serialisation must carry the values we set.
    const nlohmann::json &expected = testCase.payload["values"];
    const nlohmann::json &actual = reserialised.contains("values")
                                     ? reserialised["values"]
                                     : reserialised;

    EXPECT_EQ(actual, expected)
      << testCase.id.toStdString() << " did not round-trip; component said "
      << reserialised.dump();
  }
}

// A payload the model refuses must not reach the document.
TEST_F(ConfiguratorTest, RejectedPayloadNeverReachesTheDocument)
{
  const QByteArray before = document.toJson();

  QString message;
  EXPECT_FALSE(configurator->applyArgument(
    QStringLiteral("iterations"), nlohmann::json{{"values", {"not a number"}}},
    message));

  EXPECT_EQ(document.toJson(), before);
  EXPECT_FALSE(message.isEmpty());
}

TEST_F(ConfiguratorTest, RejectsUnknownArgument)
{
  QString message;
  EXPECT_FALSE(configurator->applyArgument(QStringLiteral("nonesuch"),
                                           nlohmann::json{{"values", {1}}},
                                           message));
  EXPECT_TRUE(message.contains(QStringLiteral("nonesuch")));
}

TEST_F(ConfiguratorTest, EditsAreUndoable)
{
  const int before = document.undoStack()->count();

  QString message;
  ASSERT_TRUE(configurator->applyArgument(QStringLiteral("iterations"),
                                          nlohmann::json{{"values", {9}}},
                                          message));

  EXPECT_GT(document.undoStack()->count(), before);

  const QByteArray edited = document.toJson();
  document.undoStack()->undo();
  EXPECT_NE(document.toJson(), edited);
}

// ── The raw pane ──────────────────────────────────────────────────────────

TEST_F(ConfiguratorTest, RawPaneTracksTheDocument)
{
  QString message;
  ASSERT_TRUE(configurator->applyArgument(QStringLiteral("iterations"),
                                          nlohmann::json{{"values", {4}}},
                                          message));

  EXPECT_TRUE(configurator->rawText().contains(QStringLiteral("iterations")));
}

TEST_F(ConfiguratorTest, RawPaneAppliesValidJson)
{
  configurator->setRawText(
    QStringLiteral("{\"iterations\": {\"values\": [11]}}"));

  QString message;
  ASSERT_TRUE(configurator->applyRawText(message)) << message.toStdString();

  const std::optional<CompositionDocument::ComponentSpec> spec =
    document.component(QStringLiteral("unit"));
  ASSERT_TRUE(spec.has_value());
  EXPECT_EQ(spec->arguments["iterations"]["values"][0], 11);
}

TEST_F(ConfiguratorTest, RawPaneRejectsMalformedJsonWithoutApplyingAnything)
{
  const QByteArray before = document.toJson();

  configurator->setRawText(QStringLiteral("{ not json"));

  QString message;
  EXPECT_FALSE(configurator->applyRawText(message));
  EXPECT_TRUE(message.contains(QStringLiteral("JSON")));
  EXPECT_EQ(document.toJson(), before);
}

// One bad argument must not leave the earlier ones applied.
TEST_F(ConfiguratorTest, RawPaneAppliesAllArgumentsOrNone)
{
  const QByteArray before = document.toJson();

  configurator->setRawText(QStringLiteral(
    "{\"iterations\": {\"values\": [3]}, \"nonesuch\": {\"values\": [1]}}"));

  QString message;
  EXPECT_FALSE(configurator->applyRawText(message));
  EXPECT_EQ(document.toJson(), before)
    << "a partially applied raw edit left the document inconsistent";
}

// ── Component-supplied editors ────────────────────────────────────────────

TEST_F(ConfiguratorTest, OffersNoComponentEditorWhenTheComponentHasNone)
{
  // The fixture does not implement IUIProvider, so the button stays hidden
  // rather than opening nothing.
  EXPECT_FALSE(configurator->hasComponentEditor());
  EXPECT_FALSE(configurator->showComponentEditor());
}

TEST_F(ConfiguratorTest, ReportsComponentsThatCannotBeLoaded)
{
  HydroCouple::SDK::IO::ComponentSpec spec;
  spec.id = "ghost";
  spec.info.componentInfoId = "org.nowhere.missing";
  ASSERT_TRUE(document.addComponent(spec, {}));

  configurator->setComponent(QStringLiteral("ghost"));

  EXPECT_TRUE(configurator->descriptors().isEmpty());
  EXPECT_FALSE(configurator->hasComponentEditor());
}

// ── A file argument ───────────────────────────────────────────────────────
//
// An argument advertises what it can read through fileFilters(); until this
// slice the Composer collected them and then built a bare QLineEdit, so the
// one thing the argument said about itself was the one thing discarded.

TEST_F(ConfiguratorTest, AFileArgumentIsOfferedAChooserAndAPlainTextOneIsNot)
{
  EXPECT_EQ(descriptorFor(QStringLiteral("rating")).kind,
            ArgumentEditorKind::FilePath);

  EXPECT_NE(configurator->findChild<QLineEdit *>(
              QStringLiteral("argument_rating_path")),
            nullptr);
  EXPECT_NE(configurator->findChild<QPushButton *>(
              QStringLiteral("argument_rating_browse")),
            nullptr)
    << "a file argument was given a text box, not a chooser";

  // 'label' is free text and must not sprout a file dialog.
  EXPECT_EQ(configurator->findChild<QPushButton *>(
              QStringLiteral("argument_label_browse")),
            nullptr);
}

TEST_F(ConfiguratorTest, TheChooserOffersTheArgumentsOwnFiltersThenAllFiles)
{
  EXPECT_EQ(descriptorFor(QStringLiteral("rating")).fileFilters,
            QStringList({QStringLiteral("Rating Tables (*.json)")}));

  EXPECT_EQ(fileDialogFilter({QStringLiteral("Rating Tables (*.json)")}),
            QStringLiteral("Rating Tables (*.json);;All Files (*)"));

  // An argument that names no filter still gets a usable dialog.
  EXPECT_EQ(fileDialogFilter({}), QStringLiteral("All Files (*)"));
}

TEST_F(ConfiguratorTest, ChoosingAFileLoadsItsValuesThroughTheComponent)
{
  QString message;
  ASSERT_TRUE(configurator->applyArgumentReference(QStringLiteral("rating"),
                                              dataPath(QStringLiteral("rating.json")),
                                              message))
    << message.toStdString();

  // The component is what read the file, so it is what must now hold it.
  configurator->setComponent(QStringLiteral("unit"));
  const nlohmann::json payload = descriptorFor(QStringLiteral("rating")).payload;

  ASSERT_TRUE(payload.contains("values"));
  EXPECT_EQ(payload["values"], nlohmann::json({2.5, 3.5, 4.5}));
}

// The document's only channel to an argument is initialize(..., JSON, ...),
// so a path recorded there would be handed back as JSON and refused. What is
// recorded is what the file turned into.
TEST_F(ConfiguratorTest, TheDocumentRecordsTheValuesTheFileProducedNotThePath)
{
  const QString path = dataPath(QStringLiteral("rating.json"));

  QString message;
  ASSERT_TRUE(configurator->applyArgumentReference(QStringLiteral("rating"), path,
                                              message))
    << message.toStdString();

  const std::optional<CompositionDocument::ComponentSpec> spec =
    document.component(QStringLiteral("unit"));
  ASSERT_TRUE(spec.has_value());

  ASSERT_TRUE(spec->arguments.contains("rating"));
  EXPECT_EQ(spec->arguments["rating"]["values"],
            nlohmann::json({2.5, 3.5, 4.5}));

  const QString recorded =
    QString::fromStdString(spec->arguments.dump());
  EXPECT_FALSE(recorded.contains(path))
    << "the path was recorded as if it were a value";
}

TEST_F(ConfiguratorTest, AFileTheComponentCannotReadIsRefusedAndNothingRecorded)
{
  const QByteArray before = document.toJson();

  QString message;
  EXPECT_FALSE(configurator->applyArgumentReference(
    QStringLiteral("rating"), dataPath(QStringLiteral("rating-not-json.txt")),
    message));
  EXPECT_FALSE(message.isEmpty());
  EXPECT_EQ(document.toJson(), before);
}

TEST_F(ConfiguratorTest, APathThatIsNotThereIsRefusedWithTheComponentsReason)
{
  const QByteArray before = document.toJson();

  QString message;
  EXPECT_FALSE(configurator->applyArgumentReference(
    QStringLiteral("rating"), dataPath(QStringLiteral("nosuchfile.json")),
    message));
  EXPECT_TRUE(message.contains(QStringLiteral("nosuchfile.json")))
    << "the refusal did not say which file: " << message.toStdString();
  EXPECT_EQ(document.toJson(), before);
}

// A path typed into the box is a path chosen; the dialog is a convenience,
// not the only way in.
TEST_F(ConfiguratorTest, TypingAPathAndPressingReturnLoadsTheFile)
{
  auto *line = configurator->findChild<QLineEdit *>(
    QStringLiteral("argument_rating_path"));
  ASSERT_NE(line, nullptr);

  line->setText(dataPath(QStringLiteral("rating.json")));
  QTest::keyClick(line, Qt::Key_Return);

  const std::optional<CompositionDocument::ComponentSpec> spec =
    document.component(QStringLiteral("unit"));
  ASSERT_TRUE(spec.has_value());
  ASSERT_TRUE(spec->arguments.contains("rating"))
    << "the path box is decoration: typing into it loaded nothing";
  EXPECT_EQ(spec->arguments["rating"]["values"],
            nlohmann::json({2.5, 3.5, 4.5}));
}

// ── Reading an argument from a layer ──────────────────────────────────────
//
// The point of the whole OGC program: a coverage fetched from a service is
// on the map, and one click hands it to a model.

TEST_F(ConfiguratorTest, AFileArgumentIsAlsoOfferedTheLayersOnTheMap)
{
  EXPECT_NE(configurator->findChild<QPushButton *>(
              QStringLiteral("argument_rating_layer")),
            nullptr);

  // Free text is not read from a layer.
  EXPECT_EQ(configurator->findChild<QPushButton *>(
              QStringLiteral("argument_label_layer")),
            nullptr);
}

TEST_F(ConfiguratorTest, ALayersAddressIsReadThroughTheComponent)
{
  const QUrl local =
    QUrl::fromLocalFile(dataPath(QStringLiteral("rating.json")));

  configurator->setLayerSources(
    [local]
    {
      return QVector<ComponentConfigurator::LayerSource>{
        {QStringLiteral("Rating coverage"), local}};
    });

  QString message;
  ASSERT_TRUE(configurator->applyArgumentReference(
    QStringLiteral("rating"), local.toString(), message))
    << message.toStdString();

  const std::optional<CompositionDocument::ComponentSpec> spec =
    document.component(QStringLiteral("unit"));
  ASSERT_TRUE(spec.has_value());
  EXPECT_EQ(spec->arguments["rating"]["values"],
            nlohmann::json({2.5, 3.5, 4.5}));
}

// An address with a scheme goes to the resolver, not to open(2). The scheme
// is deliberately one nothing handles, so this stays a local test: an http
// address here would really be fetched, because ComposerApplication installs
// a live resolver.
TEST_F(ConfiguratorTest, AnAddressWithASchemeIsResolvedRatherThanOpened)
{
  QString message;
  EXPECT_FALSE(configurator->applyArgumentReference(
    QStringLiteral("rating"), QStringLiteral("sensor://example.org/rating.json"),
    message));
  EXPECT_TRUE(message.contains(QStringLiteral("resolver")))
    << "it was opened as a file path: " << message.toStdString();
}

// A Windows path is a path. The SDK's rule decides, so there is only one.
TEST_F(ConfiguratorTest, ADriveLetterIsAPathAndNotAService)
{
  QString message;
  EXPECT_FALSE(configurator->applyArgumentReference(
    QStringLiteral("rating"), QStringLiteral("C:\\data\\rating.json"),
    message));
  EXPECT_EQ(message.indexOf(QStringLiteral("resolver")), -1)
    << "a drive letter was taken for a URI scheme: " << message.toStdString();
}

// How an argument was read is part of what it records about itself, and the
// only thing that tells a path from an address after the fact. A file: URI
// is the pair's honest test: it has a scheme, so it goes in as a URL, and it
// is read off local disk all the same.
TEST_F(ConfiguratorTest, HowAReferenceWasReadIsRecordedOnTheArgument)
{
  HydroCouple::IModelComponent *component =
    instances->instance(QStringLiteral("unit"));
  ASSERT_NE(component, nullptr);

  HydroCouple::IArgument *rating = nullptr;

  for (HydroCouple::IArgument *candidate : component->arguments())
  {
    if (candidate && candidate->id() == "rating")
    {
      rating = candidate;
    }
  }

  ASSERT_NE(rating, nullptr);

  const QString path = dataPath(QStringLiteral("rating.json"));

  QString message;
  ASSERT_TRUE(writeArgumentReference(rating, path, message))
    << message.toStdString();
  EXPECT_EQ(rating->currentArgumentInputType(),
            HydroCouple::IArgument::ArgumentInputType::File);

  ASSERT_TRUE(writeArgumentReference(
    rating, QStringLiteral("file://") + path, message))
    << message.toStdString();
  EXPECT_EQ(rating->currentArgumentInputType(),
            HydroCouple::IArgument::ArgumentInputType::URL)
    << "an address was recorded as though it had been a path";
}
