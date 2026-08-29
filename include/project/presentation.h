/*!
 * \file   presentation.h
 * \author Caleb Buahin
 * \brief  Presentation — the Composer-owned view state for a composition.
 *
 * Canvas positions, collapsed state, and view settings live *beside* the
 * composition document, never inside it. That is not merely tidiness: the
 * Composition Specification v1 schema declares `metadata` with
 * `additionalProperties: false`, so a conforming document has nowhere to put
 * GUI state at all. Keeping it in a sidecar also leaves the composition
 * host-neutral and diff-clean for the CLI, Python, and cloud runners that
 * consume the same file.
 *
 * The sidecar sits next to its document as `<name>.composer.json`. A missing
 * or unreadable sidecar is never an error — the composition remains fully
 * usable and the canvas simply lays out afresh.
 *
 * The mesh domain lives here for the same reason the canvas positions do: it
 * is Composer-owned authoring state with nowhere to go in a conforming
 * composition document. A composition names the mesh *file* a component
 * reads; the domain is the ground that file was generated over, and losing
 * it would mean a mesh nobody can regenerate or correct.
 */

#ifndef HYDROCOUPLECOMPOSER_PROJECT_PRESENTATION_H
#define HYDROCOUPLECOMPOSER_PROJECT_PRESENTATION_H

#include "mesh/meshdomain.h"

#include <QHash>
#include <QPointF>
#include <QString>

namespace HydroCouple::Composer
{

  /*!
   * \brief Where one component sits on the canvas.
   */
  struct ComponentPresentation
  {
      QPointF position;
  };

  /*!
   * \brief View state for a whole composition.
   */
  class Presentation
  {
    public:
      /*!
       * \brief The sidecar path for a composition document path.
       *
       * `flow.json` -> `flow.composer.json`; a document with no suffix simply
       * gains one.
       */
      [[nodiscard]] static QString sidecarPathFor(const QString &documentPath);

      [[nodiscard]] bool hasComponent(const QString &componentId) const;

      [[nodiscard]] ComponentPresentation component(
        const QString &componentId) const;

      void setComponent(const QString &componentId,
                        const ComponentPresentation &presentation);

      void removeComponent(const QString &componentId);

      /*!
       * \brief Renames a component's entry, keeping its placement.
       */
      void renameComponent(const QString &fromId, const QString &toId);

      [[nodiscard]] QStringList componentIds() const;

      /*!
       * \brief Whether a mesh domain has been drawn for this composition.
       *
       * Asked separately from reading it, because an empty domain and no
       * domain are the same picture on the map and different answers to
       * "has anything been drawn yet".
       */
      [[nodiscard]] bool hasMeshDomain() const;

      //! \returns The mesh domain; empty when none was drawn.
      [[nodiscard]] const MeshDomain &meshDomain() const;

      /*!
       * \brief Replaces the mesh domain.
       * \param domain The domain to keep; an empty one clears it.
       */
      void setMeshDomain(const MeshDomain &domain);

      [[nodiscard]] bool isEmpty() const;

      void clear();

      /*!
       * \brief Serialises to the sidecar's JSON text.
       */
      [[nodiscard]] QByteArray toJson() const;

      /*!
       * \brief Parses sidecar JSON, replacing current contents.
       * \returns false when the text is not valid sidecar JSON; the
       *          presentation is then left empty rather than half-applied.
       */
      bool fromJson(const QByteArray &json);

    private:
      QHash<QString, ComponentPresentation> m_components;
      MeshDomain m_meshDomain;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_PROJECT_PRESENTATION_H
