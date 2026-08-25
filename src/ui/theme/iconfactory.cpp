#include "ui/theme/iconfactory.h"

#include "ui/theme/thememanager.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPixmapCache>
#include <QSvgRenderer>

namespace HydroCouple::Composer
{
  namespace
  {
    /*!
     * \brief The mid-gray family baked into the shared SVG chrome set.
     *
     * Only these are substituted. Any other colour in an icon was chosen
     * deliberately — a status red, a water blue — and recolouring it would
     * throw away the one thing it was there to say.
     */
    const char *const kGlyphGrays[] = { "#777777", "#626262", "#989898",
                                        "#A4A0A0", "#9E9E9E" };

    //! The set is addressed by base name; the resource keeps the suffix.
    QString resourcePath(const QString &alias)
    {
      return QStringLiteral(":/icons/%1.svg").arg(alias);
    }

    QColor mixed(const QColor &foreground, const QColor &background,
                 double foregroundShare)
    {
      const double backgroundShare = 1.0 - foregroundShare;

      return QColor(
        int(foreground.red() * foregroundShare +
            background.red() * backgroundShare),
        int(foreground.green() * foregroundShare +
            background.green() * backgroundShare),
        int(foreground.blue() * foregroundShare +
            background.blue() * backgroundShare));
    }

    QColor glyphColorFor(QIcon::Mode mode)
    {
      const ThemeColors &colors = ThemeManager::instance()->colors();

      switch (mode)
      {
        case QIcon::Active:
          return colors.text;
        case QIcon::Selected:
          // Painted on the selection fill, so it has to contrast with that
          // rather than with the surface behind it.
          return colors.selectionText;
        case QIcon::Disabled:
          return mixed(colors.hintText, colors.surfaceWindow, 0.5);
        case QIcon::Normal:
        default:
          return colors.hintText;
      }
    }

    /*!
     * \brief Renders one SVG in the theme's glyph colour.
     */
    class ThemedIconEngine : public QIconEngine
    {
      public:
        explicit ThemedIconEngine(QString alias) : m_alias(std::move(alias))
        {
        }

        QIconEngine *clone() const override
        {
          return new ThemedIconEngine(m_alias);
        }

        void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode,
                   QIcon::State state) override
        {
          const qreal ratio = painter->device()
                                ? painter->device()->devicePixelRatio()
                                : 1.0;

          const QPixmap pixmap = scaledPixmap(rect.size(), mode, state, ratio);

          if (!pixmap.isNull())
          {
            painter->drawPixmap(rect, pixmap);
          }
        }

        QPixmap pixmap(const QSize &size, QIcon::Mode mode,
                       QIcon::State state) override
        {
          return scaledPixmap(size, mode, state, 1.0);
        }

        QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode,
                             QIcon::State state, qreal scale) override
        {
          Q_UNUSED(state)

          if (size.isEmpty() || scale <= 0.0)
          {
            return {};
          }

          // The scheme is in the key, so flipping the theme misses the cache
          // and re-renders rather than needing anything invalidated.
          const QString key =
            QStringLiteral("composer-icon|%1|%2|%3|%4x%5|%6")
              .arg(m_alias)
              .arg(int(ThemeManager::instance()->effectiveScheme()))
              .arg(int(mode))
              .arg(size.width())
              .arg(size.height())
              .arg(scale);

          QPixmap cached;

          if (QPixmapCache::find(key, &cached))
          {
            return cached;
          }

          QSvgRenderer renderer(recoloredSvg(glyphColorFor(mode)));

          if (!renderer.isValid())
          {
            return {};
          }

          QImage image(size * scale, QImage::Format_ARGB32_Premultiplied);
          image.fill(Qt::transparent);

          QPainter painter(&image);
          renderer.render(&painter,
                          QRectF(QPointF(0.0, 0.0), QSizeF(size * scale)));
          painter.end();

          image.setDevicePixelRatio(scale);

          cached = QPixmap::fromImage(image);
          QPixmapCache::insert(key, cached);

          return cached;
        }

        bool isNull() override
        {
          return sourceSvg().isEmpty();
        }

      private:
        const QByteArray &sourceSvg()
        {
          if (!m_loaded)
          {
            m_loaded = true;

            QFile file(resourcePath(m_alias));

            if (file.open(QIODevice::ReadOnly))
            {
              m_svg = file.readAll();
            }
          }

          return m_svg;
        }

        QByteArray recoloredSvg(const QColor &glyph)
        {
          QByteArray svg = sourceSvg();

          if (svg.isEmpty())
          {
            return svg;
          }

          const QByteArray replacement = glyph.name(QColor::HexRgb).toLatin1();

          for (const char *gray : kGlyphGrays)
          {
            svg.replace(gray, replacement.constData());
          }

          return svg;
        }

        QString m_alias;
        QByteArray m_svg;
        bool m_loaded = false;
    };

  }

  QIcon IconFactory::icon(const QString &alias)
  {
    static QHash<QString, QIcon> icons;

    const auto existing = icons.constFind(alias);

    if (existing != icons.constEnd())
    {
      return existing.value();
    }

    if (!has(alias))
    {
      return {};
    }

    // QIcon takes ownership of the engine.
    const QIcon built(new ThemedIconEngine(alias));
    icons.insert(alias, built);

    return built;
  }

  bool IconFactory::has(const QString &alias)
  {
    return !alias.isEmpty() && QFile::exists(resourcePath(alias));
  }

  QStringList IconFactory::aliases()
  {
    QStringList names;

    for (const QString &file :
         QDir(QStringLiteral(":/icons")).entryList({ QStringLiteral("*.svg") },
                                                   QDir::Files, QDir::Name))
    {
      names.append(file.chopped(4));
    }

    return names;
  }

} // namespace HydroCouple::Composer
