#include "ui/taskbar_ticker_widget.h"
#include "ui/market_detail_card.h"

#include <QApplication>
#include <QLabel>
#include <QtTest>

class TaskbarWidgetInteractionTest final : public QObject
{
    Q_OBJECT

private slots:
    void leftClickShowsDetailCard()
    {
        TaskbarTickerWidget widget;
        widget.show();
        QTest::qWaitForWindowExposed(&widget);

        QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, widget.rect().center());

        QTRY_VERIFY_WITH_TIMEOUT([]() {
            for(QWidget* window : QApplication::topLevelWidgets())
            {
                if(window->objectName() == QStringLiteral("marketDetailCard")
                   && window->isVisible())
                {
                    return true;
                }
            }
            return false;
        }(), 1000);
    }

    void positionCardShowsFundingRate()
    {
        MarketDetailCard card;
        FuturesPosition position;
        position.market = FuturesMarket::UsdMargined;
        position.symbol = QStringLiteral("BTCUSDT");
        position.side = QStringLiteral("LONG");
        position.amount = 0.01;
        position.fundingRate = 0.0001;
        position.fundingRateAvailable = true;

        card.setPositions({position});

        QLabel* fundingRate = nullptr;
        for(QLabel* label : card.findChildren<QLabel*>())
        {
            if(label->property("role").toString() == QStringLiteral("fundingRate"))
            {
                fundingRate = label;
                break;
            }
        }
        QVERIFY(fundingRate != nullptr);
        QCOMPARE(fundingRate->text(), QStringLiteral("资金 +0.0100%"));
    }
};

QTEST_MAIN(TaskbarWidgetInteractionTest)
#include "taskbar_widget_interaction_test.moc"
