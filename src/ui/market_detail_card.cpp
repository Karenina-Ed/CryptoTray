#include "market_detail_card.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStackedWidget>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include <cmath>

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

QString formatCompactNumber(double value)
{
    return QString::number(value, 'f', std::abs(value) >= 1000.0 ? 2 : 6)
        .remove(QRegularExpression(QStringLiteral("0+$")))
        .remove(QRegularExpression(QStringLiteral("\\.$")));
}

void clearLayout(QVBoxLayout* layout)
{
    while(QLayoutItem* item = layout->takeAt(0))
    {
        delete item->widget();
        delete item;
    }
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
    setFixedSize(450, 540);
    setStyleSheet(QStringLiteral(
        "#marketDetailCard { background: transparent; }"
        "QFrame#cardSurface { background: #0d0e11; border: 1px solid #272a31; border-radius: 18px; }"
        "QLabel { color: #f4f4f5; font-family: 'Segoe UI Variable Text', 'Segoe UI'; }"
        "QLabel[role='title'] { font-size: 19px; font-weight: 700; }"
        "QLabel[role='subtitle'] { color: #8f949e; font-size: 11px; }"
        "QFrame[role='coin'] { background: #15171c; border: 1px solid #24272e; border-radius: 13px; }"
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
        "QLabel[role='sectionTitle'] { color: #f4f4f5; font-size: 13px; font-weight: 700; }"
        "QLabel[role='accountStatus'] { color: #777d88; font-size: 10px; }"
        "QFrame[role='account'] { background: #15171c; border: 1px solid #24272e; border-radius: 12px; }"
        "QLabel[role='accountTitle'] { color: #8f949e; font-size: 10px; }"
        "QLabel[role='accountValue'] { color: #f4f4f5; font-size: 16px; font-weight: 700; }"
        "QFrame[role='position'] { background: #15171c; border: 1px solid #24272e; border-radius: 10px; }"
        "QLabel[role='positionSymbol'] { color: #f4f4f5; font-size: 12px; font-weight: 700; }"
        "QLabel[role='positionMeta'] { color: #858b96; font-size: 10px; }"
        "QLabel[role='positionPnl'][trend='up'] { color: #17c964; font-size: 11px; font-weight: 700; }"
        "QLabel[role='positionPnl'][trend='down'] { color: #f04444; font-size: 11px; font-weight: 700; }"
        "QStackedWidget, QWidget[role='page'], QScrollArea, QScrollArea > QWidget, "
        "QWidget#positionContent { background: transparent; border: none; }"
        "QFrame#pageSwitch { background:#15171c; border:1px solid #252830; border-radius:11px; }"
        "QPushButton[role='pageButton'] { color:#858b96; background:transparent; border:none; "
        "border-radius:8px; padding:8px 22px; font-family:'Segoe UI Variable Text','Segoe UI'; "
        "font-size:12px; font-weight:600; }"
        "QPushButton[role='pageButton']:checked { color:#07130c; background:#17c964; font-weight:700; }"
        "QPushButton[role='pageButton']:hover:!checked { color:#f4f4f5; background:#20232a; }"
        "QScrollBar:vertical { background: transparent; width: 5px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #3a3e47; border-radius: 2px; min-height: 24px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"));

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
    content->setContentsMargins(17, 16, 17, 14);
    content->setSpacing(12);

    auto* header = new QHBoxLayout();
    auto* titles = new QVBoxLayout();
    titles->setSpacing(1);
    auto* title = new QLabel(QStringLiteral("CryptoTray"), surface);
    title->setProperty("role", "title");
    auto* subtitle = new QLabel(QStringLiteral("永续行情与合约账户"), surface);
    subtitle->setProperty("role", "subtitle");
    titles->addWidget(title);
    titles->addWidget(subtitle);
    header->addLayout(titles);
    header->addStretch();
    content->addLayout(header);

    auto* pages = new QStackedWidget(surface);
    pages->setAttribute(Qt::WA_TranslucentBackground);
    auto* marketPage = new QWidget(pages);
    marketPage->setProperty("role", "page");
    marketPage->setAttribute(Qt::WA_TranslucentBackground);
    auto* marketColumn = new QVBoxLayout(marketPage);
    marketColumn->setContentsMargins(0, 0, 0, 0);
    marketColumn->setSpacing(10);
    addCoinSection(marketColumn, QStringLiteral("BTCUSDT"), btcWidgets_);
    addCoinSection(marketColumn, QStringLiteral("ETHUSDT"), ethWidgets_);
    marketColumn->addStretch();
    pages->addWidget(marketPage);

    auto* accountPage = new QWidget(pages);
    accountPage->setProperty("role", "page");
    accountPage->setAttribute(Qt::WA_TranslucentBackground);
    auto* accountColumn = new QVBoxLayout(accountPage);
    accountColumn->setContentsMargins(0, 0, 0, 0);
    accountColumn->setSpacing(9);
    pages->addWidget(accountPage);
    content->addWidget(pages, 1);

    auto* positionTitle = new QLabel(QStringLiteral("账户概览"), accountPage);
    positionTitle->setProperty("role", "sectionTitle");
    accountColumn->addWidget(positionTitle);

    auto* summaries = new QHBoxLayout();
    summaries->setSpacing(7);
    const auto addSummary = [accountPage, summaries](const QString& title, QLabel*& balance,
                                                     QLabel*& pnl) {
        auto* frame = new QFrame(accountPage);
        frame->setProperty("role", "account");
        auto* layout = new QVBoxLayout(frame);
        layout->setContentsMargins(10, 7, 10, 7);
        layout->setSpacing(2);
        auto* caption = new QLabel(title, frame);
        caption->setProperty("role", "accountTitle");
        layout->addWidget(caption);
        balance = new QLabel(QStringLiteral("--"), frame);
        balance->setProperty("role", "accountValue");
        layout->addWidget(balance);
        pnl = new QLabel(QStringLiteral("未实现 --"), frame);
        pnl->setProperty("role", "positionMeta");
        layout->addWidget(pnl);
        summaries->addWidget(frame);
    };
    addSummary(QStringLiteral("U 本位总资产"), usdBalance_, usdPnl_);
    addSummary(QStringLiteral("币本位资产"), coinBalance_, coinPnl_);
    accountColumn->addLayout(summaries);

    accountStatus_ = new QLabel(accountPage);
    accountStatus_->setProperty("role", "accountStatus");
    accountStatus_->setWordWrap(true);
    accountStatus_->hide();
    accountColumn->addWidget(accountStatus_);

    auto* holdingTitle = new QLabel(QStringLiteral("合约持仓"), accountPage);
    holdingTitle->setProperty("role", "sectionTitle");
    accountColumn->addWidget(holdingTitle);

    auto* positionScroll = new QScrollArea(accountPage);
    positionScroll->setWidgetResizable(true);
    positionScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    positionScroll->viewport()->setAutoFillBackground(false);
    auto* positionContent = new QWidget(positionScroll);
    positionContent->setObjectName(QStringLiteral("positionContent"));
    positionContent->setAttribute(Qt::WA_TranslucentBackground);
    positionContent->setAutoFillBackground(false);
    auto* positionRoot = new QVBoxLayout(positionContent);
    positionRoot->setContentsMargins(0, 0, 3, 0);
    positionRoot->setSpacing(8);

    const auto addPositionMarket = [positionContent, positionRoot](const QString& title,
                                                                  QVBoxLayout*& rows) {
        auto* label = new QLabel(title, positionContent);
        label->setProperty("role", "metricValue");
        positionRoot->addWidget(label);
        rows = new QVBoxLayout();
        rows->setSpacing(5);
        positionRoot->addLayout(rows);
    };
    addPositionMarket(QStringLiteral("U 本位"), usdPositionsLayout_);
    addPositionMarket(QStringLiteral("币本位"), coinPositionsLayout_);
    positionRoot->addStretch();
    positionScroll->setWidget(positionContent);
    accountColumn->addWidget(positionScroll, 1);

    auto* pageSwitch = new QFrame(surface);
    pageSwitch->setObjectName(QStringLiteral("pageSwitch"));
    auto* switchLayout = new QHBoxLayout(pageSwitch);
    switchLayout->setContentsMargins(3, 3, 3, 3);
    switchLayout->setSpacing(3);
    auto* marketButton = new QPushButton(QStringLiteral("市场"), pageSwitch);
    auto* accountButton = new QPushButton(QStringLiteral("账户"), pageSwitch);
    for(QPushButton* button : {marketButton, accountButton})
    {
        button->setProperty("role", "pageButton");
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        switchLayout->addWidget(button, 1);
    }
    auto* pageGroup = new QButtonGroup(pageSwitch);
    pageGroup->setExclusive(true);
    pageGroup->addButton(marketButton, 0);
    pageGroup->addButton(accountButton, 1);
    marketButton->setChecked(true);
    connect(pageGroup, &QButtonGroup::idClicked, pages, &QStackedWidget::setCurrentIndex);
    content->addWidget(pageSwitch);
    refreshPositions();
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
    setToolTip(errorMessage);
}

void MarketDetailCard::setPositions(const FuturesPositions& positions)
{
    positions_ = positions;
    refreshPositions();
}

void MarketDetailCard::setAccountOverview(const FuturesAccountOverview& overview)
{
    usdBalance_->setText(QStringLiteral("%1 USD")
                             .arg(formatCompactNumber(overview.usdMarginBalance)));
    usdPnl_->setText(QStringLiteral("未实现 %1%2 USD")
                         .arg(overview.usdUnrealizedProfit >= 0.0 ? QStringLiteral("+") : QString())
                         .arg(formatCompactNumber(overview.usdUnrealizedProfit)));
    usdPnl_->setStyleSheet(overview.usdUnrealizedProfit >= 0.0
                               ? QStringLiteral("color:#17c964;")
                               : QStringLiteral("color:#f04444;"));

    QStringList balances;
    QStringList profits;
    for(const CoinAccountAsset& asset : overview.coinAssets)
    {
        balances.append(QStringLiteral("%1 %2")
                            .arg(asset.asset, formatCompactNumber(asset.marginBalance)));
        profits.append(QStringLiteral("%1 %2%3")
                           .arg(asset.asset,
                                asset.unrealizedProfit >= 0.0 ? QStringLiteral("+") : QString(),
                                formatCompactNumber(asset.unrealizedProfit)));
    }
    coinBalance_->setText(balances.isEmpty() ? QStringLiteral("暂无资产")
                                             : balances.join(QStringLiteral(" · ")));
    coinBalance_->setStyleSheet(balances.size() > 1 ? QStringLiteral("font-size:12px;")
                                                    : QString());
    coinPnl_->setText(profits.isEmpty() ? QStringLiteral("未实现 --")
                                        : profits.join(QStringLiteral(" · ")));
}

void MarketDetailCard::setAccountState(bool configured, const QString& message)
{
    const QString display = configured
        ? message
        : QStringLiteral("%1。请右键任务栏组件配置 API").arg(message);
    accountStatus_->setText(display);
    accountStatus_->setVisible(!display.isEmpty());
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

void MarketDetailCard::refreshPositions()
{
    const auto populate = [this](QVBoxLayout* layout, FuturesMarket market) {
        clearLayout(layout);
        int count = 0;
        for(const FuturesPosition& position : positions_)
        {
            if(position.market != market)
            {
                continue;
            }
            ++count;
            auto* row = new QFrame(this);
            row->setProperty("role", "position");
            auto* box = new QVBoxLayout(row);
            box->setContentsMargins(10, 7, 10, 7);
            box->setSpacing(3);

            auto* headline = new QHBoxLayout();
            auto* symbol = new QLabel(position.symbol, row);
            symbol->setProperty("role", "positionSymbol");
            headline->addWidget(symbol);
            auto* side = new QLabel(position.side == QStringLiteral("LONG")
                                        ? QStringLiteral("多") : QStringLiteral("空"), row);
            side->setStyleSheet(position.side == QStringLiteral("LONG")
                                    ? QStringLiteral("color:#17c964;font-weight:700;")
                                    : QStringLiteral("color:#f04444;font-weight:700;"));
            headline->addWidget(side);
            headline->addStretch();
            auto* amount = new QLabel(QStringLiteral("%1 · %2x")
                                          .arg(formatCompactNumber(std::abs(position.amount)))
                                          .arg(position.leverage), row);
            amount->setProperty("role", "positionMeta");
            headline->addWidget(amount);
            box->addLayout(headline);

            auto* details = new QHBoxLayout();
            auto* prices = new QLabel(QStringLiteral("开 %1  标 %2  强平 %3")
                                          .arg(formatCompactNumber(position.entryPrice),
                                               formatCompactNumber(position.markPrice),
                                               position.liquidationPrice > 0.0
                                                   ? formatCompactNumber(position.liquidationPrice)
                                                   : QStringLiteral("--")), row);
            prices->setProperty("role", "positionMeta");
            details->addWidget(prices);
            details->addStretch();
            auto* pnl = new QLabel(QStringLiteral("%1%2 %3")
                                       .arg(position.unrealizedProfit >= 0.0 ? QStringLiteral("+") : QString())
                                       .arg(formatCompactNumber(position.unrealizedProfit), position.profitAsset), row);
            pnl->setProperty("role", "positionPnl");
            pnl->setProperty("trend", position.unrealizedProfit >= 0.0 ? "up" : "down");
            details->addWidget(pnl);
            box->addLayout(details);
            layout->addWidget(row);
        }

        if(count == 0)
        {
            auto* empty = new QLabel(QStringLiteral("暂无持仓"), this);
            empty->setProperty("role", "positionMeta");
            layout->addWidget(empty);
        }
    };

    populate(usdPositionsLayout_, FuturesMarket::UsdMargined);
    populate(coinPositionsLayout_, FuturesMarket::CoinMargined);
}
