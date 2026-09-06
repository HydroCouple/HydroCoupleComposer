/*!
 * \file   adapterinspector.h
 * \author Caleb Buahin
 * \brief  AdapterInspector — one chain step, or one connection, inspected.
 *
 * The panel holds an ADDRESS — a connection identity plus a chain index —
 * never an item pointer: canvas rebuilds destroy every item, and the
 * document is the only thing that survives them. Every edit goes back
 * through CompositionDocument::setConnectionAdapterArgument, so it lands on
 * the undo stack and every other view sees it.
 */

#ifndef HYDROCOUPLECOMPOSER_CONFIGURATOR_ADAPTERINSPECTOR_H
#define HYDROCOUPLECOMPOSER_CONFIGURATOR_ADAPTERINSPECTOR_H

#include "project/compositiondocument.h"

#include <QWidget>

class QLabel;
class QVBoxLayout;

namespace HydroCouple::Composer
{

  /*!
   * \brief Inspects one adapter chain step (editable) or one connection
   *        (read-only summary).
   */
  class AdapterInspector : public QWidget
  {
      Q_OBJECT

    public:
      explicit AdapterInspector(CompositionDocument *document,
                                QWidget *parent = nullptr);

      /*!
       * \brief Shows one chain step for editing.
       * \param connection The connection's identity (endpoints + role).
       * \param stepIndex The step within its adaptation chain.
       */
      void setChainStep(const HydroCouple::SDK::IO::ConnectionSpec &connection,
                        int stepIndex);

      /*!
       * \brief Shows a whole connection read-only: endpoints, role, chain.
       */
      void showConnection(
        const HydroCouple::SDK::IO::ConnectionSpec &connection);

      /*!
       * \brief Empties the panel.
       */
      void clearSelection();

    private:
      //! Re-reads the document at the held address; clears when it is gone.
      void refresh();

      [[nodiscard]] const HydroCouple::SDK::IO::ConnectionSpec *
      findConnection() const;

      CompositionDocument *m_document = nullptr;

      bool m_hasAddress = false;
      bool m_stepMode = false;
      HydroCouple::SDK::IO::ConnectionSpec m_identity;
      int m_stepIndex = 0;

      QLabel *m_title = nullptr;
      QWidget *m_rows = nullptr;
      QVBoxLayout *m_layout = nullptr;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_CONFIGURATOR_ADAPTERINSPECTOR_H
