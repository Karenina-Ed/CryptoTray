#include "market_message_parser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLoggingCategory>

#include <cmath>

namespace
{
bool parseNumber(const QJsonObject& object, const QString& key, double& value)
{
    // Binance 将价格和成交量编码为字符串；任何缺失、非数字或非有限值都拒绝。
    const QJsonValue jsonValue = object.value(key);
    if(!jsonValue.isString())
    {
        return false;
    }

    bool ok = false;
    const double parsed = jsonValue.toString().toDouble(&ok);
    if(!ok || !std::isfinite(parsed))
    {
        return false;
    }

    value = parsed;
    return true;
}
}

std::optional<Ticker> parseMarketMessage(const QString& message)
{
    // 解析器采用“整条消息有效才输出”的策略，防止部分旧数据污染行情状态。
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(message.toUtf8(), &error);
    if(error.error != QJsonParseError::NoError || !document.isObject())
    {
        qWarning() << "[Market] ignored invalid JSON message:" << error.errorString();
        return std::nullopt;
    }

    QJsonObject object = document.object();
    // 合约 market/stream 使用 combined stream 包装，真实行情位于 data 对象中。
    const QJsonValue combinedData = object.value(QStringLiteral("data"));
    if(combinedData.isObject())
    {
        object = combinedData.toObject();
    }
    // UTC 日线消息把价格字段放在 k 对象中，事件时间仍位于外层。
    const QJsonValue eventType = object.value(QStringLiteral("e"));
    if(eventType == QStringLiteral("kline"))
    {
        const QJsonValue klineValue = object.value(QStringLiteral("k"));
        if(!klineValue.isObject())
        {
            qWarning() << "[Market] ignored kline without payload";
            return std::nullopt;
        }
        QJsonObject kline = klineValue.toObject();
        kline.insert(QStringLiteral("E"), object.value(QStringLiteral("E")));
        object = kline;
    }

    const QJsonValue symbolValue = object.value(QStringLiteral("s"));
    const QJsonValue eventTimeValue = object.value(QStringLiteral("E"));
    if(!symbolValue.isString() || symbolValue.toString().isEmpty() || !eventTimeValue.isDouble())
    {
        qWarning() << "[Market] ignored message with missing ticker fields";
        return std::nullopt;
    }

    Ticker ticker;
    ticker.symbol = symbolValue.toString().toUpper();
    ticker.eventTime = eventTimeValue.toInteger();

    if(!parseNumber(object, QStringLiteral("c"), ticker.price)
       || !parseNumber(object, QStringLiteral("o"), ticker.openPrice)
       || !parseNumber(object, QStringLiteral("h"), ticker.highPrice)
       || !parseNumber(object, QStringLiteral("l"), ticker.lowPrice)
       || !parseNumber(object, QStringLiteral("v"), ticker.volume))
    {
        qWarning() << "[Market] ignored ticker with invalid numeric fields";
        return std::nullopt;
    }

    ticker.changePercent = ticker.openPrice != 0.0
        ? (ticker.price - ticker.openPrice) / ticker.openPrice * 100.0
        : 0.0;

    return ticker;
}
