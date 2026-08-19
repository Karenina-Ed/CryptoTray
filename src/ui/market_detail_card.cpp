#include "market_detail_card.h"

#include <QDateTime>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QVBoxLayout>

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

QString formatVolume(double volume)
{
    if(volume >= 1000000.0)
    {
        return QStringLiteral("%1M").arg(volume / 1000000.0, 0, 'f', 2);
    }
    if(volume >= 1000.0)
    {
        return QStringLiteral("%1K").arg(volume / 1000.0, 0, 'f', 2);
    }
    return QString::number(volume, 'f', 2);
}

QLabel* createMetricLabel(const QString& caption, QGridLayout* grid, int column)
{
    auto* title = new QLabel(caption);
    title->setProperty("role", "metricTitle");
    auto* value = new QLabel(QStringLiteral("--"));
    value->setProperty("role", "metricValue");
    grid->addWidget(title, 0, column);
    grid->addWidget(value, 1, column);
    return value;
}
}

MarketDetailCard::MarketDetailCard(QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
{
    // Qt::Popup 负责点击外部自动关闭；卡片保持顶层，避免成为 Explorer 任务栏的子窗口。
    setAttribute(Qt::WA_TranslucentBackground);
    setObjectName(QStringLiteral("marketDetailCard"));
    setFixedSize(382, 370);
    setStyleSheet(QStringLiteral(
        "#marketDetailCard { background: transparent; }"
        "QFrame#cardSurface { background: #101114; border: 1px solid #292c33; border-radius: 16px; }"
        "QLabel { color: #f4f4f5; font-family: 'Segoe UI Variable Text', 'Segoe UI'; }"
        "QLabel[role='title'] { font-size: 18px; font-weight: 700; }"
        "QLabel[role='subtitle'] { color: #8f949e; font-size: 11px; }"
        "QLabel[role='badge'][status='online'] { color: #17c964; background: #14291f; border-radius: 8px; padding: 3px 8px; }"
        "QLabel[role='badge'][status='offline'] { color: #f5a524; background: #302616; border-radius: 8px; padding: 3px 8px; }"
        "QFrame[role='coin'] { background: #17191e; border: 1px solid #24272e; border-radius: 12px; }"
        "QLabel[role='symbol'] { font-size: 14px; font-weight: 700; }"
        "QLabel[role='pair'] { color: #777d88; font-size: 10px; }"
        "QLabel[role='price'] { font-size: 20px; font-weight: 700; }"
        "QLabel[role='change'] { font-size: 12px; font-weight: 700; border-radius: 8px; padding: 3px 8px; }"
        "QLabel[role='change'][trend='up'] { color: #17c964; background: #14291f; }"
        "QLabel[role='change'][trend='down'] { color: #f04444; background: #30191c; }"
        "QLabel[role='change'][trend='flat'] { color: #929292; background: #24262c; }"
        "QLabel[role='metricTitle'] { color: #717782; font-size: 10px; }"
        "QLabel[role='metricValue'] { color: #d7d9de; font-size: 11px; font-weight: 600; }"
        "QLabel[role='updated'] { color: #686e78; font-size: 10px; }"
        "QLabel[role='footer'] { color: #777d88; font-size: 10px; }"));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);

    auto* surface = new QFrame(this);
    surface->setObjectName(QStringLiteral("cardSurface"));
    auto* shadow = new QGraphicsDropShadowEffect(surface);
    shadow->setBlurRadius(28.0);
    shadow->setOffset(0.0, 8.0);
    shadow->setColor(QColor(0, 0, 0, 150));
    surface->setGraphicsEffect(shadow);
    outer->addWidget(surface);

    auto* content = new QVBoxLayout(surface);
    content->setContentsMargins(16, 14, 16, 13);
    content->setSpacing(10);

    auto* header = new QHBoxLayout();
    auto* titles = new QVBoxLayout();
    titles->setSpacing(1);
    auto* title = new QLabel(QStringLiteral("市场概览"), surface);
    title->setProperty("role", "title");
    auto* subtitle = new QLabel(QStringLiteral("U 本位永续 · UTC 日线"), surface);
    subtitle->setProperty("role", "subtitle");
    titles->addWidget(title);
    titles->addWidget(subtitle);
    header->addLayout(titles);
    header->addStretch();
    connectionBadge_ = new QLabel(QStringLiteral("连接中"), surface);
    connectionBadge_->setProperty("role", "badge");
    connectionBadge_->setProperty("status", "offline");
    header->addWidget(connectionBadge_);
    content->addLayout(header);

    addCoinSection(content, QStringLiteral("BTCUSDT"), btcWidgets_);
    addCoinSection(content, QStringLiteral("ETHUSDT"), ethWidgets_);

    // footer_ = new QLabel(QStringLiteral("数据源：Binance 永续合约 · 点击卡片外部关闭"), surface);
    // footer_->setProperty("role", "footer");
    // content->addWidget(footer_);
}

void MarketDetailCard::updateTicker(const Ticker& ticker)
{
    tickers_.insert(ticker.symbol, ticker);
    if(ticker.symbol == QStringLiteral("BTCUSDT"))
    {
        refreshCoin(ticker.symbol, btcWidgets_);
    }
    else if(ticker.symbol == QStringLiteral("ETHUSDT"))
    {
        refreshCoin(ticker.symbol, ethWidgets_);
    }
}

void MarketDetailCard::setConnectionState(bool connected, const QString& errorMessage)
{
    connected_ = connected;
    errorMessage_ = errorMessage;
    connectionBadge_->setText(connected ? QStringLiteral("实时") : QStringLiteral("连接中"));
    connectionBadge_->setProperty("status", connected ? "online" : "offline");
    connectionBadge_->style()->unpolish(connectionBadge_);
    connectionBadge_->style()->polish(connectionBadge_);

    footer_->setText(!connected && !errorMessage_.isEmpty()
                         ? QStringLiteral("连接错误：%1").arg(errorMessage_)
                         : QStringLiteral("数据源：Binance 永续合约 · 点击卡片外部关闭"));
}

void MarketDetailCard::addCoinSection(QVBoxLayout* layout, const QString& symbol,
                                      CoinWidgets& widgets)
{
    auto* section = new QFrame(this);
    section->setProperty("role", "coin");
    auto* box = new QVBoxLayout(section);
    box->setContentsMargins(13, 10, 13, 9);
    box->setSpacing(6);

    auto* headline = new QHBoxLayout();
    auto* names = new QVBoxLayout();
    names->setSpacing(0);
    auto* symbolLabel = new QLabel(symbol.chopped(4), section);
    symbolLabel->setProperty("role", "symbol");
    auto* pair = new QLabel(QStringLiteral("/ USDT 永续"), section);
    pair->setProperty("role", "pair");
    names->addWidget(symbolLabel);
    names->addWidget(pair);
    headline->addLayout(names);
    headline->addStretch();
    widgets.price = new QLabel(QStringLiteral("--"), section);
    widgets.price->setProperty("role", "price");
    headline->addWidget(widgets.price);
    widgets.change = new QLabel(QStringLiteral("--"), section);
    widgets.change->setProperty("role", "change");
    widgets.change->setProperty("trend", "flat");
    headline->addWidget(widgets.change);
    box->addLayout(headline);

    auto* metrics = new QGridLayout();
    metrics->setHorizontalSpacing(14);
    metrics->setVerticalSpacing(1);
    widgets.open = createMetricLabel(QStringLiteral("开盘"), metrics, 0);
    widgets.high = createMetricLabel(QStringLiteral("最高"), metrics, 1);
    widgets.low = createMetricLabel(QStringLiteral("最低"), metrics, 2);
    widgets.volume = createMetricLabel(QStringLiteral("成交量"), metrics, 3);
    box->addLayout(metrics);

    widgets.updatedAt = new QLabel(QStringLiteral("等待行情"), section);
    widgets.updatedAt->setProperty("role", "updated");
    box->addWidget(widgets.updatedAt);
    layout->addWidget(section);
}

void MarketDetailCard::refreshCoin(const QString& symbol, CoinWidgets& widgets)
{
    const auto ticker = tickers_.constFind(symbol);
    if(ticker == tickers_.cend())
    {
        return;
    }

    widgets.price->setText(formatPrice(ticker->price));
    widgets.change->setText(formatChange(ticker->changePercent));
    widgets.change->setProperty("trend", ticker->changePercent >= 0.0 ? "up" : "down");
    widgets.change->style()->unpolish(widgets.change);
    widgets.change->style()->polish(widgets.change);
    widgets.open->setText(formatPrice(ticker->openPrice));
    widgets.high->setText(formatPrice(ticker->highPrice));
    widgets.low->setText(formatPrice(ticker->lowPrice));
    widgets.volume->setText(formatVolume(ticker->volume));

    const QDateTime time = QDateTime::fromMSecsSinceEpoch(ticker->eventTime, Qt::UTC);
    widgets.updatedAt->setText(QStringLiteral("更新于 %1 UTC")
                                   .arg(time.toString(QStringLiteral("HH:mm:ss"))));
}
