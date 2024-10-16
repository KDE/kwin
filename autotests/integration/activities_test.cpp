/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "activities.h"
#include "core/output.h"
#include "pointer_input.h"
#include "utils/xcbutils.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>

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
    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    kwinApp()->setUseKActivities(true);
    kwinApp()->start();
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
}

void ActivitiesTest::cleanupTestCase()
{
    // terminate any still running kactivitymanagerd
    QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.ActivityManager"),
        QStringLiteral("/ActivityManager"),
        QStringLiteral("org.qtproject.Qt.QCoreApplication"),
        QStringLiteral("quit")));
}

void ActivitiesTest::init()
{
    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
}

void ActivitiesTest::cleanup()
{
}

void ActivitiesTest::testSetOnActivitiesValidates()
{
    // this test verifies that windows can't be placed on activities that don't exist
    // create an xcb window
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));

    xcb_window_t windowId = xcb_generate_id(c.get());
    const QRect windowGeometry(0, 0, 100, 200);

    auto cookie = xcb_create_window_checked(c.get(), 0, windowId, rootWindow(),
                                            windowGeometry.x(),
                                            windowGeometry.y(),
                                            windowGeometry.width(),
                                            windowGeometry.height(),
                                            0, XCB_WINDOW_CLASS_INPUT_OUTPUT, 0, 0, nullptr);
    QVERIFY(!xcb_request_check(c.get(), cookie));
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
    QVERIFY(window->isDecorated());

    // verify the test machine doesn't have the following activities used
    QVERIFY(!Workspace::self()->activities()->all().contains(QStringLiteral("foo")));
    QVERIFY(!Workspace::self()->activities()->all().contains(QStringLiteral("bar")));

    window->setOnActivities(QStringList{QStringLiteral("foo"), QStringLiteral("bar")});
    QVERIFY(!window->activities().contains(QLatin1String("foo")));
    QVERIFY(!window->activities().contains(QLatin1String("bar")));

    // and destroy the window again
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    c.reset();

    QSignalSpy windowClosedSpy(window, &X11Window::closed);
    QVERIFY(windowClosedSpy.wait());
}

}

WAYLANDTEST_MAIN(KWin::ActivitiesTest)
#include "activities_test.moc"
