/*!
 * \file   testadapterfactory.cpp
 * \brief  Fixture adapted-output factory plugin.
 *
 * A loadable IAdaptedOutputFactoryComponent offering one adapter,
 * "double_it" (a linear transform with the multiplier baked to 2), over
 * rank-1 Double outputs. It exists so the registry, palette, and run
 * pipeline gates can exercise the factory kind without depending on the
 * HydroCoupleComponents repo.
 */

#include "hydrocouplesdk/data/abstractadaptedoutputfactory.h"
#include "hydrocouplesdk/data/abstractadaptedoutputfactorycomponentinfo.h"
#include "hydrocouplesdk/data/lineartransformadaptedoutput.h"
#include "hydrocouplesdk/core/identity.h"
#include "hydrocouplesdk/componentabi.h"

#include <memory>
#include <string>
#include <vector>

using namespace HydroCouple::SDK;

namespace
{

  class TestAdapterFactoryComponent final
    : public AbstractAdaptedOutputFactoryComponent
  {
    public:
      TestAdapterFactoryComponent(
        std::string_view id, AbstractAdaptedOutputFactoryComponentInfo *info)
        : AbstractAdaptedOutputFactoryComponent(id, info),
          m_offering(std::make_unique<Identity>("double_it",
                                                "Double every value"))
      {
      }

      std::vector<HydroCouple::IIdentity *> getAvailableAdaptedOutputIds(
        const HydroCouple::IOutput *provider,
        const HydroCouple::IInput * = nullptr) override
      {
        if (!provider ||
            provider->dataKind() != HydroCouple::DataKind::Float64 ||
            provider->shape().size() != 1)
        {
          return {};
        }

        return {m_offering.get()};
      }

      std::unique_ptr<HydroCouple::IAdaptedOutput> createAdaptedOutput(
        HydroCouple::IIdentity *adaptedProviderId,
        HydroCouple::IOutput *provider,
        HydroCouple::IInput * = nullptr) override
      {
        if (!adaptedProviderId || !provider ||
            adaptedProviderId->id() != "double_it")
        {
          return nullptr;
        }

        auto created = std::make_unique<LinearTransformAdaptedOutput>(
          "double_it#" + std::to_string(++m_created), provider, this);

        std::string message;
        for (HydroCouple::IArgument *argument : created->arguments())
        {
          if (argument && argument->id() == "multiplier")
          {
            (void)argument->initialize(
              R"({"values": [2.0]})",
              HydroCouple::IArgument::ArgumentInputType::JSON, message);
          }
        }

        provider->addAdaptedOutput(created.get());
        return created;
      }

    private:
      std::unique_ptr<Identity> m_offering;
      int m_created = 0;
  };

  class TestAdapterFactoryInfo final
    : public AbstractAdaptedOutputFactoryComponentInfo
  {
    public:
      TestAdapterFactoryInfo()
        : AbstractAdaptedOutputFactoryComponentInfo(
            "composer.test.adapterfactory")
      {
        setCaption("Loader Test Adapter Factory");
        setDescription(
          "Fixture adapted-output factory offering the double_it adapter.");
        setVersion("1.0.0");
      }

      std::unique_ptr<HydroCouple::IAdaptedOutputFactoryComponent>
      createComponentInstance() override
      {
        return std::make_unique<TestAdapterFactoryComponent>(
          "composer.test.adapterfactory.instance", this);
      }
  };

} // namespace

HYDROCOUPLE_DECLARE_COMPONENT(TestAdapterFactoryInfo)
