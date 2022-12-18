/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "composite.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "main.h"
#include "scene/workspacescene.h"
#include "unmanaged.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"
#include "xwayland/xwayland.h"
#include "xwayland/xwaylandlauncher.h"

#include <xcb/xcb_icccm.h>

namespace KWin
{

struct XcbConnectionDeleter
{
    void operator()(xcb_connection_t *pointer)
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
    qRegisterMetaType<X11Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup xwaylandGroup = config->group("Xwayland");
    xwaylandGroup.writeEntry(QStringLiteral("XwaylandCrashPolicy"), QStringLiteral("Stop"));
    xwaylandGroup.sync();
    kwinApp()->setConfig(config);

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void XwaylandServerCrashTest::testCrash()
{
    // This test verifies that all connected X11 clients get destroyed when Xwayland crashes.

    // Create a normal window.
    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.get()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId1 = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId1, rootWindow(),
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
    xcb_icccm_set_wm_normal_hints(c.get(), windowId1, &hints);
    xcb_map_window(c.get(), windowId1);
    xcb_flush(c.get());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    QPointer<X11Window> window = windowCreatedSpy.last().first().value<X11Window *>();
    QVERIFY(window);
    QVERIFY(window->isDecorated());

    // Create an override-redirect window.
    xcb_window_t windowId2 = xcb_generate_id(c.get());
    const uint32_t values[] = {true};
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId2, rootWindow(),
                      windowGeometry.x(), windowGeometry.y(),
                      windowGeometry.width(), windowGeometry.height(), 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
                      XCB_CW_OVERRIDE_REDIRECT, values);
    xcb_map_window(c.get(), windowId2);
    xcb_flush(c.get());

    QSignalSpy unmanagedAddedSpy(workspace(), &Workspace::unmanagedAdded);
    QVERIFY(unmanagedAddedSpy.wait());
    QPointer<Unmanaged> unmanaged = unmanagedAddedSpy.last().first().value<Unmanaged *>();
    QVERIFY(unmanaged);

    // Let's pretend that the Xwayland process has crashed.
    QSignalSpy x11ConnectionChangedSpy(kwinApp(), &Application::x11ConnectionChanged);
    Xwl::Xwayland *xwayland = static_cast<Xwl::Xwayland *>(kwinApp()->xwayland());
    xwayland->xwaylandLauncher()->process()->terminate();
    QVERIFY(x11ConnectionChangedSpy.wait());

    // When Xwayland crashes, the compositor should tear down the XCB connection and destroy
    // all connected X11 clients.
    QTRY_VERIFY(!window);
    QTRY_VERIFY(!unmanaged);
    QCOMPARE(kwinApp()->x11Connection(), nullptr);
    QCOMPARE(kwinApp()->x11RootWindow(), XCB_WINDOW_NONE);

    // Render a frame to ensure that the compositor doesn't crash.
    Compositor::self()->scene()->addRepaintFull();
    QSignalSpy frameRenderedSpy(Compositor::self()->scene(), &WorkspaceScene::frameRendered);
    QVERIFY(frameRenderedSpy.wait());
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::XwaylandServerCrashTest)
#include "xwaylandserver_crash_test.moc"
