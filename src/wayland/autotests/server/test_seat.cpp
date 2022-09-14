/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// WaylandServer
#include "wayland/display.h"
#include "wayland/keyboard_interface.h"
#include "wayland/pointer_interface.h"
#include "wayland/seat_interface.h"

using namespace KWaylandServer;

class TestWaylandServerSeat : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testCapabilities();
    void testName();
    void testPointerButton();
    void testPointerPos();
    void testRepeatInfo();
    void testMultiple();
};

static const QString s_socketName = QStringLiteral("kwin-wayland-server-seat-test-0");

void TestWaylandServerSeat::testCapabilities()
{
    KWaylandServer::Display display;
    display.addSocketName(s_socketName);
    display.start();
    SeatInterface *seat = new SeatInterface(&display, &display);
    QVERIFY(!seat->hasKeyboard());
    QVERIFY(!seat->hasPointer());
    QVERIFY(!seat->hasTouch());

    QSignalSpy keyboardSpy(seat, &SeatInterface::hasKeyboardChanged);
    seat->setHasKeyboard(true);
    QCOMPARE(keyboardSpy.count(), 1);
    QVERIFY(keyboardSpy.last().first().toBool());
    QVERIFY(seat->hasKeyboard());
    seat->setHasKeyboard(false);
    QCOMPARE(keyboardSpy.count(), 2);
    QVERIFY(!keyboardSpy.last().first().toBool());
    QVERIFY(!seat->hasKeyboard());
    seat->setHasKeyboard(false);
    QCOMPARE(keyboardSpy.count(), 2);

    QSignalSpy pointerSpy(seat, &SeatInterface::hasPointerChanged);
    seat->setHasPointer(true);
    QCOMPARE(pointerSpy.count(), 1);
    QVERIFY(pointerSpy.last().first().toBool());
    QVERIFY(seat->hasPointer());
    seat->setHasPointer(false);
    QCOMPARE(pointerSpy.count(), 2);
    QVERIFY(!pointerSpy.last().first().toBool());
    QVERIFY(!seat->hasPointer());
    seat->setHasPointer(false);
    QCOMPARE(pointerSpy.count(), 2);

    QSignalSpy touchSpy(seat, &SeatInterface::hasTouchChanged);
    seat->setHasTouch(true);
    QCOMPARE(touchSpy.count(), 1);
    QVERIFY(touchSpy.last().first().toBool());
    QVERIFY(seat->hasTouch());
    seat->setHasTouch(false);
    QCOMPARE(touchSpy.count(), 2);
    QVERIFY(!touchSpy.last().first().toBool());
    QVERIFY(!seat->hasTouch());
    seat->setHasTouch(false);
    QCOMPARE(touchSpy.count(), 2);
}

void TestWaylandServerSeat::testName()
{
    KWaylandServer::Display display;
    display.addSocketName(s_socketName);
    display.start();
    SeatInterface *seat = new SeatInterface(&display, &display);
    QCOMPARE(seat->name(), QString());

    QSignalSpy nameSpy(seat, &SeatInterface::nameChanged);
    const QString name = QStringLiteral("foobar");
    seat->setName(name);
    QCOMPARE(seat->name(), name);
    QCOMPARE(nameSpy.count(), 1);
    QCOMPARE(nameSpy.first().first().toString(), name);
    seat->setName(name);
    QCOMPARE(nameSpy.count(), 1);
}

void TestWaylandServerSeat::testPointerButton()
{
    KWaylandServer::Display display;
    display.addSocketName(s_socketName);
    display.start();
    SeatInterface *seat = new SeatInterface(&display, &display);
    seat->setHasPointer(true);

    // no button pressed yet, should be released and no serial
    QVERIFY(!seat->isPointerButtonPressed(0));
    QVERIFY(!seat->isPointerButtonPressed(1));
    QCOMPARE(seat->pointerButtonSerial(0), quint32(0));
    QCOMPARE(seat->pointerButtonSerial(1), quint32(0));

    // mark the button as pressed
    seat->notifyPointerButton(0, PointerButtonState::Pressed);
    seat->notifyPointerFrame();
    QVERIFY(seat->isPointerButtonPressed(0));
    QCOMPARE(seat->pointerButtonSerial(0), display.serial());

    // other button should still be unpressed
    QVERIFY(!seat->isPointerButtonPressed(1));
    QCOMPARE(seat->pointerButtonSerial(1), quint32(0));

    // release it again
    seat->notifyPointerButton(0, PointerButtonState::Released);
    seat->notifyPointerFrame();
    QVERIFY(!seat->isPointerButtonPressed(0));
    QCOMPARE(seat->pointerButtonSerial(0), display.serial());
}

void TestWaylandServerSeat::testPointerPos()
{
    KWaylandServer::Display display;
    display.addSocketName(s_socketName);
    display.start();
    SeatInterface *seat = new SeatInterface(&display, &display);
    seat->setHasPointer(true);
    QSignalSpy seatPosSpy(seat, &SeatInterface::pointerPosChanged);

    QCOMPARE(seat->pointerPos(), QPointF());

    seat->notifyPointerMotion(QPointF(10, 15));
    seat->notifyPointerFrame();
    QCOMPARE(seat->pointerPos(), QPointF(10, 15));
    QCOMPARE(seatPosSpy.count(), 1);
    QCOMPARE(seatPosSpy.first().first().toPointF(), QPointF(10, 15));

    seat->notifyPointerMotion(QPointF(10, 15));
    seat->notifyPointerFrame();
    QCOMPARE(seatPosSpy.count(), 1);

    seat->notifyPointerMotion(QPointF(5, 7));
    seat->notifyPointerFrame();
    QCOMPARE(seat->pointerPos(), QPointF(5, 7));
    QCOMPARE(seatPosSpy.count(), 2);
    QCOMPARE(seatPosSpy.first().first().toPointF(), QPointF(10, 15));
    QCOMPARE(seatPosSpy.last().first().toPointF(), QPointF(5, 7));
}

void TestWaylandServerSeat::testRepeatInfo()
{
    KWaylandServer::Display display;
    display.addSocketName(s_socketName);
    display.start();
    SeatInterface *seat = new SeatInterface(&display, &display);
    seat->setHasKeyboard(true);
    QCOMPARE(seat->keyboard()->keyRepeatRate(), 0);
    QCOMPARE(seat->keyboard()->keyRepeatDelay(), 0);
    seat->keyboard()->setRepeatInfo(25, 660);
    QCOMPARE(seat->keyboard()->keyRepeatRate(), 25);
    QCOMPARE(seat->keyboard()->keyRepeatDelay(), 660);
    // setting negative values should result in 0
    seat->keyboard()->setRepeatInfo(-25, -660);
    QCOMPARE(seat->keyboard()->keyRepeatRate(), 0);
    QCOMPARE(seat->keyboard()->keyRepeatDelay(), 0);
}

void TestWaylandServerSeat::testMultiple()
{
    KWaylandServer::Display display;
    display.addSocketName(s_socketName);
    display.start();
    QVERIFY(display.seats().isEmpty());
    SeatInterface *seat1 = new SeatInterface(&display, &display);
    QCOMPARE(display.seats().count(), 1);
    QCOMPARE(display.seats().at(0), seat1);
    SeatInterface *seat2 = new SeatInterface(&display, &display);
    QCOMPARE(display.seats().count(), 2);
    QCOMPARE(display.seats().at(0), seat1);
    QCOMPARE(display.seats().at(1), seat2);
    SeatInterface *seat3 = new SeatInterface(&display, &display);
    QCOMPARE(display.seats().count(), 3);
    QCOMPARE(display.seats().at(0), seat1);
    QCOMPARE(display.seats().at(1), seat2);
    QCOMPARE(display.seats().at(2), seat3);

    delete seat3;
    QCOMPARE(display.seats().count(), 2);
    QCOMPARE(display.seats().at(0), seat1);
    QCOMPARE(display.seats().at(1), seat2);

    delete seat2;
    QCOMPARE(display.seats().count(), 1);
    QCOMPARE(display.seats().at(0), seat1);

    delete seat1;
    QCOMPARE(display.seats().count(), 0);
}

QTEST_GUILESS_MAIN(TestWaylandServerSeat)
#include "test_seat.moc"
