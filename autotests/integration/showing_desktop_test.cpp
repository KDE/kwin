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
};

void ShowingDesktopTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
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

WAYLANDTEST_MAIN(ShowingDesktopTest)
#include "showing_desktop_test.moc"
