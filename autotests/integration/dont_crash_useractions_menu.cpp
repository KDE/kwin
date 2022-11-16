/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "cursor.h"
#include "keyboard_input.h"
#include "pointer_input.h"
#include "useractions.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/touch.h>

#include <linux/input.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_dont_crash_useractions_menu-0");

class TestDontCrashUseractionsMenu : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testShowHideShowUseractionsMenu();
};

void TestDontCrashUseractionsMenu::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    // force style to breeze as that's the one which triggered the crash
    QVERIFY(kwinApp()->setStyle(QStringLiteral("breeze")));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void TestDontCrashUseractionsMenu::init()
{
    QVERIFY(Test::setupWaylandConnection());

    workspace()->setActiveOutput(QPoint(640, 512));
    KWin::Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void TestDontCrashUseractionsMenu::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestDontCrashUseractionsMenu::testShowHideShowUseractionsMenu()
{
    // this test creates the condition of BUG 382063
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    auto window = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    workspace()->showWindowMenu(QRect(), window);
    auto userActionsMenu = workspace()->userActionsMenu();
    QTRY_VERIFY(userActionsMenu->isShown());
    QVERIFY(userActionsMenu->hasWindow());

    Test::keyboardKeyPressed(KEY_ESC, 0);
    Test::keyboardKeyReleased(KEY_ESC, 1);
    QTRY_VERIFY(!userActionsMenu->isShown());
    QVERIFY(!userActionsMenu->hasWindow());

    // and show again, this triggers BUG 382063
    workspace()->showWindowMenu(QRect(), window);
    QTRY_VERIFY(userActionsMenu->isShown());
    QVERIFY(userActionsMenu->hasWindow());
}

WAYLANDTEST_MAIN(TestDontCrashUseractionsMenu)
#include "dont_crash_useractions_menu.moc"
