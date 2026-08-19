#pragma once

#include "account/futures_position.h"
#include "market/ticker.h"

#include <QHash>
#include <QWidget>

class QLabel;
class QVBoxLayout;

// 详情卡片是独立顶层窗口，避免跟随任务栏子窗口一起被 Explorer 裁剪。
class MarketDetailCard final : public QWidget
{
public:
    explicit MarketDetailCard(QWidget* parent = nullptr);

    void updateTicker(const Ticker& ticker);
    void setConnectionState(bool connected, const QString& errorMessage = {});
    void setPositions(const FuturesPositions& positions);
    void setAccountOverview(const FuturesAccountOverview& overview);
    void setAccountState(bool configured, const QString& message);

private:
    struct CoinWidgets
    {
        QLabel* price = nullptr;
        QLabel* change = nullptr;
        QLabel* open = nullptr;
        QLabel* high = nullptr;
        QLabel* low = nullptr;
        QLabel* volume = nullptr;
        QLabel* updatedAt = nullptr;
    };

    void addCoinSection(QVBoxLayout* layout, const QString& symbol, CoinWidgets& widgets);
    void refreshCoin(const QString& symbol, CoinWidgets& widgets);
    void refreshPositions();

    CoinWidgets btcWidgets_;
    CoinWidgets ethWidgets_;
    QLabel* accountStatus_ = nullptr;
    QLabel* usdBalance_ = nullptr;
    QLabel* usdPnl_ = nullptr;
    QLabel* coinBalance_ = nullptr;
    QLabel* coinPnl_ = nullptr;
    QVBoxLayout* usdPositionsLayout_ = nullptr;
    QVBoxLayout* coinPositionsLayout_ = nullptr;
    QHash<QString, Ticker> tickers_;
    FuturesPositions positions_;
    bool connected_ = false;
};
