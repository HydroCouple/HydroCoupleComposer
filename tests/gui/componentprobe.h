/*!
 * \file   componentprobe.h
 * \brief  A layer that claims a component, without needing a component.
 *
 * The selection hub's question is "which component produced this layer",
 * and the answer comes from IComponentLayer. Building a real DataItemLayer
 * to ask it would mean standing up a component, its instance and a spatial
 * data item — a great deal of machinery to exercise one string, and a
 * fixture whose failures would mostly be about the machinery.
 *
 * This is a VectorProbe that also implements IComponentLayer, so a test can
 * put a known component id on a known layer and check what the hub does
 * with it. The hub asks through the interface, so a probe that implements
 * the interface is exactly as good a subject as the real thing — and the
 * suite also drives a real DataItemLayer once, in test_dataitemlayers,
 * to hold the real implementation down.
 */

#ifndef HYDROCOUPLECOMPOSER_TESTS_COMPONENTPROBE_H
#define HYDROCOUPLECOMPOSER_TESTS_COMPONENTPROBE_H

#include "layers/componentlayer.h"
#include "vectorprobe.h"

namespace HydroCouple::Composer::Testing
{
  /*!
   * \brief A vector layer that reports a component it was built from.
   */
  class ComponentProbe : public VectorProbe, public IComponentLayer
  {
    public:
      ComponentProbe(const QString &name, const QString &componentId,
                     const QString &dataItemId = QStringLiteral("values"))
        : VectorProbe(name)
      {
        setProvenance(componentId, dataItemId);
      }
  };

} // namespace HydroCouple::Composer::Testing

#endif // HYDROCOUPLECOMPOSER_TESTS_COMPONENTPROBE_H
