/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "composite.h"
#include "core/outputbackend.h"
#include "main.h"
#include "scene/workspacescene.h"
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

static const QString s_socketName = QStringLiteral("wayland_test_kwin_xwayland_server_restart-0");

class XwaylandServerRestartTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void testRestart();
};

void XwaylandServerRestartTest::initTestCase()
{
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup xwaylandGroup = config->group("Xwayland");
    xwaylandGroup.writeEntry(QStringLiteral("XwaylandCrashPolicy"), QStringLiteral("Restart"));
    xwaylandGroup.sync();
    kwinApp()->setConfig(config);

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
}

static void kwin_safe_kill(QProcess *process)
{
    // The SIGKILL signal must be sent when the event loop is spinning.
    QTimer::singleShot(1, process, &QProcess::kill);
}

void XwaylandServerRestartTest::testRestart()
{
    // This test verifies that the Xwayland server will be restarted after a crash.

    Xwl::Xwayland *xwayland = static_cast<Xwl::Xwayland *>(kwinApp()->xwayland());

    // Pretend that the Xwayland process has crashed by sending a SIGKILL to it.
    QSignalSpy startedSpy(xwayland, &Xwl::Xwayland::started);
    kwin_safe_kill(xwayland->xwaylandLauncher()->process());
    QVERIFY(startedSpy.wait());
    QCOMPARE(startedSpy.count(), 1);

    // Check that the compositor still accepts new X11 clients.
    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.get()));
    const QRect rect(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      rect.x(), rect.y(), rect.width(), rect.height(), 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, rect.x(), rect.y());
    xcb_icccm_size_hints_set_size(&hints, 1, rect.width(), rect.height());
    xcb_icccm_size_hints_set_min_size(&hints, rect.width(), rect.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.last().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QVERIFY(window->isDecorated());

    // Render a frame to ensure that the compositor doesn't crash.
    Compositor::self()->scene()->addRepaintFull();
    QSignalSpy frameRenderedSpy(Compositor::self()->scene(), &WorkspaceScene::frameRendered);
    QVERIFY(frameRenderedSpy.wait());

    // Destroy the test window.
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    QVERIFY(Test::waitForWindowDestroyed(window));
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::XwaylandServerRestartTest)
#include "xwaylandserver_restart_test.moc"
