/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2019 David Edmundson <davidedmundson@kde.org>
Copyright (C) 2019 Vlad Zahorodnii <vladzzag@gmail.com>

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
#include "cursor.h"
#include "kwin_wayland_test.h"
#include "platform.h"
#include "screens.h"
#include "xdgshellclient.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/xdgdecoration.h>
#include <KWayland/Client/xdgshell.h>

using namespace KWin;
using namespace KWayland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_placement-0");

struct PlaceWindowResult
{
    QSize initiallyConfiguredSize;
    KWayland::Client::XdgShellSurface::States initiallyConfiguredStates;
    QRect finalGeometry;
};

class TestPlacement : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void initTestCase();

    void testPlaceSmart();
    void testPlaceZeroCornered();
    void testPlaceMaximized();
    void testPlaceCentered();
    void testPlaceUnderMouse();
    void testPlaceCascaded();
    void testPlaceRandom();

private:
    void setPlacementPolicy(Placement::Policy policy);
    /*
     * Create a window with the lifespan of parent and return relevant results for testing
     * defaultSize is the buffer size to use if the compositor returns an empty size in the first configure
     * event.
     */
    PlaceWindowResult createAndPlaceWindow(const QSize &defaultSize, QObject *parent);
};

void TestPlacement::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::XdgDecoration |
                                         Test::AdditionalWaylandInterface::PlasmaShell));

    screens()->setCurrent(0);
    KWin::Cursor::setPos(QPoint(512, 512));
}

void TestPlacement::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestPlacement::initTestCase()
{
    qRegisterMetaType<KWin::XdgShellClient *>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void TestPlacement::setPlacementPolicy(Placement::Policy policy)
{
    auto group = kwinApp()->config()->group("Windows");
    group.writeEntry("Placement", Placement::policyToString(policy));
    group.sync();
    Workspace::self()->slotReconfigure();
}

PlaceWindowResult TestPlacement::createAndPlaceWindow(const QSize &defaultSize, QObject *parent)
{
    PlaceWindowResult rc;

    // create a new window
    auto surface = Test::createSurface(parent);
    auto shellSurface = Test::createXdgShellStableSurface(surface, surface, Test::CreationSetup::CreateOnly);
    QSignalSpy configSpy(shellSurface, &XdgShellSurface::configureRequested);
    surface->commit(Surface::CommitFlag::None);
    configSpy.wait();

    rc.initiallyConfiguredSize = configSpy[0][0].toSize();
    rc.initiallyConfiguredStates = configSpy[0][1].value<KWayland::Client::XdgShellSurface::States>();
    shellSurface->ackConfigure(configSpy[0][2].toUInt());

    QSize size = rc.initiallyConfiguredSize;

    if (size.isEmpty()) {
        size = defaultSize;
    }

    auto c = Test::renderAndWaitForShown(surface, size, Qt::red);

    rc.finalGeometry = c->geometry();
    return rc;
}

void TestPlacement::testPlaceSmart()
{
    setPlacementPolicy(Placement::Smart);

    QScopedPointer<QObject> testParent(new QObject); //dumb QObject just for scoping surfaces to the test

    QRegion usedArea;

    for (int i = 0; i < 4; i++) {
        PlaceWindowResult windowPlacement = createAndPlaceWindow(QSize(600, 500), testParent.data());
        // smart placement shouldn't define a size on clients
        QCOMPARE(windowPlacement.initiallyConfiguredSize, QSize(0, 0));
        QCOMPARE(windowPlacement.finalGeometry.size(), QSize(600, 500));

        // exact placement isn't a defined concept that should be tested
        // but the goal of smart placement is to make sure windows don't overlap until they need to
        // 4 windows of 600, 500 should fit without overlap
        QVERIFY(!usedArea.intersects(windowPlacement.finalGeometry));
        usedArea += windowPlacement.finalGeometry;
    }
}

void TestPlacement::testPlaceZeroCornered()
{
    setPlacementPolicy(Placement::ZeroCornered);

    QScopedPointer<QObject> testParent(new QObject);

    for (int i = 0; i < 4; i++) {
        PlaceWindowResult windowPlacement = createAndPlaceWindow(QSize(600, 500), testParent.data());
        // smart placement shouldn't define a size on clients
        QCOMPARE(windowPlacement.initiallyConfiguredSize, QSize(0, 0));
        // size should match our buffer
        QCOMPARE(windowPlacement.finalGeometry.size(), QSize(600, 500));
        //and it should be in the corner
        QCOMPARE(windowPlacement.finalGeometry.topLeft(), QPoint(0, 0));
    }
}

void TestPlacement::testPlaceMaximized()
{
    setPlacementPolicy(Placement::Maximizing);

    // add a top panel
    QScopedPointer<Surface> panelSurface(Test::createSurface());
    QScopedPointer<QObject> panelShellSurface(Test::createXdgShellStableSurface(panelSurface.data()));
    QScopedPointer<PlasmaShellSurface> plasmaSurface(Test::waylandPlasmaShell()->createSurface(panelSurface.data()));
    plasmaSurface->setRole(PlasmaShellSurface::Role::Panel);
    plasmaSurface->setPosition(QPoint(0, 0));
    Test::renderAndWaitForShown(panelSurface.data(), QSize(1280, 20), Qt::blue);

    QScopedPointer<QObject> testParent(new QObject);

    // all windows should be initially maximized with an initial configure size sent
    for (int i = 0; i < 4; i++) {
        PlaceWindowResult windowPlacement = createAndPlaceWindow(QSize(600, 500), testParent.data());
        QVERIFY(windowPlacement.initiallyConfiguredStates & XdgShellSurface::State::Maximized);
        QCOMPARE(windowPlacement.initiallyConfiguredSize, QSize(1280, 1024 - 20));
        QCOMPARE(windowPlacement.finalGeometry, QRect(0, 20, 1280, 1024 - 20)); // under the panel
    }
}

void TestPlacement::testPlaceCentered()
{
    // This test verifies that Centered placement policy works.

    KConfigGroup group = kwinApp()->config()->group("Windows");
    group.writeEntry("Placement", Placement::policyToString(Placement::Centered));
    group.sync();
    workspace()->slotReconfigure();

    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    XdgShellClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::red);
    QVERIFY(client);
    QCOMPARE(client->geometry(), QRect(590, 487, 100, 50));

    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestPlacement::testPlaceUnderMouse()
{
    // This test verifies that Under Mouse placement policy works.

    KConfigGroup group = kwinApp()->config()->group("Windows");
    group.writeEntry("Placement", Placement::policyToString(Placement::UnderMouse));
    group.sync();
    workspace()->slotReconfigure();

    KWin::Cursor::setPos(QPoint(200, 300));
    QCOMPARE(KWin::Cursor::pos(), QPoint(200, 300));

    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    XdgShellClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::red);
    QVERIFY(client);
    QCOMPARE(client->geometry(), QRect(151, 276, 100, 50));

    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestPlacement::testPlaceCascaded()
{
    // This test verifies that Cascaded placement policy works.

    KConfigGroup group = kwinApp()->config()->group("Windows");
    group.writeEntry("Placement", Placement::policyToString(Placement::Cascade));
    group.sync();
    workspace()->slotReconfigure();

    QScopedPointer<Surface> surface1(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface1(Test::createXdgShellStableSurface(surface1.data()));
    XdgShellClient *client1 = Test::renderAndWaitForShown(surface1.data(), QSize(100, 50), Qt::red);
    QVERIFY(client1);
    QCOMPARE(client1->pos(), QPoint(0, 0));
    QCOMPARE(client1->size(), QSize(100, 50));

    QScopedPointer<Surface> surface2(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface2(Test::createXdgShellStableSurface(surface2.data()));
    XdgShellClient *client2 = Test::renderAndWaitForShown(surface2.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client2);
    QCOMPARE(client2->pos(), client1->pos() + workspace()->cascadeOffset(client2));
    QCOMPARE(client2->size(), QSize(100, 50));

    QScopedPointer<Surface> surface3(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface3(Test::createXdgShellStableSurface(surface3.data()));
    XdgShellClient *client3 = Test::renderAndWaitForShown(surface3.data(), QSize(100, 50), Qt::green);
    QVERIFY(client3);
    QCOMPARE(client3->pos(), client2->pos() + workspace()->cascadeOffset(client3));
    QCOMPARE(client3->size(), QSize(100, 50));

    shellSurface3.reset();
    QVERIFY(Test::waitForWindowDestroyed(client3));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(client2));
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowDestroyed(client1));
}

void TestPlacement::testPlaceRandom()
{
    // This test verifies that Random placement policy works.

    KConfigGroup group = kwinApp()->config()->group("Windows");
    group.writeEntry("Placement", Placement::policyToString(Placement::Random));
    group.sync();
    workspace()->slotReconfigure();

    QScopedPointer<Surface> surface1(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface1(Test::createXdgShellStableSurface(surface1.data()));
    XdgShellClient *client1 = Test::renderAndWaitForShown(surface1.data(), QSize(100, 50), Qt::red);
    QVERIFY(client1);
    QCOMPARE(client1->size(), QSize(100, 50));

    QScopedPointer<Surface> surface2(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface2(Test::createXdgShellStableSurface(surface2.data()));
    XdgShellClient *client2 = Test::renderAndWaitForShown(surface2.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client2);
    QVERIFY(client2->pos() != client1->pos());
    QCOMPARE(client2->size(), QSize(100, 50));

    QScopedPointer<Surface> surface3(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface3(Test::createXdgShellStableSurface(surface3.data()));
    XdgShellClient *client3 = Test::renderAndWaitForShown(surface3.data(), QSize(100, 50), Qt::green);
    QVERIFY(client3);
    QVERIFY(client3->pos() != client1->pos());
    QVERIFY(client3->pos() != client2->pos());
    QCOMPARE(client3->size(), QSize(100, 50));

    shellSurface3.reset();
    QVERIFY(Test::waitForWindowDestroyed(client3));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(client2));
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowDestroyed(client1));
}

WAYLANDTEST_MAIN(TestPlacement)
#include "placement_test.moc"
