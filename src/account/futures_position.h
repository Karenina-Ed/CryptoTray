#pragma once

#include <QList>
#include <QString>
#include <QStringList>

enum class FuturesMarket
{
    UsdMargined,
    CoinMargined,
    Options
};

// 账户层只向 UI 暴露规范化持仓，避免界面依赖不同衍生品接口的字段差异。
struct FuturesPosition
{
    FuturesMarket market = FuturesMarket::UsdMargined;
    QString symbol;
    QString side;
    QString marginType;
    QString profitAsset;
    QString optionSide;
    double amount = 0.0;
    double entryPrice = 0.0;
    double markPrice = 0.0;
    double strikePrice = 0.0;
    double unrealizedProfit = 0.0;
    double liquidationPrice = 0.0;
    qint64 expiryDate = 0;
    int leverage = 0;
};

using FuturesPositions = QList<FuturesPosition>;

struct CoinAccountAsset
{
    QString asset;
    double marginBalance = 0.0;
    double unrealizedProfit = 0.0;
};

// 总资产是多个账户权益按公开现货价折算后的估值；原始币本位余额仍保留给 UI 展示。
struct FuturesAccountOverview
{
    double usdMarginBalance = 0.0;
    double usdUnrealizedProfit = 0.0;
    double spotEstimatedUsdt = 0.0;
    double coinEstimatedUsdt = 0.0;
    double optionEstimatedUsdt = 0.0;
    double earnFlexibleEstimatedUsdt = 0.0;
    double earnLockedEstimatedUsdt = 0.0;
    double earnEstimatedUsdt = 0.0;
    double estimatedTotalUsdt = 0.0;
    QList<CoinAccountAsset> coinAssets;
    QStringList unpricedAssets;
    bool valuationAvailable = false;
    bool valuationComplete = false;
    bool earnValuationAvailable = false;
    bool earnValuationComplete = false;
};
