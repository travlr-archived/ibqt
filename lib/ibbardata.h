// Copyright (C) 2016  ProDataLab, Peter Alexander
// GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
// http://www.gnu.org/licenses/gpl-3.0-standalone.html

#ifndef IBBARDATA_H
#define IBBARDATA_H

#include <QByteArray>

struct BarData {
    QByteArray date;
    double open;
    double high;
    double low;
    double close;
    int volume;
    double average;
    QByteArray hasGaps;
    int barCount;
};

#endif // IBBARDATA_H

