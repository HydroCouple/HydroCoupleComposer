/*!
 * \file   test_component_loader.cpp
 * \brief  Phase A2 verification — the dlopen loader and registry.
 *
 * The fixtures are real shared libraries built in-tree (see
 * tests/fixtures/testcomponent), so these assertions exercise the actual
 * cross-library vtable and allocation paths rather than a stub standing in
 * for them.
 */

#include "core/composerapplication.h"
#include "plugins/componentlibrary.h"
#include "plugins/componentregistry.h"

#include "hydrocouplesdk/component/abstractmodelcomponent.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QString>

#include <set>

using namespace HydroCouple::Composer;

namespace
{
  //! Directory the fixture libraries are built into; injected by CMake.
  QString fixtureDir()
  {
    return QStringLiteral(COMPOSER_FIXTURE_DIR);
  }

  QString fixturePath(const QString &stem)
  {
    return QDir(fixtureDir())
      .absoluteFilePath(QStringLiteral("lib") + stem +
                        ComponentLibrary::librarySuffix());
  }

  class LoaderTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_component_loader";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      static ComposerApplication *s_app;
  };

  ComposerApplication *LoaderTest::s_app = nullptr;
}

// The fixtures must exist, or every other assertion here is vacuous.
TEST_F(LoaderTest, FixtureLibrariesWereBuilt)
{
  EXPECT_TRUE(QFileInfo::exists(fixturePath(QStringLiteral("testcomponent"))))
    << "missing: " << fixturePath(QStringLiteral("testcomponent")).toStdString();
  EXPECT_TRUE(QFileInfo::exists(fixturePath(QStringLiteral("notacomponent"))));
  EXPECT_TRUE(QFileInfo::exists(fixturePath(QStringLiteral("badabicomponent"))));
}

TEST_F(LoaderTest, LoadsRealComponentLibraryAndReportsMetadata)
{
  QString message;
  std::unique_ptr<ComponentLibrary> library =
    ComponentLibrary::load(fixturePath(QStringLiteral("testcomponent")),
                           message);

  ASSERT_NE(library, nullptr) << message.toStdString();

  HydroCouple::IComponentInfo *info = library->componentInfo();
  ASSERT_NE(info, nullptr);

  EXPECT_EQ(info->id(), std::string("composer.test.component"));
  EXPECT_FALSE(info->caption().empty());
  EXPECT_EQ(info->version(), std::string("1.0.0"));

  // The loader, not the library, knows where it came from.
  EXPECT_EQ(info->libraryFilePath(),
            fixturePath(QStringLiteral("testcomponent")).toStdString());

  EXPECT_EQ(library->abiStamp(), ComponentLibrary::hostAbiStamp());
}

TEST_F(LoaderTest, InstantiatesAndDrivesComponentAcrossLibraryBoundary)
{
  ComponentRegistry registry;
  QString message;

  ASSERT_NE(registry.loadLibrary(fixturePath(QStringLiteral("testcomponent")),
                                 message),
            nullptr)
    << message.toStdString();

  std::unique_ptr<HydroCouple::IModelComponent> instance =
    registry.createInstance(QStringLiteral("composer.test.component"), message);

  ASSERT_NE(instance, nullptr) << message.toStdString();

  // Driving the lifecycle proves virtual dispatch works across the boundary,
  // which a mere successful dlopen would not.
  instance->initialize();
  EXPECT_EQ(instance->status(),
            HydroCouple::IModelComponent::ComponentStatus::Initialized);

  const std::vector<std::string> validationMessages = instance->validate();
  EXPECT_TRUE(validationMessages.empty());
  EXPECT_EQ(instance->status(),
            HydroCouple::IModelComponent::ComponentStatus::Valid);

  EXPECT_FALSE(instance->arguments().empty());
  EXPECT_FALSE(instance->outputs().empty());

  // Instances must die before the registry unloads their library.
  instance.reset();
}

TEST_F(LoaderTest, RejectsLibraryThatIsNotAComponent)
{
  QString message;
  std::unique_ptr<ComponentLibrary> library =
    ComponentLibrary::load(fixturePath(QStringLiteral("notacomponent")),
                           message);

  EXPECT_EQ(library, nullptr);
  EXPECT_TRUE(message.contains(QStringLiteral("not a HydroCouple component")))
    << message.toStdString();
}

// The mismatched library would hand back a garbage pointer if its info entry
// point were called; reaching this assertion means the loader stopped earlier.
TEST_F(LoaderTest, RejectsForeignAbiStampWithoutCallingIntoTheLibrary)
{
  QString message;
  std::unique_ptr<ComponentLibrary> library =
    ComponentLibrary::load(fixturePath(QStringLiteral("badabicomponent")),
                           message);

  EXPECT_EQ(library, nullptr);
  EXPECT_TRUE(message.contains(QStringLiteral("incompatible toolchain")))
    << message.toStdString();
  EXPECT_TRUE(message.contains(QStringLiteral("iface=99")))
    << "the diagnostic should quote what the library actually reported";
}

// A component written for HydroCouple's Python loader must stay loadable here,
// or a component's loadability depends on which host opens it.
TEST_F(LoaderTest, LoadsLegacyPythonCompatibleComponent)
{
  QString message;
  std::unique_ptr<ComponentLibrary> library =
    ComponentLibrary::load(fixturePath(QStringLiteral("legacycomponent")),
                           message);

  ASSERT_NE(library, nullptr) << message.toStdString();
  ASSERT_NE(library->componentInfo(), nullptr);

  EXPECT_EQ(library->componentInfo()->id(), std::string("composer.test.legacy"));

  // It loaded, but its toolchain could not be verified — and it must say so
  // rather than appearing validated.
  EXPECT_TRUE(library->isUnstamped());
  EXPECT_NE(library->abiStamp(), ComponentLibrary::hostAbiStamp());
}

TEST_F(LoaderTest, LegacyComponentInstantiatesThroughTheRegistry)
{
  ComponentRegistry registry;
  QString message;

  ASSERT_NE(registry.loadLibrary(fixturePath(QStringLiteral("legacycomponent")),
                                 message),
            nullptr)
    << message.toStdString();

  std::unique_ptr<HydroCouple::IModelComponent> instance =
    registry.createInstance(QStringLiteral("composer.test.legacy"), message);

  ASSERT_NE(instance, nullptr) << message.toStdString();
  instance->initialize();
  EXPECT_EQ(instance->status(),
            HydroCouple::IModelComponent::ComponentStatus::Initialized);
  instance.reset();
}

TEST_F(LoaderTest, RejectsMissingFileWithoutCrashing)
{
  QString message;
  std::unique_ptr<ComponentLibrary> library = ComponentLibrary::load(
    QDir(fixtureDir()).absoluteFilePath(QStringLiteral("no_such_library.dylib")),
    message);

  EXPECT_EQ(library, nullptr);
  EXPECT_TRUE(message.contains(QStringLiteral("no such file")))
    << message.toStdString();
}

// A plugin directory legitimately mixes components with plain libraries.
TEST_F(LoaderTest, ScanDirectoryKeepsComponentsAndRecordsRejections)
{
  ComponentRegistry registry;
  registry.setSearchPaths({fixtureDir()});

  const int loaded = registry.refresh();

  EXPECT_EQ(loaded, 3) << "the stamped component, the legacy component, and "
                          "the adapter factory should load";
  ASSERT_EQ(registry.entries().size(), 3u);

  std::set<std::string> ids;
  for (HydroCouple::IComponentInfo *info : registry.entries())
  {
    ids.insert(info->id());
  }
  EXPECT_TRUE(ids.count("composer.test.component") == 1);
  EXPECT_TRUE(ids.count("composer.test.legacy") == 1);
  EXPECT_TRUE(ids.count("composer.test.adapterfactory") == 1);

  // The rejections are diagnosable rather than silent.
  const std::vector<ComponentLoadFailure> failures = registry.failures();
  EXPECT_GE(failures.size(), 2u);

  bool sawBadAbi = false;
  for (const ComponentLoadFailure &failure : failures)
  {
    if (failure.filePath.contains(QStringLiteral("badabicomponent")))
    {
      sawBadAbi = true;
      EXPECT_TRUE(failure.message.contains(QStringLiteral("incompatible")));
    }
  }
  EXPECT_TRUE(sawBadAbi);
}

TEST_F(LoaderTest, LoadingSamePathTwiceYieldsOneEntry)
{
  ComponentRegistry registry;
  QString message;

  const QString path = fixturePath(QStringLiteral("testcomponent"));

  HydroCouple::IComponentInfo *first = registry.loadLibrary(path, message);
  HydroCouple::IComponentInfo *second = registry.loadLibrary(path, message);

  ASSERT_NE(first, nullptr) << message.toStdString();
  EXPECT_EQ(first, second) << "the info object is a library-owned singleton";
  EXPECT_EQ(registry.entries().size(), 1u);
}

// ── Adapter-factory components (CONNECT B1) ─────────────────────────────────

#include "hydrocouplesdk/data/exchangeitems1d.h"
#include "hydrocouplesdk/core/dimension.h"
#include "hydrocouplesdk/core/valuedefinition.h"

TEST_F(LoaderTest, RegistryTellsModelComponentsFromAdapterFactories)
{
  ComponentRegistry registry;
  QString message;

  ASSERT_NE(registry.loadLibrary(fixturePath(QStringLiteral("testcomponent")),
                                 message),
            nullptr)
    << message.toStdString();
  HydroCouple::IComponentInfo *factoryInfo = registry.loadLibrary(
    fixturePath(QStringLiteral("testadapterfactory")), message);
  ASSERT_NE(factoryInfo, nullptr) << message.toStdString();

  EXPECT_EQ(ComponentRegistry::kindOf(
              registry.entry(QStringLiteral("composer.test.component"))),
            ComponentRegistry::ComponentKind::Model);
  EXPECT_EQ(ComponentRegistry::kindOf(factoryInfo),
            ComponentRegistry::ComponentKind::AdapterFactory);

  const auto models =
    registry.entries(ComponentRegistry::ComponentKind::Model);
  ASSERT_EQ(models.size(), 1u);
  EXPECT_EQ(models[0]->id(), "composer.test.component");

  const auto factories =
    registry.entries(ComponentRegistry::ComponentKind::AdapterFactory);
  ASSERT_EQ(factories.size(), 1u);
  EXPECT_EQ(factories[0]->id(), "composer.test.adapterfactory");

  // Placing a factory as a component is refused with a message that says
  // where adapters actually belong.
  std::unique_ptr<HydroCouple::IModelComponent> instance =
    registry.createInstance(QStringLiteral("composer.test.adapterfactory"),
                            message);
  EXPECT_EQ(instance, nullptr);
  EXPECT_TRUE(message.contains(QStringLiteral("attach to connections")))
    << message.toStdString();
}

TEST_F(LoaderTest, CreatesAnAdapterFactoryFromItsLibrary)
{
  ComponentRegistry registry;
  QString message;

  ASSERT_NE(registry.loadLibrary(
              fixturePath(QStringLiteral("testadapterfactory")), message),
            nullptr)
    << message.toStdString();

  std::unique_ptr<HydroCouple::IAdaptedOutputFactoryComponent> factory =
    registry.createAdaptedOutputFactory(
      QStringLiteral("composer.test.adapterfactory"), message);
  ASSERT_NE(factory, nullptr) << message.toStdString();
  EXPECT_NE(factory->componentInfo(), nullptr);

  // Offer → create → transform, across the library boundary: the whole
  // factory pathway, not just a successful dlopen.
  HydroCouple::SDK::Dimension dimension{"i"};
  std::unique_ptr<HydroCouple::SDK::Quantity> quantity{
    HydroCouple::SDK::Quantity::unitLess("Q")};
  HydroCouple::SDK::Output1DDouble produced("produced", &dimension, 2,
                                            quantity.get(), nullptr);
  produced[0] = 3.0;
  produced[1] = 4.0;

  const std::vector<HydroCouple::IIdentity *> offerings =
    factory->getAvailableAdaptedOutputIds(&produced);
  ASSERT_EQ(offerings.size(), 1u);
  EXPECT_EQ(offerings[0]->id(), "double_it");

  std::unique_ptr<HydroCouple::IAdaptedOutput> adapted =
    factory->createAdaptedOutput(offerings[0], &produced);
  ASSERT_NE(adapted, nullptr);
  adapted->initialize();

  double values[2] = {0.0, 0.0};
  const int64_t shape[1] = {2};
  HydroCouple::BufferDescriptor destination;
  destination.data  = values;
  destination.kind  = HydroCouple::DataKind::Float64;
  destination.rank  = 1;
  destination.shape = shape;
  const int64_t start = 0;
  std::string readMessage;
  ASSERT_TRUE(adapted->getValuesInto(destination,
                                     std::span<const int64_t>(&start, 1),
                                     std::span<const int64_t>(shape, 1),
                                     &readMessage))
    << readMessage;
  EXPECT_DOUBLE_EQ(values[0], 6.0);
  EXPECT_DOUBLE_EQ(values[1], 8.0);

  // The provider's registry is non-owning; unregister, then destroy the
  // adapter and factory before the registry drops the library.
  EXPECT_TRUE(produced.removeAdaptedOutput(adapted.get()));
  adapted.reset();
  factory.reset();
}

TEST_F(LoaderTest, AModuleStyleFileNameIsScannedLikeAnyOtherPlugin)
{
  // Every SHIPPED component is a CMake MODULE library, which on macOS is
  // named ".so" and not ".dylib" — while this suite's own fixtures are
  // SHARED and so are ".dylib". A scan that knew only the SHARED spelling
  // found the fixtures and none of the real plugins, which is exactly why
  // Load Directory came back empty against a real component build.
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());

  const QString source =
    QDir(fixtureDir())
      .absoluteFilePath(QStringLiteral("libtestcomponent")
                        + ComponentLibrary::librarySuffix());
  ASSERT_TRUE(QFileInfo::exists(source)) << source.toStdString();

  // The same library, named the way a MODULE build names it.
  const QString moduleStyle =
    directory.filePath(QStringLiteral("libhcc_meshgenerator.so"));
  ASSERT_TRUE(QFile::copy(source, moduleStyle));

  ComponentRegistry registry;
  registry.setSearchPaths({directory.path()});

  EXPECT_EQ(registry.refresh(), 1)
    << "a module-style plugin was not scanned";
  ASSERT_EQ(registry.entries().size(), 1u);
  EXPECT_EQ(registry.entries().front()->id(),
            std::string("composer.test.component"));
}
