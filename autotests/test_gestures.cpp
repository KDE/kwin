/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "../gestures.h"

#include <QtTest/QTest>
#include <QSignalSpy>

using namespace KWin;

class GestureTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testSwipeMinFinger_data();
    void testSwipeMinFinger();
    void testSwipeMaxFinger_data();
    void testSwipeMaxFinger();
    void testDirection_data();
    void testDirection();
    void testUnregisterSwipeCancels();
    void testDeleteSwipeCancels();
    void testSwipeCancel_data();
    void testSwipeCancel();
    void testSwipeUpdateCancel();
    void testSwipeUpdateTrigger_data();
    void testSwipeUpdateTrigger();
    void testSwipeMinFingerStart_data();
    void testSwipeMinFingerStart();
    void testSwipeMaxFingerStart_data();
    void testSwipeMaxFingerStart();
    void testSwipeDiagonalCancels_data();
    void testSwipeDiagonalCancels();
};

void GestureTest::testSwipeMinFinger_data()
{
    QTest::addColumn<uint>("count");
    QTest::addColumn<uint>("expectedCount");

    QTest::newRow("0") << 0u << 0u;
    QTest::newRow("1") << 1u << 1u;
    QTest::newRow("10") << 10u << 10u;
}

void GestureTest::testSwipeMinFinger()
{
    SwipeGesture gesture;
    QCOMPARE(gesture.minimumFingerCountIsRelevant(), false);
    QCOMPARE(gesture.minimumFingerCount(), 0u);
    QFETCH(uint, count);
    gesture.setMinimumFingerCount(count);
    QCOMPARE(gesture.minimumFingerCountIsRelevant(), true);
    QTEST(gesture.minimumFingerCount(), "expectedCount");
    gesture.setMinimumFingerCount(0);
    QCOMPARE(gesture.minimumFingerCountIsRelevant(), true);
    QCOMPARE(gesture.minimumFingerCount(), 0u);
}

void GestureTest::testSwipeMaxFinger_data()
{
    QTest::addColumn<uint>("count");
    QTest::addColumn<uint>("expectedCount");

    QTest::newRow("0") << 0u << 0u;
    QTest::newRow("1") << 1u << 1u;
    QTest::newRow("10") << 10u << 10u;
}

void GestureTest::testSwipeMaxFinger()
{
    SwipeGesture gesture;
    QCOMPARE(gesture.maximumFingerCountIsRelevant(), false);
    QCOMPARE(gesture.maximumFingerCount(), 0u);
    QFETCH(uint, count);
    gesture.setMaximumFingerCount(count);
    QCOMPARE(gesture.maximumFingerCountIsRelevant(), true);
    QTEST(gesture.maximumFingerCount(), "expectedCount");
    gesture.setMaximumFingerCount(0);
    QCOMPARE(gesture.maximumFingerCountIsRelevant(), true);
    QCOMPARE(gesture.maximumFingerCount(), 0u);
}

void GestureTest::testDirection_data()
{
    QTest::addColumn<KWin::SwipeGesture::Direction>("direction");

    QTest::newRow("Up") << KWin::SwipeGesture::Direction::Up;
    QTest::newRow("Left") << KWin::SwipeGesture::Direction::Left;
    QTest::newRow("Right") << KWin::SwipeGesture::Direction::Right;
    QTest::newRow("Down") << KWin::SwipeGesture::Direction::Down;
}

void GestureTest::testDirection()
{
    SwipeGesture gesture;
    QCOMPARE(gesture.direction(), SwipeGesture::Direction::Down);
    QFETCH(KWin::SwipeGesture::Direction, direction);
    gesture.setDirection(direction);
    QCOMPARE(gesture.direction(), direction);
    // back to down
    gesture.setDirection(SwipeGesture::Direction::Down);
    QCOMPARE(gesture.direction(), SwipeGesture::Direction::Down);
}

void GestureTest::testUnregisterSwipeCancels()
{
    GestureRecognizer recognizer;
    QScopedPointer<SwipeGesture> gesture(new SwipeGesture);
    QSignalSpy startedSpy(gesture.data(), &SwipeGesture::started);
    QVERIFY(startedSpy.isValid());
    QSignalSpy cancelledSpy(gesture.data(), &SwipeGesture::cancelled);
    QVERIFY(cancelledSpy.isValid());

    recognizer.registerGesture(gesture.data());
    recognizer.startSwipeGesture(1);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(cancelledSpy.count(), 0);
    recognizer.unregisterGesture(gesture.data());
    QCOMPARE(cancelledSpy.count(), 1);

    // delete the gesture should not trigger cancel
    gesture.reset();
    QCOMPARE(cancelledSpy.count(), 1);
}

void GestureTest::testDeleteSwipeCancels()
{
    GestureRecognizer recognizer;
    QScopedPointer<SwipeGesture> gesture(new SwipeGesture);
    QSignalSpy startedSpy(gesture.data(), &SwipeGesture::started);
    QVERIFY(startedSpy.isValid());
    QSignalSpy cancelledSpy(gesture.data(), &SwipeGesture::cancelled);
    QVERIFY(cancelledSpy.isValid());

    recognizer.registerGesture(gesture.data());
    recognizer.startSwipeGesture(1);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(cancelledSpy.count(), 0);
    gesture.reset();
    QCOMPARE(cancelledSpy.count(), 1);
}

void GestureTest::testSwipeCancel_data()
{
    QTest::addColumn<KWin::SwipeGesture::Direction>("direction");

    QTest::newRow("Up") << KWin::SwipeGesture::Direction::Up;
    QTest::newRow("Left") << KWin::SwipeGesture::Direction::Left;
    QTest::newRow("Right") << KWin::SwipeGesture::Direction::Right;
    QTest::newRow("Down") << KWin::SwipeGesture::Direction::Down;
}

void GestureTest::testSwipeCancel()
{
    GestureRecognizer recognizer;
    QScopedPointer<SwipeGesture> gesture(new SwipeGesture);
    QFETCH(SwipeGesture::Direction, direction);
    gesture->setDirection(direction);
    QSignalSpy startedSpy(gesture.data(), &SwipeGesture::started);
    QVERIFY(startedSpy.isValid());
    QSignalSpy cancelledSpy(gesture.data(), &SwipeGesture::cancelled);
    QVERIFY(cancelledSpy.isValid());
    QSignalSpy triggeredSpy(gesture.data(), &SwipeGesture::triggered);
    QVERIFY(triggeredSpy.isValid());

    recognizer.registerGesture(gesture.data());
    recognizer.startSwipeGesture(1);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(cancelledSpy.count(), 0);
    recognizer.cancelSwipeGesture();
    QCOMPARE(cancelledSpy.count(), 1);
    QCOMPARE(triggeredSpy.count(), 0);
}

void GestureTest::testSwipeUpdateCancel()
{
    GestureRecognizer recognizer;
    SwipeGesture upGesture;
    upGesture.setDirection(SwipeGesture::Direction::Up);
    SwipeGesture downGesture;
    downGesture.setDirection(SwipeGesture::Direction::Down);
    SwipeGesture rightGesture;
    rightGesture.setDirection(SwipeGesture::Direction::Right);
    SwipeGesture leftGesture;
    leftGesture.setDirection(SwipeGesture::Direction::Left);

    QSignalSpy upCancelledSpy(&upGesture, &SwipeGesture::cancelled);
    QVERIFY(upCancelledSpy.isValid());
    QSignalSpy downCancelledSpy(&downGesture, &SwipeGesture::cancelled);
    QVERIFY(downCancelledSpy.isValid());
    QSignalSpy rightCancelledSpy(&rightGesture, &SwipeGesture::cancelled);
    QVERIFY(rightCancelledSpy.isValid());
    QSignalSpy leftCancelledSpy(&leftGesture, &SwipeGesture::cancelled);
    QVERIFY(leftCancelledSpy.isValid());

    QSignalSpy upTriggeredSpy(&upGesture, &SwipeGesture::triggered);
    QVERIFY(upTriggeredSpy.isValid());
    QSignalSpy downTriggeredSpy(&downGesture, &SwipeGesture::triggered);
    QVERIFY(downTriggeredSpy.isValid());
    QSignalSpy rightTriggeredSpy(&rightGesture, &SwipeGesture::triggered);
    QVERIFY(rightTriggeredSpy.isValid());
    QSignalSpy leftTriggeredSpy(&leftGesture, &SwipeGesture::triggered);
    QVERIFY(leftTriggeredSpy.isValid());

    recognizer.registerGesture(&upGesture);
    recognizer.registerGesture(&downGesture);
    recognizer.registerGesture(&rightGesture);
    recognizer.registerGesture(&leftGesture);

    recognizer.startSwipeGesture(4);

    // first a down gesture
    recognizer.updateSwipeGesture(QSizeF(1, 20));
    QCOMPARE(upCancelledSpy.count(), 1);
    QCOMPARE(downCancelledSpy.count(), 0);
    QCOMPARE(leftCancelledSpy.count(), 1);
    QCOMPARE(rightCancelledSpy.count(), 1);
    // another down gesture
    recognizer.updateSwipeGesture(QSizeF(-2, 10));
    QCOMPARE(downCancelledSpy.count(), 0);
    // and an up gesture
    recognizer.updateSwipeGesture(QSizeF(-2, -10));
    QCOMPARE(upCancelledSpy.count(), 1);
    QCOMPARE(downCancelledSpy.count(), 1);
    QCOMPARE(leftCancelledSpy.count(), 1);
    QCOMPARE(rightCancelledSpy.count(), 1);

    recognizer.endSwipeGesture();
    QCOMPARE(upCancelledSpy.count(), 1);
    QCOMPARE(downCancelledSpy.count(), 1);
    QCOMPARE(leftCancelledSpy.count(), 1);
    QCOMPARE(rightCancelledSpy.count(), 1);
    QCOMPARE(upTriggeredSpy.count(), 0);
    QCOMPARE(downTriggeredSpy.count(), 0);
    QCOMPARE(leftTriggeredSpy.count(), 0);
    QCOMPARE(rightTriggeredSpy.count(), 0);
}

void GestureTest::testSwipeUpdateTrigger_data()
{
    QTest::addColumn<KWin::SwipeGesture::Direction>("direction");
    QTest::addColumn<QSizeF>("delta");

    QTest::newRow("Up") << KWin::SwipeGesture::Direction::Up << QSizeF(2, -3);
    QTest::newRow("Left") << KWin::SwipeGesture::Direction::Left << QSizeF(-3, 1);
    QTest::newRow("Right") << KWin::SwipeGesture::Direction::Right << QSizeF(20, -19);
    QTest::newRow("Down") << KWin::SwipeGesture::Direction::Down << QSizeF(0, 50);
}

void GestureTest::testSwipeUpdateTrigger()
{
    GestureRecognizer recognizer;
    SwipeGesture gesture;
    QFETCH(SwipeGesture::Direction, direction);
    gesture.setDirection(direction);

    QSignalSpy triggeredSpy(&gesture, &SwipeGesture::triggered);
    QVERIFY(triggeredSpy.isValid());
    QSignalSpy cancelledSpy(&gesture, &SwipeGesture::cancelled);
    QVERIFY(cancelledSpy.isValid());

    recognizer.registerGesture(&gesture);

    recognizer.startSwipeGesture(1);
    QFETCH(QSizeF, delta);
    recognizer.updateSwipeGesture(delta);
    QCOMPARE(cancelledSpy.count(), 0);
    QCOMPARE(triggeredSpy.count(), 0);

    recognizer.endSwipeGesture();
    QCOMPARE(cancelledSpy.count(), 0);
    QCOMPARE(triggeredSpy.count(), 1);
}

void GestureTest::testSwipeMinFingerStart_data()
{
    QTest::addColumn<uint>("min");
    QTest::addColumn<uint>("count");
    QTest::addColumn<bool>("started");

    QTest::newRow("same") << 1u << 1u << true;
    QTest::newRow("less") << 2u << 1u << false;
    QTest::newRow("more") << 1u << 2u << true;
}

void GestureTest::testSwipeMinFingerStart()
{
    GestureRecognizer recognizer;
    SwipeGesture gesture;
    QFETCH(uint, min);
    gesture.setMinimumFingerCount(min);

    QSignalSpy startedSpy(&gesture, &SwipeGesture::started);
    QVERIFY(startedSpy.isValid());

    recognizer.registerGesture(&gesture);
    QFETCH(uint, count);
    recognizer.startSwipeGesture(count);
    QTEST(!startedSpy.isEmpty(), "started");
}

void GestureTest::testSwipeMaxFingerStart_data()
{
    QTest::addColumn<uint>("max");
    QTest::addColumn<uint>("count");
    QTest::addColumn<bool>("started");

    QTest::newRow("same") << 1u << 1u << true;
    QTest::newRow("less") << 2u << 1u << true;
    QTest::newRow("more") << 1u << 2u << false;
}

void GestureTest::testSwipeMaxFingerStart()
{
    GestureRecognizer recognizer;
    SwipeGesture gesture;
    QFETCH(uint, max);
    gesture.setMaximumFingerCount(max);

    QSignalSpy startedSpy(&gesture, &SwipeGesture::started);
    QVERIFY(startedSpy.isValid());

    recognizer.registerGesture(&gesture);
    QFETCH(uint, count);
    recognizer.startSwipeGesture(count);
    QTEST(!startedSpy.isEmpty(), "started");
}

void GestureTest::testSwipeDiagonalCancels_data()
{
    QTest::addColumn<KWin::SwipeGesture::Direction>("direction");

    QTest::newRow("Up") << KWin::SwipeGesture::Direction::Up;
    QTest::newRow("Left") << KWin::SwipeGesture::Direction::Left;
    QTest::newRow("Right") << KWin::SwipeGesture::Direction::Right;
    QTest::newRow("Down") << KWin::SwipeGesture::Direction::Down;
}

void GestureTest::testSwipeDiagonalCancels()
{
    GestureRecognizer recognizer;
    SwipeGesture gesture;
    QFETCH(SwipeGesture::Direction, direction);
    gesture.setDirection(direction);

    QSignalSpy triggeredSpy(&gesture, &SwipeGesture::triggered);
    QVERIFY(triggeredSpy.isValid());
    QSignalSpy cancelledSpy(&gesture, &SwipeGesture::cancelled);
    QVERIFY(cancelledSpy.isValid());

    recognizer.registerGesture(&gesture);

    recognizer.startSwipeGesture(1);
    recognizer.updateSwipeGesture(QSizeF(1, 1));
    QCOMPARE(cancelledSpy.count(), 1);
    QCOMPARE(triggeredSpy.count(), 0);

    recognizer.endSwipeGesture();
    QCOMPARE(cancelledSpy.count(), 1);
    QCOMPARE(triggeredSpy.count(), 0);

}

QTEST_MAIN(GestureTest)
#include "test_gestures.moc"
