#pragma once

#include <QMetaType>
#include <QString>

struct Ticker
{
    // Ticker 是 market 层向外传递的轻量快照，UI 不再重复解析 Binance JSON。
    QString symbol;

    double price = 0.0;
    double openPrice = 0.0;
    double highPrice = 0.0;
    double lowPrice = 0.0;
    double volume = 0.0;

    double changePercent = 0.0;

    qint64 eventTime = 0;
};

Q_DECLARE_METATYPE(Ticker)
