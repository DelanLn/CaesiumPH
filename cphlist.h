#ifndef CLIST_H
#define CLIST_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QTreeWidgetItem>

#define CLF_VERSION 1

class CPHList {
public:
    CPHList();
    QList<QTreeWidgetItem *> readFile(QString path);
    void writeToFile(QList<QTreeWidgetItem *> list, QString path);
};

#endif // CLIST_H
