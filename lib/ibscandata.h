// Copyright (C) 2016  ProDataLab, Peter Alexander
// GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
// http://www.gnu.org/licenses/gpl-3.0-standalone.html

#ifndef IBSCANDATA_H
#define IBSCANDATA_H

#include "ibcontract.h"
#include <QByteArray>

struct ScanData {
    ContractDetails contract;
    int rank;
    QByteArray distance;
    QByteArray benchmark;
    QByteArray projection;
    QByteArray legsStr;
};

#endif // IBSCANDATA_H

