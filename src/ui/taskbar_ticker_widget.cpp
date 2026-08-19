#include "taskbar_ticker_widget.h"

#include "market_detail_card.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
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

void TaskbarTickerWidget::setPositions(const FuturesPositions& positions)
{
    detailCard_->setPositions(positions);
}

void TaskbarTickerWidget::setAccountOverview(const FuturesAccountOverview& overview)
{
    detailCard_->setAccountOverview(overview);
}

void TaskbarTickerWidget::setAccountState(bool configured, const QString& message)
{
    detailCard_->setAccountState(configured, message);
}

void TaskbarTickerWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background: #111111; color: white; border: 1px solid #2b2b2b; padding: 6px; }"
        "QMenu::item { padding: 7px 26px; border-radius: 4px; }"
        "QMenu::item:selected { background: #242424; }"));
    QAction* configureAction = menu.addAction(QStringLiteral("配置 Binance API"));
    QAction* deleteAction = menu.addAction(QStringLiteral("删除 API 凭据"));
    menu.addSeparator();
    QAction* exitAction = menu.addAction(QStringLiteral("退出 CryptoTray"));
    connect(configureAction, &QAction::triggered,
            this, &TaskbarTickerWidget::showCredentialDialog);
    connect(deleteAction, &QAction::triggered, this, [this]() {
        const auto answer = QMessageBox::question(
            nullptr, QStringLiteral("删除 API 凭据"),
            QStringLiteral("确定从 Windows 凭据管理器中删除 Binance API 凭据吗？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if(answer == QMessageBox::Yes)
        {
            emit credentialsDeleteRequested();
        }
    });
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);
    menu.exec(event->globalPos());
}

void TaskbarTickerWidget::showCredentialDialog()
{
    // 任务栏窗口已成为 Explorer 子窗口，配置窗口必须保持独立顶层，避免被任务栏裁剪。
    QDialog dialog(nullptr);
    dialog.setWindowTitle(QStringLiteral("配置 Binance API"));
    dialog.setModal(true);
    dialog.setMinimumWidth(420);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background:#101114; color:#f4f4f5; }"
        "QLabel { color:#a7acb6; font-family:'Segoe UI Variable Text','Segoe UI'; }"
        "QLineEdit { color:#f4f4f5; background:#181a20; border:1px solid #30333b; "
        "border-radius:8px; padding:9px 10px; selection-background-color:#17c964; }"
        "QLineEdit:focus { border-color:#17c964; }"
        "QPushButton { color:#f4f4f5; background:#24272e; border:none; border-radius:8px; "
        "padding:8px 18px; }"
        "QPushButton:hover { background:#30343c; }"
        "QPushButton:default { color:#07130c; background:#17c964; font-weight:700; }"));

    auto* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(14);
    auto* description = new QLabel(
        QStringLiteral("凭据将加密保存在当前 Windows 用户的凭据管理器中。\n"
                       "建议使用仅具备账户读取权限的独立密钥。"), &dialog);
    description->setWordWrap(true);
    root->addWidget(description);

    auto* form = new QFormLayout();
    form->setSpacing(10);
    auto* apiKey = new QLineEdit(&dialog);
    apiKey->setPlaceholderText(QStringLiteral("API Key"));
    auto* secret = new QLineEdit(&dialog);
    secret->setPlaceholderText(QStringLiteral("HMAC Secret"));
    secret->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("API Key"), apiKey);
    form->addRow(QStringLiteral("Secret"), secret);
    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel,
                                         Qt::Horizontal, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, apiKey, secret]() {
        if(apiKey->text().trimmed().isEmpty() || secret->text().trimmed().isEmpty())
        {
            QMessageBox::warning(&dialog, QStringLiteral("无法保存"),
                                 QStringLiteral("API Key 和 HMAC Secret 不能为空。"));
            return;
        }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    root->addWidget(buttons);

    if(dialog.exec() == QDialog::Accepted)
    {
        emit credentialsSaveRequested(apiKey->text().trimmed(), secret->text().trimmed());
        secret->clear();
    }
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
