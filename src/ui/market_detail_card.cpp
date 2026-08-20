#include "market_detail_card.h"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QComboBox>
#include <QCompleter>
#include <QDateTime>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
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
        "QLineEdit[role='marketSearch'] { color:#f4f4f5; background:#14161b; border:1px solid #2b2f37; "
        "border-radius:11px; padding:9px 13px; font-size:12px; selection-background-color:#17c964; }"
        "QLineEdit[role='marketSearch']:hover { border-color:#3a3f49; }"
        "QLineEdit[role='marketSearch']:focus { background:#171a20; border-color:#17c964; }"
        "QPushButton[role='addSymbol'] { color:#07130c; background:#17c964; border:none; border-radius:19px; "
        "font-size:18px; font-weight:700; padding:0; }"
        "QPushButton[role='addSymbol']:hover { background:#27da75; }"
        "QPushButton[role='addSymbol']:pressed { background:#11b958; }"
        "QFrame[role='marketRow'] { background:#121419; border:none; border-bottom:1px solid #22252b; }"
        "QFrame[role='marketRow']:hover { background:#171a20; }"
        "QLabel[role='tableHeader'] { color:#656b76; font-size:9px; }"
        "QLabel[role='marketSymbol'] { color:#f4f4f5; font-size:12px; font-weight:700; }"
        "QLabel[role='marketPair'] { color:#686e78; font-size:9px; }"
        "QLabel[role='marketPrice'] { color:#f4f4f5; font-size:13px; font-weight:700; }"
        "QLabel[role='marketChange'] { font-size:11px; font-weight:700; }"
        "QLabel[role='marketChange'][trend='up'] { color:#17c964; }"
        "QLabel[role='marketChange'][trend='down'] { color:#f04444; }"
        "QLabel[role='marketChange'][trend='flat'] { color:#858b96; }"
        "QToolButton[role='favorite'] { color:#f0b90b; background:transparent; border:none; font-size:15px; }"
        "QToolButton[role='favorite']:hover { color:#f4f4f5; }"
        "QLabel[role='marketHint'] { color:#777d88; font-size:10px; }"
        "QLabel[role='metricValue'] { color: #d7d9de; font-size: 11px; font-weight: 600; }"
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
        "QComboBox[role='valuationUnit'] { color:#d7d9de; background:#17191f; border:1px solid #292c33; "
        "border-radius:8px; padding:5px 24px 5px 9px; font-size:10px; font-weight:600; }"
        "QComboBox[role='valuationUnit']:hover { border-color:#3a3e47; }"
        "QComboBox[role='valuationUnit']::drop-down { border:none; width:20px; }"
        "QComboBox[role='valuationUnit'] QAbstractItemView { color:#d7d9de; background:#15171c; "
        "border:1px solid #292c33; selection-background-color:#252930; outline:none; }"
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

    pages_ = new QStackedWidget(surface);
    pages_->setAttribute(Qt::WA_TranslucentBackground);
    auto* marketPage = new QWidget(pages_);
    marketPage->setProperty("role", "page");
    marketPage->setAttribute(Qt::WA_TranslucentBackground);
    auto* marketColumn = new QVBoxLayout(marketPage);
    marketColumn->setContentsMargins(0, 0, 0, 0);
    marketColumn->setSpacing(8);

    auto* searchRow = new QHBoxLayout();
    searchRow->setSpacing(7);
    marketSearch_ = new QLineEdit(marketPage);
    marketSearch_->setProperty("role", "marketSearch");
    marketSearch_->setPlaceholderText(QStringLiteral("搜索 U 本位永续，例如 SOL"));
    marketSearch_->setClearButtonEnabled(true);
    searchRow->addWidget(marketSearch_, 1);
    auto* addButton = new QPushButton(QStringLiteral("＋"), marketPage);
    addButton->setProperty("role", "addSymbol");
    addButton->setCursor(Qt::PointingHandCursor);
    addButton->setFixedSize(38, 38);
    addButton->setToolTip(QStringLiteral("加入自选"));
    searchRow->addWidget(addButton);
    marketColumn->addLayout(searchRow);
    connect(addButton, &QPushButton::clicked, this, &MarketDetailCard::addWatchlistSymbol);
    connect(marketSearch_, &QLineEdit::returnPressed, this, &MarketDetailCard::addWatchlistSymbol);

    marketHint_ = new QLabel(QStringLiteral("仅显示 Binance U 本位永续合约 · 24h 以 UTC 日线统计"), marketPage);
    marketHint_->setProperty("role", "marketHint");
    marketColumn->addWidget(marketHint_);

    auto* tableHeader = new QHBoxLayout();
    tableHeader->setContentsMargins(10, 0, 27, 0);
    auto* symbolHeader = new QLabel(QStringLiteral("合约"), marketPage);
    symbolHeader->setProperty("role", "tableHeader");
    auto* priceHeader = new QLabel(QStringLiteral("最新价"), marketPage);
    priceHeader->setProperty("role", "tableHeader");
    priceHeader->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto* changeHeader = new QLabel(QStringLiteral("UTC 涨跌"), marketPage);
    changeHeader->setProperty("role", "tableHeader");
    changeHeader->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto* volumeHeader = new QLabel(QStringLiteral("成交量"), marketPage);
    volumeHeader->setProperty("role", "tableHeader");
    volumeHeader->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    tableHeader->addWidget(symbolHeader, 13);
    tableHeader->addWidget(priceHeader, 10);
    tableHeader->addWidget(changeHeader, 8);
    tableHeader->addWidget(volumeHeader, 8);
    marketColumn->addLayout(tableHeader);

    marketScroll_ = new QScrollArea(marketPage);
    marketScroll_->setWidgetResizable(true);
    marketScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    marketScroll_->viewport()->setAutoFillBackground(false);
    auto* marketContent = new QWidget(marketScroll_);
    marketContent->setAttribute(Qt::WA_TranslucentBackground);
    marketRowsLayout_ = new QVBoxLayout(marketContent);
    marketRowsLayout_->setContentsMargins(0, 0, 3, 0);
    marketRowsLayout_->setSpacing(0);
    marketScroll_->setWidget(marketContent);
    marketColumn->addWidget(marketScroll_);
    pages_->addWidget(marketPage);

    auto* accountPage = new QWidget(pages_);
    accountPage->setProperty("role", "page");
    accountPage->setAttribute(Qt::WA_TranslucentBackground);
    auto* accountColumn = new QVBoxLayout(accountPage);
    accountColumn->setContentsMargins(0, 0, 0, 0);
    accountColumn->setSpacing(9);
    pages_->addWidget(accountPage);
    content->addWidget(pages_, 1);

    auto* accountTitleRow = new QHBoxLayout();
    auto* positionTitle = new QLabel(QStringLiteral("账户概览"), accountPage);
    positionTitle->setProperty("role", "sectionTitle");
    accountTitleRow->addWidget(positionTitle);
    accountTitleRow->addStretch();
    valuationUnit_ = new QComboBox(accountPage);
    valuationUnit_->setProperty("role", "valuationUnit");
    valuationUnit_->addItems({QStringLiteral("USDT"), QStringLiteral("BTC"),
                              QStringLiteral("ETH"), QStringLiteral("CNY")});
    const QString savedUnit = QSettings().value(QStringLiteral("account/valuationUnit"),
                                                QStringLiteral("USDT")).toString().toUpper();
    valuationUnit_->setCurrentText(valuationUnit_->findText(savedUnit) >= 0
                                       ? savedUnit : QStringLiteral("USDT"));
    valuationUnit_->setToolTip(QStringLiteral("选择账户资产计价单位；CNY 按 ECB 每日参考汇率近似换算"));
    accountTitleRow->addWidget(valuationUnit_);
    accountColumn->addLayout(accountTitleRow);
    connect(valuationUnit_, &QComboBox::currentTextChanged, this, [this](const QString& unit) {
        QSettings().setValue(QStringLiteral("account/valuationUnit"), unit);
        refreshAccountOverview();
    });

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
    addSummary(QStringLiteral("U 本位"), usdBalance_, usdPnl_);
    addDivider();
    addSummary(QStringLiteral("币本位"), coinBalance_, coinPnl_);
    addDivider();
    addSummary(QStringLiteral("理财"), earnBalance_, earnDetail_);
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
    connect(pageGroup, &QButtonGroup::idClicked, pages_, &QStackedWidget::setCurrentIndex);
    content->addWidget(pageSwitch);
    pageSwitch->setFixedHeight(pageSwitch->sizeHint().height());
    refreshPositions();

    const QMargins outerMargins = outer->contentsMargins();
    const QMargins contentMargins = content->contentsMargins();
    chromeHeight_ = outerMargins.top() + outerMargins.bottom()
        + contentMargins.top() + contentMargins.bottom()
        + header->sizeHint().height() + pageSwitch->sizeHint().height()
        + content->spacing() * 2 + 2;
    connect(pages_, &QStackedWidget::currentChanged, this,
            [this](int) { resizeForCurrentPage(); });
    setWatchlist({QStringLiteral("BTCUSDT"), QStringLiteral("ETHUSDT")});
    resizeForCurrentPage();
}

void MarketDetailCard::updateTicker(const Ticker& ticker)
{
    tickers_.insert(ticker.symbol, ticker);
    refreshWatchRow(ticker.symbol);
    if((ticker.symbol == QStringLiteral("BTCUSDT")
        || ticker.symbol == QStringLiteral("ETHUSDT"))
       && valuationUnit_ != nullptr
       && valuationUnit_->currentText() != QStringLiteral("USDT"))
    {
        refreshAccountOverview();
    }
}

void MarketDetailCard::setWatchlist(const QStringList& symbols)
{
    QStringList normalized;
    for(QString symbol : symbols)
    {
        symbol = symbol.trimmed().toUpper();
        if(!symbol.isEmpty() && !normalized.contains(symbol))
        {
            normalized.append(symbol);
        }
    }
    if(normalized == watchlist_ && !watchRows_.isEmpty())
    {
        return;
    }
    watchlist_ = normalized;
    rebuildWatchlist();
}

void MarketDetailCard::setAvailableSymbols(const QStringList& symbols)
{
    availableSymbols_ = symbols;
    if(marketCompleter_ != nullptr)
    {
        marketCompleter_->deleteLater();
    }
    marketCompleter_ = new QCompleter(availableSymbols_, marketSearch_);
    marketCompleter_->setCaseSensitivity(Qt::CaseInsensitive);
    marketCompleter_->setFilterMode(Qt::MatchContains);
    marketCompleter_->setCompletionMode(QCompleter::PopupCompletion);
    marketCompleter_->setMaxVisibleItems(7);
    QAbstractItemView* popup = marketCompleter_->popup();
    popup->setObjectName(QStringLiteral("marketCompleterPopup"));
    popup->setAttribute(Qt::WA_StyledBackground, true);
    popup->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    popup->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    popup->setMinimumWidth(marketSearch_->width());
    // 补全列表是独立顶层窗口，不会继承详情卡片样式，因此同时设置样式表和调色板。
    QPalette popupPalette = popup->palette();
    popupPalette.setColor(QPalette::Base, QColor(QStringLiteral("#111318")));
    popupPalette.setColor(QPalette::Text, QColor(QStringLiteral("#d7d9de")));
    popupPalette.setColor(QPalette::Highlight, QColor(QStringLiteral("#183326")));
    popupPalette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#f4f4f5")));
    popup->setPalette(popupPalette);
    popup->setStyleSheet(QStringLiteral(
        "QAbstractItemView#marketCompleterPopup { color:#d7d9de; background:#111318; "
        "border:1px solid #2b2f37; padding:5px; outline:none; "
        "font-family:'Segoe UI Variable Text','Segoe UI'; font-size:11px; }"
        "QAbstractItemView#marketCompleterPopup::item { min-height:28px; padding:2px 9px; "
        "border:none; border-radius:6px; }"
        "QAbstractItemView#marketCompleterPopup::item:hover { background:#1d2128; }"
        "QAbstractItemView#marketCompleterPopup::item:selected { color:#f4f4f5; background:#183326; }"
        "QScrollBar:vertical { background:transparent; width:5px; margin:5px 1px; }"
        "QScrollBar::handle:vertical { background:#3a3e47; border-radius:2px; min-height:24px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"));
    marketSearch_->setCompleter(marketCompleter_);
}

void MarketDetailCard::setCnyRate(double cnyPerUsdt)
{
    if(cnyPerUsdt <= 0.0)
    {
        return;
    }
    cnyPerUsdt_ = cnyPerUsdt;
    if(valuationUnit_ != nullptr && valuationUnit_->currentText() == QStringLiteral("CNY"))
    {
        refreshAccountOverview();
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
    refreshAccountOverview();
}

void MarketDetailCard::refreshAccountOverview()
{
    const FuturesAccountOverview& overview = accountOverview_;
    refreshTotalAsset();

    usdBalance_->setText(formatValuation(overview.usdMarginBalance));
    usdPnl_->setText(QStringLiteral("PnL %1%2")
                         .arg(overview.usdUnrealizedProfit >= 0.0 ? QStringLiteral("+") : QString())
                         .arg(formatValuation(overview.usdUnrealizedProfit)));
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
    coinBalance_->setText(formatValuation(overview.coinEstimatedUsdt));
    coinPnl_->setText(balances.isEmpty() ? QStringLiteral("暂无资产")
                                         : balances.join(QStringLiteral(" · ")));
    coinPnl_->setToolTip(profits.isEmpty() ? QString() : profits.join(QStringLiteral(" · ")));
    if(overview.earnValuationAvailable)
    {
        earnBalance_->setText(QStringLiteral("%1%2")
                                  .arg(overview.earnValuationComplete
                                           ? QString() : QStringLiteral("≈ "))
                                  .arg(formatValuation(overview.earnEstimatedUsdt)));
        earnDetail_->setText(QStringLiteral("活 %1 · 定 %2")
                                 .arg(formatValuation(overview.earnFlexibleEstimatedUsdt),
                                      formatValuation(overview.earnLockedEstimatedUsdt)));
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
    const QString unit = valuationUnit_ != nullptr
        ? valuationUnit_->currentText() : QStringLiteral("USDT");
    if(!totalAssetVisible_)
    {
        totalBalance_->setText(QStringLiteral("•••••• %1").arg(unit));
        totalBalance_->setToolTip(QStringLiteral("总资产已隐藏"));
        return;
    }
    if(!accountOverview_.valuationAvailable)
    {
        totalBalance_->setText(QStringLiteral("--"));
        totalBalance_->setToolTip(QStringLiteral("等待账户资产"));
        return;
    }

    const QString formattedTotal = formatValuation(accountOverview_.estimatedTotalUsdt);
    totalBalance_->setText(QStringLiteral("%1%2")
                               .arg(accountOverview_.valuationComplete
                                         ? QString() : QStringLiteral("≈ "))
                               .arg(formattedTotal));
    const QString breakdown = QStringLiteral(
        "现货 %1 · U 本位 %2 · 币本位 %3 · 期权 %4 · 理财 %5")
        .arg(formatValuation(accountOverview_.spotEstimatedUsdt),
             formatValuation(accountOverview_.usdMarginBalance),
             formatValuation(accountOverview_.coinEstimatedUsdt),
             formatValuation(accountOverview_.optionEstimatedUsdt),
             formatValuation(accountOverview_.earnEstimatedUsdt));
    QString valuationHint = accountOverview_.valuationComplete
        ? QStringLiteral("完整估值\n%1").arg(breakdown)
        : accountOverview_.unpricedAssets.isEmpty()
            ? QStringLiteral("部分账户未返回，仅显示已取得资产的估值\n%1").arg(breakdown)
            : QStringLiteral("未计价资产：%1\n%2")
                  .arg(accountOverview_.unpricedAssets.join(QStringLiteral("、")), breakdown);
    if(unit == QStringLiteral("CNY"))
    {
        valuationHint += QStringLiteral("\nCNY 按 ECB 每日 USD/CNY 参考汇率近似换算（1 USDT≈1 USD）");
    }
    totalBalance_->setToolTip(valuationHint);
}

QString MarketDetailCard::formatValuation(double usdtValue) const
{
    const QString unit = valuationUnit_ != nullptr
        ? valuationUnit_->currentText() : QStringLiteral("USDT");
    double value = usdtValue;
    int decimals = 2;
    if(unit == QStringLiteral("BTC") || unit == QStringLiteral("ETH"))
    {
        const QString symbol = unit + QStringLiteral("USDT");
        const auto ticker = tickers_.constFind(symbol);
        if(ticker == tickers_.cend() || ticker->price <= 0.0)
        {
            return QStringLiteral("-- %1").arg(unit);
        }
        value /= ticker->price;
        decimals = unit == QStringLiteral("BTC") ? 8 : 6;
    }
    else if(unit == QStringLiteral("CNY"))
    {
        if(cnyPerUsdt_ <= 0.0)
        {
            return QStringLiteral("-- CNY");
        }
        value *= cnyPerUsdt_;
    }
    return QStringLiteral("%1 %2").arg(QString::number(value, 'f', decimals), unit);
}

void MarketDetailCard::setAccountState(bool configured, const QString& message)
{
    const QString display = configured
        ? message
        : QStringLiteral("%1。请右键任务栏组件配置 API").arg(message);
    accountStatus_->setText(display);
    accountStatus_->setVisible(!display.isEmpty());
}

void MarketDetailCard::addWatchlistSymbol()
{
    QString symbol = marketSearch_->text().trimmed().toUpper();
    if(symbol.isEmpty())
    {
        return;
    }
    if(!symbol.endsWith(QStringLiteral("USDT")))
    {
        symbol += QStringLiteral("USDT");
    }
    if(watchlist_.contains(symbol))
    {
        marketHint_->setText(QStringLiteral("%1 已在自选中").arg(symbol));
        return;
    }
    if(!availableSymbols_.isEmpty() && !availableSymbols_.contains(symbol))
    {
        marketHint_->setText(QStringLiteral("未找到可交易的 U 本位永续合约：%1").arg(symbol));
        return;
    }
    if(availableSymbols_.isEmpty()
       && !QRegularExpression(QStringLiteral("^[A-Z0-9]{2,20}USDT$")).match(symbol).hasMatch())
    {
        marketHint_->setText(QStringLiteral("请输入以 USDT 结尾的合约代码"));
        return;
    }
    if(watchlist_.size() >= 20)
    {
        marketHint_->setText(QStringLiteral("自选最多添加 20 个合约"));
        return;
    }

    QStringList next = watchlist_;
    next.append(symbol);
    marketSearch_->clear();
    marketHint_->setText(QStringLiteral("已加入 %1").arg(symbol));
    setWatchlist(next);
    emit watchlistChangeRequested(next);
}

void MarketDetailCard::rebuildWatchlist()
{
    watchRows_.clear();
    clearLayout(marketRowsLayout_);
    for(const QString& symbol : watchlist_)
    {
        auto* row = new QFrame(this);
        row->setProperty("role", "marketRow");
        row->setFixedHeight(52);
        auto* line = new QHBoxLayout(row);
        line->setContentsMargins(10, 5, 6, 5);
        line->setSpacing(5);

        auto* names = new QVBoxLayout();
        names->setSpacing(0);
        QString baseAsset = symbol;
        if(baseAsset.endsWith(QStringLiteral("USDT")))
        {
            baseAsset.chop(4);
        }
        auto* name = new QLabel(baseAsset, row);
        name->setProperty("role", "marketSymbol");
        auto* pair = new QLabel(QStringLiteral("/ USDT 永续"), row);
        pair->setProperty("role", "marketPair");
        names->addWidget(name);
        names->addWidget(pair);
        line->addLayout(names, 13);

        WatchRow widgets;
        widgets.price = new QLabel(QStringLiteral("--"), row);
        widgets.price->setProperty("role", "marketPrice");
        widgets.price->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        line->addWidget(widgets.price, 10);
        widgets.change = new QLabel(QStringLiteral("--"), row);
        widgets.change->setProperty("role", "marketChange");
        widgets.change->setProperty("trend", "flat");
        widgets.change->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        line->addWidget(widgets.change, 8);
        widgets.volume = new QLabel(QStringLiteral("--"), row);
        widgets.volume->setProperty("role", "marketPair");
        widgets.volume->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        line->addWidget(widgets.volume, 8);

        auto* remove = new QToolButton(row);
        remove->setProperty("role", "favorite");
        remove->setText(QStringLiteral("★"));
        remove->setCursor(Qt::PointingHandCursor);
        remove->setToolTip(QStringLiteral("移出自选"));
        connect(remove, &QToolButton::clicked, this, [this, symbol]() {
            QStringList next = watchlist_;
            next.removeAll(symbol);
            setWatchlist(next);
            emit watchlistChangeRequested(next);
        });
        line->addWidget(remove);
        marketRowsLayout_->addWidget(row);
        watchRows_.insert(symbol, widgets);
        refreshWatchRow(symbol);
    }

    // 列表最多显示六行，更多自选通过内部滚动查看，卡片高度随少量自选自然收缩。
    if(watchlist_.isEmpty())
    {
        auto* empty = new QLabel(QStringLiteral("暂无自选，在上方搜索合约加入"), marketScroll_);
        empty->setAlignment(Qt::AlignCenter);
        empty->setProperty("role", "marketHint");
        marketRowsLayout_->addWidget(empty);
    }
    marketRowsLayout_->addStretch();
    marketScroll_->setFixedHeight(qBound(52, qMax(1, watchlist_.size()) * 52, 312));
    QTimer::singleShot(0, this, &MarketDetailCard::resizeForCurrentPage);
}

void MarketDetailCard::refreshWatchRow(const QString& symbol)
{
    const auto row = watchRows_.find(symbol);
    const auto ticker = tickers_.constFind(symbol);
    if(row == watchRows_.end() || ticker == tickers_.cend())
    {
        return;
    }
    row->price->setText(formatPrice(ticker->price));
    row->change->setText(formatChange(ticker->changePercent));
    row->change->setProperty("trend", ticker->changePercent >= 0.0 ? "up" : "down");
    row->change->style()->unpolish(row->change);
    row->change->style()->polish(row->change);
    row->volume->setText(formatVolume(ticker->volume));
}

void MarketDetailCard::resizeForCurrentPage()
{
    if(pages_ == nullptr || chromeHeight_ <= 0)
    {
        return;
    }
    const int oldBottom = y() + height();
    const int expandedHeight = 540;
    const int pageHeight = pages_->currentIndex() == 0
        ? pages_->widget(0)->sizeHint().height()
        : expandedHeight - chromeHeight_;
    pages_->setFixedHeight(pageHeight);
    setFixedHeight(pages_->currentIndex() == 0
                       ? chromeHeight_ + pageHeight : expandedHeight);
    if(isVisible())
    {
        move(x(), oldBottom - height());
    }
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
            QString amountText;
            if(position.market == FuturesMarket::Options)
            {
                amountText = formatCompactNumber(std::abs(position.amount));
            }
            else
            {
                const QString leverage = position.leverage > 0
                    ? QStringLiteral("%1x").arg(position.leverage)
                    : QStringLiteral("--");
                if(position.market == FuturesMarket::CoinMargined)
                {
                    const QString coinAmount = std::abs(position.baseAssetAmount) > 1e-12
                        ? formatCompactNumber(std::abs(position.baseAssetAmount))
                        : QStringLiteral("--");
                    amountText = QStringLiteral("%1 %2 · %3")
                                     .arg(coinAmount, position.profitAsset, leverage);
                }
                else
                {
                    amountText = QStringLiteral("%1 · %2")
                                     .arg(formatCompactNumber(std::abs(position.amount)), leverage);
                }
            }
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
            QString pnlText = QStringLiteral("%1%2 %3")
                                  .arg(position.unrealizedProfit >= 0.0 ? QStringLiteral("+") : QString())
                                  .arg(formatCompactNumber(position.unrealizedProfit), position.profitAsset);
            if(position.market != FuturesMarket::Options && position.initialMargin > 1e-12)
            {
                const double returnRate = position.unrealizedProfit
                    / position.initialMargin * 100.0;
                pnlText += QStringLiteral(" · %1%2%")
                               .arg(returnRate >= 0.0 ? QStringLiteral("+") : QString())
                               .arg(returnRate, 0, 'f', 2);
            }
            auto* pnl = new QLabel(pnlText, row);
            pnl->setProperty("role", "positionPnl");
            pnl->setProperty("trend", position.unrealizedProfit >= 0.0 ? "up" : "down");
            pnl->setToolTip(QStringLiteral("收益率 = 未实现盈亏 ÷ 持仓初始保证金"));
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
