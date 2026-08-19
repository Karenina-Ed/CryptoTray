#include "credential_store.h"

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <wincred.h>
#endif

namespace
{
#ifdef Q_OS_WIN
constexpr wchar_t CredentialTarget[] = L"CryptoTray/BinanceHmac";

QString windowsErrorMessage(DWORD code)
{
    return QStringLiteral("Windows 凭据错误 %1").arg(code);
}
#endif
}

BinanceCredentials CredentialStore::load()
{
#ifdef Q_OS_WIN
    PCREDENTIALW credential = nullptr;
    if(!CredReadW(CredentialTarget, CRED_TYPE_GENERIC, 0, &credential))
    {
        return {};
    }

    BinanceCredentials result;
    if(credential->UserName != nullptr)
    {
        result.apiKey = QString::fromWCharArray(credential->UserName).toUtf8();
    }
    result.secretKey = QByteArray(reinterpret_cast<const char*>(credential->CredentialBlob),
                                  static_cast<qsizetype>(credential->CredentialBlobSize));
    CredFree(credential);
    return result;
#else
    return {};
#endif
}

bool CredentialStore::save(const BinanceCredentials& credentials, QString* errorMessage)
{
#ifdef Q_OS_WIN
    const std::wstring apiKey = QString::fromUtf8(credentials.apiKey).toStdWString();
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<wchar_t*>(CredentialTarget);
    credential.UserName = const_cast<wchar_t*>(apiKey.c_str());
    credential.CredentialBlobSize = static_cast<DWORD>(credentials.secretKey.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(
        const_cast<char*>(credentials.secretKey.constData()));
    // LOCAL_MACHINE 表示跨当前用户会话保留，但不会漫游到其他电脑。
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if(CredWriteW(&credential, 0))
    {
        return true;
    }
    if(errorMessage != nullptr)
    {
        *errorMessage = windowsErrorMessage(GetLastError());
    }
    return false;
#else
    if(errorMessage != nullptr)
    {
        *errorMessage = QStringLiteral("当前平台不支持 Windows 凭据管理器");
    }
    return false;
#endif
}

bool CredentialStore::remove(QString* errorMessage)
{
#ifdef Q_OS_WIN
    if(CredDeleteW(CredentialTarget, CRED_TYPE_GENERIC, 0))
    {
        return true;
    }
    const DWORD error = GetLastError();
    if(error == ERROR_NOT_FOUND)
    {
        return true;
    }
    if(errorMessage != nullptr)
    {
        *errorMessage = windowsErrorMessage(error);
    }
    return false;
#else
    if(errorMessage != nullptr)
    {
        *errorMessage = QStringLiteral("当前平台不支持 Windows 凭据管理器");
    }
    return false;
#endif
}
