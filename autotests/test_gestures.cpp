/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "gestures.h"

#include <QSignalSpy>
#include <QTest>
#include <QtWidgets/qaction.h>
#include <iostream>

using namespace KWin;

class GestureTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testDirection_data();
    void testDirection();

    // swipe only
    void testMinimumX_data();
    void testMinimumX();
    void testMinimumY_data();
    void testMinimumY();
    void testMaximumX_data();
    void testMaximumX();
    void testMaximumY_data();
    void testMaximumY();
    void testStartGeometry();

    // swipe and pinch
    void testUnregisterSwipeCancels();
    void testUnregisterPinchCancels();
    void testDeleteSwipeCancels();
    void testSwipeCancel_data();
    void testSwipeCancel();
    void testSwipeUpdateTrigger_data();
    void testSwipeUpdateTrigger();

    // both
    void testSwipeFingerCount_data();
    void testSwipeFingerCount();
    void testNotEmitCallbacksBeforeDirectionDecided();

    // swipe only
    void testSwipeGeometryStart_data();
    void testSwipeGeometryStart();
};

void GestureTest::testDirection_data()
{
    QTest::addColumn<GestureDirection>("direction");

    QTest::newRow("Up") << GestureDirection::Up;
    QTest::newRow("Left") << GestureDirection::Left;
    QTest::newRow("Right") << GestureDirection::Right;
    QTest::newRow("Down") << GestureDirection::Down;
    QTest::newRow("Contracting") << GestureDirection::Contracting;
    QTest::newRow("Expanding") << GestureDirection::Expanding;
}

void GestureTest::testDirection()
{
    SwipeGesture gesture;
    QCOMPARE(gesture.direction(), 0);
    QFETCH(GestureDirection, direction);
    gesture.setDirection(direction);
    QCOMPARE(gesture.direction(), direction);
    // back to down
    gesture.setDirection(GestureDirection::Down);
    QCOMPARE(gesture.direction(), GestureDirection::Down);
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

void GestureTest::testUnregisterSwipeCancels()
{
    GestureRecognizer recognizer;
    QScopedPointer<SwipeGesture> gesture(new SwipeGesture);
    QSignalSpy startedSpy(gesture.data(), &SwipeGesture::started);
    QVERIFY(startedSpy.isValid());
    QSignalSpy cancelledSpy(gesture.data(), &SwipeGesture::cancelled);
    QVERIFY(cancelledSpy.isValid());

    recognizer.registerSwipeGesture(gesture.data());
    recognizer.startSwipeGesture(1);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(cancelledSpy.count(), 0);
    recognizer.unregisterSwipeGesture(gesture.data());
    QCOMPARE(cancelledSpy.count(), 1);

    // delete the gesture should not trigger cancel
    gesture.reset();
    QCOMPARE(cancelledSpy.count(), 1);
}

void GestureTest::testUnregisterPinchCancels()
{
    GestureRecognizer recognizer;
    QScopedPointer<PinchGesture> gesture(new PinchGesture);
    QSignalSpy startedSpy(gesture.data(), &PinchGesture::started);
    QVERIFY(startedSpy.isValid());
    QSignalSpy cancelledSpy(gesture.data(), &PinchGesture::cancelled);
    QVERIFY(cancelledSpy.isValid());

    recognizer.registerPinchGesture(gesture.data());
    recognizer.startPinchGesture(1);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(cancelledSpy.count(), 0);
    recognizer.unregisterPinchGesture(gesture.data());
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

    recognizer.registerSwipeGesture(gesture.data());
    recognizer.startSwipeGesture(1);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(cancelledSpy.count(), 0);
    gesture.reset();
    QCOMPARE(cancelledSpy.count(), 1);
}

void GestureTest::testSwipeCancel_data()
{
    QTest::addColumn<GestureDirection>("direction");

    QTest::newRow("Up") << GestureDirection::Up;
    QTest::newRow("Left") << GestureDirection::Left;
    QTest::newRow("Right") << GestureDirection::Right;
    QTest::newRow("Down") << GestureDirection::Down;
}

void GestureTest::testSwipeCancel()
{
    GestureRecognizer recognizer;
    QScopedPointer<SwipeGesture> gesture(new SwipeGesture);
    QFETCH(GestureDirection, direction);
    gesture->setDirection(direction);
    QSignalSpy startedSpy(gesture.data(), &SwipeGesture::started);
    QVERIFY(startedSpy.isValid());
    QSignalSpy cancelledSpy(gesture.data(), &SwipeGesture::cancelled);
    QVERIFY(cancelledSpy.isValid());
    QSignalSpy triggeredSpy(gesture.data(), &SwipeGesture::triggered);
    QVERIFY(triggeredSpy.isValid());

    recognizer.registerSwipeGesture(gesture.data());
    recognizer.startSwipeGesture(1);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(cancelledSpy.count(), 0);
    recognizer.cancelSwipeGesture();
    QCOMPARE(cancelledSpy.count(), 1);
    QCOMPARE(triggeredSpy.count(), 0);
}

void GestureTest::testSwipeUpdateTrigger_data()
{
    QTest::addColumn<GestureDirection>("direction");
    QTest::addColumn<QSizeF>("delta");

    QTest::newRow("Up") << GestureDirection::Up << QSizeF(2, -3);
    QTest::newRow("Left") << GestureDirection::Left << QSizeF(-3, 1);
    QTest::newRow("Right") << GestureDirection::Right << QSizeF(20, -19);
    QTest::newRow("Down") << GestureDirection::Down << QSizeF(0, 50);
}

void GestureTest::testSwipeUpdateTrigger()
{
    GestureRecognizer recognizer;
    SwipeGesture gesture;
    QFETCH(GestureDirection, direction);
    gesture.setDirection(direction);

    QSignalSpy triggeredSpy(&gesture, &SwipeGesture::triggered);
    QVERIFY(triggeredSpy.isValid());
    QSignalSpy cancelledSpy(&gesture, &SwipeGesture::cancelled);
    QVERIFY(cancelledSpy.isValid());

    recognizer.registerSwipeGesture(&gesture);

    recognizer.startSwipeGesture(1);
    QFETCH(QSizeF, delta);
    recognizer.updateSwipeGesture(delta);
    QCOMPARE(cancelledSpy.count(), 0);
    QCOMPARE(triggeredSpy.count(), 0);

    recognizer.endSwipeGesture();
    QCOMPARE(cancelledSpy.count(), 0);
    QCOMPARE(triggeredSpy.count(), 1);
}

void GestureTest::testSwipeFingerCount_data()
{
    QTest::addColumn<uint>("expected");
    QTest::addColumn<uint>("count");
    QTest::addColumn<bool>("started"); // Should it accept

    QTest::newRow("acceptable") << 1u << 1u << true;
    QTest::newRow("unacceptable") << 2u << 1u << false;
}

void GestureTest::testSwipeFingerCount()
{
    GestureRecognizer recognizer;
    SwipeGesture gesture;
    QFETCH(uint, expected);
    gesture.addFingerCount(expected);

    QSignalSpy startedSpy(&gesture, &SwipeGesture::started);
    QVERIFY(startedSpy.isValid());

    recognizer.registerSwipeGesture(&gesture);
    QFETCH(uint, count);
    recognizer.startSwipeGesture(count);
    QTEST(!startedSpy.isEmpty(), "started");
}

void GestureTest::testNotEmitCallbacksBeforeDirectionDecided()
{
    GestureRecognizer recognizer;
    SwipeGesture up;
    SwipeGesture down;
    SwipeGesture right;
    PinchGesture expand;
    PinchGesture contract;
    up.setDirection(GestureDirection::Up);
    down.setDirection(GestureDirection::Down);
    right.setDirection(GestureDirection::Right);
    expand.setDirection(GestureDirection::Expanding);
    contract.setDirection(GestureDirection::Contracting);
    recognizer.registerSwipeGesture(&up);
    recognizer.registerSwipeGesture(&down);
    recognizer.registerSwipeGesture(&right);
    recognizer.registerPinchGesture(&expand);
    recognizer.registerPinchGesture(&contract);

    QSignalSpy upSpy(&up, &SwipeGesture::triggerProgress);
    QSignalSpy downSpy(&down, &SwipeGesture::triggerProgress);
    QSignalSpy rightSpy(&right, &SwipeGesture::triggerProgress);
    QSignalSpy expandSpy(&expand, &PinchGesture::triggerProgress);
    QSignalSpy contractSpy(&contract, &PinchGesture::triggerProgress);

    // don't release callback until we know the direction of swipe gesture
    recognizer.startSwipeGesture(4);
    QCOMPARE(upSpy.count(), 0);
    QCOMPARE(downSpy.count(), 0);
    QCOMPARE(rightSpy.count(), 0);

    // up (negative y)
    recognizer.updateSwipeGesture(QSizeF(0, -1.5));
    QCOMPARE(upSpy.count(), 1);
    QCOMPARE(downSpy.count(), 0);
    QCOMPARE(rightSpy.count(), 0);

    // down (positive y)
    // recognizer.updateSwipeGesture(QSizeF(0, 0));
    recognizer.updateSwipeGesture(QSizeF(0, 3));
    QCOMPARE(upSpy.count(), 1);
    QCOMPARE(downSpy.count(), 1);
    QCOMPARE(rightSpy.count(), 0);

    // right
    recognizer.cancelSwipeGesture();
    recognizer.startSwipeGesture(4);
    recognizer.updateSwipeGesture(QSizeF(1, 0));
    QCOMPARE(upSpy.count(), 1);
    QCOMPARE(downSpy.count(), 1);
    QCOMPARE(rightSpy.count(), 1);

    recognizer.cancelSwipeGesture();

    // same test for pinch gestures
    recognizer.startPinchGesture(4);
    QCOMPARE(expandSpy.count(), 0);
    QCOMPARE(contractSpy.count(), 0);

    // contracting
    recognizer.updatePinchGesture(.5, 0, QSizeF(0, 0));
    QCOMPARE(expandSpy.count(), 0);
    QCOMPARE(contractSpy.count(), 1);

    // expanding
    recognizer.updatePinchGesture(1.5, 0, QSizeF(0, 0));
    QCOMPARE(expandSpy.count(), 1);
    QCOMPARE(contractSpy.count(), 1);
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

    recognizer.registerSwipeGesture(&gesture);
    QFETCH(QPointF, startPos);
    recognizer.startSwipeGesture(startPos);
    QTEST(!startedSpy.isEmpty(), "started");
}

QTEST_MAIN(GestureTest)
#include "test_gestures.moc"
