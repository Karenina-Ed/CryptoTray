#include "market/market_data_service.h"
#include "ui/tray_manager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>
#include <QSystemTrayIcon>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("CryptoTray"));
    QCoreApplication::setApplicationName(QStringLiteral("CryptoTray"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    // 托盘程序关闭普通窗口时仍需继续运行，生命周期由托盘菜单控制。
    QApplication::setQuitOnLastWindowClosed(false);

    if(!QSystemTrayIcon::isSystemTrayAvailable())
    {
        QMessageBox::critical(nullptr,
                              QStringLiteral("CryptoTray"),
                              QStringLiteral("The Windows system tray is not available."));
        return 1;
    }

    TrayManager trayManager;
    trayManager.show();

    // Phase 2 只启动行情服务，不把行情数据直接交给托盘 UI。
    MarketDataService marketService;
    marketService.start();

    return app.exec();
}
