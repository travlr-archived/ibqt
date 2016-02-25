#include "testibqt.h"
#include <QtDebug>

TestIbQt::TestIbQt(QObject *parent) : QObject(parent)
{
    connect(&ib, SIGNAL(twsConnected()), this, SLOT(onTwsConnected()));
    connect(&ib, SIGNAL(contractDetails(int,ContractDetails)), this, SLOT(onContractDetails(int,ContractDetails)));
    connect(&ib, SIGNAL(contractDetailsEnd(int)), this, SLOT(onContractDetailsEnd(int)));
}

void TestIbQt::run()
{
    ib.connectToTWS();
    reqContractDetails();
}

void TestIbQt::onTwsConnected()
{
    qDebug() << "TWS Connected";
}

void TestIbQt::onContractDetails(int reqId, const ContractDetails &contractDetails)
{
    qDebug() << "Sym:" << contractDetails.summary.symbol << "Exp:" << contractDetails.summary.expiry;
}

void TestIbQt::onContractDetailsEnd(int reqId)
{
    qDebug() << "onContractDetailsEnd:" << reqId;
}

void TestIbQt::reqContractDetails()
{
    Contract c;
    c.symbol = QByteArray("MSFT");
    c.secType = QByteArray("OPT");
    c.exchange = QByteArray("SMART");
    c.currency = QByteArray("USD");

    ib.reqContractDetails(ib.getTickerId(), c);
}
