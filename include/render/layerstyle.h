/*!
 * \file   layerstyle.h
 * \author Caleb Buahin
 * \brief  LayerStyle — how a layer's features are drawn, and the legend it
 *         produces.
 *
 * One style object answers both questions a renderer and a legend ask, and
 * answers them from the same state: what colour is this feature, and what
 * does the reader need to see to interpret that colour. Keeping the legend
 * derived rather than stored is what stops a map and its legend disagreeing.
 */

#ifndef HYDROCOUPLECOMPOSER_RENDER_LAYERSTYLE_H
#define HYDROCOUPLECOMPOSER_RENDER_LAYERSTYLE_H

#include "render/classification.h"
#include "render/labelconfig.h"

#include <QColor>
#include <QString>
#include <QVariant>
#include <QVector>

namespace HydroCouple::Composer
{
  class IAttributeProvider;

  /*!
   * \brief How a layer's colours are chosen.
   */
  enum class StyleMode
  {
    Single,      //!< One symbol for every feature.
    Graduated,   //!< Numeric attribute split into ordered classes.
    Categorized  //!< One colour per distinct value.
  };

  /*!
   * \brief The drawing properties shared by every feature.
   */
  struct Symbol
  {
      QColor fill = QColor(0x31, 0x82, 0xbd);
      QColor stroke = QColor(0x25, 0x5b, 0x87);
      double strokeWidth = 1.0;
      double size = 6.0;  //!< Point diameter, in pixels.
  };

  /*!
   * \brief One value of a categorised style.
   */
  struct StyleCategory
  {
      QVariant value;
      QColor color;
      QString label;
      bool visible = true;
  };

  /*!
   * \brief One row of a legend.
   */
  struct LegendItem
  {
      QString label;
      QColor color;
      bool visible = true;

      //! Index into the style's classes or categories, for editing it back.
      int index = -1;
  };

  /*!
   * \brief How a layer draws its features.
   */
  class LayerStyle
  {
    public:
      LayerStyle();

      /*!
       * \brief The colouring mode.
       */
      [[nodiscard]] StyleMode mode() const;

      /*!
       * \brief Sets the colouring mode.
       * \param mode The mode to use.
       */
      void setMode(StyleMode mode);

      /*!
       * \brief The attribute graduated and categorised modes read.
       */
      [[nodiscard]] QString attribute() const;

      /*!
       * \brief Sets the attribute to theme by.
       * \param attribute Field name, as the provider reports it.
       */
      void setAttribute(const QString &attribute);

      /*!
       * \brief The base symbol.
       */
      [[nodiscard]] const Symbol &symbol() const;

      /*!
       * \brief Sets the base symbol.
       * \param symbol The symbol to draw with.
       */
      void setSymbol(const Symbol &symbol);

      /*!
       * \brief The graduated classification, for reading and editing.
       */
      [[nodiscard]] Classification &classification();

      /*!
       * \brief The graduated classification.
       */
      [[nodiscard]] const Classification &classification() const;

      /*!
       * \brief The categorised values.
       */
      [[nodiscard]] const QVector<StyleCategory> &categories() const;

      /*!
       * \brief How this layer's features are labelled.
       *
       * Labelling lives in the style rather than beside it: both answer the
       * same question — how this layer appears — and a reader changing one
       * almost always wants the other to hand.
       */
      [[nodiscard]] const LabelConfig &labels() const;

      /*!
       * \brief The label configuration, for editing.
       */
      [[nodiscard]] LabelConfig &labels();

      /*!
       * \brief Rebuilds classes or categories from a layer's data.
       *
       * Does nothing in Single mode, which needs no data to draw.
       *
       * \param provider The layer's attributes.
       * \returns True when the style can now colour features.
       */
      bool rebuild(const IAttributeProvider &provider);

      /*!
       * \brief The colour for one feature, or an invalid colour to skip it.
       *
       * An invalid colour means "do not draw": the feature falls outside
       * every class, or its class has been switched off in the legend.
       *
       * \param provider The layer's attributes.
       * \param feature Feature index.
       */
      [[nodiscard]] QColor colorFor(const IAttributeProvider &provider,
                                    int feature) const;

      /*!
       * \brief The legend rows for this style.
       */
      [[nodiscard]] QVector<LegendItem> legendItems() const;

      /*!
       * \brief Shows or hides one legend row.
       * \param index Row index, as reported in LegendItem::index.
       * \param visible Whether matching features are drawn.
       */
      void setLegendItemVisible(int index, bool visible);

      /*!
       * \brief Sets one category's colour.
       * \param index Category index.
       * \param color The colour to use.
       */
      void setCategoryColor(int index, const QColor &color);

    private:
      StyleMode m_mode = StyleMode::Single;
      QString m_attribute;
      Symbol m_symbol;
      Classification m_classification;
      QVector<StyleCategory> m_categories;
      LabelConfig m_labels;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_RENDER_LAYERSTYLE_H
