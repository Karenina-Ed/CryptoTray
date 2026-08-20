#pragma once

#include "account/futures_position.h"
#include "market/ticker.h"

#include <QObject>
#include <QStringList>

class TaskbarTickerWidget;

class TrayManager final : public QObject
{
    Q_OBJECT

public:
    explicit TrayManager(QObject* parent = nullptr);
    ~TrayManager() override;

    void show();
    void updateTicker(const Ticker& ticker);
    void setConnected(bool connected);
    void setConnectionError(const QString& message);
    void setPositions(const FuturesPositions& positions);
    void setAccountOverview(const FuturesAccountOverview& overview);
    void setAccountState(bool configured, const QString& message);
    void setWatchlist(const QStringList& symbols);
    void setAvailableSymbols(const QStringList& symbols);
    void setCnyRate(double cnyPerUsdt);

signals:
    void credentialsSaveRequested(const QString& apiKey, const QString& secretKey);
    void credentialsDeleteRequested();
    void watchlistChangeRequested(const QStringList& symbols);

private:
    // 长条组件是唯一界面，右键菜单和退出操作由组件自身负责。
    TaskbarTickerWidget* taskbarWidget_ = nullptr;
};
