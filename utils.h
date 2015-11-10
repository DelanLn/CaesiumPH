#ifndef UTILS_H
#define UTILS_H

#include "usageinfo.h"

#include <QList>
#include <QStringList>
#include <QSize>

enum cexifs {
    EXIF_COPYRIGHT,
    EXIF_DATE,
    EXIF_COMMENTS
};

typedef struct var {
    int exif;
    QList<cexifs> importantExifs;
    int progressive;
    bool overwrite;
    int outMethodIndex;
    QString outMethodString;
} cparams;

extern QString inputFilter;
extern QStringList inputFilterList;
extern QString versionString;
extern int versionNumber;
extern int buildNumber;
extern long originalsSize, compressedSize; //Before and after bytes count
extern cparams params; //Important parameters
extern UsageInfo* uinfo;
extern QStringList osAndExtension;

QString toHumanSize(int);
double humanToDouble(QString);
QString getRatio(qint64, qint64);
char* QStringToChar(QString s);
QSize getScaledSizeWithRatio(QSize size, int square); //Image preview resize
double ratioToDouble(QString ratio);
bool isJPEG(char* path);

#endif // UTILS_H
