// Copyright (C) 2016  ProDataLab, Peter Alexander
// GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
// http://www.gnu.org/licenses/gpl-3.0-standalone.html

#ifndef IBCONTRACT_H
#define IBCONTRACT_H

#include "ibtagvalue.h"
#include <QByteArray>
#include <QList>

/*
 * SAME_POS = open/close leg value is same as combo
 */

enum LegOpenClose
{
    SAME_POS,
    OPEN_POS,
    CLOSE_POS,
    UNKNOWN_POS
};

struct ComboLeg
{
    ComboLeg()
        : conId(0)
        , ratio(0)
        , openClose(0)
        , shortSaleSlot(0)
        , exemptCode(-1)
    {}

    bool operator==(const ComboLeg & other) const
    {
        return (conId == other.conId
                && ratio == other.ratio
                && openClose == other.openClose
                && shortSaleSlot == other.shortSaleSlot
                && action == other.action
                && exchange == other.exchange
                && designatedLocation == other.designatedLocation
                && exemptCode == other.exemptCode);
    }

    long        conId;
    long        ratio;
    QByteArray  action;     // BUY/SELL/SSHORT
    QByteArray  exchange;
    long        openClose;

    // for stock legs when doing short sale
    long        shortSaleSlot;  // 1 = clearing broker, 2 = third party
    QByteArray  designatedLocation;
    int         exemptCode;
};

struct UnderComp
{
    UnderComp()
        : conId(0)
        , delta(0)
        , price(0)
    {}

    long    conId;
    double  delta;
    double  price;
};

struct Contract
{
    static void CloneComboLegs(QList<ComboLeg*>& dst, const QList<ComboLeg*>& src);


    Contract()
        : conId(0)
        , strike(0)
        , includeExpired(false)
        , underComp(NULL)
        , m_conId(0)
    {}

    long getConId() { return ++m_conId; }

    long		conId;
    QByteArray	symbol;
    QByteArray	secType;
    QByteArray	expiry;
    double		strike;
    QByteArray	right;
    QByteArray	multiplier;
    QByteArray	exchange;
    QByteArray	primaryExchange; // pick an actual (ie non-aggregate) exchange that the contract trades on.  DO NOT SET TO SMART.
    QByteArray	currency;
    QByteArray	localSymbol;
    QByteArray	tradingClass;
    bool		includeExpired;
    QByteArray	secIdType;		// CUSIP;SEDOL;ISIN;RIC
    QByteArray	secId;

    // COMBOS
    QByteArray comboLegsDescrip; // received in open order 14 and up for all combos

    QList<ComboLeg*> comboLegs;

    // delta neutral
    UnderComp* underComp;

private:
    long m_conId;

};


struct ContractDetails
{
    ContractDetails()
        : minTick(0)
        , priceMagnifier(0)
        , underConId(0)
        , evMultiplier(0)
        , callable(false)
        , putable(false)
        , coupon(0)
        , convertible(false)
        , nextOptionPartial(false)
    {
    }

    Contract	summary;
    QByteArray	marketName;
    double		minTick;
    QByteArray	orderTypes;
    QByteArray	validExchanges;
    long		priceMagnifier;
    int			underConId;
    QByteArray	longName;
    QByteArray	contractMonth;
    QByteArray	industry;
    QByteArray	category;
    QByteArray	subcategory;
    QByteArray	timeZoneId;
    QByteArray	tradingHours;
    QByteArray	liquidHours;
    QByteArray	evRule;
    double		evMultiplier;

    QList<TagValue*> secIdList;

    // BOND values
    QByteArray	cusip;
    QByteArray	ratings;
    QByteArray	descAppend;
    QByteArray	bondType;
    QByteArray	couponType;
    bool		callable;
    bool		putable;
    double		coupon;
    bool		convertible;
    QByteArray	maturity;
    QByteArray	issueDate;
    QByteArray	nextOptionDate;
    QByteArray	nextOptionType;
    bool		nextOptionPartial;
    QByteArray	notes;
};

inline
void Contract::CloneComboLegs(QList<ComboLeg *> &dst, const QList<ComboLeg*> &src)
{
    foreach (ComboLeg* leg, src) {
        if (!leg)
            continue;
        dst.append(leg);
    }
}

#endif // IBCONTRACT_H




























