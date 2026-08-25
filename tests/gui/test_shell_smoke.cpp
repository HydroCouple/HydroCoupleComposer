/*!
 * \file   test_shell_smoke.cpp
 * \brief  Phase A1 verification — the shell builds, launches offscreen, and
 *         the HydroCouple/HydroCoupleSDK dependencies are genuinely linked.
 *
 * These assertions are deliberately behavioural rather than link-level: the
 * window is constructed and its assembled children inspected, and an exported
 * SDK symbol is *called* so a headers-only or mis-resolved dependency fails
 * here rather than silently much later.
 */

#include "core/composerapplication.h"
#include "core/version.h"
#include "map/layerstackmodel.h"
#include "probelayer.h"
#include "map/mapcanvas.h"
#include "map/maplayer.h"
#include "map/tilegrid.h"
#include "ui/composermainwindow.h"
#include "ui/panels/layertreepanel.h"
#include "ui/theme/thememanager.h"
#include "ui/toolbars/ribbonbar.h"

#include "hydrocouple.h"
#include "hydrocouplesdk/io/compositionspec.h"
#include "hydrocouplesdk/version.h"

#include <gtest/gtest.h>

#include <QAction>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QProxyStyle>
#include <QStyle>
#include <QToolButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTreeView>

using namespace HydroCouple::Composer;

namespace
{
  /*!
   * \brief Owns the single QApplication shared by every test in this binary.
   */
  class ShellTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_shell_smoke";
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

  ComposerApplication *ShellTest::s_app = nullptr;
}

// The application object carries the identity QSettings and crash reporting
// depend on.
TEST_F(ShellTest, ApplicationIdentityIsConfigured)
{
  ASSERT_NE(qApp, nullptr);
  EXPECT_EQ(QApplication::organizationName(), QStringLiteral("HydroCouple"));
  EXPECT_EQ(QApplication::applicationName(),
            QStringLiteral("HydroCoupleComposer"));
  EXPECT_EQ(QApplication::applicationVersion(),
            QStringLiteral(COMPOSER_VERSION));
}

// The window must actually assemble its shell, not merely construct.
TEST_F(ShellTest, MainWindowAssemblesShell)
{
  ComposerMainWindow window;
  // Offscreen runs must not let a closing window terminate the test process.
  window.setAttribute(Qt::WA_QuitOnClose, false);
  window.show();

  EXPECT_FALSE(window.windowTitle().isEmpty());

  // The composition and the map share the central area as tabs, and the
  // session's document, registry and panels are all assembled.
  ASSERT_NE(window.canvas(), nullptr);
  ASSERT_NE(window.mapCanvas(), nullptr);

  auto *workspace =
    window.findChild<QTabWidget *>(QStringLiteral("workspaceTabs"));
  ASSERT_NE(workspace, nullptr);
  EXPECT_EQ(window.centralWidget(), workspace);
  EXPECT_NE(workspace->indexOf(window.canvas()), -1);
  EXPECT_NE(workspace->indexOf(window.mapCanvas()), -1);

  EXPECT_NE(window.document(), nullptr);
  EXPECT_NE(window.registry(), nullptr);
  EXPECT_NE(window.configurator(), nullptr);

  // The map's layer stack is shared: the tree and the canvas must be two
  // views of one model, not two copies of a list.
  ASSERT_NE(window.layerStack(), nullptr);
  ASSERT_NE(window.layerTree(), nullptr);
  EXPECT_EQ(window.mapCanvas()->model(), window.layerStack());
  EXPECT_EQ(window.layerTree()->model(), window.layerStack());

  EXPECT_NE(window.findChild<QListView *>(QStringLiteral("paletteView")),
            nullptr);
  EXPECT_NE(window.findChild<QPlainTextEdit *>(QStringLiteral("logView")),
            nullptr);

  ASSERT_NE(window.menuBar(), nullptr);
  for (const QString &menu :
       {QStringLiteral("fileMenu"), QStringLiteral("editMenu"),
        QStringLiteral("componentsMenu"), QStringLiteral("runMenu"),
        QStringLiteral("helpMenu")})
  {
    EXPECT_NE(window.findChild<QMenu *>(menu), nullptr)
      << menu.toStdString() << " is missing";
  }

  // Run controls exist and start in a coherent state: nothing is running yet.
  auto *run = window.findChild<QAction *>(QStringLiteral("runAction"));
  auto *stop = window.findChild<QAction *>(QStringLiteral("stopAction"));
  ASSERT_NE(run, nullptr);
  ASSERT_NE(stop, nullptr);
  EXPECT_TRUE(run->isEnabled());
  EXPECT_FALSE(stop->isEnabled());

  ASSERT_NE(window.statusBar(), nullptr);
  EXPECT_FALSE(window.statusBar()->currentMessage().isEmpty());
}

// The application must wear the same Fusion chrome as openswmm.gui, not the
// platform default — otherwise the two tools look like different products.
TEST_F(ShellTest, UsesTheSharedFusionTheme)
{
  ASSERT_NE(qApp, nullptr);

  ASSERT_NE(QApplication::style(), nullptr);

  // An application-wide stylesheet makes Qt wrap the real style in a private
  // QStyleSheetStyle proxy that exposes no accessible base, so
  // QApplication::style() reports the proxy. Lifting the sheet for the length
  // of the check reveals the actual style; it is restored immediately.
  const QString sheet = qApp->styleSheet();
  qApp->setStyleSheet(QString());

  const QString styleClass =
    QString::fromLatin1(QApplication::style()->metaObject()->className());

  qApp->setStyleSheet(sheet);

  EXPECT_TRUE(styleClass.contains(QStringLiteral("Fusion"), Qt::CaseInsensitive))
    << "base style is " << styleClass.toStdString() << ", not Fusion";

  // The token palette is installed, not merely the style.
  const QPalette palette = QApplication::palette();
  const ThemeColors &colors = ThemeManager::instance()->colors();

  EXPECT_EQ(palette.color(QPalette::Window), colors.surfaceWindow);
  EXPECT_EQ(palette.color(QPalette::Base), colors.surfaceRaised);
  EXPECT_EQ(palette.color(QPalette::Highlight), colors.selectionFill);

  // Offscreen platforms report an unknown OS scheme; light is the floor, so
  // headless renders do not depend on the host's appearance.
  EXPECT_EQ(ThemeManager::instance()->effectiveScheme(),
            Qt::ColorScheme::Light);

  // The focus-ring overlay is present, since Fusion's own focus rect is a
  // faint dotted line that fails to show keyboard focus.
  EXPECT_TRUE(qApp->styleSheet().contains(QStringLiteral("outline")));
}

TEST_F(ShellTest, AppearanceModesResolveAndAreNamed)
{
  ThemeManager *theme = ThemeManager::instance();
  const ThemeManager::Mode original = theme->mode();

  theme->setMode(ThemeManager::Mode::Dark);
  EXPECT_EQ(theme->effectiveScheme(), Qt::ColorScheme::Dark);
  EXPECT_NE(theme->colors().surfaceWindow,
            QColor(0xF5, 0xF5, 0xF7));

  theme->setMode(ThemeManager::Mode::Light);
  EXPECT_EQ(theme->effectiveScheme(), Qt::ColorScheme::Light);

  EXPECT_EQ(ThemeManager::modeFromString(QStringLiteral("Dark")),
            ThemeManager::Mode::Dark);
  EXPECT_EQ(ThemeManager::modeToString(ThemeManager::Mode::System),
            QStringLiteral("System"));

  theme->setMode(original);
  theme->apply();
}

// The window must carry the ribbon, not a plain button strip — the toolbar
// style is shared with openswmm.gui.
TEST_F(ShellTest, PresentsARibbonToolbar)
{
  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);
  window.show();

  auto *ribbon = window.findChild<RibbonBar *>(QStringLiteral("ribbonBar"));
  ASSERT_NE(ribbon, nullptr) << "no ribbon bar";

  // Tabs, each carrying captioned groups.
  EXPECT_EQ(ribbon->currentTab(), QStringLiteral("home"));

  const QList<RibbonGroup *> homeGroups =
    ribbon->groups(QStringLiteral("home"));
  ASSERT_FALSE(homeGroups.isEmpty()) << "the home tab has no groups";

  QStringList captions;
  for (RibbonGroup *group : homeGroups)
  {
    captions.append(group->caption());
  }

  EXPECT_TRUE(captions.contains(QStringLiteral("Simulation")))
    << captions.join(QStringLiteral(", ")).toStdString();

  ribbon->setCurrentTab(QStringLiteral("view"));
  EXPECT_EQ(ribbon->currentTab(), QStringLiteral("view"));
  EXPECT_FALSE(ribbon->groups(QStringLiteral("view")).isEmpty());
}

// Ribbon faces are large icon-over-label buttons at openswmm.gui's metrics.
TEST_F(ShellTest, RibbonUsesSharedMetricsAndHostsTheActions)
{
  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);

  auto *ribbon = window.findChild<RibbonBar *>(QStringLiteral("ribbonBar"));
  ASSERT_NE(ribbon, nullptr);

  RibbonGroup *simulation = nullptr;
  for (RibbonGroup *group : ribbon->groups(QStringLiteral("home")))
  {
    if (group->caption() == QStringLiteral("Simulation"))
    {
      simulation = group;
    }
  }
  ASSERT_NE(simulation, nullptr);

  EXPECT_EQ(simulation->height(), kRibbonRowHeight);

  auto *run = window.findChild<QAction *>(QStringLiteral("runAction"));
  ASSERT_NE(run, nullptr);

  QToolButton *button = simulation->buttonForAction(run);
  ASSERT_NE(button, nullptr) << "Run is not on the ribbon";
  EXPECT_EQ(button->toolButtonStyle(), Qt::ToolButtonTextUnderIcon);
  EXPECT_EQ(button->iconSize(), QSize(kRibbonIconFull, kRibbonIconFull));

  // Compact mode shrinks icons and moves labels beside them.
  ribbon->setMode(RibbonMode::Compact);
  EXPECT_EQ(button->toolButtonStyle(), Qt::ToolButtonTextBesideIcon);
  EXPECT_EQ(button->iconSize(), QSize(kRibbonIconCompact, kRibbonIconCompact));

  ribbon->setMode(RibbonMode::Full);
}

// Calling an exported SDK function proves the dylib is linked and loadable —
// a headers-only or mis-resolved dependency cannot pass this.
TEST_F(ShellTest, HydroCoupleSDKIsLinkedAndCallable)
{
  const std::string name = HydroCouple::SDK::IO::executionModeName(
    HydroCouple::SDK::IO::ExecutionMode::Run);

  EXPECT_FALSE(name.empty());

  HydroCouple::SDK::IO::ExecutionMode roundTrip{};
  EXPECT_TRUE(
    HydroCouple::SDK::IO::executionModeFromName(name, roundTrip));
  EXPECT_EQ(roundTrip, HydroCouple::SDK::IO::ExecutionMode::Run);
}

// The interface headers must resolve and agree with the SDK they pair with.
TEST_F(ShellTest, InterfaceAndSdkVersionsArePresent)
{
  EXPECT_STRNE(HYDROCOUPLESDK_VERSION, "");
  EXPECT_GE(HYDROCOUPLESDK_VERSION_MAJOR, 2);
}

// C1b — the map is reachable and its navigation commands reach the canvas.
TEST_F(ShellTest, MapNavigationIsWiredToTheCanvas)
{
  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);
  window.show();

  auto *ribbon = window.findChild<RibbonBar *>(QStringLiteral("ribbonBar"));
  ASSERT_NE(ribbon, nullptr);

  RibbonGroup *navigate = nullptr;
  for (RibbonGroup *group : ribbon->groups(QStringLiteral("map")))
  {
    if (group->caption() == QStringLiteral("Navigate"))
    {
      navigate = group;
    }
  }
  ASSERT_NE(navigate, nullptr) << "the ribbon has no Map ▸ Navigate group";

  auto *zoomFull = window.findChild<QAction *>(QStringLiteral("zoomFullAction"));
  auto *zoomIn = window.findChild<QAction *>(QStringLiteral("zoomInAction"));
  ASSERT_NE(zoomFull, nullptr);
  ASSERT_NE(zoomIn, nullptr);
  EXPECT_NE(navigate->buttonForAction(zoomFull), nullptr);

  // A layer far from the origin, so framing it is observable.
  window.layerStack()->addLayer(new HydroCouple::Composer::Testing::ProbeLayer(
    QStringLiteral("probe"), QRectF(5000.0, 5000.0, 100.0, 100.0)));

  // Look somewhere else first. The canvas frames the first layer to arrive on
  // its own, so triggering the action on a freshly-populated map would assert
  // nothing about the action at all.
  window.mapCanvas()->setVisibleExtent(QRectF(-10000.0, -10000.0, 10.0, 10.0));
  ASSERT_FALSE(window.mapCanvas()->transform().visibleExtent().contains(
    QPointF(5050.0, 5050.0)));

  zoomFull->trigger();

  const QRectF framed = window.mapCanvas()->transform().visibleExtent();
  EXPECT_TRUE(framed.contains(QPointF(5050.0, 5050.0)))
    << "Zoom to Full Extent did not reach the map";

  const double scale = window.mapCanvas()->transform().scale();
  zoomIn->trigger();
  EXPECT_GT(window.mapCanvas()->transform().scale(), scale);
}

// The map tab is laid out only when it is first shown, so a framing chosen
// before that — the ordinary case, since a session opens on the composition —
// has to survive the widget's real layout. It did not: resizeEvent set the
// viewport itself, which left the re-framing with nothing to notice.
TEST_F(ShellTest, FramingSurvivesTheMapTabsFirstLayout)
{
  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);
  window.resize(1400, 880);
  window.show();

  const QRectF extent(0.0, 0.0, 1000.0, 700.0);

  window.layerStack()->addLayer(new HydroCouple::Composer::Testing::ProbeLayer(
    QStringLiteral("probe"), extent));
  window.mapCanvas()->zoomToFullExtent();

  auto *workspace =
    window.findChild<QTabWidget *>(QStringLiteral("workspaceTabs"));
  ASSERT_NE(workspace, nullptr);
  workspace->setCurrentWidget(window.mapCanvas());

  QApplication::processEvents();
  QApplication::processEvents();

  const QRectF visible = window.mapCanvas()->transform().visibleExtent();

  EXPECT_TRUE(visible.adjusted(-1.0, -1.0, 1.0, 1.0).contains(extent));
  EXPECT_LT(visible.width(), extent.width() * 3.0)
    << "the map is drawn far smaller than the space it has";
}

// Framing a layer from the tree must bring the map forward: acting on a tab
// the user cannot see looks like nothing happened.
TEST_F(ShellTest, FramingALayerFromTheTreeShowsTheMap)
{
  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);
  window.show();

  auto *workspace =
    window.findChild<QTabWidget *>(QStringLiteral("workspaceTabs"));
  ASSERT_NE(workspace, nullptr);
  ASSERT_EQ(workspace->currentWidget(), window.canvas())
    << "the composition should be the tab a session opens on";

  auto *layer = new HydroCouple::Composer::Testing::ProbeLayer(
    QStringLiteral("probe"), QRectF(0.0, 0.0, 10.0, 10.0));
  window.layerStack()->addLayer(layer);

  window.layerTree()->view()->setCurrentIndex(
    window.layerStack()->index(0, 0));

  auto *zoomButton = window.layerTree()->findChild<QToolButton *>(
    QStringLiteral("layerZoomButton"));
  ASSERT_NE(zoomButton, nullptr);
  zoomButton->click();

  EXPECT_EQ(workspace->currentWidget(), window.mapCanvas());
}

// C1d — the basemap chooser keeps exactly one backdrop, at the bottom.
TEST_F(ShellTest, BasemapChoiceReplacesRatherThanStacks)
{
  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);
  window.show();

  auto *basemapMenu = window.findChild<QMenu *>(QStringLiteral("basemapMenu"));
  ASSERT_NE(basemapMenu, nullptr) << "there is no basemap chooser";

  const QList<QAction *> choices = basemapMenu->actions();
  ASSERT_GE(choices.size(), 3) << "None plus at least two providers";

  const auto basemapCount = [&window]
  {
    int count = 0;

    for (const HydroCouple::Composer::MapLayer *layer :
         window.layerStack()->layers())
    {
      if (layer->isBasemap())
      {
        ++count;
      }
    }

    return count;
  };

  choices.at(1)->trigger();
  ASSERT_EQ(basemapCount(), 1);

  // Switching providers must replace the backdrop: two would fight for the
  // same pixels and only the upper one would ever be seen.
  choices.at(2)->trigger();
  EXPECT_EQ(basemapCount(), 1);

  // And it belongs at the bottom of the stack, under every data layer.
  EXPECT_TRUE(window.layerStack()
                ->layerAt(window.layerStack()->rowCount() - 1)
                ->isBasemap());

  // "None" removes it.
  choices.at(0)->trigger();
  EXPECT_EQ(basemapCount(), 0);
}

// A basemap covers the world, so it must not be what "zoom to everything"
// frames once there is data to look at.
TEST_F(ShellTest, ZoomToFullExtentIgnoresTheBasemap)
{
  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);
  window.resize(1400, 880);
  window.show();

  auto *basemapMenu = window.findChild<QMenu *>(QStringLiteral("basemapMenu"));
  ASSERT_NE(basemapMenu, nullptr);
  basemapMenu->actions().at(1)->trigger();

  // With only a backdrop loaded, framing everything means the world.
  window.mapCanvas()->zoomToFullExtent();
  EXPECT_GT(window.mapCanvas()->transform().visibleExtent().width(),
            HydroCouple::Composer::TileGrid::kWorldHalfSpan);

  const QRectF data(1000.0, 2000.0, 500.0, 500.0);
  window.layerStack()->addLayer(new HydroCouple::Composer::Testing::ProbeLayer(
    QStringLiteral("data"), data));

  window.mapCanvas()->zoomToFullExtent();

  const QRectF framed = window.mapCanvas()->transform().visibleExtent();

  EXPECT_LT(framed.width(), HydroCouple::Composer::TileGrid::kWorldHalfSpan)
    << "the basemap dragged the view out to the whole planet";
  EXPECT_TRUE(framed.adjusted(-1.0, -1.0, 1.0, 1.0).contains(data));
}
