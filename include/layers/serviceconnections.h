/*!
 * \file   serviceconnections.h
 * \author Caleb Buahin
 * \brief  ServiceConnections — web services remembered by name.
 *
 * A service address is long, is typed once, and is then wanted again in
 * every project that draws on it. This remembers them under names the user
 * gives, with whatever credentials the service needed.
 *
 * \par Why the password is bound to the machine
 * A saved password has to be readable by this program without asking the
 * user for anything, which means the key must be derivable from the machine
 * itself. So it is: PBKDF2 over the machine's unique id, with a per-entry
 * salt, and AES-256-CBC under a per-entry random IV. That protects against
 * the realistic threat — a settings file copied, synced, or backed up
 * somewhere else, where it decrypts to nothing — and deliberately not
 * against someone already running as this user on this machine, which no
 * scheme without a master password can.
 *
 * \par Deliberate divergence
 * openswmm.gui keys its saved connections by service kind, because its
 * dialog makes the user say which kind they are adding. This dialog asks
 * the server instead (see OgcServiceDialog), so a connection here is one
 * address whose kind is discovered, and they live in one flat store.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_SERVICECONNECTIONS_H
#define HYDROCOUPLECOMPOSER_LAYERS_SERVICECONNECTIONS_H

#include <QByteArray>
#include <QString>
#include <QStringList>

class QSettings;

namespace HydroCouple::Composer
{

  /*!
   * \brief One remembered service.
   */
  struct ServiceConnection
  {
      QString name;      //!< What the user calls it.
      QString url;       //!< The address, or an XYZ template.
      QString username;  //!< Empty when the service is open.
      QString password;  //!< In the clear here; never on disk.

      [[nodiscard]] bool isValid() const
      {
        return !name.isEmpty() && !url.isEmpty();
      }
  };

  /*!
   * \brief The saved connections, read and written through QSettings.
   */
  class ServiceConnections
  {
    public:
      /*!
       * \brief Opens the store.
       *
       * \param settings Where to keep them; the application's own settings
       *        when null. Not owned.
       * \param machineId What the password key is derived from. Defaults to
       *        this machine's id; a test passes its own, which is also how
       *        the machine-binding is observable at all.
       */
      explicit ServiceConnections(QSettings *settings = nullptr,
                                  const QByteArray &machineId = QByteArray());

      //! The names of what is saved, sorted.
      [[nodiscard]] QStringList names() const;

      /*!
       * \brief One saved connection, with its password decrypted.
       *
       * A password that cannot be decrypted — a settings file from another
       * machine — comes back empty rather than as rubbish, and the rest of
       * the connection still comes back: the address is the useful half and
       * was never a secret.
       *
       * \param name Which one.
       * \returns The connection; invalid when there is no such name.
       */
      [[nodiscard]] ServiceConnection connection(const QString &name) const;

      /*!
       * \brief Saves \a connection under its own name, replacing any.
       * \returns false when the connection has no name or no address.
       */
      bool save(const ServiceConnection &connection);

      //! Forgets one; false when there was no such name.
      bool remove(const QString &name);

    private:
      [[nodiscard]] QByteArray encrypt(const QString &secret) const;

      [[nodiscard]] QString decrypt(const QByteArray &stored) const;

      QSettings *m_settings = nullptr;
      QByteArray m_machineId;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_SERVICECONNECTIONS_H
