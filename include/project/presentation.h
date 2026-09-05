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
#include <QList>
#include <QPointF>
#include <QJsonArray>
#include <QString>

namespace HydroCouple::SDK::IO
{
  struct ConnectionSpec;
}

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

      // ── Adapter-node positions ───────────────────────────────────────────
      // Where a connection's spliced adapter nodes sit, one point per chain
      // step. Addressed by the connection's full identity (endpoints +
      // role) rather than a composed string key: component ids may contain
      // any character, so a "from.output->to.input" string would be
      // ambiguous to parse back. A missing or short list is never an error
      // — unplaced steps take default positions along the edge.

      /*!
       * \brief Saved positions for a connection's chain; empty when none.
       */
      [[nodiscard]] QList<QPointF> adapterChain(
        const HydroCouple::SDK::IO::ConnectionSpec &connection) const;

      /*!
       * \brief Replaces a chain's saved positions; an empty list removes
       *        the entry.
       */
      void setAdapterChain(
        const HydroCouple::SDK::IO::ConnectionSpec &connection,
        const QList<QPointF> &positions);

      /*!
       * \brief Sets one step's position, growing the list as needed.
       */
      void setAdapterPosition(
        const HydroCouple::SDK::IO::ConnectionSpec &connection, int index,
        const QPointF &position);

      void removeAdapterChain(
        const HydroCouple::SDK::IO::ConnectionSpec &connection);

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

      /*!
       * \brief The map layers to rebuild when this composition is reopened.
       *
       * Kept as opaque JSON on purpose. What a layer needs in order to be
       * made again is the layer's own business -- a file path for one, a
       * service address and a coverage identifier for another -- and a
       * sidecar that knew the difference would have to be edited every time
       * a new kind of layer was added. It stores what it is given and hands
       * it back.
       *
       * Credentials are not among it. A project file is checked in, mailed
       * and copied between machines; a password in one is a password
       * published. They stay in the connection registry, encrypted, and a
       * reopened layer that needs one asks again.
       */
      [[nodiscard]] const QJsonArray &layers() const;

      //! Replaces the layers; an empty array clears them.
      void setLayers(const QJsonArray &layers);

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
      /*!
       * \brief One connection's adapter-node positions, with the identity
       *        fields spelled out so rename and removal never parse keys.
       */
      struct AdapterChainEntry
      {
          QString fromComponent;
          QString output;
          QString toComponent;
          QString input;
          QString role;
          QList<QPointF> positions;
      };

      //! Internal lookup key (unit-separated; never serialised).
      [[nodiscard]] static QString chainKey(const QString &fromComponent,
                                            const QString &output,
                                            const QString &toComponent,
                                            const QString &input,
                                            const QString &role);

      QHash<QString, ComponentPresentation> m_components;
      QHash<QString, AdapterChainEntry> m_adapterChains;
      MeshDomain m_meshDomain;
      QJsonArray m_layers;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_PROJECT_PRESENTATION_H
