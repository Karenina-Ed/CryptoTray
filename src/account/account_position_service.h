#pragma once

#include "futures_position.h"

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>

class QNetworkReply;

// 使用只读 USER_DATA 接口定时获取持仓；持久化职责交给独立的凭据存储类。
class AccountPositionService final : public QObject
{
    Q_OBJECT

public:
    explicit AccountPositionService(QObject* parent = nullptr);

    void start();
    void stop();
    bool credentialsAvailable() const;

public slots:
    void saveCredentials(const QString& apiKey, const QString& secretKey);
    void deleteCredentials();

signals:
    void positionsUpdated(const FuturesPositions& positions);
    void accountOverviewUpdated(const FuturesAccountOverview& overview);
    void accountStateChanged(bool configured, const QString& message);

private:
    enum class RequestKind
    {
        Positions,
        SymbolConfiguration,
        Account,
        SpotAccount,
        OptionsAccount,
        FlexibleEarn,
        LockedEarn,
        Prices
    };

    void refresh();
    void sendRequest(FuturesMarket market, RequestKind kind,
                     const QString& baseUrl, const QString& path,
                     const QByteArray& extraQuery = {});
    void sendPublicPricesRequest();
    void handleReply(QNetworkReply* reply, FuturesMarket market, RequestKind kind,
                     int credentialRevision);
    void finishRefresh();
    void calculateEstimatedTotal();

    QNetworkAccessManager network_;
    QTimer refreshTimer_;
    QByteArray apiKey_;
    QByteArray secretKey_;
    FuturesPositions pendingPositions_;
    FuturesAccountOverview pendingOverview_;
    QHash<QString, double> pendingSpotBalances_;
    QHash<QString, double> pendingOptionEquities_;
    QHash<QString, double> pendingFlexibleEarnBalances_;
    QHash<QString, double> pendingLockedEarnBalances_;
    QHash<QString, double> pendingUsdtPrices_;
    QHash<QString, int> pendingUsdLeverages_;
    QHash<QString, double> pendingCoinInitialMargins_;
    QStringList pendingErrors_;
    int pendingReplies_ = 0;
    int credentialRevision_ = 0;
    bool started_ = false;
    bool usdAccountReceived_ = false;
    bool coinAccountReceived_ = false;
    bool spotAccountReceived_ = false;
    bool optionsAccountReceived_ = false;
    bool flexibleEarnReceived_ = false;
    bool lockedEarnReceived_ = false;
    bool earnPositionsTruncated_ = false;
    bool pricesReceived_ = false;
};
