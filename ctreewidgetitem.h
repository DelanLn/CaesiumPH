#ifndef CTREEWIDGETITEM_H
#define CTREEWIDGETITEM_H

#include <QTreeWidgetItem>
#include <QFileInfo>

#include <utils.h>

class CTreeWidgetItem : public QTreeWidgetItem
{
public:
    CTreeWidgetItem(QTreeWidget* parent) : QTreeWidgetItem(parent) {}
    CTreeWidgetItem(QTreeWidget* parent, QStringList list) : QTreeWidgetItem(parent, list) {}
private:
    bool operator< (const QTreeWidgetItem &other) const {
        int column = treeWidget()->sortColumn();
        switch (column) {
        case 1:
            return QFileInfo(text(COLUMN_PATH)).size() < QFileInfo(other.text(COLUMN_PATH)).size();
        case 2:
            //Sort by compressed size
            /*
             * WARNING This methods ignores the less significant bytes of the size
             * and may lead to inaccurate sorting. Won't fix for now as not critical.
             */
            return humanToDouble(text(column)) < humanToDouble(other.text(column));
        case 3:
            //Sort by saved space
            return ratioToDouble(text(column)) < ratioToDouble(other.text(column));
        default:
            //Sort by path
            return text(column).toLower() < other.text(column).toLower();
        }
    }
};

#endif // CTREEWIDGETITEM_H
