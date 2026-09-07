/*!
 * \file   graphlayout.h
 * \author Caleb Buahin
 * \brief  Automatic placement for the coupling graph.
 *
 * The legacy Composer handed this to GraphViz's `dot`. This does the same
 * job — a layered, left-to-right arrangement where a provider stands to the
 * left of everything it feeds — without the dependency, which for one
 * screenful of components is a few dozen lines rather than a native library
 * on three platforms.
 *
 * Three steps, the same three `dot` takes:
 *
 *  1. **Rank.** Longest path from a component with nothing feeding it.
 *     Peeled a layer at a time, so rank N holds exactly the components
 *     whose deepest chain of providers is N.
 *  2. **Order within a rank.** Each component is placed near the average
 *     position of what feeds it (the barycentre heuristic), swept twice.
 *     This is what stops connections crossing more than they must.
 *  3. **Place.** Ranks become columns; order within a rank becomes rows,
 *     centred so a wide rank does not drag a narrow one to the top.
 *
 * It is a pure function of the composition and the node sizes, which is
 * what makes it gateable without a scene, a window, or a mouse.
 */

#ifndef HYDROCOUPLECOMPOSER_CANVAS_GRAPHLAYOUT_H
#define HYDROCOUPLECOMPOSER_CANVAS_GRAPHLAYOUT_H

#include "hydrocouplesdk/io/compositionspec.h"

#include <QHash>
#include <QPointF>
#include <QSizeF>
#include <QString>

namespace HydroCouple::Composer
{

  /*!
   * \brief Spacing for an automatic layout, in scene units.
   */
  struct LayoutOptions
  {
      //! Free space between one column of components and the next.
      qreal columnGap = 120.0;

      //! Free space between one component and the one below it.
      qreal rowGap = 40.0;

      //! Where the top-left of the arrangement goes.
      QPointF origin{40.0, 40.0};
  };

  /*!
   * \brief Positions every component in \a spec, left to right by dependency.
   *
   * Cycles are not an error and are not broken: a feedback loop between two
   * components is a normal coupling, and its members simply share the rank
   * they are first reached at. Every component in the document comes back
   * with a position, including ones nothing connects to.
   *
   * \param spec The composition to arrange.
   * \param sizes Each component's drawn size, by id; a component missing
   *        from here is assumed to be the default size, because a document
   *        may name a component whose library is not installed and that box
   *        still has to go somewhere.
   * \param options Spacing.
   * \returns Position per component id — the top-left corner, matching what
   *          the presentation sidecar stores.
   */
  [[nodiscard]] QHash<QString, QPointF> layoutComposition(
    const HydroCouple::SDK::IO::CompositionSpec &spec,
    const QHash<QString, QSizeF> &sizes, const LayoutOptions &options = {});

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_CANVAS_GRAPHLAYOUT_H
