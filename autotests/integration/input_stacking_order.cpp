/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "pointer_input.h"
#include "wayland/seat.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

#include <QSignalSpy>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_input_stacking_order-0");

class InputStackingOrderTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testPointerFocusUpdatesOnStackingOrderChange();

private:
    void render(KWayland::Client::Surface *surface);
};

void InputStackingOrderTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    kwinApp()->start();
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
}

void InputStackingOrderTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandPointer());

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
}

void InputStackingOrderTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void InputStackingOrderTest::render(KWayland::Client::Surface *surface)
{
    Test::render(surface, QSize(100, 50), Qt::blue);
    Test::flushWaylandConnection();
}

void InputStackingOrderTest::testPointerFocusUpdatesOnStackingOrderChange()
{
    // this test creates two windows which overlap
    // the pointer is in the overlapping area which means the top most window has focus
    // as soon as the top most window gets lowered the window should lose focus and the
    // other window should gain focus without a mouse event in between

    // create pointer and signal spy for enter and leave signals
    auto pointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &KWayland::Client::Pointer::entered);
    QSignalSpy leftSpy(pointer, &KWayland::Client::Pointer::left);

    // now create the two windows and make them overlap
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::Surface> surface1 = Test::createSurface();
    QVERIFY(surface1);
    std::unique_ptr<Test::XdgToplevel> shellSurface1 = Test::createXdgToplevelSurface(surface1.get());
    QVERIFY(shellSurface1);
    render(surface1.get());
    QVERIFY(windowAddedSpy.wait());
    Window *window1 = workspace()->activeWindow();
    QVERIFY(window1);

    std::unique_ptr<KWayland::Client::Surface> surface2 = Test::createSurface();
    QVERIFY(surface2);
    std::unique_ptr<Test::XdgToplevel> shellSurface2 = Test::createXdgToplevelSurface(surface2.get());
    QVERIFY(shellSurface2);
    render(surface2.get());
    QVERIFY(windowAddedSpy.wait());

    Window *window2 = workspace()->activeWindow();
    QVERIFY(window2);
    QVERIFY(window1 != window2);

    // now make windows overlap
    window2->move(window1->pos());
    QCOMPARE(window1->frameGeometry(), window2->frameGeometry());

    // enter
    Test::pointerMotion(QPointF(25, 25), 1);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 1);
    // window 2 should have focus
    QCOMPARE(pointer->enteredSurface(), surface2.get());
    // also on the server
    QCOMPARE(waylandServer()->seat()->focusedPointerSurface(), window2->surface());

    // raise window 1 above window 2
    QVERIFY(leftSpy.isEmpty());
    workspace()->raiseWindow(window1);
    // should send leave to window2
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.count(), 1);
    // and an enter to window1
    QCOMPARE(enteredSpy.count(), 2);
    QCOMPARE(pointer->enteredSurface(), surface1.get());
    QCOMPARE(waylandServer()->seat()->focusedPointerSurface(), window1->surface());

    // let's destroy window1, that should pass focus to window2 again
    QSignalSpy windowClosedSpy(window1, &Window::closed);
    surface1.reset();
    QVERIFY(windowClosedSpy.wait());
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 3);
    QCOMPARE(pointer->enteredSurface(), surface2.get());
    QCOMPARE(waylandServer()->seat()->focusedPointerSurface(), window2->surface());
}

}

WAYLANDTEST_MAIN(KWin::InputStackingOrderTest)
#include "input_stacking_order.moc"
