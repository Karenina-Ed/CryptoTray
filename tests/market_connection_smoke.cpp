#include "market/market_data_service.h"

#include <QCoreApplication>
#include <QSet>
#include <QTimer>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    MarketDataService service;
    QSet<QString> receivedSymbols;

    // 真实网络验证只记录每个目标币种第一次到达，避免行情每秒刷屏。
    QObject::connect(&service, &MarketDataService::tickerUpdated, &app,
                     [&](const Ticker& ticker) {
                         if(ticker.symbol != QStringLiteral("BTCUSDT")
                            && ticker.symbol != QStringLiteral("ETHUSDT"))
                         {
                             return;
                         }

                         if(!receivedSymbols.contains(ticker.symbol))
                         {
                             qInfo() << "[Smoke] received" << ticker.symbol;
                             receivedSymbols.insert(ticker.symbol);
                         }

                         if(receivedSymbols.size() == 2)
                         {
                             service.stop();
                             app.exit(0);
                         }
                     });

    // 网络不可用时明确超时失败，验证程序不会无限等待。
    QTimer::singleShot(20000, &app, [&]() {
        qWarning() << "[Smoke] timed out; received symbols:" << receivedSymbols;
        service.stop();
        app.exit(1);
    });

    service.start();
    return app.exec();
}
