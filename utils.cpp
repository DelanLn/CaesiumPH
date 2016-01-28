/**
 *
 * This file is part of CaesiumPH.
 *
 * CaesiumPH - A Caesium version featuring lossless JPEG optimization/compression
 * for photographers and webmasters.
 *
 * Copyright (C) 2016 - Matteo Paonessa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>
 *
 */

#include "utils.h"
#include "math.h"
#include <stdlib.h>

#include <QIODevice>
#include <QDate>
#include <QTreeWidgetItem>

#include <QDebug>

QString inputFilter = QIODevice::tr("Image Files") + " (*.jpg *.jpeg)";
QString clfFilter = "CaesiumPH List File (*.cphlf)";
QStringList inputFilterList = QStringList() << "*.jpg" << "*.jpeg";
QString versionString = "0.9.4-beta";
int versionNumber = 92;
int buildNumber = 20160126;
QString updateVersionTag = "";
long originalsSize = 0;
long compressedSize = 0;
int compressedFiles = 0;
cparams params;
UsageInfo* uinfo = new UsageInfo();
QStringList osAndExtension = QStringList() <<
        #ifdef _WIN32
            "win" << ".exe";
        #elif __APPLE__
            "osx" << ".dmg";
        #else
            "linux" << ".tar.gz";
        #endif
QTemporaryDir tempDir;
QElapsedTimer timer;
QString lastCPHListPath = "";

QString toHumanSize(long size) {
    //Check if size is 0 to avoid crashes
    if (size == 0) {
        return "0 bytes";
    }

    QStringList unit;
    unit << "Bytes" << "Kb" << "Mb" << "Gb" << "Tb";
    //Index of the array containing the correct unit
    double order = floor(log2(labs(size)) / 10);

    //We should never handle files over 1k Tb, but...
    if (order > 4) {
        qWarning() << "Woah, huge collection!";
        order = 4;
    }

    return QString::number(size / (pow(1024, order)), 'f', 2) + unit[(int)order];
}

double humanToDouble(QString human_size) {
    QStringList splitted = human_size.split(" ");
    if (splitted.at(1) == "Kb") {
        return QString(splitted.at(0)).toDouble() * 1024;
    } else if (splitted.at(1) == "Mb") {
        return QString(splitted.at(0)).toDouble() * 1048576;
    } else {
        return QString(splitted.at(0)).toDouble();
    }
}

QString getRatio(qint64 original, qint64 compressed) {
    return QString::number(((float) ((original - compressed) * 100) / (float) original), 'f', 1) + "%";
}

char* QStringToChar(QString s) {
    char* c_str = (char*) malloc((s.length() + 1) * sizeof(char));
    QByteArray bArray = s.toLocal8Bit();
    strcpy(c_str, bArray.data());
    return c_str;
}

QSize getScaledSizeWithRatio(QSize size, int square) {
    int w = size.width();
    int h = size.height();

    double ratio = 0.0;

    //Check the biggest between the two and scale on that dimension
    if (w >= h) {
        ratio = w / (double) square;
    } else {
        ratio = h / (double) square;
    }

    return QSize((int) round(w / ratio), (int) h / ratio);
}

double ratioToDouble(QString ratio) {
    ratio = ratio.split("%").at(0);
    return ratio.toDouble();
}

bool isJPEG(char* path) {
    FILE* fp;
    unsigned char* type_buffer = (unsigned char*) malloc(2);

    fp = fopen(path, "r");

    if (fp == NULL) {
        fprintf(stderr, "Cannot open input file for type detection. Skipping.\n");
        return false;
    }

    if (fread(type_buffer, 1, 2, fp) < 2) {
        fprintf(stderr, "Cannot read file type. Skipping.\n");
        return false;
    }

    fclose(fp);

    if (((int) type_buffer[0] == 0xFF) && ((int) type_buffer[1] == 0xD8)) {
        free(type_buffer);
        return true;
    } else {
        fprintf(stderr, "Unsupported file type. Skipping.\n");
        return false;
    }
}

QString msToFormattedString(qint64 ms) {
    if (ms < 1000) {
        return QString::number(ms) + " ms";
    } else if (ms >= 1000 && ms < 60000) {
        return QString::number(((double) ms) / 1000, 'f', 1) + "s";
    } else {
        return QString::number(ms / 60000) + ":" + (ms < 70000 ? "0" : "") + QString::number(ms / 1000 % 60) + QT_TR_NOOP(" minutes");
    }
}

bool haveSameRootFolder(QList<QTreeWidgetItem *> items) {
    QDir root = QFileInfo(items.at(0)->text(COLUMN_PATH)).dir();
    for (int i = 1; i < items.length(); i++) {
        if (QString::compare(QFileInfo(items.at(i)->text(COLUMN_PATH)).dir().absolutePath(),
                             root.absolutePath(),
                             Qt::CaseSensitive) != 0) {
            return false;
        }
    }
    return true;
}
