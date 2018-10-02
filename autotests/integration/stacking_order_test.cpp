/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>

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

#include "abstract_client.h"
#include "atoms.h"
#include "client.h"
#include "main.h"
#include "platform.h"
#include "shell_client.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/surface.h>

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_stacking_order-0");

class StackingOrderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testTransientIsAboveParent();
    void testRaiseTransient();

    void testGroupTransientIsAboveWindowGroup();
    void testRaiseGroupTransient();
    void testDontKeepAboveNonModalDialogGroupTransients();

};

void StackingOrderTest::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient *>();
    qRegisterMetaType<KWin::ShellClient *>();

    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    waylandServer()->initWorkspace();
}

void StackingOrderTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void StackingOrderTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void StackingOrderTest::testTransientIsAboveParent()
{
    // This test verifies that transients are always above their parents.

    // Create the parent.
    KWayland::Client::Surface *parentSurface =
        Test::createSurface(Test::waylandCompositor());
    QVERIFY(parentSurface);
    KWayland::Client::ShellSurface *parentShellSurface =
        Test::createShellSurface(parentSurface, parentSurface);
    QVERIFY(parentShellSurface);
    ShellClient *parent = Test::renderAndWaitForShown(parentSurface, QSize(256, 256), Qt::blue);
    QVERIFY(parent);
    QVERIFY(parent->isActive());
    QVERIFY(!parent->isTransient());

    // Initially, the stacking order should contain only the parent window.
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{parent}));

    // Create the transient.
    KWayland::Client::Surface *transientSurface =
        Test::createSurface(Test::waylandCompositor());
    QVERIFY(transientSurface);
    KWayland::Client::ShellSurface *transientShellSurface =
        Test::createShellSurface(transientSurface, transientSurface);
    QVERIFY(transientShellSurface);
    transientShellSurface->setTransient(parentSurface, QPoint(0, 0));
    ShellClient *transient = Test::renderAndWaitForShown(
        transientSurface, QSize(128, 128), Qt::red);
    QVERIFY(transient);
    QVERIFY(transient->isActive());
    QVERIFY(transient->isTransient());

    // The transient should be above the parent.
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{parent, transient}));

    // The transient still stays above the parent if we activate the latter.
    workspace()->activateClient(parent);
    QTRY_VERIFY(parent->isActive());
    QTRY_VERIFY(!transient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{parent, transient}));
}

void StackingOrderTest::testRaiseTransient()
{
    // This test verifies that both the parent and the transient will be
    // raised if either one of them is activated.

    // Create the parent.
    KWayland::Client::Surface *parentSurface =
        Test::createSurface(Test::waylandCompositor());
    QVERIFY(parentSurface);
    KWayland::Client::ShellSurface *parentShellSurface =
        Test::createShellSurface(parentSurface, parentSurface);
    QVERIFY(parentShellSurface);
    ShellClient *parent = Test::renderAndWaitForShown(parentSurface, QSize(256, 256), Qt::blue);
    QVERIFY(parent);
    QVERIFY(parent->isActive());
    QVERIFY(!parent->isTransient());

    // Initially, the stacking order should contain only the parent window.
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{parent}));

    // Create the transient.
    KWayland::Client::Surface *transientSurface =
        Test::createSurface(Test::waylandCompositor());
    QVERIFY(transientSurface);
    KWayland::Client::ShellSurface *transientShellSurface =
        Test::createShellSurface(transientSurface, transientSurface);
    QVERIFY(transientShellSurface);
    transientShellSurface->setTransient(parentSurface, QPoint(0, 0));
    ShellClient *transient = Test::renderAndWaitForShown(
        transientSurface, QSize(128, 128), Qt::red);
    QVERIFY(transient);
    QTRY_VERIFY(transient->isActive());
    QVERIFY(transient->isTransient());

    // The transient should be above the parent.
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{parent, transient}));

    // Create a window that doesn't have any relationship to the parent or the transient.
    KWayland::Client::Surface *anotherSurface =
        Test::createSurface(Test::waylandCompositor());
    QVERIFY(anotherSurface);
    KWayland::Client::ShellSurface *anotherShellSurface =
        Test::createShellSurface(anotherSurface, anotherSurface);
    QVERIFY(anotherShellSurface);
    ShellClient *anotherClient = Test::renderAndWaitForShown(anotherSurface, QSize(128, 128), Qt::green);
    QVERIFY(anotherClient);
    QVERIFY(anotherClient->isActive());
    QVERIFY(!anotherClient->isTransient());

    // The newly created surface has to be above both the parent and the transient.
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{parent, transient, anotherClient}));

    // If we activate the parent, the transient should be raised too.
    workspace()->activateClient(parent);
    QTRY_VERIFY(parent->isActive());
    QTRY_VERIFY(!transient->isActive());
    QTRY_VERIFY(!anotherClient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{anotherClient, parent, transient}));

    // Go back to the initial setup.
    workspace()->activateClient(anotherClient);
    QTRY_VERIFY(!parent->isActive());
    QTRY_VERIFY(!transient->isActive());
    QTRY_VERIFY(anotherClient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{parent, transient, anotherClient}));

    // If we activate the transient, the parent should be raised too.
    workspace()->activateClient(transient);
    QTRY_VERIFY(!parent->isActive());
    QTRY_VERIFY(transient->isActive());
    QTRY_VERIFY(!anotherClient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{anotherClient, parent, transient}));
}

static xcb_window_t createGroupWindow(xcb_connection_t *conn,
                                      const QRect &geometry,
                                      xcb_window_t leaderWid = XCB_WINDOW_NONE)
{
    xcb_window_t wid = xcb_generate_id(conn);
    xcb_create_window(
        conn,                          // c
        XCB_COPY_FROM_PARENT,          // depth
        wid,                           // wid
        rootWindow(),                  // parent
        geometry.x(),                  // x
        geometry.y(),                  // y
        geometry.width(),              // width
        geometry.height(),             // height
        0,                             // border_width
        XCB_WINDOW_CLASS_INPUT_OUTPUT, // _class
        XCB_COPY_FROM_PARENT,          // visual
        0,                             // value_mask
        nullptr                        // value_list
    );

    xcb_size_hints_t sizeHints = {};
    xcb_icccm_size_hints_set_position(&sizeHints, 1, geometry.x(), geometry.y());
    xcb_icccm_size_hints_set_size(&sizeHints, 1, geometry.width(), geometry.height());
    xcb_icccm_set_wm_normal_hints(conn, wid, &sizeHints);

    if (leaderWid == XCB_WINDOW_NONE) {
        leaderWid = wid;
    }

    xcb_change_property(
        conn,                    // c
        XCB_PROP_MODE_REPLACE,   // mode
        wid,                     // window
        atoms->wm_client_leader, // property
        XCB_ATOM_WINDOW,         // type
        32,                      // format
        1,                       // data_len
        &leaderWid               // data
    );

    return wid;
}

struct XcbConnectionDeleter
{
    static inline void cleanup(xcb_connection_t *c) {
        xcb_disconnect(c);
    }
};

void StackingOrderTest::testGroupTransientIsAboveWindowGroup()
{
    // This test verifies that group transients are always above other
    // window group members.

    const QRect geometry = QRect(0, 0, 128, 128);

    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> conn(
        xcb_connect(nullptr, nullptr));

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());

    // Create the group leader.
    xcb_window_t leaderWid = createGroupWindow(conn.data(), geometry);
    xcb_map_window(conn.data(), leaderWid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    Client *leader = windowCreatedSpy.first().first().value<Client *>();
    QVERIFY(leader);
    QVERIFY(leader->isActive());
    QCOMPARE(leader->windowId(), leaderWid);
    QVERIFY(!leader->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{leader}));

    // Create another group member.
    windowCreatedSpy.clear();
    xcb_window_t member1Wid = createGroupWindow(conn.data(), geometry, leaderWid);
    xcb_map_window(conn.data(), member1Wid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    Client *member1 = windowCreatedSpy.first().first().value<Client *>();
    QVERIFY(member1);
    QVERIFY(member1->isActive());
    QCOMPARE(member1->windowId(), member1Wid);
    QCOMPARE(member1->group(), leader->group());
    QVERIFY(!member1->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{leader, member1}));

    // Create yet another group member.
    windowCreatedSpy.clear();
    xcb_window_t member2Wid = createGroupWindow(conn.data(), geometry, leaderWid);
    xcb_map_window(conn.data(), member2Wid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    Client *member2 = windowCreatedSpy.first().first().value<Client *>();
    QVERIFY(member2);
    QVERIFY(member2->isActive());
    QCOMPARE(member2->windowId(), member2Wid);
    QCOMPARE(member2->group(), leader->group());
    QVERIFY(!member2->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{leader, member1, member2}));

    // Create a group transient.
    windowCreatedSpy.clear();
    xcb_window_t transientWid = createGroupWindow(conn.data(), geometry, leaderWid);
    xcb_icccm_set_wm_transient_for(conn.data(), transientWid, rootWindow());

    // Currently, we have some weird bug workaround: if a group transient
    // is a non-modal dialog, then it won't be kept above its window group.
    // We need to explicitly specify window type, otherwise the window type
    // will be deduced to _NET_WM_WINDOW_TYPE_DIALOG because we set transient
    // for before (the EWMH spec says to do that).
    xcb_atom_t net_wm_window_type = Xcb::Atom(
        QByteArrayLiteral("_NET_WM_WINDOW_TYPE"), false, conn.data());
    xcb_atom_t net_wm_window_type_normal = Xcb::Atom(
        QByteArrayLiteral("_NET_WM_WINDOW_TYPE_NORMAL"), false, conn.data());
    xcb_change_property(
        conn.data(),               // c
        XCB_PROP_MODE_REPLACE,     // mode
        transientWid,              // window
        net_wm_window_type,        // property
        XCB_ATOM_ATOM,             // type
        32,                        // format
        1,                         // data_len
        &net_wm_window_type_normal // data
    );

    xcb_map_window(conn.data(), transientWid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    Client *transient = windowCreatedSpy.first().first().value<Client *>();
    QVERIFY(transient);
    QVERIFY(transient->isActive());
    QCOMPARE(transient->windowId(), transientWid);
    QCOMPARE(transient->group(), leader->group());
    QVERIFY(transient->isTransient());
    QVERIFY(transient->groupTransient());
    QVERIFY(!transient->isDialog()); // See above why

    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{leader, member1, member2, transient}));

    // If we activate any member of the window group, the transient will be above it.
    workspace()->activateClient(leader);
    QTRY_VERIFY(leader->isActive());
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{member1, member2, leader, transient}));

    workspace()->activateClient(member1);
    QTRY_VERIFY(member1->isActive());
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{member2, leader, member1, transient}));

    workspace()->activateClient(member2);
    QTRY_VERIFY(member2->isActive());
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{leader, member1, member2, transient}));

    workspace()->activateClient(transient);
    QTRY_VERIFY(transient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{leader, member1, member2, transient}));
}

void StackingOrderTest::testRaiseGroupTransient()
{
    const QRect geometry = QRect(0, 0, 128, 128);

    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> conn(
        xcb_connect(nullptr, nullptr));

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());

    // Create the group leader.
    xcb_window_t leaderWid = createGroupWindow(conn.data(), geometry);
    xcb_map_window(conn.data(), leaderWid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    Client *leader = windowCreatedSpy.first().first().value<Client *>();
    QVERIFY(leader);
    QVERIFY(leader->isActive());
    QCOMPARE(leader->windowId(), leaderWid);
    QVERIFY(!leader->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{leader}));

    // Create another group member.
    windowCreatedSpy.clear();
    xcb_window_t member1Wid = createGroupWindow(conn.data(), geometry, leaderWid);
    xcb_map_window(conn.data(), member1Wid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    Client *member1 = windowCreatedSpy.first().first().value<Client *>();
    QVERIFY(member1);
    QVERIFY(member1->isActive());
    QCOMPARE(member1->windowId(), member1Wid);
    QCOMPARE(member1->group(), leader->group());
    QVERIFY(!member1->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{leader, member1}));

    // Create yet another group member.
    windowCreatedSpy.clear();
    xcb_window_t member2Wid = createGroupWindow(conn.data(), geometry, leaderWid);
    xcb_map_window(conn.data(), member2Wid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    Client *member2 = windowCreatedSpy.first().first().value<Client *>();
    QVERIFY(member2);
    QVERIFY(member2->isActive());
    QCOMPARE(member2->windowId(), member2Wid);
    QCOMPARE(member2->group(), leader->group());
    QVERIFY(!member2->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{leader, member1, member2}));

    // Create a group transient.
    windowCreatedSpy.clear();
    xcb_window_t transientWid = createGroupWindow(conn.data(), geometry, leaderWid);
    xcb_icccm_set_wm_transient_for(conn.data(), transientWid, rootWindow());

    // Currently, we have some weird bug workaround: if a group transient
    // is a non-modal dialog, then it won't be kept above its window group.
    // We need to explicitly specify window type, otherwise the window type
    // will be deduced to _NET_WM_WINDOW_TYPE_DIALOG because we set transient
    // for before (the EWMH spec says to do that).
    xcb_atom_t net_wm_window_type = Xcb::Atom(
        QByteArrayLiteral("_NET_WM_WINDOW_TYPE"), false, conn.data());
    xcb_atom_t net_wm_window_type_normal = Xcb::Atom(
        QByteArrayLiteral("_NET_WM_WINDOW_TYPE_NORMAL"), false, conn.data());
    xcb_change_property(
        conn.data(),               // c
        XCB_PROP_MODE_REPLACE,     // mode
        transientWid,              // window
        net_wm_window_type,        // property
        XCB_ATOM_ATOM,             // type
        32,                        // format
        1,                         // data_len
        &net_wm_window_type_normal // data
    );

    xcb_map_window(conn.data(), transientWid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    Client *transient = windowCreatedSpy.first().first().value<Client *>();
    QVERIFY(transient);
    QVERIFY(transient->isActive());
    QCOMPARE(transient->windowId(), transientWid);
    QCOMPARE(transient->group(), leader->group());
    QVERIFY(transient->isTransient());
    QVERIFY(transient->groupTransient());
    QVERIFY(!transient->isDialog()); // See above why

    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{leader, member1, member2, transient}));

    // Create a Wayland client that is not a member of the window group.
    KWayland::Client::Surface *anotherSurface =
        Test::createSurface(Test::waylandCompositor());
    QVERIFY(anotherSurface);
    KWayland::Client::ShellSurface *anotherShellSurface =
        Test::createShellSurface(anotherSurface, anotherSurface);
    QVERIFY(anotherShellSurface);
    ShellClient *anotherClient = Test::renderAndWaitForShown(anotherSurface, QSize(128, 128), Qt::green);
    QVERIFY(anotherClient);
    QVERIFY(anotherClient->isActive());
    QVERIFY(!anotherClient->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{leader, member1, member2, transient, anotherClient}));

    // If we activate the leader, then only it and the transient have to be raised.
    workspace()->activateClient(leader);
    QTRY_VERIFY(leader->isActive());
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{member1, member2, anotherClient, leader, transient}));

    // If another member of the window group is activated, then the transient will
    // be above that member and the leader.
    workspace()->activateClient(member2);
    QTRY_VERIFY(member2->isActive());
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{member1, anotherClient, leader, member2, transient}));

    // FIXME: If we activate the transient, only it will be raised.
    workspace()->activateClient(anotherClient);
    QTRY_VERIFY(anotherClient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{member1, leader, member2, transient, anotherClient}));

    workspace()->activateClient(transient);
    QTRY_VERIFY(transient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{member1, leader, member2, anotherClient, transient}));
}

void StackingOrderTest::testDontKeepAboveNonModalDialogGroupTransients()
{
    // Bug 76026

    const QRect geometry = QRect(0, 0, 128, 128);

    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> conn(
        xcb_connect(nullptr, nullptr));

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());

    // Create the group leader.
    xcb_window_t leaderWid = createGroupWindow(conn.data(), geometry);
    xcb_map_window(conn.data(), leaderWid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    Client *leader = windowCreatedSpy.first().first().value<Client *>();
    QVERIFY(leader);
    QVERIFY(leader->isActive());
    QCOMPARE(leader->windowId(), leaderWid);
    QVERIFY(!leader->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{leader}));

    // Create another group member.
    windowCreatedSpy.clear();
    xcb_window_t member1Wid = createGroupWindow(conn.data(), geometry, leaderWid);
    xcb_map_window(conn.data(), member1Wid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    Client *member1 = windowCreatedSpy.first().first().value<Client *>();
    QVERIFY(member1);
    QVERIFY(member1->isActive());
    QCOMPARE(member1->windowId(), member1Wid);
    QCOMPARE(member1->group(), leader->group());
    QVERIFY(!member1->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{leader, member1}));

    // Create yet another group member.
    windowCreatedSpy.clear();
    xcb_window_t member2Wid = createGroupWindow(conn.data(), geometry, leaderWid);
    xcb_map_window(conn.data(), member2Wid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    Client *member2 = windowCreatedSpy.first().first().value<Client *>();
    QVERIFY(member2);
    QVERIFY(member2->isActive());
    QCOMPARE(member2->windowId(), member2Wid);
    QCOMPARE(member2->group(), leader->group());
    QVERIFY(!member2->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{leader, member1, member2}));

    // Create a group transient.
    windowCreatedSpy.clear();
    xcb_window_t transientWid = createGroupWindow(conn.data(), geometry, leaderWid);
    xcb_icccm_set_wm_transient_for(conn.data(), transientWid, rootWindow());
    xcb_map_window(conn.data(), transientWid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    Client *transient = windowCreatedSpy.first().first().value<Client *>();
    QVERIFY(transient);
    QVERIFY(transient->isActive());
    QCOMPARE(transient->windowId(), transientWid);
    QCOMPARE(transient->group(), leader->group());
    QVERIFY(transient->isTransient());
    QVERIFY(transient->groupTransient());
    QVERIFY(transient->isDialog());
    QVERIFY(!transient->isModal());

    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{leader, member1, member2, transient}));

    workspace()->activateClient(leader);
    QTRY_VERIFY(leader->isActive());
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{member1, member2, transient, leader}));

    workspace()->activateClient(member1);
    QTRY_VERIFY(member1->isActive());
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{member2, transient, leader, member1}));

    workspace()->activateClient(member2);
    QTRY_VERIFY(member2->isActive());
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{transient, leader, member1, member2}));

    workspace()->activateClient(transient);
    QTRY_VERIFY(transient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (ToplevelList{leader, member1, member2, transient}));
}

WAYLANDTEST_MAIN(StackingOrderTest)
#include "stacking_order_test.moc"
