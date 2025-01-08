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

Q_DECLARE_METATYPE(SwipeDirection);
Q_DECLARE_METATYPE(PinchDirection);

class GestureTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testMinimumScaleDelta();
    void testUnregisterSwipeCancels();
    void testUnregisterPinchCancels();
    void testDeleteSwipeCancels();
    void testSwipeCancel_data();
    void testSwipeCancel();
    void testSwipeUpdateTrigger_data();
    void testSwipeUpdateTrigger();
    void testMinimumDeltaReached_data();
    void testMinimumDeltaReached();
    void testNotEmitCallbacksBeforeDirectionDecided();
};

void GestureTest::testMinimumDeltaReached_data()
{
    QTest::addColumn<SwipeDirection>("direction");
    QTest::addColumn<QPointF>("delta");
    QTest::addColumn<bool>("reached");
    QTest::addColumn<qreal>("progress");

    QTest::newRow("Up (more)") << SwipeDirection::Up << QPointF(0, -SwipeGesture::s_minimumDelta * 2) << true << 1.0;
    QTest::newRow("Up (exact)") << SwipeDirection::Up << QPointF(0, -SwipeGesture::s_minimumDelta) << true << 1.0;
    QTest::newRow("Up (less)") << SwipeDirection::Up << QPointF(0, -SwipeGesture::s_minimumDelta / 2) << false << 0.5;
    QTest::newRow("Left (more)") << SwipeDirection::Left << QPointF(-SwipeGesture::s_minimumDelta * 2, 0) << true << 1.0;
    QTest::newRow("Left (exact)") << SwipeDirection::Left << QPointF(-SwipeGesture::s_minimumDelta, 0) << true << 1.0;
    QTest::newRow("Left (less)") << SwipeDirection::Left << QPointF(-SwipeGesture::s_minimumDelta / 2, 0) << false << 0.5;
    QTest::newRow("Right (more)") << SwipeDirection::Right << QPointF(SwipeGesture::s_minimumDelta * 2, 0) << true << 1.0;
    QTest::newRow("Right (exact)") << SwipeDirection::Right << QPointF(SwipeGesture::s_minimumDelta, 0) << true << 1.0;
    QTest::newRow("Right (less)") << SwipeDirection::Right << QPointF(SwipeGesture::s_minimumDelta / 2, 0) << false << 0.5;
    QTest::newRow("Down (more)") << SwipeDirection::Down << QPointF(0, SwipeGesture::s_minimumDelta * 2) << true << 1.0;
    QTest::newRow("Down (exact)") << SwipeDirection::Down << QPointF(0, SwipeGesture::s_minimumDelta) << true << 1.0;
    QTest::newRow("Down (less)") << SwipeDirection::Down << QPointF(0, SwipeGesture::s_minimumDelta / 2) << false << 0.5;
}

void GestureTest::testMinimumDeltaReached()
{
    GestureRecognizer recognizer;

    // swipe gesture
    SwipeGesture gesture(1);
    QFETCH(SwipeDirection, direction);
    gesture.setDirection(direction);
    QFETCH(QPointF, delta);
    QFETCH(bool, reached);
    QCOMPARE(gesture.minimumDeltaReached(delta), reached);

    recognizer.registerSwipeGesture(&gesture);

    QSignalSpy startedSpy(&gesture, &SwipeGesture::started);
    QSignalSpy triggeredSpy(&gesture, &SwipeGesture::triggered);
    QSignalSpy cancelledSpy(&gesture, &SwipeGesture::cancelled);
    QSignalSpy progressSpy(&gesture, &SwipeGesture::progress);

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

void GestureTest::testMinimumScaleDelta()
{
    // pinch gesture
    PinchGesture gesture(4);
    gesture.setDirection(PinchDirection::Contracting);

    QCOMPARE(gesture.minimumScaleDeltaReached(1.1), false);
    QCOMPARE(gesture.minimumScaleDeltaReached(1.3), true);

    GestureRecognizer recognizer;
    recognizer.registerPinchGesture(&gesture);

    QSignalSpy startedSpy(&gesture, &PinchGesture::started);
    QSignalSpy triggeredSpy(&gesture, &PinchGesture::triggered);
    QSignalSpy cancelledSpy(&gesture, &PinchGesture::cancelled);
    QSignalSpy progressSpy(&gesture, &PinchGesture::progress);

    recognizer.startPinchGesture(4);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(triggeredSpy.count(), 0);
    QCOMPARE(cancelledSpy.count(), 0);
    QCOMPARE(progressSpy.count(), 0);
}

void GestureTest::testUnregisterSwipeCancels()
{
    GestureRecognizer recognizer;
    auto gesture = std::make_unique<SwipeGesture>(1);
    QSignalSpy startedSpy(gesture.get(), &SwipeGesture::started);
    QSignalSpy cancelledSpy(gesture.get(), &SwipeGesture::cancelled);

    recognizer.registerSwipeGesture(gesture.get());
    recognizer.startSwipeGesture(1);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(cancelledSpy.count(), 0);
    recognizer.unregisterSwipeGesture(gesture.get());
    QCOMPARE(cancelledSpy.count(), 1);

    // delete the gesture should not trigger cancel
    gesture.reset();
    QCOMPARE(cancelledSpy.count(), 1);
}

void GestureTest::testUnregisterPinchCancels()
{
    GestureRecognizer recognizer;
    auto gesture = std::make_unique<PinchGesture>(1);
    QSignalSpy startedSpy(gesture.get(), &PinchGesture::started);
    QSignalSpy cancelledSpy(gesture.get(), &PinchGesture::cancelled);

    recognizer.registerPinchGesture(gesture.get());
    recognizer.startPinchGesture(1);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(cancelledSpy.count(), 0);
    recognizer.unregisterPinchGesture(gesture.get());
    QCOMPARE(cancelledSpy.count(), 1);

    // delete the gesture should not trigger cancel
    gesture.reset();
    QCOMPARE(cancelledSpy.count(), 1);
}

void GestureTest::testDeleteSwipeCancels()
{
    GestureRecognizer recognizer;
    auto gesture = std::make_unique<SwipeGesture>(1);
    QSignalSpy startedSpy(gesture.get(), &SwipeGesture::started);
    QSignalSpy cancelledSpy(gesture.get(), &SwipeGesture::cancelled);

    recognizer.registerSwipeGesture(gesture.get());
    recognizer.startSwipeGesture(1);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(cancelledSpy.count(), 0);
    gesture.reset();
    QCOMPARE(cancelledSpy.count(), 1);
}

void GestureTest::testSwipeCancel_data()
{
    QTest::addColumn<SwipeDirection>("direction");

    QTest::newRow("Up") << SwipeDirection::Up;
    QTest::newRow("Left") << SwipeDirection::Left;
    QTest::newRow("Right") << SwipeDirection::Right;
    QTest::newRow("Down") << SwipeDirection::Down;
}

void GestureTest::testSwipeCancel()
{
    GestureRecognizer recognizer;
    auto gesture = std::make_unique<SwipeGesture>(1);
    QFETCH(SwipeDirection, direction);
    gesture->setDirection(direction);
    QSignalSpy startedSpy(gesture.get(), &SwipeGesture::started);
    QSignalSpy cancelledSpy(gesture.get(), &SwipeGesture::cancelled);
    QSignalSpy triggeredSpy(gesture.get(), &SwipeGesture::triggered);

    recognizer.registerSwipeGesture(gesture.get());
    recognizer.startSwipeGesture(1);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(cancelledSpy.count(), 0);
    recognizer.cancelSwipeGesture();
    QCOMPARE(cancelledSpy.count(), 1);
    QCOMPARE(triggeredSpy.count(), 0);
}

void GestureTest::testSwipeUpdateTrigger_data()
{
    QTest::addColumn<SwipeDirection>("direction");
    QTest::addColumn<QPointF>("delta");

    QTest::newRow("Up") << SwipeDirection::Up << QPointF(2, -300);
    QTest::newRow("Left") << SwipeDirection::Left << QPointF(-200, 1);
    QTest::newRow("Right") << SwipeDirection::Right << QPointF(400, -19);
    QTest::newRow("Down") << SwipeDirection::Down << QPointF(0, 500);
}

void GestureTest::testSwipeUpdateTrigger()
{
    GestureRecognizer recognizer;
    SwipeGesture gesture(1);
    QFETCH(SwipeDirection, direction);
    gesture.setDirection(direction);

    QSignalSpy triggeredSpy(&gesture, &SwipeGesture::triggered);
    QSignalSpy cancelledSpy(&gesture, &SwipeGesture::cancelled);

    recognizer.registerSwipeGesture(&gesture);

    recognizer.startSwipeGesture(1);
    QFETCH(QPointF, delta);
    recognizer.updateSwipeGesture(delta);
    QCOMPARE(cancelledSpy.count(), 0);
    QCOMPARE(triggeredSpy.count(), 0);

    recognizer.endSwipeGesture();
    QCOMPARE(cancelledSpy.count(), 0);
    QCOMPARE(triggeredSpy.count(), 1);
}

void GestureTest::testNotEmitCallbacksBeforeDirectionDecided()
{
    GestureRecognizer recognizer;
    SwipeGesture up{4};
    SwipeGesture down{4};
    SwipeGesture right{4};
    PinchGesture expand{4};
    PinchGesture contract{4};
    up.setDirection(SwipeDirection::Up);
    down.setDirection(SwipeDirection::Down);
    right.setDirection(SwipeDirection::Right);
    expand.setDirection(PinchDirection::Expanding);
    contract.setDirection(PinchDirection::Contracting);
    recognizer.registerSwipeGesture(&up);
    recognizer.registerSwipeGesture(&down);
    recognizer.registerSwipeGesture(&right);
    recognizer.registerPinchGesture(&expand);
    recognizer.registerPinchGesture(&contract);

    QSignalSpy upSpy(&up, &SwipeGesture::progress);
    QSignalSpy downSpy(&down, &SwipeGesture::progress);
    QSignalSpy rightSpy(&right, &SwipeGesture::progress);
    QSignalSpy expandSpy(&expand, &PinchGesture::progress);
    QSignalSpy contractSpy(&contract, &PinchGesture::progress);

    // don't release callback until we know the direction of swipe gesture
    recognizer.startSwipeGesture(4);
    QCOMPARE(upSpy.count(), 0);
    QCOMPARE(downSpy.count(), 0);
    QCOMPARE(rightSpy.count(), 0);

    // up (negative y)
    recognizer.updateSwipeGesture(QPointF(0, -1.5));
    QCOMPARE(upSpy.count(), 1);
    QCOMPARE(downSpy.count(), 0);
    QCOMPARE(rightSpy.count(), 0);

    // down (positive y)
    // recognizer.updateSwipeGesture(QPointF(0, 0));
    recognizer.updateSwipeGesture(QPointF(0, 3));
    QCOMPARE(upSpy.count(), 1);
    QCOMPARE(downSpy.count(), 1);
    QCOMPARE(rightSpy.count(), 0);

    // right
    recognizer.cancelSwipeGesture();
    recognizer.startSwipeGesture(4);
    recognizer.updateSwipeGesture(QPointF(1, 0));
    QCOMPARE(upSpy.count(), 1);
    QCOMPARE(downSpy.count(), 1);
    QCOMPARE(rightSpy.count(), 1);

    recognizer.cancelSwipeGesture();

    // same test for pinch gestures
    recognizer.startPinchGesture(4);
    QCOMPARE(expandSpy.count(), 0);
    QCOMPARE(contractSpy.count(), 0);

    // contracting
    recognizer.updatePinchGesture(.5, 0, QPointF(0, 0));
    QCOMPARE(expandSpy.count(), 0);
    QCOMPARE(contractSpy.count(), 1);

    // expanding
    recognizer.updatePinchGesture(1.5, 0, QPointF(0, 0));
    QCOMPARE(expandSpy.count(), 1);
    QCOMPARE(contractSpy.count(), 1);
}

QTEST_MAIN(GestureTest)
#include "test_gestures.moc"
