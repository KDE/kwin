/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "cursor.h"
#include "deleted.h"
#include "utils/xcbutils.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include "x11window.h"
#include <kwineffects.h>

#include <netwm.h>
#include <xcb/xcb_icccm.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_x11_desktop_window-0");

class X11DesktopWindowTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testDesktopWindow();

private:
};

void X11DesktopWindowTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWin::Deleted *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
}

void X11DesktopWindowTest::init()
{
    workspace()->setActiveOutput(QPoint(640, 512));
    Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void X11DesktopWindowTest::cleanup()
{
}

struct XcbConnectionDeleter
{
    void operator()(xcb_connection_t *pointer)
    {
        xcb_disconnect(pointer);
    }
};

void X11DesktopWindowTest::testDesktopWindow()
{
    // this test creates a desktop window with an RGBA visual and verifies that it's only considered
    // as an RGB (opaque) window in KWin

    // create an xcb window
    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.get()));

    xcb_window_t windowId = xcb_generate_id(c.get());
    const QRect windowGeometry(0, 0, 1280, 1024);

    // helper to find the visual
    auto findDepth = [&c]() -> xcb_visualid_t {
        // find a visual with 32 depth
        const xcb_setup_t *setup = xcb_get_setup(c.get());

        for (auto screen = xcb_setup_roots_iterator(setup); screen.rem; xcb_screen_next(&screen)) {
            for (auto depth = xcb_screen_allowed_depths_iterator(screen.data); depth.rem; xcb_depth_next(&depth)) {
                if (depth.data->depth != 32) {
                    continue;
                }
                const int len = xcb_depth_visuals_length(depth.data);
                const xcb_visualtype_t *visuals = xcb_depth_visuals(depth.data);

                for (int i = 0; i < len; i++) {
                    return visuals[0].visual_id;
                }
            }
        }
        return 0;
    };
    auto visualId = findDepth();
    auto colormapId = xcb_generate_id(c.get());
    auto cmCookie = xcb_create_colormap_checked(c.get(), XCB_COLORMAP_ALLOC_NONE, colormapId, rootWindow(), visualId);
    QVERIFY(!xcb_request_check(c.get(), cmCookie));

    const uint32_t values[] = {XCB_PIXMAP_NONE, Xcb::defaultScreen()->black_pixel, colormapId};
    auto cookie = xcb_create_window_checked(c.get(), 32, windowId, rootWindow(),
                                            windowGeometry.x(),
                                            windowGeometry.y(),
                                            windowGeometry.width(),
                                            windowGeometry.height(),
                                            0, XCB_WINDOW_CLASS_INPUT_OUTPUT, visualId, XCB_CW_BACK_PIXMAP | XCB_CW_BORDER_PIXEL | XCB_CW_COLORMAP, values);
    QVERIFY(!xcb_request_check(c.get(), cookie));
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    NETWinInfo info(c.get(), windowId, rootWindow(), NET::WMAllProperties, NET::WM2AllProperties);
    info.setWindowType(NET::Desktop);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    // verify through a geometry request that it's depth 32
    Xcb::WindowGeometry geo(windowId);
    QCOMPARE(geo->depth, uint8_t(32));

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QVERIFY(!window->isDecorated());
    QCOMPARE(window->windowType(), NET::Desktop);
    QCOMPARE(window->frameGeometry(), windowGeometry);
    QVERIFY(window->isDesktop());
    QCOMPARE(window->depth(), 24);
    QVERIFY(!window->hasAlpha());

    // and destroy the window again
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    c.reset();

    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    QVERIFY(windowClosedSpy.wait());
}

}

WAYLANDTEST_MAIN(KWin::X11DesktopWindowTest)
#include "desktop_window_x11_test.moc"
