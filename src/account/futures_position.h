#pragma once

#include <QList>
#include <QString>

enum class FuturesMarket
{
    UsdMargined,
    CoinMargined
};

// 账户层只向 UI 暴露规范化持仓，避免界面依赖两个 Binance 接口的字段差异。
struct FuturesPosition
{
    FuturesMarket market = FuturesMarket::UsdMargined;
    QString symbol;
    QString side;
    QString marginType;
    QString profitAsset;
    double amount = 0.0;
    double entryPrice = 0.0;
    double markPrice = 0.0;
    double unrealizedProfit = 0.0;
    double liquidationPrice = 0.0;
    int leverage = 0;
};

using FuturesPositions = QList<FuturesPosition>;

struct CoinAccountAsset
{
    QString asset;
    double marginBalance = 0.0;
    double unrealizedProfit = 0.0;
};

// U 本位可按美元口径汇总；币本位资产单位不同，必须保留逐币种余额。
struct FuturesAccountOverview
{
    double usdMarginBalance = 0.0;
    double usdUnrealizedProfit = 0.0;
    QList<CoinAccountAsset> coinAssets;
};
