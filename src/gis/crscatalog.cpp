#include "gis/crscatalog.h"

#include <QHash>

#include <ogr_srs_api.h>

namespace HydroCouple::Composer
{
  namespace
  {
    CrsKind kindOf(OSRCRSType type)
    {
      switch (type)
      {
        case OSR_CRS_TYPE_GEOGRAPHIC_2D:
        case OSR_CRS_TYPE_GEOGRAPHIC_3D:
          return CrsKind::Geographic;

        case OSR_CRS_TYPE_PROJECTED:
          return CrsKind::Projected;

        default:
          // Compound, geocentric, vertical and engineering systems are real
          // but are not what a map is drawn in, and offering them in a map
          // CRS picker would be offering a choice that cannot work.
          return CrsKind::Any;
      }
    }

    QVector<CrsEntry> buildCatalog(const QString &authority)
    {
      QVector<CrsEntry> entries;

      int count = 0;
      OSRCRSInfo **list = OSRGetCRSInfoListFromDatabase(
        authority.toUtf8().constData(), nullptr, &count);

      if (!list)
      {
        return entries;
      }

      entries.reserve(count);

      for (int i = 0; i < count; ++i)
      {
        const OSRCRSInfo *info = list[i];

        if (!info)
        {
          continue;
        }

        const CrsKind kind = kindOf(info->eType);

        // Deprecated systems still resolve, but a picker that offers them
        // hands the user a superseded definition without saying so.
        if (kind == CrsKind::Any || info->bDeprecated)
        {
          continue;
        }

        CrsEntry entry;
        entry.authName =
          QString::fromUtf8(info->pszAuthName ? info->pszAuthName : "");
        entry.code = QString::fromUtf8(info->pszCode ? info->pszCode : "");
        entry.name = QString::fromUtf8(info->pszName ? info->pszName : "");
        entry.areaName =
          QString::fromUtf8(info->pszAreaName ? info->pszAreaName : "");
        entry.kind = kind;
        entry.deprecated = info->bDeprecated != 0;

        entries.append(entry);
      }

      OSRDestroyCRSInfoList(list);

      return entries;
    }
  }

  QString CrsEntry::authCode() const
  {
    return QStringLiteral("%1:%2").arg(authName, code);
  }

  const QVector<CrsEntry> &crsCatalog(const QString &authority)
  {
    static QHash<QString, QVector<CrsEntry>> cache;

    auto found = cache.constFind(authority);

    if (found == cache.constEnd())
    {
      found = cache.insert(authority, buildCatalog(authority));
    }

    return found.value();
  }

  QVector<CrsEntry> searchCrsCatalog(const QString &query, CrsKind kind,
                                     const QString &authority)
  {
    const QVector<CrsEntry> &all = crsCatalog(authority);

    const QStringList words =
      query.split(QLatin1Char(' '), Qt::SkipEmptyParts);

    QVector<CrsEntry> matches;

    for (const CrsEntry &entry : all)
    {
      if (kind != CrsKind::Any && entry.kind != kind)
      {
        continue;
      }

      // The code is searched alongside the names, so "3857" finds the system
      // itself rather than every system whose name happens to say 3857.
      const QString haystack = QStringLiteral("%1 %2 %3")
                                 .arg(entry.name, entry.areaName,
                                      entry.authCode());

      bool matched = true;

      for (const QString &word : words)
      {
        if (!haystack.contains(word, Qt::CaseInsensitive))
        {
          matched = false;
          break;
        }
      }

      if (matched)
      {
        matches.append(entry);
      }
    }

    return matches;
  }

} // namespace HydroCouple::Composer
