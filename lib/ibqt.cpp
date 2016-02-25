// Copyright (C) 2016  ProDataLab, Peter Alexander
// GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
// http://www.gnu.org/licenses/gpl-3.0-standalone.html

#include "ibqt.h"
#include "ibdefines.h"
#include "ibbardata.h"
#include "ibscandata.h"
#include "ibcommissionreport.h"
#include "ibsocketerrors.h"
#include "ibtagvalue.h"
//#include "helpers.h"

#include <QDebug>
#include <QByteArray>
#include <QList>
#include <QVariant>
#include <QTime>
#include <QCoreApplication>

#include <cfloat>
#include <cassert>


//static const qint64 BUFFER_SIZE_HIGH_MARK = 1 * 1024 * 1024; // 1 MB


IBQt::IBQt(QObject *parent)
    : QObject(parent)
    , m_socket(NULL)
    , m_clientId(0)
    , m_outBuffer(QByteArray())
    , m_inBuffer(QByteArray())
    , m_connected(false)
    , m_serverVersion(0)
    , m_twsTime(QByteArray())
    , m_begIdx(0)
    , m_endIdx(0)
    , m_extraAuth(0)
    , m_tickerId(1)
    , m_orderId(0)
    , m_lock(true)
{
    m_socket = new QTcpSocket(this);


//    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, var);
//    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, QVariant(1));


    connect(m_socket, SIGNAL(connected()),
            this, SLOT(onConnected()));
    connect(m_socket, SIGNAL(readyRead()),
            this, SLOT(onReadyRead()));
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onSocketError(QAbstractSocket::SocketError)));
    connect(this, SIGNAL(managedAccounts(QByteArray)), this, SLOT(onManagedAccounts(QByteArray)));
}

IBQt::~IBQt() {}



void IBQt::connectToTWS(const QByteArray &host, quint16 port, int clientId)
{
    m_clientId = clientId;
    m_socket->connectToHost(QString(host), port);
    encodeField(CLIENT_VERSION);
    send();
    while (m_lock) {
        delay(100);
    }
}


void IBQt::disconnectTWS()
{
    m_twsTime.clear();
    m_serverVersion = 0;
    m_connected = false;
    m_extraAuth = false;
    m_clientId = -1;
    m_outBuffer.clear();
    m_inBuffer.clear();

    m_socket->disconnectFromHost();
}

OrderId IBQt::getOrderId()
{
    OrderId ret = m_orderId;
    ++m_orderId;
    return ret;
}

void IBQt::send()
{
//    qDebug() << "[DEBUG-send] debugBuffer" << m_debugBuffer;
    m_debugBuffer.clear();
//    qDebug() << "[DEBUG-send] rawBuffer" << m_outBuffer;

    int sent = m_socket->write(m_outBuffer);

    if (sent == m_outBuffer.size())
        m_outBuffer.clear();
    else {
//qDebug() << "[WARNING !!!] NOT ALL DATA IN THE OUT BUFFER WAS SENT... PREPENDING TO NEXT MESSAGE";
        m_outBuffer.remove(0, sent + 1);
    }
}

void IBQt::delay(int milliSecondsToWait)
{
    QTime dieTime = QTime::currentTime().addMSecs( milliSecondsToWait );
    while( QTime::currentTime() < dieTime )
    {
        QCoreApplication::processEvents( QEventLoop::AllEvents, 100 );
    }
}

void IBQt::reqHistoricalData(long tickerId, const Contract &contract, const QByteArray &endDateTime, const QByteArray &durationStr, const QByteArray &barSizeSetting, const QByteArray &whatToShow, int useRTH, int formatDate, const QList<TagValue*> &chartOptions)
{
    if (!m_connected) {
       emit error(tickerId, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    if (m_serverVersion < MIN_SERVER_VER_TRADING_CLASS) {
        if (!contract.tradingClass.isEmpty() || (contract.conId > 0)) {
            emit error(tickerId, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                  "  It does not support conId and tradingClass parameters in reqHistoricalData.");
            return;
        }
    }

    const int VERSION = 6;

    encodeField(REQ_HISTORICAL_DATA);
    encodeField(VERSION);
    encodeField(tickerId);

    // send contract fields
    if (m_serverVersion >= MIN_SERVER_VER_TRADING_CLASS) {
        encodeField(contract.conId);
    }
    encodeField(contract.symbol);
    encodeField(contract.secType);
    encodeField(contract.expiry);
    encodeField(contract.strike);
    encodeField(contract.right);
    encodeField(contract.multiplier);
    encodeField(contract.exchange);
    encodeField(contract.primaryExchange);
    encodeField(contract.currency);
    encodeField(contract.localSymbol);
    if (m_serverVersion >= MIN_SERVER_VER_TRADING_CLASS) {
        encodeField(contract.tradingClass);
    }
    encodeField(contract.includeExpired);
    encodeField(endDateTime);
    encodeField(barSizeSetting);
    encodeField(durationStr);
    encodeField(useRTH);
    encodeField(whatToShow);
    encodeField(formatDate);

    // send combo legs for BAG requests
    if (contract.secType == "BAG") {
        encodeField(contract.comboLegs.size());
        foreach(ComboLeg* leg, contract.comboLegs) {
            encodeField(leg->conId);
            encodeField(leg->ratio);
            encodeField(leg->action);
            encodeField(leg->exchange);
        }
    }

    // send chart options parameter
    if (m_serverVersion >= MIN_SERVER_VER_LINKING) {
        QByteArray chartOptionsStr;
        foreach (TagValue* tv, chartOptions) {
            chartOptionsStr.append(tv->tag);
            chartOptionsStr.append("=");
            chartOptionsStr.append(tv->value);
            chartOptionsStr.append(";");
        }
        encodeField(chartOptionsStr);
    }
    send();
}

void IBQt::cancelHistoricalData(long tickerId)
{
    // not connected?
    if( !m_connected) {
        emit error(NO_VALID_ID, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    const int VERSION = 1;

    encodeField(CANCEL_HISTORICAL_DATA);
    encodeField(VERSION);
    encodeField(tickerId);

    send();
}


void IBQt::reqCurrentTime()
{
    // not connected?
    if( !m_connected) {
        emit error(NO_VALID_ID, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    const int version = 1;
    encodeField(REQ_CURRENT_TIME);
    encodeField(version);
    send();
}

int IBQt::serverVersion()
{
    return m_serverVersion;
}

void IBQt::setSeverLogLevel(int logLevel)
{
    // not connected?
    if( !m_connected) {
        emit error( NO_VALID_ID, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    const int VERSION = 1;

    // send the set server logging level message
    encodeField( SET_SERVER_LOGLEVEL);
    encodeField( VERSION);
    encodeField( logLevel);

    send();
}




void IBQt::reqMktData(TickerId tickerId, const Contract& contract,
                               const QByteArray& genericTicks, bool snapshot, const QList<TagValue*>& mktDataOptions)
{
    // not connected?
    if( !m_connected) {
        emit error( tickerId, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    if( m_serverVersion < MIN_SERVER_VER_UNDER_COMP) {
        if( contract.underComp) {
            emit error( tickerId, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support delta-neutral orders.");
            return;
        }
    }

    if (m_serverVersion < MIN_SERVER_VER_REQ_MKT_DATA_CONID) {
        if( contract.conId > 0) {
            emit error( tickerId, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support conId parameter.");
            return;
        }
    }

    if (m_serverVersion < MIN_SERVER_VER_TRADING_CLASS) {
        if( !contract.tradingClass.isEmpty()) {
            emit error( tickerId, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support tradingClass parameter in reqMktData.");
            return;
        }
    }

    const int VERSION = 11;

    // send req mkt data msg
    encodeField( REQ_MKT_DATA);
    encodeField( VERSION);
    encodeField( tickerId);

    // send contract fields
    if( m_serverVersion >= MIN_SERVER_VER_REQ_MKT_DATA_CONID) {
        encodeField( contract.conId);
    }
    encodeField( contract.symbol);
    encodeField( contract.secType);
    encodeField( contract.expiry);
    encodeField( contract.strike);
    encodeField( contract.right);
    encodeField( contract.multiplier); // srv v15 and above

    encodeField( contract.exchange);
    encodeField( contract.primaryExchange); // srv v14 and above
    encodeField( contract.currency);

    encodeField( contract.localSymbol); // srv v2 and above

    if( m_serverVersion >= MIN_SERVER_VER_TRADING_CLASS) {
        encodeField( contract.tradingClass);
    }

    // Send combo legs for BAG requests (srv v8 and above)
    if (contract.secType == "BAG") {
        const QList<ComboLeg*> comboLegs = contract.comboLegs;
        encodeField(comboLegs.size());
        foreach (ComboLeg* cl, comboLegs) {
            encodeField(cl->conId);
            encodeField(cl->ratio);
            encodeField(cl->action);
            encodeField(cl->exchange);
        }
    }

    if( m_serverVersion >= MIN_SERVER_VER_UNDER_COMP) {
        if( contract.underComp) {
            encodeField( true);
            encodeField( contract.underComp->conId);
            encodeField( contract.underComp->delta);
            encodeField( contract.underComp->price);
        }
        else {
            encodeField( false);
        }
    }

    encodeField( genericTicks); // srv v31 and above
    encodeField( snapshot); // srv v35 and above

    // send mktDataOptions parameter
    if( m_serverVersion >= MIN_SERVER_VER_LINKING) {
        QByteArray mktDataOptionsStr;
        foreach (TagValue* tv, mktDataOptions) {
            mktDataOptionsStr.append(tv->tag);
            mktDataOptionsStr.append("=");
            mktDataOptionsStr.append(tv->value);
            mktDataOptionsStr.append(";");
        }
        encodeField( mktDataOptionsStr);
    }

    send();
}

void IBQt::reqRealTimeBars(const long &tickerId, const Contract &contract, const int &barSize, const QByteArray &whatToShow, const bool &useRTH, const QList<TagValue *> &realTimeBarsOptions)
{
    if (!m_connected) {
      emit error(tickerId, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    if (m_serverVersion < MIN_SERVER_VER_TRADING_CLASS) {
        if (!contract.tradingClass.isEmpty() || contract.conId > 0) {
          emit error(tickerId, UPDATE_TWS.code(), UPDATE_TWS.msg()
                  + "  It does not support conId and tradingClass params in reqRealTimeBars.");
            return;
        }
    }

    const int VERSION = 3;


    encodeField(REQ_REAL_TIME_BARS);
    encodeField(VERSION);
    encodeField(tickerId);

    // send contract fields
    if( m_serverVersion >= MIN_SERVER_VER_TRADING_CLASS) {
        encodeField(contract.conId);
    }
    encodeField(contract.symbol);
    encodeField(contract.secType);
    encodeField(contract.expiry);
    encodeField(contract.strike);
    encodeField(contract.right);
    encodeField(contract.multiplier);
    encodeField(contract.exchange);
    encodeField(contract.primaryExchange);
    encodeField(contract.currency);
    encodeField(contract.localSymbol);
    if( m_serverVersion >= MIN_SERVER_VER_TRADING_CLASS) {
        encodeField(contract.tradingClass);
    }
    encodeField(barSize);
    encodeField(whatToShow);
    encodeField(useRTH);

    // send realTimeBarsOptions parameter
    if( m_serverVersion >= MIN_SERVER_VER_LINKING) {
        QByteArray realTimeBarsOptionsStr;
        if( realTimeBarsOptions.size() > 0) {
            for( int i = 0; i < realTimeBarsOptions.size(); ++i) {
                const TagValue* tagValue = realTimeBarsOptions.at(i);
                realTimeBarsOptionsStr += tagValue->tag;
                realTimeBarsOptionsStr += "=";
                realTimeBarsOptionsStr += tagValue->value;
                realTimeBarsOptionsStr += ";";
            }
        }
        encodeField(realTimeBarsOptionsStr);
    }

    send();
}

void IBQt::cancelRealTimeBars(long tickerId)
{
    // not connected?
    if( !m_connected) {
      emit error(NO_VALID_ID, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    const int VERSION = 1;

    encodeField(CANCEL_REAL_TIME_BARS);
    encodeField(VERSION);
    encodeField(tickerId);

    send();
}

void IBQt::placeOrder(long id, const Contract &contract, const Order &order)
{
//qDebug() << "[DEBUG-IBClient::placeOrder]";

    // not connected?
    if( !m_connected) {
      emit error(id, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    // Not needed anymore validation
    //if( m_serverVersion < MIN_SERVER_VER_SCALE_ORDERS) {
    //	if( order.scaleNumComponents != UNSET_INTEGER ||
    //		order.scaleComponentSize != UNSET_INTEGER ||
    //		order.scalePriceIncrement != UNSET_DOUBLE) {
    //emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
    //			"  It does not support Scale orders.");
    //		return;
    //	}
    //}
    //
    //if( m_serverVersion < MIN_SERVER_VER_SSHORT_COMBO_LEGS) {
    //	if( contract.comboLegs && !contract.comboLegs->empty()) {
    //		typedef Contract::ComboLegList ComboLegList;
    //		const ComboLegList& comboLegs = *contract.comboLegs;
    //		ComboLegList::const_iterator iter = comboLegs.begin();
    //		const ComboLegList::const_iterator iterEnd = comboLegs.end();
    //		for( ; iter != iterEnd; ++iter) {
    //			const ComboLeg* comboLeg = *iter;
    //			assert( comboLeg);
    //			if( comboLeg->shortSaleSlot != 0 ||
    //				!comboLeg->designatedLocation.IsEmpty()) {
    //		emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
    //					"  It does not support SSHORT flag for combo legs.");
    //				return;
    //			}
    //		}
    //	}
    //}
    //
    //if( m_serverVersion < MIN_SERVER_VER_WHAT_IF_ORDERS) {
    //	if( order.whatIf) {
    //emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
    //			"  It does not support what-if orders.");
    //		return;
    //	}
    //}

    if( m_serverVersion < MIN_SERVER_VER_UNDER_COMP) {
        if( contract.underComp) {
          emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support delta-neutral orders.");
            return;
        }
    }

    if( m_serverVersion < MIN_SERVER_VER_SCALE_ORDERS2) {
        if( order.scaleSubsLevelSize != UNSET_INTEGER) {
          emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support Subsequent Level Size for Scale orders.");
            return;
        }
    }

    if( m_serverVersion < MIN_SERVER_VER_ALGO_ORDERS) {

        if( !IsEmpty(order.algoStrategy)) {
          emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support algo orders.");
            return;
        }
    }

    if( m_serverVersion < MIN_SERVER_VER_NOT_HELD) {
        if (order.notHeld) {
          emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support notHeld parameter.");
            return;
        }
    }

    if (m_serverVersion < MIN_SERVER_VER_SEC_ID_TYPE) {
        if( !IsEmpty(contract.secIdType) || !IsEmpty(contract.secId)) {
          emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support secIdType and secId parameters.");
            return;
        }
    }

    if (m_serverVersion < MIN_SERVER_VER_PLACE_ORDER_CONID) {
        if( contract.conId > 0) {
          emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support conId parameter.");
            return;
        }
    }

    if (m_serverVersion < MIN_SERVER_VER_SSHORTX) {
        if( order.exemptCode != -1) {
          emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support exemptCode parameter.");
            return;
        }
    }

    if (m_serverVersion < MIN_SERVER_VER_SSHORTX) {
        for( int i = 0; i < contract.comboLegs.size(); ++i) {
            const ComboLeg* comboLeg = contract.comboLegs.at(i);
            assert( comboLeg);
            if( comboLeg->exemptCode != -1 ){
              emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                    "  It does not support exemptCode parameter.");
                return;
            }
        }
    }

    if( m_serverVersion < MIN_SERVER_VER_HEDGE_ORDERS) {
        if( !IsEmpty(order.hedgeType)) {
          emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support hedge orders.");
            return;
        }
    }

    if( m_serverVersion < MIN_SERVER_VER_OPT_OUT_SMART_ROUTING) {
        if (order.optOutSmartRouting) {
          emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support optOutSmartRouting parameter.");
            return;
        }
    }

    if (m_serverVersion < MIN_SERVER_VER_DELTA_NEUTRAL_CONID) {
        if (order.deltaNeutralConId > 0
                || !IsEmpty(order.deltaNeutralSettlingFirm)
                || !IsEmpty(order.deltaNeutralClearingAccount)
                || !IsEmpty(order.deltaNeutralClearingIntent)
                ) {
          emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support deltaNeutral parameters: ConId, SettlingFirm, ClearingAccount, ClearingIntent.");
            return;
        }
    }

    if (m_serverVersion < MIN_SERVER_VER_DELTA_NEUTRAL_OPEN_CLOSE) {
        if (!IsEmpty(order.deltaNeutralOpenClose)
                || order.deltaNeutralShortSale
                || order.deltaNeutralShortSaleSlot > 0
                || !IsEmpty(order.deltaNeutralDesignatedLocation)
                ) {
          emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support deltaNeutral parameters: OpenClose, ShortSale, ShortSaleSlot, DesignatedLocation.");
            return;
        }
    }

    if (m_serverVersion < MIN_SERVER_VER_SCALE_ORDERS3) {
        if (order.scalePriceIncrement > 0 && order.scalePriceIncrement != UNSET_DOUBLE) {
            if (order.scalePriceAdjustValue != UNSET_DOUBLE
                || order.scalePriceAdjustInterval != UNSET_INTEGER
                || order.scaleProfitOffset != UNSET_DOUBLE
                || order.scaleAutoReset
                || order.scaleInitPosition != UNSET_INTEGER
                || order.scaleInitFillQty != UNSET_INTEGER
                || order.scaleRandomPercent) {
              emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                        "  It does not support Scale order parameters: PriceAdjustValue, PriceAdjustInterval, " +
                        "ProfitOffset, AutoReset, InitPosition, InitFillQty and RandomPercent");
                return;
            }
        }
    }

    if (m_serverVersion < MIN_SERVER_VER_ORDER_COMBO_LEGS_PRICE && Compare(contract.secType, "BAG") == 0) {
        for( int i = 0; i < order.orderComboLegs.size(); ++i) {
            const OrderComboLeg* orderComboLeg = order.orderComboLegs.at(i);
            assert( orderComboLeg);
            if( orderComboLeg->price != UNSET_DOUBLE) {
              emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                    "  It does not support per-leg prices for order combo legs.");
                return;
            }
        }
    }

    if (m_serverVersion < MIN_SERVER_VER_TRAILING_PERCENT) {
        if (order.trailingPercent != UNSET_DOUBLE) {
          emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                    "  It does not support trailing percent parameter");
            return;
        }
    }

    if (m_serverVersion < MIN_SERVER_VER_TRADING_CLASS) {
        if( !IsEmpty(contract.tradingClass)) {
          emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support tradingClass parameter in placeOrder.");
            return;
        }
    }

    if (m_serverVersion < MIN_SERVER_VER_SCALE_TABLE) {
        if( !IsEmpty(order.scaleTable) || !IsEmpty(order.activeStartTime) || !IsEmpty(order.activeStopTime)) {
          emit error(id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                    "  It does not support scaleTable, activeStartTime and activeStopTime parameters");
            return;
        }
    }

    int VERSION = (m_serverVersion < MIN_SERVER_VER_NOT_HELD) ? 27 : 42;

    // send place order msg
    encodeField(PLACE_ORDER);
    encodeField(VERSION);
    encodeField(id);



    // send contract fields
    if( m_serverVersion >= MIN_SERVER_VER_PLACE_ORDER_CONID) {
        encodeField(contract.conId);
    }
    encodeField(contract.symbol);
    encodeField(contract.secType);
    encodeField(contract.expiry);
    encodeField(contract.strike);
    encodeField(contract.right);
    encodeField(contract.multiplier); // srv v15 and above
    encodeField(contract.exchange);
    encodeField(contract.primaryExchange); // srv v14 and above
    encodeField(contract.currency);
    encodeField(contract.localSymbol); // srv v2 and above
    if( m_serverVersion >= MIN_SERVER_VER_TRADING_CLASS) {
        encodeField(contract.tradingClass);
    }

    if( m_serverVersion >= MIN_SERVER_VER_SEC_ID_TYPE){
        encodeField(contract.secIdType);
        encodeField(contract.secId);
    }

    // send main order fields
    encodeField(order.action);
    encodeField(order.totalQuantity);
    encodeField(order.orderType);
    if( m_serverVersion < MIN_SERVER_VER_ORDER_COMBO_LEGS_PRICE) {
        encodeField(order.lmtPrice == UNSET_DOUBLE ? 0 : order.lmtPrice);
    }
    else {
        encodeFieldMax( order.lmtPrice);
    }
    if( m_serverVersion < MIN_SERVER_VER_TRAILING_PERCENT) {
        encodeField(order.auxPrice == UNSET_DOUBLE ? 0 : order.auxPrice);
    }
    else {
        encodeFieldMax( order.auxPrice);
    }

//    qDebug() << "[[order.auxPrice]]" << order.auxPrice;


    // send extended order fields
    encodeField(order.tif);
//    qDebug() << "[[order.tif]]" << order.tif;
    encodeField(order.ocaGroup);
//    qDebug() << "[[order.ocaGroup]]" << order.ocaGroup;
    encodeField(order.account);
//    qDebug() << "[[order.acconut]]" << order.account;
    encodeField(order.openClose);
//    qDebug() << "[[order.openClose]]" << order.openClose;
    encodeField(order.origin);
//    qDebug() << "[[order.origin]]" << order.origin;
    encodeField(order.orderRef);
//    qDebug() << "[[order.orderRef]]" << order.orderRef;
    encodeField(order.transmit);
//    qDebug() << "[[order.transmit]]" << order.transmit;
    encodeField(order.parentId); // srv v4 and above
//    qDebug() << "[[order.parentId]]" << order.parentId;

    encodeField(order.blockOrder); // srv v5 and above
    encodeField(order.sweepToFill); // srv v5 and above
    encodeField(order.displaySize); // srv v5 and above
    encodeField(order.triggerMethod); // srv v5 and above

    //if( m_serverVersion < 38) {
    // will never happen
    //	ENCODE_FIELD(/* order.ignoreRth */ false);
    //}
    //else {
        encodeField(order.outsideRth); // srv v5 and above
    //}

    encodeField(order.hidden); // srv v7 and above

    // Send combo legs for BAG requests (srv v8 and above)
    if( Compare(contract.secType, "BAG") == 0)
    {
        const int comboLegsCount = contract.comboLegs.size();
        encodeField(comboLegsCount);
        if( comboLegsCount > 0) {
            for( int i = 0; i < comboLegsCount; ++i) {
                const ComboLeg* comboLeg = contract.comboLegs.at(i);
                assert( comboLeg);
                encodeField(comboLeg->conId);
                encodeField(comboLeg->ratio);
                encodeField(comboLeg->action);
                encodeField(comboLeg->exchange);
                encodeField(comboLeg->openClose);

                encodeField(comboLeg->shortSaleSlot); // srv v35 and above
                encodeField(comboLeg->designatedLocation); // srv v35 and above
                if (m_serverVersion >= MIN_SERVER_VER_SSHORTX_OLD) {
                    encodeField(comboLeg->exemptCode);
                }
            }
        }
    }

    // Send order combo legs for BAG requests
    if( m_serverVersion >= MIN_SERVER_VER_ORDER_COMBO_LEGS_PRICE && Compare(contract.secType, "BAG") == 0)
    {
        const int orderComboLegsCount = order.orderComboLegs.size();
        encodeField(orderComboLegsCount);
        if( orderComboLegsCount > 0) {
            for( int i = 0; i < orderComboLegsCount; ++i) {
                const OrderComboLeg* orderComboLeg = order.orderComboLegs.at(i);
                assert( orderComboLeg);
                encodeFieldMax( orderComboLeg->price);
            }
        }
    }

    if( m_serverVersion >= MIN_SERVER_VER_SMART_COMBO_ROUTING_PARAMS && Compare(contract.secType, "BAG") == 0) {
        const int smartComboRoutingParamsCount = order.smartComboRoutingParams.size();
        encodeField(smartComboRoutingParamsCount);
        if( smartComboRoutingParamsCount > 0) {
            for( int i = 0; i < smartComboRoutingParamsCount; ++i) {
                const TagValue* tagValue = order.smartComboRoutingParams.at(i);
                encodeField(tagValue->tag);
                encodeField(tagValue->value);
            }
        }
    }

    /////////////////////////////////////////////////////////////////////////////
    // Send the shares allocation.
    //
    // This specifies the number of order shares allocated to each Financial
    // Advisor managed account. The format of the allocation string is as
    // follows:
    //			<account_code1>/<number_shares1>,<account_code2>/<number_shares2>,...N
    // E.g.
    //		To allocate 20 shares of a 100 share order to account 'U101' and the
    //      residual 80 to account 'U203' enter the following share allocation string:
    //          U101/20,U203/80
    /////////////////////////////////////////////////////////////////////////////
    {
        // send deprecated sharesAllocation field
        QByteArray ba = "";
        encodeField(ba); // srv v9 and above
    }

    encodeField(order.discretionaryAmt); // srv v10 and above
    encodeField(order.goodAfterTime); // srv v11 and above
    encodeField(order.goodTillDate); // srv v12 and above

    encodeField(order.faGroup); // srv v13 and above
    encodeField(order.faMethod); // srv v13 and above
    encodeField(order.faPercentage); // srv v13 and above
    encodeField(order.faProfile); // srv v13 and above

    // institutional short saleslot data (srv v18 and above)
    encodeField(order.shortSaleSlot);      // 0 for retail, 1 or 2 for institutions
    encodeField(order.designatedLocation); // populate only when shortSaleSlot = 2.
    if (m_serverVersion >= MIN_SERVER_VER_SSHORTX_OLD) {
        encodeField(order.exemptCode);
    }

    // not needed anymore
    //bool isVolOrder = (order.orderType.CompareNoCase("VOL") == 0);

    // srv v19 and above fields
    encodeField(order.ocaType);
    //if( m_serverVersion < 38) {
    // will never happen
    //	send( /* order.rthOnly */ false);
    //}
    encodeField(order.rule80A);
    encodeField(order.settlingFirm);
    encodeField(order.allOrNone);
    encodeFieldMax( order.minQty);
    encodeFieldMax( order.percentOffset);
    encodeField(order.eTradeOnly);
    encodeField(order.firmQuoteOnly);
    encodeFieldMax( order.nbboPriceCap);
    encodeField(order.auctionStrategy); // AUCTION_MATCH, AUCTION_IMPROVEMENT, AUCTION_TRANSPARENT
    encodeFieldMax( order.startingPrice);
    encodeFieldMax( order.stockRefPrice);
    encodeFieldMax( order.delta);
    // Volatility orders had specific watermark price attribs in server version 26
    //double lower = (m_serverVersion == 26 && isVolOrder) ? DBL_MAX : order.stockRangeLower;
    //double upper = (m_serverVersion == 26 && isVolOrder) ? DBL_MAX : order.stockRangeUpper;
    encodeFieldMax( order.stockRangeLower);
    encodeFieldMax( order.stockRangeUpper);

    encodeField(order.overridePercentageConstraints); // srv v22 and above

    // Volatility orders (srv v26 and above)
    encodeFieldMax( order.volatility);
    encodeFieldMax( order.volatilityType);
    // will never happen
    //if( m_serverVersion < 28) {
    //	send( order.deltaNeutralOrderType.CompareNoCase("MKT") == 0);
    //}
    //else {
        encodeField(order.deltaNeutralOrderType); // srv v28 and above
        encodeFieldMax( order.deltaNeutralAuxPrice); // srv v28 and above

        if (m_serverVersion >= MIN_SERVER_VER_DELTA_NEUTRAL_CONID && !IsEmpty(order.deltaNeutralOrderType)){
            encodeField(order.deltaNeutralConId);
            encodeField(order.deltaNeutralSettlingFirm);
            encodeField(order.deltaNeutralClearingAccount);
            encodeField(order.deltaNeutralClearingIntent);
        }

        if (m_serverVersion >= MIN_SERVER_VER_DELTA_NEUTRAL_OPEN_CLOSE && !IsEmpty(order.deltaNeutralOrderType)){
            encodeField(order.deltaNeutralOpenClose);
            encodeField(order.deltaNeutralShortSale);
            encodeField(order.deltaNeutralShortSaleSlot);
            encodeField(order.deltaNeutralDesignatedLocation);
        }

    //}
    encodeField(order.continuousUpdate);
    //if( m_serverVersion == 26) {
    //	// Volatility orders had specific watermark price attribs in server version 26
    //	double lower = (isVolOrder ? order.stockRangeLower : DBL_MAX);
    //	double upper = (isVolOrder ? order.stockRangeUpper : DBL_MAX);
    //	encodeFieldMax( lower);
    //	encodeFieldMax( upper);
    //}
    encodeFieldMax( order.referencePriceType);

    encodeFieldMax( order.trailStopPrice); // srv v30 and above

    if( m_serverVersion >= MIN_SERVER_VER_TRAILING_PERCENT) {
        encodeFieldMax( order.trailingPercent);
    }

    // SCALE orders
    if( m_serverVersion >= MIN_SERVER_VER_SCALE_ORDERS2) {
        encodeFieldMax( order.scaleInitLevelSize);
        encodeFieldMax( order.scaleSubsLevelSize);
    }
    else {
        // srv v35 and above)
        encodeField(""); // for not supported scaleNumComponents
        encodeFieldMax( order.scaleInitLevelSize); // for scaleComponentSize
    }

    encodeFieldMax( order.scalePriceIncrement);

    if( m_serverVersion >= MIN_SERVER_VER_SCALE_ORDERS3
        && order.scalePriceIncrement > 0.0 && order.scalePriceIncrement != UNSET_DOUBLE) {
        encodeFieldMax( order.scalePriceAdjustValue);
        encodeFieldMax( order.scalePriceAdjustInterval);
        encodeFieldMax( order.scaleProfitOffset);
        encodeField(order.scaleAutoReset);
        encodeFieldMax( order.scaleInitPosition);
        encodeFieldMax( order.scaleInitFillQty);
        encodeField(order.scaleRandomPercent);
    }

    if( m_serverVersion >= MIN_SERVER_VER_SCALE_TABLE) {
        encodeField(order.scaleTable);
        encodeField(order.activeStartTime);
        encodeField(order.activeStopTime);
    }

    // HEDGE orders
    if( m_serverVersion >= MIN_SERVER_VER_HEDGE_ORDERS) {
        encodeField(order.hedgeType);
        if ( !IsEmpty(order.hedgeType)) {
            encodeField(order.hedgeParam);
        }
    }

    if( m_serverVersion >= MIN_SERVER_VER_OPT_OUT_SMART_ROUTING){
        encodeField(order.optOutSmartRouting);
    }

    if( m_serverVersion >= MIN_SERVER_VER_PTA_ORDERS) {
        encodeField(order.clearingAccount);
        encodeField(order.clearingIntent);
    }

    if( m_serverVersion >= MIN_SERVER_VER_NOT_HELD){
        encodeField(order.notHeld);
    }

    if( m_serverVersion >= MIN_SERVER_VER_UNDER_COMP) {
        if( contract.underComp) {
            const UnderComp& underComp = *contract.underComp;
            encodeField(true);
            encodeField(underComp.conId);
            encodeField(underComp.delta);
            encodeField(underComp.price);
        }
        else {
            encodeField(false);
        }
    }

    if( m_serverVersion >= MIN_SERVER_VER_ALGO_ORDERS) {
        encodeField(order.algoStrategy);

        if( !IsEmpty(order.algoStrategy)) {
            const int algoParamsCount = order.algoParams.size();
            encodeField(algoParamsCount);
            if( algoParamsCount > 0) {
                for( int i = 0; i < algoParamsCount; ++i) {
                    const TagValue* tagValue = order.algoParams.at(i);
                    encodeField(tagValue->tag);
                    encodeField(tagValue->value);
                }
            }
        }
    }

    encodeField(order.whatIf); // srv v36 and above

    // send miscOptions parameter
    if( m_serverVersion >= MIN_SERVER_VER_LINKING) {
        QByteArray miscOptionsStr("");
        const int orderMiscOptionsCount = order.orderMiscOptions.size();
        if( orderMiscOptionsCount > 0) {
            for( int i = 0; i < orderMiscOptionsCount; ++i) {
                const TagValue* tagValue = order.orderMiscOptions.at(i);
                miscOptionsStr += tagValue->tag;
                miscOptionsStr += "=";
                miscOptionsStr += tagValue->value;
                miscOptionsStr += ";";
            }
        }
        encodeField(miscOptionsStr);
    }

    send();
}

void IBQt::cancelOrder(long id)
{
    // not connected?
    if( !m_connected) {
        error( NO_VALID_ID, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    const int VERSION = 1;

    encodeField( CANCEL_ORDER);
    encodeField( VERSION);
    encodeField( id);

    send();
}

void IBQt::reqAccountUpdates(bool subscribe, const QByteArray &acctCode)
{
    // not connected?
    if( !m_connected) {
        error( NO_VALID_ID, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }


    const int VERSION = 2;

    // send req acct msg
    encodeField( REQ_ACCT_DATA);
    encodeField( VERSION);
    encodeField( subscribe);  // TRUE = subscribe, FALSE = unsubscribe.

    // Send the account code. This will only be used for FA clients
    encodeField( acctCode); // srv v9 and above

    send();
}

void IBQt::reqPositions()
{
    // not connected?
    if( !m_connected) {
        error( NO_VALID_ID, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    if (m_serverVersion < MIN_SERVER_VER_POSITIONS) {
        emit error( NO_VALID_ID, UPDATE_TWS.code(), UPDATE_TWS.msg() +
            "  It does not support positions request.");
        return;
    }

    const int VERSION = 1;

    encodeField(REQ_POSITIONS);
    encodeField(VERSION);

    send();
}

void IBQt::cancelPositions()
{
    // not connected?
    if( !m_connected) {
        error( NO_VALID_ID, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    if (m_serverVersion < MIN_SERVER_VER_POSITIONS) {
        emit error( NO_VALID_ID, UPDATE_TWS.code(), UPDATE_TWS.msg() +
            "  It does not support positions cancellation.");
        return;
    }

    const int VERSION = 1;

    encodeField(CANCEL_POSITIONS);
    encodeField(VERSION);

    send();
}


void IBQt::reqOpenOrders()
{
    // not connected?
    if( !m_connected) {
        emit error( NO_VALID_ID, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    const int VERSION = 1;

    // send req open orders msg
    encodeField( REQ_OPEN_ORDERS);
    encodeField( VERSION);

    send();
}

void IBQt::reqAllOpenOrders()
{
    // not connected?
    if( !m_connected) {
        emit error( NO_VALID_ID, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    const int VERSION = 1;

    // send req open orders msg
    encodeField( REQ_ALL_OPEN_ORDERS);
    encodeField( VERSION);

    send();
}

void IBQt::reqContractDetails(int reqId, const Contract &contract)
{
    // not connected?
    if( !m_connected) {
        emit error( NO_VALID_ID, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    // Not needed anymore validation
    // This feature is only available for versions of TWS >=4
    //if( m_serverVersion < 4) {
    //	emit error( NO_VALID_ID, UPDATE_TWS.code(), UPDATE_TWS.msg());
    //	return;
    //}
    if (m_serverVersion < MIN_SERVER_VER_SEC_ID_TYPE) {
        if( !IsEmpty(contract.secIdType) || !IsEmpty(contract.secId)) {
            emit error( reqId, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support secIdType and secId parameters.");
            return;
        }
    }
    if (m_serverVersion < MIN_SERVER_VER_TRADING_CLASS) {
        if( !IsEmpty(contract.tradingClass)) {
            emit error( reqId, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support tradingClass parameter in reqContractDetails.");
            return;
        }
    }

    const int VERSION = 7;

    // send req mkt data msg
    encodeField( REQ_CONTRACT_DATA);
    encodeField( VERSION);

    if( m_serverVersion >= MIN_SERVER_VER_CONTRACT_DATA_CHAIN) {
        encodeField( reqId);
    }

    // send contract fields
    encodeField( contract.conId); // srv v37 and above
    encodeField( contract.symbol);
    encodeField( contract.secType);
    encodeField( contract.expiry);
    encodeField( contract.strike);
    encodeField( contract.right);
    encodeField( contract.multiplier); // srv v15 and above
    encodeField( contract.exchange);
    encodeField( contract.currency);
    encodeField( contract.localSymbol);
    if( m_serverVersion >= MIN_SERVER_VER_TRADING_CLASS) {
        encodeField( contract.tradingClass);
    }
    encodeField( contract.includeExpired); // srv v31 and above

    if( m_serverVersion >= MIN_SERVER_VER_SEC_ID_TYPE){
        encodeField( contract.secIdType);
        encodeField( contract.secId);
    }

    send();
}

void IBQt::reqIds(int numIds)
{
//qDebug() << "[DEBUG-reqIds]";

    // not connected?
    if( !m_connected) {
      emit error(numIds, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    const int VERSION = 1;

    // send req open orders msg
    encodeField(REQ_IDS);
    encodeField(VERSION);
    encodeField(numIds);

    send();
}

void IBQt::excerciseOptions(long id, const Contract &contract, int exerciseAction, int exerciseQuantity, const QByteArray &account, int override)
{
    // not connected?
    if( !m_connected) {
        emit error( id, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    if (m_serverVersion < MIN_SERVER_VER_TRADING_CLASS) {
        if( !IsEmpty(contract.tradingClass)) {
            emit error( id, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support conId, multiplier and tradingClass parameters in exerciseOptions.");
            return;
        }
    }

    const int VERSION = 2;

    encodeField( EXERCISE_OPTIONS);
    encodeField( VERSION);
    encodeField( id);

    // send contract fields
    if( m_serverVersion >= MIN_SERVER_VER_TRADING_CLASS) {
        encodeField( contract.conId);
    }
    encodeField( contract.symbol);
    encodeField( contract.secType);
    encodeField( contract.expiry);
    encodeField( contract.strike);
    encodeField( contract.right);
    encodeField( contract.multiplier);
    encodeField( contract.exchange);
    encodeField( contract.currency);
    encodeField( contract.localSymbol);
    if( m_serverVersion >= MIN_SERVER_VER_TRADING_CLASS) {
        encodeField( contract.tradingClass);
    }
    encodeField( exerciseAction);
    encodeField( exerciseQuantity);
    encodeField( account);
    encodeField( override);

    send();
}

void IBQt::cancelMktData(long tickerId)
{
    // not connected?
    if( !m_connected) {
        emit error( tickerId, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    const int VERSION = 2;

    // send cancel mkt data msg
    encodeField( CANCEL_MKT_DATA);
    encodeField( VERSION);
    encodeField( tickerId);

    send();
}



void IBQt::calculateImpliedVolatility(long tickerId, const Contract &contract, double optionPrice, double underPrice)
{
    // not connected?
    if( !m_connected) {
        emit error( tickerId, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    const int VERSION = 2;

    encodeField(REQ_CALC_IMPLIED_VOLAT);
    encodeField(VERSION);
    encodeField(tickerId);
    encodeField(contract.conId);
    encodeField(contract.symbol);
    encodeField(contract.expiry);
    encodeField(contract.strike);
    encodeField(contract.right);
    encodeField(contract.multiplier);
    encodeField(contract.exchange);
    encodeField(contract.primaryExchange);
    encodeField(contract.currency);
    encodeField(contract.localSymbol);
    if (m_serverVersion >= MIN_SERVER_VER_TRADING_CLASS)
        encodeField(contract.tradingClass);
    encodeField(optionPrice);
    encodeField(underPrice);

    send();
}

void IBQt::cancelCalculateImpliedVolatility(long tickerId)
{
    // not connected?
    if( !m_connected) {
        emit error( tickerId, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    // supported in this server version?
    if(m_serverVersion < MIN_SERVER_VER_CANCEL_CALC_IMPLIED_VOLAT) {
        emit error( tickerId, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                    "  It does not support calculate implied volatility cancellation.");
        return;
    }

    const int VERSION = 1;

    encodeField(CANCEL_CALC_IMPLIED_VOLAT);
    encodeField(VERSION);
    encodeField(tickerId);

    send();
}

void IBQt::calculateOptionPrice(long reqId, const Contract &contract, double volatility, double underPrice)
{
    // not connected?
    if( !m_connected) {
        emit error( reqId, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    if (m_serverVersion < MIN_SERVER_VER_REQ_CALC_OPTION_PRICE) {
        emit error( reqId, UPDATE_TWS.code(), UPDATE_TWS.msg() +
            "  It does not support calculate option price requests.");
        return;

    }
    if (m_serverVersion < MIN_SERVER_VER_TRADING_CLASS) {
        if( !IsEmpty(contract.tradingClass)) {
            emit error( reqId, UPDATE_TWS.code(), UPDATE_TWS.msg() +
                "  It does not support tradingClass parameter in calculateOptionPrice.");
            return;
        }
    }

    const int VERSION = 2;

    encodeField( REQ_CALC_OPTION_PRICE);
    encodeField( VERSION);
    encodeField( reqId);

    // send contract fields
    encodeField( contract.conId);
    encodeField( contract.symbol);
    encodeField( contract.secType);
    encodeField( contract.expiry);
    encodeField( contract.strike);
    encodeField( contract.right);
    encodeField( contract.multiplier);
    encodeField( contract.exchange);
    encodeField( contract.primaryExchange);
    encodeField( contract.currency);
    encodeField( contract.localSymbol);
    if( m_serverVersion >= MIN_SERVER_VER_TRADING_CLASS) {
        encodeField( contract.tradingClass);
    }

    encodeField( volatility);
    encodeField( underPrice);

    send();
}

void IBQt::cancelCalculateOptionPrice(long reqId)
{
    // not connected?
    if( !m_connected) {
        emit error( reqId, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    if (m_serverVersion < MIN_SERVER_VER_CANCEL_CALC_OPTION_PRICE) {
        emit error( reqId, UPDATE_TWS.code(), UPDATE_TWS.msg() +
            "  It does not support calculate option price cancellation.");
        return;

    }

    const int VERSION = 1;

    encodeField( CANCEL_CALC_OPTION_PRICE);
    encodeField( VERSION);
    encodeField( reqId);

    send();
}

void IBQt::reqMarketDataType(int marketDataType)
{
    // not connected?
    if( !m_connected) {
        emit error( NO_VALID_ID, NOT_CONNECTED.code(), NOT_CONNECTED.msg());
        return;
    }

    if (m_serverVersion < MIN_SERVER_VER_CANCEL_CALC_OPTION_PRICE) {
        emit error( NO_VALID_ID, UPDATE_TWS.code(), UPDATE_TWS.msg() +
            "  It does not support market data type requests.");
        return;

    }

    const int VERSION = 1;

    encodeField( REQ_MARKET_DATA_TYPE);
    encodeField( VERSION);
    encodeField( marketDataType);

    send();
}

void IBQt::onConnected()
{
//qDebug() << "TWS is connected";
//    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 16384);
//    /*qDebug*/() << "[DEBUG-onConnected] socketRecvBufferSize:" << m_socket->socketOption(QAbstractSocket::ReceiveBufferSizeSocketOption).toInt();
//    qDebug() << "[DEBUG-onConnected] readBufferSize:" << m_socket->readBufferSize();
}

void IBQt::onReadyRead()
{
//    qDebug();
//    qDebug();
//    qDebug() << "[DEBUG-onReadyRead] bytesAvailable:" << m_socket->bytesAvailable();


    m_inBuffer.append(m_socket->readAll());

    if (m_socket->bytesAvailable() == 65536)
        qFatal("[CRITICAL ERROR] Socket Buffer is too small.. increase size in registry.. (is this Windows 7)");

//    qDebug() << "[DEBUG-onReadyRead] m_inBuffer.size():" << m_inBuffer.size();
//    qDebug() << "[DEBUG-onReadyRead] raw:" << m_inBuffer;


    while (m_inBuffer.size()) {

//    qDebug() << "[REP]" << m_inBuffer;

    // I'M GETTING EMPTY MESSAGES.. IS THIS THE CORRECT WAY TO HANDLE THIS ???

    if (m_inBuffer.isEmpty()) {
//qDebug() << "Received empty onReadyRead message.. ignoring it..";
        return;
    }

    // WHY ARE MESSAGES FROM TWS SARTING WITH A '\0' ??? ... IS THIS THE CORRECT WAY TO HANDLE IT?
    if (m_inBuffer.startsWith('\0')) {
//qDebug() << "[DEBUG] onReadyRead message started with a \'\0\' ??? wtf, I'm removing it";
        m_inBuffer.remove(0, 1);
    }

    // DEBUG
    //    qDebug() << "onReadyRead msg:" << m_inBuffer;

    if (!m_connected) {
        decodeField(m_serverVersion);

        if (m_serverVersion >= 20)
            decodeField(m_twsTime);

        if (m_serverVersion < SERVER_VERSION) {
            m_socket->disconnectFromHost();
            qCritical("The TWS is out of date and must be upgraded.");
        }

        m_connected = true;
        emit twsConnected();

        //        cleanInBuffer();

        // DEBUG
        //        qDebug() << "server version:" << m_serverVersion;
        //        qDebug() << "tws time:" << m_twsTime;
        //        qDebug() << "left over:" << decodeField(m_inBuffer);

        // send the clientId
        if (m_serverVersion >= 3) {
            if (m_serverVersion < MIN_SERVER_VER_LINKING) {
                encodeField(m_clientId);
                send();
            }
            else if (!m_extraAuth) {
                const int VERSION = 1;
                encodeField(START_API);
                encodeField(VERSION);
                encodeField(m_clientId);
                send();
            }
        }
    }

    else { // yes we're connected

        int msgId = 0;

        decodeField(msgId);

//        qDebug() << "[DEBUG-onReadyRead] msgId:" << msgId;


        switch (msgId) {
        case TICK_PRICE:
        {
            int version;
            int tickerId;
            int tickTypeInt;
            double price;
            int size;
            int canAutoExecute;

            decodeField(version);
            decodeField(tickerId);
            decodeField(tickTypeInt);
            decodeField(price);
            decodeField(size);
            decodeField(canAutoExecute);

            emit tickPrice(tickerId, (TickType)tickTypeInt, price, canAutoExecute);

            // process version 2 fields here
            {
                TickType sizeTickType = NOT_SET;
                switch ((TickType)tickTypeInt) {
                case BID:
                    sizeTickType = BID_SIZE;
                    break;
                case ASK:
                    sizeTickType = ASK_SIZE;
                    break;
                case LAST:
                    sizeTickType = LAST_SIZE;
                    break;
                default:
                    break;
                }
                if (sizeTickType != NOT_SET)
                    emit tickSize(tickerId, sizeTickType, size);
            }
            break;
        }
        case TICK_SIZE:
        {
            int version;
            int tickerId;
            int tickTypeInt;
            int size;

            decodeField(version);
            decodeField(tickerId);
            decodeField(tickTypeInt);
            decodeField(size);

            emit tickSize(tickerId, (TickType)tickTypeInt, size);
            break;
        }
        case TICK_OPTION_COMPUTATION:
        {
            int version;
            int tickerId;
            int tickTypeInt;
            double impliedVol;
            double delta;

            double optPrice = DBL_MAX;
            double pvDividend = DBL_MAX;
            double gamma = DBL_MAX;
            double vega = DBL_MAX;
            double theta = DBL_MAX;
            double undPrice = DBL_MAX;

            decodeField(version);
            decodeField(tickerId);
            decodeField(tickTypeInt);
            decodeField(impliedVol);
            decodeField(delta);

            if (impliedVol < 0)
                impliedVol = DBL_MAX;
            if ((delta > 1) || (delta < -1))
                delta = DBL_MAX;
            if ((version >= 6) || tickTypeInt == MODEL_OPTION) {
                decodeField(optPrice);
                decodeField(pvDividend);

                if (optPrice < 0)
                    optPrice = DBL_MAX;
                if (pvDividend < 0)
                    pvDividend = DBL_MAX;
            }
            if (version >= 6) {
                decodeField(gamma);
                decodeField(vega);
                decodeField(theta);
                decodeField(undPrice);

                if (gamma > 1 || gamma < -1)
                    gamma = DBL_MAX;
                if (vega > 1 || vega < -1)
                    vega = DBL_MAX;
                if (theta > 1 || theta < -1)
                    theta = DBL_MAX;
                if (undPrice < 0)
                    undPrice = DBL_MAX;
            }
            emit tickOptionComputation(tickerId, (TickType)tickTypeInt, impliedVol, delta, optPrice, pvDividend, gamma, vega, theta, undPrice);
            break;
        }
        case TICK_GENERIC:
        {
            int version;
            int tickerId;
            int tickTypeInt;
            double value;

            decodeField(version);
            decodeField(tickerId);
            decodeField(tickTypeInt);
            decodeField(value);

            emit tickGeneric(tickerId, (TickType)tickTypeInt, value);
            break;
        }
        case TICK_STRING:
        {
            int version;
            int tickerId;
            int tickTypeInt;
            QByteArray value;

            decodeField(version);
            decodeField(tickerId);
            decodeField(tickTypeInt);
            decodeField(value);

            emit tickString(tickerId, (TickType)tickTypeInt, value);
            break;
        }
        case TICK_EFP:
        {
            int version;
            int tickerId;
            int tickTypeInt;
            double basisPoints;
            QByteArray formattedBasisPoints;
            double impliedFuturesPrice;
            int holdDays;
            QByteArray futureExpiry;
            double dividendImpact;
            double dividendsToExpiry;

            decodeField(version);
            decodeField(tickerId);
            decodeField(tickTypeInt);
            decodeField(basisPoints);
            decodeField(formattedBasisPoints);
            decodeField(impliedFuturesPrice);
            decodeField(holdDays);
            decodeField(futureExpiry);
            decodeField(dividendImpact);
            decodeField(dividendsToExpiry);

            emit tickEFP(tickerId, (TickType)tickTypeInt, basisPoints, formattedBasisPoints, impliedFuturesPrice, holdDays, futureExpiry, dividendImpact, dividendsToExpiry);
            break;
        }
        case ORDER_STATUS:
        {
            int version;
            int orderId;
            QByteArray status;
            int filled;
            int remaining;
            double avgFillPrice;
            int permId;
            int parentId;
            double lastFillPrice;
            int clientId;
            QByteArray whyHeld;

            decodeField(version);
            decodeField(orderId);
            decodeField(status);
            decodeField(filled);
            decodeField(remaining);
            decodeField(avgFillPrice);
            decodeField(permId);
            decodeField(parentId);
            decodeField(lastFillPrice);
            decodeField(clientId);
            decodeField(whyHeld);

            emit orderStatus(orderId, status, filled, remaining, avgFillPrice, permId, parentId, lastFillPrice, clientId, whyHeld);
            break;
        }
        case ERR_MSG:
        {
            int version;
            int id;
            int errorCode;
            QByteArray errorMsg;

            decodeField(version);
            decodeField(id);
            decodeField(errorCode);
            decodeField(errorMsg);

            emit error(id, errorCode, errorMsg);
            break;
        }
        case OPEN_ORDER:
        {
            // read version
            int version;
            decodeField(version);

            // read order id
            Order order;
            decodeField(order.orderId);

            // read contract fields
            Contract contract;
            decodeField(contract.conId); // ver 17 field
            decodeField(contract.symbol);
            decodeField(contract.secType);
            decodeField(contract.expiry);
            decodeField(contract.strike);
            decodeField(contract.right);
            if (version >= 32) {
                decodeField(contract.multiplier);
            }
            decodeField(contract.exchange);
            decodeField(contract.currency);
            decodeField(contract.localSymbol); // ver 2 field
            if (version >= 32) {
                decodeField(contract.tradingClass);
            }

            // read order fields
            decodeField(order.action);
            decodeField(order.totalQuantity);
            decodeField(order.orderType);
            if (version < 29) {
                decodeField(order.lmtPrice);
            }
            else {
                decodeFieldMax( order.lmtPrice);
            }
            if (version < 30) {
                decodeField(order.auxPrice);
            }
            else {
                decodeFieldMax( order.auxPrice);
            }
            decodeField(order.tif);
            decodeField(order.ocaGroup);
            decodeField(order.account);
            decodeField(order.openClose);

            int orderOriginInt;
            decodeField(orderOriginInt);
            order.origin = (Origin)orderOriginInt;

            decodeField(order.orderRef);
            decodeField(order.clientId); // ver 3 field
            decodeField(order.permId); // ver 4 field

            //if( version < 18) {
            //	// will never happen
            //	/* order.ignoreRth = */ readBoolFromInt();
            //}

            decodeField(order.outsideRth); // ver 18 field
            decodeField(order.hidden); // ver 4 field
            decodeField(order.discretionaryAmt); // ver 4 field
            decodeField(order.goodAfterTime); // ver 5 field

            {
                QByteArray sharesAllocation;
                decodeField(sharesAllocation); // deprecated ver 6 field
            }

            decodeField(order.faGroup); // ver 7 field
            decodeField(order.faMethod); // ver 7 field
            decodeField(order.faPercentage); // ver 7 field
            decodeField(order.faProfile); // ver 7 field

            decodeField(order.goodTillDate); // ver 8 field

            decodeField(order.rule80A); // ver 9 field
            decodeFieldMax( order.percentOffset); // ver 9 field
            decodeField(order.settlingFirm); // ver 9 field
            decodeField(order.shortSaleSlot); // ver 9 field
            decodeField(order.designatedLocation); // ver 9 field
            if( m_serverVersion == MIN_SERVER_VER_SSHORTX_OLD){
                int exemptCode;
                decodeField(exemptCode);
            }
            else if( version >= 23){
                decodeField(order.exemptCode);
            }
            decodeField(order.auctionStrategy); // ver 9 field
            decodeFieldMax( order.startingPrice); // ver 9 field
            decodeFieldMax( order.stockRefPrice); // ver 9 field
            decodeFieldMax( order.delta); // ver 9 field
            decodeFieldMax( order.stockRangeLower); // ver 9 field
            decodeFieldMax( order.stockRangeUpper); // ver 9 field
            decodeField(order.displaySize); // ver 9 field

            //if( version < 18) {
            //		// will never happen
            //		/* order.rthOnly = */ readBoolFromInt();
            //}

            decodeField(order.blockOrder); // ver 9 field
            decodeField(order.sweepToFill); // ver 9 field
            decodeField(order.allOrNone); // ver 9 field
            decodeFieldMax( order.minQty); // ver 9 field
            decodeField(order.ocaType); // ver 9 field
            decodeField(order.eTradeOnly); // ver 9 field
            decodeField(order.firmQuoteOnly); // ver 9 field
            decodeFieldMax( order.nbboPriceCap); // ver 9 field

            decodeField(order.parentId); // ver 10 field
            decodeField(order.triggerMethod); // ver 10 field

            decodeFieldMax( order.volatility); // ver 11 field
            decodeField(order.volatilityType); // ver 11 field
            decodeField(order.deltaNeutralOrderType); // ver 11 field (had a hack for ver 11)
            decodeFieldMax( order.deltaNeutralAuxPrice); // ver 12 field

            if (version >= 27 && !order.deltaNeutralOrderType.isEmpty()) {
                decodeField(order.deltaNeutralConId);
                decodeField(order.deltaNeutralSettlingFirm);
                decodeField(order.deltaNeutralClearingAccount);
                decodeField(order.deltaNeutralClearingIntent);
            }

            if (version >= 31 && !order.deltaNeutralOrderType.isEmpty()) {
                decodeField(order.deltaNeutralOpenClose);
                decodeField(order.deltaNeutralShortSale);
                decodeField(order.deltaNeutralShortSaleSlot);
                decodeField(order.deltaNeutralDesignatedLocation);
            }

            decodeField(order.continuousUpdate); // ver 11 field

            // will never happen
            //if( m_serverVersion == 26) {
            //	order.stockRangeLower = readDouble();
            //	order.stockRangeUpper = readDouble();
            //}

            decodeField(order.referencePriceType); // ver 11 field

            decodeFieldMax( order.trailStopPrice); // ver 13 field

            if (version >= 30) {
                decodeFieldMax( order.trailingPercent);
            }

            decodeFieldMax( order.basisPoints); // ver 14 field
            decodeFieldMax( order.basisPointsType); // ver 14 field
            decodeField(contract.comboLegsDescrip); // ver 14 field

            if (version >= 29) {
                int comboLegsCount = 0;
                decodeField(comboLegsCount);

                if (comboLegsCount > 0) {
                    QList<ComboLeg*> comboLegs;
                    for (int i = 0; i < comboLegsCount; ++i) {
                        ComboLeg* comboLeg = new ComboLeg();
                        decodeField(comboLeg->conId);
                        decodeField(comboLeg->ratio);
                        decodeField(comboLeg->action);
                        decodeField(comboLeg->exchange);
                        decodeField(comboLeg->openClose);
                        decodeField(comboLeg->shortSaleSlot);
                        decodeField(comboLeg->designatedLocation);
                        decodeField(comboLeg->exemptCode);

                        comboLegs.append(comboLeg);
                    }
                    contract.comboLegs = comboLegs;
                }

                int orderComboLegsCount = 0;
                decodeField(orderComboLegsCount);
                if (orderComboLegsCount > 0) {
                    QList<OrderComboLeg*> orderComboLegs;
                    for (int i = 0; i < orderComboLegsCount; ++i) {
                        OrderComboLeg* orderComboLeg = new OrderComboLeg();
                        decodeFieldMax( orderComboLeg->price);

                        orderComboLegs.append(orderComboLeg);
                    }
                    order.orderComboLegs = orderComboLegs;
                }
            }

            if (version >= 26) {
                int smartComboRoutingParamsCount = 0;
                decodeField(smartComboRoutingParamsCount);
                if( smartComboRoutingParamsCount > 0) {
                    QList<TagValue*> smartComboRoutingParams;
                    for( int i = 0; i < smartComboRoutingParamsCount; ++i) {
                        TagValue* tagValue = new TagValue();
                        decodeField(tagValue->tag);
                        decodeField(tagValue->value);
                        smartComboRoutingParams.append(tagValue);
                    }
                    order.smartComboRoutingParams = smartComboRoutingParams;
                }
            }

            if( version >= 20) {
                decodeFieldMax( order.scaleInitLevelSize);
                decodeFieldMax( order.scaleSubsLevelSize);
            }
            else {
                // ver 15 fields
                int notSuppScaleNumComponents = 0;
                decodeFieldMax( notSuppScaleNumComponents);
                decodeFieldMax( order.scaleInitLevelSize); // scaleComponectSize
            }
            decodeFieldMax( order.scalePriceIncrement); // ver 15 field

            if (version >= 28 && order.scalePriceIncrement > 0.0 && order.scalePriceIncrement != UNSET_DOUBLE) {
                decodeFieldMax( order.scalePriceAdjustValue);
                decodeFieldMax( order.scalePriceAdjustInterval);
                decodeFieldMax( order.scaleProfitOffset);
                decodeField(order.scaleAutoReset);
                decodeFieldMax( order.scaleInitPosition);
                decodeFieldMax( order.scaleInitFillQty);
                decodeField(order.scaleRandomPercent);
            }

            if( version >= 24) {
                decodeField(order.hedgeType);
                if( !order.hedgeType.isEmpty()) {
                    decodeField(order.hedgeParam);
                }
            }

            if( version >= 25) {
                decodeField(order.optOutSmartRouting);
            }

            decodeField(order.clearingAccount); // ver 19 field
            decodeField(order.clearingIntent); // ver 19 field

            if( version >= 22) {
                decodeField(order.notHeld);
            }

            UnderComp underComp;
            if( version >= 20) {
                bool underCompPresent = false;
                decodeField(underCompPresent);
                if( underCompPresent){
                    decodeField(underComp.conId);
                    decodeField(underComp.delta);
                    decodeField(underComp.price);
                    contract.underComp = &underComp;
                }
            }


            if( version >= 21) {
                decodeField(order.algoStrategy);
                if( !order.algoStrategy.isEmpty()) {
                    int algoParamsCount = 0;
                    decodeField(algoParamsCount);
                    if( algoParamsCount > 0) {
                        for( int i = 0; i < algoParamsCount; ++i) {
                            TagValue* tagValue = new TagValue();
                            decodeField(tagValue->tag);
                            decodeField(tagValue->value);
                            order.algoParams.append( tagValue);
                        }
                    }
                }
            }

            OrderState orderState;

            decodeField(order.whatIf); // ver 16 field

            decodeField(orderState.status); // ver 16 field
            decodeField(orderState.initMargin); // ver 16 field
            decodeField(orderState.maintMargin); // ver 16 field
            decodeField(orderState.equityWithLoan); // ver 16 field
            decodeFieldMax( orderState.commission); // ver 16 field
            decodeFieldMax( orderState.minCommission); // ver 16 field
            decodeFieldMax( orderState.maxCommission); // ver 16 field
            decodeField(orderState.commissionCurrency); // ver 16 field
            decodeField(orderState.warningText); // ver 16 field

            emit openOrder( (OrderId)order.orderId, contract, order, orderState);
            break;
        }
        case ACCT_VALUE:
        {
            int version;
            QByteArray key;
            QByteArray val;
            QByteArray cur;
            QByteArray accountName;

            decodeField(version);
            decodeField(key);
            decodeField(val);
            decodeField(cur);
            decodeField(accountName); // ver 2 field

            emit updateAccountValue( key, val, cur, accountName);
            break;
        }

        case PORTFOLIO_VALUE:
        {
            // decode version
            int version;
            decodeField(version);

            // read contract fields
            Contract contract;
            decodeField(contract.conId); // ver 6 field
            decodeField(contract.symbol);
            decodeField(contract.secType);
            decodeField(contract.expiry);
            decodeField(contract.strike);
            decodeField(contract.right);

            if( version >= 7) {
                decodeField(contract.multiplier);
                decodeField(contract.primaryExchange);
            }

            decodeField(contract.currency);
            decodeField(contract.localSymbol); // ver 2 field
            if (version >= 8) {
                decodeField(contract.tradingClass);
            }

            int     position;
            double  marketPrice;
            double  marketValue;
            double  averageCost;
            double  unrealizedPNL;
            double  realizedPNL;

            decodeField(position);
            decodeField(marketPrice);
            decodeField(marketValue);
            decodeField(averageCost); // ver 3 field
            decodeField(unrealizedPNL); // ver 3 field
            decodeField(realizedPNL); // ver 3 field

            QByteArray accountName;
            decodeField(accountName); // ver 4 field
            if( version == 6 && m_serverVersion == 39) {
                decodeField(contract.primaryExchange);
            }

            emit updatePortfolio( contract,
                                  position, marketPrice, marketValue, averageCost,
                                  unrealizedPNL, realizedPNL, accountName);

            break;
        }
        case ACCT_UPDATE_TIME:
        {
            int version;
            QByteArray accountTime;

            decodeField(version);
            decodeField(accountTime);

            emit updateAccountTime( accountTime);
            break;
        }

        case NEXT_VALID_ID:
        {
            int version;
            int orderId;

            decodeField(version);
            decodeField(orderId);

            emit nextValidId(orderId);
            break;
        }

        case CONTRACT_DATA:
        {
            int version;
            decodeField(version);

            int reqId = -1;
            if( version >= 3) {
                decodeField(reqId);
            }

            ContractDetails contract;
            decodeField(contract.summary.symbol);
            decodeField(contract.summary.secType);
            decodeField(contract.summary.expiry);
            decodeField(contract.summary.strike);
            decodeField(contract.summary.right);
            decodeField(contract.summary.exchange);
            decodeField(contract.summary.currency);
            decodeField(contract.summary.localSymbol);
            decodeField(contract.marketName);
            decodeField(contract.summary.tradingClass);
            decodeField(contract.summary.conId);
            decodeField(contract.minTick);
            decodeField(contract.summary.multiplier);
            decodeField(contract.orderTypes);
            decodeField(contract.validExchanges);
            decodeField(contract.priceMagnifier); // ver 2 field
            if( version >= 4) {
                decodeField(contract.underConId);
            }
            if( version >= 5) {
                decodeField(contract.longName);
                decodeField(contract.summary.primaryExchange);
            }
            if( version >= 6) {
                decodeField(contract.contractMonth);
                decodeField(contract.industry);
                decodeField(contract.category);
                decodeField(contract.subcategory);
                decodeField(contract.timeZoneId);
                decodeField(contract.tradingHours);
                decodeField(contract.liquidHours);
            }
            if( version >= 8) {
                decodeField(contract.evRule);
                decodeField(contract.evMultiplier);
            }
            if( version >= 7) {
                int secIdListCount = 0;
                decodeField(secIdListCount);
                if( secIdListCount > 0) {
                    for( int i = 0; i < secIdListCount; ++i) {
                        TagValue* tagValue = new TagValue();
                        decodeField(tagValue->tag);
                        decodeField(tagValue->value);
                        contract.secIdList.append( tagValue);
                    }
                }
            }

            emit contractDetails( reqId, contract);
            break;
        }

        case BOND_CONTRACT_DATA:
        {
            int version;
            decodeField(version);

            int reqId = -1;
            if( version >= 3) {
                decodeField(reqId);
            }

            ContractDetails contract;
            decodeField(contract.summary.symbol);
            decodeField(contract.summary.secType);
            decodeField(contract.cusip);
            decodeField(contract.coupon);
            decodeField(contract.maturity);
            decodeField(contract.issueDate);
            decodeField(contract.ratings);
            decodeField(contract.bondType);
            decodeField(contract.couponType);
            decodeField(contract.convertible);
            decodeField(contract.callable);
            decodeField(contract.putable);
            decodeField(contract.descAppend);
            decodeField(contract.summary.exchange);
            decodeField(contract.summary.currency);
            decodeField(contract.marketName);
            decodeField(contract.summary.tradingClass);
            decodeField(contract.summary.conId);
            decodeField(contract.minTick);
            decodeField(contract.orderTypes);
            decodeField(contract.validExchanges);
            decodeField(contract.nextOptionDate); // ver 2 field
            decodeField(contract.nextOptionType); // ver 2 field
            decodeField(contract.nextOptionPartial); // ver 2 field
            decodeField(contract.notes); // ver 2 field
            if( version >= 4) {
                decodeField(contract.longName);
            }
            if( version >= 6) {
                decodeField(contract.evRule);
                decodeField(contract.evMultiplier);
            }
            if( version >= 5) {
                int secIdListCount = 0;
                decodeField(secIdListCount);
                if( secIdListCount > 0) {
                    for( int i = 0; i < secIdListCount; ++i) {
                        TagValue* tagValue = new TagValue();
                        decodeField(tagValue->tag);
                        decodeField(tagValue->value);
                        contract.secIdList.append(tagValue);
                    }
                }
            }

            emit bondContractDetails( reqId, contract);
            break;
        }

        case EXECUTION_DATA:
        {
            int version;
            decodeField(version);

            int reqId = -1;
            if( version >= 7) {
                decodeField(reqId);
            }

            int orderId;
            decodeField(orderId);

            // decode contract fields
            Contract contract;
            decodeField(contract.conId); // ver 5 field
            decodeField(contract.symbol);
            decodeField(contract.secType);
            decodeField(contract.expiry);
            decodeField(contract.strike);
            decodeField(contract.right);
            if( version >= 9) {
                decodeField(contract.multiplier);
            }
            decodeField(contract.exchange);
            decodeField(contract.currency);
            decodeField(contract.localSymbol);
            if (version >= 10) {
                decodeField(contract.tradingClass);
            }

            // decode execution fields
            Execution exec;
            exec.orderId = orderId;
            decodeField(exec.execId);
            decodeField(exec.time);
            decodeField(exec.acctNumber);
            decodeField(exec.exchange);
            decodeField(exec.side);
            decodeField(exec.shares);
            decodeField(exec.price);
            decodeField(exec.permId); // ver 2 field
            decodeField(exec.clientId); // ver 3 field
            decodeField(exec.liquidation); // ver 4 field

            if( version >= 6) {
                decodeField(exec.cumQty);
                decodeField(exec.avgPrice);
            }

            if( version >= 8) {
                decodeField(exec.orderRef);
            }

            if( version >= 9) {
                decodeField(exec.evRule);
                decodeField(exec.evMultiplier);
            }

            emit execDetails( reqId, contract, exec);
            break;
        }

        case MARKET_DEPTH:
        {
            int version;
            int id;
            int position;
            int operation;
            int side;
            double price;
            int size;

            decodeField(version);
            decodeField(id);
            decodeField(position);
            decodeField(operation);
            decodeField(side);
            decodeField(price);
            decodeField(size);

            emit updateMktDepth( id, position, operation, side, price, size);
            break;
        }

        case MARKET_DEPTH_L2:
        {
            int version;
            int id;
            int position;
            QByteArray marketMaker;
            int operation;
            int side;
            double price;
            int size;

            decodeField(version);
            decodeField(id);
            decodeField(position);
            decodeField(marketMaker);
            decodeField(operation);
            decodeField(side);
            decodeField(price);
            decodeField(size);

            emit updateMktDepthL2( id, position, marketMaker, operation, side,
                                   price, size);

            break;
        }

        case NEWS_BULLETINS:
        {
            int version;
            int msgId;
            int msgType;
            QByteArray newsMessage;
            QByteArray originatingExch;

            decodeField(version);
            decodeField(msgId);
            decodeField(msgType);
            decodeField(newsMessage);
            decodeField(originatingExch);

            emit updateNewsBulletin( msgId, msgType, newsMessage, originatingExch);
            break;
        }

        case MANAGED_ACCTS:
        {
            int version;
            QByteArray accountsList;

            decodeField(version);
            decodeField(accountsList);

            emit managedAccounts( accountsList);
            break;
        }

        case RECEIVE_FA:
        {
            int version;
            int faDataTypeInt;
            QByteArray cxml;

            decodeField(version);
            decodeField(faDataTypeInt);
            decodeField(cxml);

            emit receiveFA( (FaDataType)faDataTypeInt, cxml);
            break;
        }

        case HISTORICAL_DATA:
        {
            int version;
            int reqId;
            QByteArray startDateStr;
            QByteArray endDateStr;

            decodeField(version);
            decodeField(reqId);
            decodeField(startDateStr); // ver 2 field
            decodeField(endDateStr); // ver 2 field

            int itemCount;
            decodeField(itemCount);

            QVector<BarData> bars;

            for( int ctr = 0; ctr < itemCount; ++ctr) {
                BarData bar;
                decodeField(bar.date);
                decodeField(bar.open);
                decodeField(bar.high);
                decodeField(bar.low);
                decodeField(bar.close);
                decodeField(bar.volume);
                decodeField(bar.average);
                decodeField(bar.hasGaps);
                decodeField(bar.barCount); // ver 3 field

                bars.push_back(bar);
            }

            //            assert( (int)bars.size() == itemCount);

//qDebug() << "[DEBUG-HISTORICAL_DATA] bars.size():" << bars.size() << "itemCount:" << itemCount;

            for( int ctr = 0; ctr < bars.size(); ++ctr) {

                const BarData& bar = bars[ctr];
                emit historicalData( reqId, bar.date, bar.open, bar.high, bar.low,
                                     bar.close, bar.volume, bar.barCount, bar.average,
                                     (bar.hasGaps == "true" ? 1 : 0));
            }

            // send end of dataset marker
            QByteArray finishedStr = QByteArray("finished-") + startDateStr + "-" + endDateStr;
            emit historicalData( reqId, finishedStr, -1, -1, -1, -1, -1, -1, -1, 0);

            break;
        }

        case SCANNER_DATA:
        {
            int version;
            int tickerId;

            decodeField(version);
            decodeField(tickerId);

            int numberOfElements;
            decodeField(numberOfElements);

            typedef std::vector<ScanData> ScanDataList;
            ScanDataList scannerDataList;

            scannerDataList.reserve( numberOfElements);

            for( int ctr=0; ctr < numberOfElements; ++ctr) {

                ScanData data;

                decodeField(data.rank);
                decodeField(data.contract.summary.conId); // ver 3 field
                decodeField(data.contract.summary.symbol);
                decodeField(data.contract.summary.secType);
                decodeField(data.contract.summary.expiry);
                decodeField(data.contract.summary.strike);
                decodeField(data.contract.summary.right);
                decodeField(data.contract.summary.exchange);
                decodeField(data.contract.summary.currency);
                decodeField(data.contract.summary.localSymbol);
                decodeField(data.contract.marketName);
                decodeField(data.contract.summary.tradingClass);
                decodeField(data.distance);
                decodeField(data.benchmark);
                decodeField(data.projection);
                decodeField(data.legsStr);

                scannerDataList.push_back( data);
            }

            //            assert( (int)scannerDataList.size() == numberOfElements);

            for( int ctr=0; ctr < numberOfElements; ++ctr) {

                const ScanData& data = scannerDataList[ctr];
                emit scannerData( tickerId, data.rank, data.contract,
                                  data.distance, data.benchmark, data.projection, data.legsStr);
            }

            emit scannerDataEnd( tickerId);
            break;
        }

        case SCANNER_PARAMETERS:
        {
            int version;
            QByteArray xml;

            decodeField(version);
            decodeField(xml);

            emit scannerParameters( xml);
            break;
        }

        case CURRENT_TIME:
        {
            int version;
            int time;

            decodeField(version);
            decodeField(time);

            emit currentTime( time);
            break;
        }

        case REAL_TIME_BARS:
        {
            int version;
            int reqId;
            int time;
            double open;
            double high;
            double low;
            double close;
            int volume;
            double average;
            int count;

            decodeField(version);
            decodeField(reqId);
            decodeField(time);
            decodeField(open);
            decodeField(high);
            decodeField(low);
            decodeField(close);
            decodeField(volume);
            decodeField(average);
            decodeField(count);

            emit realtimeBar( reqId, time, open, high, low, close,
                              volume, average, count);

            break;
        }

        case FUNDAMENTAL_DATA:
        {
            int version;
            int reqId;
            QByteArray data;

            decodeField(version);
            decodeField(reqId);
            decodeField(data);

            emit fundamentalData( reqId, data);
            break;
        }

        case CONTRACT_DATA_END:
        {
            int version;
            int reqId;

            decodeField(version);
            decodeField(reqId);

            emit contractDetailsEnd( reqId);
            break;
        }

        case OPEN_ORDER_END:
        {
            int version;

            decodeField(version);

            emit openOrderEnd();
            break;
        }

        case ACCT_DOWNLOAD_END:
        {
            int version;
            QByteArray account;

            decodeField(version);
            decodeField(account);

            emit accountDownloadEnd( account);
            break;
        }

        case EXECUTION_DATA_END:
        {
            int version;
            int reqId;

            decodeField(version);
            decodeField(reqId);

            emit execDetailsEnd( reqId);
            break;
        }

        case DELTA_NEUTRAL_VALIDATION:
        {
            int version;
            int reqId;

            decodeField(version);
            decodeField(reqId);

            UnderComp underComp;

            decodeField(underComp.conId);
            decodeField(underComp.delta);
            decodeField(underComp.price);

            emit deltaNeutralValidation( reqId, underComp);
            break;
        }

        case TICK_SNAPSHOT_END:
        {
            int version;
            int reqId;

            decodeField(version);
            decodeField(reqId);

            emit tickSnapshotEnd( reqId);
            break;
        }

        case MARKET_DATA_TYPE:
        {
            int version;
            int reqId;
            int marketDataTypeVal;

            decodeField(version);
            decodeField(reqId);
            decodeField(marketDataTypeVal);

            emit marketDataType( reqId, marketDataTypeVal);
            break;
        }

        case COMMISSION_REPORT:
        {
            int version;
            decodeField(version);

            CommissionReport cr;
            decodeField(cr.execId);
            decodeField(cr.commission);
            decodeField(cr.currency);
            decodeField(cr.realizedPNL);
            decodeField(cr.yield);
            decodeField(cr.yieldRedemptionDate);

            emit commissionReport( cr);
            break;
        }

        case POSITION_DATA:
        {
            int version;
            QByteArray account;
            int pos;
            double avgCost = 0;

            decodeField(version);
            decodeField(account);

            // decode contract fields
            Contract contract;
            decodeField(contract.conId);
            decodeField(contract.symbol);
            decodeField(contract.secType);
            decodeField(contract.expiry);
            decodeField(contract.strike);
            decodeField(contract.right);
            decodeField(contract.multiplier);
            decodeField(contract.exchange);
            decodeField(contract.currency);
            decodeField(contract.localSymbol);
            if (version >= 2) {
                decodeField(contract.tradingClass);
            }

            decodeField(pos);
            if (version >= 3) {
                decodeField(avgCost);
            }

            emit position( account, contract, pos, avgCost);
            break;
        }

        case POSITION_END:
        {
            int version;

            decodeField(version);

            emit positionEnd();
            break;
        }

        case ACCOUNT_SUMMARY:
        {
            int version;
            int reqId;
            QByteArray account;
            QByteArray tag;
            QByteArray value;
            QByteArray curency;

            decodeField(version);
            decodeField(reqId);
            decodeField(account);
            decodeField(tag);
            decodeField(value);
            decodeField(curency);

            emit accountSummary( reqId, account, tag, value, curency);
            break;
        }

        case ACCOUNT_SUMMARY_END:
        {
            int version;
            int reqId;

            decodeField(version);
            decodeField(reqId);

            emit accountSummaryEnd( reqId);
            break;
        }

        case VERIFY_MESSAGE_API:
        {
            int version;
            QByteArray apiData;

            decodeField(version);
            decodeField(apiData);

            emit verifyMessageAPI( apiData);
            break;
        }

        case VERIFY_COMPLETED:
        {
            int version;
            QByteArray isSuccessful;
            QByteArray errorText;

            decodeField(version);
            decodeField(isSuccessful);
            decodeField(errorText);

            bool bRes = isSuccessful == "true";

            if (bRes) {
                const int VERSION = 1;
                encodeField(START_API);
                encodeField(VERSION);
                encodeField(m_clientId);
                send();
            }

            emit verifyCompleted( bRes, errorText);
            break;
        }

        case DISPLAY_GROUP_LIST:
        {
            int version;
            int reqId;
            QByteArray groups;

            decodeField(version);
            decodeField(reqId);
            decodeField(groups);

            emit displayGroupList( reqId, groups);
            break;
        }

        case DISPLAY_GROUP_UPDATED:
        {
            int version;
            int reqId;
            QByteArray contractInfo;

            decodeField(version);
            decodeField(reqId);
            decodeField(contractInfo);

            emit displayGroupUpdated( reqId, contractInfo);
            break;
        }

        default:
        {
            QString errstr("[CRITICAL] UNKNOWN_ID: msgId:"
                           + QString::number(msgId)
                           + "buffer: "
                           + m_inBuffer.mid(m_endIdx));
            // pDebug(errstr);

            emit error( msgId, UNKNOWN_ID.code(), UNKNOWN_ID.msg());
            disconnectTWS();
            emit connectionClosed();
            break;
        }
        }
    }

    cleanInBuffer();

    }
}

void IBQt::onSocketError(QAbstractSocket::SocketError socketError)
{
//    QAbstractSocket::ConnectionRefusedError	0	The connection was refused by the peer (or timed out).
//    QAbstractSocket::RemoteHostClosedError	1	The remote host closed the connection. Note that the client socket (i.e., this socket) will be closed after the remote close notification has been sent.
//    QAbstractSocket::HostNotFoundError	2	The host address was not found.
//    QAbstractSocket::SocketAccessError	3	The socket operation failed because the application lacked the required privileges.
//    QAbstractSocket::SocketResourceError	4	The local system ran out of resources (e.g., too many sockets).
//    QAbstractSocket::SocketTimeoutError	5	The socket operation timed out.
//    QAbstractSocket::DatagramTooLargeError	6	The datagram was larger than the operating system's limit (which can be as low as 8192 bytes).
//    QAbstractSocket::NetworkError	7	An error occurred with the network (e.g., the network cable was accidentally plugged out).
//    QAbstractSocket::AddressInUseError	8	The address specified to QAbstractSocket::bind() is already in use and was set to be exclusive.
//    QAbstractSocket::SocketAddressNotAvailableError	9	The address specified to QAbstractSocket::bind() does not belong to the host.
//    QAbstractSocket::UnsupportedSocketOperationError	10	The requested socket operation is not supported by the local operating system (e.g., lack of IPv6 support).
//    QAbstractSocket::ProxyAuthenticationRequiredError	12	The socket is using a proxy, and the proxy requires authentication.
//    QAbstractSocket::SslHandshakeFailedError	13	The SSL/TLS handshake failed, so the connection was closed (only used in QSslSocket)
//    QAbstractSocket::UnfinishedSocketOperationError	11	Used by QAbstractSocketEngine only, The last operation attempted has not finished yet (still in progress in the background).
//    QAbstractSocket::ProxyConnectionRefusedError	14	Could not contact the proxy server because the connection to that server was denied
//    QAbstractSocket::ProxyConnectionClosedError	15	The connection to the proxy server was closed unexpectedly (before the connection to the final peer was established)
//    QAbstractSocket::ProxyConnectionTimeoutError	16	The connection to the proxy server timed out or the proxy server stopped responding in the authentication phase.
//    QAbstractSocket::ProxyNotFoundError	17	The proxy address set with setProxy() (or the application proxy) was not found.
//    QAbstractSocket::ProxyProtocolError	18	The connection negotiation with the proxy server failed, because the response from the proxy server could not be understood.
//    QAbstractSocket::OperationError	19	An operation was attempted while the socket was in a state that did not permit it.
//    QAbstractSocket::SslInternalError	20	The SSL library being used reported an internal error. This is probably the result of a bad installation or misconfiguration of the library.
//    QAbstractSocket::SslInvalidUserDataError	21	Invalid data (certificate, key, cypher, etc.) was provided and its use resulted in an error in the SSL library.
//    QAbstractSocket::TemporaryError	22	A temporary error occurred (e.g., operation would block and socket is non-blocking).
//    QAbstractSocket::UnknownSocketError	-1	An unidentified error occurred.

//qDebug() << "[DEBUG-onSocketError] ERROR:" << socketError;
    switch (socketError)
    {
    case QAbstractSocket::ConnectionRefusedError:
        emit ibSocketError("Connection Refused");
    default:
        emit ibSocketError("Network Socket Error");
    }
}

void IBQt::onManagedAccounts(const QByteArray &accountsList)
{
    Q_UNUSED(accountsList)
//    qDebug() << "UNLOCKING NOW";
    m_lock = false;
}

QTcpSocket *IBQt::getSocket() const
{
    return m_socket;
}




void IBQt::decodeField(int &value)
{
    value = decodeField().toInt();
}

void IBQt::decodeField(bool &value)
{
    value = (decodeField().toInt() ? 1 : 0);
}

void IBQt::decodeField(long &value)
{
    value = decodeField().toLong();
}

void IBQt::decodeField(double &value)
{
    value = decodeField().toDouble();
}

void IBQt::decodeField(QByteArray & value)
{
    value = decodeField();
}

QByteArray IBQt::decodeField()
{
    QByteArray ret;
    m_endIdx = m_inBuffer.indexOf('\0', m_begIdx);
    ret = m_inBuffer.mid(m_begIdx, m_endIdx - m_begIdx);
//    qDebug() << "[DEBUG-decodeField] field:" << ret << "m_inBuffer.size():" << m_inBuffer.size() << "m_endIdx:" << m_endIdx << "m_lastEndIdx:" << m_lastEndIdx;

    m_lastEndIdx = m_endIdx;
    m_begIdx = m_endIdx + 1;

//    qDebug() << "[DEBUG-decodeField]" << ret;

    return ret;
}

void IBQt::decodeFieldMax(int &value)
{
    QByteArray str;
    decodeField(str);
    value = (str.isEmpty() ? UNSET_INTEGER : str.toInt());
}

void IBQt::decodeFieldMax(long &value)
{
    QByteArray str;
    decodeField(str);
    value = (str.isEmpty() ? UNSET_INTEGER : str.toLong());
}

void IBQt::decodeFieldMax(double &value)
{
    QByteArray str;
    decodeField(str);
    value = (str.isEmpty() ? UNSET_DOUBLE : str.toDouble());
}

void IBQt::encodeField(const int &value)
{
    encodeField(QByteArray::number(value));
}

void IBQt::encodeField(const bool &value)
{
    encodeField(QByteArray::number((value == true ? 1 : 0)));
}

void IBQt::encodeField(const long &value)
{
    encodeField(QByteArray::number((int)value));
}

void IBQt::encodeField(const double &value)
{
    encodeField(QByteArray::number(value));
}

void IBQt::encodeField(const QByteArray &buf)
{
//    if (buf == "1")
//        qDebug() << "[DEBUG-encodeField]";
    m_debugBuffer.append(buf).append(" ");

    m_outBuffer.append(buf);
    m_outBuffer.append('\0');
}

void IBQt::encodeFieldMax(int value)
{
    if (value == INT_MAX) {
        QByteArray ba = "";
        encodeField(ba);
        return;
    }
    encodeField(value);
}

void IBQt::encodeFieldMax(double value)
{
    if (value == DBL_MAX) {
        QByteArray ba = "";
        encodeField(ba);
        return;
    }
    encodeField(value);
}

void IBQt::cleanInBuffer()
{
//    qDebug() << "[DEBUG-cleanInBuffer]";

    // m_lastEndIdx is used to fix continued parsing after raw data has actually ended

    if (m_endIdx == m_inBuffer.size() - 1 || m_endIdx < m_lastEndIdx) {
        m_inBuffer.clear();
        m_begIdx = m_endIdx = 0;
//        qDebug() << "[DEBUG-cleanInBuffer] CLEAN BUFFER";

    }
    else {
        m_inBuffer.remove(0, m_endIdx + 1);
        m_begIdx = 0;        
    }
    if (!m_inBuffer.isEmpty()) {
//        qDebug() << "[DEBUG-cleanInBuffer] remnants:" << m_inBuffer;
//        qDebug() << "[DEBUG-cleanInBuffer] remnants.. m_begIdx:" << m_begIdx << "m_endIdx:" << m_endIdx << "m_inBuffer.size():" << m_inBuffer.size();

    }
}

