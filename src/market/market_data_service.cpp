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
    if(!started_ || websocket_.state() != QAbstractSocket::UnconnectedState)
    {
        return;
    }

    qInfo() << "[Market] connecting to Binance";
    websocket_.open(QNetworkRequest(QUrl(QStringLiteral("wss://stream.binance.com:9443/ws"))));
}

void MarketDataService::subscribe()
{
    // 使用一个 WebSocket 连接订阅多个小写 stream，响应中的 symbol 仍为大写。
    QJsonObject request;
    request.insert(QStringLiteral("method"), QStringLiteral("SUBSCRIBE"));
    request.insert(QStringLiteral("params"),
                   QJsonArray{QStringLiteral("btcusdt@miniTicker"),
                              QStringLiteral("ethusdt@miniTicker")});
    request.insert(QStringLiteral("id"), 1);

    websocket_.sendTextMessage(QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact)));
    qInfo() << "[Market] subscribed BTCUSDT ETHUSDT";
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
}
