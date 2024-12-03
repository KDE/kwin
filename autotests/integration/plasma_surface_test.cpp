/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "pointer_input.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

using namespace KWin;

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
    void testOSDPlacement();
    void testOSDPlacementManualPosition();
    void testPanelActivate_data();
    void testPanelActivate();
    void testMovable_data();
    void testMovable();

private:
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::PlasmaShell *m_plasmaShell = nullptr;
};

void PlasmaSurfaceTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({QRect(0, 0, 1280, 1024)});

    kwinApp()->start();
}

void PlasmaSurfaceTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::PlasmaShell));
    m_compositor = Test::waylandCompositor();
    m_plasmaShell = Test::waylandPlasmaShell();

    KWin::input()->pointer()->warp(QPointF(640, 512));
}

void PlasmaSurfaceTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void PlasmaSurfaceTest::testRoleOnAllDesktops_data()
{
    QTest::addColumn<KWayland::Client::PlasmaShellSurface::Role>("role");
    QTest::addColumn<bool>("expectedOnAllDesktops");

    QTest::newRow("Desktop") << KWayland::Client::PlasmaShellSurface::Role::Desktop << true;
    QTest::newRow("Panel") << KWayland::Client::PlasmaShellSurface::Role::Panel << true;
    QTest::newRow("OSD") << KWayland::Client::PlasmaShellSurface::Role::OnScreenDisplay << true;
    QTest::newRow("Normal") << KWayland::Client::PlasmaShellSurface::Role::Normal << false;
    QTest::newRow("Notification") << KWayland::Client::PlasmaShellSurface::Role::Notification << true;
    QTest::newRow("ToolTip") << KWayland::Client::PlasmaShellSurface::Role::ToolTip << true;
    QTest::newRow("CriticalNotification") << KWayland::Client::PlasmaShellSurface::Role::CriticalNotification << true;
    QTest::newRow("AppletPopup") << KWayland::Client::PlasmaShellSurface::Role::AppletPopup << true;
}

void PlasmaSurfaceTest::testRoleOnAllDesktops()
{
    // this test verifies that a XdgShellClient is set on all desktops when the role changes
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> plasmaSurface(m_plasmaShell->createSurface(surface.get()));
    QVERIFY(plasmaSurface != nullptr);

    // now render to map the window
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);

    // currently the role is not yet set, so the window should not be on all desktops
    QCOMPARE(window->isOnAllDesktops(), false);

    // now let's try to change that
    QSignalSpy onAllDesktopsSpy(window, &Window::desktopsChanged);
    QFETCH(KWayland::Client::PlasmaShellSurface::Role, role);
    plasmaSurface->setRole(role);
    QFETCH(bool, expectedOnAllDesktops);
    QCOMPARE(onAllDesktopsSpy.wait(), expectedOnAllDesktops);
    QCOMPARE(window->isOnAllDesktops(), expectedOnAllDesktops);

    // let's create a second window where we init a little bit different
    // first creating the PlasmaSurface then the Shell Surface
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    QVERIFY(surface2 != nullptr);
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> plasmaSurface2(m_plasmaShell->createSurface(surface2.get()));
    QVERIFY(plasmaSurface2 != nullptr);
    plasmaSurface2->setRole(role);
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    QVERIFY(shellSurface2 != nullptr);
    auto c2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(c2);
    QVERIFY(window != c2);

    QCOMPARE(c2->isOnAllDesktops(), expectedOnAllDesktops);
}

void PlasmaSurfaceTest::testAcceptsFocus_data()
{
    QTest::addColumn<KWayland::Client::PlasmaShellSurface::Role>("role");
    QTest::addColumn<bool>("wantsInput");
    QTest::addColumn<bool>("active");

    QTest::newRow("Desktop") << KWayland::Client::PlasmaShellSurface::Role::Desktop << true << true;
    QTest::newRow("Panel") << KWayland::Client::PlasmaShellSurface::Role::Panel << true << false;
    QTest::newRow("OSD") << KWayland::Client::PlasmaShellSurface::Role::OnScreenDisplay << false << false;
    QTest::newRow("Normal") << KWayland::Client::PlasmaShellSurface::Role::Normal << true << true;
    QTest::newRow("Notification") << KWayland::Client::PlasmaShellSurface::Role::Notification << false << false;
    QTest::newRow("ToolTip") << KWayland::Client::PlasmaShellSurface::Role::ToolTip << false << false;
    QTest::newRow("CriticalNotification") << KWayland::Client::PlasmaShellSurface::Role::CriticalNotification << false << false;
    QTest::newRow("AppletPopup") << KWayland::Client::PlasmaShellSurface::Role::AppletPopup << true << true;
}

void PlasmaSurfaceTest::testAcceptsFocus()
{
    // this test verifies that some surface roles don't get focus
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> plasmaSurface(m_plasmaShell->createSurface(surface.get()));
    QVERIFY(plasmaSurface != nullptr);
    QFETCH(KWayland::Client::PlasmaShellSurface::Role, role);
    plasmaSurface->setRole(role);

    // now render to map the window
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    QVERIFY(window);
    QTEST(window->wantsInput(), "wantsInput");
    QTEST(window->isActive(), "active");
}

void PlasmaSurfaceTest::testOSDPlacement()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> plasmaSurface(m_plasmaShell->createSurface(surface.get()));
    QVERIFY(plasmaSurface != nullptr);
    plasmaSurface->setRole(KWayland::Client::PlasmaShellSurface::Role::OnScreenDisplay);

    // now render and map the window
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    QVERIFY(window);
    QCOMPARE(window->windowType(), WindowType::OnScreenDisplay);
    QVERIFY(window->isOnScreenDisplay());
    QCOMPARE(window->frameGeometry(), QRect(1280 / 2 - 100 / 2, 2 * 1024 / 3 - 50 / 2, 100, 50));

    // change the screen size
    const QList<QRect> geometries{QRect(0, 0, 1280, 1024), QRect(1280, 0, 1280, 1024)};
    Test::setOutputConfig(geometries);
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), geometries[0]);
    QCOMPARE(outputs[1]->geometry(), geometries[1]);

    QCOMPARE(window->frameGeometry(), QRect(1280 / 2 - 100 / 2, 2 * 1024 / 3 - 50 / 2, 100, 50));

    // change size of window
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    Test::render(surface.get(), QSize(200, 100), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(1280 / 2 - 200 / 2, 2 * 1024 / 3 - 100 / 2, 200, 100));
}

void PlasmaSurfaceTest::testOSDPlacementManualPosition()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> plasmaSurface(m_plasmaShell->createSurface(surface.get()));
    QVERIFY(plasmaSurface != nullptr);
    plasmaSurface->setRole(KWayland::Client::PlasmaShellSurface::Role::OnScreenDisplay);

    plasmaSurface->setPosition(QPoint(50, 70));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);

    // now render and map the window
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    QVERIFY(window);
    QVERIFY(!window->isPlaceable());
    QCOMPARE(window->windowType(), WindowType::OnScreenDisplay);
    QVERIFY(window->isOnScreenDisplay());
    QCOMPARE(window->frameGeometry(), QRect(50, 70, 100, 50));
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
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> plasmaSurface(m_plasmaShell->createSurface(surface.get()));
    QVERIFY(plasmaSurface != nullptr);
    plasmaSurface->setRole(KWayland::Client::PlasmaShellSurface::Role::Panel);
    QFETCH(bool, wantsFocus);
    plasmaSurface->setPanelTakesFocus(wantsFocus);

    auto panel = Test::renderAndWaitForShown(surface.get(), QSize(100, 200), Qt::blue);

    QVERIFY(panel);
    QCOMPARE(panel->windowType(), WindowType::Dock);
    QVERIFY(panel->isDock());
    QFETCH(bool, active);
    QCOMPARE(panel->dockWantsInput(), active);
    QCOMPARE(panel->isActive(), active);
}

void PlasmaSurfaceTest::testMovable_data()
{
    QTest::addColumn<KWayland::Client::PlasmaShellSurface::Role>("role");
    QTest::addColumn<bool>("movable");
    QTest::addColumn<bool>("movableAcrossScreens");
    QTest::addColumn<bool>("resizable");

    QTest::newRow("normal") << KWayland::Client::PlasmaShellSurface::Role::Normal << true << true << true;
    QTest::newRow("desktop") << KWayland::Client::PlasmaShellSurface::Role::Desktop << false << false << false;
    QTest::newRow("panel") << KWayland::Client::PlasmaShellSurface::Role::Panel << false << false << false;
    QTest::newRow("osd") << KWayland::Client::PlasmaShellSurface::Role::OnScreenDisplay << false << false << false;
}

void PlasmaSurfaceTest::testMovable()
{
    // this test verifies that certain window types from PlasmaShellSurface are not moveable or resizable
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);

    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    // and a PlasmaShellSurface
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> plasmaSurface(Test::waylandPlasmaShell()->createSurface(surface.get()));
    QVERIFY(plasmaSurface != nullptr);
    QFETCH(KWayland::Client::PlasmaShellSurface::Role, role);
    plasmaSurface->setRole(role);
    // let's render
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    QVERIFY(window);
    QTEST(window->isMovable(), "movable");
    QTEST(window->isMovableAcrossScreens(), "movableAcrossScreens");
    QTEST(window->isResizable(), "resizable");
    surface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

WAYLANDTEST_MAIN(PlasmaSurfaceTest)
#include "plasma_surface_test.moc"
