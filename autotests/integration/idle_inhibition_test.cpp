/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "input.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>

using namespace KWin;

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

    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    kwinApp()->start();
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
    // no idle inhibitors at the start
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{});

    // now create window
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));

    // now create inhibition on window
    std::unique_ptr<Test::IdleInhibitorV1> inhibitor(Test::createIdleInhibitorV1(surface.get()));
    QVERIFY(inhibitor);

    // render the window
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // this should inhibit our server object
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{window});

    // deleting the object should uninhibit again
    inhibitor.reset();
    Test::flushWaylandConnection(); // don't use QTRY_COMPARE(), it doesn't spin event loop
    QGuiApplication::processEvents();
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{});

    // inhibit again and destroy window
    std::unique_ptr<Test::IdleInhibitorV1> inhibitor2(Test::createIdleInhibitorV1(surface.get()));
    Test::flushWaylandConnection();
    QGuiApplication::processEvents();
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{window});

    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{});
}

void TestIdleInhibition::testDontInhibitWhenNotOnCurrentDesktop()
{
    // This test verifies that the idle inhibitor object is not honored when
    // the associated surface is not on the current virtual desktop.

    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);

    // Create the test window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);

    // Create the inhibitor object.
    std::unique_ptr<Test::IdleInhibitorV1> inhibitor(Test::createIdleInhibitorV1(surface.get()));
    QVERIFY(inhibitor);

    // Render the window.
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // The test window should be only on the first virtual desktop.
    QCOMPARE(window->desktops().count(), 1);
    QCOMPARE(window->desktops().first(), VirtualDesktopManager::self()->desktops().first());

    // This should inhibit our server object.
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{window});

    // Switch to the second virtual desktop.
    VirtualDesktopManager::self()->setCurrent(2);

    // The surface is no longer visible, so the compositor don't have to honor the
    // idle inhibitor object.
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{});

    // Switch back to the first virtual desktop.
    VirtualDesktopManager::self()->setCurrent(1);

    // The test window became visible again, so the compositor has to honor the idle
    // inhibitor object back again.
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{window});

    // Destroy the test window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{});
}

void TestIdleInhibition::testDontInhibitWhenMinimized()
{
    // This test verifies that the idle inhibitor object is not honored when the
    // associated surface is minimized.

    // Create the test window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);

    // Create the inhibitor object.
    std::unique_ptr<Test::IdleInhibitorV1> inhibitor(Test::createIdleInhibitorV1(surface.get()));
    QVERIFY(inhibitor);

    // Render the window.
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // This should inhibit our server object.
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{window});

    // Minimize the window, the idle inhibitor object should not be honored.
    window->setMinimized(true);
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{});

    // Unminimize the window, the idle inhibitor object should be honored back again.
    window->setMinimized(false);
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{window});

    // Destroy the test window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{});
}

void TestIdleInhibition::testDontInhibitWhenUnmapped()
{
    // This test verifies that the idle inhibitor object is not honored by KWin
    // when the associated window is unmapped.

    // Create the test window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);

    // Create the inhibitor object.
    std::unique_ptr<Test::IdleInhibitorV1> inhibitor(Test::createIdleInhibitorV1(surface.get()));
    QVERIFY(inhibitor);

    // Map the window.
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    Test::render(surface.get(), QSize(100, 50), Qt::blue);
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
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{window});

    // Unmap the window.
    surface->attachBuffer(KWayland::Client::Buffer::Ptr());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(Test::waitForWindowClosed(window));

    // The surface is no longer visible, so the compositor doesn't have to honor the
    // idle inhibitor object.
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{});

    // Tell the compositor that we want to map the surface.
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // The compositor will respond with a configure event.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);

    // Map the window.
    Test::render(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(windowAddedSpy.wait());
    QCOMPARE(windowAddedSpy.count(), 2);
    window = windowAddedSpy.last().first().value<Window *>();
    QVERIFY(window);
    QCOMPARE(window->readyForPainting(), true);

    // The test window became visible again, so the compositor has to honor the idle
    // inhibitor object back again.
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{window});

    // Destroy the test window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{});
}

void TestIdleInhibition::testDontInhibitWhenLeftCurrentDesktop()
{
    // This test verifies that the idle inhibitor object is not honored by KWin
    // when the associated surface leaves the current virtual desktop.

    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);

    // Create the test window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);

    // Create the inhibitor object.
    std::unique_ptr<Test::IdleInhibitorV1> inhibitor(Test::createIdleInhibitorV1(surface.get()));
    QVERIFY(inhibitor);

    // Render the window.
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // The test window should be only on the first virtual desktop.
    QCOMPARE(window->desktops().count(), 1);
    QCOMPARE(window->desktops().first(), VirtualDesktopManager::self()->desktops().first());

    // This should inhibit our server object.
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{window});

    // Let the window enter the second virtual desktop.
    window->enterDesktop(VirtualDesktopManager::self()->desktops().at(1));
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{window});

    // If the window leaves the first virtual desktop, then the associated idle
    // inhibitor object should not be honored.
    window->leaveDesktop(VirtualDesktopManager::self()->desktops().at(0));
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{});

    // If the window enters the first desktop, then the associated idle inhibitor
    // object should be honored back again.
    window->enterDesktop(VirtualDesktopManager::self()->desktops().at(0));
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{window});

    // Destroy the test window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
    QCOMPARE(input()->idleInhibitors(), QList<Window *>{});
}

WAYLANDTEST_MAIN(TestIdleInhibition)
#include "idle_inhibition_test.moc"
