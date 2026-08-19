#include "taskbar_ticker_widget.h"

#include "market_detail_card.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QScreen>
#include <QStyle>
#include <QTimer>
#include <QHBoxLayout>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace
{
QString formatPrice(double price)
{
    return QString::number(price, 'f', price >= 1000.0 ? 2 : 4);
}

QString formatChange(double change)
{
    return QStringLiteral("%1%2%")
        .arg(change >= 0.0 ? QStringLiteral("+") : QString())
        .arg(change, 0, 'f', 2);
}
}

TaskbarTickerWidget::TaskbarTickerWidget(QWidget* parent)
    : QWidget(parent)
    , attachTimer_(new QTimer(this))
    , detailCard_(std::make_unique<MarketDetailCard>())
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_TranslucentBackground);
    setObjectName(QStringLiteral("taskbarTicker"));
    setCursor(Qt::PointingHandCursor);
    // 两组行情保持单行展示，同时避免占用过多任务栏空间。
    setFixedWidth(310);
    setStyleSheet(QStringLiteral(
        // 1/255 的透明度肉眼不可见，但能让 Windows 将空白区域纳入鼠标命中范围。
        "#taskbarTicker { background: rgba(0, 0, 0, 1); border: none; }"
        "QLabel { font-family: 'Segoe UI Variable Text', 'Segoe UI'; }"
        "QLabel[role='symbol'] { color: #aeb6c7; font-size: 11px; font-weight: 600; }"
        "QLabel[role='price'] { color: #ffffff; font-size: 14px; font-weight: 700; }"
        "QLabel[trend='up'] { color: #17c964; font-size: 11px; font-weight: 600; }"
        "QLabel[trend='down'] { color: #f04444; font-size: 11px; font-weight: 600; }"
        "QLabel[trend='flat'] { color: #929292; font-size: 11px; font-weight: 600; }"));

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(4, 3, 4, 3);
    root->setSpacing(3);
    statusDot_ = new QLabel(QStringLiteral("●"), this);
    statusDot_->setAttribute(Qt::WA_TransparentForMouseEvents);
    statusDot_->setFixedWidth(7);
    root->addWidget(statusDot_);

    const auto addGroup = [this, root](const QString& symbol, RowWidgets& row) {
        row.symbol = new QLabel(symbol, this);
        row.symbol->setAttribute(Qt::WA_TransparentForMouseEvents);
        row.symbol->setProperty("role", "symbol");
        row.symbol->setFixedWidth(22);
        row.price = new QLabel(QStringLiteral("连接中"), this);
        row.price->setAttribute(Qt::WA_TransparentForMouseEvents);
        row.price->setProperty("role", "price");
        row.price->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row.price->setFixedWidth(64);
        row.change = new QLabel(QStringLiteral("--"), this);
        row.change->setAttribute(Qt::WA_TransparentForMouseEvents);
        row.change->setProperty("trend", "flat");
        row.change->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row.change->setFixedWidth(45);
        root->addWidget(row.symbol);
        root->addWidget(row.price);
        root->addWidget(row.change);
    };
    addGroup(QStringLiteral("BTC"), btcRow_);
    auto* separator = new QLabel(QStringLiteral("│"), this);
    separator->setAttribute(Qt::WA_TransparentForMouseEvents);
    separator->setStyleSheet(QStringLiteral("color: #3c4353;"));
    root->addWidget(separator);
    addGroup(QStringLiteral("ETH"), ethRow_);

    attachTimer_->setInterval(2000);
    connect(attachTimer_, &QTimer::timeout, this, [this]() {
        const bool nowEmbedded = embedIntoTaskbar();
        if(nowEmbedded != embedded_)
        {
            embedded_ = nowEmbedded;
            emit embedStateChanged(embedded_);
        }
    });
    refreshDisplay();
}

TaskbarTickerWidget::~TaskbarTickerWidget() = default;

bool TaskbarTickerWidget::showInTaskbar()
{
    show();
    embedded_ = embedIntoTaskbar();
    qInfo() << "[UI] taskbar embedded=" << embedded_ << "size=" << size();
    attachTimer_->start();
    emit embedStateChanged(embedded_);
    return embedded_;
}

void TaskbarTickerWidget::updateTicker(const Ticker& ticker)
{
    const bool firstTicker = !tickers_.contains(ticker.symbol);
    tickers_.insert(ticker.symbol, ticker);
    detailCard_->updateTicker(ticker);
    refreshDisplay();
    if(firstTicker)
    {
        qInfo() << "[UI] rendered first ticker for" << ticker.symbol
                << "price=" << ticker.price << "change=" << ticker.changePercent;
    }
}

void TaskbarTickerWidget::setConnected(bool connected)
{
    connected_ = connected;
    if(connected)
    {
        connectionError_.clear();
    }
    detailCard_->setConnectionState(connected, connectionError_);
    refreshDisplay();
}

void TaskbarTickerWidget::setConnectionError(const QString& message)
{
    connectionError_ = message;
    detailCard_->setConnectionState(connected_, connectionError_);
    refreshDisplay();
}

void TaskbarTickerWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background: #111111; color: white; border: 1px solid #2b2b2b; padding: 6px; }"
        "QMenu::item { padding: 7px 26px; border-radius: 4px; }"
        "QMenu::item:selected { background: #242424; }"));
    QAction* exitAction = menu.addAction(QStringLiteral("退出 CryptoTray"));
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);
    menu.exec(event->globalPos());
}

void TaskbarTickerWidget::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton)
    {
        toggleDetailCard();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void TaskbarTickerWidget::closeEvent(QCloseEvent* event)
{
    // 任务栏组件是主界面，Alt+F4 或 Explorer 请求关闭时应结束整个后台进程。
    event->accept();
    qApp->quit();
}

bool TaskbarTickerWidget::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    auto* nativeMessage = static_cast<MSG*>(message);
    if(nativeMessage != nullptr && nativeMessage->message == WM_NCHITTEST)
    {
        // 透明任务栏子窗口也必须把完整矩形报告为客户端区域，而不只是有文字的像素。
        *result = HTCLIENT;
        return true;
    }
#endif
    return QWidget::nativeEvent(eventType, message, result);
}

bool TaskbarTickerWidget::embedIntoTaskbar()
{
#ifdef Q_OS_WIN
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    HWND window = reinterpret_cast<HWND>(winId());
    if(taskbar == nullptr || window == nullptr)
    {
        qWarning() << "[UI] taskbar/window handle missing"
                   << "taskbar=" << taskbar << "window=" << window
                   << "error=" << GetLastError();
        return false;
    }

    RECT taskbarRect{};
    if(!GetClientRect(taskbar, &taskbarRect))
    {
        qWarning() << "[UI] GetClientRect failed error=" << GetLastError();
        return false;
    }
    const int taskbarWidth = taskbarRect.right - taskbarRect.left;
    const int taskbarHeight = taskbarRect.bottom - taskbarRect.top;
    if(taskbarWidth < taskbarHeight)
    {
        qWarning() << "[UI] vertical taskbar is not supported"
                   << "size=" << taskbarWidth << taskbarHeight;
        return false;
    }

    int rightEdge = taskbarWidth - 8;
    HWND notifyArea = FindWindowExW(taskbar, nullptr, L"TrayNotifyWnd", nullptr);
    if(notifyArea != nullptr)
    {
        RECT notifyRect{};
        if(GetWindowRect(notifyArea, &notifyRect))
        {
            POINT left{notifyRect.left, notifyRect.top};
            ScreenToClient(taskbar, &left);
            rightEdge = left.x - 6;
        }
    }

    const int targetHeight = qMax(36, taskbarHeight - 4);
    const int targetX = qMax(0, rightEdge - width());
    const int targetY = (taskbarHeight - targetHeight) / 2;
    if(GetParent(window) != taskbar)
    {
        SetLastError(ERROR_SUCCESS);
        if(SetParent(window, taskbar) == nullptr && GetLastError() != ERROR_SUCCESS)
        {
            qWarning() << "[UI] SetParent failed error=" << GetLastError();
            return false;
        }
        LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
        SetWindowLongPtrW(window, GWL_STYLE, (style & ~WS_POPUP) | WS_CHILD);
    }
    if(SetWindowPos(window, HWND_TOP, targetX, targetY, width(), targetHeight,
                    SWP_NOACTIVATE | SWP_SHOWWINDOW) == FALSE)
    {
        qWarning() << "[UI] SetWindowPos failed error=" << GetLastError()
                   << "target=" << targetX << targetY << width() << targetHeight
                   << "taskbar=" << taskbarWidth << taskbarHeight;
        return false;
    }
    return true;
#else
    return false;
#endif
}

void TaskbarTickerWidget::refreshDisplay()
{
    const auto updateRow = [this](const QString& symbol, RowWidgets& row) {
        const auto ticker = tickers_.constFind(symbol);
        if(ticker == tickers_.cend())
        {
            row.price->setText(connected_ ? QStringLiteral("获取中") : QStringLiteral("连接中"));
            row.change->setText(QStringLiteral("--"));
            row.change->setProperty("trend", "flat");
        }
        else
        {
            row.price->setText(formatPrice(ticker->price));
            row.change->setText(formatChange(ticker->changePercent));
            row.change->setProperty("trend", ticker->changePercent >= 0.0 ? "up" : "down");
        }
        row.change->style()->unpolish(row.change);
        row.change->style()->polish(row.change);
    };
    updateRow(QStringLiteral("BTCUSDT"), btcRow_);
    updateRow(QStringLiteral("ETHUSDT"), ethRow_);
    statusDot_->setStyleSheet(connected_ ? QStringLiteral("color: #17c964;")
                                         : QStringLiteral("color: #f5a524;"));
    // QWidget 被跨进程挂到 Explorer 后，Qt 的常规 update() 偶尔只更新后备缓冲，
    // 但任务栏子窗口没有收到 WM_PAINT。同步刷新 Qt 控件并通知 Win32 重绘整棵子窗口。
    repaint();
#ifdef Q_OS_WIN
    if(embedded_)
    {
        HWND window = reinterpret_cast<HWND>(winId());
        RedrawWindow(window, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    }
#endif
}

void TaskbarTickerWidget::toggleDetailCard()
{
    if(detailCard_->isVisible())
    {
        detailCard_->hide();
        return;
    }

    const QPoint anchor = mapToGlobal(QPoint(width(), 0));
    QScreen* screen = QGuiApplication::screenAt(anchor);
    if(screen == nullptr)
    {
        screen = QGuiApplication::primaryScreen();
    }
    const QRect available = screen->availableGeometry();
    int x = anchor.x() - detailCard_->width();
    x = qBound(available.left() + 8, x, available.right() - detailCard_->width() - 8);
    int y = mapToGlobal(QPoint(0, 0)).y() - detailCard_->height() - 8;
    if(y < available.top() + 8)
    {
        y = mapToGlobal(QPoint(0, height())).y() + 8;
    }
    detailCard_->move(x, y);
    detailCard_->show();
    detailCard_->raise();
}
