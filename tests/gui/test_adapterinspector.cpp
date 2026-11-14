/*!
 * \file   test_adapterinspector.cpp
 * \brief  CONNECT C5 — the adapter/connection inspector and its selection
 *         routing.
 *
 * Driven through the real main window, because the property under test is
 * the ROUTE: canvas selection → inspector address → document edit. The
 * inspector holds an address (connection identity + step index), never an
 * item pointer, and every edit goes back through the document.
 */

#include "canvas/compositioncanvas.h"
#include "canvas/compositionscene.h"
#include "configurator/adapterinspector.h"
#include "core/composerapplication.h"
#include "plugins/componentlibrary.h"
#include "ui/composermainwindow.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QTest>
#include <QUndoStack>

using namespace HydroCouple::Composer;
using ConnectionSpec = HydroCouple::SDK::IO::ConnectionSpec;

namespace
{
  QString fixturePath(const QString &stem)
  {
    return QDir(QStringLiteral(COMPOSER_FIXTURE_DIR))
      .absoluteFilePath(QStringLiteral("lib") + stem +
                        ComponentLibrary::librarySuffix());
  }

  //! A resolvable pair whose connection carries a two-step chain, the first
  //! step with an editable argument payload.
  QByteArray adaptedDocument()
  {
    return R"({
      "schema_version": "1.1",
      "components": [
        { "id": "prov",
          "info": { "component_info_id": "composer.test.component" } },
        { "id": "consumer",
          "info": { "component_info_id": "composer.test.component" } }
      ],
      "connections": [
        { "from": { "component": "prov", "output": "values",
                    "adapted_outputs": [
                      { "id": "linear_transform",
                        "factory": "hydrocouple.sdk.adapters",
                        "arguments": { "multiplier": { "values": [2.0] } } },
                      { "id": "shift" } ] },
          "to": { "component": "consumer", "input": "inflow" } }
      ]
    })";
  }

  class InspectorTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_adapterinspector";
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
        window = std::make_unique<ComposerMainWindow>();
        window->setAttribute(Qt::WA_QuitOnClose, false);

        QString message;
        ASSERT_NE(window->registry()->loadLibrary(
                    fixturePath(QStringLiteral("testcomponent")), message),
                  nullptr)
          << message.toStdString();
        ASSERT_TRUE(window->document()->loadFromJson(adaptedDocument(),
                                                     message))
          << message.toStdString();

        auto *canvas = window->findChild<CompositionCanvas *>(
          QStringLiteral("compositionCanvas"));
        ASSERT_NE(canvas, nullptr);
        scene = canvas->compositionScene();
        ASSERT_NE(scene, nullptr);
        ASSERT_EQ(scene->adapterNodes().size(), 2);
      }

      [[nodiscard]] QString title() const
      {
        auto *label = window->adapterInspector()->findChild<QLabel *>(
          QStringLiteral("adapterInspectorTitle"));
        return label ? label->text() : QString();
      }

      std::unique_ptr<ComposerMainWindow> window;
      CompositionScene *scene = nullptr;

      static ComposerApplication *s_app;
  };

  ComposerApplication *InspectorTest::s_app = nullptr;
}

TEST_F(InspectorTest, SelectingAnAdapterNodeShowsItsChainStepInTheInspector)
{
  scene->adapterNodes()[1]->setSelected(true);

  EXPECT_TRUE(title().contains(QStringLiteral("shift")))
    << title().toStdString();
  EXPECT_TRUE(title().contains(QStringLiteral("step 2 of 2")))
    << title().toStdString();

  // A connection edge shows the read-only summary instead.
  scene->clearSelection();
  scene->edges().first()->setSelected(true);
  EXPECT_TRUE(title().contains(QStringLiteral("prov.values")))
    << title().toStdString();
  EXPECT_TRUE(title().contains(QStringLiteral("consumer.inflow")));

  // And clearing the selection empties the panel rather than going stale.
  scene->clearSelection();
  EXPECT_TRUE(title().contains(QStringLiteral("Nothing selected")))
    << title().toStdString();
}

TEST_F(InspectorTest, EditingAChainStepArgumentIsRecordedInTheDocument)
{
  scene->adapterNodes()[0]->setSelected(true);

  auto *editor = window->adapterInspector()->findChild<QLineEdit *>(
    QStringLiteral("adapter_argument_multiplier"));
  ASSERT_NE(editor, nullptr) << "no editor for the step's argument";

  editor->setText(QStringLiteral("{\"values\": [7.0]}"));
  QTest::keyClick(editor, Qt::Key_Return);

  const auto &chain =
    window->document()->spec().connections[0].adaptedOutputs;
  EXPECT_EQ(chain[0].arguments["multiplier"]["values"][0].get<double>(), 7.0)
    << "the edit never reached the document";
  EXPECT_EQ(window->document()->undoStack()->undoText(),
            QStringLiteral("Set 'multiplier' on adapter 'linear_transform'"));

  // JSON that does not parse is refused visibly and never reaches the
  // document.
  auto *badEditor = window->adapterInspector()->findChild<QLineEdit *>(
    QStringLiteral("adapter_argument_multiplier"));
  ASSERT_NE(badEditor, nullptr);
  badEditor->setText(QStringLiteral("{oops"));
  QTest::keyClick(badEditor, Qt::Key_Return);
  EXPECT_EQ(window->document()
              ->spec()
              .connections[0]
              .adaptedOutputs[0]
              .arguments["multiplier"]["values"][0]
              .get<double>(),
            7.0);
  EXPECT_FALSE(badEditor->styleSheet().isEmpty())
    << "the refusal was invisible";

  // When the step goes away under the panel, the address stops resolving
  // and the panel empties.
  ConnectionSpec identity;
  identity.fromComponent = "prov";
  identity.output = "values";
  identity.toComponent = "consumer";
  identity.input = "inflow";
  ASSERT_TRUE(window->document()->removeConnection(identity));
  EXPECT_TRUE(title().contains(QStringLiteral("Nothing selected")))
    << title().toStdString();
}

TEST_F(InspectorTest, TheInspectorEmptiesWhenItsAddressStopsResolving)
{
  // A BARE inspector, no scene and no selection routing to clean up after
  // it: the address-invalidation on refresh is the only thing standing
  // between this panel and editing a step that no longer exists.
  AdapterInspector inspector(window->document());

  ConnectionSpec identity;
  identity.fromComponent = "prov";
  identity.output = "values";
  identity.toComponent = "consumer";
  identity.input = "inflow";

  inspector.setChainStep(identity, 0);
  auto *label = inspector.findChild<QLabel *>(
    QStringLiteral("adapterInspectorTitle"));
  ASSERT_NE(label, nullptr);
  ASSERT_TRUE(label->text().contains(QStringLiteral("linear_transform")));

  // The step goes away under the panel (another view shortened the chain).
  ASSERT_TRUE(window->document()->removeConnectionAdapter(identity, 1));
  EXPECT_TRUE(label->text().contains(QStringLiteral("linear_transform")))
    << "step 0 still exists and should still show";

  ASSERT_TRUE(window->document()->removeConnection(identity));
  EXPECT_TRUE(label->text().contains(QStringLiteral("Nothing selected")))
    << "the panel kept editing a connection that is gone: "
    << label->text().toStdString();
}
