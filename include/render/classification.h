/*!
 * \file   classification.h
 * \author Caleb Buahin
 * \brief  Classification — turning a column of numbers into map classes.
 *
 * The method matters more than it looks. Equal interval shows the shape of
 * the range; quantile shows the shape of the distribution; natural breaks
 * looks for the gaps the data already has. The same values under two methods
 * produce two maps that tell different stories, so the method is part of the
 * style and is written into the legend rather than assumed.
 */

#ifndef HYDROCOUPLECOMPOSER_RENDER_CLASSIFICATION_H
#define HYDROCOUPLECOMPOSER_RENDER_CLASSIFICATION_H

#include "render/colorramp.h"

#include <QColor>
#include <QString>
#include <QVector>

namespace HydroCouple::Composer
{

  /*!
   * \brief How class boundaries are chosen.
   */
  enum class ClassificationMethod
  {
    EqualInterval,  //!< Equal-width classes across the range.
    Quantile,       //!< Equal feature counts per class.
    NaturalBreaks,  //!< Jenks: minimises variance within classes.
    Manual          //!< Boundaries set by the user; never recomputed.
  };

  /*!
   * \brief One class of a graduated style.
   *
   * The interval is half-open, [lower, upper), so adjacent classes cannot
   * both claim a boundary value. The last class includes its upper bound —
   * otherwise the single largest feature in the layer falls out of the map.
   */
  struct ClassBreak
  {
      double lower = 0.0;
      double upper = 0.0;
      QColor color;
      QString label;
      bool visible = true;  //!< Per-class visibility, driven from the legend.
  };

  /*!
   * \brief A set of class breaks and the recipe that produced them.
   */
  class Classification
  {
    public:
      Classification();

      /*!
       * \brief The method used to choose boundaries.
       */
      [[nodiscard]] ClassificationMethod method() const;

      /*!
       * \brief Sets the method. Does not recompute; call classify().
       * \param method The method to use.
       */
      void setMethod(ClassificationMethod method);

      /*!
       * \brief How many classes to produce.
       */
      [[nodiscard]] int classCount() const;

      /*!
       * \brief Sets the class count. Does not recompute; call classify().
       * \param count Clamped to [1, 24].
       */
      void setClassCount(int count);

      /*!
       * \brief The ramp colours are drawn from.
       */
      [[nodiscard]] const ColorRamp &ramp() const;

      /*!
       * \brief Sets the ramp and recolours the existing classes.
       * \param ramp The ramp to use.
       */
      void setRamp(const ColorRamp &ramp);

      /*!
       * \brief Decimal places used in class labels.
       */
      [[nodiscard]] int labelPrecision() const;

      /*!
       * \brief Sets the decimal places used in class labels.
       * \param precision Clamped to [0, 12].
       */
      void setLabelPrecision(int precision);

      /*!
       * \brief Computes class breaks from \a values.
       *
       * Values need not be sorted. Fewer distinct values than requested
       * classes yields fewer classes rather than empty ones, because an
       * empty class is a legend entry that matches nothing.
       *
       * \param values The data to classify.
       * \returns True when at least one class resulted.
       */
      bool classify(const QVector<double> &values);

      /*!
       * \brief Sets class boundaries directly, switching to Manual.
       * \param boundaries Ascending edge values; N edges give N-1 classes.
       * \returns True when at least one class resulted.
       */
      bool setManualBreaks(const QVector<double> &boundaries);

      /*!
       * \brief The classes, in ascending order.
       */
      [[nodiscard]] const QVector<ClassBreak> &breaks() const;

      /*!
       * \brief The index of the class containing \a value, or -1.
       * \param value The value to place.
       */
      [[nodiscard]] int indexFor(double value) const;

      /*!
       * \brief Shows or hides one class.
       * \param index Class index.
       * \param visible Whether features in the class are drawn.
       */
      void setClassVisible(int index, bool visible);

      /*!
       * \brief Overrides one class's colour.
       * \param index Class index.
       * \param color The colour to use.
       */
      void setClassColor(int index, const QColor &color);

      /*!
       * \brief Whether any classes have been computed.
       */
      [[nodiscard]] bool isEmpty() const;

      /*!
       * \brief A human-readable name for \a method.
       * \param method The method to name.
       */
      [[nodiscard]] static QString methodName(ClassificationMethod method);

    private:
      void buildFrom(const QVector<double> &boundaries);
      void applyRampColors();

      ClassificationMethod m_method = ClassificationMethod::EqualInterval;
      int m_classCount = 5;
      int m_labelPrecision = 2;
      ColorRamp m_ramp;
      QVector<ClassBreak> m_breaks;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_RENDER_CLASSIFICATION_H
