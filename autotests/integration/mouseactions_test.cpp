/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "pointer_input.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>
#include <linux/input.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_mouseactions-0");

class MouseActionsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testMouseActivateRaiseOnReleaseAndPassClick();
};

void MouseActionsTest::initTestCase()
{
    qRegisterMetaType<Window *>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
    });

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
}

void MouseActionsTest::init()
{
    QVERIFY(Test::setupWaylandConnection());

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
}

void MouseActionsTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void MouseActionsTest::testMouseActivateRaiseOnReleaseAndPassClick()
{
    // Create two windows on the left screen.
    auto surface1 = Test::createSurface();
    auto shellSurface1 = Test::createXdgToplevelSurface(surface1.get());
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 100), Qt::blue);
    QVERIFY(window1);
    QVERIFY(window1->isActive());

    auto surface2 = Test::createSurface();
    auto shellSurface2 = Test::createXdgToplevelSurface(surface2.get());
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 100), Qt::blue);
    QVERIFY(window2);
    QVERIFY(window2->isActive());

    window1->move(QPoint(200, 200));
    window2->move(QPoint(300, 300));

    workspace()->activateWindow(window2);
    workspace()->raiseWindow(window2);
    QVERIFY(window2->isActive());
    QCOMPARE_GT(window2->stackingOrder(), window1->stackingOrder());

    uint32_t time = 0;

    Test::pointerMotion(QPoint(250, 250), time++);

    // the window should be activated on press, and raised on release
    Test::pointerButtonPressed(BTN_LEFT, time++);
    QVERIFY(window1->isActive());
    QCOMPARE_GT(window2->stackingOrder(), window1->stackingOrder());
    Test::pointerButtonReleased(BTN_LEFT, time++);
    QVERIFY(window1->isActive());
    QCOMPARE_GT(window1->stackingOrder(), window2->stackingOrder());

    workspace()->activateWindow(window2);
    workspace()->raiseWindow(window2);

    // the window should not be raised if the cursor's moved to a different window in between
    Test::pointerMotion(QPoint(250, 250), time++);
    Test::pointerButtonPressed(BTN_LEFT, time++);
    QVERIFY(window1->isActive());
    QCOMPARE_GT(window2->stackingOrder(), window1->stackingOrder());
    Test::pointerMotion(QPoint(350, 350), time++);
    Test::pointerButtonReleased(BTN_LEFT, time++);
    QCOMPARE_GT(window2->stackingOrder(), window1->stackingOrder());

    workspace()->activateWindow(window2);
    workspace()->raiseWindow(window2);

    // also check if the windows overlap: only the topmost one should be raised on button release
    window1->move(QPoint(250, 250));
    Test::pointerMotion(QPoint(275, 275), time++);
    Test::pointerButtonPressed(BTN_LEFT, time++);
    QVERIFY(window1->isActive());
    QCOMPARE_GT(window2->stackingOrder(), window1->stackingOrder());
    Test::pointerMotion(QPoint(325, 325), time++);
    Test::pointerButtonReleased(BTN_LEFT, time++);
    QCOMPARE_GT(window2->stackingOrder(), window1->stackingOrder());
}

}

WAYLANDTEST_MAIN(KWin::MouseActionsTest)
#include "mouseactions_test.moc"
