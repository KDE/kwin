/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"
#include "composite.h"
#include "main.h"
#include "platform.h"
#include "scene.h"
#include "screens.h"
#include "unmanaged.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11client.h"
#include "xwl/xwayland_interface.h"

#include <xcb/xcb_icccm.h>

namespace KWin
{

struct XcbConnectionDeleter
{
    static inline void cleanup(xcb_connection_t *pointer)
    {
        xcb_disconnect(pointer);
    }
};

static const QString s_socketName = QStringLiteral("wayland_test_kwin_xwayland_server_crash-0");

class XwaylandServerCrashTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void testCrash();
};

void XwaylandServerCrashTest::initTestCase()
{
    qRegisterMetaType<Unmanaged *>();
    qRegisterMetaType<X11Client *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup xwaylandGroup = config->group("Xwayland");
    xwaylandGroup.writeEntry(QStringLiteral("XwaylandCrashPolicy"), QStringLiteral("Stop"));
    xwaylandGroup.sync();
    kwinApp()->setConfig(config);

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void XwaylandServerCrashTest::testCrash()
{
    // This test verifies that all connected X11 clients get destroyed when Xwayland crashes.

    // Create a normal window.
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t window1 = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, window1, rootWindow(),
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
    xcb_icccm_set_wm_normal_hints(c.data(), window1, &hints);
    xcb_map_window(c.data(), window1);
    xcb_flush(c.data());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    QPointer<X11Client> client = windowCreatedSpy.last().first().value<X11Client *>();
    QVERIFY(client);
    QVERIFY(client->isDecorated());

    // Create an override-redirect window.
    xcb_window_t window2 = xcb_generate_id(c.data());
    const uint32_t values[] = { true };
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, window2, rootWindow(),
                      windowGeometry.x(), windowGeometry.y(),
                      windowGeometry.width(), windowGeometry.height(), 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
                      XCB_CW_OVERRIDE_REDIRECT, values);
    xcb_map_window(c.data(), window2);
    xcb_flush(c.data());

    QSignalSpy unmanagedAddedSpy(workspace(), &Workspace::unmanagedAdded);
    QVERIFY(unmanagedAddedSpy.isValid());
    QVERIFY(unmanagedAddedSpy.wait());
    QPointer<Unmanaged> unmanaged = unmanagedAddedSpy.last().first().value<Unmanaged *>();
    QVERIFY(unmanaged);

    // Let's pretend that the Xwayland process has crashed.
    QSignalSpy x11ConnectionChangedSpy(kwinApp(), &Application::x11ConnectionChanged);
    QVERIFY(x11ConnectionChangedSpy.isValid());
    xwayland()->process()->terminate();
    QVERIFY(x11ConnectionChangedSpy.wait());

    // When Xwayland crashes, the compositor should tear down the XCB connection and destroy
    // all connected X11 clients.
    QTRY_VERIFY(!client);
    QTRY_VERIFY(!unmanaged);
    QCOMPARE(kwinApp()->x11Connection(), nullptr);
    QCOMPARE(kwinApp()->x11DefaultScreen(), nullptr);
    QCOMPARE(kwinApp()->x11RootWindow(), XCB_WINDOW_NONE);
    QCOMPARE(kwinApp()->x11ScreenNumber(), -1);

    // Render a frame to ensure that the compositor doesn't crash.
    Compositor::self()->addRepaintFull();
    QSignalSpy frameRenderedSpy(Compositor::self()->scene(), &Scene::frameRendered);
    QVERIFY(frameRenderedSpy.wait());
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::XwaylandServerCrashTest)
#include "xwaylandserver_crash_test.moc"
