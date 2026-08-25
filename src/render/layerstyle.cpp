#include "render/layerstyle.h"

#include "render/attributeprovider.h"

namespace HydroCouple::Composer
{

  LayerStyle::LayerStyle() = default;

  StyleMode LayerStyle::mode() const
  {
    return m_mode;
  }

  void LayerStyle::setMode(StyleMode mode)
  {
    m_mode = mode;
  }

  QString LayerStyle::attribute() const
  {
    return m_attribute;
  }

  void LayerStyle::setAttribute(const QString &attribute)
  {
    m_attribute = attribute;
  }

  const Symbol &LayerStyle::symbol() const
  {
    return m_symbol;
  }

  void LayerStyle::setSymbol(const Symbol &symbol)
  {
    m_symbol = symbol;
  }

  Classification &LayerStyle::classification()
  {
    return m_classification;
  }

  const Classification &LayerStyle::classification() const
  {
    return m_classification;
  }

  const LabelConfig &LayerStyle::labels() const
  {
    return m_labels;
  }

  LabelConfig &LayerStyle::labels()
  {
    return m_labels;
  }

  const QVector<StyleCategory> &LayerStyle::categories() const
  {
    return m_categories;
  }

  bool LayerStyle::rebuild(const IAttributeProvider &provider)
  {
    if (m_mode == StyleMode::Single)
    {
      return true;
    }

    // A field the layer does not have — a style carried over from another
    // dataset, or a renamed column — yields no values, so the paths below
    // already refuse to colour anything. There is deliberately no separate
    // guard for it: the one that stood here reset the classification, which
    // threw away the ramp and class count the user had chosen.
    if (m_mode == StyleMode::Graduated)
    {
      return m_classification.classify(provider.numericValues(m_attribute));
    }

    const QVector<QVariant> distinct = provider.distinctValues(m_attribute);

    // Colours come from the same ramp the graduated mode uses, so switching
    // between the two modes does not change the layer's palette.
    const QVector<QColor> colors =
      m_classification.ramp().sample(static_cast<int>(distinct.size()));

    QVector<StyleCategory> rebuilt;
    rebuilt.reserve(distinct.size());

    for (int i = 0; i < distinct.size(); ++i)
    {
      StyleCategory category;
      category.value = distinct.at(i);
      category.label = distinct.at(i).toString();
      category.color = i < colors.size() ? colors.at(i) : m_symbol.fill;

      // Visibility set in the legend survives a rebuild: refreshing a layer
      // must not switch categories the user turned off back on.
      for (const StyleCategory &previous : m_categories)
      {
        if (previous.value == category.value)
        {
          category.visible = previous.visible;
          category.color = previous.color;
          break;
        }
      }

      rebuilt.append(category);
    }

    m_categories = rebuilt;

    return !m_categories.isEmpty();
  }

  QColor LayerStyle::colorFor(const IAttributeProvider &provider,
                              int feature) const
  {
    if (m_mode == StyleMode::Single)
    {
      return m_symbol.fill;
    }

    const QVariant value = provider.attributeValue(feature, m_attribute);

    if (m_mode == StyleMode::Graduated)
    {
      bool ok = false;
      const double numeric = value.toDouble(&ok);

      if (!ok)
      {
        return {};
      }

      const int index = m_classification.indexFor(numeric);

      if (index < 0)
      {
        return {};
      }

      const ClassBreak &entry = m_classification.breaks().at(index);

      return entry.visible ? entry.color : QColor();
    }

    for (const StyleCategory &category : m_categories)
    {
      if (category.value == value)
      {
        return category.visible ? category.color : QColor();
      }
    }

    // A value no category covers is left undrawn rather than given the base
    // symbol, which would make it look like a category of its own.
    return {};
  }

  QVector<LegendItem> LayerStyle::legendItems() const
  {
    QVector<LegendItem> items;

    if (m_mode == StyleMode::Single)
    {
      return items;
    }

    if (m_mode == StyleMode::Graduated)
    {
      const QVector<ClassBreak> &breaks = m_classification.breaks();
      items.reserve(breaks.size());

      for (int i = 0; i < breaks.size(); ++i)
      {
        items.append(
          {breaks.at(i).label, breaks.at(i).color, breaks.at(i).visible, i});
      }

      return items;
    }

    items.reserve(m_categories.size());

    for (int i = 0; i < m_categories.size(); ++i)
    {
      items.append({m_categories.at(i).label, m_categories.at(i).color,
                    m_categories.at(i).visible, i});
    }

    return items;
  }

  void LayerStyle::setLegendItemVisible(int index, bool visible)
  {
    if (m_mode == StyleMode::Graduated)
    {
      m_classification.setClassVisible(index, visible);
      return;
    }

    if (index >= 0 && index < m_categories.size())
    {
      m_categories[index].visible = visible;
    }
  }

  void LayerStyle::setCategoryColor(int index, const QColor &color)
  {
    if (index >= 0 && index < m_categories.size() && color.isValid())
    {
      m_categories[index].color = color;
    }
  }

} // namespace HydroCouple::Composer
