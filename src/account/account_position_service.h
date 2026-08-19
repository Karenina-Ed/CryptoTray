#pragma once

#include "futures_position.h"

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
        Account
    };

    void refresh();
    void sendRequest(FuturesMarket market, RequestKind kind,
                     const QString& baseUrl, const QString& path);
    void handleReply(QNetworkReply* reply, FuturesMarket market, RequestKind kind,
                     int credentialRevision);
    void finishRefresh();

    QNetworkAccessManager network_;
    QTimer refreshTimer_;
    QByteArray apiKey_;
    QByteArray secretKey_;
    FuturesPositions pendingPositions_;
    FuturesAccountOverview pendingOverview_;
    QStringList pendingErrors_;
    int pendingReplies_ = 0;
    int credentialRevision_ = 0;
    bool started_ = false;
};
