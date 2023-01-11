/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "atoms.h"
#include "composite.h"
#include "core/outputbackend.h"
#include "cursor.h"
#include "deleted.h"
#include "effectloader.h"
#include "effects.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"

#include <KWayland/Client/surface.h>

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

void X11WindowTest::initTestCase_data()
{
    QTest::addColumn<qreal>("scale");
    QTest::newRow("normal") << 1.0;
    QTest::newRow("scaled2x") << 2.0;
}

void X11WindowTest::initTestCase()
{
    qRegisterMetaType<KWin::Deleted *>();
    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024)));
    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QVERIFY(KWin::Compositor::self());
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
    void operator()(xcb_connection_t *pointer)
    {
        xcb_disconnect(pointer);
    }
};

void X11WindowTest::testMinimumSize()
{
    // This test verifies that the minimum size constraint is correctly applied.

    QFETCH_GLOBAL(qreal, scale);
    kwinApp()->setXwaylandScale(scale);

    // Create an xcb window.
    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
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
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    QCOMPARE(window->clientSize().width(), 100 / scale);

    window->keyPressEvent(Qt::Key_Right);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos);
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 0);
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    QCOMPARE(window->clientSize().width(), 100 / scale);

    window->keyPressEvent(Qt::Key_Right);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QVERIFY(frameGeometryChangedSpy.wait());
    // whilst X11 window size goes through scale, the increment is a logical value kwin side
    QCOMPARE(window->clientSize().width(), 100 / scale + 8);

    window->keyPressEvent(Qt::Key_Up);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, -8));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    QCOMPARE(window->clientSize().height(), 200 / scale);

    window->keyPressEvent(Qt::Key_Down);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    QCOMPARE(window->clientSize().height(), 200 / scale);

    window->keyPressEvent(Qt::Key_Down);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 8));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 2);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->clientSize().height(), 200 / scale + 8);

    // Finish the resize operation.
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 0);
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!window->isInteractiveResize());

    // Destroy the window.
    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    QVERIFY(windowClosedSpy.wait());
    c.reset();
}

void X11WindowTest::testMaximumSize()
{
    // This test verifies that the maximum size constraint is correctly applied.
    QFETCH_GLOBAL(qreal, scale);
    kwinApp()->setXwaylandScale(scale);

    // Create an xcb window.
    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
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
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    QCOMPARE(window->clientSize().width(), 100 / scale);

    window->keyPressEvent(Qt::Key_Left);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos);
    QVERIFY(!clientStepUserMovedResizedSpy.wait(10));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 0);
    QCOMPARE(window->clientSize().width(), 100 / scale);

    window->keyPressEvent(Qt::Key_Left);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(-8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->clientSize().width(), 100 / scale - 8);

    window->keyPressEvent(Qt::Key_Down);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(-8, 8));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    QCOMPARE(window->clientSize().height(), 200 / scale);

    window->keyPressEvent(Qt::Key_Up);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(-8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    QCOMPARE(window->clientSize().height(), 200 / scale);

    window->keyPressEvent(Qt::Key_Up);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(-8, -8));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 2);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->clientSize().height(), 200 / scale - 8);

    // Finish the resize operation.
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 0);
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!window->isInteractiveResize());

    // Destroy the window.
    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    QVERIFY(windowClosedSpy.wait());
    c.reset();
}

void X11WindowTest::testResizeIncrements()
{
    // This test verifies that the resize increments constraint is correctly applied.
    QFETCH_GLOBAL(qreal, scale);
    kwinApp()->setXwaylandScale(scale);

    // Create an xcb window.
    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
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
    xcb_icccm_size_hints_set_base_size(&hints, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_size_hints_set_resize_inc(&hints, 3, 5);
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
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

    //  100 + 8 logical pixels, rounded to resize increments. This will differ on scale
    const qreal horizontalResizeInc = 3 / scale;
    const qreal verticalResizeInc = 5 / scale;
    const qreal expectedHorizontalResizeInc = std::floor(8. / horizontalResizeInc) * horizontalResizeInc;
    const qreal expectedVerticalResizeInc = std::floor(8. / verticalResizeInc) * verticalResizeInc;

    QCOMPARE(window->clientSize(), QSizeF(100, 200) / scale + QSizeF(expectedHorizontalResizeInc, 0));

    window->keyPressEvent(Qt::Key_Down);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 8));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 2);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->clientSize(), QSize(100, 200) / scale + QSizeF(expectedHorizontalResizeInc, expectedVerticalResizeInc));

    // Finish the resize operation.
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 0);
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!window->isInteractiveResize());

    // Destroy the window.
    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    QVERIFY(windowClosedSpy.wait());
    c.reset();
}

void X11WindowTest::testResizeIncrementsNoBaseSize()
{
    QFETCH_GLOBAL(qreal, scale);
    kwinApp()->setXwaylandScale(scale);

    // Create an xcb window.
    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
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
    xcb_icccm_size_hints_set_resize_inc(&hints, 3, 5);
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
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

    //  100 + 8 pixels, rounded to resize increments. This will differ on scale
    const qreal horizontalResizeInc = 3 / scale;
    const qreal verticalResizeInc = 5 / scale;
    const qreal expectedHorizontalResizeInc = std::floor(8. / horizontalResizeInc) * horizontalResizeInc;
    const qreal expectedVerticalResizeInc = std::floor(8. / verticalResizeInc) * verticalResizeInc;

    QCOMPARE(window->clientSize(), QSizeF(100, 200) / scale + QSizeF(expectedHorizontalResizeInc, 0));

    window->keyPressEvent(Qt::Key_Down);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 8));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 2);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->clientSize(), QSizeF(100, 200) / scale + QSizeF(expectedHorizontalResizeInc, expectedVerticalResizeInc));

    // Finish the resize operation.
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 0);
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!window->isInteractiveResize());

    // Destroy the window.
    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    QVERIFY(windowClosedSpy.wait());
    c.reset();
}

void X11WindowTest::testTrimCaption_data()
{
    QFETCH_GLOBAL(qreal, scale);
    kwinApp()->setXwaylandScale(scale);

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
    QFETCH_GLOBAL(qreal, scale);
    kwinApp()->setXwaylandScale(scale);

    // this test verifies that caption is properly trimmed

    // create an xcb window
    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
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

    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    QVERIFY(windowClosedSpy.wait());
    xcb_destroy_window(c.get(), windowId);
    c.reset();
}

void X11WindowTest::testFullscreenLayerWithActiveWaylandWindow()
{
    QFETCH_GLOBAL(qreal, scale);
    kwinApp()->setXwaylandScale(scale);

    // this test verifies that an X11 fullscreen window does not stay in the active layer
    // when a Wayland window is active, see BUG: 375759
    QCOMPARE(workspace()->outputs().count(), 1);

    // first create an X11 window
    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
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
    NETWinInfo info(c.get(), windowId, kwinApp()->x11RootWindow(), NET::Properties(), NET::Properties2());
    info.setState(NET::FullScreen, NET::FullScreen);
    NETRootInfo rootInfo(c.get(), NET::Properties());
    rootInfo.setActiveWindow(windowId, NET::FromApplication, XCB_CURRENT_TIME, XCB_WINDOW_NONE);
    xcb_flush(c.get());
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
    xcb_unmap_window(c.get(), windowId);
    xcb_flush(c.get());
}

void X11WindowTest::testFocusInWithWaylandLastActiveWindow()
{
    // this test verifies that Workspace::allowWindowActivation does not crash if last client was a Wayland client
    QFETCH_GLOBAL(qreal, scale);
    kwinApp()->setXwaylandScale(scale);

    // create an X11 window
    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
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
    QVERIFY(Test::waitForWindowDestroyed(waylandWindow));

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

void X11WindowTest::testX11WindowId()
{
    QFETCH_GLOBAL(qreal, scale);
    kwinApp()->setXwaylandScale(scale);

    // create an X11 window
    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
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
    QCOMPARE(window->window(), windowId);
    QCOMPARE(window->internalId().isNull(), false);
    const auto uuid = window->internalId();
    QUuid deletedUuid;
    QCOMPARE(deletedUuid.isNull(), true);

    connect(window, &X11Window::windowClosed, this, [&deletedUuid](Window *, Deleted *d) {
        deletedUuid = d->internalId();
    });

    NETRootInfo rootInfo(c.get(), NET::WMAllProperties);
    QCOMPARE(rootInfo.activeWindow(), window->window());

    // activate a wayland window
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(waylandWindow);
    QVERIFY(waylandWindow->isActive());
    xcb_flush(kwinApp()->x11Connection());

    NETRootInfo rootInfo2(c.get(), NET::WMAllProperties);
    QCOMPARE(rootInfo2.activeWindow(), 0u);

    // back to X11 window
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(waylandWindow));

    QTRY_VERIFY(window->isActive());
    NETRootInfo rootInfo3(c.get(), NET::WMAllProperties);
    QCOMPARE(rootInfo3.activeWindow(), window->window());

    // and destroy the window again
    xcb_unmap_window(c.get(), windowId);
    xcb_flush(c.get());
    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    QVERIFY(windowClosedSpy.wait());

    QCOMPARE(deletedUuid.isNull(), false);
    QCOMPARE(deletedUuid, uuid);
}

void X11WindowTest::testCaptionChanges()
{
    QFETCH_GLOBAL(qreal, scale);
    kwinApp()->setXwaylandScale(scale);

    // verifies that caption is updated correctly when the X11 window updates it
    // BUG: 383444
    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
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
    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    xcb_unmap_window(c.get(), windowId);
    xcb_flush(c.get());
    QVERIFY(windowClosedSpy.wait());
    xcb_destroy_window(c.get(), windowId);
    c.reset();
}

void X11WindowTest::testCaptionWmName()
{
    QFETCH_GLOBAL(qreal, scale);
    kwinApp()->setXwaylandScale(scale);

    // this test verifies that a caption set through WM_NAME is read correctly

    // open glxgears as that one only uses WM_NAME
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);

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
    QFETCH_GLOBAL(qreal, scale);
    kwinApp()->setXwaylandScale(scale);

    // BUG 384760
    // create first window
    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
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

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QCOMPARE(window->caption(), QStringLiteral("foo"));

    // create second window with same caption
    xcb_window_t w2 = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, w2, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_icccm_set_wm_normal_hints(c.get(), w2, &hints);
    NETWinInfo info2(c.get(), w2, kwinApp()->x11RootWindow(), NET::Properties(), NET::Properties2());
    info2.setName("foo");
    info2.setIconName("foo");
    xcb_map_window(c.get(), w2);
    xcb_flush(c.get());

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

    NETWinInfo info4(c.get(), w2, kwinApp()->x11RootWindow(), NET::Properties(), NET::Properties2());
    info4.setName("foobar");
    info4.setIconName("foobar");
    xcb_map_window(c.get(), w2);
    xcb_flush(c.get());

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

    QFETCH_GLOBAL(qreal, scale);
    kwinApp()->setXwaylandScale(scale);

    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
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

    QFETCH_GLOBAL(qreal, scale);
    kwinApp()->setXwaylandScale(scale);

    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> connection(xcb_connect(nullptr, nullptr));
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
    QVERIFY(Test::waitForWindowDestroyed(window2));
    QVERIFY(window1->isActive());

    // Destroy the first test window.
    xcb_destroy_window(connection.get(), windowId1);
    xcb_flush(connection.get());
    QVERIFY(Test::waitForWindowDestroyed(window1));
}

void X11WindowTest::testReentrantMoveResize()
{
    // This test verifies that calling moveResize() from a slot connected directly
    // to the frameGeometryChanged() signal won't cause an infinite recursion.

    QFETCH_GLOBAL(qreal, scale);
    kwinApp()->setXwaylandScale(scale);

    // Create a test window.
    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
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
    QVERIFY(Test::waitForWindowDestroyed(window));
}

WAYLANDTEST_MAIN(X11WindowTest)
#include "x11_window_test.moc"
