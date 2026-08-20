#include "market_data_service.h"

#include "market_message_parser.h"

#include <QAbstractSocket>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QXmlStreamReader>

#include <algorithm>

MarketDataService::MarketDataService(QObject* parent)
    : QObject(parent)
{
    QSettings settings;
    const QStringList savedWatchlist = settings.contains(QStringLiteral("market/watchlist"))
        ? settings.value(QStringLiteral("market/watchlist")).toStringList()
        : QStringList{QStringLiteral("BTCUSDT"), QStringLiteral("ETHUSDT")};
    setWatchlist(savedWatchlist);

    // 单次定时器配合 isActive() 检查，保证同一时刻最多只有一个重连任务。
    reconnectTimer_.setSingleShot(true);

    connect(&reconnectTimer_, &QTimer::timeout, this, &MarketDataService::connectToServer);
    connect(&websocket_, &QWebSocket::connected, this, [this]() {
        qInfo() << "[Market] connected";
        reconnectTimer_.stop();
        reconnectDelayMs_ = 2000;
        sendSubscription(QStringLiteral("SUBSCRIBE"), subscriptionSymbols(watchlist_));
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
    fetchExchangeInfo();
    fetchCnyRate();
    connectToServer();
}

void MarketDataService::stop()
{
    started_ = false;
    reconnectTimer_.stop();
    websocket_.close();
}

QStringList MarketDataService::watchlist() const
{
    return watchlist_;
}

void MarketDataService::setWatchlist(const QStringList& symbols)
{
    QStringList normalized;
    for(QString symbol : symbols)
    {
        symbol = symbol.trimmed().toUpper();
        if(!symbol.isEmpty() && !normalized.contains(symbol))
        {
            normalized.append(symbol);
        }
        if(normalized.size() == 20)
        {
            break;
        }
    }
    if(normalized == watchlist_)
    {
        return;
    }

    const QStringList previousSubscriptions = subscriptionSymbols(watchlist_);
    watchlist_ = normalized;
    QSettings().setValue(QStringLiteral("market/watchlist"), watchlist_);
    emit watchlistChanged(watchlist_);

    if(websocket_.state() == QAbstractSocket::ConnectedState)
    {
        const QStringList nextSubscriptions = subscriptionSymbols(watchlist_);
        QStringList removed;
        QStringList added;
        for(const QString& symbol : previousSubscriptions)
        {
            if(!nextSubscriptions.contains(symbol))
            {
                removed.append(symbol);
            }
        }
        for(const QString& symbol : nextSubscriptions)
        {
            if(!previousSubscriptions.contains(symbol))
            {
                added.append(symbol);
            }
        }
        sendSubscription(QStringLiteral("UNSUBSCRIBE"), removed);
        sendSubscription(QStringLiteral("SUBSCRIBE"), added);
    }
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

void MarketDataService::sendSubscription(const QString& method, const QStringList& symbols)
{
    if(symbols.isEmpty())
    {
        return;
    }

    QJsonArray streams;
    for(const QString& symbol : symbols)
    {
        streams.append(symbol.toLower() + QStringLiteral("@kline_1d"));
    }
    QJsonObject request;
    request.insert(QStringLiteral("method"), method);
    request.insert(QStringLiteral("params"), streams);
    request.insert(QStringLiteral("id"), QStringLiteral("cryptotray-%1").arg(method.toLower()));

    websocket_.sendTextMessage(QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact)));
    qInfo() << "[Market]" << method << symbols << "perpetual UTC daily klines";
}

QStringList MarketDataService::subscriptionSymbols(const QStringList& watchlist) const
{
    // 任务栏始终显示 BTC、ETH；即使用户从自选中移除，也保留这两个基础订阅。
    QStringList symbols{QStringLiteral("BTCUSDT"), QStringLiteral("ETHUSDT")};
    for(const QString& symbol : watchlist)
    {
        if(!symbols.contains(symbol))
        {
            symbols.append(symbol);
        }
    }
    return symbols;
}

void MarketDataService::fetchExchangeInfo()
{
    QNetworkReply* reply = network_.get(QNetworkRequest(
        QUrl(QStringLiteral("https://fapi.binance.com/fapi/v1/exchangeInfo"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if(reply->error() == QNetworkReply::NoError)
        {
            const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
            QStringList symbols;
            for(const QJsonValue& value : document.object().value(QStringLiteral("symbols")).toArray())
            {
                const QJsonObject symbol = value.toObject();
                if(symbol.value(QStringLiteral("contractType")).toString() == QStringLiteral("PERPETUAL")
                   && symbol.value(QStringLiteral("status")).toString() == QStringLiteral("TRADING")
                   && symbol.value(QStringLiteral("quoteAsset")).toString() == QStringLiteral("USDT"))
                {
                    symbols.append(symbol.value(QStringLiteral("symbol")).toString());
                }
            }
            symbols.sort(Qt::CaseInsensitive);
            emit availableSymbolsChanged(symbols);
        }
        else
        {
            qWarning() << "[Market] exchange info error:" << reply->errorString();
        }
        reply->deleteLater();
    });
}

void MarketDataService::fetchCnyRate()
{
    QNetworkReply* reply = network_.get(QNetworkRequest(QUrl(
        QStringLiteral("https://www.ecb.europa.eu/stats/eurofxref/eurofxref-daily.xml"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if(reply->error() == QNetworkReply::NoError)
        {
            QXmlStreamReader xml(reply->readAll());
            double usdPerEur = 0.0;
            double cnyPerEur = 0.0;
            while(!xml.atEnd())
            {
                xml.readNext();
                if(!xml.isStartElement() || xml.name() != QStringLiteral("Cube"))
                {
                    continue;
                }
                const auto attributes = xml.attributes();
                const QString currency = attributes.value(QStringLiteral("currency")).toString();
                const double rate = attributes.value(QStringLiteral("rate")).toDouble();
                if(currency == QStringLiteral("USD"))
                {
                    usdPerEur = rate;
                }
                else if(currency == QStringLiteral("CNY"))
                {
                    cnyPerEur = rate;
                }
            }
            if(!xml.hasError() && usdPerEur > 0.0 && cnyPerEur > 0.0)
            {
                // USDT 计价近似按 1 USDT = 1 USD，再用 ECB 的交叉汇率换算人民币。
                emit cnyRateUpdated(cnyPerEur / usdPerEur);
            }
        }
        else
        {
            qWarning() << "[Market] ECB rate error:" << reply->errorString();
        }
        reply->deleteLater();
    });
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
