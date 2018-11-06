#ifndef DOMITEM_H
#define DOMITEM_H

#include <QDomNode>
#include <QHash>
#include <QVariant>
#include <QIcon>

class DomItem
{
public:

    DomItem(const QDomNode &node, int row, DomItem *parent = nullptr);

    ~DomItem();

    DomItem *child(int row);

    DomItem *parent();

    QDomNode node() const;

    int row();

    QVariant data(int column, int role) const;

    bool setData(int column, const QVariant &value, int role);

    Qt::ItemFlags flags(int column) const;

    int getRowCount() const;

private:
    static bool m_iconsLoaded;
    static QIcon m_elementIcon, m_attributeIcon;
    QDomNode m_domNode;
    QHash<int,DomItem*> m_childItems;
    DomItem *m_parentItem;
    int m_rowNumber;
};

#endif // DOMITEM_H
