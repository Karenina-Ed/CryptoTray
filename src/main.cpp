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

    return app.exec();
}

