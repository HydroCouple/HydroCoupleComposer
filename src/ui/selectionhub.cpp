#include "ui/selectionhub.h"

#include "canvas/canvasitems.h"
#include "canvas/compositionscene.h"
#include "layers/componentlayer.h"
#include "map/layerstackmodel.h"
#include "map/maplayer.h"

namespace HydroCouple::Composer
{

  SelectionHub::SelectionHub(CompositionScene *scene, LayerStackModel *stack,
                             QObject *parent)
    : QObject(parent), m_scene(scene), m_stack(stack)
  {
    if (m_scene)
    {
      connect(m_scene, &QGraphicsScene::selectionChanged, this,
              &SelectionHub::onSceneSelectionChanged);
    }
  }

  QString SelectionHub::componentOf(const MapLayer *layer)
  {
    // Asked of the interface, not of a list of layer classes: a second
    // kind of component-backed layer must not mean a second branch here.
    const auto *provenance = dynamic_cast<const IComponentLayer *>(layer);

    return provenance ? provenance->componentId() : QString();
  }

  QVector<MapLayer *> SelectionHub::layersOf(const QString &componentId) const
  {
    QVector<MapLayer *> found;

    if (!m_stack || componentId.isEmpty())
    {
      // An empty id would otherwise match every layer that never recorded
      // one, which is every file layer on the map.
      return found;
    }

    for (MapLayer *layer : m_stack->layers())
    {
      if (componentOf(layer) == componentId)
      {
        found.append(layer);
      }
    }

    return found;
  }

  void SelectionHub::onSceneSelectionChanged()
  {
    forwardOnce(
      [this]
      {
        QString componentId;

        for (QGraphicsItem *item : m_scene->selectedItems())
        {
          if (auto *node = qgraphicsitem_cast<ComponentNodeItem *>(item))
          {
            componentId = node->componentId();
            break;
          }
        }

        const QVector<MapLayer *> layers = layersOf(componentId);

        // Selecting a component does not change which *features* are
        // selected: a component is not a feature, and clearing the map's
        // selection because the user clicked a box on the canvas would
        // throw away work they did with the pointer.
        if (!layers.isEmpty())
        {
          Q_EMIT currentLayerRequested(layers.first());
        }
      });
  }

  void SelectionHub::featurePicked(MapLayer *layer)
  {
    forwardOnce(
      [this, layer]
      {
        // Announced even when empty. A pick that landed on a file layer
        // has no component behind it, and leaving the canvas showing the
        // last one that did would be a highlight that means nothing.
        Q_EMIT componentSelectionRequested(componentOf(layer));
      });
  }

  void SelectionHub::runItemShown(MapLayer *layer)
  {
    forwardOnce(
      [this, layer]
      {
        if (layer)
        {
          Q_EMIT currentLayerRequested(layer);
        }

        Q_EMIT componentSelectionRequested(componentOf(layer));
      });
  }

} // namespace HydroCouple::Composer
