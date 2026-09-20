/*!
 * \file   navigation.h
 * \author Caleb Buahin
 * \brief  How a drag, a wheel notch and a named view move the camera.
 *
 * The 3D view's navigation, as arithmetic over plain values rather than as
 * code inside an event handler. The reason is the one U4 paid for: the
 * parts of navigation that can be *wrong* — a sign, an inverted wheel, a
 * modifier read backwards, a "reset" that resets to the wrong place — are
 * exactly the parts that need no graphics device to check, and leaving
 * them inside QRhiWidget's event handlers puts them where no gate under
 * the offscreen platform can reach.
 *
 * What stays in SceneView is reading the preference and applying the
 * result. What lives here is every decision either could get wrong.
 */

#ifndef HYDROCOUPLECOMPOSER_SCENE_NAVIGATION_H
#define HYDROCOUPLECOMPOSER_SCENE_NAVIGATION_H

#include <QPoint>
#include <QString>

#include <Qt>

namespace HydroCouple::Composer
{
  /*!
   * \brief Whether switching tabs hands the framing over.
   */
  enum class ViewLink
  {
    OnTabSwitch,  //!< The arriving view is framed on what the other showed.
    Never,        //!< Each view keeps its own framing.
  };

  /*!
   * \brief Which drag pans the 3D view.
   *
   * Middle and right always pan whatever this says — that convention is
   * older than the preference and costs nothing to keep. This decides
   * only whether *shift* and the left button do so as well, for the
   * benefit of trackpads and mice that have no middle button at all.
   */
  enum class PanModifier
  {
    MiddleDrag,  //!< Middle or right only.
    ShiftDrag,   //!< Shift with the left button pans too.
  };

  /*!
   * \brief A named camera position the user can ask for by name.
   */
  enum class NamedView
  {
    Reset,  //!< North-up at the default tilt: where a new view starts.
    Top,    //!< Straight down, azimuth untouched.
  };

  //! The link setting named by \a name; OnTabSwitch for anything else.
  [[nodiscard]] ViewLink viewLinkFromName(const QString &name);

  //! The stored name for \a link; the inverse of viewLinkFromName().
  [[nodiscard]] QString viewLinkName(ViewLink link);

  //! The pan setting named by \a name; MiddleDrag for anything else.
  [[nodiscard]] PanModifier panModifierFromName(const QString &name);

  //! The stored name for \a modifier.
  [[nodiscard]] QString panModifierName(PanModifier modifier);

  /*!
   * \brief How far an orbit drag turns the camera.
   */
  struct OrbitStep
  {
      double azimuth = 0.0;
      double elevation = 0.0;
  };

  /*!
   * \brief The turn \a travel pixels of drag asks for.
   *
   * \param travel Pixels moved since the last event.
   * \param degreesPerPixel The sensitivity preference. Clamped to a
   *        positive, usable range: a sensitivity of zero is a view that
   *        cannot be turned at all, which reads as a broken mouse rather
   *        than as a setting, and a negative one silently inverts both
   *        axes when the user meant to slow the camera down.
   */
  [[nodiscard]] OrbitStep orbitStep(const QPoint &travel,
                                    double degreesPerPixel);

  /*!
   * \brief The dolly factor for a wheel event.
   *
   * \param angleDeltaY Qt's eighths of a degree; 120 is one notch.
   * \param zoomPerNotch How much one notch moves. Clamped above 1.
   * \param inverted Whether the user has asked for the opposite direction.
   * \returns The factor to dolly by; exactly 1.0 for no movement, which
   *          the caller should treat as "not a zoom" rather than applying.
   */
  [[nodiscard]] double wheelDollyFactor(int angleDeltaY, double zoomPerNotch,
                                        bool inverted);

  /*!
   * \brief Whether a press begins a pan.
   *
   * \param button The button pressed.
   * \param modifiers The modifiers held with it.
   * \param setting What the user asked for.
   */
  [[nodiscard]] bool pressShouldPan(Qt::MouseButton button,
                                    Qt::KeyboardModifiers modifiers,
                                    PanModifier setting);

  /*!
   * \brief The angles \a view names.
   *
   * \param view Which named view.
   * \param[in,out] azimuth Set for Reset; left alone for Top, because a
   *        plan view has no meaningful azimuth and spinning the map would
   *        answer a question the user did not ask.
   * \param[in,out] elevation Always set.
   */
  void namedViewAngles(NamedView view, double &azimuth, double &elevation);

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_SCENE_NAVIGATION_H
