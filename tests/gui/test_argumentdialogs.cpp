/*!
 * \file   test_argumentdialogs.cpp
 * \brief  U2b — the window that edits one argument.
 *
 * The contract, and nothing else: a dialog is given a payload and returns
 * a payload; it never writes to a component and never writes to the
 * document; and **nothing is committed until the user asks**.
 *
 * That last one is the difference between these windows and the inline
 * editors in the dock, which commit on every valueChanged. Fine for a
 * spin box; wrong for a window with a Cancel button, because a Cancel
 * that left half the typing behind would be a lie. It is also the
 * property a careless refactor is most likely to lose, since wiring a
 * textChanged straight to the committer would make every gate here pass
 * except one.
 *
 * The committer is a callback rather than a configurator, so these run
 * with no component, no library and no document — which is the only
 * reason they run at all outside a full build.
 */

#include "configurator/argumentdescriptor.h"
#include "ui/dialogs/argumenteditordialog.h"
#include "ui/dialogs/argumenteditorfactory.h"
#include "ui/dialogs/rawargumentdialog.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>

#include <memory>
#include <vector>

using namespace HydroCouple::Composer;

namespace
{
  //! What a committed payload looked like, so a gate can say "nothing
  //! reached the component" rather than only "the dialog said so".
  struct Recorder
  {
      std::vector<nlohmann::json> accepted;
      QString refuseWith;

      [[nodiscard]] ArgumentCommitter committer()
      {
        return [this](const QString &, const nlohmann::json &payload,
                      QString &message)
        {
          if (!refuseWith.isEmpty())
          {
            message = refuseWith;

            return false;
          }

          accepted.push_back(payload);

          return true;
        };
      }
  };

  ArgumentDescriptor rawDescriptor()
  {
    ArgumentDescriptor descriptor;
    descriptor.id = QStringLiteral("kinetics");
    descriptor.caption = QStringLiteral("Kinetics");
    descriptor.description = QStringLiteral("Reaction terms.");
    descriptor.kind = ArgumentEditorKind::Raw;
    descriptor.inlineKind = ArgumentEditorKind::Raw;
    descriptor.payload = nlohmann::json{{"values", {1, 2, 3}}};

    return descriptor;
  }

  QPushButton *button(ArgumentEditorDialog *dialog,
                      QDialogButtonBox::StandardButton which)
  {
    auto *box = dialog->findChild<QDialogButtonBox *>(
      QStringLiteral("argumentButtons"));

    return box ? box->button(which) : nullptr;
  }
}

TEST(ArgumentDialogTest, ItHydratesFromItsPayloadAndGivesTheSameOneBack)
{
  // The B2 hydration contract, extended to dialogs: opened on a payload
  // and closed without an edit, what comes out must equal what went in.
  // A dialog that quietly reformatted or reordered would show a document
  // as modified every time someone looked at it.
  RawArgumentDialog dialog(rawDescriptor());

  EXPECT_EQ(dialog.payload(), rawDescriptor().payload);

  // And the *editor* shows it, which is not the same claim. payload()
  // falls back to what the window opened with when the text will not
  // parse — empty text included — so a window that hydrated nothing at
  // all would still answer correctly here while showing the user a blank
  // box. That is the bug this second half catches: the first version of
  // this gate had only the line above, and a mutation emptying the
  // editor survived it.
  EXPECT_FALSE(dialog.text().isEmpty()) << "the window opened blank";

  const nlohmann::json shown = nlohmann::json::parse(
    dialog.text().toStdString(), nullptr, false);

  EXPECT_FALSE(shown.is_discarded()) << "the window shows text it cannot read";
  EXPECT_EQ(shown, rawDescriptor().payload);
}

TEST(ArgumentDialogTest, NothingReachesTheComponentUntilTheUserAsks)
{
  // The property this whole design rests on. Typing is not committing.
  Recorder recorder;

  RawArgumentDialog dialog(rawDescriptor());
  dialog.setCommitter(recorder.committer());

  dialog.setText(QStringLiteral(R"({"values":[9,9,9]})"));

  EXPECT_TRUE(recorder.accepted.empty())
    << "an edit reached the component without the user applying it";

  // And the dialog does hold the edit — it is waiting, not ignoring.
  EXPECT_EQ(dialog.payload(), (nlohmann::json{{"values", {9, 9, 9}}}));

  ASSERT_TRUE(dialog.apply());
  ASSERT_EQ(recorder.accepted.size(), 1u);
  EXPECT_EQ(recorder.accepted.front(),
            (nlohmann::json{{"values", {9, 9, 9}}}));
}

TEST(ArgumentDialogTest, CancellingAfterEditsLeavesTheComponentAlone)
{
  Recorder recorder;

  auto *dialog = new RawArgumentDialog(rawDescriptor());
  dialog->setCommitter(recorder.committer());

  dialog->setText(QStringLiteral(R"({"values":[7]})"));

  QPushButton *cancel = button(dialog, QDialogButtonBox::Cancel);
  ASSERT_NE(cancel, nullptr);
  cancel->click();

  EXPECT_TRUE(recorder.accepted.empty())
    << "Cancel committed the edits it was supposed to discard";
  EXPECT_FALSE(dialog->hasCommitted());

  delete dialog;
}

TEST(ArgumentDialogTest, ARefusalIsShownAndNothingIsRecorded)
{
  // The component's message, verbatim, in the strip — not a message of
  // the Composer's own invention. The component knows why it said no and
  // we do not.
  Recorder recorder;
  recorder.refuseWith = QStringLiteral("rate must be positive");

  RawArgumentDialog dialog(rawDescriptor());
  dialog.setCommitter(recorder.committer());

  dialog.setText(QStringLiteral(R"({"values":[-1]})"));

  EXPECT_FALSE(dialog.apply());
  EXPECT_EQ(dialog.refusal(), QStringLiteral("rate must be positive"));
  EXPECT_TRUE(recorder.accepted.empty());
  EXPECT_FALSE(dialog.hasCommitted());
}

TEST(ArgumentDialogTest, OkClosesOnlyWhenTheComponentTookIt)
{
  // A window that shut on a refusal would throw the user's work away and
  // take the explanation with it.
  Recorder recorder;
  recorder.refuseWith = QStringLiteral("no");

  auto *dialog = new RawArgumentDialog(rawDescriptor());
  dialog->setCommitter(recorder.committer());

  QPushButton *ok = button(dialog, QDialogButtonBox::Ok);
  ASSERT_NE(ok, nullptr);

  ok->click();

  EXPECT_TRUE(dialog->isVisible() || !dialog->result())
    << "OK closed the window on a refusal";
  EXPECT_FALSE(dialog->hasCommitted());

  recorder.refuseWith.clear();
  ok->click();

  EXPECT_TRUE(dialog->hasCommitted());
  EXPECT_EQ(recorder.accepted.size(), 1u);

  delete dialog;
}

TEST(ArgumentDialogTest, TextThatIsNotJsonIsTheDialogsRefusalNotTheComponents)
{
  // A parse error is not a modelling error. Passing it on would send the
  // user looking at their model instead of at the brace they left open,
  // and would spend a component's validation on a typing mistake.
  Recorder recorder;

  RawArgumentDialog dialog(rawDescriptor());
  dialog.setCommitter(recorder.committer());

  dialog.setText(QStringLiteral("{ this is not json"));

  EXPECT_FALSE(dialog.apply());
  EXPECT_FALSE(dialog.refusal().isEmpty());
  EXPECT_TRUE(recorder.accepted.empty())
    << "unparseable text was offered to the component";

  // And the payload accessor stays safe for anyone who ignores all that:
  // it answers what the window opened with rather than throwing.
  EXPECT_EQ(dialog.payload(), rawDescriptor().payload);
}

TEST(ArgumentDialogTest, ADialogWithNothingListeningSaysSo)
{
  // A dead Apply button that reports success is the hardest kind of bug
  // to notice, because everything looks like it worked.
  RawArgumentDialog dialog(rawDescriptor());

  EXPECT_FALSE(dialog.apply());
  EXPECT_FALSE(dialog.refusal().isEmpty());
}

TEST(ArgumentDialogTest, ApplyingTwiceCommitsTwiceAndKeepsTheSecond)
{
  // Apply is not Ok: the window stays open and remains usable, which is
  // the whole point of having both.
  Recorder recorder;

  RawArgumentDialog dialog(rawDescriptor());
  dialog.setCommitter(recorder.committer());

  dialog.setText(QStringLiteral(R"({"values":[1]})"));
  ASSERT_TRUE(dialog.apply());

  dialog.setText(QStringLiteral(R"({"values":[2]})"));
  ASSERT_TRUE(dialog.apply());

  ASSERT_EQ(recorder.accepted.size(), 2u);
  EXPECT_EQ(recorder.accepted.back(), (nlohmann::json{{"values", {2}}}));
  EXPECT_TRUE(dialog.refusal().isEmpty());
}

TEST(ArgumentDialogTest, ARefusalIsClearedOnceSomethingIsAccepted)
{
  // A strip still showing yesterday's complaint about a value that has
  // since been accepted is worse than an empty one.
  Recorder recorder;
  recorder.refuseWith = QStringLiteral("too small");

  RawArgumentDialog dialog(rawDescriptor());
  dialog.setCommitter(recorder.committer());

  EXPECT_FALSE(dialog.apply());
  ASSERT_FALSE(dialog.refusal().isEmpty());

  recorder.refuseWith.clear();

  EXPECT_TRUE(dialog.apply());
  EXPECT_TRUE(dialog.refusal().isEmpty());
}

TEST(ArgumentDialogTest, EveryKindGetsAWindowAndTheFactorySaysWhichAreTyped)
{
  // An argument the Composer cannot type is still one the user may need
  // to change, so the factory never returns nothing.
  const ArgumentEditorKind all[] = {
    ArgumentEditorKind::Categorical, ArgumentEditorKind::Number,
    ArgumentEditorKind::Integer,     ArgumentEditorKind::Boolean,
    ArgumentEditorKind::Text,        ArgumentEditorKind::FilePath,
    ArgumentEditorKind::Table,       ArgumentEditorKind::Quantity,
    ArgumentEditorKind::TimeSeries,  ArgumentEditorKind::Mesh,
    ArgumentEditorKind::Geometry,    ArgumentEditorKind::Raster,
    ArgumentEditorKind::IdTable,     ArgumentEditorKind::Duration,
    ArgumentEditorKind::Crs,         ArgumentEditorKind::LongText,
    ArgumentEditorKind::Raw,
  };

  for (ArgumentEditorKind kind : all)
  {
    ArgumentDescriptor descriptor = rawDescriptor();
    descriptor.kind = kind;

    std::unique_ptr<ArgumentEditorDialog> dialog(
      createArgumentEditor(descriptor));

    ASSERT_NE(dialog, nullptr)
      << "no window for " << argumentEditorKindName(kind).toStdString();

    EXPECT_EQ(dialog->descriptor().kind, kind);

    // Whatever the window is, the hydration contract holds for it.
    EXPECT_EQ(dialog->payload(), descriptor.payload)
      << argumentEditorKindName(kind).toStdString()
      << " did not give back the payload it was opened on";

    // And until U2c lands them, none of these is a typed editor. The
    // dock reads this to decide what Edit… promises, so a kind wrongly
    // reported as typed would overstate what is behind the button.
    EXPECT_FALSE(hasTypedEditor(kind))
      << argumentEditorKindName(kind).toStdString()
      << " claims a typed editor it does not have";
  }
}

TEST(ArgumentDialogTest, TheWindowNamesTheArgumentItEdits)
{
  // Several of these are open at once, modelessly, over a map. A row of
  // identical windows titled "Edit argument" would be unusable.
  RawArgumentDialog dialog(rawDescriptor());

  EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Kinetics"));
  EXPECT_EQ(dialog.objectName(),
            QStringLiteral("argumentDialog_kinetics"));

  auto *caption =
    dialog.findChild<QLabel *>(QStringLiteral("argumentCaption"));
  ASSERT_NE(caption, nullptr);
  EXPECT_EQ(caption->text(), QStringLiteral("Kinetics"));

  auto *description =
    dialog.findChild<QLabel *>(QStringLiteral("argumentDescription"));
  ASSERT_NE(description, nullptr);
  EXPECT_EQ(description->text(), QStringLiteral("Reaction terms."));
}

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
