#include "scene/navigation.h"

#include <algorithm>
#include <cmath>

namespace HydroCouple::Composer
{
  namespace
  {
    //! The tilt a new 3D view starts at, and the one Reset returns to.
    //! Camera's own default; named here so the two cannot drift apart
    //! without someone noticing they are two numbers.
    constexpr double kDefaultElevation = 45.0;

    //! Sensitivity bounds. A drag across a 1000-pixel view turns the
    //! camera between a quarter turn and three full ones — outside that
    //! the view is either unturnable or uncontrollable, and neither is a
    //! setting anyone chose on purpose.
    constexpr double kMinDegreesPerPixel = 0.05;
    constexpr double kMaxDegreesPerPixel = 1.2;

    //! A notch must actually move. Exactly 1.0 is a wheel that does
    //! nothing, and below 1.0 it reverses — which is what the inverted
    //! flag is for, and having two ways to invert is a way to invert
    //! twice by accident.
    constexpr double kMinZoomPerNotch = 1.01;
    constexpr double kMaxZoomPerNotch = 4.0;
  } // namespace

  ViewLink viewLinkFromName(const QString &name)
  {
    if (name == QLatin1String("Never"))
    {
      return ViewLink::Never;
    }

    // Anything else, including a settings file hand-edited into nonsense,
    // is the behaviour the application had before this was a preference.
    return ViewLink::OnTabSwitch;
  }

  QString viewLinkName(ViewLink link)
  {
    if (link == ViewLink::Never)
    {
      return QStringLiteral("Never");
    }

    return QStringLiteral("OnTabSwitch");
  }

  PanModifier panModifierFromName(const QString &name)
  {
    if (name == QLatin1String("ShiftDrag"))
    {
      return PanModifier::ShiftDrag;
    }

    return PanModifier::MiddleDrag;
  }

  QString panModifierName(PanModifier modifier)
  {
    if (modifier == PanModifier::ShiftDrag)
    {
      return QStringLiteral("ShiftDrag");
    }

    return QStringLiteral("MiddleDrag");
  }

  OrbitStep orbitStep(const QPoint &travel, double degreesPerPixel)
  {
    const double rate =
      std::clamp(degreesPerPixel, kMinDegreesPerPixel, kMaxDegreesPerPixel);

    OrbitStep step;

    // Dragging right turns the scene right, which means turning the
    // camera the other way — the sign that makes orbiting feel like
    // turning an object rather than steering past one.
    step.azimuth = -double(travel.x()) * rate;
    step.elevation = double(travel.y()) * rate;

    return step;
  }

  double wheelDollyFactor(int angleDeltaY, double zoomPerNotch, bool inverted)
  {
    // Qt reports eighths of a degree and a notch is fifteen degrees, so
    // one notch is 120. Getting this wrong is not subtle — /8 would make
    // the wheel fifteen times too sensitive — but it is the sort of wrong
    // that reads as a "feel" problem rather than as a bug.
    const double notches = double(angleDeltaY) / 120.0;

    // No special case for zero, deliberately. pow(x, 0) is exactly 1.0
    // for every finite x, so a wheel event reporting no movement already
    // returns exactly 1.0 and the caller can still tell "nothing
    // happened" from "something very small happened". An early return
    // here would be a branch that cannot change an answer — and a branch
    // that cannot change an answer is one nobody will notice deleting.
    const double perNotch =
      std::clamp(zoomPerNotch, kMinZoomPerNotch, kMaxZoomPerNotch);

    // Wheel forward zooms in: the camera comes closer, so the factor is
    // below one. Inverting is a single negation of the exponent rather
    // than a second expression, because two expressions that must remain
    // each other's inverse will not remain each other's inverse.
    const double exponent = inverted ? -notches : notches;

    return std::pow(1.0 / perNotch, exponent);
  }

  bool pressShouldPan(Qt::MouseButton button, Qt::KeyboardModifiers modifiers,
                      PanModifier setting)
  {
    // Middle and right always pan, under every setting and every tool.
    // The convention is older than the preference, every GIS 3D view has
    // it, and taking it away would break muscle memory to no purpose.
    if (button == Qt::MiddleButton || button == Qt::RightButton)
    {
      return true;
    }

    return setting == PanModifier::ShiftDrag && button == Qt::LeftButton
           && modifiers.testFlag(Qt::ShiftModifier);
  }

  void namedViewAngles(NamedView view, double &azimuth, double &elevation)
  {
    switch (view)
    {
      case NamedView::Top:
        // Azimuth untouched: a plan view has no meaningful one, and
        // spinning the map because the user asked to look down would be
        // answering a question they did not ask. The same rule the gizmo's
        // Up arm follows, and deliberately the same — two controls that
        // mean "look down" must not mean two different things.
        elevation = 90.0;

        return;

      case NamedView::Reset:
        break;
    }

    azimuth = 0.0;
    elevation = kDefaultElevation;
  }

} // namespace HydroCouple::Composer
