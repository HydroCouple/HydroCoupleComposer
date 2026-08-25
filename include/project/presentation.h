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
 */

#ifndef HYDROCOUPLECOMPOSER_PROJECT_PRESENTATION_H
#define HYDROCOUPLECOMPOSER_PROJECT_PRESENTATION_H

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
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_PROJECT_PRESENTATION_H
