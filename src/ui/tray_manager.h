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
    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu* trayMenu_ = nullptr;
};
