/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_showing_desktop-0");

class ShowingDesktopTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testRestoreFocus();
    void testRestoreFocusWithDesktopWindow();
    void testQuitAfterActivatingHiddenWindow();
    void testDontQuitAfterActivatingDock();
    void testQuitAfterAddingWindow();
    void testDontQuitAfterAddingDock();
};

void ShowingDesktopTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    kwinApp()->start();
}

void ShowingDesktopTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::LayerShellV1));
}

void ShowingDesktopTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void ShowingDesktopTest::testRestoreFocus()
{
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    auto window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    auto window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window1 != window2);

    QCOMPARE(workspace()->activeWindow(), window2);
    workspace()->slotToggleShowDesktop();
    QVERIFY(workspace()->showingDesktop());
    workspace()->slotToggleShowDesktop();
    QVERIFY(!workspace()->showingDesktop());

    QVERIFY(workspace()->activeWindow());
    QCOMPARE(workspace()->activeWindow(), window2);
}

void ShowingDesktopTest::testRestoreFocusWithDesktopWindow()
{
    // first create a desktop window

    std::unique_ptr<KWayland::Client::Surface> desktopSurface(Test::createSurface());
    std::unique_ptr<Test::LayerSurfaceV1> desktopShellSurface(Test::createLayerSurfaceV1(desktopSurface.get(), QStringLiteral("desktop")));
    desktopShellSurface->set_keyboard_interactivity(1);
    desktopShellSurface->set_layer(Test::LayerShellV1::layer_background);
    desktopShellSurface->set_size(0, 0);
    desktopShellSurface->set_exclusive_zone(-1);
    desktopShellSurface->set_anchor(Test::LayerSurfaceV1::anchor_bottom
        | Test::LayerSurfaceV1::anchor_top
        | Test::LayerSurfaceV1::anchor_left
        | Test::LayerSurfaceV1::anchor_right);
    desktopSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy desktopConfigureRequestedSpy(desktopShellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(desktopConfigureRequestedSpy.wait());
    auto desktop = Test::renderAndWaitForShown(desktopSurface.get(), desktopConfigureRequestedSpy.last().at(1).toSize(), Qt::blue);
    QVERIFY(desktop);
    QVERIFY(desktop->isDesktop());

    // now create some windows
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    auto window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    auto window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window1 != window2);

    QCOMPARE(workspace()->activeWindow(), window2);
    workspace()->slotToggleShowDesktop();
    QVERIFY(workspace()->showingDesktop());
    QCOMPARE(workspace()->activeWindow(), desktop);
    workspace()->slotToggleShowDesktop();
    QVERIFY(!workspace()->showingDesktop());

    QVERIFY(workspace()->activeWindow());
    QCOMPARE(workspace()->activeWindow(), window2);
}

void ShowingDesktopTest::testQuitAfterActivatingHiddenWindow()
{
    // This test verifies that the show desktop mode is deactivated after activating a hidden window.

    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    auto window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    auto window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QCOMPARE(workspace()->activeWindow(), window2);

    workspace()->slotToggleShowDesktop();
    QVERIFY(workspace()->showingDesktop());

    workspace()->activateWindow(window1);
    QVERIFY(!workspace()->showingDesktop());
}

void ShowingDesktopTest::testDontQuitAfterActivatingDock()
{
    // This test verifies that activating windows belonging to desktop doesn't break showing desktop mode.

    std::unique_ptr<KWayland::Client::Surface> desktopSurface(Test::createSurface());
    std::unique_ptr<Test::LayerSurfaceV1> desktopShellSurface(Test::createLayerSurfaceV1(desktopSurface.get(), QStringLiteral("desktop")));
    desktopShellSurface->set_keyboard_interactivity(1);
    desktopShellSurface->set_layer(Test::LayerShellV1::layer_background);
    desktopShellSurface->set_size(0, 0);
    desktopShellSurface->set_exclusive_zone(-1);
    desktopShellSurface->set_anchor(Test::LayerSurfaceV1::anchor_bottom
                                    | Test::LayerSurfaceV1::anchor_top
                                    | Test::LayerSurfaceV1::anchor_left
                                    | Test::LayerSurfaceV1::anchor_right);
    desktopSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy desktopConfigureRequestedSpy(desktopShellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(desktopConfigureRequestedSpy.wait());
    auto desktop = Test::renderAndWaitForShown(desktopSurface.get(), desktopConfigureRequestedSpy.last().at(1).toSize(), Qt::blue);

    std::unique_ptr<KWayland::Client::Surface> dockSurface{Test::createSurface()};
    std::unique_ptr<Test::LayerSurfaceV1> dockShellSurface{Test::createLayerSurfaceV1(dockSurface.get(), QStringLiteral("dock"))};
    dockShellSurface->set_size(1280, 50);
    dockShellSurface->set_anchor(Test::LayerSurfaceV1::anchor_bottom);
    dockShellSurface->set_exclusive_zone(50);
    dockShellSurface->set_keyboard_interactivity(1);
    dockSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy dockConfigureRequestedSpy(dockShellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(dockConfigureRequestedSpy.wait());
    auto dock = Test::renderAndWaitForShown(dockSurface.get(), dockConfigureRequestedSpy.last().at(1).toSize(), Qt::blue);

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window->isActive());

    workspace()->slotToggleShowDesktop();
    QVERIFY(workspace()->showingDesktop());
    QVERIFY(desktop->isActive());

    workspace()->activateWindow(dock);
    QVERIFY(workspace()->showingDesktop());
    QVERIFY(dock->isActive());

    workspace()->activateWindow(desktop);
    QVERIFY(workspace()->showingDesktop());
    QVERIFY(desktop->isActive());

    workspace()->slotToggleShowDesktop();
    QVERIFY(!workspace()->showingDesktop());
}

void ShowingDesktopTest::testQuitAfterAddingWindow()
{
    // This test verifies that the show desktop mode is deactivated after mapping a new window.

    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);

    workspace()->slotToggleShowDesktop();
    QVERIFY(workspace()->showingDesktop());

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);

    QVERIFY(!workspace()->showingDesktop());
}

void ShowingDesktopTest::testDontQuitAfterAddingDock()
{
    // This test verifies that the show desktop mode is not broken after adding a dock.

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window->isActive());

    workspace()->slotToggleShowDesktop();
    QVERIFY(workspace()->showingDesktop());

    std::unique_ptr<KWayland::Client::Surface> dockSurface{Test::createSurface()};
    std::unique_ptr<Test::LayerSurfaceV1> dockShellSurface{Test::createLayerSurfaceV1(dockSurface.get(), QStringLiteral("dock"))};
    dockShellSurface->set_size(1280, 50);
    dockShellSurface->set_anchor(Test::LayerSurfaceV1::anchor_bottom);
    dockShellSurface->set_exclusive_zone(50);
    dockShellSurface->set_keyboard_interactivity(1);
    dockSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy dockConfigureRequestedSpy(dockShellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(dockConfigureRequestedSpy.wait());
    auto dock = Test::renderAndWaitForShown(dockSurface.get(), dockConfigureRequestedSpy.last().at(1).toSize(), Qt::blue);
    QVERIFY(dock->isActive());

    QVERIFY(workspace()->showingDesktop());
    workspace()->slotToggleShowDesktop();
}

WAYLANDTEST_MAIN(ShowingDesktopTest)
#include "showing_desktop_test.moc"
