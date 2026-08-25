/*!
 * \file   legacycomponent.cpp
 * \brief  A component exporting only the legacy `CreateComponentInfo` factory
 *         that HydroCouple's Python bindings load.
 *
 * Guards the compatibility path: a component written for the Python loader
 * must remain loadable by Composer, or the ecosystem splits in two.
 */

#include "hydrocouplesdk/component/abstractmodelcomponent.h"
#include "hydrocouplesdk/component/abstractmodelcomponentinfo.h"
#include "hydrocouplesdk/core/dimension.h"
#include "hydrocouplesdk/core/valuedefinition.h"

#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#  define LEGACY_EXPORT __declspec(dllexport)
#else
#  define LEGACY_EXPORT __attribute__((visibility("default")))
#endif

using namespace HydroCouple;
using namespace HydroCouple::SDK;

namespace
{
  class LegacyComponent : public AbstractModelComponent
  {
    public:
      explicit LegacyComponent(std::string_view id)
        : AbstractModelComponent(id, "Legacy Component")
      {
      }

      std::vector<std::string> validate() override
      {
        setStatus(ComponentStatus::Validating);
        setStatus(ComponentStatus::Valid);
        return {};
      }

      void prepare() override
      {
        setStatus(ComponentStatus::Preparing);
        setStatus(ComponentStatus::Updated, "prepared");
      }

      void update(const std::vector<IOutput *> & = {}) override
      {
        setStatus(ComponentStatus::Done, "stepped");
      }

      void finish() override
      {
        setStatus(ComponentStatus::Finishing);
        setStatus(ComponentStatus::Finished);
      }

    protected:
      void createArguments() override {}

      bool initializeArguments(std::string &message) override
      {
        message.clear();
        return true;
      }

      void createInputs() override {}
      void createOutputs() override {}
      void initializeFailureCleanUp() override {}
  };

  class LegacyComponentInfo : public AbstractModelComponentInfo
  {
    public:
      LegacyComponentInfo()
        : AbstractModelComponentInfo("composer.test.legacy")
      {
        setCaption("Legacy Component");
        setDescription("Exports only the pre-stamp CreateComponentInfo factory.");
        setVersion("0.9.0");
      }

      std::unique_ptr<IModelComponent> createComponentInstance() override
      {
        return std::make_unique<LegacyComponent>("composer.test.legacy.instance");
      }
  };
} // namespace

// The convention HydroCouple's Python loader expects by default.
extern "C" LEGACY_EXPORT HydroCouple::IModelComponentInfo *CreateComponentInfo(void)
{
  static LegacyComponentInfo s_info;
  return &s_info;
}
