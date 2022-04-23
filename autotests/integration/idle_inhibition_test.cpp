/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "platform.h"
#include "virtualdesktops.h"
#include "wayland/display.h"
#include "wayland/idle_interface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>

using namespace KWin;
using namespace KWayland::Client;
using KWaylandServer::IdleInterface;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_idle_inhbition_test-0");

class TestIdleInhibition : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testInhibit();
    void testDontInhibitWhenNotOnCurrentDesktop();
    void testDontInhibitWhenMinimized();
    void testDontInhibitWhenUnmapped();
    void testDontInhibitWhenLeftCurrentDesktop();
};

void TestIdleInhibition::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    Test::initWaylandWorkspace();
}

void TestIdleInhibition::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::IdleInhibitV1));
}

void TestIdleInhibition::cleanup()
{
    Test::destroyWaylandConnection();

    VirtualDesktopManager::self()->setCount(1);
    QCOMPARE(VirtualDesktopManager::self()->count(), 1u);
}

void TestIdleInhibition::testInhibit()
{
    auto idle = waylandServer()->display()->findChild<IdleInterface *>();
    QVERIFY(idle);
    QVERIFY(!idle->isInhibited());
    QSignalSpy inhibitedSpy(idle, &IdleInterface::inhibitedChanged);
    QVERIFY(inhibitedSpy.isValid());

    // now create window
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));

    // now create inhibition on window
    QScopedPointer<Test::IdleInhibitorV1> inhibitor(Test::createIdleInhibitorV1(surface.data()));
    QVERIFY(inhibitor);

    // render the window
    auto window = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // this should inhibit our server object
    QVERIFY(idle->isInhibited());

    // deleting the object should uninhibit again
    inhibitor.reset();
    QVERIFY(inhibitedSpy.wait());
    QVERIFY(!idle->isInhibited());

    // inhibit again and destroy window
    QScopedPointer<Test::IdleInhibitorV1> inhibitor2(Test::createIdleInhibitorV1(surface.data()));
    QVERIFY(inhibitedSpy.wait());
    QVERIFY(idle->isInhibited());

    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
    QTRY_VERIFY(!idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 4);
}

void TestIdleInhibition::testDontInhibitWhenNotOnCurrentDesktop()
{
    // This test verifies that the idle inhibitor object is not honored when
    // the associated surface is not on the current virtual desktop.

    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);

    // Get reference to the idle interface.
    auto idle = waylandServer()->display()->findChild<IdleInterface *>();
    QVERIFY(idle);
    QVERIFY(!idle->isInhibited());
    QSignalSpy inhibitedSpy(idle, &IdleInterface::inhibitedChanged);
    QVERIFY(inhibitedSpy.isValid());

    // Create the test window.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());

    // Create the inhibitor object.
    QScopedPointer<Test::IdleInhibitorV1> inhibitor(Test::createIdleInhibitorV1(surface.data()));
    QVERIFY(inhibitor);

    // Render the window.
    auto window = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // The test window should be only on the first virtual desktop.
    QCOMPARE(window->desktops().count(), 1);
    QCOMPARE(window->desktops().first(), VirtualDesktopManager::self()->desktops().first());

    // This should inhibit our server object.
    QVERIFY(idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 1);

    // Switch to the second virtual desktop.
    VirtualDesktopManager::self()->setCurrent(2);

    // The surface is no longer visible, so the compositor don't have to honor the
    // idle inhibitor object.
    QVERIFY(!idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 2);

    // Switch back to the first virtual desktop.
    VirtualDesktopManager::self()->setCurrent(1);

    // The test window became visible again, so the compositor has to honor the idle
    // inhibitor object back again.
    QVERIFY(idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 3);

    // Destroy the test window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
    QTRY_VERIFY(!idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 4);
}

void TestIdleInhibition::testDontInhibitWhenMinimized()
{
    // This test verifies that the idle inhibitor object is not honored when the
    // associated surface is minimized.

    // Get reference to the idle interface.
    auto idle = waylandServer()->display()->findChild<IdleInterface *>();
    QVERIFY(idle);
    QVERIFY(!idle->isInhibited());
    QSignalSpy inhibitedSpy(idle, &IdleInterface::inhibitedChanged);
    QVERIFY(inhibitedSpy.isValid());

    // Create the test window.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());

    // Create the inhibitor object.
    QScopedPointer<Test::IdleInhibitorV1> inhibitor(Test::createIdleInhibitorV1(surface.data()));
    QVERIFY(inhibitor);

    // Render the window.
    auto window = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // This should inhibit our server object.
    QVERIFY(idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 1);

    // Minimize the window, the idle inhibitor object should not be honored.
    window->minimize();
    QVERIFY(!idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 2);

    // Unminimize the window, the idle inhibitor object should be honored back again.
    window->unminimize();
    QVERIFY(idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 3);

    // Destroy the test window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
    QTRY_VERIFY(!idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 4);
}

void TestIdleInhibition::testDontInhibitWhenUnmapped()
{
    // This test verifies that the idle inhibitor object is not honored by KWin
    // when the associated window is unmapped.

    // Get reference to the idle interface.
    auto idle = waylandServer()->display()->findChild<IdleInterface *>();
    QVERIFY(idle);
    QVERIFY(!idle->isInhibited());
    QSignalSpy inhibitedSpy(idle, &IdleInterface::inhibitedChanged);
    QVERIFY(inhibitedSpy.isValid());

    // Create the test window.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.isValid());

    // Create the inhibitor object.
    QScopedPointer<Test::IdleInhibitorV1> inhibitor(Test::createIdleInhibitorV1(surface.data()));
    QVERIFY(inhibitor);

    // Map the window.
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowAddedSpy.isValid());
    Test::render(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(windowAddedSpy.isEmpty());
    QVERIFY(windowAddedSpy.wait());
    QCOMPARE(windowAddedSpy.count(), 1);
    Window *window = windowAddedSpy.last().first().value<Window *>();
    QVERIFY(window);
    QCOMPARE(window->readyForPainting(), true);

    // The compositor will respond with a configure event when the surface becomes active.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    // This should inhibit our server object.
    QVERIFY(idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 1);

    // Unmap the window.
    surface->attachBuffer(Buffer::Ptr());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(Test::waitForWindowDestroyed(window));

    // The surface is no longer visible, so the compositor doesn't have to honor the
    // idle inhibitor object.
    QVERIFY(!idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 2);

    // Tell the compositor that we want to map the surface.
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // The compositor will respond with a configure event.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);

    // Map the window.
    Test::render(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(windowAddedSpy.wait());
    QCOMPARE(windowAddedSpy.count(), 2);
    window = windowAddedSpy.last().first().value<Window *>();
    QVERIFY(window);
    QCOMPARE(window->readyForPainting(), true);

    // The test window became visible again, so the compositor has to honor the idle
    // inhibitor object back again.
    QVERIFY(idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 3);

    // Destroy the test window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
    QTRY_VERIFY(!idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 4);
}

void TestIdleInhibition::testDontInhibitWhenLeftCurrentDesktop()
{
    // This test verifies that the idle inhibitor object is not honored by KWin
    // when the associated surface leaves the current virtual desktop.

    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);

    // Get reference to the idle interface.
    auto idle = waylandServer()->display()->findChild<IdleInterface *>();
    QVERIFY(idle);
    QVERIFY(!idle->isInhibited());
    QSignalSpy inhibitedSpy(idle, &IdleInterface::inhibitedChanged);
    QVERIFY(inhibitedSpy.isValid());

    // Create the test window.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());

    // Create the inhibitor object.
    QScopedPointer<Test::IdleInhibitorV1> inhibitor(Test::createIdleInhibitorV1(surface.data()));
    QVERIFY(inhibitor);

    // Render the window.
    auto window = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // The test window should be only on the first virtual desktop.
    QCOMPARE(window->desktops().count(), 1);
    QCOMPARE(window->desktops().first(), VirtualDesktopManager::self()->desktops().first());

    // This should inhibit our server object.
    QVERIFY(idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 1);

    // Let the window enter the second virtual desktop.
    window->enterDesktop(VirtualDesktopManager::self()->desktops().at(1));
    QCOMPARE(inhibitedSpy.count(), 1);

    // If the window leaves the first virtual desktop, then the associated idle
    // inhibitor object should not be honored.
    window->leaveDesktop(VirtualDesktopManager::self()->desktops().at(0));
    QVERIFY(!idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 2);

    // If the window enters the first desktop, then the associated idle inhibitor
    // object should be honored back again.
    window->enterDesktop(VirtualDesktopManager::self()->desktops().at(0));
    QVERIFY(idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 3);

    // Destroy the test window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
    QTRY_VERIFY(!idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 4);
}

WAYLANDTEST_MAIN(TestIdleInhibition)
#include "idle_inhibition_test.moc"
