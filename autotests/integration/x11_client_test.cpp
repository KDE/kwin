/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "kwin_wayland_test.h"
#include "composite.h"
#include "effects.h"
#include "effectloader.h"
#include "cursor.h"
#include "platform.h"
#include "scene_qpainter.h"
#include "screens.h"
#include "shell_client.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>
#include <KWayland/Client/shell.h>

#include <netwm.h>
#include <xcb/xcb_icccm.h>

using namespace KWin;
using namespace KWayland::Client;
static const QString s_socketName = QStringLiteral("wayland_test_x11_client-0");

class X11ClientTest : public QObject
{
Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testCaptionSimplified();
    void testFullscreenLayerWithActiveWaylandWindow();
    void testFocusInWithWaylandLastActiveWindow();
    void testX11WindowId();
};

void X11ClientTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QVERIFY(KWin::Compositor::self());
    waylandServer()->initWorkspace();
}

void X11ClientTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void X11ClientTest::cleanup()
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


void X11ClientTest::testCaptionSimplified()
{
    // this test verifies that caption is properly trimmed
    // see BUG 323798 comment #12
    QSignalSpy windowAddedSpy(effects, &EffectsHandler::windowAdded);
    QVERIFY(windowAddedSpy.isValid());

    // create an xcb window
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t w = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, w, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.data(), w, &hints);
    NETWinInfo winInfo(c.data(), w, rootWindow(), NET::Properties(), NET::Properties2());
    const QByteArray origTitle = QByteArrayLiteral("Was tun, wenn Schüler Autismus haben?\342\200\250\342\200\250\342\200\250 – Marlies Hübner - Mozilla Firefox");
    winInfo.setName(origTitle.constData());
    xcb_map_window(c.data(), w);
    xcb_flush(c.data());

    // we should get a client for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    Client *client = windowCreatedSpy.first().first().value<Client*>();
    QVERIFY(client);
    QCOMPARE(client->window(), w);
    QVERIFY(client->caption() != QString::fromUtf8(origTitle));
    QCOMPARE(client->caption(), QString::fromUtf8(origTitle).simplified());

    // and destroy the window again
    xcb_unmap_window(c.data(), w);
    xcb_flush(c.data());

    QSignalSpy windowClosedSpy(client, &Client::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    QVERIFY(windowClosedSpy.wait());
    xcb_destroy_window(c.data(), w);
    c.reset();
}

void X11ClientTest::testFullscreenLayerWithActiveWaylandWindow()
{
    // this test verifies that an X11 fullscreen window does not stay in the active layer
    // when a Wayland window is active, see BUG: 375759
    QCOMPARE(screens()->count(), 1);

    // first create an X11 window
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t w = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, w, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.data(), w, &hints);
    xcb_map_window(c.data(), w);
    xcb_flush(c.data());

    // we should get a client for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    Client *client = windowCreatedSpy.first().first().value<Client*>();
    QVERIFY(client);
    QCOMPARE(client->window(), w);
    QVERIFY(!client->isFullScreen());
    QVERIFY(client->isActive());
    QCOMPARE(client->layer(), NormalLayer);

    workspace()->slotWindowFullScreen();
    QVERIFY(client->isFullScreen());
    QCOMPARE(client->layer(), ActiveLayer);
    QCOMPARE(workspace()->stackingOrder().last(), client);

    // now let's open a Wayland window
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
    auto waylandClient = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(waylandClient);
    QVERIFY(waylandClient->isActive());
    QCOMPARE(waylandClient->layer(), NormalLayer);
    QCOMPARE(workspace()->stackingOrder().last(), waylandClient);
    QCOMPARE(workspace()->xStackingOrder().last(), waylandClient);
    QCOMPARE(client->layer(), NormalLayer);

    // now activate fullscreen again
    workspace()->activateClient(client);
    QTRY_VERIFY(client->isActive());
    QCOMPARE(client->layer(), ActiveLayer);
    QCOMPARE(workspace()->stackingOrder().last(), client);
    QCOMPARE(workspace()->xStackingOrder().last(), client);

    // activate wayland window again
    workspace()->activateClient(waylandClient);
    QTRY_VERIFY(waylandClient->isActive());
    QCOMPARE(workspace()->stackingOrder().last(), waylandClient);
    QCOMPARE(workspace()->xStackingOrder().last(), waylandClient);

    // back to x window
    workspace()->activateClient(client);
    QTRY_VERIFY(client->isActive());
    // remove fullscreen
    QVERIFY(client->isFullScreen());
    workspace()->slotWindowFullScreen();
    QVERIFY(!client->isFullScreen());
    // and fullscreen again
    workspace()->slotWindowFullScreen();
    QVERIFY(client->isFullScreen());
    QCOMPARE(workspace()->stackingOrder().last(), client);
    QCOMPARE(workspace()->xStackingOrder().last(), client);

    // activate wayland window again
    workspace()->activateClient(waylandClient);
    QTRY_VERIFY(waylandClient->isActive());
    QCOMPARE(workspace()->stackingOrder().last(), waylandClient);
    QCOMPARE(workspace()->xStackingOrder().last(), waylandClient);

    // back to X11 window
    workspace()->activateClient(client);
    QTRY_VERIFY(client->isActive());
    // remove fullscreen
    QVERIFY(client->isFullScreen());
    workspace()->slotWindowFullScreen();
    QVERIFY(!client->isFullScreen());
    // and fullscreen through X API
    NETWinInfo info(c.data(), w, kwinApp()->x11RootWindow(), NET::Properties(), NET::Properties2());
    info.setState(NET::FullScreen, NET::FullScreen);
    NETRootInfo rootInfo(c.data(), NET::Properties());
    rootInfo.setActiveWindow(w, NET::FromApplication, XCB_CURRENT_TIME, XCB_WINDOW_NONE);
    xcb_flush(c.data());
    QTRY_VERIFY(client->isFullScreen());
    QCOMPARE(workspace()->stackingOrder().last(), client);
    QCOMPARE(workspace()->xStackingOrder().last(), client);

    // activate wayland window again
    workspace()->activateClient(waylandClient);
    QTRY_VERIFY(waylandClient->isActive());
    QCOMPARE(workspace()->stackingOrder().last(), waylandClient);
    QCOMPARE(workspace()->xStackingOrder().last(), waylandClient);
    QCOMPARE(client->layer(), NormalLayer);

    // close the window
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(waylandClient));
    QTRY_VERIFY(client->isActive());
    QCOMPARE(client->layer(), ActiveLayer);

    // and destroy the window again
    xcb_unmap_window(c.data(), w);
    xcb_flush(c.data());
}

void X11ClientTest::testFocusInWithWaylandLastActiveWindow()
{
    // this test verifies that Workspace::allowClientActivation does not crash if last client was a Wayland client

    // create an X11 window
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t w = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, w, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.data(), w, &hints);
    xcb_map_window(c.data(), w);
    xcb_flush(c.data());

    // we should get a client for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    Client *client = windowCreatedSpy.first().first().value<Client*>();
    QVERIFY(client);
    QCOMPARE(client->window(), w);
    QVERIFY(client->isActive());

    // create Wayland window
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
    auto waylandClient = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(waylandClient);
    QVERIFY(waylandClient->isActive());
    // activate no window
    workspace()->setActiveClient(nullptr);
    QVERIFY(!waylandClient->isActive());
    QVERIFY(!workspace()->activeClient());
    // and close Wayland window again
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(waylandClient));

    // and try to activate the x11 client through X11 api
    const auto cookie = xcb_set_input_focus_checked(c.data(), XCB_INPUT_FOCUS_NONE, w, XCB_CURRENT_TIME);
    auto error = xcb_request_check(c.data(), cookie);
    QVERIFY(!error);
    // this accesses last_active_client on trying to activate
    QTRY_VERIFY(client->isActive());

    // and destroy the window again
    xcb_unmap_window(c.data(), w);
    xcb_flush(c.data());
}

void X11ClientTest::testX11WindowId()
{
    // create an X11 window
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t w = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, w, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.data(), w, &hints);
    xcb_map_window(c.data(), w);
    xcb_flush(c.data());

    // we should get a client for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    Client *client = windowCreatedSpy.first().first().value<Client*>();
    QVERIFY(client);
    QCOMPARE(client->windowId(), w);
    QVERIFY(client->isActive());
    QCOMPARE(client->window(), w);

    NETRootInfo rootInfo(c.data(), NET::WMAllProperties);
    QCOMPARE(rootInfo.activeWindow(), client->window());

    // activate a wayland window
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
    auto waylandClient = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(waylandClient);
    QVERIFY(waylandClient->isActive());
    xcb_flush(kwinApp()->x11Connection());

    NETRootInfo rootInfo2(c.data(), NET::WMAllProperties);
    QCOMPARE(rootInfo2.activeWindow(), 0u);

    // back to X11 client
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(waylandClient));

    QTRY_VERIFY(client->isActive());
    NETRootInfo rootInfo3(c.data(), NET::WMAllProperties);
    QCOMPARE(rootInfo3.activeWindow(), client->window());

    // and destroy the window again
    xcb_unmap_window(c.data(), w);
    xcb_flush(c.data());
}

WAYLANDTEST_MAIN(X11ClientTest)
#include "x11_client_test.moc"
