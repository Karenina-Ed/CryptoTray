#include "account/account_position_service.h"
#include "market/market_data_service.h"
#include "ui/tray_manager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>
#include <QTimer>

namespace
{
// GUI 子系统没有控制台，统一把 Qt 诊断信息写入临时目录，便于排查后台连接问题。
void applicationMessageHandler(QtMsgType type, const QMessageLogContext&, const QString& message)
{
    static QMutex mutex;
    const QMutexLocker locker(&mutex);
    QFile logFile(QDir::temp().filePath(QStringLiteral("CryptoTray.log")));
    if(!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        return;
    }

    const char* level = type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg
        ? "WARN" : "INFO";
    QTextStream stream(&logFile);
    stream << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
           << " [" << level << "] " << message << Qt::endl;
}
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    qInstallMessageHandler(applicationMessageHandler);

    QCoreApplication::setOrganizationName(QStringLiteral("CryptoTray"));
    QCoreApplication::setApplicationName(QStringLiteral("CryptoTray"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    // 托盘程序关闭普通窗口时仍需继续运行，生命周期由托盘菜单控制。
    QApplication::setQuitOnLastWindowClosed(false);

    // 避免旧实例和新实例在任务栏中重叠，并同时建立重复行情连接。
    QLockFile instanceLock(QDir::temp().filePath(QStringLiteral("CryptoTray.instance.lock")));
    instanceLock.setStaleLockTime(0);
    if(!instanceLock.tryLock(100))
    {
        // 异常崩溃会留下锁文件；Windows 会阻止删除仍由存活进程持有的锁，
        // 因此这里只恢复已确认无主的锁，不会绕过正常的单实例保护。
        const bool recovered = instanceLock.error() == QLockFile::LockFailedError
            && instanceLock.removeStaleLockFile() && instanceLock.tryLock(100);
        if(!recovered)
        {
            return 0;
        }
    }

    TrayManager trayManager;

    // 行情服务与界面通过信号连接，网络层不直接持有或操作 QWidget。
    MarketDataService marketService;
    AccountPositionService accountService;
    QObject::connect(&marketService, &MarketDataService::tickerUpdated,
                     &trayManager, &TrayManager::updateTicker);
    QObject::connect(&marketService, &MarketDataService::connectionStateChanged,
                     &trayManager, &TrayManager::setConnected);
    QObject::connect(&marketService, &MarketDataService::connectionError,
                     &trayManager, &TrayManager::setConnectionError);
    QObject::connect(&trayManager, &TrayManager::watchlistChangeRequested,
                     &marketService, &MarketDataService::setWatchlist);
    QObject::connect(&marketService, &MarketDataService::watchlistChanged,
                     &trayManager, &TrayManager::setWatchlist);
    QObject::connect(&marketService, &MarketDataService::availableSymbolsChanged,
                     &trayManager, &TrayManager::setAvailableSymbols);
    QObject::connect(&marketService, &MarketDataService::cnyRateUpdated,
                     &trayManager, &TrayManager::setCnyRate);
    QObject::connect(&accountService, &AccountPositionService::positionsUpdated,
                     &trayManager, &TrayManager::setPositions);
    QObject::connect(&accountService, &AccountPositionService::accountOverviewUpdated,
                     &trayManager, &TrayManager::setAccountOverview);
    QObject::connect(&accountService, &AccountPositionService::accountStateChanged,
                     &trayManager, &TrayManager::setAccountState);
    QObject::connect(&trayManager, &TrayManager::credentialsSaveRequested,
                     &accountService, &AccountPositionService::saveCredentials);
    QObject::connect(&trayManager, &TrayManager::credentialsDeleteRequested,
                     &accountService, &AccountPositionService::deleteCredentials);
    trayManager.setWatchlist(marketService.watchlist());
    trayManager.show();
    // 任务栏子窗口完成挂载且事件循环启动后再建立连接，避免初始化阶段丢失异步套接字事件。
    QTimer::singleShot(0, &app, [&marketService, &accountService]() {
        qInfo() << "[App] event loop started; starting services";
        marketService.start();
        accountService.start();
    });

    return app.exec();
}
