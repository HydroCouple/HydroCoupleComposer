#include "project/componentinstances.h"

#include "hydrocouplesdk/data/sdkadaptedoutputfactory.h"

#include <QStringList>

#include <exception>

namespace HydroCouple::Composer
{

  namespace
  {
    ExchangeItemDescriptor describe(HydroCouple::IExchangeItem *item,
                                    bool isMultiInput)
    {
      ExchangeItemDescriptor descriptor;
      descriptor.id = QString::fromStdString(item->id());
      descriptor.caption = QString::fromStdString(item->caption());
      descriptor.isMultiInput = isMultiInput;

      if (descriptor.caption.isEmpty())
      {
        descriptor.caption = descriptor.id;
      }

      return descriptor;
    }
  } // namespace

  ComponentInstances::ComponentInstances(CompositionDocument *document,
                                         ComponentRegistry *registry,
                                         QObject *parent)
    : QObject(parent),
      m_document(document),
      m_registry(registry)
  {
    // A component removed from the document must not keep an instance alive,
    // and one re-added must be realised afresh.
    if (m_document)
    {
      connect(m_document, &CompositionDocument::componentsChanged, this,
              [this]
              {
                const QStringList live = m_document->componentIds();

                for (const QString &id : m_instances.keys())
                {
                  if (!live.contains(id))
                  {
                    m_instances.remove(id);
                    m_failures.remove(id);
                    Q_EMIT instanceChanged(id);
                  }
                }
              });
    }

    // Edit-time adapter factories go stale when libraries come and go.
    if (m_registry)
    {
      connect(m_registry, &ComponentRegistry::registryChanged, this,
              [this]
              {
                m_adapterFactories.clear();
                m_adapterFactoriesRealised = false;
              });
    }
  }

  ComponentInstances::~ComponentInstances()
  {
    clear();
  }

  std::vector<HydroCouple::IAdaptedOutputFactory *>
  ComponentInstances::adapterFactories()
  {
    if (!m_adapterFactoriesRealised && m_registry)
    {
      m_adapterFactoriesRealised = true;

      for (HydroCouple::IComponentInfo *info : m_registry->entries(
             ComponentRegistry::ComponentKind::AdapterFactory))
      {
        QString failure;
        std::unique_ptr<HydroCouple::IAdaptedOutputFactoryComponent> factory =
          m_registry->createAdaptedOutputFactory(
            QString::fromStdString(info->id()), failure);

        if (factory)
        {
          m_adapterFactories.push_back(std::move(factory));
        }
      }
    }

    std::vector<HydroCouple::IAdaptedOutputFactory *> factories;
    factories.push_back(HydroCouple::SDK::SdkAdaptedOutputFactory::instance());

    for (const std::unique_ptr<HydroCouple::IAdaptedOutputFactoryComponent>
           &factory : m_adapterFactories)
    {
      factories.push_back(factory.get());
    }

    return factories;
  }

  QString ComponentInstances::registryIdFor(const QString &componentId) const
  {
    if (!m_document)
    {
      return QString();
    }

    const std::optional<CompositionDocument::ComponentSpec> spec =
      m_document->component(componentId);

    if (!spec)
    {
      return QString();
    }

    // The document may name the component-info id explicitly; when it does
    // not, the instance id is the best available guess.
    const QString infoId = QString::fromStdString(spec->info.componentInfoId);

    return infoId.isEmpty() ? componentId : infoId;
  }

  HydroCouple::IModelComponent *ComponentInstances::instance(
    const QString &componentId)
  {
    if (const auto it = m_instances.constFind(componentId);
        it != m_instances.constEnd())
    {
      return it.value().get();
    }

    if (!m_registry || !m_document)
    {
      return nullptr;
    }

    const QString registryId = registryIdFor(componentId);

    if (registryId.isEmpty())
    {
      m_failures.insert(componentId,
                        QStringLiteral("'%1' is not in this composition")
                          .arg(componentId));
      return nullptr;
    }

    QString message;
    std::unique_ptr<HydroCouple::IModelComponent> created =
      m_registry->createInstance(registryId, message);

    if (!created)
    {
      m_failures.insert(componentId, message);
      return nullptr;
    }

    // A freshly created component has no exchange items yet: the interface is
    // explicit that Inputs and Outputs are only set once initialize() has
    // completed, and that arguments() is the sole property valid before then.
    // The canvas cannot draw a single port without this step.
    HydroCouple::IModelComponent *candidate = created.get();

    try
    {
      candidate->initialize();
    }
    catch (const std::exception &error)
    {
      m_failures.insert(componentId,
                        QStringLiteral("initialize() threw: %1")
                          .arg(QString::fromUtf8(error.what())));
      return nullptr;
    }

    if (candidate->status() !=
        HydroCouple::IModelComponent::ComponentStatus::Initialized)
    {
      QStringList diagnostics;

      for (const HydroCouple::ErrorEntry &error : candidate->errors())
      {
        diagnostics.append(QString::fromStdString(error.message));
      }

      m_failures.insert(
        componentId,
        diagnostics.isEmpty()
          ? QStringLiteral("'%1' did not reach Initialized").arg(componentId)
          : diagnostics.join(QStringLiteral("; ")));

      return nullptr;
    }

    m_failures.remove(componentId);

    HydroCouple::IModelComponent *raw = created.get();
    m_instances.insert(componentId, std::shared_ptr<HydroCouple::IModelComponent>(
                                      std::move(created)));

    Q_EMIT instanceChanged(componentId);

    return raw;
  }

  QString ComponentInstances::failure(const QString &componentId) const
  {
    return m_failures.value(componentId);
  }

  QList<ExchangeItemDescriptor> ComponentInstances::inputs(
    const QString &componentId)
  {
    QList<ExchangeItemDescriptor> descriptors;

    HydroCouple::IModelComponent *component = instance(componentId);

    if (!component)
    {
      return descriptors;
    }

    for (HydroCouple::IInput *input : component->inputs())
    {
      if (!input)
      {
        continue;
      }

      descriptors.append(
        describe(input, dynamic_cast<HydroCouple::IMultiInput *>(input) != nullptr));
    }

    return descriptors;
  }

  QList<ExchangeItemDescriptor> ComponentInstances::outputs(
    const QString &componentId)
  {
    QList<ExchangeItemDescriptor> descriptors;

    HydroCouple::IModelComponent *component = instance(componentId);

    if (!component)
    {
      return descriptors;
    }

    for (HydroCouple::IOutput *output : component->outputs())
    {
      if (output)
      {
        descriptors.append(describe(output, false));
      }
    }

    return descriptors;
  }

  void ComponentInstances::clear()
  {
    const QStringList ids = m_instances.keys();

    m_instances.clear();
    m_failures.clear();
    m_adapterFactories.clear();
    m_adapterFactoriesRealised = false;

    for (const QString &id : ids)
    {
      Q_EMIT instanceChanged(id);
    }
  }

} // namespace HydroCouple::Composer
