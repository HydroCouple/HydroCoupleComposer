/*!
 * \file   crscatalog.h
 * \author Caleb Buahin
 * \brief  The coordinate reference systems PROJ knows about, searchable.
 *
 * A CRS picker needs a list to pick from, and PROJ's database is the list —
 * some fourteen thousand entries, read once and kept. Reading it per keystroke
 * would make the search box unusable, so the catalogue is built on first use
 * and the search runs over what is already in memory.
 *
 * Only the fields a chooser shows are kept. The full definition arrives later,
 * from SpatialReference::fromAuthority, and only for the one entry chosen.
 */

#ifndef HYDROCOUPLECOMPOSER_GIS_CRSCATALOG_H
#define HYDROCOUPLECOMPOSER_GIS_CRSCATALOG_H

#include <QString>
#include <QVector>

namespace HydroCouple::Composer
{
  /*!
   * \brief What kind of coordinates a system holds.
   */
  enum class CrsKind
  {
    Any,          //!< No filtering.
    Geographic,   //!< Angular — longitude and latitude.
    Projected,    //!< Linear — metres, feet, and the like.
  };

  /*!
   * \brief One entry in the catalogue.
   */
  struct CrsEntry
  {
      QString authName;     //!< Authority, e.g. "EPSG".
      QString code;         //!< Code within that authority, e.g. "3857".
      QString name;         //!< e.g. "WGS 84 / Pseudo-Mercator".
      QString areaName;     //!< Where it applies, e.g. "World".
      CrsKind kind = CrsKind::Any;
      bool deprecated = false;

      //! \returns The authority and code as one token, e.g. "EPSG:3857".
      [[nodiscard]] QString authCode() const;
  };

  /*!
   * \brief Every non-deprecated system \a authority publishes.
   *
   * Built once per authority and cached, because the underlying query walks
   * the whole PROJ database.
   *
   * \param authority Authority to list, e.g. "EPSG".
   */
  [[nodiscard]] const QVector<CrsEntry> &crsCatalog(
    const QString &authority = QStringLiteral("EPSG"));

  /*!
   * \brief The entries matching every word of \a query.
   *
   * Every word, not the whole phrase: "utm 17n" should find "WGS 84 / UTM
   * zone 17N", which no substring of the query appears in. Codes are matched
   * too, so typing 3857 finds it directly.
   *
   * \param query Words to match; empty returns everything of \a kind.
   * \param kind Restrict to geographic or projected systems.
   * \param authority Authority to search.
   */
  [[nodiscard]] QVector<CrsEntry> searchCrsCatalog(
    const QString &query, CrsKind kind = CrsKind::Any,
    const QString &authority = QStringLiteral("EPSG"));

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_GIS_CRSCATALOG_H
