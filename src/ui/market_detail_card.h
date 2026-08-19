#pragma once

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

    CoinWidgets btcWidgets_;
    CoinWidgets ethWidgets_;
    QLabel* connectionBadge_ = nullptr;
    QLabel* footer_ = nullptr;
    QHash<QString, Ticker> tickers_;
    bool connected_ = false;
    QString errorMessage_;
};
