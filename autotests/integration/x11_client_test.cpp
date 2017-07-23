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
};

void X11ClientTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

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

WAYLANDTEST_MAIN(X11ClientTest)
#include "x11_client_test.moc"
