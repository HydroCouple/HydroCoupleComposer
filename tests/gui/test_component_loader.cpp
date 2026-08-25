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

  EXPECT_EQ(loaded, 2) << "the stamped and the legacy component should load";
  ASSERT_EQ(registry.entries().size(), 2u);

  std::set<std::string> ids;
  for (HydroCouple::IComponentInfo *info : registry.entries())
  {
    ids.insert(info->id());
  }
  EXPECT_TRUE(ids.count("composer.test.component") == 1);
  EXPECT_TRUE(ids.count("composer.test.legacy") == 1);

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
