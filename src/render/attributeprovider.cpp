#include "render/attributeprovider.h"

namespace HydroCouple::Composer
{

  QVector<double> IAttributeProvider::numericValues(const QString &field) const
  {
    QVector<double> values;

    const int count = featureCount();
    values.reserve(count);

    for (int feature = 0; feature < count; ++feature)
    {
      bool ok = false;
      const double value = attributeValue(feature, field).toDouble(&ok);

      if (ok)
      {
        values.append(value);
      }
    }

    return values;
  }

  QVector<QVariant> IAttributeProvider::distinctValues(
    const QString &field) const
  {
    QVector<QVariant> distinct;

    const int count = featureCount();

    for (int feature = 0; feature < count; ++feature)
    {
      const QVariant value = attributeValue(feature, field);

      if (!value.isValid())
      {
        continue;
      }

      // Linear search rather than a hash: QVariant has no qHash for every
      // type it can hold, and a categorised layer with enough distinct values
      // for this to matter is one nobody can read anyway.
      if (!distinct.contains(value))
      {
        distinct.append(value);
      }
    }

    return distinct;
  }

  bool IAttributeProvider::hasField(const QString &field) const
  {
    for (const AttributeField &candidate : attributeFields())
    {
      if (candidate.name == field)
      {
        return true;
      }
    }

    return false;
  }

} // namespace HydroCouple::Composer
