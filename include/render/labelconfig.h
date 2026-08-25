/*!
 * \file   labelconfig.h
 * \author Caleb Buahin
 * \brief  LabelConfig and LabelPainter — drawing feature labels legibly.
 *
 * Labels are where a map stops being a picture and starts being readable, and
 * where an unconsidered implementation ruins both: text over text is worse
 * than no text at all. LabelPainter therefore refuses placements that collide
 * with labels already drawn, and draws a halo so text stays legible over
 * whatever it lands on.
 */

#ifndef HYDROCOUPLECOMPOSER_RENDER_LABELCONFIG_H
#define HYDROCOUPLECOMPOSER_RENDER_LABELCONFIG_H

#include <QColor>
#include <QFont>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVector>

class QPainter;

namespace HydroCouple::Composer
{

  /*!
   * \brief Where a label sits relative to its feature.
   */
  enum class LabelPlacement
  {
    AboveRight,  //!< The cartographic default for point features.
    Above,
    Below,
    Left,
    Right,
    Centered
  };

  /*!
   * \brief How a layer labels its features.
   */
  struct LabelConfig
  {
      bool enabled = false;

      //! Attribute whose value is drawn. Empty means nothing is labelled.
      QString fieldName;

      QFont font;
      QColor color = Qt::black;

      //! Halo colour drawn behind the text; transparent disables the halo.
      QColor haloColor = QColor(255, 255, 255, 220);

      double haloWidth = 2.0;
      LabelPlacement placement = LabelPlacement::AboveRight;

      //! Gap in pixels between the feature and its text.
      double offset = 4.0;

      //! Scale bounds as map-scale denominators, 0 meaning unbounded. Labels
      //! hide when zoomed further out than minScale or further in than
      //! maxScale — the standard way to keep a dense layer readable.
      double minScale = 0.0;
      double maxScale = 0.0;
  };

  /*!
   * \brief Stateless label placement and drawing.
   */
  class LabelPainter
  {
    public:
      /*!
       * \brief Whether labels show at the given map scale.
       * \param config The layer's label configuration.
       * \param scaleDenominator The map scale as 1:N; non-positive disables
       *        the check.
       */
      [[nodiscard]] static bool scaleVisible(const LabelConfig &config,
                                             double scaleDenominator);

      /*!
       * \brief The text box for a label anchored at \a anchor.
       * \param config The layer's label configuration.
       * \param anchor The feature's position, in pixels.
       * \param textSize The text's bounding size.
       */
      [[nodiscard]] static QRectF labelRect(const LabelConfig &config,
                                            const QPointF &anchor,
                                            const QSizeF &textSize);

      /*!
       * \brief Draws \a text in \a box, with a halo when configured.
       * \param painter Painter to draw with.
       * \param config The layer's label configuration.
       * \param box The rectangle from labelRect().
       * \param text The text to draw.
       */
      static void draw(QPainter &painter, const LabelConfig &config,
                       const QRectF &box, const QString &text);
  };

  /*!
   * \brief Accepts labels that fit and rejects those that would overlap.
   *
   * One instance per rendered frame, in draw order: the first label to ask
   * for a piece of the canvas keeps it. Draw order is therefore priority
   * order, which is why layers draw their most important features first.
   */
  class LabelCollisionMap
  {
    public:
      /*!
       * \brief Constructs an empty map.
       * \param padding Extra space kept clear around each label, in pixels.
       */
      explicit LabelCollisionMap(double padding = 2.0);

      /*!
       * \brief Reserves \a box when it is free.
       * \param box The label's rectangle.
       * \returns True when the label may be drawn.
       */
      bool tryPlace(const QRectF &box);

      /*!
       * \brief How many labels have been placed.
       */
      [[nodiscard]] int placedCount() const;

      /*!
       * \brief Forgets every placement, for the next frame.
       */
      void clear();

    private:
      QVector<QRectF> m_placed;
      double m_padding = 2.0;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_RENDER_LABELCONFIG_H
