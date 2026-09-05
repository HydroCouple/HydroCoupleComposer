/*!
 * \file   componentinstances.h
 * \author Caleb Buahin
 * \brief  ComponentInstances — the live components behind a composition.
 *
 * A composition document names components and connects their exchange items,
 * but it does not list what those items *are*: the set of inputs and outputs
 * belongs to the component, not to the document. Anything that needs that set
 * — the canvas drawing ports, the configurator listing arguments, the
 * simulation manager running the thing — needs real instances.
 *
 * This class realises them from a ComponentRegistry and keeps them keyed by
 * the document's component ids. Instances are created lazily and reported
 * per-component when they cannot be created, because a document routinely
 * references a component whose library is not installed on this machine, and
 * that must degrade to "this box cannot be drawn in full" rather than to a
 * failed open.
 *
 * \note Instances must not outlive the registry that loaded their libraries;
 *       `clear()` happens before the registry goes away.
 */

#ifndef HYDROCOUPLECOMPOSER_PROJECT_COMPONENTINSTANCES_H
#define HYDROCOUPLECOMPOSER_PROJECT_COMPONENTINSTANCES_H

#include "plugins/componentregistry.h"
#include "project/compositiondocument.h"

#include <QHash>
#include <QObject>
#include <QString>

#include <memory>

namespace HydroCouple::Composer
{

  /*!
   * \brief One exchange item, described for the UI.
   */
  struct ExchangeItemDescriptor
  {
      QString id;
      QString caption;
      bool isMultiInput = false;
  };

  /*!
   * \brief Live IModelComponent instances for a CompositionDocument.
   */
  class ComponentInstances : public QObject
  {
      Q_OBJECT

    public:
      /*!
       * \brief Binds instances to \a document, realised from \a registry.
       * \param document Composition whose components are realised.
       * \param registry Source of component libraries.
       * \param parent Optional Qt parent.
       */
      ComponentInstances(CompositionDocument *document,
                         ComponentRegistry *registry,
                         QObject *parent = nullptr);

      ~ComponentInstances() override;

      /*!
       * \brief The instance for \a componentId, creating it on first request.
       * \returns nullptr when the component cannot be instantiated; call
       *          failure() for why.
       */
      HydroCouple::IModelComponent *instance(const QString &componentId);

      /*!
       * \brief Why \a componentId could not be instantiated, or an empty string.
       */
      [[nodiscard]] QString failure(const QString &componentId) const;

      /*!
       * \brief The component's inputs, or an empty list when unavailable.
       * \param componentId Component to describe.
       */
      [[nodiscard]] QList<ExchangeItemDescriptor> inputs(
        const QString &componentId);

      /*!
       * \brief The component's outputs, or an empty list when unavailable.
       * \param componentId Component to describe.
       */
      [[nodiscard]] QList<ExchangeItemDescriptor> outputs(
        const QString &componentId);

      /*!
       * \brief Drops every instance; safe to call repeatedly.
       */
      void clear();

      /*!
       * \brief The registry these instances are realised from.
       */
      [[nodiscard]] ComponentRegistry *registry() const { return m_registry; }

      /*!
       * \brief Live standalone adapter factories for edit-time availability
       *        queries: the SDK's own first, then one instance per loaded
       *        factory library.
       *
       * Lazily realised and owned here, introspection only — a run realises
       * its own set. Invalidated when the registry changes; dropped by
       * clear(), before the registry goes away.
       */
      [[nodiscard]] std::vector<HydroCouple::IAdaptedOutputFactory *>
      adapterFactories();

    Q_SIGNALS:
      /*!
       * \brief Emitted when a component's instance appears or is dropped.
       * \param componentId The affected component.
       */
      void instanceChanged(const QString &componentId);

    private:
      //! Which component-info id in the registry backs a document component.
      [[nodiscard]] QString registryIdFor(const QString &componentId) const;

      CompositionDocument *m_document = nullptr;
      ComponentRegistry *m_registry = nullptr;

      QHash<QString, std::shared_ptr<HydroCouple::IModelComponent>> m_instances;
      QHash<QString, QString> m_failures;

      std::vector<std::unique_ptr<HydroCouple::IAdaptedOutputFactoryComponent>>
        m_adapterFactories;
      bool m_adapterFactoriesRealised = false;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_PROJECT_COMPONENTINSTANCES_H
