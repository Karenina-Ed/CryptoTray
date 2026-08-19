#include "market_data_service.h"

#include "market_message_parser.h"

#include <QAbstractSocket>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkRequest>
#include <QUrl>

#include <algorithm>

MarketDataService::MarketDataService(QObject* parent)
    : QObject(parent)
{
    // 单次定时器配合 isActive() 检查，保证同一时刻最多只有一个重连任务。
    reconnectTimer_.setSingleShot(true);

    connect(&reconnectTimer_, &QTimer::timeout, this, &MarketDataService::connectToServer);
    connect(&websocket_, &QWebSocket::connected, this, [this]() {
        qInfo() << "[Market] connected";
        reconnectTimer_.stop();
        reconnectDelayMs_ = 2000;
        subscribe();
        emit connected();
        emit connectionStateChanged(true);
    });
    connect(&websocket_, &QWebSocket::disconnected, this, [this]() {
        qInfo() << "[Market] disconnected";
        emit disconnected();
        emit connectionStateChanged(false);
        scheduleReconnect();
    });
    connect(&websocket_, &QWebSocket::textMessageReceived,
            this, &MarketDataService::handleTextMessage);
    connect(&websocket_, qOverload<QAbstractSocket::SocketError>(&QWebSocket::error), this,
            [this](QAbstractSocket::SocketError) {
                qWarning() << "[Market] WebSocket error:" << websocket_.errorString();
                emit connectionError(websocket_.errorString());
                scheduleReconnect();
            });
}

void MarketDataService::start()
{
    if(started_)
    {
        return;
    }

    started_ = true;
    qInfo() << "[Market] start requested";
    reconnectDelayMs_ = 2000;
    loggedSymbols_.clear();
    connectToServer();
}

void MarketDataService::stop()
{
    started_ = false;
    reconnectTimer_.stop();
    websocket_.close();
}

void MarketDataService::connectToServer()
{
    qInfo() << "[Market] connectToServer state=" << websocket_.state()
            << "started=" << started_;
    if(!started_ || websocket_.state() != QAbstractSocket::UnconnectedState)
    {
        return;
    }

    qInfo() << "[Market] connecting to Binance USD-M perpetual futures";
    // fstream 的 BTCUSDT、ETHUSDT 对应 U 本位永续合约，日线边界使用 UTC。
    websocket_.open(QNetworkRequest(QUrl(QStringLiteral("wss://fstream.binance.com/market/stream"))));
}

void MarketDataService::subscribe()
{
    // 使用一个 market/stream 连接订阅两个永续合约；新协议要求请求 ID 为字符串。
    QJsonObject request;
    request.insert(QStringLiteral("method"), QStringLiteral("SUBSCRIBE"));
    request.insert(QStringLiteral("params"),
                   QJsonArray{QStringLiteral("btcusdt@kline_1d"),
                              QStringLiteral("ethusdt@kline_1d")});
    request.insert(QStringLiteral("id"), QStringLiteral("cryptotray-market"));

    websocket_.sendTextMessage(QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact)));
    qInfo() << "[Market] subscribed BTCUSDT ETHUSDT perpetual UTC daily klines";
}

void MarketDataService::scheduleReconnect()
{
    if(!started_ || reconnectTimer_.isActive()
       || websocket_.state() != QAbstractSocket::UnconnectedState)
    {
        return;
    }

    const int delayMs = reconnectDelayMs_;
    // 指数退避序列为 2、4、8、16、30 秒；连接成功后恢复为 2 秒。
    reconnectDelayMs_ = std::min(reconnectDelayMs_ * 2, 30000);
    qInfo() << "[Market] reconnect in" << delayMs << "ms";
    reconnectTimer_.start(delayMs);
}

void MarketDataService::handleTextMessage(const QString& message)
{
    const std::optional<Ticker> ticker = parseMarketMessage(message);
    if(ticker)
    {
        if(!loggedSymbols_.contains(ticker->symbol))
        {
            loggedSymbols_.insert(ticker->symbol);
            qInfo() << "[Market] received first ticker for" << ticker->symbol;
        }
        emit tickerUpdated(*ticker);
    }
    else if(message.contains(QStringLiteral("\"id\"")))
    {
        // 只记录订阅确认或错误响应，避免正常行情解析失败时刷屏。
        qInfo() << "[Market] subscription response:" << message.left(500);
    }
}
