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
#include "x11window.h"

#include <KWayland/Client/surface.h>
#include <netwm.h>
#include <xcb/xcb_icccm.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_activation-0");

class ActivationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testSwitchToWindowToLeft();
    void testSwitchToWindowToRight();
    void testSwitchToWindowAbove();
    void testSwitchToWindowBelow();
    void testSwitchToWindowMaximized();
    void testSwitchToWindowFullScreen();
    void testActiveFullscreen();

private:
    void stackScreensHorizontally();
    void stackScreensVertically();
};

void ActivationTest::initTestCase()
{
    qRegisterMetaType<Window *>();

    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    kwinApp()->start();
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void ActivationTest::init()
{
    QVERIFY(Test::setupWaylandConnection());

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
}

void ActivationTest::cleanup()
{
    Test::destroyWaylandConnection();

    stackScreensHorizontally();
}

void ActivationTest::testSwitchToWindowToLeft()
{
    // This test verifies that "Switch to Window to the Left" shortcut works.

    // Prepare the test environment.
    stackScreensHorizontally();

    // Create several windows on the left screen.
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window1);
    QVERIFY(window1->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window2);
    QVERIFY(window2->isActive());

    window1->move(QPoint(300, 200));
    window2->move(QPoint(500, 200));

    // Create several windows on the right screen.
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    Window *window3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window3);
    QVERIFY(window3->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface4(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface4(Test::createXdgToplevelSurface(surface4.get()));
    Window *window4 = Test::renderAndWaitForShown(surface4.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window4);
    QVERIFY(window4->isActive());

    window3->move(QPoint(1380, 200));
    window4->move(QPoint(1580, 200));

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window3->isActive());

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window2->isActive());

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window1->isActive());

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window4->isActive());

    // Destroy all windows.
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowClosed(window1));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowClosed(window2));
    shellSurface3.reset();
    QVERIFY(Test::waitForWindowClosed(window3));
    shellSurface4.reset();
    QVERIFY(Test::waitForWindowClosed(window4));
}

void ActivationTest::testSwitchToWindowToRight()
{
    // This test verifies that "Switch to Window to the Right" shortcut works.

    // Prepare the test environment.
    stackScreensHorizontally();

    // Create several windows on the left screen.
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window1);
    QVERIFY(window1->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window2);
    QVERIFY(window2->isActive());

    window1->move(QPoint(300, 200));
    window2->move(QPoint(500, 200));

    // Create several windows on the right screen.
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    Window *window3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window3);
    QVERIFY(window3->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface4(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface4(Test::createXdgToplevelSurface(surface4.get()));
    Window *window4 = Test::renderAndWaitForShown(surface4.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window4);
    QVERIFY(window4->isActive());

    window3->move(QPoint(1380, 200));
    window4->move(QPoint(1580, 200));

    // Switch to window to the right.
    workspace()->switchWindow(Workspace::DirectionEast);
    QVERIFY(window1->isActive());

    // Switch to window to the right.
    workspace()->switchWindow(Workspace::DirectionEast);
    QVERIFY(window2->isActive());

    // Switch to window to the right.
    workspace()->switchWindow(Workspace::DirectionEast);
    QVERIFY(window3->isActive());

    // Switch to window to the right.
    workspace()->switchWindow(Workspace::DirectionEast);
    QVERIFY(window4->isActive());

    // Destroy all windows.
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowClosed(window1));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowClosed(window2));
    shellSurface3.reset();
    QVERIFY(Test::waitForWindowClosed(window3));
    shellSurface4.reset();
    QVERIFY(Test::waitForWindowClosed(window4));
}

void ActivationTest::testSwitchToWindowAbove()
{
    // This test verifies that "Switch to Window Above" shortcut works.

    // Prepare the test environment.
    stackScreensVertically();

    // Create several windows on the top screen.
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window1);
    QVERIFY(window1->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window2);
    QVERIFY(window2->isActive());

    window1->move(QPoint(200, 300));
    window2->move(QPoint(200, 500));

    // Create several windows on the bottom screen.
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    Window *window3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window3);
    QVERIFY(window3->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface4(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface4(Test::createXdgToplevelSurface(surface4.get()));
    Window *window4 = Test::renderAndWaitForShown(surface4.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window4);
    QVERIFY(window4->isActive());

    window3->move(QPoint(200, 1224));
    window4->move(QPoint(200, 1424));

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window3->isActive());

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window2->isActive());

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window1->isActive());

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window4->isActive());

    // Destroy all windows.
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowClosed(window1));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowClosed(window2));
    shellSurface3.reset();
    QVERIFY(Test::waitForWindowClosed(window3));
    shellSurface4.reset();
    QVERIFY(Test::waitForWindowClosed(window4));
}

void ActivationTest::testSwitchToWindowBelow()
{
    // This test verifies that "Switch to Window Bottom" shortcut works.

    // Prepare the test environment.
    stackScreensVertically();

    // Create several windows on the top screen.
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window1);
    QVERIFY(window1->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window2);
    QVERIFY(window2->isActive());

    window1->move(QPoint(200, 300));
    window2->move(QPoint(200, 500));

    // Create several windows on the bottom screen.
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    Window *window3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window3);
    QVERIFY(window3->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface4(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface4(Test::createXdgToplevelSurface(surface4.get()));
    Window *window4 = Test::renderAndWaitForShown(surface4.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window4);
    QVERIFY(window4->isActive());

    window3->move(QPoint(200, 1224));
    window4->move(QPoint(200, 1424));

    // Switch to window below.
    workspace()->switchWindow(Workspace::DirectionSouth);
    QVERIFY(window1->isActive());

    // Switch to window below.
    workspace()->switchWindow(Workspace::DirectionSouth);
    QVERIFY(window2->isActive());

    // Switch to window below.
    workspace()->switchWindow(Workspace::DirectionSouth);
    QVERIFY(window3->isActive());

    // Switch to window below.
    workspace()->switchWindow(Workspace::DirectionSouth);
    QVERIFY(window4->isActive());

    // Destroy all windows.
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowClosed(window1));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowClosed(window2));
    shellSurface3.reset();
    QVERIFY(Test::waitForWindowClosed(window3));
    shellSurface4.reset();
    QVERIFY(Test::waitForWindowClosed(window4));
}

void ActivationTest::testSwitchToWindowMaximized()
{
    // This test verifies that we switch to the top-most maximized window, i.e.
    // the one that user sees at the moment. See bug 411356.

    // Prepare the test environment.
    stackScreensHorizontally();

    // Create several maximized windows on the left screen.
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    QSignalSpy toplevelConfigureRequestedSpy1(shellSurface1.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy1(shellSurface1->xdgSurface(), &Test::XdgSurface::configureRequested);
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window1);
    QVERIFY(window1->isActive());
    QVERIFY(surfaceConfigureRequestedSpy1.wait()); // Wait for the configure event with the activated state.
    workspace()->slotWindowMaximize();
    QVERIFY(surfaceConfigureRequestedSpy1.wait());
    QSignalSpy frameGeometryChangedSpy1(window1, &Window::frameGeometryChanged);
    shellSurface1->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy1.last().at(0).value<quint32>());
    Test::render(surface1.get(), toplevelConfigureRequestedSpy1.last().at(0).toSize(), Qt::red);
    QVERIFY(frameGeometryChangedSpy1.wait());

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    QSignalSpy toplevelConfigureRequestedSpy2(shellSurface2.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy2(shellSurface2->xdgSurface(), &Test::XdgSurface::configureRequested);
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window2);
    QVERIFY(window2->isActive());
    QVERIFY(surfaceConfigureRequestedSpy2.wait()); // Wait for the configure event with the activated state.
    workspace()->slotWindowMaximize();
    QVERIFY(surfaceConfigureRequestedSpy2.wait());
    QSignalSpy frameGeometryChangedSpy2(window2, &Window::frameGeometryChanged);
    shellSurface2->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy2.last().at(0).value<quint32>());
    Test::render(surface2.get(), toplevelConfigureRequestedSpy2.last().at(0).toSize(), Qt::red);
    QVERIFY(frameGeometryChangedSpy2.wait());

    const QList<Window *> stackingOrder = workspace()->stackingOrder();
    QVERIFY(stackingOrder.indexOf(window1) < stackingOrder.indexOf(window2));
    QCOMPARE(window1->maximizeMode(), MaximizeFull);
    QCOMPARE(window2->maximizeMode(), MaximizeFull);

    // Create several windows on the right screen.
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    Window *window3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window3);
    QVERIFY(window3->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface4(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface4(Test::createXdgToplevelSurface(surface4.get()));
    Window *window4 = Test::renderAndWaitForShown(surface4.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window4);
    QVERIFY(window4->isActive());

    window3->move(QPoint(1380, 200));
    window4->move(QPoint(1580, 200));

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window3->isActive());

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window2->isActive());

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window4->isActive());

    // Destroy all windows.
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowClosed(window1));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowClosed(window2));
    shellSurface3.reset();
    QVERIFY(Test::waitForWindowClosed(window3));
    shellSurface4.reset();
    QVERIFY(Test::waitForWindowClosed(window4));
}

void ActivationTest::testSwitchToWindowFullScreen()
{
    // This test verifies that we switch to the top-most fullscreen window, i.e.
    // the one that user sees at the moment. See bug 411356.

    // Prepare the test environment.
    stackScreensVertically();

    // Create several maximized windows on the top screen.
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    QSignalSpy toplevelConfigureRequestedSpy1(shellSurface1.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy1(shellSurface1->xdgSurface(), &Test::XdgSurface::configureRequested);
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window1);
    QVERIFY(window1->isActive());
    QVERIFY(surfaceConfigureRequestedSpy1.wait()); // Wait for the configure event with the activated state.
    workspace()->slotWindowFullScreen();
    QVERIFY(surfaceConfigureRequestedSpy1.wait());
    QSignalSpy frameGeometryChangedSpy1(window1, &Window::frameGeometryChanged);
    shellSurface1->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy1.last().at(0).value<quint32>());
    Test::render(surface1.get(), toplevelConfigureRequestedSpy1.last().at(0).toSize(), Qt::red);
    QVERIFY(frameGeometryChangedSpy1.wait());

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    QSignalSpy toplevelConfigureRequestedSpy2(shellSurface2.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy2(shellSurface2->xdgSurface(), &Test::XdgSurface::configureRequested);
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window2);
    QVERIFY(window2->isActive());
    QVERIFY(surfaceConfigureRequestedSpy2.wait()); // Wait for the configure event with the activated state.
    workspace()->slotWindowFullScreen();
    QVERIFY(surfaceConfigureRequestedSpy2.wait());
    QSignalSpy frameGeometryChangedSpy2(window2, &Window::frameGeometryChanged);
    shellSurface2->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy2.last().at(0).value<quint32>());
    Test::render(surface2.get(), toplevelConfigureRequestedSpy2.last().at(0).toSize(), Qt::red);
    QVERIFY(frameGeometryChangedSpy2.wait());

    const QList<Window *> stackingOrder = workspace()->stackingOrder();
    QVERIFY(stackingOrder.indexOf(window1) < stackingOrder.indexOf(window2));
    QVERIFY(window1->isFullScreen());
    QVERIFY(window2->isFullScreen());

    // Create several windows on the bottom screen.
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    Window *window3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window3);
    QVERIFY(window3->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface4(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface4(Test::createXdgToplevelSurface(surface4.get()));
    Window *window4 = Test::renderAndWaitForShown(surface4.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window4);
    QVERIFY(window4->isActive());

    window3->move(QPoint(200, 1224));
    window4->move(QPoint(200, 1424));

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window3->isActive());

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window2->isActive());

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window4->isActive());

    // Destroy all windows.
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowClosed(window1));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowClosed(window2));
    shellSurface3.reset();
    QVERIFY(Test::waitForWindowClosed(window3));
    shellSurface4.reset();
    QVERIFY(Test::waitForWindowClosed(window4));
}

void ActivationTest::stackScreensHorizontally()
{
    // Process pending wl_output bind requests before destroying all outputs.
    QTest::qWait(1);

    const QList<QRect> screenGeometries{
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    };
    Test::setOutputConfig(screenGeometries);
}

void ActivationTest::stackScreensVertically()
{
    // Process pending wl_output bind requests before destroying all outputs.
    QTest::qWait(1);

    const QList<QRect> screenGeometries{
        QRect(0, 0, 1280, 1024),
        QRect(0, 1024, 1280, 1024),
    };
    Test::setOutputConfig(screenGeometries);
}

static X11Window *createX11Window(xcb_connection_t *connection, const QRect &geometry, std::function<void(xcb_window_t)> setup = {})
{
    xcb_window_t windowId = xcb_generate_id(connection);
    xcb_create_window(connection, XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      geometry.x(),
                      geometry.y(),
                      geometry.width(),
                      geometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);

    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, geometry.x(), geometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, geometry.width(), geometry.height());
    xcb_icccm_set_wm_normal_hints(connection, windowId, &hints);

    if (setup) {
        setup(windowId);
    }

    xcb_map_window(connection, windowId);
    xcb_flush(connection);

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    if (!windowCreatedSpy.wait()) {
        return nullptr;
    }
    return windowCreatedSpy.last().first().value<X11Window *>();
}

void ActivationTest::testActiveFullscreen()
{
    // Tests that an active X11 fullscreen window gets removed from the active layer
    // when activating a Wayland window, even if there's a pending activation request
    // for the X11 window
    Test::setOutputConfig({QRect(0, 0, 1280, 1024)});

    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *x11Window = createX11Window(c.get(), QRect(0, 0, 100, 200));

    // make it fullscreen
    x11Window->setFullScreen(true);
    QVERIFY(x11Window->isFullScreen());
    QCOMPARE(x11Window->layer(), Layer::ActiveLayer);

    // now, activate it again
    workspace()->activateWindow(x11Window);

    // now, create and activate a Wayland window
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(500, 300), Qt::blue);
    QVERIFY(waylandWindow);

    // the Wayland window should become active
    // and the X11 window should not be in the active layer anymore
    QSignalSpy stackingOrder(workspace(), &Workspace::stackingOrderChanged);
    workspace()->activateWindow(waylandWindow);
    QCOMPARE(workspace()->activeWindow(), waylandWindow);
    QCOMPARE(x11Window->layer(), Layer::NormalLayer);
}
}

WAYLANDTEST_MAIN(KWin::ActivationTest)
#include "activation_test.moc"
