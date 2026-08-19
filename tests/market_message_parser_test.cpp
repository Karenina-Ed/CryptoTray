#include "market/market_message_parser.h"

#include <QTest>

#include <cmath>

class MarketMessageParserTest final : public QObject
{
    Q_OBJECT

private slots:
    void parsesMiniTicker();
    void rejectsInvalidMessages_data();
    void rejectsInvalidMessages();
    void handlesZeroOpenPrice();
};

void MarketMessageParserTest::parsesMiniTicker()
{
    const QString message = QStringLiteral(R"({
        "e":"24hrMiniTicker",
        "E":123456789,
        "s":"BTCUSDT",
        "c":"113428.52",
        "o":"110000.00",
        "h":"114000.00",
        "l":"109000.00",
        "v":"12345.0"
    })");

    const std::optional<Ticker> ticker = parseMarketMessage(message);
    QVERIFY(ticker.has_value());
    QCOMPARE(ticker->symbol, QStringLiteral("BTCUSDT"));
    QCOMPARE(ticker->eventTime, 123456789);
    QCOMPARE(ticker->price, 113428.52);
    QCOMPARE(ticker->openPrice, 110000.0);
    QCOMPARE(ticker->highPrice, 114000.0);
    QCOMPARE(ticker->lowPrice, 109000.0);
    QCOMPARE(ticker->volume, 12345.0);
    QVERIFY(qAbs(ticker->changePercent - 3.11683636363636) < 0.0000001);
}

void MarketMessageParserTest::rejectsInvalidMessages_data()
{
    // 数据驱动测试集中覆盖协议外形错误、字段缺失和数字转换失败。
    QTest::addColumn<QString>("message");
    QTest::newRow("empty-object") << QStringLiteral("{}");
    QTest::newRow("null") << QStringLiteral("null");
    QTest::newRow("array") << QStringLiteral("[]");
    QTest::newRow("garbage") << QStringLiteral("garbage");
    QTest::newRow("missing-price")
        << QStringLiteral(R"({"E":1,"s":"BTCUSDT","o":"1","h":"1","l":"1","v":"1"})");
    QTest::newRow("invalid-price")
        << QStringLiteral(R"({"E":1,"s":"BTCUSDT","c":"bad","o":"1","h":"1","l":"1","v":"1"})");
}

void MarketMessageParserTest::rejectsInvalidMessages()
{
    QFETCH(QString, message);
    QVERIFY(!parseMarketMessage(message).has_value());
}

void MarketMessageParserTest::handlesZeroOpenPrice()
{
    const QString message = QStringLiteral(
        R"({"E":1,"s":"BTCUSDT","c":"10","o":"0","h":"10","l":"0","v":"1"})");

    const std::optional<Ticker> ticker = parseMarketMessage(message);
    QVERIFY(ticker.has_value());
    QCOMPARE(ticker->changePercent, 0.0);
    QVERIFY(std::isfinite(ticker->changePercent));
}

QTEST_APPLESS_MAIN(MarketMessageParserTest)

#include "market_message_parser_test.moc"
