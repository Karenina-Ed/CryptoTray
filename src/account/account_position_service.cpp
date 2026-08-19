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
    pendingReplies_ = 5;
    sendRequest(FuturesMarket::UsdMargined, RequestKind::Positions,
                QStringLiteral("https://fapi.binance.com"),
                QStringLiteral("/fapi/v3/positionRisk"));
    sendRequest(FuturesMarket::CoinMargined, RequestKind::Positions,
                QStringLiteral("https://dapi.binance.com"),
                QStringLiteral("/dapi/v1/positionRisk"));
    sendRequest(FuturesMarket::Options, RequestKind::Positions,
                QStringLiteral("https://eapi.binance.com"),
                QStringLiteral("/eapi/v1/position"));
    sendRequest(FuturesMarket::UsdMargined, RequestKind::Account,
                QStringLiteral("https://fapi.binance.com"),
                QStringLiteral("/fapi/v3/account"));
    sendRequest(FuturesMarket::CoinMargined, RequestKind::Account,
                QStringLiteral("https://dapi.binance.com"),
                QStringLiteral("/dapi/v1/account"));
}

void AccountPositionService::sendRequest(FuturesMarket market, RequestKind kind,
                                         const QString& baseUrl, const QString& path)
{
    // 参数顺序必须与签名输入完全一致，signature 始终放在查询串末尾。
    const QByteArray query = QByteArrayLiteral("timestamp=")
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
    const bool expectedShape = kind == RequestKind::Positions ? document.isArray()
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
        const QString marketName = market == FuturesMarket::UsdMargined
            ? QStringLiteral("U 本位")
            : market == FuturesMarket::CoinMargined ? QStringLiteral("币本位")
                                                     : QStringLiteral("期权");
        pendingErrors_.append(QStringLiteral("%1：%2").arg(marketName, message));
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
            position.liquidationPrice = object.value(QStringLiteral("liquidationPrice")).toString().toDouble();
            position.expiryDate = object.value(QStringLiteral("expiryDate")).toVariant().toLongLong();
            position.leverage = object.value(QStringLiteral("leverage")).toString().toInt();
            pendingPositions_.append(position);
        }
    }
    else
    {
        const QJsonObject account = document.object();
        if(market == FuturesMarket::UsdMargined)
        {
            pendingOverview_.usdMarginBalance = account.value(
                QStringLiteral("totalMarginBalance")).toString().toDouble();
            pendingOverview_.usdUnrealizedProfit = account.value(
                QStringLiteral("totalUnrealizedProfit")).toString().toDouble();
        }
        else
        {
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

    emit positionsUpdated(pendingPositions_);
    emit accountOverviewUpdated(pendingOverview_);
    emit accountStateChanged(true, pendingErrors_.isEmpty()
                                      ? QString()
                                      : pendingErrors_.join(QStringLiteral("；")));
}
