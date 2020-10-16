/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "../gestures.h"

#include <QTest>
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
    void testMinimumX_data();
    void testMinimumX();
    void testMinimumY_data();
    void testMinimumY();
    void testMaximumX_data();
    void testMaximumX();
    void testMaximumY_data();
    void testMaximumY();
    void testStartGeometry();
    void testSetMinimumDelta();
    void testMinimumDeltaReached_data();
    void testMinimumDeltaReached();
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
    void testSwipeGeometryStart_data();
    void testSwipeGeometryStart();
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

void GestureTest::testMinimumX_data()
{
    QTest::addColumn<int>("min");

    QTest::newRow("0") << 0;
    QTest::newRow("-1") << -1;
    QTest::newRow("1") << 1;
}

void GestureTest::testMinimumX()
{
    SwipeGesture gesture;
    QCOMPARE(gesture.minimumX(), 0);
    QCOMPARE(gesture.minimumXIsRelevant(), false);
    QFETCH(int, min);
    gesture.setMinimumX(min);
    QCOMPARE(gesture.minimumX(), min);
    QCOMPARE(gesture.minimumXIsRelevant(), true);
}

void GestureTest::testMinimumY_data()
{
    QTest::addColumn<int>("min");

    QTest::newRow("0") << 0;
    QTest::newRow("-1") << -1;
    QTest::newRow("1") << 1;
}

void GestureTest::testMinimumY()
{
    SwipeGesture gesture;
    QCOMPARE(gesture.minimumY(), 0);
    QCOMPARE(gesture.minimumYIsRelevant(), false);
    QFETCH(int, min);
    gesture.setMinimumY(min);
    QCOMPARE(gesture.minimumY(), min);
    QCOMPARE(gesture.minimumYIsRelevant(), true);
}

void GestureTest::testMaximumX_data()
{
    QTest::addColumn<int>("max");

    QTest::newRow("0") << 0;
    QTest::newRow("-1") << -1;
    QTest::newRow("1") << 1;
}

void GestureTest::testMaximumX()
{
    SwipeGesture gesture;
    QCOMPARE(gesture.maximumX(), 0);
    QCOMPARE(gesture.maximumXIsRelevant(), false);
    QFETCH(int, max);
    gesture.setMaximumX(max);
    QCOMPARE(gesture.maximumX(), max);
    QCOMPARE(gesture.maximumXIsRelevant(), true);
}

void GestureTest::testMaximumY_data()
{
    QTest::addColumn<int>("max");

    QTest::newRow("0") << 0;
    QTest::newRow("-1") << -1;
    QTest::newRow("1") << 1;
}

void GestureTest::testMaximumY()
{
    SwipeGesture gesture;
    QCOMPARE(gesture.maximumY(), 0);
    QCOMPARE(gesture.maximumYIsRelevant(), false);
    QFETCH(int, max);
    gesture.setMaximumY(max);
    QCOMPARE(gesture.maximumY(), max);
    QCOMPARE(gesture.maximumYIsRelevant(), true);
}

void GestureTest::testStartGeometry()
{
    SwipeGesture gesture;
    gesture.setStartGeometry(QRect(1, 2, 20, 30));
    QCOMPARE(gesture.minimumXIsRelevant(), true);
    QCOMPARE(gesture.minimumYIsRelevant(), true);
    QCOMPARE(gesture.maximumXIsRelevant(), true);
    QCOMPARE(gesture.maximumYIsRelevant(), true);
    QCOMPARE(gesture.minimumX(), 1);
    QCOMPARE(gesture.minimumY(), 2);
    QCOMPARE(gesture.maximumX(), 21);
    QCOMPARE(gesture.maximumY(), 32);
}

void GestureTest::testSetMinimumDelta()
{
    SwipeGesture gesture;
    QCOMPARE(gesture.isMinimumDeltaRelevant(), false);
    QCOMPARE(gesture.minimumDelta(), QSizeF());
    QCOMPARE(gesture.minimumDeltaReached(QSizeF()), true);
    gesture.setMinimumDelta(QSizeF(2, 3));
    QCOMPARE(gesture.isMinimumDeltaRelevant(), true);
    QCOMPARE(gesture.minimumDelta(), QSizeF(2, 3));
    QCOMPARE(gesture.minimumDeltaReached(QSizeF()), false);
    QCOMPARE(gesture.minimumDeltaReached(QSizeF(2, 3)), true);
}

void GestureTest::testMinimumDeltaReached_data()
{
    QTest::addColumn<KWin::SwipeGesture::Direction>("direction");
    QTest::addColumn<QSizeF>("minimumDelta");
    QTest::addColumn<QSizeF>("delta");
    QTest::addColumn<bool>("reached");
    QTest::addColumn<qreal>("progress");

    QTest::newRow("Up (more)") << KWin::SwipeGesture::Direction::Up << QSizeF(0, -30) << QSizeF(0, -40) << true << 1.0;
    QTest::newRow("Up (exact)") << KWin::SwipeGesture::Direction::Up << QSizeF(0, -30) << QSizeF(0, -30) << true << 1.0;
    QTest::newRow("Up (less)") << KWin::SwipeGesture::Direction::Up << QSizeF(0, -30) << QSizeF(0, -29) << false << 29.0/30.0;
    QTest::newRow("Left (more)") << KWin::SwipeGesture::Direction::Left << QSizeF(-30, -30) << QSizeF(-40, 20) << true << 1.0;
    QTest::newRow("Left (exact)") << KWin::SwipeGesture::Direction::Left << QSizeF(-30, -40) << QSizeF(-30, 0) << true << 1.0;
    QTest::newRow("Left (less)") << KWin::SwipeGesture::Direction::Left << QSizeF(-30, -30) << QSizeF(-29, 0) << false << 29.0/30.0;
    QTest::newRow("Right (more)") << KWin::SwipeGesture::Direction::Right << QSizeF(30, -30) << QSizeF(40, 20) << true << 1.0;
    QTest::newRow("Right (exact)") << KWin::SwipeGesture::Direction::Right << QSizeF(30, -40) << QSizeF(30, 0) << true << 1.0;
    QTest::newRow("Right (less)") << KWin::SwipeGesture::Direction::Right << QSizeF(30, -30) << QSizeF(29, 0) << false << 29.0/30.0;
    QTest::newRow("Down (more)") << KWin::SwipeGesture::Direction::Down << QSizeF(0, 30) << QSizeF(0, 40) << true << 1.0;
    QTest::newRow("Down (exact)") << KWin::SwipeGesture::Direction::Down << QSizeF(0, 30) << QSizeF(0, 30) << true << 1.0;
    QTest::newRow("Down (less)") << KWin::SwipeGesture::Direction::Down << QSizeF(0, 30) << QSizeF(0, 29) << false << 29.0/30.0;
}

void GestureTest::testMinimumDeltaReached()
{
    SwipeGesture gesture;
    QFETCH(SwipeGesture::Direction, direction);
    gesture.setDirection(direction);
    QFETCH(QSizeF, minimumDelta);
    gesture.setMinimumDelta(minimumDelta);
    QFETCH(QSizeF, delta);
    QFETCH(bool, reached);
    QCOMPARE(gesture.minimumDeltaReached(delta), reached);

    GestureRecognizer recognizer;
    recognizer.registerGesture(&gesture);

    QSignalSpy startedSpy(&gesture, &SwipeGesture::started);
    QVERIFY(startedSpy.isValid());
    QSignalSpy triggeredSpy(&gesture, &SwipeGesture::triggered);
    QVERIFY(triggeredSpy.isValid());
    QSignalSpy cancelledSpy(&gesture, &SwipeGesture::cancelled);
    QVERIFY(cancelledSpy.isValid());
    QSignalSpy progressSpy(&gesture, &SwipeGesture::progress);
    QVERIFY(progressSpy.isValid());

    recognizer.startSwipeGesture(1);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(triggeredSpy.count(), 0);
    QCOMPARE(cancelledSpy.count(), 0);
    QCOMPARE(progressSpy.count(), 0);

    recognizer.updateSwipeGesture(delta);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(triggeredSpy.count(), 0);
    QCOMPARE(cancelledSpy.count(), 0);
    QCOMPARE(progressSpy.count(), 1);
    QTEST(progressSpy.first().first().value<qreal>(), "progress");

    recognizer.endSwipeGesture();
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(progressSpy.count(), 1);
    QCOMPARE(triggeredSpy.isEmpty(), !reached);
    QCOMPARE(cancelledSpy.isEmpty(), reached);
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

    QSignalSpy upProgressSpy(&upGesture, &SwipeGesture::progress);
    QVERIFY(upProgressSpy.isValid());
    QSignalSpy downProgressSpy(&downGesture, &SwipeGesture::progress);
    QVERIFY(downProgressSpy.isValid());
    QSignalSpy leftProgressSpy(&leftGesture, &SwipeGesture::progress);
    QVERIFY(leftProgressSpy.isValid());
    QSignalSpy rightProgressSpy(&rightGesture, &SwipeGesture::progress);
    QVERIFY(rightProgressSpy.isValid());

    recognizer.registerGesture(&upGesture);
    recognizer.registerGesture(&downGesture);
    recognizer.registerGesture(&rightGesture);
    recognizer.registerGesture(&leftGesture);

    QCOMPARE(recognizer.startSwipeGesture(4), 4);

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

    QCOMPARE(upProgressSpy.count(), 0);
    QCOMPARE(downProgressSpy.count(), 0);
    QCOMPARE(leftProgressSpy.count(), 0);
    QCOMPARE(rightProgressSpy.count(), 0);
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

void GestureTest::testSwipeGeometryStart_data()
{
    QTest::addColumn<QRect>("geometry");
    QTest::addColumn<QPointF>("startPos");
    QTest::addColumn<bool>("started");

    QTest::newRow("top left") << QRect(0, 0, 10, 20) << QPointF(0, 0) << true;
    QTest::newRow("top right") << QRect(0, 0, 10, 20) << QPointF(10, 0) << true;
    QTest::newRow("bottom left") << QRect(0, 0, 10, 20) << QPointF(0, 20) << true;
    QTest::newRow("bottom right") << QRect(0, 0, 10, 20) << QPointF(10, 20) << true;
    QTest::newRow("x too small") << QRect(10, 20, 30, 40) << QPointF(9, 25) << false;
    QTest::newRow("y too small") << QRect(10, 20, 30, 40) << QPointF(25, 19) << false;
    QTest::newRow("x too large") << QRect(10, 20, 30, 40) << QPointF(41, 25) << false;
    QTest::newRow("y too large") << QRect(10, 20, 30, 40) << QPointF(25, 61) << false;
    QTest::newRow("inside") << QRect(10, 20, 30, 40) << QPointF(25, 25) << true;
}

void GestureTest::testSwipeGeometryStart()
{
    GestureRecognizer recognizer;
    SwipeGesture gesture;
    QFETCH(QRect, geometry);
    gesture.setStartGeometry(geometry);

    QSignalSpy startedSpy(&gesture, &SwipeGesture::started);
    QVERIFY(startedSpy.isValid());

    recognizer.registerGesture(&gesture);
    QFETCH(QPointF, startPos);
    recognizer.startSwipeGesture(startPos);
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
