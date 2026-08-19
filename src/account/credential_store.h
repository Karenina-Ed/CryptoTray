#pragma once

#include <QByteArray>
#include <QString>

struct BinanceCredentials
{
    QByteArray apiKey;
    QByteArray secretKey;

    bool isValid() const { return !apiKey.isEmpty() && !secretKey.isEmpty(); }
};

// 凭据存储与网络服务分离，Secret 不经过 QSettings、日志或普通配置文件。
class CredentialStore final
{
public:
    static BinanceCredentials load();
    static bool save(const BinanceCredentials& credentials, QString* errorMessage = nullptr);
    static bool remove(QString* errorMessage = nullptr);
};

