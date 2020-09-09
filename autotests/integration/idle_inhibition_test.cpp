/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_client.h"
#include "platform.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/idleinhibit.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/xdgshell.h>

#include <KWaylandServer/display.h>
#include <KWaylandServer/idle_interface.h>

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
    qRegisterMetaType<KWin::AbstractClient*>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    waylandServer()->initWorkspace();
}

void TestIdleInhibition::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::IdleInhibition));

}

void TestIdleInhibition::cleanup()
{
    Test::destroyWaylandConnection();

    VirtualDesktopManager::self()->setCount(1);
    QCOMPARE(VirtualDesktopManager::self()->count(), 1u);
}

void TestIdleInhibition::testInhibit()
{
    auto idle = waylandServer()->display()->findChild<IdleInterface*>();
    QVERIFY(idle);
    QVERIFY(!idle->isInhibited());
    QSignalSpy inhibitedSpy(idle, &IdleInterface::inhibitedChanged);
    QVERIFY(inhibitedSpy.isValid());

    // now create window
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));

    // now create inhibition on window
    QScopedPointer<IdleInhibitor> inhibitor(Test::waylandIdleInhibitManager()->createInhibitor(surface.data()));
    QVERIFY(inhibitor->isValid());

    // render the client
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);

    // this should inhibit our server object
    QVERIFY(idle->isInhibited());

    // deleting the object should uninhibit again
    inhibitor.reset();
    QVERIFY(inhibitedSpy.wait());
    QVERIFY(!idle->isInhibited());

    // inhibit again and destroy window
    Test::waylandIdleInhibitManager()->createInhibitor(surface.data(), surface.data());
    QVERIFY(inhibitedSpy.wait());
    QVERIFY(idle->isInhibited());

    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(c));
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

    // Create the test client.
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());

    // Create the inhibitor object.
    QScopedPointer<IdleInhibitor> inhibitor(Test::waylandIdleInhibitManager()->createInhibitor(surface.data()));
    QVERIFY(inhibitor->isValid());

    // Render the client.
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);

    // The test client should be only on the first virtual desktop.
    QCOMPARE(c->desktops().count(), 1);
    QCOMPARE(c->desktops().first(), VirtualDesktopManager::self()->desktops().first());

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

    // The test client became visible again, so the compositor has to honor the idle
    // inhibitor object back again.
    QVERIFY(idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 3);

    // Destroy the test client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(c));
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

    // Create the test client.
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());

    // Create the inhibitor object.
    QScopedPointer<IdleInhibitor> inhibitor(Test::waylandIdleInhibitManager()->createInhibitor(surface.data()));
    QVERIFY(inhibitor->isValid());

    // Render the client.
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);

    // This should inhibit our server object.
    QVERIFY(idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 1);

    // Minimize the client, the idle inhibitor object should not be honored.
    c->minimize();
    QVERIFY(!idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 2);

    // Unminimize the client, the idle inhibitor object should be honored back again.
    c->unminimize();
    QVERIFY(idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 3);

    // Destroy the test client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(c));
    QTRY_VERIFY(!idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 4);
}

void TestIdleInhibition::testDontInhibitWhenUnmapped()
{
    // This test verifies that the idle inhibitor object is not honored by KWin
    // when the associated client is unmapped.

    // Get reference to the idle interface.
    auto idle = waylandServer()->display()->findChild<IdleInterface *>();
    QVERIFY(idle);
    QVERIFY(!idle->isInhibited());
    QSignalSpy inhibitedSpy(idle, &IdleInterface::inhibitedChanged);
    QVERIFY(inhibitedSpy.isValid());

    // Create the test client.
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());

    // Create the inhibitor object.
    QScopedPointer<IdleInhibitor> inhibitor(Test::waylandIdleInhibitManager()->createInhibitor(surface.data()));
    QVERIFY(inhibitor->isValid());

    // Map the client.
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Test::render(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(clientAddedSpy.isEmpty());
    QVERIFY(clientAddedSpy.wait());
    QCOMPARE(clientAddedSpy.count(), 1);
    AbstractClient *client = clientAddedSpy.last().first().value<AbstractClient *>();
    QVERIFY(client);
    QCOMPARE(client->readyForPainting(), true);

    // The compositor will respond with a configure event when the surface becomes active.
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);

    // This should inhibit our server object.
    QVERIFY(idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 1);

    // Unmap the client.
    surface->attachBuffer(Buffer::Ptr());
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(Test::waitForWindowDestroyed(client));

    // The surface is no longer visible, so the compositor doesn't have to honor the
    // idle inhibitor object.
    QVERIFY(!idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 2);

    // Tell the compositor that we want to map the surface.
    surface->commit(Surface::CommitFlag::None);

    // The compositor will respond with a configure event.
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 2);

    // Map the client.
    Test::render(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(clientAddedSpy.wait());
    QCOMPARE(clientAddedSpy.count(), 2);
    client = clientAddedSpy.last().first().value<AbstractClient *>();
    QVERIFY(client);
    QCOMPARE(client->readyForPainting(), true);

    // The test client became visible again, so the compositor has to honor the idle
    // inhibitor object back again.
    QVERIFY(idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 3);

    // Destroy the test client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
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

    // Create the test client.
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());

    // Create the inhibitor object.
    QScopedPointer<IdleInhibitor> inhibitor(Test::waylandIdleInhibitManager()->createInhibitor(surface.data()));
    QVERIFY(inhibitor->isValid());

    // Render the client.
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);

    // The test client should be only on the first virtual desktop.
    QCOMPARE(c->desktops().count(), 1);
    QCOMPARE(c->desktops().first(), VirtualDesktopManager::self()->desktops().first());

    // This should inhibit our server object.
    QVERIFY(idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 1);

    // Let the client enter the second virtual desktop.
    c->enterDesktop(VirtualDesktopManager::self()->desktops().at(1));
    QCOMPARE(inhibitedSpy.count(), 1);

    // If the client leaves the first virtual desktop, then the associated idle
    // inhibitor object should not be honored.
    c->leaveDesktop(VirtualDesktopManager::self()->desktops().at(0));
    QVERIFY(!idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 2);

    // If the client enters the first desktop, then the associated idle inhibitor
    // object should be honored back again.
    c->enterDesktop(VirtualDesktopManager::self()->desktops().at(0));
    QVERIFY(idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 3);

    // Destroy the test client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(c));
    QTRY_VERIFY(!idle->isInhibited());
    QCOMPARE(inhibitedSpy.count(), 4);
}

WAYLANDTEST_MAIN(TestIdleInhibition)
#include "idle_inhibition_test.moc"
