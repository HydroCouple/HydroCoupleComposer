/*!
 * \file   composerapplication.h
 * \author Caleb Buahin
 * \brief  ComposerApplication — the QApplication subclass owning process-wide
 *         setup (identity, settings scope, style).
 *
 * Kept separate from main() so the offscreen test harness can construct the
 * very same application object the shipped binary uses.
 */

#ifndef HYDROCOUPLECOMPOSER_CORE_COMPOSERAPPLICATION_H
#define HYDROCOUPLECOMPOSER_CORE_COMPOSERAPPLICATION_H

#include <QApplication>

namespace HydroCouple::Composer
{

  /*!
   * \brief The application object for HydroCoupleComposer.
   */
  class ComposerApplication : public QApplication
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs the application and applies organization/application
       *        identity so QSettings resolves consistently everywhere.
       */
      ComposerApplication(int &argc, char **argv);

      ~ComposerApplication() override;

      /*!
       * \brief The version string this binary was built from.
       */
      [[nodiscard]] static QString versionString();
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_CORE_COMPOSERAPPLICATION_H
