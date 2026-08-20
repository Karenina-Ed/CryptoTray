#pragma once

#include "account/futures_position.h"
#include "market/ticker.h"

#include <QHash>
#include <QStringList>
#include <QWidget>

class QLabel;
class QFrame;
class QHBoxLayout;
class QComboBox;
class QCompleter;
class QLineEdit;
class QScrollArea;
class QStackedWidget;
class QToolButton;
class QVBoxLayout;

// 详情卡片是独立顶层窗口，避免跟随任务栏子窗口一起被 Explorer 裁剪。
class MarketDetailCard final : public QWidget
{
    Q_OBJECT

public:
    explicit MarketDetailCard(QWidget* parent = nullptr);

    void updateTicker(const Ticker& ticker);
    void setConnectionState(bool connected, const QString& errorMessage = {});
    void setPositions(const FuturesPositions& positions);
    void setAccountOverview(const FuturesAccountOverview& overview);
    void setAccountState(bool configured, const QString& message);
    void setWatchlist(const QStringList& symbols);
    void setAvailableSymbols(const QStringList& symbols);
    void setCnyRate(double cnyPerUsdt);

signals:
    void watchlistChangeRequested(const QStringList& symbols);

private:
    struct WatchRow
    {
        QLabel* price = nullptr;
        QLabel* change = nullptr;
        QLabel* volume = nullptr;
    };

    void addWatchlistSymbol();
    void rebuildWatchlist();
    void refreshWatchRow(const QString& symbol);
    void resizeForCurrentPage();
    void refreshAccountOverview();
    void refreshPositions();
    void refreshTotalAsset();
    QString formatValuation(double usdtValue) const;

    QLineEdit* marketSearch_ = nullptr;
    QCompleter* marketCompleter_ = nullptr;
    QLabel* marketHint_ = nullptr;
    QScrollArea* marketScroll_ = nullptr;
    QVBoxLayout* marketRowsLayout_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    QComboBox* valuationUnit_ = nullptr;
    QLabel* accountStatus_ = nullptr;
    QLabel* totalBalance_ = nullptr;
    QToolButton* visibilityButton_ = nullptr;
    QLabel* usdBalance_ = nullptr;
    QLabel* usdPnl_ = nullptr;
    QLabel* coinBalance_ = nullptr;
    QLabel* coinPnl_ = nullptr;
    QLabel* earnBalance_ = nullptr;
    QLabel* earnDetail_ = nullptr;
    QLabel* allocationText_ = nullptr;
    QHBoxLayout* allocationLayout_ = nullptr;
    QFrame* usdAllocationSegment_ = nullptr;
    QFrame* coinAllocationSegment_ = nullptr;
    QFrame* earnAllocationSegment_ = nullptr;
    QFrame* otherAllocationSegment_ = nullptr;
    QVBoxLayout* usdPositionsLayout_ = nullptr;
    QVBoxLayout* coinPositionsLayout_ = nullptr;
    QVBoxLayout* optionPositionsLayout_ = nullptr;
    QHash<QString, WatchRow> watchRows_;
    QHash<QString, Ticker> tickers_;
    QStringList watchlist_;
    QStringList availableSymbols_;
    FuturesPositions positions_;
    FuturesAccountOverview accountOverview_;
    int chromeHeight_ = 0;
    double cnyPerUsdt_ = 0.0;
    bool connected_ = false;
    bool totalAssetVisible_ = true;
};
