#pragma once

#include <QObject>

class QMenu;
class QSystemTrayIcon;

class TrayManager final : public QObject
{
    Q_OBJECT

public:
    explicit TrayManager(QObject* parent = nullptr);
    ~TrayManager() override;

    void show();

private:
    // 托盘图标由 QObject 父子关系释放；QMenu 是 QWidget，需在析构函数中释放。
    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu* trayMenu_ = nullptr;
};
