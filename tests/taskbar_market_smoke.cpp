#include "market/market_data_service.h"
#include "ui/taskbar_ticker_widget.h"

#include <QApplication>
#include <QSet>
#include <QTimer>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    TaskbarTickerWidget widget;
    MarketDataService service;
    QSet<QString> receivedSymbols;

    // 冒烟程序复用正式组件，只在两个永续标的都到达后才视为完整链路成功。
    QObject::connect(&service, &MarketDataService::connectionStateChanged,
                     &widget, &TaskbarTickerWidget::setConnected);
    QObject::connect(&service, &MarketDataService::connectionError,
                     &widget, &TaskbarTickerWidget::setConnectionError);
    QObject::connect(&service, &MarketDataService::tickerUpdated, &app,
                     [&](const Ticker& ticker) {
                         widget.updateTicker(ticker);
                         receivedSymbols.insert(ticker.symbol);
                         if(receivedSymbols.contains(QStringLiteral("BTCUSDT"))
                            && receivedSymbols.contains(QStringLiteral("ETHUSDT")))
                         {
                             service.stop();
                             app.exit(0);
                         }
                     });

    widget.showInTaskbar();
    QTimer::singleShot(0, &service, &MarketDataService::start);
    QTimer::singleShot(20000, &app, [&]() {
        service.stop();
        app.exit(1);
    });
    return app.exec();
}
