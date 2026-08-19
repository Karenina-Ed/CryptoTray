#pragma once

#include "market/ticker.h"

#include <QHash>
#include <QWidget>

#include <memory>

class QLabel;
class QTimer;
class QCloseEvent;
class QMouseEvent;
class MarketDetailCard;

class TaskbarTickerWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit TaskbarTickerWidget(QWidget* parent = nullptr);
    ~TaskbarTickerWidget() override;

    bool showInTaskbar();
    void updateTicker(const Ticker& ticker);
    void setConnected(bool connected);
    void setConnectionError(const QString& message);

signals:
    void embedStateChanged(bool embedded);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    struct RowWidgets
    {
        QLabel* symbol = nullptr;
        QLabel* price = nullptr;
        QLabel* change = nullptr;
    };

    bool embedIntoTaskbar();
    void refreshDisplay();
    void toggleDetailCard();

    RowWidgets btcRow_;
    RowWidgets ethRow_;
    QLabel* statusDot_ = nullptr;
    QTimer* attachTimer_ = nullptr;
    // 卡片必须是无 QWidget 父对象的顶层窗口，因此由任务栏组件显式独占其生命周期。
    std::unique_ptr<MarketDetailCard> detailCard_;
    QHash<QString, Ticker> tickers_;
    bool connected_ = false;
    bool embedded_ = false;
    QString connectionError_;
};
