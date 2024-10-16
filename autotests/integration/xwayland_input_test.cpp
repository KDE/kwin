/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "pointer_input.h"
#include "wayland/seat.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"

#include <QAbstractEventDispatcher>
#include <QSocketNotifier>

#include <netwm.h>
#include <xcb/xcb_icccm.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_xwayland_input-0");

class XWaylandInputTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void testPointerEnterLeaveSsd();
    void testPointerEventLeaveCsd();
};

void XWaylandInputTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
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
    setenv("QT_QPA_PLATFORM", "wayland", true);
}

void XWaylandInputTest::init()
{
    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));

    QVERIFY(waylandServer()->windows().isEmpty());
}

class X11EventReaderHelper : public QObject
{
    Q_OBJECT
public:
    X11EventReaderHelper(xcb_connection_t *c);

Q_SIGNALS:
    void entered(const QPoint &localPoint);
    void left(const QPoint &localPoint);

private:
    void processXcbEvents();
    xcb_connection_t *m_connection;
    QSocketNotifier *m_notifier;
};

X11EventReaderHelper::X11EventReaderHelper(xcb_connection_t *c)
    : QObject()
    , m_connection(c)
    , m_notifier(new QSocketNotifier(xcb_get_file_descriptor(m_connection), QSocketNotifier::Read, this))
{
    connect(m_notifier, &QSocketNotifier::activated, this, &X11EventReaderHelper::processXcbEvents);
    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, &X11EventReaderHelper::processXcbEvents);
    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::awake, this, &X11EventReaderHelper::processXcbEvents);
}

void X11EventReaderHelper::processXcbEvents()
{
    while (auto event = xcb_poll_for_event(m_connection)) {
        const uint8_t eventType = event->response_type & ~0x80;
        switch (eventType) {
        case XCB_ENTER_NOTIFY: {
            auto enterEvent = reinterpret_cast<xcb_enter_notify_event_t *>(event);
            Q_EMIT entered(QPoint(enterEvent->event_x, enterEvent->event_y));
            break;
        }
        case XCB_LEAVE_NOTIFY: {
            auto leaveEvent = reinterpret_cast<xcb_leave_notify_event_t *>(event);
            Q_EMIT left(QPoint(leaveEvent->event_x, leaveEvent->event_y));
            break;
        }
        }
        free(event);
    }
    xcb_flush(m_connection);
}

void XWaylandInputTest::testPointerEnterLeaveSsd()
{
    // this test simulates a pointer enter and pointer leave on a server-side decorated X11 window

    // create the test window
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    if (xcb_get_setup(c.get())->release_number < 11800000) {
        QSKIP("XWayland 1.18 required");
    }

    xcb_warp_pointer(connection(), XCB_WINDOW_NONE, kwinApp()->x11RootWindow(), 0, 0, 0, 0, 640, 512);
    xcb_flush(connection());

    X11EventReaderHelper eventReader(c.get());
    QSignalSpy enteredSpy(&eventReader, &X11EventReaderHelper::entered);
    QSignalSpy leftSpy(&eventReader, &X11EventReaderHelper::left);

    xcb_window_t windowId = xcb_generate_id(c.get());
    const QRect windowGeometry = QRect(0, 0, 100, 200);
    const uint32_t values[] = {
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW};
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, XCB_CW_EVENT_MASK, values);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    NETWinInfo info(c.get(), windowId, rootWindow(), NET::WMAllProperties, NET::WM2AllProperties);
    info.setWindowType(NET::Normal);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.last().first().value<X11Window *>();
    QVERIFY(window);
    QVERIFY(window->isDecorated());
    QVERIFY(!window->hasStrut());
    QVERIFY(!window->readyForPainting());

    QMetaObject::invokeMethod(window, "setReadyForPainting");
    QVERIFY(window->readyForPainting());
    QVERIFY(Test::waitForWaylandSurface(window));

    // move pointer into the window, should trigger an enter
    QVERIFY(!exclusiveContains(window->frameGeometry(), Cursors::self()->mouse()->pos()));
    QVERIFY(enteredSpy.isEmpty());
    input()->pointer()->warp(window->frameGeometry().center().toPoint());
    QCOMPARE(waylandServer()->seat()->focusedPointerSurface(), window->surface());
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.last().first().toPoint(), (window->frameGeometry().center() - QPointF(window->frameMargins().left(), window->frameMargins().top())).toPoint());

    // move out of window
    input()->pointer()->warp(window->frameGeometry().bottomRight() + QPointF(10, 10));
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.last().first().toPoint(), (window->frameGeometry().center() - QPointF(window->frameMargins().left(), window->frameMargins().top())).toPoint());

    // destroy window again
    QSignalSpy windowClosedSpy(window, &X11Window::closed);
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    QVERIFY(windowClosedSpy.wait());
}

void XWaylandInputTest::testPointerEventLeaveCsd()
{
    // this test simulates a pointer enter and pointer leave on a client-side decorated X11 window

    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));

    xcb_warp_pointer(connection(), XCB_WINDOW_NONE, kwinApp()->x11RootWindow(), 0, 0, 0, 0, 640, 512);
    xcb_flush(connection());

    if (xcb_get_setup(c.get())->release_number < 11800000) {
        QSKIP("XWayland 1.18 required");
    }
    if (!Xcb::Extensions::self()->isShapeAvailable()) {
        QSKIP("SHAPE extension is required");
    }

    X11EventReaderHelper eventReader(c.get());
    QSignalSpy enteredSpy(&eventReader, &X11EventReaderHelper::entered);
    QSignalSpy leftSpy(&eventReader, &X11EventReaderHelper::left);

    // Extents of the client-side drop-shadow.
    NETStrut clientFrameExtent;
    clientFrameExtent.left = 10;
    clientFrameExtent.right = 10;
    clientFrameExtent.top = 5;
    clientFrameExtent.bottom = 20;

    // Need to set the bounding shape in order to create a window without decoration.
    xcb_rectangle_t boundingRect;
    boundingRect.x = 0;
    boundingRect.y = 0;
    boundingRect.width = 100 + clientFrameExtent.left + clientFrameExtent.right;
    boundingRect.height = 200 + clientFrameExtent.top + clientFrameExtent.bottom;

    xcb_window_t windowId = xcb_generate_id(c.get());
    const uint32_t values[] = {
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW};
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      boundingRect.x, boundingRect.y, boundingRect.width, boundingRect.height,
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, XCB_CW_EVENT_MASK, values);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, boundingRect.x, boundingRect.y);
    xcb_icccm_size_hints_set_size(&hints, 1, boundingRect.width, boundingRect.height);
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    xcb_shape_rectangles(c.get(), XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING,
                         XCB_CLIP_ORDERING_UNSORTED, windowId, 0, 0, 1, &boundingRect);
    NETWinInfo info(c.get(), windowId, rootWindow(), NET::WMAllProperties, NET::WM2AllProperties);
    info.setWindowType(NET::Normal);
    info.setGtkFrameExtents(clientFrameExtent);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.last().first().value<X11Window *>();
    QVERIFY(window);
    QVERIFY(!window->isDecorated());
    QVERIFY(window->isClientSideDecorated());
    QCOMPARE(window->bufferGeometry(), QRectF(0, 0, 120, 225));
    QCOMPARE(window->frameGeometry(), QRectF(10, 5, 100, 200));

    QMetaObject::invokeMethod(window, "setReadyForPainting");
    QVERIFY(window->readyForPainting());
    QVERIFY(Test::waitForWaylandSurface(window));

    // Move pointer into the window, should trigger an enter.
    QVERIFY(!exclusiveContains(window->frameGeometry(), Cursors::self()->mouse()->pos()));
    QVERIFY(enteredSpy.isEmpty());
    input()->pointer()->warp(window->frameGeometry().center().toPoint());
    QCOMPARE(waylandServer()->seat()->focusedPointerSurface(), window->surface());
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.last().first().toPoint(), QPoint(60, 105));

    // Move out of the window, should trigger a leave.
    QVERIFY(leftSpy.isEmpty());
    input()->pointer()->warp(window->frameGeometry().bottomRight() + QPoint(100, 100));
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.last().first().toPoint(), QPoint(60, 105));

    // Destroy the window.
    QSignalSpy windowClosedSpy(window, &X11Window::closed);
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    QVERIFY(windowClosedSpy.wait());
}

}

WAYLANDTEST_MAIN(KWin::XWaylandInputTest)
#include "xwayland_input_test.moc"
