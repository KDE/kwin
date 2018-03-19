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
#include "platform.h"
#include "activities.h"
#include "client.h"
#include "cursor.h"
#include "deleted.h"
#include "screenedge.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "shell_client.h"
#include "xcbutils.h"
#include <kwineffects.h>

#include <netwm.h>
#include <xcb/xcb_icccm.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_activities-0");

class ActivitiesTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    void testSetOnActivitiesValidates();

private:
};

void ActivitiesTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    qRegisterMetaType<KWin::Deleted*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    kwinApp()->setUseKActivities(true);
    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
    waylandServer()->initWorkspace();
}

void ActivitiesTest::cleanupTestCase()
{
    QProcess::execute(QStringLiteral("kactivitymanagerd"), QStringList{QStringLiteral("stop")});
}

void ActivitiesTest::init()
{
    screens()->setCurrent(0);
    Cursor::setPos(QPoint(640, 512));
}

void ActivitiesTest::cleanup()
{
}

struct XcbConnectionDeleter
{
    static inline void cleanup(xcb_connection_t *pointer)
    {
        xcb_disconnect(pointer);
    }
};

void ActivitiesTest::testSetOnActivitiesValidates()
{
    // this test creates a Client and sets it on activities which don't exist
    // that should result in the window being on all activities
    // create an xcb window
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));

    xcb_window_t w = xcb_generate_id(c.data());
    const QRect windowGeometry(0, 0, 100, 200);

    auto cookie = xcb_create_window_checked(c.data(), 0, w, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, 0, 0, nullptr);
    QVERIFY(!xcb_request_check(c.data(), cookie));
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
    QVERIFY(client->isDecorated());

    //verify the test machine doesn't have the following activities used
    QVERIFY(!Activities::self()->all().contains(QStringLiteral("foo")));
    QVERIFY(!Activities::self()->all().contains(QStringLiteral("bar")));

    //setting the client to an invalid activities should result in the client being on all activities
    client->setOnActivity(QStringLiteral("foo"), true);
    QVERIFY(client->isOnAllActivities());
    QVERIFY(!client->activities().contains(QLatin1String("foo")));

    client->setOnActivities(QStringList{QStringLiteral("foo"), QStringLiteral("bar")});
    QVERIFY(client->isOnAllActivities());
    QVERIFY(!client->activities().contains(QLatin1String("foo")));
    QVERIFY(!client->activities().contains(QLatin1String("bar")));

    // and destroy the window again
    xcb_unmap_window(c.data(), w);
    xcb_destroy_window(c.data(), w);
    xcb_flush(c.data());
    c.reset();

    QSignalSpy windowClosedSpy(client, &Client::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    QVERIFY(windowClosedSpy.wait());
}

}

WAYLANDTEST_MAIN(KWin::ActivitiesTest)
#include "activities_test.moc"
