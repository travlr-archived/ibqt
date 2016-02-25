// Copyright (C) 2016  ProDataLab, Peter Alexander
// GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
// http://www.gnu.org/licenses/gpl-3.0-standalone.html

#ifndef IBQt_H
#define IBQt_H

//#include "ibclient_global.h"

#include "ibticktype.h"
#include "ibfadatatype.h"
#include "iborder.h"
#include "iborderstate.h"
#include "ibexecution.h"
#include "ibcontract.h"
#include <QObject>
#include <QTcpSocket>

//struct ContractDetails;
//struct Contract;
struct CommissionReport;
struct UnderComp;
//struct Order;
//struct OrderState;
//struct Execution;
struct TagValue;


#define TickerId long
#define OrderId  long


#define pDebug(errStr) qDebug() << "[DEBUG-" << __func__ << __LINE__ << "]" << (errStr)



class /*IBCLIENTSHARED_EXPORT*/ IBQt : public QObject
{
    Q_OBJECT
public:
    explicit IBQt(QObject *parent = 0);
    ~IBQt();

    // NOT API SPECIFIC
    TickerId getTickerId() { return m_tickerId++; }
    OrderId  getOrderId();
    void    setOrderId(long orderId) { m_orderId = orderId; }


    // CONNECTION AND SERVER
    void connectToTWS(const QByteArray &host="127.0.0.1", quint16 port=7497, int clientId=0);
    void disconnectTWS();
    void reqCurrentTime();
    int serverVersion();
    void setSeverLogLevel(int logLevel);
//    void twsConnectionTime();
//    void checkMessages();
//    void reqGlobalCancel();


    // MARKET DATA
    void reqMktData(TickerId tickerId, const Contract& contract, const QByteArray& genericTicks, bool snapshot, const QList<TagValue*>& mktDataOptions = QList<TagValue*>());
    void cancelMktData(TickerId tickerId);
    void calculateImpliedVolatility(TickerId tickerId, const Contract & contract, double optionPrice, double underPrice);
    void cancelCalculateImpliedVolatility(TickerId tickerId);
    void calculateOptionPrice(TickerId reqId, const Contract & contract, double volatility, double underPrice);
    void cancelCalculateOptionPrice(TickerId reqId);
    void reqMarketDataType(int marketDataType);


    // ORDERS
    void placeOrder(OrderId id, const Contract & contract, const Order & order);
    void cancelOrder(OrderId id);
    void reqOpenOrders();
    void reqAllOpenOrders();
//    void reqAutoOpenOrders(bool autoBind);
    void reqIds(int numIds);
    void excerciseOptions(TickerId id, const Contract & contract, int exerciseAction, int exerciseQuantity, const QByteArray & account, int override);
    //    void reqGlobalCancel();


    // ACCOUNT AND PORTFOLIO
    void reqAccountUpdates(bool subscribe, const QByteArray & acctCode);
//    void reqAccountSummary();
//    void cancelAccountSummary(int reqId);
    void reqPositions();
    void cancelPositions();


    // EXECUTIONS
//    void reqExecutions(int reqId, ExecutionFilter filter);


    // CONTRACT DETAILS
    void reqContractDetails(int reqId, const Contract & contract);


    // MARKET DEPTH
//    void reqMktDepth(TickerId id, const Contract & contract, int numRows, TagValueListSPtr mktDepthOptions);
//    void cancelMktDepth(TickerId id);


    // NEWS BULLETINS
//    void reqNewsBulletins(bool allMsgs);
//    void cancellNewsBulletins();


    // FINANCIAL ADVISORS
//    void reqManagedAccts();
//    void reqFA(faDataType faDataType);
//    void replaceFA(faDataType faDataType, const QByteArray & cxml);


    // HISTORICAL DATA
    void reqHistoricalData( TickerId tickerId, const Contract &contract, const QByteArray &endDateTime, const QByteArray &durationStr, const QByteArray & barSizeSetting, const QByteArray &whatToShow, int useRTH, int formatDate, const QList<TagValue*> & chartOptions);
    void cancelHistoricalData(TickerId tickerId);


    // MARKET SCANNERS
//    void reqScannerParameters();
//    void reqScannerSubscription(int tickerId, ScannerSubscription scannerSubscription, TagValueListSPtr ScannerSubscriptionOptions);
//    void cancelScannerSubscription(int tickerId);


    // REAL TIME BARS
    void reqRealTimeBars(const TickerId & tickerId, const Contract & contract, const int & barSize, const QByteArray & whatToShow, const bool & useRTH, const QList<TagValue*> & realTimeBarsOptions);
    void cancelRealTimeBars(TickerId tickerId);


    // FUNDAMENTAL DATA
//    void reqFundamentalData(TickerId reqId, const Contract & contract, const QByteArray & reportType);
//    void cancelFundamentalData(TickerId reqId);


    // DISPLAY GROUPS
//    void queryDisplayGroups(int reqId);
//    void subscribeToGroupEvents(int reqId, int groupId);
//    void updateDisplayGroup(int reqId, const QByteArray & contractInfo);
//    void unsubscribeFromGroupEvents(int reqId);





    QTcpSocket *getSocket() const;

signals:
    void twsConnected();
    void ibSocketError(const QString & errorString);
    void verifyMessageAPI( const QByteArray& apiData);                      // not documented
    void verifyCompleted( bool isSuccessful, const QByteArray& errorText);  // not documented


    // CONNECTION AND SERVER
    void winError( const QByteArray &str, int lastError);
    void error(const int id, const int errorCode, const QByteArray errorString);
    void connectionClosed();
    void currentTime(long time);


    // MARKET DATA
    void tickPrice(const TickerId & tickerId, const TickType & field, const double & price, const int & canAutoExecute);
    void tickSize(const TickerId & tickerId, const TickType & field, const int & size);
    void tickOptionComputation(const TickerId & tickerId, const TickType & tickType, const double & impliedVol, const double & delta,
        const double & optPrice, const double & pvDividend, const double & gamma, const double & vega, const double & theta, const double & undPrice);
    void tickGeneric(const TickerId & tickerId, const TickType & tickType, const double & value);
    void tickString(const TickerId & tickerId, const TickType & tickType, const QByteArray & value);
    void tickEFP(const TickerId & tickerId, const TickType & tickType, const double & basisPoints, const QByteArray & formattedBasisPoints,
        double totalDividends, int holdDays, const QByteArray& futureExpiry, double dividendImpact, double dividendsToExpiry);
    void tickSnapshotEnd( int reqId);
    void marketDataType( TickerId reqId, int marketDataType);


    // ORDERS
    void orderStatus( OrderId orderId, const QByteArray &status, int filled,
        int remaining, double avgFillPrice, int permId, int parentId,
        double lastFillPrice, int clientId, const QByteArray& whyHeld);
    void openOrder( OrderId orderId, const Contract&, const Order&, const OrderState&);
    void openOrderEnd();
    void nextValidId( OrderId orderId);
    void deltaNeutralValidation(int reqId, const UnderComp& underComp);


    // ACCOUNT AND PORTFOLIO
    void updateAccountValue(const QByteArray& key, const QByteArray& val,
        const QByteArray& currency, const QByteArray& accountName);
    void updatePortfolio( const Contract& contract, int position,
       double marketPrice, double marketValue, double averageCost,
       double unrealizedPNL, double realizedPNL, const QByteArray& accountName);
    void updateAccountTime(const QByteArray& timeStamp);
    void accountDownloadEnd(const QByteArray& accountName);
    void accountSummary( int reqId, const QByteArray& account, const QByteArray& tag, const QByteArray& value, const QByteArray& curency);
    void accountSummaryEnd( int reqId);
    void position( const QByteArray& account, const Contract& contract, int position, double avgCost);
    void positionEnd();


    // NEWS BULLETINS
    void updateNewsBulletin(int msgId, int msgType, const QByteArray& newsMessage, const QByteArray& originExch);


    // CONTRACT DETAILS
    void contractDetails( int reqId, const ContractDetails& contractDetails);
    void contractDetailsEnd( int reqId);
    void bondContractDetails( int reqId, const ContractDetails& contractDetails);


    // EXECUTIONS
    void execDetails( int reqId, const Contract& contract, const Execution& execution);
    void execDetailsEnd( int reqId);
    void commissionReport( const CommissionReport &commissionReport);


    // MARKET DEPTH
    void updateMktDepth(TickerId id, int position, int operation, int side,
       double price, int size);
    void updateMktDepthL2(TickerId id, int position, QByteArray marketMaker, int operation,
       int side, double price, int size);


    // FINANCIAL ADVISORS
    void managedAccounts( const QByteArray& accountsList);
    void receiveFA(FaDataType pFaDataType, const QByteArray& cxml);


    // HISTORICAL DATA
    void historicalData(TickerId reqId, const QByteArray& date, double open, double high,
        double low, double close, int volume, int barCount, double WAP, int hasGaps);


    // MARKET SCANNERS
    void scannerParameters(const QByteArray &xml);
    void scannerData(int reqId, int rank, const ContractDetails &contractDetails,
        const QByteArray &distance, const QByteArray &benchmark, const QByteArray &projection,
        const QByteArray &legsStr);
    void scannerDataEnd(int reqId);


    // REAL TIME BARS
    void realtimeBar(TickerId reqId, long time, double open, double high, double low, double close,
        long volume, double wap, int count);


    // FUNDAMENTAL DATA
    void fundamentalData(TickerId reqId, const QByteArray& data);


    // DISPLAY GROUPS
    void displayGroupList( int reqId, const QByteArray& groups);
    void displayGroupUpdated( int reqId, const QByteArray& contractInfo);



private slots:
    void onConnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onManagedAccounts( const QByteArray& accountsList);

private:
    QTcpSocket* m_socket;
    int         m_clientId;
    QByteArray  m_outBuffer;
    QByteArray  m_inBuffer;
    bool        m_connected;
    int         m_serverVersion;
    QByteArray  m_twsTime;
    int         m_begIdx;
    int         m_endIdx;
    int         m_lastEndIdx;
    bool        m_extraAuth;

    QByteArray  m_debugBuffer;

    TickerId    m_tickerId;
    OrderId     m_orderId;
    bool        m_lock;

    void        decodeField(int & value);
    void        decodeField(bool & value);
    void        decodeField(long & value);
    void        decodeField(double & value);
    void        decodeField(QByteArray & value);
    QByteArray  decodeField();

    void        decodeFieldMax(int & value);
    void        decodeFieldMax(long & value);
    void        decodeFieldMax(double & value);

    void        encodeField(const int & value);
    void        encodeField(const bool & value);
    void        encodeField(const long & value);
    void        encodeField(const double & value);
    void        encodeField(const QByteArray & buf);
    void        encodeFieldMax(int value);
    void        encodeFieldMax(double value);

    void        cleanInBuffer();

    bool        IsEmpty(const QByteArray & ba) { return ba.isEmpty(); }
    int         Compare(const QByteArray& a1, const QByteArray & a2) { return (a1==a2?0:1); }
    void        send();
    void        delay(int milliSecondsToWait);

};

#endif // IBQt_H
