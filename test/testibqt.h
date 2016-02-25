#ifndef TESTIBQT_H
#define TESTIBQT_H

#include "ibqt.h"
#include <QObject>

class TestIbQt : public QObject
{
    Q_OBJECT
public:
    explicit TestIbQt(QObject *parent = 0);

public slots:
    void run();

private slots:
    void onTwsConnected();
    void onManagedAccounts(const QByteArray & accountsList);
    void onContractDetails(int reqId, const ContractDetails & contractDetails);
    void onContractDetailsEnd(int reqId);

private:
    IBQt ib;

    void reqContractDetails();
};

#endif // TESTIBQT_H
