

#include "dommodel/domitem.h"
#include "dommodel/dommodel.h"

#include <QtXml>



DomModel::DomModel(const QDomDocument &document, QObject *parent)
  : QAbstractItemModel(parent),
    m_domDocument(document)
{
  m_rootItem = new DomItem(m_domDocument, 0);


}

DomModel::~DomModel()
{
  delete m_rootItem;
}

QVariant DomModel::data(const QModelIndex &index, int role) const
{
  if (!index.isValid())
    return QVariant();

  DomItem *item = static_cast<DomItem*>(index.internalPointer());
  return item->data(index.column(), role);
}

Qt::ItemFlags DomModel::flags(const QModelIndex &index) const
{
  if (!index.isValid())
    return 0;

  DomItem *item = static_cast<DomItem*>(index.internalPointer());
  return item->flags(index.column());
}

QVariant DomModel::headerData(int section, Qt::Orientation orientation,
                              int role) const
{
  if (orientation == Qt::Horizontal
      && role == Qt::DisplayRole)
  {
    switch (section)
    {
      case 0:
        return tr("Name");
      case 1:
        return tr("Value");
      default:
        return QVariant();
    }
  }

  return QVariant();
}

QModelIndex DomModel::index(int row, int column, const QModelIndex &parent) const
{
  if (!hasIndex(row, column, parent))
    return QModelIndex();

  DomItem *parentItem;

  if (!parent.isValid())
    parentItem = m_rootItem;
  else
    parentItem = static_cast<DomItem*>(parent.internalPointer());

  DomItem *childItem = parentItem->child(row);

  if (childItem)
    return createIndex(row, column, childItem);
  else
    return QModelIndex();
}

QModelIndex DomModel::parent(const QModelIndex &child) const
{
  if (!child.isValid())
    return QModelIndex();

  DomItem *childItem = static_cast<DomItem*>(child.internalPointer());
  DomItem *parentItem = childItem->parent();

  if (!parentItem || parentItem == m_rootItem)
    return QModelIndex();

  return createIndex(parentItem->row(), 0, parentItem);
}

int DomModel::rowCount(const QModelIndex &parent) const
{
  if (parent.column() > 0)
    return 0;

  DomItem *parentItem;

  if (!parent.isValid())
    parentItem = m_rootItem;
  else
    parentItem = static_cast<DomItem*>(parent.internalPointer());

  return parentItem->getRowCount();
}

int DomModel::columnCount(const QModelIndex &/*parent*/) const
{
  return 2;
}

bool DomModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
  if (!index.isValid())
    return false;

  DomItem *item = static_cast<DomItem*>(index.internalPointer());
  return item->setData(index.column(), value, role);
}

void DomModel::setDomDocument(const QDomDocument &domDocument)
{
  beginResetModel();

  m_domDocument = domDocument;
  delete m_rootItem;
  m_rootItem = new DomItem(domDocument, 0);

  endResetModel();
}

QDomDocument DomModel::domDocument() const
{
  return m_domDocument;
}
