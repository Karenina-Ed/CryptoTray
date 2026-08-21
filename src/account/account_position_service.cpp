#include "account_position_service.h"

#include "credential_store.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <cmath>
#include <optional>

namespace
{
constexpr auto ApiKeyVariable = "CRYPTOTRAY_BINANCE_API_KEY";
constexpr auto SecretVariable = "CRYPTOTRAY_BINANCE_API_SECRET";

QString coinProfitAsset(const QJsonObject& object)
{
    const QString pair = object.value(QStringLiteral("pair")).toString();
    const int usdIndex = pair.indexOf(QStringLiteral("USD"));
    return usdIndex > 0 ? pair.left(usdIndex) : QStringLiteral("币");
}

QString positionSide(const QJsonObject& object, double amount)
{
    const QString apiSide = object.value(QStringLiteral("positionSide")).toString();
    if(apiSide == QStringLiteral("LONG") || apiSide == QStringLiteral("SHORT"))
    {
        return apiSide;
    }
    return amount >= 0.0 ? QStringLiteral("LONG") : QStringLiteral("SHORT");
}

QString positionKey(const QString& symbol, const QString& side)
{
    return symbol + QLatin1Char('|') + side;
}

}

AccountPositionService::AccountPositionService(QObject* parent)
    : QObject(parent)
{
    const BinanceCredentials stored = CredentialStore::load();
    if(stored.isValid())
    {
        apiKey_ = stored.apiKey;
        secretKey_ = stored.secretKey;
    }
    else
    {
        // 环境变量仅作为开发和迁移兼容入口，不会自动写入凭据管理器。
        apiKey_ = qgetenv(ApiKeyVariable).trimmed();
        secretKey_ = qgetenv(SecretVariable).trimmed();
    }
    refreshTimer_.setInterval(10000);
    connect(&refreshTimer_, &QTimer::timeout, this, &AccountPositionService::refresh);
}

void AccountPositionService::start()
{
    if(started_)
    {
        return;
    }
    started_ = true;
    if(!credentialsAvailable())
    {
        emit accountStateChanged(false, QStringLiteral("未配置 Binance HMAC API 密钥"));
        return;
    }

    emit accountStateChanged(true, QStringLiteral("正在读取持仓"));
    refresh();
    refreshTimer_.start();
}

void AccountPositionService::stop()
{
    started_ = false;
    refreshTimer_.stop();
}

bool AccountPositionService::credentialsAvailable() const
{
    return !apiKey_.isEmpty() && !secretKey_.isEmpty();
}

void AccountPositionService::saveCredentials(const QString& apiKey, const QString& secretKey)
{
    BinanceCredentials credentials{apiKey.trimmed().toUtf8(), secretKey.trimmed().toUtf8()};
    if(!credentials.isValid())
    {
        emit accountStateChanged(false, QStringLiteral("API Key 和 Secret 不能为空"));
        return;
    }

    QString error;
    if(!CredentialStore::save(credentials, &error))
    {
        emit accountStateChanged(false, error);
        return;
    }

    ++credentialRevision_;
    pendingReplies_ = 0;
    apiKey_ = credentials.apiKey;
    secretKey_ = credentials.secretKey;
    started_ = true;
    emit accountStateChanged(true, QStringLiteral("凭据已保存，正在同步账户"));
    refresh();
    refreshTimer_.start();
}

void AccountPositionService::deleteCredentials()
{
    QString error;
    if(!CredentialStore::remove(&error))
    {
        emit accountStateChanged(true, error);
        return;
    }

    ++credentialRevision_;
    pendingReplies_ = 0;
    refreshTimer_.stop();
    apiKey_.clear();
    secretKey_.clear();
    emit positionsUpdated({});
    emit accountOverviewUpdated({});
    emit accountStateChanged(false, QStringLiteral("尚未配置 Binance API 密钥"));
}

void AccountPositionService::refresh()
{
    if(!started_ || !credentialsAvailable() || pendingReplies_ != 0)
    {
        return;
    }

    pendingPositions_.clear();
    pendingErrors_.clear();
    pendingOverview_ = {};
    pendingSpotBalances_.clear();
    pendingOptionEquities_.clear();
    pendingFlexibleEarnBalances_.clear();
    pendingLockedEarnBalances_.clear();
    pendingUsdtPrices_.clear();
    pendingUsdFundingRates_.clear();
    pendingCoinFundingRates_.clear();
    pendingUsdLeverages_.clear();
    pendingCoinInitialMargins_.clear();
    pendingCoinContractSizes_.clear();
    usdAccountReceived_ = false;
    coinAccountReceived_ = false;
    spotAccountReceived_ = false;
    optionsAccountReceived_ = false;
    flexibleEarnReceived_ = false;
    lockedEarnReceived_ = false;
    earnPositionsTruncated_ = false;
    pricesReceived_ = false;
    pendingReplies_ = 14;
    sendRequest(FuturesMarket::UsdMargined, RequestKind::Positions,
                QStringLiteral("https://fapi.binance.com"),
                QStringLiteral("/fapi/v3/positionRisk"));
    sendRequest(FuturesMarket::CoinMargined, RequestKind::Positions,
                QStringLiteral("https://dapi.binance.com"),
                QStringLiteral("/dapi/v1/positionRisk"));
    sendRequest(FuturesMarket::Options, RequestKind::Positions,
                QStringLiteral("https://eapi.binance.com"),
                QStringLiteral("/eapi/v1/position"));
    // positionRisk V3 不再返回 leverage，按 symbolConfig 的账户配置补回实际初始杠杆。
    sendRequest(FuturesMarket::UsdMargined, RequestKind::SymbolConfiguration,
                QStringLiteral("https://fapi.binance.com"),
                QStringLiteral("/fapi/v1/symbolConfig"));
    sendRequest(FuturesMarket::UsdMargined, RequestKind::Account,
                QStringLiteral("https://fapi.binance.com"),
                QStringLiteral("/fapi/v3/account"));
    sendRequest(FuturesMarket::CoinMargined, RequestKind::Account,
                QStringLiteral("https://dapi.binance.com"),
                QStringLiteral("/dapi/v1/account"));
    sendRequest(FuturesMarket::UsdMargined, RequestKind::SpotAccount,
                QStringLiteral("https://api.binance.com"),
                QStringLiteral("/api/v3/account"));
    sendRequest(FuturesMarket::Options, RequestKind::OptionsAccount,
                QStringLiteral("https://eapi.binance.com"),
                QStringLiteral("/eapi/v1/marginAccount"));
    sendRequest(FuturesMarket::UsdMargined, RequestKind::FlexibleEarn,
                QStringLiteral("https://api.binance.com"),
                QStringLiteral("/sapi/v1/simple-earn/flexible/position"),
                QByteArrayLiteral("current=1&size=100"));
    sendRequest(FuturesMarket::UsdMargined, RequestKind::LockedEarn,
                QStringLiteral("https://api.binance.com"),
                QStringLiteral("/sapi/v1/simple-earn/locked/position"),
                QByteArrayLiteral("current=1&size=100"));
    sendPublicPricesRequest();
    sendFundingRatesRequest(FuturesMarket::UsdMargined);
    sendFundingRatesRequest(FuturesMarket::CoinMargined);
    sendCoinExchangeInfoRequest();
}

void AccountPositionService::sendRequest(FuturesMarket market, RequestKind kind,
                                         const QString& baseUrl, const QString& path,
                                         const QByteArray& extraQuery)
{
    // 参数顺序必须与签名输入完全一致，signature 始终放在查询串末尾。
    const QByteArray query = (extraQuery.isEmpty() ? QByteArray() : extraQuery + '&')
        + QByteArrayLiteral("timestamp=")
        + QByteArray::number(QDateTime::currentMSecsSinceEpoch())
        + QByteArrayLiteral("&recvWindow=5000");
    const QByteArray signature = QMessageAuthenticationCode::hash(
        query, secretKey_, QCryptographicHash::Sha256).toHex();
    const QUrl url(baseUrl + path + QStringLiteral("?")
                   + QString::fromLatin1(query) + QStringLiteral("&signature=")
                   + QString::fromLatin1(signature));

    QNetworkRequest request(url);
    request.setRawHeader("X-MBX-APIKEY", apiKey_);
    QNetworkReply* reply = network_.get(request);
    const int revision = credentialRevision_;
    connect(reply, &QNetworkReply::finished, this, [this, reply, market, kind, revision]() {
        handleReply(reply, market, kind, revision);
    });
}

void AccountPositionService::sendPublicPricesRequest()
{
    QNetworkReply* reply = network_.get(
        QNetworkRequest(QUrl(QStringLiteral("https://api.binance.com/api/v3/ticker/price"))));
    const int revision = credentialRevision_;
    connect(reply, &QNetworkReply::finished, this, [this, reply, revision]() {
        handleReply(reply, FuturesMarket::UsdMargined, RequestKind::Prices, revision);
    });
}

void AccountPositionService::sendFundingRatesRequest(FuturesMarket market)
{
    const bool usdMargined = market == FuturesMarket::UsdMargined;
    const QString url = usdMargined
        ? QStringLiteral("https://fapi.binance.com/fapi/v1/premiumIndex")
        : QStringLiteral("https://dapi.binance.com/dapi/v1/premiumIndex");
    QNetworkReply* reply = network_.get(QNetworkRequest(QUrl(url)));
    const int revision = credentialRevision_;
    connect(reply, &QNetworkReply::finished, this, [this, reply, market, revision]() {
        handleReply(reply, market, RequestKind::FundingRates, revision);
    });
}

void AccountPositionService::sendCoinExchangeInfoRequest()
{
    QNetworkReply* reply = network_.get(
        QNetworkRequest(QUrl(QStringLiteral("https://dapi.binance.com/dapi/v1/exchangeInfo"))));
    const int revision = credentialRevision_;
    connect(reply, &QNetworkReply::finished, this, [this, reply, revision]() {
        handleReply(reply, FuturesMarket::CoinMargined,
                    RequestKind::CoinExchangeInfo, revision);
    });
}

void AccountPositionService::handleReply(QNetworkReply* reply, FuturesMarket market,
                                         RequestKind kind, int credentialRevision)
{
    if(credentialRevision != credentialRevision_)
    {
        reply->deleteLater();
        return;
    }
    const QByteArray payload = reply->readAll();
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    const bool expectedShape = kind == RequestKind::Positions
            || kind == RequestKind::SymbolConfiguration || kind == RequestKind::Prices
            || kind == RequestKind::FundingRates
        ? document.isArray()
        : document.isObject();
    if(reply->error() != QNetworkReply::NoError || !expectedShape)
    {
        QString message = reply->errorString();
        if(document.isObject())
        {
            const QJsonObject error = document.object();
            message = QStringLiteral("%1 (%2)")
                .arg(error.value(QStringLiteral("msg")).toString())
                .arg(error.value(QStringLiteral("code")).toInt());
        }
        const QString sourceName = kind == RequestKind::SymbolConfiguration
            ? QStringLiteral("U 本位杠杆配置")
            : kind == RequestKind::CoinExchangeInfo
                ? QStringLiteral("币本位合约信息")
            : kind == RequestKind::SpotAccount
            ? QStringLiteral("现货账户")
            : kind == RequestKind::OptionsAccount
                ? QStringLiteral("期权账户")
                : kind == RequestKind::FlexibleEarn
                    ? QStringLiteral("理财活期")
                    : kind == RequestKind::LockedEarn
                        ? QStringLiteral("理财定期")
                : kind == RequestKind::Prices
                    ? QStringLiteral("资产估值行情")
                    : kind == RequestKind::FundingRates
                        ? (market == FuturesMarket::UsdMargined
                               ? QStringLiteral("U 本位资金费率")
                               : QStringLiteral("币本位资金费率"))
                    : market == FuturesMarket::UsdMargined
                        ? QStringLiteral("U 本位")
                        : market == FuturesMarket::CoinMargined
                            ? QStringLiteral("币本位")
                            : QStringLiteral("期权");
        pendingErrors_.append(QStringLiteral("%1：%2").arg(sourceName, message));
    }
    else if(kind == RequestKind::Positions)
    {
        for(const QJsonValue& value : document.array())
        {
            const QJsonObject object = value.toObject();
            const bool isOption = market == FuturesMarket::Options;
            const double amount = object.value(isOption ? QStringLiteral("quantity")
                                                        : QStringLiteral("positionAmt"))
                                      .toString().toDouble();
            if(std::abs(amount) < 1e-12)
            {
                continue;
            }

            FuturesPosition position;
            position.market = market;
            position.symbol = object.value(QStringLiteral("symbol")).toString();
            position.side = isOption ? object.value(QStringLiteral("side")).toString()
                                     : positionSide(object, amount);
            position.marginType = object.value(QStringLiteral("marginType")).toString();
            position.profitAsset = isOption
                ? object.value(QStringLiteral("quoteAsset")).toString(QStringLiteral("USDT"))
                : market == FuturesMarket::UsdMargined
                    ? object.value(QStringLiteral("marginAsset")).toString(QStringLiteral("USDT"))
                    : coinProfitAsset(object);
            position.optionSide = object.value(QStringLiteral("optionSide")).toString();
            position.amount = amount;
            position.entryPrice = object.value(QStringLiteral("entryPrice")).toString().toDouble();
            position.markPrice = object.value(QStringLiteral("markPrice")).toString().toDouble();
            position.strikePrice = object.value(QStringLiteral("strikePrice")).toString().toDouble();
            position.unrealizedProfit = object.value(
                isOption ? QStringLiteral("unrealizedPNL")
                         : QStringLiteral("unRealizedProfit")).toString().toDouble();
            position.initialMargin = object.value(
                QStringLiteral("positionInitialMargin")).toString().toDouble();
            position.liquidationPrice = object.value(QStringLiteral("liquidationPrice")).toString().toDouble();
            position.expiryDate = object.value(QStringLiteral("expiryDate")).toVariant().toLongLong();
            // 币本位接口仍返回字符串；toVariant() 同时兼容字符串和数字字段。
            position.leverage = object.value(QStringLiteral("leverage")).toVariant().toInt();
            pendingPositions_.append(position);
        }
    }
    else if(kind == RequestKind::SymbolConfiguration)
    {
        for(const QJsonValue& value : document.array())
        {
            const QJsonObject configuration = value.toObject();
            const QString symbol = configuration.value(QStringLiteral("symbol")).toString();
            const int leverage = configuration.value(QStringLiteral("leverage")).toInt();
            if(!symbol.isEmpty() && leverage > 0)
            {
                pendingUsdLeverages_.insert(symbol, leverage);
            }
        }
    }
    else if(kind == RequestKind::Account)
    {
        const QJsonObject account = document.object();
        if(market == FuturesMarket::UsdMargined)
        {
            usdAccountReceived_ = true;
            pendingOverview_.usdMarginBalance = account.value(
                QStringLiteral("totalMarginBalance")).toString().toDouble();
            pendingOverview_.usdUnrealizedProfit = account.value(
                QStringLiteral("totalUnrealizedProfit")).toString().toDouble();
        }
        else
        {
            coinAccountReceived_ = true;
            for(const QJsonValue& value : account.value(QStringLiteral("assets")).toArray())
            {
                const QJsonObject asset = value.toObject();
                CoinAccountAsset balance;
                balance.asset = asset.value(QStringLiteral("asset")).toString();
                balance.marginBalance = asset.value(
                    QStringLiteral("marginBalance")).toString().toDouble();
                balance.unrealizedProfit = asset.value(
                    QStringLiteral("unrealizedProfit")).toString().toDouble();
                if(std::abs(balance.marginBalance) >= 1e-12
                   || std::abs(balance.unrealizedProfit) >= 1e-12)
                {
                    pendingOverview_.coinAssets.append(balance);
                }
            }
            for(const QJsonValue& value : account.value(QStringLiteral("positions")).toArray())
            {
                const QJsonObject position = value.toObject();
                const double amount = position.value(QStringLiteral("positionAmt"))
                                          .toString().toDouble();
                const double initialMargin = position.value(
                    QStringLiteral("positionInitialMargin")).toString().toDouble();
                if(std::abs(amount) < 1e-12 || initialMargin <= 0.0)
                {
                    continue;
                }
                const QString symbol = position.value(QStringLiteral("symbol")).toString();
                pendingCoinInitialMargins_.insert(
                    positionKey(symbol, positionSide(position, amount)), initialMargin);
            }
        }
    }
    else if(kind == RequestKind::SpotAccount)
    {
        spotAccountReceived_ = true;
        for(const QJsonValue& value : document.object().value(QStringLiteral("balances")).toArray())
        {
            const QJsonObject asset = value.toObject();
            const double amount = asset.value(QStringLiteral("free")).toString().toDouble()
                + asset.value(QStringLiteral("locked")).toString().toDouble();
            if(std::abs(amount) >= 1e-12)
            {
                pendingSpotBalances_.insert(asset.value(QStringLiteral("asset")).toString(), amount);
            }
        }
    }
    else if(kind == RequestKind::OptionsAccount)
    {
        optionsAccountReceived_ = true;
        for(const QJsonValue& value : document.object().value(QStringLiteral("asset")).toArray())
        {
            const QJsonObject asset = value.toObject();
            const double equity = asset.value(QStringLiteral("equity")).toString().toDouble();
            if(std::abs(equity) >= 1e-12)
            {
                pendingOptionEquities_.insert(asset.value(QStringLiteral("asset")).toString(), equity);
            }
        }
    }
    else if(kind == RequestKind::FlexibleEarn || kind == RequestKind::LockedEarn)
    {
        const bool flexible = kind == RequestKind::FlexibleEarn;
        flexibleEarnReceived_ = flexibleEarnReceived_ || flexible;
        lockedEarnReceived_ = lockedEarnReceived_ || !flexible;
        QHash<QString, double>& balances = flexible
            ? pendingFlexibleEarnBalances_ : pendingLockedEarnBalances_;
        const QJsonObject response = document.object();
        const QJsonArray rows = response.value(QStringLiteral("rows")).toArray();
        for(const QJsonValue& value : rows)
        {
            const QJsonObject position = value.toObject();
            const QString asset = position.value(QStringLiteral("asset")).toString();
            const QString amountField = flexible ? QStringLiteral("totalAmount")
                                                 : QStringLiteral("amount");
            const double amount = position.value(amountField).toVariant().toDouble();
            if(!asset.isEmpty() && std::abs(amount) >= 1e-12)
            {
                balances[asset] += amount;
            }
        }
        if(response.value(QStringLiteral("total")).toVariant().toInt() > rows.size())
        {
            earnPositionsTruncated_ = true;
            pendingErrors_.append(QStringLiteral("理财持仓超过 100 条，仅统计首批数据"));
        }
    }
    else if(kind == RequestKind::Prices)
    {
        pricesReceived_ = true;
        for(const QJsonValue& value : document.array())
        {
            const QJsonObject ticker = value.toObject();
            const double price = ticker.value(QStringLiteral("price")).toString().toDouble();
            if(price > 0.0)
            {
                pendingUsdtPrices_.insert(ticker.value(QStringLiteral("symbol")).toString(), price);
            }
        }
    }
    else if(kind == RequestKind::FundingRates)
    {
        QHash<QString, double>& rates = market == FuturesMarket::UsdMargined
            ? pendingUsdFundingRates_ : pendingCoinFundingRates_;
        for(const QJsonValue& value : document.array())
        {
            const QJsonObject premium = value.toObject();
            const QString symbol = premium.value(QStringLiteral("symbol")).toString();
            bool validRate = false;
            const double rate = premium.value(QStringLiteral("lastFundingRate"))
                                    .toVariant().toDouble(&validRate);
            if(!symbol.isEmpty() && validRate && std::isfinite(rate))
            {
                rates.insert(symbol, rate);
            }
        }
    }
    else if(kind == RequestKind::CoinExchangeInfo)
    {
        for(const QJsonValue& value : document.object().value(QStringLiteral("symbols")).toArray())
        {
            const QJsonObject symbol = value.toObject();
            const QString name = symbol.value(QStringLiteral("symbol")).toString();
            const double contractSize = symbol.value(
                QStringLiteral("contractSize")).toVariant().toDouble();
            if(!name.isEmpty() && contractSize > 0.0)
            {
                pendingCoinContractSizes_.insert(name, contractSize);
            }
        }
    }

    reply->deleteLater();
    --pendingReplies_;
    finishRefresh();
}

void AccountPositionService::finishRefresh()
{
    if(pendingReplies_ != 0)
    {
        return;
    }

    // 两个请求异步完成，统一在本轮全部结束后合并，避免依赖响应先后顺序。
    for(FuturesPosition& position : pendingPositions_)
    {
        if(position.market == FuturesMarket::UsdMargined)
        {
            position.leverage = pendingUsdLeverages_.value(position.symbol, 0);
            const auto rate = pendingUsdFundingRates_.constFind(position.symbol);
            if(rate != pendingUsdFundingRates_.cend())
            {
                position.fundingRate = rate.value();
                position.fundingRateAvailable = true;
            }
        }
        else if(position.market == FuturesMarket::CoinMargined)
        {
            position.initialMargin = pendingCoinInitialMargins_.value(
                positionKey(position.symbol, position.side), 0.0);
            const double contractSize = pendingCoinContractSizes_.value(position.symbol, 0.0);
            if(contractSize > 0.0 && position.markPrice > 0.0)
            {
                // COIN-M 为反向合约：美元合约价值除以标记价格即为基础币数量。
                position.baseAssetAmount = position.amount * contractSize / position.markPrice;
            }
            const auto rate = pendingCoinFundingRates_.constFind(position.symbol);
            if(rate != pendingCoinFundingRates_.cend())
            {
                position.fundingRate = rate.value();
                position.fundingRateAvailable = true;
            }
        }
    }
    calculateEstimatedTotal();
    emit positionsUpdated(pendingPositions_);
    emit accountOverviewUpdated(pendingOverview_);
    emit accountStateChanged(true, pendingErrors_.isEmpty()
                                      ? QString()
                                      : pendingErrors_.join(QStringLiteral("；")));
}

void AccountPositionService::calculateEstimatedTotal()
{
    const auto valueInUsdt = [this](const QString& asset, double amount)
        -> std::optional<double> {
        if(asset == QStringLiteral("USDT"))
        {
            return amount;
        }

        const auto direct = pendingUsdtPrices_.constFind(asset + QStringLiteral("USDT"));
        if(direct != pendingUsdtPrices_.cend())
        {
            return amount * direct.value();
        }
        const auto inverse = pendingUsdtPrices_.constFind(QStringLiteral("USDT") + asset);
        if(inverse != pendingUsdtPrices_.cend())
        {
            return amount / inverse.value();
        }

        // 小币种常常只有 BTC 交易对，使用 BTCUSDT 做一次交叉折算。
        const auto assetBtc = pendingUsdtPrices_.constFind(asset + QStringLiteral("BTC"));
        const auto btcUsdt = pendingUsdtPrices_.constFind(QStringLiteral("BTCUSDT"));
        if(assetBtc != pendingUsdtPrices_.cend() && btcUsdt != pendingUsdtPrices_.cend())
        {
            return amount * assetBtc.value() * btcUsdt.value();
        }
        return std::nullopt;
    };

    const auto addAssets = [&valueInUsdt, this](const QHash<QString, double>& assets,
                                                double& subtotal) {
        bool complete = true;
        for(auto it = assets.cbegin(); it != assets.cend(); ++it)
        {
            const std::optional<double> value = valueInUsdt(it.key(), it.value());
            if(value)
            {
                subtotal += *value;
            }
            else
            {
                complete = false;
                pendingOverview_.unpricedAssets.append(it.key());
            }
        }
        return complete;
    };

    addAssets(pendingSpotBalances_, pendingOverview_.spotEstimatedUsdt);
    QHash<QString, double> coinBalances;
    for(const CoinAccountAsset& asset : pendingOverview_.coinAssets)
    {
        coinBalances.insert(asset.asset, asset.marginBalance);
    }
    addAssets(coinBalances, pendingOverview_.coinEstimatedUsdt);
    addAssets(pendingOptionEquities_, pendingOverview_.optionEstimatedUsdt);
    const bool flexibleEarnPriced = addAssets(
        pendingFlexibleEarnBalances_, pendingOverview_.earnFlexibleEstimatedUsdt);
    const bool lockedEarnPriced = addAssets(
        pendingLockedEarnBalances_, pendingOverview_.earnLockedEstimatedUsdt);
    pendingOverview_.earnEstimatedUsdt = pendingOverview_.earnFlexibleEstimatedUsdt
        + pendingOverview_.earnLockedEstimatedUsdt;
    pendingOverview_.earnValuationAvailable = flexibleEarnReceived_ || lockedEarnReceived_;
    pendingOverview_.earnValuationComplete = flexibleEarnReceived_ && lockedEarnReceived_
        && flexibleEarnPriced && lockedEarnPriced && !earnPositionsTruncated_;

    pendingOverview_.estimatedTotalUsdt = pendingOverview_.usdMarginBalance
        + pendingOverview_.spotEstimatedUsdt
        + pendingOverview_.coinEstimatedUsdt
        + pendingOverview_.optionEstimatedUsdt
        + pendingOverview_.earnEstimatedUsdt;
    pendingOverview_.unpricedAssets.removeDuplicates();
    pendingOverview_.valuationAvailable = usdAccountReceived_ || coinAccountReceived_
        || spotAccountReceived_ || optionsAccountReceived_
        || flexibleEarnReceived_ || lockedEarnReceived_;
    pendingOverview_.valuationComplete = usdAccountReceived_ && coinAccountReceived_
        && spotAccountReceived_ && optionsAccountReceived_ && pricesReceived_
        && flexibleEarnReceived_ && lockedEarnReceived_ && !earnPositionsTruncated_
        && pendingOverview_.unpricedAssets.isEmpty();
}
