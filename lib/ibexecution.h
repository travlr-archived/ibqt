// Copyright (C) 2016  ProDataLab, Peter Alexander
// GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
// http://www.gnu.org/licenses/gpl-3.0-standalone.html

#ifndef IBEXECUTION_H
#define IBEXECUTION_H

#include <QByteArray>

struct Execution
{
    Execution()
    {
        shares = 0;
        price = 0;
        permId = 0;
        clientId = 0;
        orderId = 0;
        cumQty = 0;
        avgPrice = 0;
        evMultiplier = 0;
    }

    QByteArray	execId;
    QByteArray	time;
    QByteArray	acctNumber;
    QByteArray	exchange;
    QByteArray	side;
    int			shares;
    double		price;
    int			permId;
    long		clientId;
    long		orderId;
    int			liquidation;
    int			cumQty;
    double		avgPrice;
    QByteArray	orderRef;
    QByteArray	evRule;
    double		evMultiplier;
};

struct ExecutionFilter
{
    ExecutionFilter()
        : m_clientId(0)
    {
    }

    // Filter fields
    long		m_clientId;
    QByteArray	m_acctCode;
    QByteArray	m_time;
    QByteArray	m_symbol;
    QByteArray	m_secType;
    QByteArray	m_exchange;
    QByteArray	m_side;
};

#endif // IBEXECUTION_H

