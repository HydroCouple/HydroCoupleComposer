#include "dommodel/domitem.h"


#include <QtXml>

bool DomItem::m_iconsLoaded = false;
QIcon DomItem::m_elementIcon = QIcon();
QIcon DomItem::m_attributeIcon = QIcon();

DomItem::DomItem(const QDomNode &node, int row, DomItem *parent)
  : m_domNode(node),
    m_parentItem(parent),
    m_rowNumber(row)
{
  if(!m_iconsLoaded)
  {
    m_iconsLoaded = true;
    m_elementIcon = QIcon(":/HydroCoupleComposer/elementtagicon");
    m_attributeIcon = QIcon(":/HydroCoupleComposer/attributetagicon");
  }
}

DomItem::~DomItem()
{
  QHash<int,DomItem*>::iterator it;

  for (it = m_childItems.begin(); it != m_childItems.end(); ++it)
    delete it.value();
}

QDomNode DomItem::node() const
{
  return m_domNode;
}

DomItem *DomItem::parent()
{
  return m_parentItem;
}

DomItem *DomItem::child(int row)
{
  if(m_domNode.nodeType() != QDomNode::AttributeNode)
  {
    if (m_childItems.contains(row))
      return m_childItems[row];

    QDomNamedNodeMap attributeMap = m_domNode.attributes();
    int attributeSize = attributeMap.size();

    if(row >= 0 && row < attributeSize)
    {
      QDomNode childNode = attributeMap.item(row);
      DomItem *childItem = new DomItem(childNode, row, this);
      m_childItems[row] = childItem;
      return childItem;
    }

    if(row >= attributeSize && row < attributeSize + m_domNode.childNodes().count())
    {
      QDomNode childNode = m_domNode.childNodes().item(row - attributeSize);
      DomItem *childItem = new DomItem(childNode, row, this);
      m_childItems[row] = childItem;
      return childItem;
    }
  }

  return nullptr;
}

int DomItem::row()
{
  return m_rowNumber;
}

QVariant DomItem::data(int column, int role) const
{
  switch (column)
  {
    case 0:
      {
        switch (role)
        {
          case Qt::DisplayRole:
            {
              return m_domNode.nodeName();
            }
            break;
          case Qt::DecorationRole:
            {
              switch (m_domNode.nodeType())
              {
                case QDomNode::ElementNode:
                case QDomNode::TextNode:
                case QDomNode::CDATASectionNode:
                case QDomNode::EntityReferenceNode:
                case QDomNode::EntityNode:
                case QDomNode::ProcessingInstructionNode:
                case QDomNode::CommentNode:
                case QDomNode::DocumentNode:
                case QDomNode::DocumentTypeNode:
                case QDomNode::NotationNode:
                case QDomNode::BaseNode:
                case QDomNode::CharacterDataNode:
                  return m_elementIcon;
                  break;
                case QDomNode::AttributeNode:
                  return m_attributeIcon;
                  break;
                default:
                  return QVariant();
                  break;
              }
            }
            break;
          default:
            return QVariant();
            break;
        }
        break;
      }
    case 1:
      {
        switch (role)
        {
          case Qt::DisplayRole:
          case Qt::EditRole:
            {
              return m_domNode.nodeValue();
            }
            break;
        }
        break;
      }
      break;
    default:
      return QVariant();
      break;
  }

  return QVariant();
}

bool DomItem::setData(int column, const QVariant &value, int role)
{
  if(column == 1)
  {
    m_domNode.setNodeValue(value.toString());
  }

  return false;
}

Qt::ItemFlags DomItem::flags(int column) const
{
  Qt::ItemFlags  flags;

  flags = flags | Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable;

  if(column == 1)
  {
    flags = flags | Qt::ItemFlag::ItemIsEditable;
  }

  return flags;
}


int DomItem::getRowCount() const
{
  if(m_domNode.nodeType() == QDomNode::AttributeNode)
  {
    return 0;
  }

  return m_domNode.attributes().count() + m_domNode.childNodes().count();
}
