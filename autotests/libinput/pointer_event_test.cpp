/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#include "mock_libinput.h"
#include "../../libinput/device.h"
#include "../../libinput/events.h"

#include <QtTest/QtTest>

#include <linux/input.h>

Q_DECLARE_METATYPE(libinput_event_type)
Q_DECLARE_METATYPE(libinput_button_state)

using namespace KWin::LibInput;

class TestLibinputPointerEvent : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testType_data();
    void testType();
    void testButton_data();
    void testButton();
    void testAxis_data();
    void testAxis();
    void testMotion();
    void testAbsoluteMotion();

private:
    libinput_device *m_nativeDevice = nullptr;
    Device *m_device = nullptr;
};

void TestLibinputPointerEvent::init()
{
    m_nativeDevice = new libinput_device;
    m_nativeDevice->pointer = true;
    m_nativeDevice->deviceSize = QSizeF(12.5, 13.8);
    m_device = new Device(m_nativeDevice);
}

void TestLibinputPointerEvent::cleanup()
{
    delete m_device;
    m_device = nullptr;

    delete m_nativeDevice;
    m_nativeDevice = nullptr;
}

void TestLibinputPointerEvent::testType_data()
{
    QTest::addColumn<libinput_event_type>("type");

    QTest::newRow("motion") << LIBINPUT_EVENT_POINTER_MOTION;
    QTest::newRow("absolute motion") << LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE;
    QTest::newRow("button") << LIBINPUT_EVENT_POINTER_BUTTON;
    QTest::newRow("axis") << LIBINPUT_EVENT_POINTER_AXIS;
}

void TestLibinputPointerEvent::testType()
{
    // this test verifies the initialization of a PointerEvent and the parent Event class
    libinput_event_pointer *pointerEvent = new libinput_event_pointer;
    QFETCH(libinput_event_type, type);
    pointerEvent->type = type;
    pointerEvent->device = m_nativeDevice;

    QScopedPointer<Event> event(Event::create(pointerEvent));
    // API of event
    QCOMPARE(event->type(), type);
    QCOMPARE(event->device(), m_device);
    QCOMPARE(event->nativeDevice(), m_nativeDevice);
    QCOMPARE((libinput_event*)(*event.data()), pointerEvent);
    // verify it's a pointer event
    QVERIFY(dynamic_cast<PointerEvent*>(event.data()));
    QCOMPARE((libinput_event_pointer*)(*dynamic_cast<PointerEvent*>(event.data())), pointerEvent);
}

void TestLibinputPointerEvent::testButton_data()
{
    QTest::addColumn<libinput_button_state>("buttonState");
    QTest::addColumn<KWin::InputRedirection::PointerButtonState>("expectedButtonState");
    QTest::addColumn<quint32>("button");
    QTest::addColumn<quint32>("time");

    QTest::newRow("pressed") << LIBINPUT_BUTTON_STATE_RELEASED << KWin::InputRedirection::PointerButtonReleased << quint32(BTN_RIGHT) << 100u;
    QTest::newRow("released") << LIBINPUT_BUTTON_STATE_PRESSED << KWin::InputRedirection::PointerButtonPressed << quint32(BTN_LEFT) << 200u;
}

void TestLibinputPointerEvent::testButton()
{
    // this test verifies the button press/release
    libinput_event_pointer *pointerEvent = new libinput_event_pointer;
    pointerEvent->device = m_nativeDevice;
    pointerEvent->type = LIBINPUT_EVENT_POINTER_BUTTON;
    QFETCH(libinput_button_state, buttonState);
    pointerEvent->buttonState = buttonState;
    QFETCH(quint32, button);
    pointerEvent->button = button;
    QFETCH(quint32, time);
    pointerEvent->time = time;

    QScopedPointer<Event> event(Event::create(pointerEvent));
    auto pe = dynamic_cast<PointerEvent*>(event.data());
    QVERIFY(pe);
    QCOMPARE(pe->type(), LIBINPUT_EVENT_POINTER_BUTTON);
    QTEST(pe->buttonState(), "expectedButtonState");
    QCOMPARE(pe->button(), button);
    QCOMPARE(pe->time(), time);
    QCOMPARE(pe->timeMicroseconds(), quint64(time * 1000));
}

void TestLibinputPointerEvent::testAxis_data()
{
    QTest::addColumn<bool>("horizontal");
    QTest::addColumn<bool>("vertical");
    QTest::addColumn<QPointF>("value");
    QTest::addColumn<quint32>("time");

    QTest::newRow("horizontal") << true << false << QPointF(3.0, 0.0) << 100u;
    QTest::newRow("vertical") << false << true << QPointF(0.0, 2.5) << 200u;
    QTest::newRow("both") << true << true << QPointF(1.1, 4.2) << 300u;
}

void TestLibinputPointerEvent::testAxis()
{
    // this test verifies pointer axis functionality
    libinput_event_pointer *pointerEvent = new libinput_event_pointer;
    pointerEvent->device = m_nativeDevice;
    pointerEvent->type = LIBINPUT_EVENT_POINTER_AXIS;
    QFETCH(bool, horizontal);
    QFETCH(bool, vertical);
    QFETCH(QPointF, value);
    QFETCH(quint32, time);
    pointerEvent->horizontalAxis = horizontal;
    pointerEvent->verticalAxis = vertical;
    pointerEvent->horizontalAxisValue = value.x();
    pointerEvent->verticalAxisValue = value.y();
    pointerEvent->time = time;

    QScopedPointer<Event> event(Event::create(pointerEvent));
    auto pe = dynamic_cast<PointerEvent*>(event.data());
    QVERIFY(pe);
    QCOMPARE(pe->type(), LIBINPUT_EVENT_POINTER_AXIS);
    QCOMPARE(pe->axis().contains(KWin::InputRedirection::PointerAxisHorizontal), horizontal);
    QCOMPARE(pe->axis().contains(KWin::InputRedirection::PointerAxisVertical), vertical);
    QCOMPARE(pe->axisValue(KWin::InputRedirection::PointerAxisHorizontal), value.x());
    QCOMPARE(pe->axisValue(KWin::InputRedirection::PointerAxisVertical), value.y());
    QCOMPARE(pe->time(), time);
}

void TestLibinputPointerEvent::testMotion()
{
    // this test verifies pointer motion (delta)
    libinput_event_pointer *pointerEvent = new libinput_event_pointer;
    pointerEvent->device = m_nativeDevice;
    pointerEvent->type = LIBINPUT_EVENT_POINTER_MOTION;
    pointerEvent->delta = QSizeF(2.1, 4.5);
    pointerEvent->time = 500u;

    QScopedPointer<Event> event(Event::create(pointerEvent));
    auto pe = dynamic_cast<PointerEvent*>(event.data());
    QVERIFY(pe);
    QCOMPARE(pe->type(), LIBINPUT_EVENT_POINTER_MOTION);
    QCOMPARE(pe->time(), 500u);
    QCOMPARE(pe->delta(), QSizeF(2.1, 4.5));
}

void TestLibinputPointerEvent::testAbsoluteMotion()
{
    // this test verifies absolute pointer motion
    libinput_event_pointer *pointerEvent = new libinput_event_pointer;
    pointerEvent->device = m_nativeDevice;
    pointerEvent->type = LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE;
    pointerEvent->absolutePos = QPointF(6.25, 6.9);
    pointerEvent->time = 500u;

    QScopedPointer<Event> event(Event::create(pointerEvent));
    auto pe = dynamic_cast<PointerEvent*>(event.data());
    QVERIFY(pe);
    QCOMPARE(pe->type(), LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE);
    QCOMPARE(pe->time(), 500u);
    QCOMPARE(pe->absolutePos(), QPointF(6.25, 6.9));
    QCOMPARE(pe->absolutePos(QSize(1280, 1024)), QPointF(640, 512));
}

QTEST_GUILESS_MAIN(TestLibinputPointerEvent)
#include "pointer_event_test.moc"
