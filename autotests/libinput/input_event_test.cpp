/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mock_libinput.h"

#include "backends/libinput/device.h"
#include "input_event.h"

#include <QtTest>

Q_DECLARE_METATYPE(KWin::SwitchEvent::State);

using namespace KWin;
using namespace KWin::LibInput;
using namespace std::literals;

class InputEventsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testInitMouseEvent_data();
    void testInitMouseEvent();
    void testInitKeyEvent_data();
    void testInitKeyEvent();
    void testInitWheelEvent_data();
    void testInitWheelEvent();
    void testInitSwitchEvent_data();
    void testInitSwitchEvent();
};

void InputEventsTest::testInitMouseEvent_data()
{
    QTest::addColumn<QEvent::Type>("type");

    QTest::newRow("Press") << QEvent::MouseButtonPress;
    QTest::newRow("Release") << QEvent::MouseButtonRelease;
    QTest::newRow("Move") << QEvent::MouseMove;
}

void InputEventsTest::testInitMouseEvent()
{
    // this test verifies that a MouseEvent is constructed correctly

    // first create the test LibInput::Device
    libinput_device device;
    Device d(&device);

    QFETCH(QEvent::Type, type);
    // now create our own event
    MouseEvent event(type, QPointF(100, 200), Qt::LeftButton, Qt::LeftButton | Qt::RightButton,
                     Qt::ShiftModifier | Qt::ControlModifier, 300ms, QPointF(1, 2), QPointF(3, 4), &d);
    // and verify the contract of QMouseEvent
    QCOMPARE(event.type(), type);
    QCOMPARE(event.globalPos(), QPoint(100, 200));
    QCOMPARE(event.screenPos(), QPointF(100, 200));
    QCOMPARE(event.localPos(), QPointF(100, 200));
    QCOMPARE(event.button(), Qt::LeftButton);
    QCOMPARE(event.buttons(), Qt::LeftButton | Qt::RightButton);
    QCOMPARE(event.modifiers(), Qt::ShiftModifier | Qt::ControlModifier);
    QCOMPARE(event.timestamp(), 300ms);
    // and our custom argument
    QCOMPARE(event.device(), &d);
    QCOMPARE(event.delta(), QPointF(1, 2));
    QCOMPARE(event.deltaUnaccelerated(), QPointF(3, 4));
}

void InputEventsTest::testInitKeyEvent_data()
{
    QTest::addColumn<QEvent::Type>("type");
    QTest::addColumn<bool>("autorepeat");

    QTest::newRow("Press") << QEvent::KeyPress << false;
    QTest::newRow("Repeat") << QEvent::KeyPress << true;
    QTest::newRow("Release") << QEvent::KeyRelease << false;
}

void InputEventsTest::testInitKeyEvent()
{
    // this test verifies that a KeyEvent is constructed correctly

    // first create the test LibInput::Device
    libinput_device device;
    Device d(&device);

    // setup event
    QFETCH(QEvent::Type, type);
    QFETCH(bool, autorepeat);
    KeyEvent event(type, Qt::Key_Space, Qt::ShiftModifier | Qt::ControlModifier, 200, 300,
                   QStringLiteral(" "), autorepeat, 400ms, &d);
    // and verify the contract of QKeyEvent
    QCOMPARE(event.type(), type);
    QCOMPARE(event.isAutoRepeat(), autorepeat);
    QCOMPARE(event.key(), int(Qt::Key_Space));
    QCOMPARE(event.nativeScanCode(), 200u);
    QCOMPARE(event.nativeVirtualKey(), 300u);
    QCOMPARE(event.text(), QStringLiteral(" "));
    QCOMPARE(event.count(), 1);
    QCOMPARE(event.nativeModifiers(), 0u);
    QCOMPARE(event.modifiers(), Qt::ShiftModifier | Qt::ControlModifier);
    QCOMPARE(event.timestamp(), 400ms);
    // and our custom argument
    QCOMPARE(event.device(), &d);
}

void InputEventsTest::testInitWheelEvent_data()
{
    QTest::addColumn<Qt::Orientation>("orientation");
    QTest::addColumn<qreal>("delta");
    QTest::addColumn<qint32>("deltaV120");
    QTest::addColumn<QPoint>("expectedAngleDelta");

    QTest::newRow("horiz") << Qt::Horizontal << 3.3 << 1 << QPoint(3, 0);
    QTest::newRow("vert") << Qt::Vertical << 2.4 << 2 << QPoint(0, 2);
}

void InputEventsTest::testInitWheelEvent()
{
    // this test verifies that a WheelEvent is constructed correctly

    // first create the test LibInput::Device
    libinput_device device;
    Device d(&device);

    // setup event
    QFETCH(Qt::Orientation, orientation);
    QFETCH(qreal, delta);
    QFETCH(qint32, deltaV120);
    WheelEvent event(QPointF(100, 200), delta, deltaV120, orientation, Qt::LeftButton | Qt::RightButton,
                     Qt::ShiftModifier | Qt::ControlModifier, InputRedirection::PointerAxisSourceWheel, 300ms, &d);
    // compare QWheelEvent contract
    QCOMPARE(event.type(), QEvent::Wheel);
    QCOMPARE(event.position(), QPointF(100, 200));
    QCOMPARE(event.globalPosition(), QPointF(100, 200));
    QCOMPARE(event.buttons(), Qt::LeftButton | Qt::RightButton);
    QCOMPARE(event.modifiers(), Qt::ShiftModifier | Qt::ControlModifier);
    QCOMPARE(event.timestamp(), 300ms);
    QTEST(event.angleDelta(), "expectedAngleDelta");
    QTEST(event.orientation(), "orientation");
    QTEST(event.delta(), "delta");
    QTEST(event.deltaV120(), "deltaV120");
    QCOMPARE(event.axisSource(), InputRedirection::PointerAxisSourceWheel);
    // and our custom argument
    QCOMPARE(event.device(), &d);
}

void InputEventsTest::testInitSwitchEvent_data()
{
    QTest::addColumn<KWin::SwitchEvent::State>("state");
    QTest::addColumn<quint64>("timestamp");

    QTest::newRow("on") << SwitchEvent::State::On << quint64{23456790};
    QTest::newRow("off") << SwitchEvent::State::Off << quint64{45689235987};
}

void InputEventsTest::testInitSwitchEvent()
{
    // this test verifies that a SwitchEvent is constructed correctly
    libinput_device device;
    Device d(&device);

    QFETCH(SwitchEvent::State, state);
    QFETCH(quint64, timestamp);
    SwitchEvent event(state, std::chrono::microseconds(timestamp), &d);

    QCOMPARE(event.state(), state);
    QCOMPARE(event.timestamp(), std::chrono::microseconds(timestamp));
    QCOMPARE(event.device(), &d);
}

QTEST_GUILESS_MAIN(InputEventsTest)
#include "input_event_test.moc"
