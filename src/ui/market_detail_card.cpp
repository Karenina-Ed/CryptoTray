#include "market_detail_card.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
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

QIcon visibilityIcon(bool visible)
{
    QPixmap pixmap(20, 20);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(QStringLiteral("#8f949e")), 1.6,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    QPainterPath eye;
    eye.moveTo(2.0, 10.0);
    eye.cubicTo(5.5, 4.8, 14.5, 4.8, 18.0, 10.0);
    eye.cubicTo(14.5, 15.2, 5.5, 15.2, 2.0, 10.0);
    painter.drawPath(eye);
    painter.drawEllipse(QPointF(10.0, 10.0), 2.4, 2.4);
    if(!visible)
    {
        painter.drawLine(QPointF(3.0, 17.0), QPointF(17.0, 3.0));
    }
    return QIcon(pixmap);
}
}

MarketDetailCard::MarketDetailCard(QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
{
    // Qt::Popup 负责点击外部自动关闭；卡片保持顶层，避免成为 Explorer 任务栏的子窗口。
    setAttribute(Qt::WA_TranslucentBackground);
    setObjectName(QStringLiteral("marketDetailCard"));
    setFixedWidth(450);
    resize(450, 540);
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
        "QFrame[role='accountStrip'] { background:#121419; border:1px solid #24272e; border-radius:12px; }"
        "QFrame[role='accountDivider'] { background:#292c33; border:none; }"
        "QLabel[role='accountTitle'] { color: #8f949e; font-size: 10px; }"
        "QLabel[role='accountValue'] { color:#f4f4f5; font-size:15px; font-weight:700; }"
        "QLabel[role='allocationText'] { color:#777d88; font-size:9px; }"
        "QFrame[role='allocationTrack'] { background:#1c1f25; border:none; border-radius:2px; }"
        "QLabel[role='headerTotalCaption'] { color:#717782; font-size:9px; }"
        "QLabel[role='headerTotalValue'] { color:#f4f4f5; font-size:18px; font-weight:700; }"
        "QToolButton[role='visibility'] { background:transparent; border:none; border-radius:7px; padding:3px; }"
        "QToolButton[role='visibility']:hover { background:#20232a; }"
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

    auto* totalText = new QVBoxLayout();
    totalText->setSpacing(1);
    auto* totalCaptionRow = new QHBoxLayout();
    totalCaptionRow->setSpacing(3);
    totalCaptionRow->addStretch();
    auto* totalCaption = new QLabel(QStringLiteral("总资产"), surface);
    totalCaption->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    totalCaption->setProperty("role", "headerTotalCaption");
    totalCaptionRow->addWidget(totalCaption);

    visibilityButton_ = new QToolButton(surface);
    visibilityButton_->setProperty("role", "visibility");
    visibilityButton_->setCheckable(true);
    visibilityButton_->setChecked(true);
    visibilityButton_->setIcon(visibilityIcon(true));
    visibilityButton_->setIconSize(QSize(16, 16));
    visibilityButton_->setCursor(Qt::PointingHandCursor);
    visibilityButton_->setToolTip(QStringLiteral("隐藏总资产"));
    connect(visibilityButton_, &QToolButton::toggled, this, [this](bool visible) {
        totalAssetVisible_ = visible;
        visibilityButton_->setIcon(visibilityIcon(visible));
        visibilityButton_->setToolTip(visible ? QStringLiteral("隐藏总资产")
                                              : QStringLiteral("显示总资产"));
        refreshTotalAsset();
    });
    totalCaptionRow->addWidget(visibilityButton_);
    totalText->addLayout(totalCaptionRow);

    totalBalance_ = new QLabel(QStringLiteral("--"), surface);
    totalBalance_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    totalBalance_->setProperty("role", "headerTotalValue");
    totalText->addWidget(totalBalance_);
    header->addLayout(totalText);
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

    auto* accountStrip = new QFrame(accountPage);
    accountStrip->setProperty("role", "accountStrip");
    auto* summaries = new QHBoxLayout(accountStrip);
    summaries->setContentsMargins(10, 8, 10, 8);
    summaries->setSpacing(9);
    const auto addSummary = [accountStrip, summaries](const QString& title,
                                                      QLabel*& balance, QLabel*& detail) {
        auto* layout = new QVBoxLayout();
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);
        auto* caption = new QLabel(title, accountStrip);
        caption->setProperty("role", "accountTitle");
        layout->addWidget(caption);
        balance = new QLabel(QStringLiteral("--"), accountStrip);
        balance->setProperty("role", "accountValue");
        layout->addWidget(balance);
        detail = new QLabel(QStringLiteral("--"), accountStrip);
        detail->setProperty("role", "positionMeta");
        // 次级原币明细允许被压缩，避免多币种文本把固定宽度卡片横向撑开。
        detail->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        layout->addWidget(detail);
        summaries->addLayout(layout, 1);
    };
    const auto addDivider = [accountStrip, summaries]() {
        auto* divider = new QFrame(accountStrip);
        divider->setProperty("role", "accountDivider");
        divider->setFixedWidth(1);
        summaries->addWidget(divider);
    };
    addSummary(QStringLiteral("U 本位 / USDT"), usdBalance_, usdPnl_);
    addDivider();
    addSummary(QStringLiteral("币本位 / USDT"), coinBalance_, coinPnl_);
    addDivider();
    addSummary(QStringLiteral("理财 / USDT"), earnBalance_, earnDetail_);
    accountColumn->addWidget(accountStrip);

    allocationText_ = new QLabel(QStringLiteral("资产占比 --"), accountPage);
    allocationText_->setProperty("role", "allocationText");
    accountColumn->addWidget(allocationText_);
    auto* allocationTrack = new QFrame(accountPage);
    allocationTrack->setProperty("role", "allocationTrack");
    allocationTrack->setFixedHeight(4);
    allocationLayout_ = new QHBoxLayout(allocationTrack);
    allocationLayout_->setContentsMargins(0, 0, 0, 0);
    allocationLayout_->setSpacing(1);
    const auto addSegment = [allocationTrack, this](const QString& color, QFrame*& segment) {
        segment = new QFrame(allocationTrack);
        segment->setStyleSheet(QStringLiteral("background:%1;border:none;border-radius:2px;")
                                   .arg(color));
        allocationLayout_->addWidget(segment);
    };
    addSegment(QStringLiteral("#17c964"), usdAllocationSegment_);
    addSegment(QStringLiteral("#3b82f6"), coinAllocationSegment_);
    addSegment(QStringLiteral("#a970ff"), earnAllocationSegment_);
    addSegment(QStringLiteral("#5d6470"), otherAllocationSegment_);
    accountColumn->addWidget(allocationTrack);

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
    addPositionMarket(QStringLiteral("期权"), optionPositionsLayout_);
    positionRoot->addStretch();
    positionScroll->setWidget(positionContent);
    accountColumn->addWidget(positionScroll, 1);

    auto* pageSwitch = new QFrame(surface);
    pageSwitch->setObjectName(QStringLiteral("pageSwitch"));
    pageSwitch->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
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
    pageSwitch->setFixedHeight(pageSwitch->sizeHint().height());
    refreshPositions();

    const int expandedHeight = 540;
    const QMargins outerMargins = outer->contentsMargins();
    const QMargins contentMargins = content->contentsMargins();
    const int chromeHeight = outerMargins.top() + outerMargins.bottom()
        + contentMargins.top() + contentMargins.bottom()
        + header->sizeHint().height() + pageSwitch->sizeHint().height()
        + content->spacing() * 2 + 2;
    const auto resizeForPage = [this, pages, marketColumn,
                                expandedHeight, chromeHeight](int index) {
        const int oldBottom = y() + height();
        const int pageHeight = index == 0
            ? marketColumn->sizeHint().height()
            : expandedHeight - chromeHeight;
        pages->setFixedHeight(pageHeight);
        const int targetHeight = index == 0 ? chromeHeight + pageHeight : expandedHeight;
        // 顶层弹出窗口使用固定高度，避免布局把切换栏拉伸来填充旧页面留下的空间。
        setFixedHeight(targetHeight);
        if(isVisible())
        {
            // 页面切换时保持底边锚定任务栏，避免卡片向下伸出屏幕。
            move(x(), oldBottom - height());
        }
    };
    connect(pages, &QStackedWidget::currentChanged, this, resizeForPage);
    resizeForPage(0);
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
    accountOverview_ = overview;
    refreshTotalAsset();

    usdBalance_->setText(formatCompactNumber(overview.usdMarginBalance));
    usdPnl_->setText(QStringLiteral("PnL %1%2")
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
    coinBalance_->setText(formatCompactNumber(overview.coinEstimatedUsdt));
    coinPnl_->setText(balances.isEmpty() ? QStringLiteral("暂无资产")
                                         : balances.join(QStringLiteral(" · ")));
    coinPnl_->setToolTip(profits.isEmpty() ? QString() : profits.join(QStringLiteral(" · ")));
    if(overview.earnValuationAvailable)
    {
        earnBalance_->setText(QStringLiteral("%1%2")
                                  .arg(overview.earnValuationComplete
                                           ? QString() : QStringLiteral("≈ "))
                                  .arg(formatCompactNumber(overview.earnEstimatedUsdt)));
        earnDetail_->setText(QStringLiteral("活 %1 · 定 %2")
                                 .arg(formatCompactNumber(overview.earnFlexibleEstimatedUsdt),
                                      formatCompactNumber(overview.earnLockedEstimatedUsdt)));
    }
    else
    {
        earnBalance_->setText(QStringLiteral("--"));
        earnDetail_->setText(QStringLiteral("理财数据未取得"));
    }

    const double otherValue = overview.spotEstimatedUsdt + overview.optionEstimatedUsdt;
    const QList<double> allocationValues{
        std::max(0.0, overview.usdMarginBalance),
        std::max(0.0, overview.coinEstimatedUsdt),
        std::max(0.0, overview.earnEstimatedUsdt),
        std::max(0.0, otherValue)};
    const QList<QFrame*> allocationSegments{
        usdAllocationSegment_, coinAllocationSegment_,
        earnAllocationSegment_, otherAllocationSegment_};
    const double allocationTotal = allocationValues[0] + allocationValues[1]
        + allocationValues[2] + allocationValues[3];
    QStringList allocationParts;
    const QStringList allocationNames{
        QStringLiteral("U"), QStringLiteral("币"),
        QStringLiteral("理财"), QStringLiteral("其他")};
    for(int index = 0; index < allocationValues.size(); ++index)
    {
        const bool visible = allocationTotal > 0.0 && allocationValues[index] > 1e-9;
        allocationSegments[index]->setVisible(visible);
        allocationLayout_->setStretch(index, visible
            ? std::max(1, qRound(allocationValues[index] / allocationTotal * 1000.0)) : 0);
        if(visible)
        {
            allocationParts.append(QStringLiteral("%1 %2%")
                                       .arg(allocationNames[index])
                                       .arg(allocationValues[index] / allocationTotal * 100.0,
                                            0, 'f', 0));
        }
    }
    allocationText_->setText(allocationParts.isEmpty()
        ? QStringLiteral("资产占比 --")
        : QStringLiteral("资产占比  %1").arg(allocationParts.join(QStringLiteral(" · "))));
}

void MarketDetailCard::refreshTotalAsset()
{
    if(!totalAssetVisible_)
    {
        totalBalance_->setText(QStringLiteral("•••••• USDT"));
        totalBalance_->setToolTip(QStringLiteral("总资产已隐藏"));
        return;
    }
    if(!accountOverview_.valuationAvailable)
    {
        totalBalance_->setText(QStringLiteral("--"));
        totalBalance_->setToolTip(QStringLiteral("等待账户资产"));
        return;
    }

    totalBalance_->setText(QStringLiteral("%1%2 USDT")
                               .arg(accountOverview_.valuationComplete
                                        ? QString() : QStringLiteral("≈ "))
                               .arg(formatCompactNumber(accountOverview_.estimatedTotalUsdt)));
    const QString breakdown = QStringLiteral(
        "现货 %1 · U 本位 %2 · 币本位 %3 · 期权 %4 · 理财 %5")
        .arg(formatCompactNumber(accountOverview_.spotEstimatedUsdt),
             formatCompactNumber(accountOverview_.usdMarginBalance),
             formatCompactNumber(accountOverview_.coinEstimatedUsdt),
             formatCompactNumber(accountOverview_.optionEstimatedUsdt),
             formatCompactNumber(accountOverview_.earnEstimatedUsdt));
    const QString valuationHint = accountOverview_.valuationComplete
        ? QStringLiteral("完整估值\n%1").arg(breakdown)
        : accountOverview_.unpricedAssets.isEmpty()
            ? QStringLiteral("部分账户未返回，仅显示已取得资产的估值\n%1").arg(breakdown)
            : QStringLiteral("未计价资产：%1\n%2")
                  .arg(accountOverview_.unpricedAssets.join(QStringLiteral("、")), breakdown);
    totalBalance_->setToolTip(valuationHint);
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
            if(position.market == FuturesMarket::Options)
            {
                auto* optionSide = new QLabel(position.optionSide == QStringLiteral("CALL")
                                                  ? QStringLiteral("看涨")
                                                  : QStringLiteral("看跌"), row);
                optionSide->setProperty("role", "positionMeta");
                headline->addWidget(optionSide);
            }
            headline->addStretch();
            const QString amountText = position.market == FuturesMarket::Options
                ? formatCompactNumber(std::abs(position.amount))
                : QStringLiteral("%1 · %2x")
                      .arg(formatCompactNumber(std::abs(position.amount)))
                      .arg(position.leverage);
            auto* amount = new QLabel(amountText, row);
            amount->setProperty("role", "positionMeta");
            headline->addWidget(amount);
            box->addLayout(headline);

            auto* details = new QHBoxLayout();
            const QString priceText = position.market == FuturesMarket::Options
                ? QStringLiteral("开 %1  标 %2  行权 %3 · %4 到期")
                      .arg(formatCompactNumber(position.entryPrice),
                           formatCompactNumber(position.markPrice),
                           formatCompactNumber(position.strikePrice),
                           QDateTime::fromMSecsSinceEpoch(position.expiryDate, Qt::UTC)
                               .toString(QStringLiteral("MM-dd")))
                : QStringLiteral("开 %1  标 %2  强平 %3")
                      .arg(formatCompactNumber(position.entryPrice),
                           formatCompactNumber(position.markPrice),
                           position.liquidationPrice > 0.0
                               ? formatCompactNumber(position.liquidationPrice)
                               : QStringLiteral("--"));
            auto* prices = new QLabel(priceText, row);
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
    populate(optionPositionsLayout_, FuturesMarket::Options);
}
