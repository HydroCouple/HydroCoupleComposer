#include "ui/dialogs/argumenteditorfactory.h"

#include "ui/dialogs/rawargumentdialog.h"

namespace HydroCouple::Composer
{
  bool hasTypedEditor(ArgumentEditorKind kind)
  {
    switch (kind)
    {
      // As each dialog lands in U2c it moves from the group below to
      // here, and this function is the one place that has to change.
      case ArgumentEditorKind::Categorical:
      case ArgumentEditorKind::Number:
      case ArgumentEditorKind::Integer:
      case ArgumentEditorKind::Boolean:
      case ArgumentEditorKind::Text:
      case ArgumentEditorKind::FilePath:
      case ArgumentEditorKind::Table:
      case ArgumentEditorKind::Quantity:
      case ArgumentEditorKind::TimeSeries:
      case ArgumentEditorKind::Mesh:
      case ArgumentEditorKind::Geometry:
      case ArgumentEditorKind::Raster:
      case ArgumentEditorKind::IdTable:
      case ArgumentEditorKind::Duration:
      case ArgumentEditorKind::Crs:
      case ArgumentEditorKind::LongText:
      case ArgumentEditorKind::Raw:
        break;
    }

    // Nothing yet. Written as a switch with every kind listed rather than
    // as `return false`, so that adding a kind to the enum is a warning
    // here — a new kind with no decision recorded is exactly what this
    // function exists to prevent.
    return false;
  }

  ArgumentEditorDialog *createArgumentEditor(
    const ArgumentDescriptor &descriptor, QWidget *parent)
  {
    // Never null. An argument the Composer cannot type is still one the
    // user may need to change, and the raw editor can change any of them.
    return new RawArgumentDialog(descriptor, parent);
  }

} // namespace HydroCouple::Composer
