/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "pointer_input.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include "xdgactivationv1.h"

#include <KWayland/Client/seat.h>
#include <KWayland/Client/surface.h>
#include <linux/input.h>

#if KWIN_BUILD_X11
#include "x11window.h"

#include <netwm.h>
#include <xcb/xcb_icccm.h>
#endif

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
    void testXdgActivation();
    void testGlobalShortcutActivation();

private:
    void stackScreensHorizontally();
    void stackScreensVertically();
};

void ActivationTest::initTestCase()
{
    qRegisterMetaType<Window *>();

    QVERIFY(waylandServer()->init(s_socketName));
    kwinApp()->start();
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void ActivationTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::XdgActivation | Test::AdditionalWaylandInterface::Seat));

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
    Test::XdgToplevelWindow window1;
    QVERIFY(window1.show());
    QVERIFY(window1.m_window->isActive());

    Test::XdgToplevelWindow window2;
    QVERIFY(window2.show());
    QVERIFY(window2.m_window->isActive());

    window1.m_window->move(QPoint(300, 200));
    window2.m_window->move(QPoint(500, 200));

    // Create several windows on the right screen.
    Test::XdgToplevelWindow window3;
    QVERIFY(window3.show());
    QVERIFY(window3.m_window->isActive());

    Test::XdgToplevelWindow window4;
    QVERIFY(window4.show());
    QVERIFY(window4.m_window->isActive());

    window3.m_window->move(QPoint(1380, 200));
    window4.m_window->move(QPoint(1580, 200));

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window3.m_window->isActive());

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window2.m_window->isActive());

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window1.m_window->isActive());

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window4.m_window->isActive());
}

void ActivationTest::testSwitchToWindowToRight()
{
    // This test verifies that "Switch to Window to the Right" shortcut works.

    // Prepare the test environment.
    stackScreensHorizontally();

    // Create several windows on the left screen.
    Test::XdgToplevelWindow window1;
    QVERIFY(window1.show());
    QVERIFY(window1.m_window->isActive());

    Test::XdgToplevelWindow window2;
    QVERIFY(window2.show());
    QVERIFY(window2.m_window->isActive());

    window1.m_window->move(QPoint(300, 200));
    window2.m_window->move(QPoint(500, 200));

    // Create several windows on the right screen.
    Test::XdgToplevelWindow window3;
    QVERIFY(window3.show());
    QVERIFY(window3.m_window->isActive());

    Test::XdgToplevelWindow window4;
    QVERIFY(window4.show());
    QVERIFY(window4.m_window->isActive());

    window3.m_window->move(QPoint(1380, 200));
    window4.m_window->move(QPoint(1580, 200));

    // Switch to window to the right.
    workspace()->switchWindow(Workspace::DirectionEast);
    QVERIFY(window1.m_window->isActive());

    // Switch to window to the right.
    workspace()->switchWindow(Workspace::DirectionEast);
    QVERIFY(window2.m_window->isActive());

    // Switch to window to the right.
    workspace()->switchWindow(Workspace::DirectionEast);
    QVERIFY(window3.m_window->isActive());

    // Switch to window to the right.
    workspace()->switchWindow(Workspace::DirectionEast);
    QVERIFY(window4.m_window->isActive());
}

void ActivationTest::testSwitchToWindowAbove()
{
    // This test verifies that "Switch to Window Above" shortcut works.

    // Prepare the test environment.
    stackScreensVertically();

    // Create several windows on the top screen.
    Test::XdgToplevelWindow window1;
    QVERIFY(window1.show());
    QVERIFY(window1.m_window->isActive());

    Test::XdgToplevelWindow window2;
    QVERIFY(window2.show());
    QVERIFY(window2.m_window->isActive());

    window1.m_window->move(QPoint(200, 300));
    window2.m_window->move(QPoint(200, 500));

    // Create several windows on the bottom screen.
    Test::XdgToplevelWindow window3;
    QVERIFY(window3.show());
    QVERIFY(window3.m_window->isActive());

    Test::XdgToplevelWindow window4;
    QVERIFY(window4.show());
    QVERIFY(window4.m_window->isActive());

    window3.m_window->move(QPoint(200, 1224));
    window4.m_window->move(QPoint(200, 1424));

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window3.m_window->isActive());

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window2.m_window->isActive());

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window1.m_window->isActive());

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window4.m_window->isActive());
}

void ActivationTest::testSwitchToWindowBelow()
{
    // This test verifies that "Switch to Window Bottom" shortcut works.

    // Prepare the test environment.
    stackScreensVertically();

    // Create several windows on the top screen.
    Test::XdgToplevelWindow window1;
    QVERIFY(window1.show());
    QVERIFY(window1.m_window->isActive());

    Test::XdgToplevelWindow window2;
    QVERIFY(window2.show());
    QVERIFY(window2.m_window->isActive());

    window1.m_window->move(QPoint(200, 300));
    window2.m_window->move(QPoint(200, 500));

    // Create several windows on the bottom screen.
    Test::XdgToplevelWindow window3;
    QVERIFY(window3.show());
    QVERIFY(window3.m_window->isActive());

    Test::XdgToplevelWindow window4;
    QVERIFY(window4.show());
    QVERIFY(window4.m_window->isActive());

    window3.m_window->move(QPoint(200, 1224));
    window4.m_window->move(QPoint(200, 1424));

    // Switch to window below.
    workspace()->switchWindow(Workspace::DirectionSouth);
    QVERIFY(window1.m_window->isActive());

    // Switch to window below.
    workspace()->switchWindow(Workspace::DirectionSouth);
    QVERIFY(window2.m_window->isActive());

    // Switch to window below.
    workspace()->switchWindow(Workspace::DirectionSouth);
    QVERIFY(window3.m_window->isActive());

    // Switch to window below.
    workspace()->switchWindow(Workspace::DirectionSouth);
    QVERIFY(window4.m_window->isActive());
}

void ActivationTest::testSwitchToWindowMaximized()
{
    // This test verifies that we switch to the top-most maximized window, i.e.
    // the one that user sees at the moment. See bug 411356.

    // Prepare the test environment.
    stackScreensHorizontally();

    // Create several maximized windows on the left screen.
    Test::XdgToplevelWindow window1;
    QVERIFY(window1.show());
    QVERIFY(window1.m_window->isActive());
    QVERIFY(window1.waitSurfaceConfigure());
    workspace()->slotWindowMaximize();
    QVERIFY(window1.handleConfigure(Qt::red));

    Test::XdgToplevelWindow window2;
    QVERIFY(window2.show());
    QVERIFY(window2.m_window->isActive());
    QVERIFY(window2.waitSurfaceConfigure());
    workspace()->slotWindowMaximize();
    QVERIFY(window2.handleConfigure(Qt::red));

    const QList<Window *> stackingOrder = workspace()->stackingOrder();
    QVERIFY(stackingOrder.indexOf(window1.m_window) < stackingOrder.indexOf(window2.m_window));
    QCOMPARE(window1.m_window->maximizeMode(), MaximizeFull);
    QCOMPARE(window2.m_window->maximizeMode(), MaximizeFull);

    // Create several windows on the right screen.
    Test::XdgToplevelWindow window3;
    QVERIFY(window3.show());
    QVERIFY(window3.m_window->isActive());

    Test::XdgToplevelWindow window4;
    QVERIFY(window4.show());
    QVERIFY(window4.m_window->isActive());

    window3.m_window->move(QPoint(1380, 200));
    window4.m_window->move(QPoint(1580, 200));

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window3.m_window->isActive());

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window2.m_window->isActive());

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window4.m_window->isActive());
}

void ActivationTest::testSwitchToWindowFullScreen()
{
    // This test verifies that we switch to the top-most fullscreen window, i.e.
    // the one that user sees at the moment. See bug 411356.

    // Prepare the test environment.
    stackScreensVertically();

    // Create several maximized windows on the top screen.
    Test::XdgToplevelWindow window1;
    QVERIFY(window1.show());
    QVERIFY(window1.m_window->isActive());
    QVERIFY(window1.waitSurfaceConfigure());
    workspace()->slotWindowFullScreen();
    QVERIFY(window1.handleConfigure(Qt::red));

    Test::XdgToplevelWindow window2;
    QVERIFY(window2.show());
    QVERIFY(window2.m_window->isActive());
    QVERIFY(window2.waitSurfaceConfigure());
    workspace()->slotWindowFullScreen();
    QVERIFY(window2.handleConfigure());

    const QList<Window *> stackingOrder = workspace()->stackingOrder();
    QVERIFY(stackingOrder.indexOf(window1.m_window) < stackingOrder.indexOf(window2.m_window));
    QVERIFY(window1.m_window->isFullScreen());
    QVERIFY(window2.m_window->isFullScreen());

    // Create several windows on the bottom screen.
    Test::XdgToplevelWindow window3;
    QVERIFY(window3.show());
    QVERIFY(window3.m_window->isActive());

    Test::XdgToplevelWindow window4;
    QVERIFY(window4.show());
    QVERIFY(window4.m_window->isActive());

    window3.m_window->move(QPoint(200, 1224));
    window4.m_window->move(QPoint(200, 1424));

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window3.m_window->isActive());

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window2.m_window->isActive());

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window4.m_window->isActive());
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

#if KWIN_BUILD_X11
static X11Window *createX11Window(xcb_connection_t *connection, const QRect &geometry, std::function<void(xcb_window_t)> setup = {})
{
    xcb_window_t windowId = xcb_generate_id(connection);
    xcb_create_window(connection, XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      geometry.x(),
                      geometry.y(),
                      geometry.width(),
                      geometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);

    xcb_size_hints_t hints{};
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
#endif

void ActivationTest::testActiveFullscreen()
{
#if KWIN_BUILD_X11
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
    Test::XdgToplevelWindow waylandWindow;
    QVERIFY(waylandWindow.show());

    // the Wayland window should become active
    // and the X11 window should not be in the active layer anymore
    QSignalSpy stackingOrder(workspace(), &Workspace::stackingOrderChanged);
    workspace()->activateWindow(waylandWindow.m_window);
    QCOMPARE(workspace()->activeWindow(), waylandWindow.m_window);
    QCOMPARE(x11Window->layer(), Layer::NormalLayer);
#endif
}

static std::vector<std::unique_ptr<Test::XdgToplevelWindow>> setupWindows(uint32_t &time)
{
    // re-create the same setup every time for reduced confusion
    std::vector<std::unique_ptr<Test::XdgToplevelWindow>> ret;
    for (int i = 0; i < 3; i++) {
        auto window = std::make_unique<Test::XdgToplevelWindow>();
        window->show();
        window->m_window->move(QPoint(150 * i, 0));
        workspace()->activateWindow(window->m_window);

        Test::pointerMotion(window->m_window->frameGeometry().center(), time++);
        Test::pointerButtonPressed(1, time++);
        Test::pointerButtonReleased(1, time++);
        ret.push_back(std::move(window));
    }
    return ret;
}

void ActivationTest::testXdgActivation()
{
    Test::setOutputConfig({QRect(0, 0, 1280, 1024)});

    uint32_t time = 0;

    auto windows = setupWindows(time);

    QSignalSpy activationSpy(workspace(), &Workspace::windowActivated);

    // activating a window without a valid token should fail
    Test::xdgActivation()->activate(QString(), *windows[1]->m_surface);
    QVERIFY(!activationSpy.wait(10));

    // using the surface and a correct serial should make it work
    auto token = Test::xdgActivation()->createToken();
    token->set_surface(*windows.back()->m_surface);
    token->set_serial(windows.back()->m_window->lastUsageSerial(), *Test::waylandSeat());
    Test::xdgActivation()->activate(token->commitAndWait(), *windows[1]->m_surface);
    QVERIFY(activationSpy.wait());
    QCOMPARE(workspace()->activeWindow(), windows[1]->m_window);

    // it should even work without the surface, if the serial is correct
    windows = setupWindows(time);
    token = Test::xdgActivation()->createToken();
    token->set_serial(windows.back()->m_window->lastUsageSerial(), *Test::waylandSeat());
    Test::xdgActivation()->activate(token->commitAndWait(), *windows[1]->m_surface);
    QVERIFY(activationSpy.wait(10));
    QCOMPARE(workspace()->activeWindow(), windows[1]->m_window);

    // activation should still work if the window is closed after creating the token
    windows = setupWindows(time);
    token = Test::xdgActivation()->createToken();
    token->set_surface(*windows[2]->m_surface);
    token->set_serial(windows[2]->m_window->lastUsageSerial(), *Test::waylandSeat());
    QString result = token->commitAndWait();

    windows[2]->unmap();
    QVERIFY(activationSpy.wait(10));
    QCOMPARE(workspace()->activeWindow(), windows[1]->m_window);

    Test::xdgActivation()->activate(result, *windows[0]->m_surface);
    QVERIFY(activationSpy.wait(10));
    QCOMPARE(workspace()->activeWindow(), windows[0]->m_window);

    // ...unless the user interacted with another window in between
    windows = setupWindows(time);
    token = Test::xdgActivation()->createToken();
    token->set_surface(*windows[2]->m_surface);
    token->set_serial(windows[2]->m_window->lastUsageSerial(), *Test::waylandSeat());
    result = token->commitAndWait();

    windows[2]->unmap();
    QVERIFY(activationSpy.wait(10));
    QCOMPARE(workspace()->activeWindow(), windows[1]->m_window);

    Test::pointerMotion(windows[1]->m_window->frameGeometry().center(), time++);
    Test::pointerButtonPressed(1, time++);
    Test::pointerButtonReleased(1, time++);

    Test::xdgActivation()->activate(result, *windows[0]->m_surface);
    QVERIFY(!activationSpy.wait(10));
    QCOMPARE(workspace()->activeWindow(), windows[1]->m_window);

    // ensure that windows are only activated on show with a valid activation token
    options->setFocusStealingPreventionLevel(FocusStealingPreventionLevel::Extreme);

    // creating a new window and immediately activating it should work
    windows.clear();
    windows = setupWindows(time);
    token = Test::xdgActivation()->createToken();
    token->set_surface(*windows[2]->m_surface);
    token->set_serial(windows[2]->m_window->lastUsageSerial(), *Test::waylandSeat());
    result = token->commitAndWait();
    {
        Test::XdgToplevelWindow window{[&result](KWayland::Client::Surface *surface, Test::XdgToplevel *toplevel) {
            Test::xdgActivation()->activate(result, *surface);
        }};
        QVERIFY(window.show());
        QCOMPARE(workspace()->activeWindow(), window.m_window);
    }

    // activation should fail if the user clicks on another window in between
    // creating the activation token and using it
    windows = setupWindows(time);
    token = Test::xdgActivation()->createToken();
    token->set_surface(*windows[2]->m_surface);
    token->set_serial(windows[2]->m_window->lastUsageSerial(), *Test::waylandSeat());
    result = token->commitAndWait();

    Test::pointerMotion(windows[1]->m_window->frameGeometry().center(), time++);
    Test::pointerButtonPressed(1, time++);
    Test::pointerButtonReleased(1, time++);

    {
        Test::XdgToplevelWindow window{[&result](KWayland::Client::Surface *surface, Test::XdgToplevel *toplevel) {
            Test::xdgActivation()->activate(result, *surface);
        }};
        QVERIFY(window.show());
        QCOMPARE(workspace()->activeWindow(), windows[1]->m_window);
        // if activation of a new window is not granted, it should be stacked behind the active window
        QCOMPARE_LT(window.m_window->stackingOrder(), workspace()->activeWindow()->stackingOrder());
    }

    // same for pointer input on the currently focused window
    windows = setupWindows(time);
    token = Test::xdgActivation()->createToken();
    token->set_surface(*windows[2]->m_surface);
    token->set_serial(windows[2]->m_window->lastUsageSerial(), *Test::waylandSeat());
    result = token->commitAndWait();

    Test::pointerMotion(windows[2]->m_window->frameGeometry().center(), time++);
    Test::pointerButtonPressed(1, time++);
    Test::pointerButtonReleased(1, time++);

    {
        Test::XdgToplevelWindow window{[&result](KWayland::Client::Surface *surface, Test::XdgToplevel *toplevel) {
            Test::xdgActivation()->activate(result, *surface);
        }};
        QVERIFY(window.show());
        QCOMPARE(workspace()->activeWindow(), windows[2]->m_window);
        QCOMPARE_LT(window.m_window->stackingOrder(), workspace()->activeWindow()->stackingOrder());
    }

    // same for keyboard input on the currently focused window
    windows = setupWindows(time);
    token = Test::xdgActivation()->createToken();
    token->set_surface(*windows[2]->m_surface);
    token->set_serial(windows[2]->m_window->lastUsageSerial(), *Test::waylandSeat());
    result = token->commitAndWait();

    Test::keyboardKeyPressed(KEY_A, time++);
    Test::keyboardKeyReleased(KEY_A, time++);

    {
        Test::XdgToplevelWindow window{[&result](KWayland::Client::Surface *surface, Test::XdgToplevel *toplevel) {
            Test::xdgActivation()->activate(result, *surface);
        }};
        QVERIFY(window.show());
        QCOMPARE(workspace()->activeWindow(), windows[2]->m_window);
        // if activation of a new window is not granted, it should be stacked behind the active window
        QCOMPARE_LT(window.m_window->stackingOrder(), workspace()->activeWindow()->stackingOrder());
    }

    // but modifier keys must not interfere in activation
    windows = setupWindows(time);
    token = Test::xdgActivation()->createToken();
    token->set_surface(*windows[2]->m_surface);
    token->set_serial(windows[2]->m_window->lastUsageSerial(), *Test::waylandSeat());
    result = token->commitAndWait();

    Test::keyboardKeyPressed(KEY_LEFTSHIFT, time++);
    Test::keyboardKeyReleased(KEY_LEFTSHIFT, time++);

    {
        Test::XdgToplevelWindow window{[&result](KWayland::Client::Surface *surface, Test::XdgToplevel *toplevel) {
            Test::xdgActivation()->activate(result, *surface);
        }};
        QVERIFY(window.show());
        QCOMPARE(workspace()->activeWindow(), window.m_window);
    }

    // focus stealing prevention level High is more lax and should activate windows
    // even with an invalid token if the app id matches the last granted activation token
    options->setFocusStealingPreventionLevel(FocusStealingPreventionLevel::High);
    windows = setupWindows(time);
    windows[1]->m_toplevel->set_app_id("test_app_id");
    token = Test::xdgActivation()->createToken();
    token->set_surface(*windows[2]->m_surface);
    token->set_serial(windows[2]->m_window->lastUsageSerial(), *Test::waylandSeat());
    token->set_app_id("test_app_id");
    result = token->commitAndWait();
    Test::xdgActivation()->activate(QString(), *windows[1]->m_surface);
    QVERIFY(activationSpy.wait());
    QCOMPARE(workspace()->activeWindow(), windows[1]->m_window);

    // new windows should also be activated if the app id matches,
    // even if they don't actually request activation
    windows = setupWindows(time);
    token = Test::xdgActivation()->createToken();
    token->set_surface(*windows[2]->m_surface);
    token->set_serial(windows[2]->m_window->lastUsageSerial(), *Test::waylandSeat());
    token->set_app_id("test_app_id_2");
    result = token->commitAndWait();
    {
        Test::XdgToplevelWindow window{[&result](Test::XdgToplevel *toplevel) {
            toplevel->set_app_id("test_app_id_2");
        }};
        QVERIFY(window.show());
        QCOMPARE(workspace()->activeWindow(), window.m_window);
    }

    // with focus stealing prevention level Low, every new window should unconditionally be activated,
    // even if it doesn't request an activation token at all
    options->setFocusStealingPreventionLevel(FocusStealingPreventionLevel::Low);
    windows = setupWindows(time);
    {
        Test::XdgToplevelWindow window;
        QVERIFY(window.show());
        QCOMPARE(workspace()->activeWindow(), window.m_window);
    }

    // with focus stealing prevention disabled, every activation token is considered "valid"
    options->setFocusStealingPreventionLevel(FocusStealingPreventionLevel::None);
    windows = setupWindows(time);
    Test::xdgActivation()->activate(QString(), *windows[1]->m_surface);
    QVERIFY(activationSpy.wait());
    QCOMPARE(workspace()->activeWindow(), windows[1]->m_window);
}

class TokenSpy : public InputEventSpy
{
public:
    void keyboardKey(KeyboardKeyEvent *event) override
    {
        if (event->state == KeyboardKeyState::Pressed && event->key == Qt::Key::Key_A) {
            latestToken = waylandServer()->xdgActivationIntegration()->requestPrivilegedToken(
                nullptr, input()->lastInteractionSerial(), waylandServer()->seat(), "test");
        }
    }

    QString latestToken;
};

void ActivationTest::testGlobalShortcutActivation()
{
    Test::setOutputConfig({QRect(0, 0, 1280, 1024)});
    options->setFocusStealingPreventionLevel(FocusStealingPreventionLevel::Extreme);

    // This spy needs to be used because normal keyboard shortcuts are signaled asynchronously,
    // while the shortcut for launching an application creates the token immediately.
    // The spy emulates the latter behavior.
    TokenSpy tokenSpy;
    input()->installInputEventSpy(&tokenSpy);
    QSignalSpy activationSpy(workspace(), &Workspace::windowActivated);

    uint32_t time = 0;

    // just triggering the shortcut normally should have working activation
    auto windows = setupWindows(time);

    Test::keyboardKeyPressed(KEY_LEFTSHIFT, time++);
    Test::keyboardKeyPressed(KEY_A, time++);
    Test::keyboardKeyReleased(KEY_A, time++);
    Test::keyboardKeyReleased(KEY_LEFTSHIFT, time++);

    Test::xdgActivation()->activate(tokenSpy.latestToken, *windows[1]->m_surface);
    QVERIFY(activationSpy.wait());
    QCOMPARE(workspace()->activeWindow(), windows[1]->m_window);

    // if we press a non-shift key after triggering the shortcut,
    // activation should fail
    windows = setupWindows(time);

    Test::keyboardKeyPressed(KEY_LEFTSHIFT, time++);
    Test::keyboardKeyPressed(KEY_A, time++);
    Test::keyboardKeyReleased(KEY_A, time++);
    Test::keyboardKeyReleased(KEY_LEFTSHIFT, time++);

    Test::keyboardKeyPressed(KEY_B, time++);
    Test::keyboardKeyReleased(KEY_B, time++);

    Test::xdgActivation()->activate(tokenSpy.latestToken, *windows[1]->m_surface);
    QVERIFY(!activationSpy.wait(10));
    QCOMPARE(workspace()->activeWindow(), windows[2]->m_window);
}
}

WAYLANDTEST_MAIN(KWin::ActivationTest)
#include "activation_test.moc"
