/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "keyboard_input.h"
#include "pointer_input.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/pointer.h>
#include <KWayland/Client/seat.h>

#include <linux/input.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_mouse_keys-0");

class MouseKeysTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testMove();
    void testClick();
};

void MouseKeysTest::initTestCase()
{
    KConfig kaccessConfig("kaccessrc");
    kaccessConfig.group(QStringLiteral("Mouse")).writeEntry("MouseKeys", true);
    kaccessConfig.sync();

    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();
}

void MouseKeysTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandPointer());

    workspace()->setActiveOutput(QPoint(500, 500));
    input()->pointer()->warp(QPoint(500, 500));
}

void MouseKeysTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void MouseKeysTest::testMove()
{
    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Window *waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(1000, 1000), Qt::blue);
    QVERIFY(waylandWindow);

    QSignalSpy pointerSpy(pointer.get(), &KWayland::Client::Pointer::motion);

    quint32 timestamp = 0;

    // left
    Test::keyboardKeyPressed(KEY_KP4, ++timestamp);
    Test::keyboardKeyReleased(KEY_KP4, ++timestamp);
    QVERIFY(pointerSpy.wait());
    QCOMPARE(pointerSpy.first().first().toPointF(), QPointF(495, 500));
    pointerSpy.clear();

    // left up
    Test::keyboardKeyPressed(KEY_KP7, ++timestamp);
    Test::keyboardKeyReleased(KEY_KP7, ++timestamp);
    QVERIFY(pointerSpy.wait());
    QCOMPARE(pointerSpy.first().first().toPointF(), QPointF(490, 495));
    pointerSpy.clear();

    // up
    Test::keyboardKeyPressed(KEY_KP8, ++timestamp);
    Test::keyboardKeyReleased(KEY_KP8, ++timestamp);
    QVERIFY(pointerSpy.wait());
    QCOMPARE(pointerSpy.first().first().toPointF(), QPointF(490, 490));
    pointerSpy.clear();

    // right up
    Test::keyboardKeyPressed(KEY_KP9, ++timestamp);
    Test::keyboardKeyReleased(KEY_KP9, ++timestamp);
    QVERIFY(pointerSpy.wait());
    QCOMPARE(pointerSpy.first().first().toPointF(), QPointF(495, 485));
    pointerSpy.clear();

    // right
    Test::keyboardKeyPressed(KEY_KP6, ++timestamp);
    Test::keyboardKeyReleased(KEY_KP6, ++timestamp);
    QVERIFY(pointerSpy.wait());
    QCOMPARE(pointerSpy.first().first().toPointF(), QPointF(500, 485));
    pointerSpy.clear();

    // right down
    Test::keyboardKeyPressed(KEY_KP3, ++timestamp);
    Test::keyboardKeyReleased(KEY_KP3, ++timestamp);
    QVERIFY(pointerSpy.wait());
    QCOMPARE(pointerSpy.first().first().toPointF(), QPointF(505, 490));
    pointerSpy.clear();

    // down
    Test::keyboardKeyPressed(KEY_KP2, ++timestamp);
    Test::keyboardKeyReleased(KEY_KP2, ++timestamp);
    QVERIFY(pointerSpy.wait());
    QCOMPARE(pointerSpy.first().first().toPointF(), QPointF(505, 495));
    pointerSpy.clear();

    // left down
    Test::keyboardKeyPressed(KEY_KP1, ++timestamp);
    Test::keyboardKeyReleased(KEY_KP1, ++timestamp);
    QVERIFY(pointerSpy.wait());
    QCOMPARE(pointerSpy.first().first().toPointF(), QPointF(500, 500));
    pointerSpy.clear();
}

void MouseKeysTest::testClick()
{
    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Window *waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(1000, 1000), Qt::blue);
    QVERIFY(waylandWindow);

    QSignalSpy pointerSpy(pointer.get(), &KWayland::Client::Pointer::buttonStateChanged);

    quint32 timestamp = 0;

    // left mouse button is default
    Test::keyboardKeyPressed(KEY_KP5, ++timestamp);
    QVERIFY(pointerSpy.wait());
    QCOMPARE(pointerSpy.first()[2], BTN_LEFT);
    QCOMPARE(pointerSpy.first()[3], (int)KWayland::Client::Pointer::ButtonState::Pressed);
    pointerSpy.clear();

    Test::keyboardKeyReleased(KEY_KP5, ++timestamp);
    QVERIFY(pointerSpy.wait());
    QCOMPARE(pointerSpy.first()[2], BTN_LEFT);
    QCOMPARE(pointerSpy.first()[3], (int)KWayland::Client::Pointer::ButtonState::Released);
    pointerSpy.clear();

    // switch to middle
    Test::keyboardKeyPressed(KEY_KPASTERISK, ++timestamp);
    Test::keyboardKeyReleased(KEY_KPASTERISK, ++timestamp);

    Test::keyboardKeyPressed(KEY_KP5, ++timestamp);
    QVERIFY(pointerSpy.wait());
    QCOMPARE(pointerSpy.first()[2], BTN_MIDDLE);
    QCOMPARE(pointerSpy.first()[3], (int)KWayland::Client::Pointer::ButtonState::Pressed);
    pointerSpy.clear();

    Test::keyboardKeyReleased(KEY_KP5, ++timestamp);
    QVERIFY(pointerSpy.wait());
    QCOMPARE(pointerSpy.first()[2], BTN_MIDDLE);
    QCOMPARE(pointerSpy.first()[3], (int)KWayland::Client::Pointer::ButtonState::Released);
    pointerSpy.clear();

    // switch to right
    Test::keyboardKeyPressed(KEY_KPMINUS, ++timestamp);
    Test::keyboardKeyReleased(KEY_KPMINUS, ++timestamp);

    Test::keyboardKeyPressed(KEY_KP5, ++timestamp);
    QVERIFY(pointerSpy.wait());
    QCOMPARE(pointerSpy.first()[2], BTN_RIGHT);
    QCOMPARE(pointerSpy.first()[3], (int)KWayland::Client::Pointer::ButtonState::Pressed);
    pointerSpy.clear();

    Test::keyboardKeyReleased(KEY_KP5, ++timestamp);
    QVERIFY(pointerSpy.wait());
    QCOMPARE(pointerSpy.first()[2], BTN_RIGHT);
    QCOMPARE(pointerSpy.first()[3], (int)KWayland::Client::Pointer::ButtonState::Released);
    pointerSpy.clear();

    // and back to left
    Test::keyboardKeyPressed(KEY_KPSLASH, ++timestamp);
    Test::keyboardKeyReleased(KEY_KPSLASH, ++timestamp);

    Test::keyboardKeyPressed(KEY_KP5, ++timestamp);
    QVERIFY(pointerSpy.wait());
    QCOMPARE(pointerSpy.first()[2], BTN_LEFT);
    QCOMPARE(pointerSpy.first()[3], (int)KWayland::Client::Pointer::ButtonState::Pressed);
    pointerSpy.clear();

    Test::keyboardKeyReleased(KEY_KP5, ++timestamp);
    QVERIFY(pointerSpy.wait());
    QCOMPARE(pointerSpy.first()[2], BTN_LEFT);
    QCOMPARE(pointerSpy.first()[3], (int)KWayland::Client::Pointer::ButtonState::Released);
    pointerSpy.clear();
}
}

WAYLANDTEST_MAIN(KWin::MouseKeysTest)
#include "mouse_keys_test.moc"
