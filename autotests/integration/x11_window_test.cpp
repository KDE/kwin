/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "atoms.h"
#include "composite.h"
#include "cursor.h"
#include "deleted.h"
#include "effectloader.h"
#include "effects.h"
#include "platform.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"

#include <KWayland/Client/surface.h>

#include <netwm.h>
#include <xcb/xcb_icccm.h>

using namespace KWin;
using namespace KWayland::Client;
static const QString s_socketName = QStringLiteral("wayland_test_x11_window-0");

class X11WindowTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testMinimumSize();
    void testMaximumSize();
    void testResizeIncrements();
    void testResizeIncrementsNoBaseSize();
    void testTrimCaption_data();
    void testTrimCaption();
    void testFullscreenLayerWithActiveWaylandWindow();
    void testFocusInWithWaylandLastActiveWindow();
    void testX11WindowId();
    void testCaptionChanges();
    void testCaptionWmName();
    void testCaptionMultipleWindows();
    void testFullscreenWindowGroups();
    void testActivateFocusedWindow();
    void testReentrantMoveResize();
};

void X11WindowTest::initTestCase()
{
    qRegisterMetaType<KWin::Deleted *>();
    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName));
    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QVERIFY(KWin::Compositor::self());
    Test::initWaylandWorkspace();
}

void X11WindowTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void X11WindowTest::cleanup()
{
    Test::destroyWaylandConnection();
}

struct XcbConnectionDeleter
{
    static inline void cleanup(xcb_connection_t *pointer)
    {
        xcb_disconnect(pointer);
    }
};

void X11WindowTest::testMinimumSize()
{
    // This test verifies that the minimum size constraint is correctly applied.

    // Create an xcb window.
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
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
    xcb_icccm_set_wm_normal_hints(c.data(), windowId, &hints);
    xcb_map_window(c.data(), windowId);
    xcb_flush(c.data());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.last().first().value<X11Window *>();
    QVERIFY(window);
    QVERIFY(window->isDecorated());

    QSignalSpy clientStartMoveResizedSpy(window, &Window::clientStartUserMovedResized);
    QSignalSpy clientStepUserMovedResizedSpy(window, &Window::clientStepUserMovedResized);
    QSignalSpy clientFinishUserMovedResizedSpy(window, &Window::clientFinishUserMovedResized);
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);

    // Begin resize.
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!window->isInteractiveResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(window->isInteractiveResize());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();

    window->keyPressEvent(Qt::Key_Left);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(-8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 0);
    QVERIFY(!frameGeometryChangedSpy.wait(1000));
    QCOMPARE(window->clientSize().width(), 100);

    window->keyPressEvent(Qt::Key_Right);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos);
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 0);
    QVERIFY(!frameGeometryChangedSpy.wait(1000));
    QCOMPARE(window->clientSize().width(), 100);

    window->keyPressEvent(Qt::Key_Right);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->clientSize().width(), 108);

    window->keyPressEvent(Qt::Key_Up);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, -8));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QVERIFY(!frameGeometryChangedSpy.wait(1000));
    QCOMPARE(window->clientSize().height(), 200);

    window->keyPressEvent(Qt::Key_Down);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QVERIFY(!frameGeometryChangedSpy.wait(1000));
    QCOMPARE(window->clientSize().height(), 200);

    window->keyPressEvent(Qt::Key_Down);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 8));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 2);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->clientSize().height(), 208);

    // Finish the resize operation.
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 0);
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!window->isInteractiveResize());

    // Destroy the window.
    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    xcb_unmap_window(c.data(), windowId);
    xcb_destroy_window(c.data(), windowId);
    xcb_flush(c.data());
    QVERIFY(windowClosedSpy.wait());
    c.reset();
}

void X11WindowTest::testMaximumSize()
{
    // This test verifies that the maximum size constraint is correctly applied.

    // Create an xcb window.
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
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
    xcb_icccm_set_wm_normal_hints(c.data(), windowId, &hints);
    xcb_map_window(c.data(), windowId);
    xcb_flush(c.data());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.last().first().value<X11Window *>();
    QVERIFY(window);
    QVERIFY(window->isDecorated());

    QSignalSpy clientStartMoveResizedSpy(window, &Window::clientStartUserMovedResized);
    QSignalSpy clientStepUserMovedResizedSpy(window, &Window::clientStepUserMovedResized);
    QSignalSpy clientFinishUserMovedResizedSpy(window, &Window::clientFinishUserMovedResized);
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);

    // Begin resize.
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!window->isInteractiveResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(window->isInteractiveResize());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();

    window->keyPressEvent(Qt::Key_Right);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 0);
    QVERIFY(!frameGeometryChangedSpy.wait(1000));
    QCOMPARE(window->clientSize().width(), 100);

    window->keyPressEvent(Qt::Key_Left);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos);
    QVERIFY(!clientStepUserMovedResizedSpy.wait(1000));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 0);
    QCOMPARE(window->clientSize().width(), 100);

    window->keyPressEvent(Qt::Key_Left);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(-8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->clientSize().width(), 92);

    window->keyPressEvent(Qt::Key_Down);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(-8, 8));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QVERIFY(!frameGeometryChangedSpy.wait(1000));
    QCOMPARE(window->clientSize().height(), 200);

    window->keyPressEvent(Qt::Key_Up);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(-8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QVERIFY(!frameGeometryChangedSpy.wait(1000));
    QCOMPARE(window->clientSize().height(), 200);

    window->keyPressEvent(Qt::Key_Up);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(-8, -8));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 2);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->clientSize().height(), 192);

    // Finish the resize operation.
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 0);
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!window->isInteractiveResize());

    // Destroy the window.
    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    xcb_unmap_window(c.data(), windowId);
    xcb_destroy_window(c.data(), windowId);
    xcb_flush(c.data());
    QVERIFY(windowClosedSpy.wait());
    c.reset();
}

void X11WindowTest::testResizeIncrements()
{
    // This test verifies that the resize increments constraint is correctly applied.

    // Create an xcb window.
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_size_hints_set_base_size(&hints, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_size_hints_set_resize_inc(&hints, 3, 5);
    xcb_icccm_set_wm_normal_hints(c.data(), windowId, &hints);
    xcb_map_window(c.data(), windowId);
    xcb_flush(c.data());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.last().first().value<X11Window *>();
    QVERIFY(window);
    QVERIFY(window->isDecorated());

    QSignalSpy clientStartMoveResizedSpy(window, &Window::clientStartUserMovedResized);
    QSignalSpy clientStepUserMovedResizedSpy(window, &Window::clientStepUserMovedResized);
    QSignalSpy clientFinishUserMovedResizedSpy(window, &Window::clientFinishUserMovedResized);
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);

    // Begin resize.
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!window->isInteractiveResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(window->isInteractiveResize());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();

    window->keyPressEvent(Qt::Key_Right);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->clientSize(), QSize(106, 200));

    window->keyPressEvent(Qt::Key_Down);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 8));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 2);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->clientSize(), QSize(106, 205));

    // Finish the resize operation.
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 0);
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!window->isInteractiveResize());

    // Destroy the window.
    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    xcb_unmap_window(c.data(), windowId);
    xcb_destroy_window(c.data(), windowId);
    xcb_flush(c.data());
    QVERIFY(windowClosedSpy.wait());
    c.reset();
}

void X11WindowTest::testResizeIncrementsNoBaseSize()
{
    // Create an xcb window.
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
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
    xcb_icccm_size_hints_set_resize_inc(&hints, 3, 5);
    xcb_icccm_set_wm_normal_hints(c.data(), windowId, &hints);
    xcb_map_window(c.data(), windowId);
    xcb_flush(c.data());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.last().first().value<X11Window *>();
    QVERIFY(window);
    QVERIFY(window->isDecorated());

    QSignalSpy clientStartMoveResizedSpy(window, &Window::clientStartUserMovedResized);
    QSignalSpy clientStepUserMovedResizedSpy(window, &Window::clientStepUserMovedResized);
    QSignalSpy clientFinishUserMovedResizedSpy(window, &Window::clientFinishUserMovedResized);
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);

    // Begin resize.
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!window->isInteractiveResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(window->isInteractiveResize());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();

    window->keyPressEvent(Qt::Key_Right);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->clientSize(), QSize(106, 200));

    window->keyPressEvent(Qt::Key_Down);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 8));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 2);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->clientSize(), QSize(106, 205));

    // Finish the resize operation.
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 0);
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!window->isInteractiveResize());

    // Destroy the window.
    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    xcb_unmap_window(c.data(), windowId);
    xcb_destroy_window(c.data(), windowId);
    xcb_flush(c.data());
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
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.data(), windowId, &hints);
    NETWinInfo winInfo(c.data(), windowId, rootWindow(), NET::Properties(), NET::Properties2());
    QFETCH(QByteArray, originalTitle);
    winInfo.setName(originalTitle);
    xcb_map_window(c.data(), windowId);
    xcb_flush(c.data());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QFETCH(QByteArray, expectedTitle);
    QCOMPARE(window->caption(), QString::fromUtf8(expectedTitle));

    // and destroy the window again
    xcb_unmap_window(c.data(), windowId);
    xcb_flush(c.data());

    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    QVERIFY(windowClosedSpy.wait());
    xcb_destroy_window(c.data(), windowId);
    c.reset();
}

void X11WindowTest::testFullscreenLayerWithActiveWaylandWindow()
{
    // this test verifies that an X11 fullscreen window does not stay in the active layer
    // when a Wayland window is active, see BUG: 375759
    QCOMPARE(screens()->count(), 1);

    // first create an X11 window
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.data(), windowId, &hints);
    xcb_map_window(c.data(), windowId);
    xcb_flush(c.data());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.isValid());
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
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    auto waylandWindow = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
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
    NETWinInfo info(c.data(), windowId, kwinApp()->x11RootWindow(), NET::Properties(), NET::Properties2());
    info.setState(NET::FullScreen, NET::FullScreen);
    NETRootInfo rootInfo(c.data(), NET::Properties());
    rootInfo.setActiveWindow(windowId, NET::FromApplication, XCB_CURRENT_TIME, XCB_WINDOW_NONE);
    xcb_flush(c.data());
    QTRY_VERIFY(window->isFullScreen());
    QCOMPARE(workspace()->stackingOrder().last(), window);
    QCOMPARE(workspace()->stackingOrder().last(), window);

    // activate wayland window again
    workspace()->activateWindow(waylandWindow);
    QTRY_VERIFY(waylandWindow->isActive());
    QCOMPARE(workspace()->stackingOrder().last(), waylandWindow);
    QCOMPARE(workspace()->stackingOrder().last(), waylandWindow);
    QCOMPARE(window->layer(), NormalLayer);

    // close the window
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(waylandWindow));
    QTRY_VERIFY(window->isActive());
    QCOMPARE(window->layer(), ActiveLayer);

    // and destroy the window again
    xcb_unmap_window(c.data(), windowId);
    xcb_flush(c.data());
}

void X11WindowTest::testFocusInWithWaylandLastActiveWindow()
{
    // this test verifies that Workspace::allowWindowActivation does not crash if last client was a Wayland client

    // create an X11 window
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.data(), windowId, &hints);
    xcb_map_window(c.data(), windowId);
    xcb_flush(c.data());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QVERIFY(window->isActive());

    // create Wayland window
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    auto waylandWindow = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(waylandWindow);
    QVERIFY(waylandWindow->isActive());
    // activate no window
    workspace()->setActiveWindow(nullptr);
    QVERIFY(!waylandWindow->isActive());
    QVERIFY(!workspace()->activeWindow());
    // and close Wayland window again
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(waylandWindow));

    // and try to activate the x11 window through X11 api
    const auto cookie = xcb_set_input_focus_checked(c.data(), XCB_INPUT_FOCUS_NONE, windowId, XCB_CURRENT_TIME);
    auto error = xcb_request_check(c.data(), cookie);
    QVERIFY(!error);
    // this accesses m_lastActiveWindow on trying to activate
    QTRY_VERIFY(window->isActive());

    // and destroy the window again
    xcb_unmap_window(c.data(), windowId);
    xcb_flush(c.data());
}

void X11WindowTest::testX11WindowId()
{
    // create an X11 window
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.data(), windowId, &hints);
    xcb_map_window(c.data(), windowId);
    xcb_flush(c.data());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QVERIFY(window->isActive());
    QCOMPARE(window->window(), windowId);
    QCOMPARE(window->internalId().isNull(), false);
    const auto uuid = window->internalId();
    QUuid deletedUuid;
    QCOMPARE(deletedUuid.isNull(), true);

    connect(window, &X11Window::windowClosed, this, [&deletedUuid](Window *, Deleted *d) {
        deletedUuid = d->internalId();
    });

    NETRootInfo rootInfo(c.data(), NET::WMAllProperties);
    QCOMPARE(rootInfo.activeWindow(), window->window());

    // activate a wayland window
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    auto waylandWindow = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(waylandWindow);
    QVERIFY(waylandWindow->isActive());
    xcb_flush(kwinApp()->x11Connection());

    NETRootInfo rootInfo2(c.data(), NET::WMAllProperties);
    QCOMPARE(rootInfo2.activeWindow(), 0u);

    // back to X11 window
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(waylandWindow));

    QTRY_VERIFY(window->isActive());
    NETRootInfo rootInfo3(c.data(), NET::WMAllProperties);
    QCOMPARE(rootInfo3.activeWindow(), window->window());

    // and destroy the window again
    xcb_unmap_window(c.data(), windowId);
    xcb_flush(c.data());
    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    QVERIFY(windowClosedSpy.wait());

    QCOMPARE(deletedUuid.isNull(), false);
    QCOMPARE(deletedUuid, uuid);
}

void X11WindowTest::testCaptionChanges()
{
    // verifies that caption is updated correctly when the X11 window updates it
    // BUG: 383444
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.data(), windowId, &hints);
    NETWinInfo info(c.data(), windowId, kwinApp()->x11RootWindow(), NET::Properties(), NET::Properties2());
    info.setName("foo");
    xcb_map_window(c.data(), windowId);
    xcb_flush(c.data());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QCOMPARE(window->caption(), QStringLiteral("foo"));

    QSignalSpy captionChangedSpy(window, &X11Window::captionChanged);
    QVERIFY(captionChangedSpy.isValid());
    info.setName("bar");
    xcb_flush(c.data());
    QVERIFY(captionChangedSpy.wait());
    QCOMPARE(window->caption(), QStringLiteral("bar"));

    // and destroy the window again
    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    xcb_unmap_window(c.data(), windowId);
    xcb_flush(c.data());
    QVERIFY(windowClosedSpy.wait());
    xcb_destroy_window(c.data(), windowId);
    c.reset();
}

void X11WindowTest::testCaptionWmName()
{
    // this test verifies that a caption set through WM_NAME is read correctly

    // open glxgears as that one only uses WM_NAME
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowAddedSpy.isValid());

    QProcess glxgears;
    glxgears.setProgram(QStringLiteral("glxgears"));
    glxgears.start();
    QVERIFY(glxgears.waitForStarted());

    QVERIFY(windowAddedSpy.wait());
    QCOMPARE(windowAddedSpy.count(), 1);
    QCOMPARE(workspace()->clientList().count(), 1);
    X11Window *glxgearsWindow = workspace()->clientList().first();
    QCOMPARE(glxgearsWindow->caption(), QStringLiteral("glxgears"));

    glxgears.terminate();
    QVERIFY(glxgears.waitForFinished());
}

void X11WindowTest::testCaptionMultipleWindows()
{
    // BUG 384760
    // create first window
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.data(), windowId, &hints);
    NETWinInfo info(c.data(), windowId, kwinApp()->x11RootWindow(), NET::Properties(), NET::Properties2());
    info.setName("foo");
    xcb_map_window(c.data(), windowId);
    xcb_flush(c.data());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QCOMPARE(window->caption(), QStringLiteral("foo"));

    // create second window with same caption
    xcb_window_t w2 = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, w2, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_icccm_set_wm_normal_hints(c.data(), w2, &hints);
    NETWinInfo info2(c.data(), w2, kwinApp()->x11RootWindow(), NET::Properties(), NET::Properties2());
    info2.setName("foo");
    info2.setIconName("foo");
    xcb_map_window(c.data(), w2);
    xcb_flush(c.data());

    windowCreatedSpy.clear();
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window2 = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window2);
    QCOMPARE(window2->window(), w2);
    QCOMPARE(window2->caption(), QStringLiteral("foo <2>\u200E"));
    NETWinInfo info3(kwinApp()->x11Connection(), w2, kwinApp()->x11RootWindow(), NET::WMVisibleName | NET::WMVisibleIconName, NET::Properties2());
    QCOMPARE(QByteArray(info3.visibleName()), QByteArrayLiteral("foo <2>\u200E"));
    QCOMPARE(QByteArray(info3.visibleIconName()), QByteArrayLiteral("foo <2>\u200E"));

    QSignalSpy captionChangedSpy(window2, &X11Window::captionChanged);
    QVERIFY(captionChangedSpy.isValid());

    NETWinInfo info4(c.data(), w2, kwinApp()->x11RootWindow(), NET::Properties(), NET::Properties2());
    info4.setName("foobar");
    info4.setIconName("foobar");
    xcb_map_window(c.data(), w2);
    xcb_flush(c.data());

    QVERIFY(captionChangedSpy.wait());
    QCOMPARE(window2->caption(), QStringLiteral("foobar"));
    NETWinInfo info5(kwinApp()->x11Connection(), w2, kwinApp()->x11RootWindow(), NET::WMVisibleName | NET::WMVisibleIconName, NET::Properties2());
    QCOMPARE(QByteArray(info5.visibleName()), QByteArray());
    QTRY_COMPARE(QByteArray(info5.visibleIconName()), QByteArray());
}

void X11WindowTest::testFullscreenWindowGroups()
{
    // this test creates an X11 window and puts it to full screen
    // then a second window is created which is in the same window group
    // BUG: 388310

    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.data(), windowId, &hints);
    xcb_change_property(c.data(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
    xcb_map_window(c.data(), windowId);
    xcb_flush(c.data());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.isValid());
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
    xcb_window_t w2 = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, w2, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints2;
    memset(&hints2, 0, sizeof(hints2));
    xcb_icccm_size_hints_set_position(&hints2, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints2, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.data(), w2, &hints2);
    xcb_change_property(c.data(), XCB_PROP_MODE_REPLACE, w2, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
    xcb_map_window(c.data(), w2);
    xcb_flush(c.data());

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

    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> connection(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(connection.data()));

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.isValid());

    const QRect windowGeometry(0, 0, 100, 200);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());

    // Create the first test window.
    const xcb_window_t windowId1 = xcb_generate_id(connection.data());
    xcb_create_window(connection.data(), XCB_COPY_FROM_PARENT, windowId1, rootWindow(),
                      windowGeometry.x(), windowGeometry.y(),
                      windowGeometry.width(), windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_icccm_set_wm_normal_hints(connection.data(), windowId1, &hints);
    xcb_change_property(connection.data(), XCB_PROP_MODE_REPLACE, windowId1,
                        atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId1);
    xcb_map_window(connection.data(), windowId1);
    xcb_flush(connection.data());
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window1 = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window1);
    QCOMPARE(window1->window(), windowId1);
    QCOMPARE(window1->isActive(), true);

    // Create the second test window.
    const xcb_window_t windowId2 = xcb_generate_id(connection.data());
    xcb_create_window(connection.data(), XCB_COPY_FROM_PARENT, windowId2, rootWindow(),
                      windowGeometry.x(), windowGeometry.y(),
                      windowGeometry.width(), windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_icccm_set_wm_normal_hints(connection.data(), windowId2, &hints);
    xcb_change_property(connection.data(), XCB_PROP_MODE_REPLACE, windowId2,
                        atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId2);
    xcb_map_window(connection.data(), windowId2);
    xcb_flush(connection.data());
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window2 = windowCreatedSpy.last().first().value<X11Window *>();
    QVERIFY(window2);
    QCOMPARE(window2->window(), windowId2);
    QCOMPARE(window2->isActive(), true);

    // When the second test window is destroyed, the window manager will attempt to activate the
    // next window in the focus chain, which is the first window.
    xcb_set_input_focus(connection.data(), XCB_INPUT_FOCUS_POINTER_ROOT, windowId1, XCB_CURRENT_TIME);
    xcb_destroy_window(connection.data(), windowId2);
    xcb_flush(connection.data());
    QVERIFY(Test::waitForWindowDestroyed(window2));
    QVERIFY(window1->isActive());

    // Destroy the first test window.
    xcb_destroy_window(connection.data(), windowId1);
    xcb_flush(connection.data());
    QVERIFY(Test::waitForWindowDestroyed(window1));
}

void X11WindowTest::testReentrantMoveResize()
{
    // This test verifies that calling moveResize() from a slot connected directly
    // to the frameGeometryChanged() signal won't cause an infinite recursion.

    // Create a test window.
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.data(), windowId, &hints);
    xcb_change_property(c.data(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
    xcb_map_window(c.data(), windowId);
    xcb_flush(c.data());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->pos(), QPoint(0, 0));

    // Let's pretend that there is a script that really wants the window to be at (100, 100).
    connect(window, &Window::frameGeometryChanged, this, [window]() {
        window->moveResize(QRect(QPoint(100, 100), window->size()));
    });

    // Trigger the lambda above.
    window->move(QPoint(40, 50));

    // Eventually, the window will end up at (100, 100).
    QCOMPARE(window->pos(), QPoint(100, 100));

    // Destroy the test window.
    xcb_destroy_window(c.data(), windowId);
    xcb_flush(c.data());
    QVERIFY(Test::waitForWindowDestroyed(window));
}

WAYLANDTEST_MAIN(X11WindowTest)
#include "x11_window_test.moc"
