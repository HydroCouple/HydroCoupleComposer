/*!
 * \file   selectionhub.h
 * \author Caleb Buahin
 * \brief  SelectionHub — one selection, seen from the canvas and the map.
 *
 * The map, the 3D view and the attribute table have shared a selection
 * since C4: one layer holds it, and the three are views of that. The
 * composition canvas was outside it — selecting a component highlighted
 * nothing on the map, and picking a feature said nothing about which
 * component had produced it.
 *
 * This joins the two without giving either a pointer to the other. It
 * owns no selection: it listens to the canvas and is told about picks,
 * and it answers by asking the stack and by emitting what the layer tree
 * should make current. That keeps `map/` free of any knowledge of the
 * canvas (which is why this lives in `ui/`), and keeps the panels free of
 * each other.
 *
 * \threadsafety GUI thread only, like every view it serves.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_SELECTIONHUB_H
#define HYDROCOUPLECOMPOSER_UI_SELECTIONHUB_H

#include <QObject>
#include <QString>
#include <QVector>

namespace HydroCouple::Composer
{
  class CompositionScene;
  class LayerStackModel;
  class MapLayer;

  /*!
   * \brief Carries selection between the composition canvas and the layers.
   */
  class SelectionHub : public QObject
  {
      Q_OBJECT

    public:
      /*!
       * \brief Joins \a scene and \a stack.
       * \param scene The composition canvas's scene. Not owned; may be null.
       * \param stack The layer stack. Not owned; may be null.
       * \param parent Optional Qt parent.
       */
      SelectionHub(CompositionScene *scene, LayerStackModel *stack,
                   QObject *parent = nullptr);

      /*!
       * \brief Every layer built from the component \a componentId.
       *
       * Top-first, as the stack lists them, so "the first" means the one
       * the user can see.
       */
      [[nodiscard]] QVector<MapLayer *> layersOf(
        const QString &componentId) const;

      /*!
       * \brief The component a \a layer was built from, if any.
       *
       * Empty for a layer read from a file, and for a difference layer,
       * which is derived from two runs and so belongs to no one component.
       */
      [[nodiscard]] static QString componentOf(const MapLayer *layer);

      /*!
       * \brief Reports that a view picked a feature on \a layer.
       *
       * Called by the window from the one place both views' picks arrive,
       * so the hub needs to know about neither view.
       *
       * \param layer The layer picked in, or nullptr when the click missed.
       */
      void featurePicked(MapLayer *layer);

      /*!
       * \brief Reports that the run browser was asked to show \a layer.
       */
      void runItemShown(MapLayer *layer);

    Q_SIGNALS:
      /*!
       * \brief The layer tree should make \a layer current.
       *
       * A request rather than an order: the tree is a view of the stack
       * and decides for itself what "current" looks like.
       */
      void currentLayerRequested(HydroCouple::Composer::MapLayer *layer);

      /*!
       * \brief The canvas should select the component \a componentId.
       *
       * Empty means "nothing" — the pick landed on a layer with no
       * component behind it, and a stale highlight would be a lie.
       */
      void componentSelectionRequested(const QString &componentId);

    private:
      //! The canvas's selection changed: highlight that component's layers.
      void onSceneSelectionChanged();

      /*!
       * \brief Runs \a work with the guard raised, or not at all.
       *
       * Each direction of the exchange can trigger the other, so without
       * this the pair recurses until the stack overflows — which does not
       * fail a test, it kills the process (C4c's lesson).
       */
      template <typename Work>
      void forwardOnce(Work work)
      {
        if (m_forwarding)
        {
          return;
        }

        m_forwarding = true;
        work();
        m_forwarding = false;
      }

      CompositionScene *m_scene = nullptr;
      LayerStackModel *m_stack = nullptr;
      bool m_forwarding = false;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_SELECTIONHUB_H
