#include "layers/serviceconnections.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QSettings>
#include <QSysInfo>

#include <openssl/evp.h>

namespace HydroCouple::Composer
{

  namespace
  {
    //! Where in QSettings the connections live.
    constexpr const char *kGroup = "serviceConnections";

    constexpr int kSaltBytes = 16;
    constexpr int kIvBytes = 16;
    constexpr int kKeyBytes = 32; // AES-256.

    //! Deliberately slow: the stored bytes are offline-attackable.
    constexpr int kIterations = 100000;

    QByteArray randomBytes(int count)
    {
      QByteArray bytes(count, Qt::Uninitialized);
      QRandomGenerator::system()->generate(
        reinterpret_cast<quint32 *>(bytes.data()),
        reinterpret_cast<quint32 *>(bytes.data() + bytes.size()));

      return bytes;
    }

    QByteArray keyFrom(const QByteArray &machineId, const QByteArray &salt)
    {
      return QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256, machineId, salt, kIterations, kKeyBytes);
    }

    /*!
     * \brief AES-256-CBC, one direction or the other.
     * \returns The transformed bytes, or empty on any failure — including a
     *          wrong key, which CBC padding catches on decryption and which
     *          is exactly the copied-settings-file case.
     */
    QByteArray cipher(const QByteArray &input, const QByteArray &key,
                      const QByteArray &iv, bool encrypting)
    {
      EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();

      if (!context)
      {
        return {};
      }

      QByteArray output(input.size() + EVP_MAX_BLOCK_LENGTH,
                        Qt::Uninitialized);
      int written = 0;
      int total = 0;
      bool ok =
        EVP_CipherInit_ex(context, EVP_aes_256_cbc(), nullptr,
                          reinterpret_cast<const unsigned char *>(key.data()),
                          reinterpret_cast<const unsigned char *>(iv.data()),
                          encrypting ? 1 : 0)
        == 1;

      if (ok)
      {
        ok = EVP_CipherUpdate(
               context, reinterpret_cast<unsigned char *>(output.data()),
               &written,
               reinterpret_cast<const unsigned char *>(input.data()),
               static_cast<int>(input.size()))
             == 1;
        total = written;
      }

      if (ok)
      {
        ok = EVP_CipherFinal_ex(
               context,
               reinterpret_cast<unsigned char *>(output.data()) + total,
               &written)
             == 1;
        total += written;
      }

      EVP_CIPHER_CTX_free(context);

      if (!ok)
      {
        return {};
      }

      output.resize(total);

      return output;
    }
  } // namespace

  ServiceConnections::ServiceConnections(QSettings *settings,
                                         const QByteArray &machineId)
    : m_settings(settings),
      m_machineId(machineId.isEmpty() ? QSysInfo::machineUniqueId()
                                      : machineId)
  {
    if (m_machineId.isEmpty())
    {
      // A machine that will not say who it is still has to store
      // passwords; a constant key is weaker but honest about it, and the
      // alternative is refusing to remember anything.
      m_machineId = QByteArrayLiteral("hydrocouple-composer");
    }
  }

  QByteArray ServiceConnections::encrypt(const QString &secret) const
  {
    if (secret.isEmpty())
    {
      return {};
    }

    const QByteArray salt = randomBytes(kSaltBytes);
    const QByteArray iv = randomBytes(kIvBytes);
    const QByteArray body =
      cipher(secret.toUtf8(), keyFrom(m_machineId, salt), iv, true);

    if (body.isEmpty())
    {
      return {};
    }

    // Salt and IV travel with what they made: both are public by
    // construction, and neither can be recovered from the other.
    return salt + iv + body;
  }

  QString ServiceConnections::decrypt(const QByteArray &stored) const
  {
    if (stored.size() <= kSaltBytes + kIvBytes)
    {
      return {};
    }

    const QByteArray salt = stored.left(kSaltBytes);
    const QByteArray iv = stored.mid(kSaltBytes, kIvBytes);
    const QByteArray body = stored.mid(kSaltBytes + kIvBytes);
    const QByteArray plain =
      cipher(body, keyFrom(m_machineId, salt), iv, false);

    return plain.isEmpty() ? QString() : QString::fromUtf8(plain);
  }

  QStringList ServiceConnections::names() const
  {
    QSettings own;
    QSettings &settings = m_settings ? *m_settings : own;

    settings.beginGroup(QLatin1String(kGroup));
    QStringList names = settings.childGroups();
    settings.endGroup();

    names.sort(Qt::CaseInsensitive);

    return names;
  }

  ServiceConnection ServiceConnections::connection(const QString &name) const
  {
    QSettings own;
    QSettings &settings = m_settings ? *m_settings : own;

    ServiceConnection connection;

    settings.beginGroup(QLatin1String(kGroup));
    settings.beginGroup(name);

    const QString url = settings.value(QStringLiteral("url")).toString();

    if (!url.isEmpty())
    {
      connection.name = name;
      connection.url = url;
      connection.username =
        settings.value(QStringLiteral("username")).toString();
      connection.password = decrypt(
        settings.value(QStringLiteral("password")).toByteArray());
    }

    settings.endGroup();
    settings.endGroup();

    return connection;
  }

  bool ServiceConnections::save(const ServiceConnection &connection)
  {
    if (!connection.isValid())
    {
      return false;
    }

    QSettings own;
    QSettings &settings = m_settings ? *m_settings : own;

    settings.beginGroup(QLatin1String(kGroup));
    settings.beginGroup(connection.name);
    settings.setValue(QStringLiteral("url"), connection.url);
    settings.setValue(QStringLiteral("username"), connection.username);
    settings.setValue(QStringLiteral("password"),
                      encrypt(connection.password));
    settings.endGroup();
    settings.endGroup();
    settings.sync();

    return true;
  }

  bool ServiceConnections::remove(const QString &name)
  {
    if (!names().contains(name))
    {
      return false;
    }

    QSettings own;
    QSettings &settings = m_settings ? *m_settings : own;

    settings.beginGroup(QLatin1String(kGroup));
    settings.remove(name);
    settings.endGroup();
    settings.sync();

    return true;
  }

} // namespace HydroCouple::Composer
