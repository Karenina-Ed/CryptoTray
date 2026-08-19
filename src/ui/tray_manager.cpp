#include "tray_manager.h"

#include "taskbar_ticker_widget.h"

TrayManager::TrayManager(QObject* parent)
    : QObject(parent)
    , taskbarWidget_(new TaskbarTickerWidget())
{
}

TrayManager::~TrayManager()
{
    delete taskbarWidget_;
}

void TrayManager::updateTicker(const Ticker& ticker)
{
    taskbarWidget_->updateTicker(ticker);
}

void TrayManager::setConnected(bool connected)
{
    taskbarWidget_->setConnected(connected);
}

void TrayManager::setConnectionError(const QString& message)
{
    taskbarWidget_->setConnectionError(message);
}

void TrayManager::show()
{
    taskbarWidget_->showInTaskbar();
}
