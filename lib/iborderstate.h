// Copyright (C) 2016  ProDataLab, Peter Alexander
// GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
// http://www.gnu.org/licenses/gpl-3.0-standalone.html

#ifndef IBORDERSTATE_H
#define IBORDERSTATE_H

#include "iborder.h"

struct OrderState {

    OrderState()
        :
        commission(UNSET_DOUBLE),
        minCommission(UNSET_DOUBLE),
        maxCommission(UNSET_DOUBLE)
    {}

    QByteArray status;

    QByteArray initMargin;
    QByteArray maintMargin;
    QByteArray equityWithLoan;

    double  commission;
    double  minCommission;
    double  maxCommission;
    QByteArray commissionCurrency;

    QByteArray warningText;
};

#endif // IBORDERSTATE_H

