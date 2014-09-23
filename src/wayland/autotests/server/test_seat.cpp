/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
// Qt
#include <QtTest/QtTest>
// WaylandServer
#include "../../src/server/display.h"
#include "../../src/server/seat_interface.h"

using namespace KWayland::Server;


class TestWaylandServerSeat : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testCapabilities();
    void testName();
    void testPointerButton();
    void testPointerPos();
    void testDestroyThroughTerminate();
};

static const QString s_socketName = QStringLiteral("kwin-wayland-server-seat-test-0");

void TestWaylandServerSeat::testCapabilities()
{
    Display display;
    display.setSocketName(s_socketName);
    display.start();
    SeatInterface *seat = display.createSeat();
    QVERIFY(!seat->hasKeyboard());
    QVERIFY(!seat->hasPointer());
    QVERIFY(!seat->hasTouch());

    QSignalSpy keyboardSpy(seat, SIGNAL(hasKeyboardChanged(bool)));
    QVERIFY(keyboardSpy.isValid());
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

    QSignalSpy pointerSpy(seat, SIGNAL(hasPointerChanged(bool)));
    QVERIFY(pointerSpy.isValid());
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

    QSignalSpy touchSpy(seat, SIGNAL(hasTouchChanged(bool)));
    QVERIFY(touchSpy.isValid());
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
    Display display;
    display.setSocketName(s_socketName);
    display.start();
    SeatInterface *seat = display.createSeat();
    QCOMPARE(seat->name(), QString());

    QSignalSpy nameSpy(seat, SIGNAL(nameChanged(QString)));
    QVERIFY(nameSpy.isValid());
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
    Display display;
    display.setSocketName(s_socketName);
    display.start();
    SeatInterface *seat = display.createSeat();
    PointerInterface *pointer = seat->pointer();

    // no button pressed yet, should be released and no serial
    QVERIFY(!pointer->isButtonPressed(0));
    QVERIFY(!pointer->isButtonPressed(1));
    QCOMPARE(pointer->buttonSerial(0), quint32(0));
    QCOMPARE(pointer->buttonSerial(1), quint32(0));

    // mark the button as pressed
    pointer->buttonPressed(0);
    QVERIFY(pointer->isButtonPressed(0));
    QCOMPARE(pointer->buttonSerial(0), display.serial());

    // other button should still be unpressed
    QVERIFY(!pointer->isButtonPressed(1));
    QCOMPARE(pointer->buttonSerial(1), quint32(0));

    // release it again
    pointer->buttonReleased(0);
    QVERIFY(!pointer->isButtonPressed(0));
    QCOMPARE(pointer->buttonSerial(0), display.serial());
}

void TestWaylandServerSeat::testPointerPos()
{
    Display display;
    display.setSocketName(s_socketName);
    display.start();
    SeatInterface *seat = display.createSeat();
    PointerInterface *pointer = seat->pointer();
    QSignalSpy posSpy(pointer, SIGNAL(globalPosChanged(QPoint)));
    QVERIFY(posSpy.isValid());

    QCOMPARE(pointer->globalPos(), QPoint());

    pointer->setGlobalPos(QPoint(10, 15));
    QCOMPARE(pointer->globalPos(), QPoint(10, 15));
    QCOMPARE(posSpy.count(), 1);
    QCOMPARE(posSpy.first().first().toPoint(), QPoint(10, 15));

    pointer->setGlobalPos(QPoint(10, 15));
    QCOMPARE(posSpy.count(), 1);
}

void TestWaylandServerSeat::testDestroyThroughTerminate()
{
    Display display;
    display.setSocketName(s_socketName);
    display.start();
    SeatInterface *seat = display.createSeat();
    QSignalSpy destroyedSpy(seat, SIGNAL(destroyed(QObject*)));
    QVERIFY(destroyedSpy.isValid());
    display.terminate();
    QVERIFY(!destroyedSpy.isEmpty());
}

QTEST_GUILESS_MAIN(TestWaylandServerSeat)
#include "test_seat.moc"
