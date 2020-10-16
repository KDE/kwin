/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_client.h"
#include "platform.h"
#include "cursor.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

using namespace KWin;
using namespace KWayland::Client;

Q_DECLARE_METATYPE(KWin::Layer)

static const QString s_socketName = QStringLiteral("wayland_test_kwin_plasma_surface-0");

class PlasmaSurfaceTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testRoleOnAllDesktops_data();
    void testRoleOnAllDesktops();
    void testAcceptsFocus_data();
    void testAcceptsFocus();

    void testDesktopIsOpaque();
    void testPanelWindowsCanCover_data();
    void testPanelWindowsCanCover();
    void testOSDPlacement();
    void testOSDPlacementManualPosition();
    void testPanelTypeHasStrut_data();
    void testPanelTypeHasStrut();
    void testPanelActivate_data();
    void testPanelActivate();

private:
    KWayland::Client::Compositor *m_compositor = nullptr;
    PlasmaShell *m_plasmaShell = nullptr;
};

void PlasmaSurfaceTest::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
}

void PlasmaSurfaceTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::PlasmaShell));
    m_compositor = Test::waylandCompositor();
    m_plasmaShell = Test::waylandPlasmaShell();

    KWin::Cursors::self()->mouse()->setPos(640, 512);
}

void PlasmaSurfaceTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void PlasmaSurfaceTest::testRoleOnAllDesktops_data()
{
    QTest::addColumn<PlasmaShellSurface::Role>("role");
    QTest::addColumn<bool>("expectedOnAllDesktops");

    QTest::newRow("Desktop") << PlasmaShellSurface::Role::Desktop << true;
    QTest::newRow("Panel") << PlasmaShellSurface::Role::Panel << true;
    QTest::newRow("OSD") << PlasmaShellSurface::Role::OnScreenDisplay << true;
    QTest::newRow("Normal") << PlasmaShellSurface::Role::Normal << false;
    QTest::newRow("Notification") << PlasmaShellSurface::Role::Notification << true;
    QTest::newRow("ToolTip") << PlasmaShellSurface::Role::ToolTip << true;
    QTest::newRow("CriticalNotification") << PlasmaShellSurface::Role::CriticalNotification << true;
}

void PlasmaSurfaceTest::testRoleOnAllDesktops()
{
    // this test verifies that a XdgShellClient is set on all desktops when the role changes
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QScopedPointer<PlasmaShellSurface> plasmaSurface(m_plasmaShell->createSurface(surface.data()));
    QVERIFY(!plasmaSurface.isNull());

    // now render to map the window
    AbstractClient *c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);

    // currently the role is not yet set, so the window should not be on all desktops
    QCOMPARE(c->isOnAllDesktops(), false);

    // now let's try to change that
    QSignalSpy onAllDesktopsSpy(c, &AbstractClient::desktopChanged);
    QVERIFY(onAllDesktopsSpy.isValid());
    QFETCH(PlasmaShellSurface::Role, role);
    plasmaSurface->setRole(role);
    QFETCH(bool, expectedOnAllDesktops);
    QCOMPARE(onAllDesktopsSpy.wait(), expectedOnAllDesktops);
    QCOMPARE(c->isOnAllDesktops(), expectedOnAllDesktops);

    // let's create a second window where we init a little bit different
    // first creating the PlasmaSurface then the Shell Surface
    QScopedPointer<Surface> surface2(Test::createSurface());
    QVERIFY(!surface2.isNull());
    QScopedPointer<PlasmaShellSurface> plasmaSurface2(m_plasmaShell->createSurface(surface2.data()));
    QVERIFY(!plasmaSurface2.isNull());
    plasmaSurface2->setRole(role);
    QScopedPointer<XdgShellSurface> shellSurface2(Test::createXdgShellStableSurface(surface2.data()));
    QVERIFY(!shellSurface2.isNull());
    auto c2 = Test::renderAndWaitForShown(surface2.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c2);
    QVERIFY(c != c2);

    QCOMPARE(c2->isOnAllDesktops(), expectedOnAllDesktops);
}

void PlasmaSurfaceTest::testAcceptsFocus_data()
{
    QTest::addColumn<PlasmaShellSurface::Role>("role");
    QTest::addColumn<bool>("wantsInput");
    QTest::addColumn<bool>("active");

    QTest::newRow("Desktop") << PlasmaShellSurface::Role::Desktop << true << true;
    QTest::newRow("Panel") << PlasmaShellSurface::Role::Panel << true << false;
    QTest::newRow("OSD") << PlasmaShellSurface::Role::OnScreenDisplay << false << false;
    QTest::newRow("Normal") << PlasmaShellSurface::Role::Normal << true << true;
    QTest::newRow("Notification") << PlasmaShellSurface::Role::Notification << false << false;
    QTest::newRow("ToolTip") << PlasmaShellSurface::Role::ToolTip << false << false;
    QTest::newRow("CriticalNotification") << PlasmaShellSurface::Role::CriticalNotification << false << false;
}

void PlasmaSurfaceTest::testAcceptsFocus()
{
    // this test verifies that some surface roles don't get focus
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QScopedPointer<PlasmaShellSurface> plasmaSurface(m_plasmaShell->createSurface(surface.data()));
    QVERIFY(!plasmaSurface.isNull());
    QFETCH(PlasmaShellSurface::Role, role);
    plasmaSurface->setRole(role);

    // now render to map the window
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(c);
    QTEST(c->wantsInput(), "wantsInput");
    QTEST(c->isActive(), "active");
}

void PlasmaSurfaceTest::testDesktopIsOpaque()
{
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QScopedPointer<PlasmaShellSurface> plasmaSurface(m_plasmaShell->createSurface(surface.data()));
    QVERIFY(!plasmaSurface.isNull());
    plasmaSurface->setRole(PlasmaShellSurface::Role::Desktop);

    // now render to map the window
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(c);
    QCOMPARE(c->windowType(), NET::Desktop);
    QVERIFY(c->isDesktop());

    QVERIFY(!c->hasAlpha());
    QCOMPARE(c->depth(), 24);
}

void PlasmaSurfaceTest::testOSDPlacement()
{
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QScopedPointer<PlasmaShellSurface> plasmaSurface(m_plasmaShell->createSurface(surface.data()));
    QVERIFY(!plasmaSurface.isNull());
    plasmaSurface->setRole(PlasmaShellSurface::Role::OnScreenDisplay);

    // now render and map the window
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(c);
    QCOMPARE(c->windowType(), NET::OnScreenDisplay);
    QVERIFY(c->isOnScreenDisplay());
    QCOMPARE(c->frameGeometry(), QRect(1280 / 2 - 100 / 2, 2 * 1024 / 3 - 50 / 2, 100, 50));

    // change the screen size
    QSignalSpy screensChangedSpy(screens(), &Screens::changed);
    QVERIFY(screensChangedSpy.isValid());
    const QVector<QRect> geometries{QRect(0, 0, 1280, 1024), QRect(1280, 0, 1280, 1024)};
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs",
                              Qt::DirectConnection,
                              Q_ARG(int, 2),
                              Q_ARG(QVector<QRect>, geometries));
    QVERIFY(screensChangedSpy.wait());
    QCOMPARE(screensChangedSpy.count(), 1);
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), geometries.at(0));
    QCOMPARE(screens()->geometry(1), geometries.at(1));

    QCOMPARE(c->frameGeometry(), QRect(1280 / 2 - 100 / 2, 2 * 1024 / 3 - 50 / 2, 100, 50));

    // change size of window
    QSignalSpy frameGeometryChangedSpy(c, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    Test::render(surface.data(), QSize(200, 100), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(c->frameGeometry(), QRect(1280 / 2 - 200 / 2, 2 * 1024 / 3 - 100 / 2, 200, 100));
}

void PlasmaSurfaceTest::testOSDPlacementManualPosition()
{
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<PlasmaShellSurface> plasmaSurface(m_plasmaShell->createSurface(surface.data()));
    QVERIFY(!plasmaSurface.isNull());
    plasmaSurface->setRole(PlasmaShellSurface::Role::OnScreenDisplay);

    plasmaSurface->setPosition(QPoint(50, 70));

    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());

    // now render and map the window
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(c);
    QVERIFY(c->isInitialPositionSet());
    QCOMPARE(c->windowType(), NET::OnScreenDisplay);
    QVERIFY(c->isOnScreenDisplay());
    QCOMPARE(c->frameGeometry(), QRect(50, 70, 100, 50));
}


void PlasmaSurfaceTest::testPanelTypeHasStrut_data()
{
    QTest::addColumn<PlasmaShellSurface::PanelBehavior>("panelBehavior");
    QTest::addColumn<bool>("expectedStrut");
    QTest::addColumn<QRect>("expectedMaxArea");
    QTest::addColumn<KWin::Layer>("expectedLayer");

    QTest::newRow("always visible - xdgWmBase") << PlasmaShellSurface::PanelBehavior::AlwaysVisible << true << QRect(0, 50, 1280, 974) << KWin::DockLayer;
    QTest::newRow("autohide - xdgWmBase") << PlasmaShellSurface::PanelBehavior::AutoHide << false << QRect(0, 0, 1280, 1024) << KWin::AboveLayer;
    QTest::newRow("windows can cover - xdgWmBase") << PlasmaShellSurface::PanelBehavior::WindowsCanCover << false << QRect(0, 0, 1280, 1024) << KWin::NormalLayer;
    QTest::newRow("windows go below - xdgWmBase") << PlasmaShellSurface::PanelBehavior::WindowsGoBelow << false << QRect(0, 0, 1280, 1024) << KWin::AboveLayer;
}

void PlasmaSurfaceTest::testPanelTypeHasStrut()
{
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<QObject> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QScopedPointer<PlasmaShellSurface> plasmaSurface(m_plasmaShell->createSurface(surface.data()));
    QVERIFY(!plasmaSurface.isNull());
    plasmaSurface->setRole(PlasmaShellSurface::Role::Panel);
    plasmaSurface->setPosition(QPoint(0, 0));
    QFETCH(PlasmaShellSurface::PanelBehavior, panelBehavior);
    plasmaSurface->setPanelBehavior(panelBehavior);

    // now render and map the window
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(c);
    QCOMPARE(c->windowType(), NET::Dock);
    QVERIFY(c->isDock());
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 100, 50));
    QTEST(c->hasStrut(), "expectedStrut");
    QTEST(workspace()->clientArea(MaximizeArea, 0, 0), "expectedMaxArea");
    QTEST(c->layer(), "expectedLayer");
}

void PlasmaSurfaceTest::testPanelWindowsCanCover_data()
{
    QTest::addColumn<QRect>("panelGeometry");
    QTest::addColumn<QRect>("windowGeometry");
    QTest::addColumn<QPoint>("triggerPoint");

    QTest::newRow("top-full-edge") << QRect(0, 0, 1280, 30) << QRect(0, 0, 200, 300) << QPoint(100, 0);
    QTest::newRow("top-left-edge") << QRect(0, 0, 1000, 30) << QRect(0, 0, 200, 300) << QPoint(100, 0);
    QTest::newRow("top-right-edge") << QRect(280, 0, 1000, 30) << QRect(1000, 0, 200, 300) << QPoint(1000, 0);
    QTest::newRow("bottom-full-edge") << QRect(0, 994, 1280, 30) << QRect(0, 724, 200, 300) << QPoint(100, 1023);
    QTest::newRow("bottom-left-edge") << QRect(0, 994, 1000, 30) << QRect(0, 724, 200, 300) << QPoint(100, 1023);
    QTest::newRow("bottom-right-edge") << QRect(280, 994, 1000, 30) << QRect(1000, 724, 200, 300) << QPoint(1000, 1023);
    QTest::newRow("left-full-edge") << QRect(0, 0, 30, 1024) << QRect(0, 0, 200, 300) << QPoint(0, 100);
    QTest::newRow("left-top-edge") << QRect(0, 0, 30, 800) << QRect(0, 0, 200, 300) << QPoint(0, 100);
    QTest::newRow("left-bottom-edge") << QRect(0, 200, 30, 824) << QRect(0, 0, 200, 300) << QPoint(0, 250);
    QTest::newRow("right-full-edge") << QRect(1250, 0, 30, 1024) << QRect(1080, 0, 200, 300) << QPoint(1279, 100);
    QTest::newRow("right-top-edge") << QRect(1250, 0, 30, 800) << QRect(1080, 0, 200, 300) << QPoint(1279, 100);
    QTest::newRow("right-bottom-edge") << QRect(1250, 200, 30, 824) << QRect(1080, 0, 200, 300) << QPoint(1279, 250);
}

void PlasmaSurfaceTest::testPanelWindowsCanCover()
{
    // this test verifies the behavior of a panel with windows can cover
    // triggering the screen edge should raise the panel.
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QScopedPointer<PlasmaShellSurface> plasmaSurface(m_plasmaShell->createSurface(surface.data()));
    QVERIFY(!plasmaSurface.isNull());
    plasmaSurface->setRole(PlasmaShellSurface::Role::Panel);
    QFETCH(QRect, panelGeometry);
    plasmaSurface->setPosition(panelGeometry.topLeft());
    plasmaSurface->setPanelBehavior(PlasmaShellSurface::PanelBehavior::WindowsCanCover);

    // now render and map the window
    auto panel = Test::renderAndWaitForShown(surface.data(), panelGeometry.size(), Qt::blue);

    QVERIFY(panel);
    QCOMPARE(panel->windowType(), NET::Dock);
    QVERIFY(panel->isDock());
    QCOMPARE(panel->frameGeometry(), panelGeometry);
    QCOMPARE(panel->hasStrut(), false);
    QCOMPARE(workspace()->clientArea(MaximizeArea, 0, 0), QRect(0, 0, 1280, 1024));
    QCOMPARE(panel->layer(), KWin::NormalLayer);

    // create a Window
    QScopedPointer<Surface> surface2(Test::createSurface());
    QVERIFY(!surface2.isNull());
    QScopedPointer<XdgShellSurface> shellSurface2(Test::createXdgShellStableSurface(surface2.data()));
    QVERIFY(!shellSurface2.isNull());

    QFETCH(QRect, windowGeometry);
    auto c = Test::renderAndWaitForShown(surface2.data(), windowGeometry.size(), Qt::red);

    QVERIFY(c);
    QCOMPARE(c->windowType(), NET::Normal);
    QVERIFY(c->isActive());
    QCOMPARE(c->layer(), KWin::NormalLayer);
    c->move(windowGeometry.topLeft());
    QCOMPARE(c->frameGeometry(), windowGeometry);

    auto stackingOrder = workspace()->stackingOrder();
    QCOMPARE(stackingOrder.count(), 2);
    QCOMPARE(stackingOrder.first(), panel);
    QCOMPARE(stackingOrder.last(), c);

    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);
    QVERIFY(stackingOrderChangedSpy.isValid());
    // trigger screenedge
    QFETCH(QPoint, triggerPoint);
    KWin::Cursors::self()->mouse()->setPos(triggerPoint);
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(stackingOrderChangedSpy.count(), 1);
    stackingOrder = workspace()->stackingOrder();
    QCOMPARE(stackingOrder.count(), 2);
    QCOMPARE(stackingOrder.first(), c);
    QCOMPARE(stackingOrder.last(), panel);
}

void PlasmaSurfaceTest::testPanelActivate_data()
{
    QTest::addColumn<bool>("wantsFocus");
    QTest::addColumn<bool>("active");

    QTest::newRow("no focus") << false << false;
    QTest::newRow("focus") << true << true;
}

void PlasmaSurfaceTest::testPanelActivate()
{
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QScopedPointer<PlasmaShellSurface> plasmaSurface(m_plasmaShell->createSurface(surface.data()));
    QVERIFY(!plasmaSurface.isNull());
    plasmaSurface->setRole(PlasmaShellSurface::Role::Panel);
    QFETCH(bool, wantsFocus);
    plasmaSurface->setPanelTakesFocus(wantsFocus);

    auto panel = Test::renderAndWaitForShown(surface.data(), QSize(100, 200), Qt::blue);

    QVERIFY(panel);
    QCOMPARE(panel->windowType(), NET::Dock);
    QVERIFY(panel->isDock());
    QFETCH(bool, active);
    QCOMPARE(panel->dockWantsInput(), active);
    QCOMPARE(panel->isActive(), active);
}

WAYLANDTEST_MAIN(PlasmaSurfaceTest)
#include "plasma_surface_test.moc"
