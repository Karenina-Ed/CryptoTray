#pragma once

#include "ticker.h"

#include <QObject>
#include <QSet>
#include <QTimer>
#include <QWebSocket>

class MarketDataService final : public QObject
{
    Q_OBJECT

public:
    explicit MarketDataService(QObject* parent = nullptr);

    void start();
    void stop();

signals:
    void tickerUpdated(const Ticker& ticker);
    void connected();
    void disconnected();
    void connectionStateChanged(bool connected);
    void connectionError(const QString& message);

private:
    // 服务只负责网络、解析和信号输出，不持有或操作任何 QWidget。
    void connectToServer();
    void subscribe();
    void scheduleReconnect();
    void handleTextMessage(const QString& message);

    QWebSocket websocket_;
    QTimer reconnectTimer_;
    int reconnectDelayMs_ = 2000;
    // started_ 区分主动 stop() 与意外断线，主动停止后不再安排重连。
    bool started_ = false;
    // 每个 symbol 只记录第一条有效行情，用于验证数据链且避免长期刷屏。
    QSet<QString> loggedSymbols_;
};
