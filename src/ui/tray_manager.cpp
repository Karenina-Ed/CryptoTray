#include "tray_manager.h"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QFont>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>

namespace
{
QIcon createTrayIcon()
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#F7931A")));
    painter.drawEllipse(2, 2, 60, 60);

    QFont font(QStringLiteral("Segoe UI"));
    font.setPixelSize(39);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("C"));

    return QIcon(pixmap);
}
}

TrayManager::TrayManager(QObject* parent)
    : QObject(parent)
    , trayIcon_(new QSystemTrayIcon(createTrayIcon(), this))
    , trayMenu_(new QMenu())
{
    QAction* showMarketAction = trayMenu_->addAction(QStringLiteral("显示行情"));
    showMarketAction->setEnabled(false);
    showMarketAction->setToolTip(QStringLiteral("行情窗口将在后续阶段实现"));

    QAction* exitAction = trayMenu_->addAction(QStringLiteral("退出"));
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);

    trayIcon_->setContextMenu(trayMenu_);
    trayIcon_->setToolTip(QStringLiteral("CryptoTray"));
}

TrayManager::~TrayManager()
{
    delete trayMenu_;
}

void TrayManager::show()
{
    trayIcon_->show();
}
