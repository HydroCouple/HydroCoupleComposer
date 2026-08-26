#include "ui/panels/attributetablemodel.h"

#include "map/maplayer.h"

namespace HydroCouple::Composer
{
  AttributeTableModel::AttributeTableModel(QObject *parent)
    : QAbstractTableModel(parent)
  {
  }

  AttributeTableModel::~AttributeTableModel()
  {
    QObject::disconnect(m_watch);
  }

  void AttributeTableModel::setLayer(MapLayer *layer)
  {
    if (m_layer == layer)
    {
      return;
    }

    beginResetModel();

    QObject::disconnect(m_watch);

    m_layer = layer;

    // A layer is a MapLayer and, when it has attributes, an
    // IAttributeProvider as well. The cast is the question "does this layer
    // have attributes", asked once.
    m_provider = dynamic_cast<const IAttributeProvider *>(layer);
    m_fields = m_provider ? m_provider->attributeFields()
                          : QVector<AttributeField>();

    if (m_layer)
    {
      m_watch = connect(m_layer, &QObject::destroyed, this,
                        [this] { setLayer(nullptr); });
    }

    endResetModel();
  }

  MapLayer *AttributeTableModel::layer() const
  {
    return m_layer;
  }

  void AttributeTableModel::refresh()
  {
    beginResetModel();

    m_fields = m_provider ? m_provider->attributeFields()
                          : QVector<AttributeField>();

    endResetModel();
  }

  int AttributeTableModel::rowCount(const QModelIndex &parent) const
  {
    // A table has no children, and answering otherwise makes a view ask for
    // rows under every cell.
    if (parent.isValid() || !m_provider)
    {
      return 0;
    }

    return m_provider->featureCount();
  }

  int AttributeTableModel::columnCount(const QModelIndex &parent) const
  {
    return parent.isValid() ? 0 : int(m_fields.size());
  }

  QVariant AttributeTableModel::data(const QModelIndex &index, int role) const
  {
    if (!index.isValid() || !m_provider || index.column() >= m_fields.size())
    {
      return {};
    }

    if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
    {
      return {};
    }

    return m_provider->attributeValue(index.row(),
                                      m_fields.at(index.column()).name);
  }

  QVariant AttributeTableModel::headerData(int section,
                                           Qt::Orientation orientation,
                                           int role) const
  {
    if (role != Qt::DisplayRole)
    {
      return {};
    }

    if (orientation == Qt::Vertical)
    {
      // The feature's own index, which is what the selection and picking both
      // speak in — so a row number in this table means something outside it.
      return section;
    }

    if (section < 0 || section >= m_fields.size())
    {
      return {};
    }

    const AttributeField &field = m_fields.at(section);
    const QString name =
      field.displayName.isEmpty() ? field.name : field.displayName;

    return field.unit.isEmpty()
             ? name
             : QStringLiteral("%1 (%2)").arg(name, field.unit);
  }

} // namespace HydroCouple::Composer
