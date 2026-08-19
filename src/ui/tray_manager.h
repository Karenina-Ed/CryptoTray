#pragma once

#include "market/ticker.h"

#include <QObject>

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

private:
    // 长条组件是唯一界面，右键菜单和退出操作由组件自身负责。
    TaskbarTickerWidget* taskbarWidget_ = nullptr;
};
