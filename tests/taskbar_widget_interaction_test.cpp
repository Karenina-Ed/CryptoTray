#include "ui/taskbar_ticker_widget.h"

#include <QApplication>
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
};

QTEST_MAIN(TaskbarWidgetInteractionTest)
#include "taskbar_widget_interaction_test.moc"
