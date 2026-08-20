#include "tray_manager.h"

#include "taskbar_ticker_widget.h"

TrayManager::TrayManager(QObject* parent)
    : QObject(parent)
    , taskbarWidget_(new TaskbarTickerWidget())
{
    connect(taskbarWidget_, &TaskbarTickerWidget::credentialsSaveRequested,
            this, &TrayManager::credentialsSaveRequested);
    connect(taskbarWidget_, &TaskbarTickerWidget::credentialsDeleteRequested,
            this, &TrayManager::credentialsDeleteRequested);
    connect(taskbarWidget_, &TaskbarTickerWidget::watchlistChangeRequested,
            this, &TrayManager::watchlistChangeRequested);
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

void TrayManager::setPositions(const FuturesPositions& positions)
{
    taskbarWidget_->setPositions(positions);
}

void TrayManager::setAccountOverview(const FuturesAccountOverview& overview)
{
    taskbarWidget_->setAccountOverview(overview);
}

void TrayManager::setAccountState(bool configured, const QString& message)
{
    taskbarWidget_->setAccountState(configured, message);
}

void TrayManager::setWatchlist(const QStringList& symbols)
{
    taskbarWidget_->setWatchlist(symbols);
}

void TrayManager::setAvailableSymbols(const QStringList& symbols)
{
    taskbarWidget_->setAvailableSymbols(symbols);
}

void TrayManager::setCnyRate(double cnyPerUsdt)
{
    taskbarWidget_->setCnyRate(cnyPerUsdt);
}

void TrayManager::show()
{
    taskbarWidget_->showInTaskbar();
}
