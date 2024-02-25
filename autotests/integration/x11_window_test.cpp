/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "atoms.h"
#include "compositor.h"
#include "cursor.h"
#include "pointer_input.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"

#include <KWayland/Client/surface.h>

#include <linux/input-event-codes.h>
#include <netwm.h>
#include <xcb/xcb_icccm.h>

using namespace KWin;
static const QString s_socketName = QStringLiteral("wayland_test_x11_window-0");

class X11WindowTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase_data();
    void initTestCase();
    void init();
    void cleanup();

    void testMaximizedFull();
    void testInitiallyMaximizedFull();
    void testRequestMaximizedFull();
    void testMaximizedVertical();
    void testInitiallyMaximizedVertical();
    void testRequestMaximizedVertical();
    void testMaximizedHorizontal();
    void testInitiallyMaximizedHorizontal();
    void testRequestMaximizedHorizontal();
    void testInteractiveMoveUnmaximizeFull();
    void testInteractiveMoveUnmaximizeInitiallyFull();
    void testInteractiveMoveUnmaximizeHorizontal();
    void testInteractiveMoveUnmaximizeInitiallyHorizontal();
    void testInteractiveMoveUnmaximizeVertical();
    void testInteractiveMoveUnmaximizeInitiallyVertical();
    void testFullScreen();
    void testInitiallyFullScreen();
    void testRequestFullScreen();
    void testFullscreenLayerWithActiveWaylandWindow();
    void testFullscreenWindowGroups();
    void testKeepBelow();
    void testInitiallyKeepBelow();
    void testKeepAbove();
    void testInitiallyKeepAbove();
    void testMinimized();
    void testInitiallyMinimized();
    void testRequestMinimized();
    void testSkipSwitcher();
    void testInitiallySkipSwitcher();
    void testRequestSkipSwitcher();
    void testSkipPager();
    void testInitiallySkipPager();
    void testRequestSkipPager();
    void testSkipTaskbar();
    void testInitiallySkipTaskbar();
    void testRequestSkipTaskbar();
    void testOpacity();
    void testDesktop_data();
    void testDesktop();
    void testInitialDesktop_data();
    void testInitialDesktop();
    void testChangeDesktop();
    void testOnAllDesktops();
    void testInitialOnAllDesktops();
    void testChangeOnAllDesktops();
    void testNetWmKeyboardMove();
    void testNetWmKeyboardMoveCancel();
    void testNetWmKeyboardResize();
    void testNetWmKeyboardResizeCancel();
    void testNetWmButtonMove();
    void testNetWmButtonMoveNotPressed();
    void testNetWmButtonMoveCancel();
    void testNetWmButtonSize_data();
    void testNetWmButtonSize();
    void testNetWmButtonSizeNotPressed();
    void testNetWmButtonSizeCancel();
    void testMinimumSize();
    void testMaximumSize();
    void testTrimCaption_data();
    void testTrimCaption();
    void testFocusInWithWaylandLastActiveWindow();
    void testCaptionChanges();
    void testCaptionWmName();
    void testActivateFocusedWindow();
    void testReentrantMoveResize();
    void testTransient();
    void testGroupTransient();
    void testCloseTransient();
    void testCloseInactiveTransient();
    void testCloseGroupTransient();
    void testCloseInactiveGroupTransient();
    void testModal();
    void testGroupModal();
    void testCloseModal();
    void testCloseInactiveModal();
    void testCloseGroupModal();
    void testCloseInactiveGroupModal();
    void testStackAboveFromApplication();
    void testStackAboveFromTool();
    void testStackAboveSibling();
    void testStackBelowFromApplication();
    void testStackBelowFromTool();
    void testStackBelowSibling();
    void testStackTopIfFromApplication();
    void testStackTopIfFromTool();
    void testStackBottomIfFromApplication();
    void testStackBottomIfFromTool();
    void testStackOppositeFromApplication();
    void testStackOppositeFromTool();
    void testStackOppositeNoSibling();
    void testOverrideRedirectReparent();
    void testOverrideRedirectStackingAbove();
    void testOverrideRedirectStackingBelow();
};

void X11WindowTest::initTestCase_data()
{
    QTest::addColumn<qreal>("scale");
    QTest::newRow("normal") << 1.0;
    QTest::newRow("scaled2x") << 2.0;
}

void X11WindowTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({QRect(0, 0, 1280, 1024)});
    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(KWin::Compositor::self());
}

void X11WindowTest::init()
{
    VirtualDesktopManager::self()->setCount(2);

    QFETCH_GLOBAL(qreal, scale);
    kwinApp()->setXwaylandScale(scale);

    QVERIFY(Test::setupWaylandConnection());
}

void X11WindowTest::cleanup()
{
    Test::destroyWaylandConnection();
}

static X11Window *createWindow(xcb_connection_t *connection, const QRect &geometry, std::function<void(xcb_window_t)> setup = {})
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

enum {
    MWM_HINTS_FUNCTIONS = (1L << 0),

    MWM_FUNC_ALL = (1L << 0),
    MWM_FUNC_RESIZE = (1L << 1),
    MWM_FUNC_MOVE = (1L << 2),
    MWM_FUNC_MINIMIZE = (1L << 3),
    MWM_FUNC_MAXIMIZE = (1L << 4),
    MWM_FUNC_CLOSE = (1L << 5),

    MWM_HINTS_DECORATIONS = (1L << 1),

    MWM_DECOR_ALL = (1L << 0),
    MWM_DECOR_BORDER = (1L << 1),
    MWM_DECOR_RESIZEH = (1L << 2),
    MWM_DECOR_TITLE = (1L << 3),
    MWM_DECOR_MENU = (1L << 4),
    MWM_DECOR_MINIMIZE = (1L << 5),
    MWM_DECOR_MAXIMIZE = (1L << 6),
};

struct MotifHints
{
    uint32_t flags = 0;
    uint32_t functions = 0;
    uint32_t decorations = 0;
    int32_t input_mode = 0;
    uint32_t status = 0;
};

static void applyMotifHints(xcb_connection_t *connection, xcb_window_t window, const MotifHints &hints)
{
    if (hints.flags) {
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, atoms->motif_wm_hints, atoms->motif_wm_hints, 32, 5, &hints);
    } else {
        xcb_delete_property(connection, window, atoms->motif_wm_hints);
    }
}

void X11WindowTest::testMaximizedFull()
{
    // This test verifies that toggling maximized mode works as expected and state changes are propagated to the client.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Make the window maximized.
    const QRectF originalGeometry = window->frameGeometry();
    const QRectF workArea = workspace()->clientArea(MaximizeArea, window);
    QSignalSpy maximizedChangedSpy(window, &Window::maximizedChanged);
    window->maximize(MaximizeFull);
    QCOMPARE(maximizedChangedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);
    QCOMPARE(window->frameGeometry(), workArea);
    QCOMPARE(window->geometryRestore(), originalGeometry);

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY((winInfo.state() & NET::Max) == NET::Max);
    }

    // Restore the window.
    window->maximize(MaximizeRestore);
    QCOMPARE(maximizedChangedSpy.count(), 2);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), originalGeometry);

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY(!(winInfo.state() & NET::Max));
    }
}

void X11WindowTest::testInitiallyMaximizedFull()
{
    // This test verifies that a window can be shown already in the maximized state.

    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::Max, NET::Max);
    });
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);
}

void X11WindowTest::testRequestMaximizedFull()
{
    // This test verifies that the client can toggle the maximized state.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Set maximized state.
    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::Max, NET::Max);
        xcb_flush(c.get());
    }
    QSignalSpy maximizedChangedSpy(window, &Window::maximizedChanged);
    QVERIFY(maximizedChangedSpy.wait());
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);

    // Unset maximized state.
    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::State(), NET::Max);
        xcb_flush(c.get());
    }
    QVERIFY(maximizedChangedSpy.wait());
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
}

void X11WindowTest::testMaximizedVertical()
{
    // This test verifies that toggling maximized vertically mode works as expected and state changes are propagated to the client.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Make the window maximized.
    const QRectF originalGeometry = window->frameGeometry();
    const QRectF workArea = workspace()->clientArea(MaximizeArea, window);
    QSignalSpy maximizedChangedSpy(window, &Window::maximizedChanged);
    window->maximize(MaximizeVertical);
    QCOMPARE(maximizedChangedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeVertical);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeVertical);
    QCOMPARE(window->frameGeometry(), QRectF(originalGeometry.x(), workArea.y(), originalGeometry.width(), workArea.height()));
    QCOMPARE(window->geometryRestore(), originalGeometry);

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY((winInfo.state() & NET::Max) == NET::MaxVert);
    }

    // Restore the window.
    window->maximize(MaximizeRestore);
    QCOMPARE(maximizedChangedSpy.count(), 2);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), originalGeometry);

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY(!(winInfo.state() & NET::Max));
    }
}

void X11WindowTest::testInitiallyMaximizedVertical()
{
    // This test verifies that a window can be shown already in the maximized vertically state.

    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::MaxVert, NET::MaxVert);
    });
    QCOMPARE(window->maximizeMode(), MaximizeVertical);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeVertical);
}

void X11WindowTest::testRequestMaximizedVertical()
{
    // This test verifies that the client can toggle the maximized vertically state.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Set maximized state.
    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::MaxVert, NET::MaxVert);
        xcb_flush(c.get());
    }
    QSignalSpy maximizedChangedSpy(window, &Window::maximizedChanged);
    QVERIFY(maximizedChangedSpy.wait());
    QCOMPARE(window->maximizeMode(), MaximizeVertical);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeVertical);

    // Unset maximized state.
    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::State(), NET::MaxVert);
        xcb_flush(c.get());
    }
    QVERIFY(maximizedChangedSpy.wait());
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
}

void X11WindowTest::testMaximizedHorizontal()
{
    // This test verifies that toggling maximized horizontally mode works as expected and state changes are propagated to the client.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Make the window maximized.
    const QRectF originalGeometry = window->frameGeometry();
    const QRectF workArea = workspace()->clientArea(MaximizeArea, window);
    QSignalSpy maximizedChangedSpy(window, &Window::maximizedChanged);
    window->maximize(MaximizeHorizontal);
    QCOMPARE(maximizedChangedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeHorizontal);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeHorizontal);
    QCOMPARE(window->frameGeometry(), QRectF(workArea.x(), originalGeometry.y(), workArea.width(), originalGeometry.height()));
    QCOMPARE(window->geometryRestore(), originalGeometry);

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY((winInfo.state() & NET::Max) == NET::MaxHoriz);
    }

    // Restore the window.
    window->maximize(MaximizeRestore);
    QCOMPARE(maximizedChangedSpy.count(), 2);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), originalGeometry);

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY(!(winInfo.state() & NET::Max));
    }
}

void X11WindowTest::testInitiallyMaximizedHorizontal()
{
    // This test verifies that a window can be shown already in the maximized horizontally state.

    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::MaxHoriz, NET::MaxHoriz);
    });
    QCOMPARE(window->maximizeMode(), MaximizeHorizontal);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeHorizontal);
}

void X11WindowTest::testRequestMaximizedHorizontal()
{
    // This test verifies that the client can toggle the maximized horizontally state.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Set maximized state.
    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::MaxHoriz, NET::MaxHoriz);
        xcb_flush(c.get());
    }
    QSignalSpy maximizedChangedSpy(window, &Window::maximizedChanged);
    QVERIFY(maximizedChangedSpy.wait());
    QCOMPARE(window->maximizeMode(), MaximizeHorizontal);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeHorizontal);

    // Unset maximized state.
    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::State(), NET::MaxHoriz);
        xcb_flush(c.get());
    }
    QVERIFY(maximizedChangedSpy.wait());
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
}

void X11WindowTest::testInteractiveMoveUnmaximizeFull()
{
    // This test verifies that a maximized x11 window is going to be properly unmaximized when it's dragged.

    // Create the window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(100, 100, 100, 200));

    // Make the window maximized.
    const QRectF originalGeometry = window->frameGeometry();
    window->maximize(MaximizeFull);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);

    // Start interactive move.
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &Window::interactiveMoveResizeStepped);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);
    const qreal xOffset = 0.2;
    const qreal yOffset = 0.5;
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(window->x() + window->width() * xOffset, window->y() + window->height() * yOffset), timestamp++);
    window->performMousePressCommand(Options::MouseMove, input()->pointer()->pos());
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);

    // Move the window to unmaximize it.
    Test::pointerMotionRelative(QPointF(0, 100), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), Xcb::fromXNative(Xcb::toXNative(QRectF(input()->pointer()->pos() - QPointF(originalGeometry.width() * xOffset, originalGeometry.height() * yOffset), originalGeometry.size()))));

    // Move the window again.
    const QRectF normalGeometry = window->frameGeometry();
    Test::pointerMotionRelative(QPointF(0, 10), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), normalGeometry.translated(0, 10));

    // Finish interactive move.
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
}

void X11WindowTest::testInteractiveMoveUnmaximizeInitiallyFull()
{
    // This test verifies that an initially maximized x11 window will be properly unmaximized when it's dragged.

    // Create the window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(100, 100, 100, 200), [&c](xcb_window_t windowId) {
        NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::Max, NET::Max);
    });

    // Start interactive move.
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &Window::interactiveMoveResizeStepped);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);
    const qreal xOffset = 0.2;
    const qreal yOffset = 0.5;
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(window->x() + window->width() * xOffset, window->y() + window->height() * yOffset), timestamp++);
    window->performMousePressCommand(Options::MouseMove, input()->pointer()->pos());
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);

    // Move the window to unmaximize it.
    const QSizeF restoredSize = window->geometryRestore().size();
    Test::pointerMotionRelative(QPointF(0, 100), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), Xcb::fromXNative(Xcb::toXNative(QRectF(input()->pointer()->pos() - QPointF(restoredSize.width() * xOffset, restoredSize.height() * yOffset), restoredSize))));

    // Move the window again.
    const QRectF normalGeometry = window->frameGeometry();
    Test::pointerMotionRelative(QPointF(0, 10), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), normalGeometry.translated(0, 10));

    // Finish interactive move.
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
}

void X11WindowTest::testInteractiveMoveUnmaximizeHorizontal()
{
    // This test verifies that a maximized horizontally x11 window is going to be properly unmaximized when it's dragged.

    // Create the window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(100, 100, 100, 200));

    // Make the window maximized.
    const QRectF originalGeometry = window->frameGeometry();
    window->maximize(MaximizeHorizontal);
    QCOMPARE(window->maximizeMode(), MaximizeHorizontal);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeHorizontal);

    // Start interactive move.
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &Window::interactiveMoveResizeStepped);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);
    const qreal xOffset = 0.2;
    const qreal yOffset = 0.5;
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(window->x() + window->width() * xOffset, window->y() + window->height() * yOffset), timestamp++);
    window->performMousePressCommand(Options::MouseMove, input()->pointer()->pos());
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeHorizontal);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeHorizontal);

    // Move the window to unmaximize it.
    Test::pointerMotionRelative(QPointF(100, 0), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), Xcb::fromXNative(Xcb::toXNative(QRectF(input()->pointer()->pos() - QPointF(originalGeometry.width() * xOffset, originalGeometry.height() * yOffset), originalGeometry.size()))));

    // Move the window again.
    const QRectF normalGeometry = window->frameGeometry();
    Test::pointerMotionRelative(QPointF(10, 0), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), normalGeometry.translated(10, 0));

    // Finish interactive move.
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
}

void X11WindowTest::testInteractiveMoveUnmaximizeInitiallyHorizontal()
{
    // This test verifies that an initially maximized horizontally x11 window will be properly unmaximized when it's dragged.

    // Create the window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(100, 100, 100, 200), [&c](xcb_window_t windowId) {
        NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::MaxHoriz, NET::MaxHoriz);
    });

    // Start interactive move.
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &Window::interactiveMoveResizeStepped);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);
    const qreal xOffset = 0.2;
    const qreal yOffset = 0.5;
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(window->x() + window->width() * xOffset, window->y() + window->height() * yOffset), timestamp++);
    window->performMousePressCommand(Options::MouseMove, input()->pointer()->pos());
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeHorizontal);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeHorizontal);

    // Move the window to unmaximize it.
    const QSizeF restoredSize = window->geometryRestore().size();
    Test::pointerMotionRelative(QPointF(100, 0), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), Xcb::fromXNative(Xcb::toXNative(QRectF(input()->pointer()->pos() - QPointF(restoredSize.width() * xOffset, restoredSize.height() * yOffset), restoredSize))));

    // Move the window again.
    const QRectF normalGeometry = window->frameGeometry();
    Test::pointerMotionRelative(QPointF(10, 0), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), normalGeometry.translated(10, 0));

    // Finish interactive move.
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
}

void X11WindowTest::testInteractiveMoveUnmaximizeVertical()
{
    // This test verifies that a maximized vertically x11 window is going to be properly unmaximized when it's dragged.

    // Create the window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(100, 100, 100, 200));

    // Make the window maximized.
    const QRectF originalGeometry = window->frameGeometry();
    window->maximize(MaximizeVertical);
    QCOMPARE(window->maximizeMode(), MaximizeVertical);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeVertical);

    // Start interactive move.
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &Window::interactiveMoveResizeStepped);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);
    const qreal xOffset = 0.2;
    const qreal yOffset = 0.5;
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(window->x() + window->width() * xOffset, window->y() + window->height() * yOffset), timestamp++);
    window->performMousePressCommand(Options::MouseMove, input()->pointer()->pos());
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeVertical);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeVertical);

    // Move the window to unmaximize it.
    Test::pointerMotionRelative(QPointF(0, 100), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), Xcb::fromXNative(Xcb::toXNative(QRectF(input()->pointer()->pos() - QPointF(originalGeometry.width() * xOffset, originalGeometry.height() * yOffset), originalGeometry.size()))));

    // Move the window again.
    const QRectF normalGeometry = window->frameGeometry();
    Test::pointerMotionRelative(QPointF(0, 10), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), normalGeometry.translated(0, 10));

    // Finish interactive move.
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
}

void X11WindowTest::testInteractiveMoveUnmaximizeInitiallyVertical()
{
    // This test verifies that an initially maximized vertically x11 window will be properly unmaximized when it's dragged.

    // Create the window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(100, 100, 100, 200), [&c](xcb_window_t windowId) {
        NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::MaxVert, NET::MaxVert);
    });

    // Start interactive move.
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &Window::interactiveMoveResizeStepped);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);
    const qreal xOffset = 0.2;
    const qreal yOffset = 0.5;
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(window->x() + window->width() * xOffset, window->y() + window->height() * yOffset), timestamp++);
    window->performMousePressCommand(Options::MouseMove, input()->pointer()->pos());
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeVertical);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeVertical);

    // Move the window to unmaximize it.
    const QSizeF restoredSize = window->geometryRestore().size();
    Test::pointerMotionRelative(QPointF(0, 100), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), Xcb::fromXNative(Xcb::toXNative(QRectF(input()->pointer()->pos() - QPointF(restoredSize.width() * xOffset, restoredSize.height() * yOffset), restoredSize))));

    // Move the window again.
    const QRectF normalGeometry = window->frameGeometry();
    Test::pointerMotionRelative(QPointF(0, 10), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), normalGeometry.translated(0, 10));

    // Finish interactive move.
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
}

void X11WindowTest::testFullScreen()
{
    // This test verifies that the fullscreen mode can be toggled and state changes are propagated to the client.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Make the window maximized.
    const QRectF originalGeometry = window->frameGeometry();
    const QRectF screenArea = workspace()->clientArea(ScreenArea, window);
    QSignalSpy fullScreenChangedSpy(window, &Window::fullScreenChanged);
    window->setFullScreen(true);
    QCOMPARE(fullScreenChangedSpy.count(), 1);
    QCOMPARE(window->isFullScreen(), true);
    QCOMPARE(window->isRequestedFullScreen(), true);
    QCOMPARE(window->frameGeometry(), screenArea);
    QCOMPARE(window->fullscreenGeometryRestore(), originalGeometry);

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY(winInfo.state() & NET::FullScreen);
    }

    // Restore the window.
    window->setFullScreen(false);
    QCOMPARE(fullScreenChangedSpy.count(), 2);
    QCOMPARE(window->isFullScreen(), false);
    QCOMPARE(window->isRequestedFullScreen(), false);
    QCOMPARE(window->frameGeometry(), originalGeometry);

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY(!(winInfo.state() & NET::FullScreen));
    }
}

void X11WindowTest::testInitiallyFullScreen()
{
    // This test verifies that a window can be shown already in the fullscreen state.

    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::FullScreen, NET::FullScreen);
    });
    QCOMPARE(window->isFullScreen(), true);
    QCOMPARE(window->isRequestedFullScreen(), true);
}

void X11WindowTest::testRequestFullScreen()
{
    // This test verifies that the client can toggle the fullscreen state.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Set fullscreen state.
    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::FullScreen, NET::FullScreen);
        xcb_flush(c.get());
    }
    QSignalSpy fullScreenChangedSpy(window, &Window::fullScreenChanged);
    QVERIFY(fullScreenChangedSpy.wait());
    QCOMPARE(window->isFullScreen(), true);
    QCOMPARE(window->isRequestedFullScreen(), true);

    // Unset fullscreen state.
    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::State(), NET::FullScreen);
        xcb_flush(c.get());
    }
    QVERIFY(fullScreenChangedSpy.wait());
    QCOMPARE(window->isFullScreen(), false);
    QCOMPARE(window->isRequestedFullScreen(), false);
}

void X11WindowTest::testKeepBelow()
{
    // This test verifies that keep below state can be toggled and its changes are propagated to the client.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Set keep below.
    QSignalSpy keepBelowChangedSpy(window, &Window::keepBelowChanged);
    window->setKeepBelow(true);
    QCOMPARE(keepBelowChangedSpy.count(), 1);
    QVERIFY(window->keepBelow());

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY(winInfo.state() & NET::KeepBelow);
    }

    // Unset keep below.
    window->setKeepBelow(false);
    QCOMPARE(keepBelowChangedSpy.count(), 2);
    QVERIFY(!window->keepBelow());

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY(!(winInfo.state() & NET::KeepBelow));
    }
}

void X11WindowTest::testInitiallyKeepBelow()
{
    // This test verifies that a window can be shown already with the keep below state set.

    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::KeepBelow, NET::KeepBelow);
    });
    QVERIFY(window->keepBelow());
}

void X11WindowTest::testKeepAbove()
{
    // This test verifies that keep above state can be toggled and its changes are propagated to the client.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Set keep above.
    QSignalSpy keepAboveChangedSpy(window, &Window::keepAboveChanged);
    window->setKeepAbove(true);
    QCOMPARE(keepAboveChangedSpy.count(), 1);
    QVERIFY(window->keepAbove());

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY(winInfo.state() & NET::KeepAbove);
    }

    // Unset keep above.
    window->setKeepAbove(false);
    QCOMPARE(keepAboveChangedSpy.count(), 2);
    QVERIFY(!window->keepAbove());

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY(!(winInfo.state() & NET::KeepAbove));
    }
}

void X11WindowTest::testInitiallyKeepAbove()
{
    // This test verifies that a window can be shown already with the keep above state set.

    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::KeepAbove, NET::KeepAbove);
    });
    QVERIFY(window->keepAbove());
}

void X11WindowTest::testMinimized()
{
    // This test verifies that a window can be minimized/unminimized and its changes are propagated to the client.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Minimize.
    QSignalSpy minimizedChangedSpy(window, &Window::minimizedChanged);
    window->setMinimized(true);
    QCOMPARE(minimizedChangedSpy.count(), 1);
    QVERIFY(window->isMinimized());

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY(winInfo.state() & NET::Hidden);
    }

    // Unminimize.
    window->setMinimized(false);
    QCOMPARE(minimizedChangedSpy.count(), 2);
    QVERIFY(!window->isMinimized());

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY(!(winInfo.state() & NET::Hidden));
    }
}

void X11WindowTest::testInitiallyMinimized()
{
    // This test verifies that a window can be shown already in the minimized state.

    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        xcb_icccm_wm_hints_t hints;
        memset(&hints, 0, sizeof(hints));
        xcb_icccm_wm_hints_set_iconic(&hints);
        xcb_icccm_set_wm_hints(c.get(), windowId, &hints);
    });
    QVERIFY(window->isMinimized());

    {
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY(winInfo.state() & NET::Hidden);
    }
}

void X11WindowTest::testRequestMinimized()
{
    // This test verifies that the client can set the minimized state.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Set minimized state.
    {
        xcb_client_message_event_t event;
        event.response_type = XCB_CLIENT_MESSAGE;
        event.format = 32;
        event.sequence = 0;
        event.window = window->window();
        event.type = atoms->wm_change_state;
        event.data.data32[0] = XCB_ICCCM_WM_STATE_ICONIC;
        event.data.data32[1] = 0;
        event.data.data32[2] = 0;
        event.data.data32[3] = 0;
        event.data.data32[4] = 0;

        xcb_send_event(c.get(), 0, kwinApp()->x11RootWindow(),
                       XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                       reinterpret_cast<const char *>(&event));
        xcb_flush(c.get());
    }
    QSignalSpy minimizedChangedSpy(window, &Window::minimizedChanged);
    QVERIFY(minimizedChangedSpy.wait());
    QVERIFY(window->isMinimized());
}

void X11WindowTest::testSkipSwitcher()
{
    // This test verifies that skip switcher changes are propagated to the client.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Set skip switcher.
    QSignalSpy skipSwitcherChangedSpy(window, &Window::skipSwitcherChanged);
    window->setSkipSwitcher(true);
    QCOMPARE(skipSwitcherChangedSpy.count(), 1);
    QVERIFY(window->skipSwitcher());

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY(winInfo.state() & NET::SkipSwitcher);
    }

    // Unset skip switcher.
    window->setSkipSwitcher(false);
    QCOMPARE(skipSwitcherChangedSpy.count(), 2);
    QVERIFY(!window->skipSwitcher());

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY(!(winInfo.state() & NET::SkipSwitcher));
    }
}

void X11WindowTest::testInitiallySkipSwitcher()
{
    // This test verifies that a window can be shown already with the skip switcher state set.

    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::SkipSwitcher, NET::SkipSwitcher);
    });
    QVERIFY(window->skipSwitcher());
}

void X11WindowTest::testRequestSkipSwitcher()
{
    // This test verifies that the client can change the skip switcher state.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Set the skip switcher state.
    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::SkipSwitcher, NET::SkipSwitcher);
        xcb_flush(c.get());
    }
    QSignalSpy skipSwitcherChangedSpy(window, &Window::skipSwitcherChanged);
    QVERIFY(skipSwitcherChangedSpy.wait());
    QVERIFY(window->skipSwitcher());

    // Unset the skip switcher state.
    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::State(), NET::SkipSwitcher);
        xcb_flush(c.get());
    }
    QVERIFY(skipSwitcherChangedSpy.wait());
    QVERIFY(!window->skipSwitcher());
}

void X11WindowTest::testSkipPager()
{
    // This test verifies that skip pager changes are propagated to the client.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Set skip pager.
    QSignalSpy skipPagerChangedSpy(window, &Window::skipPagerChanged);
    window->setSkipPager(true);
    QCOMPARE(skipPagerChangedSpy.count(), 1);
    QVERIFY(window->skipPager());

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY(winInfo.state() & NET::SkipPager);
    }

    // Unset skip pager.
    window->setSkipPager(false);
    QCOMPARE(skipPagerChangedSpy.count(), 2);
    QVERIFY(!window->skipPager());

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY(!(winInfo.state() & NET::SkipPager));
    }
}

void X11WindowTest::testInitiallySkipPager()
{
    // This test verifies that a window can be shown already with the skip pager state set.

    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::SkipPager, NET::SkipPager);
    });
    QVERIFY(window->skipPager());
}

void X11WindowTest::testRequestSkipPager()
{
    // This test verifies that the client can change the skip pager state.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Set the skip pager state.
    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::SkipPager, NET::SkipPager);
        xcb_flush(c.get());
    }
    QSignalSpy skipPagerChangedSpy(window, &Window::skipPagerChanged);
    QVERIFY(skipPagerChangedSpy.wait());
    QVERIFY(window->skipPager());

    // Unset the skip pager state.
    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::State(), NET::SkipPager);
        xcb_flush(c.get());
    }
    QVERIFY(skipPagerChangedSpy.wait());
    QVERIFY(!window->skipPager());
}

void X11WindowTest::testSkipTaskbar()
{
    // This test verifies that skip taskbar changes are propagated to the client.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Set skip taskbar.
    QSignalSpy skipTaskbarChangedSpy(window, &Window::skipTaskbarChanged);
    window->setSkipTaskbar(true);
    QCOMPARE(skipTaskbarChangedSpy.count(), 1);
    QVERIFY(window->skipTaskbar());

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY(winInfo.state() & NET::SkipTaskbar);
    }

    // Unset skip taskbar.
    window->setSkipTaskbar(false);
    QCOMPARE(skipTaskbarChangedSpy.count(), 2);
    QVERIFY(!window->skipTaskbar());

    {
        Xcb::sync();
        NETWinInfo winInfo(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        QVERIFY(!(winInfo.state() & NET::SkipTaskbar));
    }
}

void X11WindowTest::testInitiallySkipTaskbar()
{
    // This test verifies that a window can be shown already with the skip taskbar state set.

    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::SkipTaskbar, NET::SkipTaskbar);
    });
    QVERIFY(window->skipTaskbar());
}

void X11WindowTest::testRequestSkipTaskbar()
{
    // This test verifies that the client can change the skip taskbar state.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Set the skip taskbar state.
    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::SkipTaskbar, NET::SkipTaskbar);
        xcb_flush(c.get());
    }
    QSignalSpy skipTaskbarChangedSpy(window, &Window::skipTaskbarChanged);
    QVERIFY(skipTaskbarChangedSpy.wait());
    QVERIFY(window->skipTaskbar());

    // Unset the skip taskbar state.
    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::State(), NET::SkipTaskbar);
        xcb_flush(c.get());
    }
    QVERIFY(skipTaskbarChangedSpy.wait());
    QVERIFY(!window->skipTaskbar());
}

void X11WindowTest::testOpacity()
{
    // This test verifies that _NET_WM_WINDOW_OPACITY is properly sync'ed with Window::opacity().

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::Properties(), NET::WM2Opacity);
        info.setOpacityF(0.5);
    });
    QCOMPARE(window->opacity(), 0.5);

    // Change the opacity.
    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::Properties(), NET::WM2Opacity);
        info.setOpacityF(0.8);
        xcb_flush(c.get());
    }
    QSignalSpy opacityChangedSpy(window, &Window::opacityChanged);
    QVERIFY(opacityChangedSpy.wait());
    QCOMPARE(window->opacity(), 0.8);
}

void X11WindowTest::testDesktop_data()
{
    QTest::addColumn<uint>("desktopId");

    QTest::addRow("first") << uint(1);
    QTest::addRow("second") << uint(2);
}

void X11WindowTest::testDesktop()
{
    // This test verifies that desktop changes are propagated to the client.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Send the window to the given desktop.
    QFETCH(uint, desktopId);
    VirtualDesktop *desktop = VirtualDesktopManager::self()->desktopForX11Id(desktopId);
    window->setDesktops({desktop});

    {
        Xcb::sync();
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMDesktop, NET::Properties2());
        QCOMPARE(info.desktop(), desktopId);
    }
}

void X11WindowTest::testInitialDesktop_data()
{
    QTest::addColumn<uint>("desktopId");

    QTest::addRow("first") << uint(1);
    QTest::addRow("second") << uint(2);
}

void X11WindowTest::testInitialDesktop()
{
    // This test verifies that a window can be shown initially on a particular virtual desktop.

    QFETCH(uint, desktopId);
    VirtualDesktop *desktop = VirtualDesktopManager::self()->desktopForX11Id(desktopId);

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &desktopId](xcb_window_t windowId) {
        NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::WMDesktop, NET::Properties2());
        info.setDesktop(desktopId);
    });
    QCOMPARE(window->desktops(), (QList<VirtualDesktop *>{desktop}));
}

void X11WindowTest::testChangeDesktop()
{
    // This test verifies that the client can send its window to another virtual desktop.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::WMDesktop, NET::Properties2());
        info.setDesktop(1);
    });

    // Send the window to another desktop.
    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMDesktop, NET::Properties2());
        info.setDesktop(2);
        xcb_flush(c.get());
    }
    QSignalSpy desktopsChangedSpy(window, &Window::desktopsChanged);
    QVERIFY(desktopsChangedSpy.wait());
    QCOMPARE(window->desktops(), (QList<VirtualDesktop *>{VirtualDesktopManager::self()->desktopForX11Id(2)}));
}

void X11WindowTest::testOnAllDesktops()
{
    // This test verifies that desktop changes are propagated to the client.

    VirtualDesktop *activeDesktop = VirtualDesktopManager::self()->currentDesktop();

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMDesktop, NET::Properties2());
        QCOMPARE(info.desktop(), activeDesktop->x11DesktopNumber());
    }

    // Set the window on all desktops.
    window->setOnAllDesktops(true);

    {
        Xcb::sync();
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMDesktop, NET::Properties2());
        QCOMPARE(info.desktop(), -1);
    }

    // Unset the window on all desktops.
    window->setOnAllDesktops(false);

    {
        Xcb::sync();
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMDesktop, NET::Properties2());
        QCOMPARE(info.desktop(), activeDesktop->x11DesktopNumber());
    }
}

void X11WindowTest::testInitialOnAllDesktops()
{
    // This test verifies that a client can show a window, which is already on all desktops.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::WMDesktop, NET::Properties2());
        info.setDesktop(-1);
    });
    QVERIFY(window->isOnAllDesktops());
}

void X11WindowTest::testChangeOnAllDesktops()
{
    // This test verifies that a client can set/unset its window on all desktops.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));

    // Set the window on all desktops.
    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMDesktop, NET::Properties2());
        info.setDesktop(-1);
        xcb_flush(c.get());
    }
    QSignalSpy desktopsChangedSpy(window, &Window::desktopsChanged);
    QVERIFY(desktopsChangedSpy.wait());
    QVERIFY(window->isOnAllDesktops());

    // Unset the window on all desktops.
    {
        NETWinInfo info(c.get(), window->window(), kwinApp()->x11RootWindow(), NET::WMDesktop, NET::Properties2());
        info.setDesktop(1);
        xcb_flush(c.get());
    }
    QVERIFY(desktopsChangedSpy.wait());
    QCOMPARE(window->desktops(), (QList<VirtualDesktop *>{VirtualDesktopManager::self()->desktopForX11Id(1)}));
}

void X11WindowTest::testNetWmKeyboardMove()
{
    // This test verifies that a client can initiate a keyboard interactive move operation.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(100, 100, 100, 200));

    // Request interactive move.
    {
        NETRootInfo root(c.get(), NET::Properties());
        root.moveResizeRequest(window->window(), Xcb::toXNative(window->x() + window->width() / 2), Xcb::toXNative(window->y() + window->height() / 2), NET::KeyboardMove, XCB_BUTTON_INDEX_1);
        xcb_flush(c.get());
    }
    QSignalSpy interactiveMoveResizeStartedSpy(window, &X11Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &X11Window::interactiveMoveResizeFinished);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &X11Window::interactiveMoveResizeStepped);
    QVERIFY(interactiveMoveResizeStartedSpy.wait());
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QVERIFY(window->isInteractiveMove());

    // Move the window to the right.
    const QRectF originalGeometry = window->frameGeometry();
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_RIGHT, timestamp++);
    Test::keyboardKeyReleased(KEY_RIGHT, timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->frameGeometry(), originalGeometry.translated(8, 0));

    // Finish the interactive move.
    Test::keyboardKeyPressed(KEY_ENTER, timestamp++);
    Test::keyboardKeyReleased(KEY_ENTER, timestamp++);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
    QCOMPARE(window->frameGeometry(), originalGeometry.translated(8, 0));
}

void X11WindowTest::testNetWmKeyboardMoveCancel()
{
    // This test verifies that a client can initiate a keyboard interactive move operation and then cancel it.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(100, 100, 100, 200));

    // Request interactive move.
    {
        NETRootInfo root(c.get(), NET::Properties());
        root.moveResizeRequest(window->window(), Xcb::toXNative(window->x() + window->width() / 2), Xcb::toXNative(window->y() + window->height() / 2), NET::KeyboardMove, XCB_BUTTON_INDEX_ANY);
        xcb_flush(c.get());
    }
    QSignalSpy interactiveMoveResizeStartedSpy(window, &X11Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &X11Window::interactiveMoveResizeFinished);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &X11Window::interactiveMoveResizeStepped);
    QVERIFY(interactiveMoveResizeStartedSpy.wait());
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QVERIFY(window->isInteractiveMove());

    // Move the window to the right.
    const QRectF originalGeometry = window->frameGeometry();
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_RIGHT, timestamp++);
    Test::keyboardKeyReleased(KEY_RIGHT, timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->frameGeometry(), originalGeometry.translated(8, 0));

    // Cancel the interactive move.
    {
        NETRootInfo root(c.get(), NET::Properties());
        root.moveResizeRequest(window->window(), Xcb::toXNative(window->x() + window->width() / 2), Xcb::toXNative(window->y() + window->height() / 2), NET::MoveResizeCancel, XCB_BUTTON_INDEX_ANY);
        xcb_flush(c.get());
    }
    QVERIFY(interactiveMoveResizeFinishedSpy.wait());
    QCOMPARE(window->frameGeometry(), originalGeometry);
}

void X11WindowTest::testNetWmKeyboardResize()
{
    // This test verifies that a client can initiate a keyboard interactive resize operation.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(100, 100, 100, 200));

    // Request interactive resize.
    {
        NETRootInfo root(c.get(), NET::Properties());
        root.moveResizeRequest(window->window(), Xcb::toXNative(window->x() + window->width()), Xcb::toXNative(window->y() + window->height()), NET::KeyboardSize, XCB_BUTTON_INDEX_1);
        xcb_flush(c.get());
    }
    QSignalSpy interactiveMoveResizeStartedSpy(window, &X11Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &X11Window::interactiveMoveResizeFinished);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &X11Window::interactiveMoveResizeStepped);
    QVERIFY(interactiveMoveResizeStartedSpy.wait());
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QVERIFY(window->isInteractiveResize());

    // Move the window to the right, the frame geometry will be updated some time later.
    const QRectF originalGeometry = window->frameGeometry();
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_RIGHT, timestamp++);
    Test::keyboardKeyReleased(KEY_RIGHT, timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->frameGeometry(), originalGeometry.adjusted(0, 0, 8, 0));

    // Finish the interactive move.
    Test::keyboardKeyPressed(KEY_ENTER, timestamp++);
    Test::keyboardKeyReleased(KEY_ENTER, timestamp++);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
    QCOMPARE(window->frameGeometry(), originalGeometry.adjusted(0, 0, 8, 0));
}

void X11WindowTest::testNetWmKeyboardResizeCancel()
{
    // This test verifies that a client can initiate a keyboard interactive resize operation and then cancel it.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(100, 100, 100, 200));

    // Request interactive resize.
    {
        NETRootInfo root(c.get(), NET::Properties());
        root.moveResizeRequest(window->window(), Xcb::toXNative(window->x() + window->width()), Xcb::toXNative(window->y() + window->height()), NET::KeyboardSize, XCB_BUTTON_INDEX_1);
        xcb_flush(c.get());
    }
    QSignalSpy interactiveMoveResizeStartedSpy(window, &X11Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &X11Window::interactiveMoveResizeFinished);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &X11Window::interactiveMoveResizeStepped);
    QVERIFY(interactiveMoveResizeStartedSpy.wait());
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QVERIFY(window->isInteractiveResize());

    // Move the window to the right, the frame geometry will be updated some time later.
    const QRectF originalGeometry = window->frameGeometry();
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_RIGHT, timestamp++);
    Test::keyboardKeyReleased(KEY_RIGHT, timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->frameGeometry(), originalGeometry.adjusted(0, 0, 8, 0));

    // Cancel the interactive move.
    {
        NETRootInfo root(c.get(), NET::Properties());
        root.moveResizeRequest(window->window(), Xcb::toXNative(window->x() + window->width()), Xcb::toXNative(window->y() + window->height()), NET::MoveResizeCancel, XCB_BUTTON_INDEX_ANY);
        xcb_flush(c.get());
    }
    QVERIFY(interactiveMoveResizeFinishedSpy.wait());
    QCOMPARE(window->frameGeometry(), originalGeometry);
}

void X11WindowTest::testNetWmButtonMove()
{
    // This test verifies that a client can initiate an interactive move operation controlled by the pointer.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(100, 100, 100, 200));

    // Request interactive move.
    const QRectF originalGeometry = window->frameGeometry();
    quint32 timestamp = 0;
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    {
        NETRootInfo root(c.get(), NET::Properties());
        root.moveResizeRequest(window->window(), Xcb::toXNative(window->x() + window->width() / 2), Xcb::toXNative(window->y() + window->height() / 2), NET::Move, XCB_BUTTON_INDEX_1);
        xcb_flush(c.get());
    }
    QSignalSpy interactiveMoveResizeStartedSpy(window, &X11Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &X11Window::interactiveMoveResizeFinished);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &X11Window::interactiveMoveResizeStepped);
    QVERIFY(interactiveMoveResizeStartedSpy.wait());
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QVERIFY(window->isInteractiveMove());

    // Move the window to the right.
    Test::pointerMotionRelative(QPointF(8, 0), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->frameGeometry(), originalGeometry.translated(8, 0));

    // Finish the interactive move.
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
    QCOMPARE(window->frameGeometry(), originalGeometry.translated(8, 0));
}

void X11WindowTest::testNetWmButtonMoveNotPressed()
{
    // This test verifies that an interactive move operation won't be started if the specified button is not pressed.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(100, 100, 100, 200));

    // Request interactive move.
    {
        NETRootInfo root(c.get(), NET::Properties());
        root.moveResizeRequest(window->window(), Xcb::toXNative(window->x() + window->width() / 2), Xcb::toXNative(window->y() + window->height() / 2), NET::Move, XCB_BUTTON_INDEX_1);
        xcb_flush(c.get());
    }
    QSignalSpy interactiveMoveResizeStartedSpy(window, &X11Window::interactiveMoveResizeStarted);
    QVERIFY(!interactiveMoveResizeStartedSpy.wait(10));
}

void X11WindowTest::testNetWmButtonMoveCancel()
{
    // This test verifies that a client can initiate an interactive move operation controlled by the pointer and then cancel it.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(100, 100, 100, 200));

    // Request interactive move.
    const QRectF originalGeometry = window->frameGeometry();
    quint32 timestamp = 0;
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    auto releaseButton = qScopeGuard([&timestamp]() {
        Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    });
    {
        NETRootInfo root(c.get(), NET::Properties());
        root.moveResizeRequest(window->window(), Xcb::toXNative(window->x() + window->width() / 2), Xcb::toXNative(window->y() + window->height() / 2), NET::Move, XCB_BUTTON_INDEX_1);
        xcb_flush(c.get());
    }
    QSignalSpy interactiveMoveResizeStartedSpy(window, &X11Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &X11Window::interactiveMoveResizeFinished);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &X11Window::interactiveMoveResizeStepped);
    QVERIFY(interactiveMoveResizeStartedSpy.wait());
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QVERIFY(window->isInteractiveMove());

    // Move the window to the right.
    Test::pointerMotionRelative(QPointF(8, 0), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->frameGeometry(), originalGeometry.translated(8, 0));

    // Cancel the interactive move.
    {
        NETRootInfo root(c.get(), NET::Properties());
        root.moveResizeRequest(window->window(), Xcb::toXNative(window->x() + window->width() / 2), Xcb::toXNative(window->y() + window->height() / 2), NET::MoveResizeCancel, XCB_BUTTON_INDEX_ANY);
        xcb_flush(c.get());
    }
    QVERIFY(interactiveMoveResizeFinishedSpy.wait());
    QCOMPARE(window->frameGeometry(), originalGeometry);
}

void X11WindowTest::testNetWmButtonSize_data()
{
    QTest::addColumn<Gravity>("gravity");
    QTest::addColumn<NET::Direction>("direction");

    QTest::addRow("top-left") << Gravity::TopLeft << NET::Direction::TopLeft;
    QTest::addRow("top") << Gravity::Top << NET::Direction::Top;
    QTest::addRow("top-right") << Gravity::TopRight << NET::Direction::TopRight;
    QTest::addRow("right") << Gravity::Right << NET::Direction::Right;
    QTest::addRow("bottom-right") << Gravity::BottomRight << NET::Direction::BottomRight;
    QTest::addRow("bottom") << Gravity::Bottom << NET::Direction::Bottom;
    QTest::addRow("bottom-left") << Gravity::BottomLeft << NET::Direction::BottomLeft;
    QTest::addRow("left") << Gravity::Left << NET::Direction::Left;
}

static QPointF directionToVector(NET::Direction direction, const QSizeF &size)
{
    switch (direction) {
    case NET::Direction::TopLeft:
        return QPointF(-size.width(), -size.height());
    case NET::Direction::Top:
        return QPointF(0, -size.height());
    case NET::Direction::TopRight:
        return QPointF(size.width(), -size.height());
    case NET::Direction::Right:
        return QPointF(size.width(), 0);
    case NET::Direction::BottomRight:
        return QPointF(size.width(), size.height());
    case NET::Direction::Bottom:
        return QPointF(0, size.height());
    case NET::Direction::BottomLeft:
        return QPointF(-size.width(), size.height());
    case NET::Direction::Left:
        return QPointF(-size.width(), 0);
    default:
        Q_UNREACHABLE();
    }
}

static QRectF expandRect(const QRectF &rect, NET::Direction direction, const QSizeF &amount)
{
    switch (direction) {
    case NET::Direction::TopLeft:
        return rect.adjusted(-amount.width(), -amount.height(), 0, 0);
    case NET::Direction::Top:
        return rect.adjusted(0, -amount.height(), 0, 0);
    case NET::Direction::TopRight:
        return rect.adjusted(0, -amount.height(), amount.width(), 0);
    case NET::Direction::Right:
        return rect.adjusted(0, 0, amount.width(), 0);
    case NET::Direction::BottomRight:
        return rect.adjusted(0, 0, amount.width(), amount.height());
    case NET::Direction::Bottom:
        return rect.adjusted(0, 0, 0, amount.height());
    case NET::Direction::BottomLeft:
        return rect.adjusted(-amount.width(), 0, 0, amount.height());
    case NET::Direction::Left:
        return rect.adjusted(-amount.width(), 0, 0, 0);
    default:
        Q_UNREACHABLE();
    }
}

void X11WindowTest::testNetWmButtonSize()
{
    // This test verifies that a client can initiate an interactive move operation controlled by the pointer.

    QFETCH(NET::Direction, direction);

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(100, 100, 300, 400));

    // Request interactive move.
    const QRectF originalGeometry = window->frameGeometry();
    const QPointF initialPointer = window->frameGeometry().center() + directionToVector(direction, originalGeometry.size() * 0.5);
    quint32 timestamp = 0;
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    Test::pointerMotion(initialPointer, timestamp++);
    {
        NETRootInfo root(c.get(), NET::Properties());
        root.moveResizeRequest(window->window(), Xcb::toXNative(initialPointer.x()), Xcb::toXNative(initialPointer.y()), direction, XCB_BUTTON_INDEX_1);
        xcb_flush(c.get());
    }
    QSignalSpy interactiveMoveResizeStartedSpy(window, &X11Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &X11Window::interactiveMoveResizeFinished);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &X11Window::interactiveMoveResizeStepped);
    QVERIFY(interactiveMoveResizeStartedSpy.wait());
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QVERIFY(window->isInteractiveResize());
    QTEST(window->interactiveMoveResizeGravity(), "gravity");

    // Resize the window a tiny bit.
    Test::pointerMotionRelative(directionToVector(direction, QSizeF(8, 8)), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->frameGeometry(), expandRect(originalGeometry, direction, QSizeF(8, 8)));

    // Finish the interactive move.
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
    QCOMPARE(window->frameGeometry(), expandRect(originalGeometry, direction, QSizeF(8, 8)));
}

void X11WindowTest::testNetWmButtonSizeNotPressed()
{
    // This test verifies that an interactive siize operation won't be started if the specified button is not pressed.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(100, 100, 300, 400));

    // Request interactive move.
    {
        NETRootInfo root(c.get(), NET::Properties());
        root.moveResizeRequest(window->window(), Xcb::toXNative(window->x()), Xcb::toXNative(window->y()), NET::TopLeft, XCB_BUTTON_INDEX_1);
        xcb_flush(c.get());
    }
    QSignalSpy interactiveMoveResizeStartedSpy(window, &X11Window::interactiveMoveResizeStarted);
    QVERIFY(!interactiveMoveResizeStartedSpy.wait(10));
}

void X11WindowTest::testNetWmButtonSizeCancel()
{
    // This test verifies that a client can start an interactive resize and then cancel it.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(100, 100, 300, 400));

    // Request interactive resize.
    const QRectF originalGeometry = window->frameGeometry();
    quint32 timestamp = 0;
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    auto releaseButton = qScopeGuard([&timestamp]() {
        Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    });
    Test::pointerMotion(originalGeometry.topLeft(), timestamp++);
    {
        NETRootInfo root(c.get(), NET::Properties());
        root.moveResizeRequest(window->window(), Xcb::toXNative(window->x()), Xcb::toXNative(window->y()), NET::TopLeft, XCB_BUTTON_INDEX_1);
        xcb_flush(c.get());
    }
    QSignalSpy interactiveMoveResizeStartedSpy(window, &X11Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &X11Window::interactiveMoveResizeFinished);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &X11Window::interactiveMoveResizeStepped);
    QVERIFY(interactiveMoveResizeStartedSpy.wait());
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QVERIFY(window->isInteractiveResize());

    // Resize the window a tiny bit.
    Test::pointerMotionRelative(QPointF(-8, -8), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->frameGeometry(), originalGeometry.adjusted(-8, -8, 0, 0));

    // Cancel the interactive resize.
    {
        NETRootInfo root(c.get(), NET::Properties());
        root.moveResizeRequest(window->window(), Xcb::toXNative(window->x()), Xcb::toXNative(window->y()), NET::MoveResizeCancel, XCB_BUTTON_INDEX_1);
        xcb_flush(c.get());
    }
    QVERIFY(interactiveMoveResizeFinishedSpy.wait());
    QCOMPARE(window->frameGeometry(), originalGeometry);
}

void X11WindowTest::testMinimumSize()
{
    // This test verifies that the minimum size constraint is correctly applied.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_size_hints_set_min_size(&hints, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.last().first().value<X11Window *>();
    QVERIFY(window);
    QVERIFY(window->isDecorated());

    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &Window::interactiveMoveResizeStepped);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);

    // Begin resize.
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!window->isInteractiveResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QVERIFY(window->isInteractiveResize());

    const QPointF cursorPos = KWin::Cursors::self()->mouse()->pos();

    const qreal scale = kwinApp()->xwaylandScale();
    window->keyPressEvent(Qt::Key_Left);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(-8, 0));
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    QCOMPARE(window->clientSize().width(), 100 / scale);

    window->keyPressEvent(Qt::Key_Right);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    QCOMPARE(window->clientSize().width(), 100 / scale);

    window->keyPressEvent(Qt::Key_Right);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    // whilst X11 window size goes through scale, the increment is a logical value kwin side
    QCOMPARE(window->clientSize().width(), 100 / scale + 8);

    window->keyPressEvent(Qt::Key_Up);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, -8));
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    QCOMPARE(window->clientSize().height(), 200 / scale);

    window->keyPressEvent(Qt::Key_Down);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    QCOMPARE(window->clientSize().height(), 200 / scale);

    window->keyPressEvent(Qt::Key_Down);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 8));
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 2);
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    QCOMPARE(window->clientSize().height(), 200 / scale + 8);

    // Finish the resize operation.
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 0);
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!window->isInteractiveResize());

    // Destroy the window.
    QSignalSpy windowClosedSpy(window, &X11Window::closed);
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    QVERIFY(windowClosedSpy.wait());
    c.reset();
}

void X11WindowTest::testMaximumSize()
{
    // This test verifies that the maximum size constraint is correctly applied.

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_size_hints_set_max_size(&hints, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.last().first().value<X11Window *>();
    QVERIFY(window);
    QVERIFY(window->isDecorated());

    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &Window::interactiveMoveResizeStepped);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);

    // Begin resize.
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!window->isInteractiveResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QVERIFY(window->isInteractiveResize());

    const QPointF cursorPos = KWin::Cursors::self()->mouse()->pos();

    const qreal scale = kwinApp()->xwaylandScale();
    window->keyPressEvent(Qt::Key_Right);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    QCOMPARE(window->clientSize().width(), 100 / scale);

    window->keyPressEvent(Qt::Key_Left);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos);
    QVERIFY(!interactiveMoveResizeSteppedSpy.wait(10));
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QCOMPARE(window->clientSize().width(), 100 / scale);

    window->keyPressEvent(Qt::Key_Left);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(-8, 0));
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    QCOMPARE(window->clientSize().width(), 100 / scale - 8);

    window->keyPressEvent(Qt::Key_Down);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(-8, 8));
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    QCOMPARE(window->clientSize().height(), 200 / scale);

    window->keyPressEvent(Qt::Key_Up);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(-8, 0));
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    QCOMPARE(window->clientSize().height(), 200 / scale);

    window->keyPressEvent(Qt::Key_Up);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(-8, -8));
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 2);
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    QCOMPARE(window->clientSize().height(), 200 / scale - 8);

    // Finish the resize operation.
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 0);
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!window->isInteractiveResize());

    // Destroy the window.
    QSignalSpy windowClosedSpy(window, &X11Window::closed);
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    QVERIFY(windowClosedSpy.wait());
    c.reset();
}

void X11WindowTest::testTrimCaption_data()
{
    QTest::addColumn<QByteArray>("originalTitle");
    QTest::addColumn<QByteArray>("expectedTitle");

    QTest::newRow("simplified")
        << QByteArrayLiteral("Was tun, wenn Schüler Autismus haben?\342\200\250\342\200\250\342\200\250 – Marlies Hübner - Mozilla Firefox")
        << QByteArrayLiteral("Was tun, wenn Schüler Autismus haben? – Marlies Hübner - Mozilla Firefox");

    QTest::newRow("with emojis")
        << QByteArrayLiteral("\bTesting non\302\255printable:\177, emoij:\360\237\230\203, non-characters:\357\277\276")
        << QByteArrayLiteral("Testing nonprintable:, emoij:\360\237\230\203, non-characters:");
}

void X11WindowTest::testTrimCaption()
{
    // this test verifies that caption is properly trimmed

    // create an xcb window
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    NETWinInfo winInfo(c.get(), windowId, rootWindow(), NET::Properties(), NET::Properties2());
    QFETCH(QByteArray, originalTitle);
    winInfo.setName(originalTitle);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QFETCH(QByteArray, expectedTitle);
    QCOMPARE(window->caption(), QString::fromUtf8(expectedTitle));

    // and destroy the window again
    xcb_unmap_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowClosedSpy(window, &X11Window::closed);
    QVERIFY(windowClosedSpy.wait());
    xcb_destroy_window(c.get(), windowId);
    c.reset();
}

void X11WindowTest::testFullscreenLayerWithActiveWaylandWindow()
{
    // this test verifies that an X11 fullscreen window does not stay in the active layer
    // when a Wayland window is active, see BUG: 375759
    QCOMPARE(workspace()->outputs().count(), 1);

    // first create an X11 window
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QVERIFY(!window->isFullScreen());
    QVERIFY(window->isActive());
    QCOMPARE(window->layer(), NormalLayer);

    workspace()->slotWindowFullScreen();
    QVERIFY(window->isFullScreen());
    QCOMPARE(window->layer(), ActiveLayer);
    QCOMPARE(workspace()->stackingOrder().last(), window);

    // now let's open a Wayland window
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(waylandWindow);
    QVERIFY(waylandWindow->isActive());
    QCOMPARE(waylandWindow->layer(), NormalLayer);
    QCOMPARE(workspace()->stackingOrder().last(), waylandWindow);
    QCOMPARE(workspace()->stackingOrder().last(), waylandWindow);
    QCOMPARE(window->layer(), NormalLayer);

    // now activate fullscreen again
    workspace()->activateWindow(window);
    QTRY_VERIFY(window->isActive());
    QCOMPARE(window->layer(), ActiveLayer);
    QCOMPARE(workspace()->stackingOrder().last(), window);
    QCOMPARE(workspace()->stackingOrder().last(), window);

    // activate wayland window again
    workspace()->activateWindow(waylandWindow);
    QTRY_VERIFY(waylandWindow->isActive());
    QCOMPARE(workspace()->stackingOrder().last(), waylandWindow);
    QCOMPARE(workspace()->stackingOrder().last(), waylandWindow);

    // back to x window
    workspace()->activateWindow(window);
    QTRY_VERIFY(window->isActive());
    // remove fullscreen
    QVERIFY(window->isFullScreen());
    workspace()->slotWindowFullScreen();
    QVERIFY(!window->isFullScreen());
    // and fullscreen again
    workspace()->slotWindowFullScreen();
    QVERIFY(window->isFullScreen());
    QCOMPARE(workspace()->stackingOrder().last(), window);
    QCOMPARE(workspace()->stackingOrder().last(), window);

    // activate wayland window again
    workspace()->activateWindow(waylandWindow);
    QTRY_VERIFY(waylandWindow->isActive());
    QCOMPARE(workspace()->stackingOrder().last(), waylandWindow);
    QCOMPARE(workspace()->stackingOrder().last(), waylandWindow);

    // back to X11 window
    workspace()->activateWindow(window);
    QTRY_VERIFY(window->isActive());
    // remove fullscreen
    QVERIFY(window->isFullScreen());
    workspace()->slotWindowFullScreen();
    QVERIFY(!window->isFullScreen());

    // and fullscreen through X API
    {
        Xcb::sync(); // sync so NETWinInfo fetches the correct current state

        NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::Properties(), NET::Properties2());
        info.setState(NET::FullScreen, NET::FullScreen);
        NETRootInfo rootInfo(c.get(), NET::Properties());
        rootInfo.setActiveWindow(windowId, NET::FromApplication, XCB_CURRENT_TIME, XCB_WINDOW_NONE);
        xcb_flush(c.get());
        QTRY_VERIFY(window->isFullScreen());
        QCOMPARE(workspace()->stackingOrder().last(), window);
        QCOMPARE(workspace()->stackingOrder().last(), window);
    }

    // activate wayland window again
    workspace()->activateWindow(waylandWindow);
    QTRY_VERIFY(waylandWindow->isActive());
    QCOMPARE(workspace()->stackingOrder().last(), waylandWindow);
    QCOMPARE(workspace()->stackingOrder().last(), waylandWindow);
    QCOMPARE(window->layer(), NormalLayer);

    // close the window
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowClosed(waylandWindow));
    QTRY_VERIFY(window->isActive());
    QCOMPARE(window->layer(), ActiveLayer);

    // and destroy the window again
    xcb_unmap_window(c.get(), windowId);
    xcb_flush(c.get());
}

void X11WindowTest::testFocusInWithWaylandLastActiveWindow()
{
    // this test verifies that Workspace::allowWindowActivation does not crash if last client was a Wayland client

    // create an X11 window
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QVERIFY(window->isActive());

    // create Wayland window
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(waylandWindow);
    QVERIFY(waylandWindow->isActive());
    // activate no window
    workspace()->setActiveWindow(nullptr);
    QVERIFY(!waylandWindow->isActive());
    QVERIFY(!workspace()->activeWindow());
    // and close Wayland window again
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowClosed(waylandWindow));

    // and try to activate the x11 window through X11 api
    const auto cookie = xcb_set_input_focus_checked(c.get(), XCB_INPUT_FOCUS_NONE, windowId, XCB_CURRENT_TIME);
    auto error = xcb_request_check(c.get(), cookie);
    QVERIFY(!error);
    // this accesses m_lastActiveWindow on trying to activate
    QTRY_VERIFY(window->isActive());

    // and destroy the window again
    xcb_unmap_window(c.get(), windowId);
    xcb_flush(c.get());
}

void X11WindowTest::testCaptionChanges()
{
    // verifies that caption is updated correctly when the X11 window updates it
    // BUG: 383444
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::Properties(), NET::Properties2());
    info.setName("foo");
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QCOMPARE(window->caption(), QStringLiteral("foo"));

    QSignalSpy captionChangedSpy(window, &X11Window::captionChanged);
    info.setName("bar");
    xcb_flush(c.get());
    QVERIFY(captionChangedSpy.wait());
    QCOMPARE(window->caption(), QStringLiteral("bar"));

    // and destroy the window again
    QSignalSpy windowClosedSpy(window, &X11Window::closed);
    xcb_unmap_window(c.get(), windowId);
    xcb_flush(c.get());
    QVERIFY(windowClosedSpy.wait());
    xcb_destroy_window(c.get(), windowId);
    c.reset();
}

void X11WindowTest::testCaptionWmName()
{
    // this test verifies that a caption set through WM_NAME is read correctly

    // open glxgears as that one only uses WM_NAME
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);

    QProcess glxgears;
    glxgears.setProgram(QStringLiteral("glxgears"));
    glxgears.start();
    QVERIFY(glxgears.waitForStarted());

    QVERIFY(windowAddedSpy.wait());
    QCOMPARE(windowAddedSpy.count(), 1);
    QCOMPARE(workspace()->windows().count(), 1);
    Window *glxgearsWindow = workspace()->windows().first();
    QCOMPARE(glxgearsWindow->caption(), QStringLiteral("glxgears"));

    glxgears.terminate();
    QVERIFY(glxgears.waitForFinished());
}

void X11WindowTest::testFullscreenWindowGroups()
{
    // this test creates an X11 window and puts it to full screen
    // then a second window is created which is in the same window group
    // BUG: 388310

    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QCOMPARE(window->isActive(), true);

    QCOMPARE(window->isFullScreen(), false);
    QCOMPARE(window->layer(), NormalLayer);
    workspace()->slotWindowFullScreen();
    QCOMPARE(window->isFullScreen(), true);
    QCOMPARE(window->layer(), ActiveLayer);

    // now let's create a second window
    windowCreatedSpy.clear();
    xcb_window_t w2 = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, w2, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints2;
    memset(&hints2, 0, sizeof(hints2));
    xcb_icccm_size_hints_set_position(&hints2, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints2, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), w2, &hints2);
    xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, w2, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
    xcb_map_window(c.get(), w2);
    xcb_flush(c.get());

    QVERIFY(windowCreatedSpy.wait());
    X11Window *window2 = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window2);
    QVERIFY(window != window2);
    QCOMPARE(window2->window(), w2);
    QCOMPARE(window2->isActive(), true);
    QCOMPARE(window2->group(), window->group());
    // first window should be moved back to normal layer
    QCOMPARE(window->isActive(), false);
    QCOMPARE(window->isFullScreen(), true);
    QCOMPARE(window->layer(), NormalLayer);

    // activating the fullscreen window again, should move it to active layer
    workspace()->activateWindow(window);
    QTRY_COMPARE(window->layer(), ActiveLayer);
}

void X11WindowTest::testActivateFocusedWindow()
{
    // The window manager may call XSetInputFocus() on a window that already has focus, in which
    // case no FocusIn event will be generated and the window won't be marked as active. This test
    // verifies that we handle that subtle case properly.

    Test::XcbConnectionPtr connection = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(connection.get()));

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);

    const QRect windowGeometry(0, 0, 100, 200);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());

    // Create the first test window.
    const xcb_window_t windowId1 = xcb_generate_id(connection.get());
    xcb_create_window(connection.get(), XCB_COPY_FROM_PARENT, windowId1, rootWindow(),
                      windowGeometry.x(), windowGeometry.y(),
                      windowGeometry.width(), windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_icccm_set_wm_normal_hints(connection.get(), windowId1, &hints);
    xcb_change_property(connection.get(), XCB_PROP_MODE_REPLACE, windowId1,
                        atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId1);
    xcb_map_window(connection.get(), windowId1);
    xcb_flush(connection.get());
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window1 = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window1);
    QCOMPARE(window1->window(), windowId1);
    QCOMPARE(window1->isActive(), true);

    // Create the second test window.
    const xcb_window_t windowId2 = xcb_generate_id(connection.get());
    xcb_create_window(connection.get(), XCB_COPY_FROM_PARENT, windowId2, rootWindow(),
                      windowGeometry.x(), windowGeometry.y(),
                      windowGeometry.width(), windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_icccm_set_wm_normal_hints(connection.get(), windowId2, &hints);
    xcb_change_property(connection.get(), XCB_PROP_MODE_REPLACE, windowId2,
                        atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId2);
    xcb_map_window(connection.get(), windowId2);
    xcb_flush(connection.get());
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window2 = windowCreatedSpy.last().first().value<X11Window *>();
    QVERIFY(window2);
    QCOMPARE(window2->window(), windowId2);
    QCOMPARE(window2->isActive(), true);

    // When the second test window is destroyed, the window manager will attempt to activate the
    // next window in the focus chain, which is the first window.
    xcb_set_input_focus(connection.get(), XCB_INPUT_FOCUS_POINTER_ROOT, windowId1, XCB_CURRENT_TIME);
    xcb_destroy_window(connection.get(), windowId2);
    xcb_flush(connection.get());
    QVERIFY(Test::waitForWindowClosed(window2));
    QVERIFY(window1->isActive());

    // Destroy the first test window.
    xcb_destroy_window(connection.get(), windowId1);
    xcb_flush(connection.get());
    QVERIFY(Test::waitForWindowClosed(window1));
}

void X11WindowTest::testReentrantMoveResize()
{
    // This test verifies that calling moveResize() from a slot connected directly
    // to the frameGeometryChanged() signal won't cause an infinite recursion.

    // Create a test window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->pos(), QPoint(0, 0));

    // Let's pretend that there is a script that really wants the window to be at (100, 100).
    connect(window, &Window::frameGeometryChanged, this, [window]() {
        window->moveResize(QRectF(QPointF(100, 100), window->size()));
    });

    // Trigger the lambda above.
    window->move(QPoint(40, 50));

    // Eventually, the window will end up at (100, 100).
    QCOMPARE(window->pos(), QPoint(100, 100));

    // Destroy the test window.
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    QVERIFY(Test::waitForWindowClosed(window));
}

void X11WindowTest::testTransient()
{
    // Create a parent and a child windows.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *parent = createWindow(c.get(), QRect(0, 0, 100, 200));
    X11Window *child = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &parent](xcb_window_t windowId) {
        xcb_icccm_set_wm_transient_for(c.get(), windowId, parent->window());
    });
    QVERIFY(child->isTransient());
    QCOMPARE(child->transientFor(), parent);
    QVERIFY(parent->hasTransient(child, true));
}

void X11WindowTest::testGroupTransient()
{
    // Create the leader, a follower and a dialog window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *leader = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
    });
    X11Window *follower = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &leader](xcb_window_t windowId) {
        const xcb_window_t leaderId = leader->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
    });
    X11Window *dialog = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &leader](xcb_window_t windowId) {
        const xcb_window_t leaderId = leader->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
        xcb_icccm_set_wm_transient_for(c.get(), windowId, kwinApp()->x11RootWindow());
    });
    QVERIFY(dialog->isTransient());
    QCOMPARE(dialog->transientFor(), nullptr);
    QVERIFY(leader->hasTransient(dialog, true));
    QVERIFY(follower->hasTransient(dialog, true));

    // The group transient should not act as transient for unrelated windows.
    Test::XcbConnectionPtr c1 = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *unrelated = createWindow(c1.get(), QRect(0, 0, 100, 200));
    QVERIFY(!unrelated->hasTransient(dialog, true));
}

void X11WindowTest::testCloseTransient()
{
    // This test verifies that the parent window will be activated when a transient is closed.

    // Create a parent and a child windows.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *parent = createWindow(c.get(), QRect(0, 0, 100, 200));
    X11Window *child = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &parent](xcb_window_t windowId) {
        xcb_icccm_set_wm_transient_for(c.get(), windowId, parent->window());
    });
    QCOMPARE(child->transientFor(), parent);
    QCOMPARE(workspace()->activeWindow(), child);

    // Close the child.
    QSignalSpy childClosedSpy(child, &Window::closed);
    xcb_unmap_window(c.get(), child->window());
    xcb_destroy_window(c.get(), child->window());
    xcb_flush(c.get());
    QVERIFY(childClosedSpy.wait());
    QCOMPARE(workspace()->activeWindow(), parent);
}

void X11WindowTest::testCloseInactiveTransient()
{
    // This test verifies that the parent window will not be activated when an inactive transient is closed.

    // Create a parent and a child windows.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *parent = createWindow(c.get(), QRect(0, 0, 100, 200));
    X11Window *child = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &parent](xcb_window_t windowId) {
        xcb_icccm_set_wm_transient_for(c.get(), windowId, parent->window());
    });
    QCOMPARE(child->transientFor(), parent);
    QCOMPARE(workspace()->activeWindow(), child);

    // Show another window.
    Test::XcbConnectionPtr c1 = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *unrelated = createWindow(c1.get(), QRect(0, 0, 100, 200));
    QCOMPARE(workspace()->activeWindow(), unrelated);

    // Close the child.
    QSignalSpy childClosedSpy(child, &Window::closed);
    xcb_unmap_window(c.get(), child->window());
    xcb_destroy_window(c.get(), child->window());
    xcb_flush(c.get());
    QVERIFY(childClosedSpy.wait());
    QCOMPARE(workspace()->activeWindow(), unrelated);
}

void X11WindowTest::testCloseGroupTransient()
{
    // This test verifies that when an active group transient is closed, the focus will be passed to one of its main windows.

    // Create the leader, a follower and a dialog window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *leader = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
    });
    X11Window *follower = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &leader](xcb_window_t windowId) {
        const xcb_window_t leaderId = leader->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
    });
    X11Window *dialog = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &leader](xcb_window_t windowId) {
        const xcb_window_t leaderId = leader->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
        xcb_icccm_set_wm_transient_for(c.get(), windowId, kwinApp()->x11RootWindow());
    });
    QVERIFY(dialog->isTransient());
    QCOMPARE(dialog->transientFor(), nullptr);
    QVERIFY(leader->hasTransient(dialog, true));
    QVERIFY(follower->hasTransient(dialog, true));
    QCOMPARE(workspace()->activeWindow(), dialog);

    // Close the dialog.
    QSignalSpy dialogClosedSpy(dialog, &Window::closed);
    xcb_unmap_window(c.get(), dialog->window());
    xcb_destroy_window(c.get(), dialog->window());
    xcb_flush(c.get());
    QVERIFY(dialogClosedSpy.wait());
    QVERIFY(workspace()->activeWindow() == leader || workspace()->activeWindow() == follower);
}

void X11WindowTest::testCloseInactiveGroupTransient()
{
    // This test verifies that when an inactive group transient is closed, the focus will not be passed to one of its main windows.

    // Create the leader, a follower and a dialog window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *leader = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
    });
    X11Window *follower = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &leader](xcb_window_t windowId) {
        const xcb_window_t leaderId = leader->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
    });
    X11Window *dialog = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &leader](xcb_window_t windowId) {
        const xcb_window_t leaderId = leader->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
        xcb_icccm_set_wm_transient_for(c.get(), windowId, kwinApp()->x11RootWindow());
    });
    QVERIFY(dialog->isTransient());
    QCOMPARE(dialog->transientFor(), nullptr);
    QVERIFY(leader->hasTransient(dialog, true));
    QVERIFY(follower->hasTransient(dialog, true));
    QCOMPARE(workspace()->activeWindow(), dialog);

    // Show another window.
    Test::XcbConnectionPtr c1 = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *unrelated = createWindow(c1.get(), QRect(0, 0, 100, 200));
    QCOMPARE(workspace()->activeWindow(), unrelated);

    // Close the dialog.
    QSignalSpy dialogClosedSpy(dialog, &Window::closed);
    xcb_unmap_window(c.get(), dialog->window());
    xcb_destroy_window(c.get(), dialog->window());
    xcb_flush(c.get());
    QVERIFY(dialogClosedSpy.wait());
    QCOMPARE(workspace()->activeWindow(), unrelated);
}

void X11WindowTest::testModal()
{
    // Create a parent and a child windows.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *parent = createWindow(c.get(), QRect(0, 0, 100, 200));
    X11Window *child = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &parent](xcb_window_t windowId) {
        xcb_icccm_set_wm_transient_for(c.get(), windowId, parent->window());
    });
    QVERIFY(!child->isModal());
    QCOMPARE(child->transientFor(), parent);

    // Set modal state.
    {
        NETWinInfo info(c.get(), child->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::Modal, NET::Modal);
        xcb_flush(c.get());
    }
    QSignalSpy modalChangedSpy(child, &Window::modalChanged);
    QVERIFY(modalChangedSpy.wait());
    QVERIFY(child->isModal());

    // Unset modal state.
    {
        NETWinInfo info(c.get(), child->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::State(), NET::Modal);
        xcb_flush(c.get());
    }
    QVERIFY(modalChangedSpy.wait());
    QVERIFY(!child->isModal());

    // Set modal state and try to activate the parent window, it should not succeed.
    {
        NETWinInfo info(c.get(), child->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::Modal, NET::Modal);
        xcb_flush(c.get());
    }
    QVERIFY(modalChangedSpy.wait());
    QVERIFY(child->isModal());
    workspace()->activateWindow(parent);
    QCOMPARE(workspace()->activeWindow(), child);

    // It should be okay to activate an unrelated window.
    Test::XcbConnectionPtr c1 = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *unrelated = createWindow(c1.get(), QRect(0, 0, 100, 200));
    QCOMPARE(workspace()->activeWindow(), unrelated);
}

void X11WindowTest::testGroupModal()
{
    // This test verifies that a dialog can be modal to the window group.

    // Create the leader, a follower and a dialog window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *leader = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
    });
    X11Window *follower = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &leader](xcb_window_t windowId) {
        const xcb_window_t leaderId = leader->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
    });
    X11Window *dialog = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &leader](xcb_window_t windowId) {
        const xcb_window_t leaderId = leader->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
        xcb_icccm_set_wm_transient_for(c.get(), windowId, kwinApp()->x11RootWindow());
    });
    QVERIFY(dialog->isTransient());
    QVERIFY(leader->hasTransient(dialog, true));
    QVERIFY(follower->hasTransient(dialog, true));

    // Set modal state.
    {
        NETWinInfo info(c.get(), dialog->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::Modal, NET::Modal);
        xcb_flush(c.get());
    }
    QSignalSpy modalChangedSpy(dialog, &Window::modalChanged);
    QVERIFY(modalChangedSpy.wait());
    QVERIFY(dialog->isModal());

    // Unset modal state.
    {
        NETWinInfo info(c.get(), dialog->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::State(), NET::Modal);
        xcb_flush(c.get());
    }
    QVERIFY(modalChangedSpy.wait());
    QVERIFY(!dialog->isModal());

    // Set modal state and try to activate other windows in the group, it should not succeed.
    {
        NETWinInfo info(c.get(), dialog->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::Modal, NET::Modal);
        xcb_flush(c.get());
    }
    QVERIFY(modalChangedSpy.wait());
    QVERIFY(dialog->isModal());
    workspace()->activateWindow(leader);
    QCOMPARE(workspace()->activeWindow(), dialog);
    workspace()->activateWindow(follower);
    QCOMPARE(workspace()->activeWindow(), dialog);

    // It should be okay to activate an unrelated window.
    Test::XcbConnectionPtr c1 = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *unrelated = createWindow(c1.get(), QRect(0, 0, 100, 200));
    QCOMPARE(workspace()->activeWindow(), unrelated);
}

void X11WindowTest::testCloseModal()
{
    // This test verifies that the parent window will be activated when an active modal dialog is closed.

    // Create a parent and a child windows.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *parent = createWindow(c.get(), QRect(0, 0, 100, 200));
    X11Window *child = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &parent](xcb_window_t windowId) {
        xcb_icccm_set_wm_transient_for(c.get(), windowId, parent->window());
    });
    QVERIFY(!child->isModal());
    QCOMPARE(child->transientFor(), parent);

    // Set modal state.
    {
        NETWinInfo info(c.get(), child->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::Modal, NET::Modal);
        xcb_flush(c.get());
    }
    QSignalSpy modalChangedSpy(child, &Window::modalChanged);
    QVERIFY(modalChangedSpy.wait());
    QVERIFY(child->isModal());
    QCOMPARE(workspace()->activeWindow(), child);

    // Close the child.
    QSignalSpy childClosedSpy(child, &Window::closed);
    xcb_unmap_window(c.get(), child->window());
    xcb_destroy_window(c.get(), child->window());
    xcb_flush(c.get());
    QVERIFY(childClosedSpy.wait());
    QCOMPARE(workspace()->activeWindow(), parent);
}

void X11WindowTest::testCloseInactiveModal()
{
    // This test verifies that the parent window will not be activated when an inactive modal dialog is closed.

    // Create a parent and a child windows.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *parent = createWindow(c.get(), QRect(0, 0, 100, 200));
    X11Window *child = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &parent](xcb_window_t windowId) {
        xcb_icccm_set_wm_transient_for(c.get(), windowId, parent->window());
    });
    QVERIFY(!child->isModal());
    QCOMPARE(child->transientFor(), parent);

    // Set modal state.
    {
        NETWinInfo info(c.get(), child->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::Modal, NET::Modal);
        xcb_flush(c.get());
    }
    QSignalSpy modalChangedSpy(child, &Window::modalChanged);
    QVERIFY(modalChangedSpy.wait());
    QVERIFY(child->isModal());
    QCOMPARE(workspace()->activeWindow(), child);

    // Show another window.
    Test::XcbConnectionPtr c1 = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *unrelated = createWindow(c1.get(), QRect(0, 0, 100, 200));
    QCOMPARE(workspace()->activeWindow(), unrelated);

    // Close the child.
    QSignalSpy childClosedSpy(child, &Window::closed);
    xcb_unmap_window(c.get(), child->window());
    xcb_destroy_window(c.get(), child->window());
    xcb_flush(c.get());
    QVERIFY(childClosedSpy.wait());
    QCOMPARE(workspace()->activeWindow(), unrelated);
}

void X11WindowTest::testCloseGroupModal()
{
    // This test verifies that when an active modal group dialog is closed, the focus will be passed to one of its main windows.

    // Create the leader, a follower and a dialog window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *leader = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
    });
    X11Window *follower = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &leader](xcb_window_t windowId) {
        const xcb_window_t leaderId = leader->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
    });
    X11Window *dialog = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &leader](xcb_window_t windowId) {
        const xcb_window_t leaderId = leader->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
        xcb_icccm_set_wm_transient_for(c.get(), windowId, kwinApp()->x11RootWindow());
    });
    QVERIFY(dialog->isTransient());
    QVERIFY(leader->hasTransient(dialog, true));
    QVERIFY(follower->hasTransient(dialog, true));

    // Set modal state.
    {
        NETWinInfo info(c.get(), dialog->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::Modal, NET::Modal);
        xcb_flush(c.get());
    }
    QSignalSpy modalChangedSpy(dialog, &Window::modalChanged);
    QVERIFY(modalChangedSpy.wait());
    QVERIFY(dialog->isModal());
    QCOMPARE(workspace()->activeWindow(), dialog);

    // Close the dialog.
    QSignalSpy dialogClosedSpy(dialog, &Window::closed);
    xcb_unmap_window(c.get(), dialog->window());
    xcb_destroy_window(c.get(), dialog->window());
    xcb_flush(c.get());
    QVERIFY(dialogClosedSpy.wait());
    QVERIFY(workspace()->activeWindow() == leader || workspace()->activeWindow() == follower);
}

void X11WindowTest::testCloseInactiveGroupModal()
{
    // This test verifies that when an inactive modal group dialog is closed, the focus will not be passed to one of its main windows.

    // Create the leader, a follower and a dialog window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *leader = createWindow(c.get(), QRect(0, 0, 100, 200), [&c](xcb_window_t windowId) {
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
    });
    X11Window *follower = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &leader](xcb_window_t windowId) {
        const xcb_window_t leaderId = leader->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
    });
    X11Window *dialog = createWindow(c.get(), QRect(0, 0, 100, 200), [&c, &leader](xcb_window_t windowId) {
        const xcb_window_t leaderId = leader->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
        xcb_icccm_set_wm_transient_for(c.get(), windowId, kwinApp()->x11RootWindow());
    });
    QVERIFY(dialog->isTransient());
    QVERIFY(leader->hasTransient(dialog, true));
    QVERIFY(follower->hasTransient(dialog, true));

    // Set modal state.
    {
        NETWinInfo info(c.get(), dialog->window(), kwinApp()->x11RootWindow(), NET::WMState, NET::Properties2());
        info.setState(NET::Modal, NET::Modal);
        xcb_flush(c.get());
    }
    QSignalSpy modalChangedSpy(dialog, &Window::modalChanged);
    QVERIFY(modalChangedSpy.wait());
    QVERIFY(dialog->isModal());
    QCOMPARE(workspace()->activeWindow(), dialog);

    // Show another window.
    Test::XcbConnectionPtr c1 = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *unrelated = createWindow(c1.get(), QRect(0, 0, 100, 200));
    QCOMPARE(workspace()->activeWindow(), unrelated);

    // Close the dialog.
    QSignalSpy dialogClosedSpy(dialog, &Window::closed);
    xcb_unmap_window(c.get(), dialog->window());
    xcb_destroy_window(c.get(), dialog->window());
    xcb_flush(c.get());
    QVERIFY(dialogClosedSpy.wait());
    QCOMPARE(workspace()->activeWindow(), unrelated);
}

void X11WindowTest::testStackAboveFromApplication()
{
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    NETRootInfo root(c.get(), NET::Properties());

    X11Window *window1 = createWindow(c.get(), QRect(0, 0, 100, 100), [&](xcb_window_t windowId) {
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
    });
    X11Window *window2 = createWindow(c.get(), QRect(0, 0, 100, 100), [&](xcb_window_t windowId) {
        xcb_window_t leaderId = window1->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
    });
    X11Window *window3 = createWindow(c.get(), QRect(0, 0, 100, 100));
    X11Window *window4 = createWindow(c.get(), QRect(0, 0, 100, 100));

    // window1 and window2 belong to the same application, window1 will be raised only above window2.
    root.restackRequest(window1->window(), NET::FromApplication, XCB_WINDOW_NONE, XCB_STACK_MODE_ABOVE, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window2, window1, window3, window4}));

    // There are no other windows that belong to the same application as window3, so the stack won't change.
    root.restackRequest(window3->window(), NET::FromApplication, XCB_WINDOW_NONE, XCB_STACK_MODE_ABOVE, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(!stackingOrderChangedSpy.wait(10));
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window2, window1, window3, window4}));
}

void X11WindowTest::testStackAboveFromTool()
{
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    NETRootInfo root(c.get(), NET::Properties());

    X11Window *window1 = createWindow(c.get(), QRect(0, 0, 100, 100), [&](xcb_window_t windowId) {
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
    });
    X11Window *window2 = createWindow(c.get(), QRect(0, 0, 100, 100), [&](xcb_window_t windowId) {
        xcb_window_t leaderId = window1->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
    });
    X11Window *window3 = createWindow(c.get(), QRect(0, 0, 100, 100));
    X11Window *window4 = createWindow(c.get(), QRect(0, 0, 100, 100));

    // window1 and window2 belong to the same application, but window1 will be raised globally because of the from_tool flag.
    root.restackRequest(window1->window(), NET::FromTool, XCB_WINDOW_NONE, XCB_STACK_MODE_ABOVE, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window2, window3, window4, window1}));

    // same with window3, whether there are other windows that belong to the same application doesn't matter.
    root.restackRequest(window3->window(), NET::FromTool, XCB_WINDOW_NONE, XCB_STACK_MODE_ABOVE, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window2, window4, window1, window3}));
}

void X11WindowTest::testStackAboveSibling()
{
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    NETRootInfo root(c.get(), NET::Properties());

    X11Window *window1 = createWindow(c.get(), QRect(0, 0, 100, 100));
    X11Window *window2 = createWindow(c.get(), QRect(0, 0, 100, 100));
    X11Window *window3 = createWindow(c.get(), QRect(0, 0, 100, 100));

    // Restack window1 above window3.
    root.restackRequest(window1->window(), NET::FromApplication, window3->window(), XCB_STACK_MODE_ABOVE, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window2, window3, window1}));

    // Repeat again.
    root.restackRequest(window1->window(), NET::FromApplication, window3->window(), XCB_STACK_MODE_ABOVE, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(!stackingOrderChangedSpy.wait(10));
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window2, window3, window1}));

    // Restack window1 above window2. Note that since window1 is already window2, it's okay if the WM noops.
    root.restackRequest(window1->window(), NET::FromApplication, window2->window(), XCB_STACK_MODE_ABOVE, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window2, window1, window3}));
}

void X11WindowTest::testStackBelowFromApplication()
{
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    NETRootInfo root(c.get(), NET::Properties());

    X11Window *window1 = createWindow(c.get(), QRect(0, 0, 100, 100));
    X11Window *window2 = createWindow(c.get(), QRect(0, 0, 100, 100), [&](xcb_window_t windowId) {
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
    });
    X11Window *window3 = createWindow(c.get(), QRect(0, 0, 100, 100), [&](xcb_window_t windowId) {
        xcb_window_t leaderId = window2->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
    });
    X11Window *window4 = createWindow(c.get(), QRect(0, 0, 100, 100));

    root.restackRequest(window3->window(), NET::FromApplication, XCB_WINDOW_NONE, XCB_STACK_MODE_BELOW, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window3, window1, window2, window4}));

    root.restackRequest(window4->window(), NET::FromApplication, XCB_WINDOW_NONE, XCB_STACK_MODE_BELOW, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window4, window3, window1, window2}));
}

void X11WindowTest::testStackBelowFromTool()
{
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    NETRootInfo root(c.get(), NET::Properties());

    X11Window *window1 = createWindow(c.get(), QRect(0, 0, 100, 100));
    X11Window *window2 = createWindow(c.get(), QRect(0, 0, 100, 100), [&](xcb_window_t windowId) {
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
    });
    X11Window *window3 = createWindow(c.get(), QRect(0, 0, 100, 100), [&](xcb_window_t windowId) {
        xcb_window_t leaderId = window2->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
    });
    X11Window *window4 = createWindow(c.get(), QRect(0, 0, 100, 100));

    // window2 and window3 belong to the same application, but window3 will be lowered globally because of the from_tool flag.
    root.restackRequest(window3->window(), NET::FromApplication, XCB_WINDOW_NONE, XCB_STACK_MODE_BELOW, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window3, window1, window2, window4}));

    // same with window4.
    root.restackRequest(window4->window(), NET::FromApplication, XCB_WINDOW_NONE, XCB_STACK_MODE_BELOW, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window4, window3, window1, window2}));
}

void X11WindowTest::testStackBelowSibling()
{
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    NETRootInfo root(c.get(), NET::Properties());

    X11Window *window1 = createWindow(c.get(), QRect(0, 0, 100, 100));
    X11Window *window2 = createWindow(c.get(), QRect(0, 0, 100, 100));
    X11Window *window3 = createWindow(c.get(), QRect(0, 0, 100, 100));

    // Restack window3 below window2.
    root.restackRequest(window3->window(), NET::FromApplication, window2->window(), XCB_STACK_MODE_BELOW, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1, window3, window2}));

    // Repeat again.
    root.restackRequest(window3->window(), NET::FromApplication, window2->window(), XCB_STACK_MODE_BELOW, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(!stackingOrderChangedSpy.wait(10));
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1, window3, window2}));

    // Restack window1 below window2. Note that since window1 is already window2, it's okay if the WM noops.
    root.restackRequest(window1->window(), NET::FromApplication, window2->window(), XCB_STACK_MODE_BELOW, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window3, window1, window2}));
}

void X11WindowTest::testStackTopIfFromApplication()
{
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    NETRootInfo root(c.get(), NET::Properties());

    X11Window *window1 = createWindow(c.get(), QRect(100, 0, 100, 100), [&](xcb_window_t windowId) {
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
        applyMotifHints(c.get(), windowId, MotifHints{
                                               .flags = MWM_HINTS_DECORATIONS,
                                               .decorations = 0,
                                           });
    });
    X11Window *window2 = createWindow(c.get(), QRect(200, 0, 100, 100), [&](xcb_window_t windowId) {
        xcb_window_t leaderId = window1->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
        applyMotifHints(c.get(), windowId, MotifHints{
                                               .flags = MWM_HINTS_DECORATIONS,
                                               .decorations = 0,
                                           });
    });
    X11Window *window3 = createWindow(c.get(), QRect(300, 0, 100, 100), [&](xcb_window_t windowId) {
        applyMotifHints(c.get(), windowId, MotifHints{
                                               .flags = MWM_HINTS_DECORATIONS,
                                               .decorations = 0,
                                           });
    });

    // Restack window1 above window2, no change will occur because there's no overlap.
    root.restackRequest(window1->window(), NET::FromApplication, window2->window(), XCB_STACK_MODE_TOP_IF, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);
    QVERIFY(!stackingOrderChangedSpy.wait(10));
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1, window2, window3}));

    // Create an overlap between window1 and window2, now, TopIf should work.
    root.moveResizeWindowRequest(window1->window(), (1 << 8) | (1 << 9), 150, 0, 0, 0);
    root.restackRequest(window1->window(), NET::FromApplication, window2->window(), XCB_STACK_MODE_TOP_IF, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window2, window1, window3}));

    // Repeat again
    root.restackRequest(window1->window(), NET::FromApplication, window2->window(), XCB_STACK_MODE_TOP_IF, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(!stackingOrderChangedSpy.wait(10));
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window2, window1, window3}));
}

void X11WindowTest::testStackTopIfFromTool()
{
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    NETRootInfo root(c.get(), NET::Properties());

    X11Window *window1 = createWindow(c.get(), QRect(100, 0, 100, 100), [&](xcb_window_t windowId) {
        applyMotifHints(c.get(), windowId, MotifHints{
                                               .flags = MWM_HINTS_DECORATIONS,
                                               .decorations = 0,
                                           });
    });
    X11Window *window2 = createWindow(c.get(), QRect(200, 0, 100, 100), [&](xcb_window_t windowId) {
        applyMotifHints(c.get(), windowId, MotifHints{
                                               .flags = MWM_HINTS_DECORATIONS,
                                               .decorations = 0,
                                           });
    });
    X11Window *window3 = createWindow(c.get(), QRect(300, 0, 100, 100), [&](xcb_window_t windowId) {
        applyMotifHints(c.get(), windowId, MotifHints{
                                               .flags = MWM_HINTS_DECORATIONS,
                                               .decorations = 0,
                                           });
    });

    // Restack window1 above window2, no change will occur because there's no overlap.
    root.restackRequest(window1->window(), NET::FromTool, window2->window(), XCB_STACK_MODE_TOP_IF, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);
    QVERIFY(!stackingOrderChangedSpy.wait(10));
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1, window2, window3}));

    // Create an overlap between window1 and window2, now, TopIf should work.
    root.moveResizeWindowRequest(window1->window(), (1 << 8) | (1 << 9), 150, 0, 0, 0);
    root.restackRequest(window1->window(), NET::FromTool, window2->window(), XCB_STACK_MODE_TOP_IF, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window2, window3, window1}));

    // Repeat again
    root.restackRequest(window1->window(), NET::FromTool, window2->window(), XCB_STACK_MODE_TOP_IF, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(!stackingOrderChangedSpy.wait(10));
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window2, window3, window1}));
}

void X11WindowTest::testStackBottomIfFromApplication()
{
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    NETRootInfo root(c.get(), NET::Properties());

    X11Window *window1 = createWindow(c.get(), QRect(100, 0, 100, 100), [&](xcb_window_t windowId) {
        applyMotifHints(c.get(), windowId, MotifHints{
                                               .flags = MWM_HINTS_DECORATIONS,
                                               .decorations = 0,
                                           });
    });
    X11Window *window2 = createWindow(c.get(), QRect(200, 0, 100, 100), [&](xcb_window_t windowId) {
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
        applyMotifHints(c.get(), windowId, MotifHints{
                                               .flags = MWM_HINTS_DECORATIONS,
                                               .decorations = 0,
                                           });
    });
    X11Window *window3 = createWindow(c.get(), QRect(300, 0, 100, 100), [&](xcb_window_t windowId) {
        xcb_window_t leaderId = window2->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
        applyMotifHints(c.get(), windowId, MotifHints{
                                               .flags = MWM_HINTS_DECORATIONS,
                                               .decorations = 0,
                                           });
    });

    // Restack window3 below window2, no change will occur because there's no overlap.
    root.restackRequest(window3->window(), NET::FromApplication, window2->window(), XCB_STACK_MODE_BOTTOM_IF, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);
    QVERIFY(!stackingOrderChangedSpy.wait(10));
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1, window2, window3}));

    // Create an overlap between window2 and window3, now, BottomIf should work.
    root.moveResizeWindowRequest(window2->window(), (1 << 8) | (1 << 9), 250, 0, 0, 0);
    root.restackRequest(window3->window(), NET::FromApplication, window2->window(), XCB_STACK_MODE_BOTTOM_IF, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window3, window1, window2}));

    // Repeat again
    root.restackRequest(window3->window(), NET::FromApplication, window2->window(), XCB_STACK_MODE_BOTTOM_IF, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(!stackingOrderChangedSpy.wait(10));
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window3, window1, window2}));
}

void X11WindowTest::testStackBottomIfFromTool()
{
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    NETRootInfo root(c.get(), NET::Properties());

    X11Window *window1 = createWindow(c.get(), QRect(100, 0, 100, 100), [&](xcb_window_t windowId) {
        applyMotifHints(c.get(), windowId, MotifHints{
                                               .flags = MWM_HINTS_DECORATIONS,
                                               .decorations = 0,
                                           });
    });
    X11Window *window2 = createWindow(c.get(), QRect(200, 0, 100, 100), [&](xcb_window_t windowId) {
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
        applyMotifHints(c.get(), windowId, MotifHints{
                                               .flags = MWM_HINTS_DECORATIONS,
                                               .decorations = 0,
                                           });
    });
    X11Window *window3 = createWindow(c.get(), QRect(300, 0, 100, 100), [&](xcb_window_t windowId) {
        xcb_window_t leaderId = window2->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
        applyMotifHints(c.get(), windowId, MotifHints{
                                               .flags = MWM_HINTS_DECORATIONS,
                                               .decorations = 0,
                                           });
    });

    // Restack window3 below window2, no change will occur because there's no overlap.
    root.restackRequest(window3->window(), NET::FromTool, window2->window(), XCB_STACK_MODE_BOTTOM_IF, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);
    QVERIFY(!stackingOrderChangedSpy.wait(10));
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1, window2, window3}));

    // Create an overlap between window2 and window3, now, BottomIf should work.
    root.moveResizeWindowRequest(window2->window(), (1 << 8) | (1 << 9), 250, 0, 0, 0);
    root.restackRequest(window3->window(), NET::FromTool, window2->window(), XCB_STACK_MODE_BOTTOM_IF, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window3, window1, window2}));

    // Repeat again
    root.restackRequest(window3->window(), NET::FromTool, window2->window(), XCB_STACK_MODE_BOTTOM_IF, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(!stackingOrderChangedSpy.wait(10));
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window3, window1, window2}));
}

void X11WindowTest::testStackOppositeFromApplication()
{
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    NETRootInfo root(c.get(), NET::Properties());

    X11Window *window1 = createWindow(c.get(), QRect(100, 0, 100, 100), [&](xcb_window_t windowId) {
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
        applyMotifHints(c.get(), windowId, MotifHints{
                                               .flags = MWM_HINTS_DECORATIONS,
                                               .decorations = 0,
                                           });
    });
    X11Window *window2 = createWindow(c.get(), QRect(200, 0, 100, 100), [&](xcb_window_t windowId) {
        xcb_window_t leaderId = window1->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
        applyMotifHints(c.get(), windowId, MotifHints{
                                               .flags = MWM_HINTS_DECORATIONS,
                                               .decorations = 0,
                                           });
    });
    X11Window *window3 = createWindow(c.get(), QRect(300, 0, 100, 100), [&](xcb_window_t windowId) {
        applyMotifHints(c.get(), windowId, MotifHints{
                                               .flags = MWM_HINTS_DECORATIONS,
                                               .decorations = 0,
                                           });
    });

    // window2 is above window1, so it will be lowered
    root.restackRequest(window2->window(), NET::FromApplication, window1->window(), XCB_STACK_MODE_OPPOSITE, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window2, window1, window3}));

    // Repeat again
    root.restackRequest(window2->window(), NET::FromApplication, window1->window(), XCB_STACK_MODE_OPPOSITE, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1, window2, window3}));

    // Other window
    root.restackRequest(window2->window(), NET::FromApplication, window3->window(), XCB_STACK_MODE_OPPOSITE, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1, window3, window2}));

    // Repeat again
    root.restackRequest(window2->window(), NET::FromApplication, window3->window(), XCB_STACK_MODE_OPPOSITE, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1, window2, window3}));
}

void X11WindowTest::testStackOppositeFromTool()
{
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    NETRootInfo root(c.get(), NET::Properties());

    X11Window *window1 = createWindow(c.get(), QRect(100, 0, 100, 100), [&](xcb_window_t windowId) {
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
    });
    X11Window *window2 = createWindow(c.get(), QRect(200, 0, 100, 100), [&](xcb_window_t windowId) {
        xcb_window_t leaderId = window1->window();
        xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &leaderId);
    });
    X11Window *window3 = createWindow(c.get(), QRect(300, 0, 100, 100));

    // window2 is above window1, so it will be lowered
    root.restackRequest(window2->window(), NET::FromTool, window1->window(), XCB_STACK_MODE_OPPOSITE, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window2, window1, window3}));

    // Repeat again
    root.restackRequest(window2->window(), NET::FromTool, window1->window(), XCB_STACK_MODE_OPPOSITE, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1, window2, window3}));

    // Other window
    root.restackRequest(window2->window(), NET::FromTool, window3->window(), XCB_STACK_MODE_OPPOSITE, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1, window3, window2}));

    // Repeat again
    root.restackRequest(window2->window(), NET::FromTool, window3->window(), XCB_STACK_MODE_OPPOSITE, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1, window2, window3}));
}

void X11WindowTest::testStackOppositeNoSibling()
{
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    NETRootInfo root(c.get(), NET::Properties());

    X11Window *window1 = createWindow(c.get(), QRect(100, 0, 100, 100));
    X11Window *window2 = createWindow(c.get(), QRect(200, 0, 100, 100));
    X11Window *window3 = createWindow(c.get(), QRect(300, 0, 100, 100));

    root.restackRequest(window2->window(), NET::FromTool, XCB_WINDOW_NONE, XCB_STACK_MODE_OPPOSITE, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1, window3, window2}));

    root.restackRequest(window2->window(), NET::FromTool, XCB_WINDOW_NONE, XCB_STACK_MODE_OPPOSITE, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window2, window1, window3}));

    root.restackRequest(window2->window(), NET::FromTool, XCB_WINDOW_NONE, XCB_STACK_MODE_OPPOSITE, XCB_CURRENT_TIME);
    xcb_flush(c.get());
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1, window3, window2}));
}

void X11WindowTest::testOverrideRedirectReparent()
{
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));

    X11Window *parent = createWindow(c.get(), QRect(0, 0, 200, 200));
    X11Window *child = createWindow(c.get(), QRect(0, 0, 100, 100), [&](xcb_window_t windowId) {
        quint32 value = 1;
        xcb_change_window_attributes(c.get(), windowId, XCB_CW_OVERRIDE_REDIRECT, &value);
    });

    xcb_reparent_window(c.get(), child->window(), parent->window(), 0, 0);
    xcb_flush(c.get());
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(!windowAddedSpy.wait(10));
}

void X11WindowTest::testOverrideRedirectStackingAbove()
{
    Test::XcbConnectionPtr c = Test::createX11Connection();
    xcb_window_t windowAId = 0;
    X11Window *windowA = createWindow(c.get(), QRect(0, 0, 100, 100), [&](xcb_window_t windowId) {
        windowAId = windowId;
        quint32 value = 1;
        xcb_change_window_attributes(c.get(), windowId, XCB_CW_OVERRIDE_REDIRECT, &value);
    });
    X11Window *windowB = createWindow(c.get(), QRect(0, 0, 100, 100), [&](xcb_window_t windowId) {
        quint32 value = 1;
        xcb_change_window_attributes(c.get(), windowId, XCB_CW_OVERRIDE_REDIRECT, &value);

        // restack before showing
        uint32_t values[] = {windowAId, XCB_STACK_MODE_ABOVE};
        xcb_configure_window(c.get(), windowId,
                             XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE,
                             values);
    });
    QVERIFY(workspace()->windows().count() == 2);
    QVERIFY(workspace()->stackingOrder().indexOf(windowA) == 0);
    QVERIFY(workspace()->stackingOrder().indexOf(windowB) == 1);
}

void X11WindowTest::testOverrideRedirectStackingBelow()
{
    Test::XcbConnectionPtr c = Test::createX11Connection();
    xcb_window_t windowAId = 0;
    X11Window *windowA = createWindow(c.get(), QRect(0, 0, 100, 100), [&](xcb_window_t windowId) {
        windowAId = windowId;
        quint32 value = 1;
        xcb_change_window_attributes(c.get(), windowId, XCB_CW_OVERRIDE_REDIRECT, &value);
    });
    X11Window *windowB = createWindow(c.get(), QRect(0, 0, 100, 100), [&](xcb_window_t windowId) {
        quint32 value = 1;
        xcb_change_window_attributes(c.get(), windowId, XCB_CW_OVERRIDE_REDIRECT, &value);

        // restack before showing
        uint32_t values[] = {windowAId, XCB_STACK_MODE_BELOW};
        xcb_configure_window(c.get(), windowId,
                             XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE,
                             values);
    });
    QVERIFY(workspace()->windows().count() == 2);
    QVERIFY(workspace()->stackingOrder().indexOf(windowA) == 1);
    QVERIFY(workspace()->stackingOrder().indexOf(windowB) == 0);
}

WAYLANDTEST_MAIN(X11WindowTest)
#include "x11_window_test.moc"
