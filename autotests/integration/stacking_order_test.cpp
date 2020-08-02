/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "abstract_client.h"
#include "atoms.h"
#include "x11client.h"
#include "deleted.h"
#include "main.h"
#include "platform.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
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
    void testDeletedTransient();

    void testGroupTransientIsAboveWindowGroup();
    void testRaiseGroupTransient();
    void testDeletedGroupTransient();
    void testDontKeepAboveNonModalDialogGroupTransients();

    void testKeepAbove();
    void testKeepBelow();

};

void StackingOrderTest::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient *>();
    qRegisterMetaType<KWin::Deleted *>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
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
    KWayland::Client::XdgShellSurface *parentShellSurface =
        Test::createXdgShellStableSurface(parentSurface, parentSurface);
    QVERIFY(parentShellSurface);
    AbstractClient *parent = Test::renderAndWaitForShown(parentSurface, QSize(256, 256), Qt::blue);
    QVERIFY(parent);
    QVERIFY(parent->isActive());
    QVERIFY(!parent->isTransient());

    // Initially, the stacking order should contain only the parent window.
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{parent}));

    // Create the transient.
    KWayland::Client::Surface *transientSurface =
        Test::createSurface(Test::waylandCompositor());
    QVERIFY(transientSurface);
    KWayland::Client::XdgShellSurface *transientShellSurface =
        Test::createXdgShellStableSurface(transientSurface, transientSurface);
    QVERIFY(transientShellSurface);
    transientShellSurface->setTransientFor(parentShellSurface);
    AbstractClient *transient = Test::renderAndWaitForShown(
        transientSurface, QSize(128, 128), Qt::red);
    QVERIFY(transient);
    QVERIFY(transient->isActive());
    QVERIFY(transient->isTransient());

    // The transient should be above the parent.
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{parent, transient}));

    // The transient still stays above the parent if we activate the latter.
    workspace()->activateClient(parent);
    QTRY_VERIFY(parent->isActive());
    QTRY_VERIFY(!transient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{parent, transient}));
}

void StackingOrderTest::testRaiseTransient()
{
    // This test verifies that both the parent and the transient will be
    // raised if either one of them is activated.

    // Create the parent.
    KWayland::Client::Surface *parentSurface =
        Test::createSurface(Test::waylandCompositor());
    QVERIFY(parentSurface);
    KWayland::Client::XdgShellSurface *parentShellSurface =
        Test::createXdgShellStableSurface(parentSurface, parentSurface);
    QVERIFY(parentShellSurface);
    AbstractClient *parent = Test::renderAndWaitForShown(parentSurface, QSize(256, 256), Qt::blue);
    QVERIFY(parent);
    QVERIFY(parent->isActive());
    QVERIFY(!parent->isTransient());

    // Initially, the stacking order should contain only the parent window.
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{parent}));

    // Create the transient.
    KWayland::Client::Surface *transientSurface =
        Test::createSurface(Test::waylandCompositor());
    QVERIFY(transientSurface);
    KWayland::Client::XdgShellSurface *transientShellSurface =
        Test::createXdgShellStableSurface(transientSurface, transientSurface);
    QVERIFY(transientShellSurface);
    transientShellSurface->setTransientFor(parentShellSurface);
    AbstractClient *transient = Test::renderAndWaitForShown(
        transientSurface, QSize(128, 128), Qt::red);
    QVERIFY(transient);
    QTRY_VERIFY(transient->isActive());
    QVERIFY(transient->isTransient());

    // The transient should be above the parent.
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{parent, transient}));

    // Create a window that doesn't have any relationship to the parent or the transient.
    KWayland::Client::Surface *anotherSurface =
        Test::createSurface(Test::waylandCompositor());
    QVERIFY(anotherSurface);
    KWayland::Client::XdgShellSurface *anotherShellSurface =
        Test::createXdgShellStableSurface(anotherSurface, anotherSurface);
    QVERIFY(anotherShellSurface);
    AbstractClient *anotherClient = Test::renderAndWaitForShown(anotherSurface, QSize(128, 128), Qt::green);
    QVERIFY(anotherClient);
    QVERIFY(anotherClient->isActive());
    QVERIFY(!anotherClient->isTransient());

    // The newly created surface has to be above both the parent and the transient.
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{parent, transient, anotherClient}));

    // If we activate the parent, the transient should be raised too.
    workspace()->activateClient(parent);
    QTRY_VERIFY(parent->isActive());
    QTRY_VERIFY(!transient->isActive());
    QTRY_VERIFY(!anotherClient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{anotherClient, parent, transient}));

    // Go back to the initial setup.
    workspace()->activateClient(anotherClient);
    QTRY_VERIFY(!parent->isActive());
    QTRY_VERIFY(!transient->isActive());
    QTRY_VERIFY(anotherClient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{parent, transient, anotherClient}));

    // If we activate the transient, the parent should be raised too.
    workspace()->activateClient(transient);
    QTRY_VERIFY(!parent->isActive());
    QTRY_VERIFY(transient->isActive());
    QTRY_VERIFY(!anotherClient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{anotherClient, parent, transient}));
}

struct WindowUnrefDeleter
{
    static inline void cleanup(Deleted *d) {
        if (d != nullptr) {
            d->unrefWindow();
        }
    }
};

void StackingOrderTest::testDeletedTransient()
{
    // This test verifies that deleted transients are kept above their
    // old parents.

    // Create the parent.
    KWayland::Client::Surface *parentSurface =
        Test::createSurface(Test::waylandCompositor());
    QVERIFY(parentSurface);
    KWayland::Client::XdgShellSurface *parentShellSurface =
        Test::createXdgShellStableSurface(parentSurface, parentSurface);
    QVERIFY(parentShellSurface);
    AbstractClient *parent = Test::renderAndWaitForShown(parentSurface, QSize(256, 256), Qt::blue);
    QVERIFY(parent);
    QVERIFY(parent->isActive());
    QVERIFY(!parent->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{parent}));

    // Create the first transient.
    KWayland::Client::Surface *transient1Surface =
        Test::createSurface(Test::waylandCompositor());
    QVERIFY(transient1Surface);
    KWayland::Client::XdgShellSurface *transient1ShellSurface =
        Test::createXdgShellStableSurface(transient1Surface, transient1Surface);
    QVERIFY(transient1ShellSurface);
    transient1ShellSurface->setTransientFor(parentShellSurface);
    AbstractClient *transient1 = Test::renderAndWaitForShown(
        transient1Surface, QSize(128, 128), Qt::red);
    QVERIFY(transient1);
    QTRY_VERIFY(transient1->isActive());
    QVERIFY(transient1->isTransient());
    QCOMPARE(transient1->transientFor(), parent);

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{parent, transient1}));

    // Create the second transient.
    KWayland::Client::Surface *transient2Surface =
        Test::createSurface(Test::waylandCompositor());
    QVERIFY(transient2Surface);
    KWayland::Client::XdgShellSurface *transient2ShellSurface =
        Test::createXdgShellStableSurface(transient2Surface, transient2Surface);
    QVERIFY(transient2ShellSurface);
    transient2ShellSurface->setTransientFor(transient1ShellSurface);
    AbstractClient *transient2 = Test::renderAndWaitForShown(
        transient2Surface, QSize(128, 128), Qt::red);
    QVERIFY(transient2);
    QTRY_VERIFY(transient2->isActive());
    QVERIFY(transient2->isTransient());
    QCOMPARE(transient2->transientFor(), transient1);

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{parent, transient1, transient2}));

    // Activate the parent, both transients have to be above it.
    workspace()->activateClient(parent);
    QTRY_VERIFY(parent->isActive());
    QTRY_VERIFY(!transient1->isActive());
    QTRY_VERIFY(!transient2->isActive());

    // Close the top-most transient.
    connect(transient2, &AbstractClient::windowClosed, this,
        [](Toplevel *toplevel, Deleted *deleted) {
            Q_UNUSED(toplevel)
            deleted->refWindow();
        }
    );

    QSignalSpy windowClosedSpy(transient2, &AbstractClient::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    delete transient2ShellSurface;
    delete transient2Surface;
    QVERIFY(windowClosedSpy.wait());

    QScopedPointer<Deleted, WindowUnrefDeleter> deletedTransient(
        windowClosedSpy.first().at(1).value<Deleted *>());
    QVERIFY(deletedTransient.data());

    // The deleted transient still has to be above its old parent (transient1).
    QTRY_VERIFY(parent->isActive());
    QTRY_VERIFY(!transient1->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{parent, transient1, deletedTransient.data()}));
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
    X11Client *leader = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(leader);
    QVERIFY(leader->isActive());
    QCOMPARE(leader->windowId(), leaderWid);
    QVERIFY(!leader->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader}));

    // Create another group member.
    windowCreatedSpy.clear();
    xcb_window_t member1Wid = createGroupWindow(conn.data(), geometry, leaderWid);
    xcb_map_window(conn.data(), member1Wid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    X11Client *member1 = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(member1);
    QVERIFY(member1->isActive());
    QCOMPARE(member1->windowId(), member1Wid);
    QCOMPARE(member1->group(), leader->group());
    QVERIFY(!member1->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader, member1}));

    // Create yet another group member.
    windowCreatedSpy.clear();
    xcb_window_t member2Wid = createGroupWindow(conn.data(), geometry, leaderWid);
    xcb_map_window(conn.data(), member2Wid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    X11Client *member2 = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(member2);
    QVERIFY(member2->isActive());
    QCOMPARE(member2->windowId(), member2Wid);
    QCOMPARE(member2->group(), leader->group());
    QVERIFY(!member2->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader, member1, member2}));

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
    X11Client *transient = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(transient);
    QVERIFY(transient->isActive());
    QCOMPARE(transient->windowId(), transientWid);
    QCOMPARE(transient->group(), leader->group());
    QVERIFY(transient->isTransient());
    QVERIFY(transient->groupTransient());
    QVERIFY(!transient->isDialog()); // See above why

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader, member1, member2, transient}));

    // If we activate any member of the window group, the transient will be above it.
    workspace()->activateClient(leader);
    QTRY_VERIFY(leader->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{member1, member2, leader, transient}));

    workspace()->activateClient(member1);
    QTRY_VERIFY(member1->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{member2, leader, member1, transient}));

    workspace()->activateClient(member2);
    QTRY_VERIFY(member2->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader, member1, member2, transient}));

    workspace()->activateClient(transient);
    QTRY_VERIFY(transient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader, member1, member2, transient}));
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
    X11Client *leader = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(leader);
    QVERIFY(leader->isActive());
    QCOMPARE(leader->windowId(), leaderWid);
    QVERIFY(!leader->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader}));

    // Create another group member.
    windowCreatedSpy.clear();
    xcb_window_t member1Wid = createGroupWindow(conn.data(), geometry, leaderWid);
    xcb_map_window(conn.data(), member1Wid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    X11Client *member1 = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(member1);
    QVERIFY(member1->isActive());
    QCOMPARE(member1->windowId(), member1Wid);
    QCOMPARE(member1->group(), leader->group());
    QVERIFY(!member1->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader, member1}));

    // Create yet another group member.
    windowCreatedSpy.clear();
    xcb_window_t member2Wid = createGroupWindow(conn.data(), geometry, leaderWid);
    xcb_map_window(conn.data(), member2Wid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    X11Client *member2 = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(member2);
    QVERIFY(member2->isActive());
    QCOMPARE(member2->windowId(), member2Wid);
    QCOMPARE(member2->group(), leader->group());
    QVERIFY(!member2->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader, member1, member2}));

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
    X11Client *transient = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(transient);
    QVERIFY(transient->isActive());
    QCOMPARE(transient->windowId(), transientWid);
    QCOMPARE(transient->group(), leader->group());
    QVERIFY(transient->isTransient());
    QVERIFY(transient->groupTransient());
    QVERIFY(!transient->isDialog()); // See above why

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader, member1, member2, transient}));

    // Create a Wayland client that is not a member of the window group.
    KWayland::Client::Surface *anotherSurface =
        Test::createSurface(Test::waylandCompositor());
    QVERIFY(anotherSurface);
    KWayland::Client::XdgShellSurface *anotherShellSurface =
        Test::createXdgShellStableSurface(anotherSurface, anotherSurface);
    QVERIFY(anotherShellSurface);
    AbstractClient *anotherClient = Test::renderAndWaitForShown(anotherSurface, QSize(128, 128), Qt::green);
    QVERIFY(anotherClient);
    QVERIFY(anotherClient->isActive());
    QVERIFY(!anotherClient->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader, member1, member2, transient, anotherClient}));

    // If we activate the leader, then only it and the transient have to be raised.
    workspace()->activateClient(leader);
    QTRY_VERIFY(leader->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{member1, member2, anotherClient, leader, transient}));

    // If another member of the window group is activated, then the transient will
    // be above that member and the leader.
    workspace()->activateClient(member2);
    QTRY_VERIFY(member2->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{member1, anotherClient, leader, member2, transient}));

    // FIXME: If we activate the transient, only it will be raised.
    workspace()->activateClient(anotherClient);
    QTRY_VERIFY(anotherClient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{member1, leader, member2, transient, anotherClient}));

    workspace()->activateClient(transient);
    QTRY_VERIFY(transient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{member1, leader, member2, anotherClient, transient}));
}

void StackingOrderTest::testDeletedGroupTransient()
{
    // This test verifies that deleted group transients are kept above their
    // old window groups.

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
    X11Client *leader = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(leader);
    QVERIFY(leader->isActive());
    QCOMPARE(leader->windowId(), leaderWid);
    QVERIFY(!leader->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader}));

    // Create another group member.
    windowCreatedSpy.clear();
    xcb_window_t member1Wid = createGroupWindow(conn.data(), geometry, leaderWid);
    xcb_map_window(conn.data(), member1Wid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    X11Client *member1 = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(member1);
    QVERIFY(member1->isActive());
    QCOMPARE(member1->windowId(), member1Wid);
    QCOMPARE(member1->group(), leader->group());
    QVERIFY(!member1->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader, member1}));

    // Create yet another group member.
    windowCreatedSpy.clear();
    xcb_window_t member2Wid = createGroupWindow(conn.data(), geometry, leaderWid);
    xcb_map_window(conn.data(), member2Wid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    X11Client *member2 = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(member2);
    QVERIFY(member2->isActive());
    QCOMPARE(member2->windowId(), member2Wid);
    QCOMPARE(member2->group(), leader->group());
    QVERIFY(!member2->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader, member1, member2}));

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
    X11Client *transient = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(transient);
    QVERIFY(transient->isActive());
    QCOMPARE(transient->windowId(), transientWid);
    QCOMPARE(transient->group(), leader->group());
    QVERIFY(transient->isTransient());
    QVERIFY(transient->groupTransient());
    QVERIFY(!transient->isDialog()); // See above why

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader, member1, member2, transient}));

    // Unmap the transient.
    connect(transient, &X11Client::windowClosed, this,
        [](Toplevel *toplevel, Deleted *deleted) {
            Q_UNUSED(toplevel)
            deleted->refWindow();
        }
    );

    QSignalSpy windowClosedSpy(transient, &X11Client::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    xcb_unmap_window(conn.data(), transientWid);
    xcb_flush(conn.data());
    QVERIFY(windowClosedSpy.wait());

    QScopedPointer<Deleted, WindowUnrefDeleter> deletedTransient(
        windowClosedSpy.first().at(1).value<Deleted *>());
    QVERIFY(deletedTransient.data());

    // The transient has to be above each member of the window group.
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader, member1, member2, deletedTransient.data()}));
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
    X11Client *leader = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(leader);
    QVERIFY(leader->isActive());
    QCOMPARE(leader->windowId(), leaderWid);
    QVERIFY(!leader->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader}));

    // Create another group member.
    windowCreatedSpy.clear();
    xcb_window_t member1Wid = createGroupWindow(conn.data(), geometry, leaderWid);
    xcb_map_window(conn.data(), member1Wid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    X11Client *member1 = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(member1);
    QVERIFY(member1->isActive());
    QCOMPARE(member1->windowId(), member1Wid);
    QCOMPARE(member1->group(), leader->group());
    QVERIFY(!member1->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader, member1}));

    // Create yet another group member.
    windowCreatedSpy.clear();
    xcb_window_t member2Wid = createGroupWindow(conn.data(), geometry, leaderWid);
    xcb_map_window(conn.data(), member2Wid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    X11Client *member2 = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(member2);
    QVERIFY(member2->isActive());
    QCOMPARE(member2->windowId(), member2Wid);
    QCOMPARE(member2->group(), leader->group());
    QVERIFY(!member2->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader, member1, member2}));

    // Create a group transient.
    windowCreatedSpy.clear();
    xcb_window_t transientWid = createGroupWindow(conn.data(), geometry, leaderWid);
    xcb_icccm_set_wm_transient_for(conn.data(), transientWid, rootWindow());
    xcb_map_window(conn.data(), transientWid);
    xcb_flush(conn.data());

    QVERIFY(windowCreatedSpy.wait());
    X11Client *transient = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(transient);
    QVERIFY(transient->isActive());
    QCOMPARE(transient->windowId(), transientWid);
    QCOMPARE(transient->group(), leader->group());
    QVERIFY(transient->isTransient());
    QVERIFY(transient->groupTransient());
    QVERIFY(transient->isDialog());
    QVERIFY(!transient->isModal());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader, member1, member2, transient}));

    workspace()->activateClient(leader);
    QTRY_VERIFY(leader->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{member1, member2, transient, leader}));

    workspace()->activateClient(member1);
    QTRY_VERIFY(member1->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{member2, transient, leader, member1}));

    workspace()->activateClient(member2);
    QTRY_VERIFY(member2->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{transient, leader, member1, member2}));

    workspace()->activateClient(transient);
    QTRY_VERIFY(transient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{leader, member1, member2, transient}));
}

void StackingOrderTest::testKeepAbove()
{
    // This test verifies that "keep-above" windows are kept above other windows.

    // Create the first client.
    KWayland::Client::Surface *clientASurface =
        Test::createSurface(Test::waylandCompositor());
    QVERIFY(clientASurface);
    KWayland::Client::XdgShellSurface *clientAShellSurface =
        Test::createXdgShellStableSurface(clientASurface, clientASurface);
    QVERIFY(clientAShellSurface);
    AbstractClient *clientA = Test::renderAndWaitForShown(clientASurface, QSize(128, 128), Qt::green);
    QVERIFY(clientA);
    QVERIFY(clientA->isActive());
    QVERIFY(!clientA->keepAbove());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{clientA}));

    // Create the second client.
    KWayland::Client::Surface *clientBSurface =
        Test::createSurface(Test::waylandCompositor());
    QVERIFY(clientBSurface);
    KWayland::Client::XdgShellSurface *clientBShellSurface =
        Test::createXdgShellStableSurface(clientBSurface, clientBSurface);
    QVERIFY(clientBShellSurface);
    AbstractClient *clientB = Test::renderAndWaitForShown(clientBSurface, QSize(128, 128), Qt::green);
    QVERIFY(clientB);
    QVERIFY(clientB->isActive());
    QVERIFY(!clientB->keepAbove());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{clientA, clientB}));

    // Go to the initial test position.
    workspace()->activateClient(clientA);
    QTRY_VERIFY(clientA->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{clientB, clientA}));

    // Set the "keep-above" flag on the client B, it should go above other clients.
    {
        StackingUpdatesBlocker blocker(workspace());
        clientB->setKeepAbove(true);
    }

    QVERIFY(clientB->keepAbove());
    QVERIFY(!clientB->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{clientA, clientB}));
}

void StackingOrderTest::testKeepBelow()
{
    // This test verifies that "keep-below" windows are kept below other windows.

    // Create the first client.
    KWayland::Client::Surface *clientASurface =
        Test::createSurface(Test::waylandCompositor());
    QVERIFY(clientASurface);
    KWayland::Client::XdgShellSurface *clientAShellSurface =
        Test::createXdgShellStableSurface(clientASurface, clientASurface);
    QVERIFY(clientAShellSurface);
    AbstractClient *clientA = Test::renderAndWaitForShown(clientASurface, QSize(128, 128), Qt::green);
    QVERIFY(clientA);
    QVERIFY(clientA->isActive());
    QVERIFY(!clientA->keepBelow());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{clientA}));

    // Create the second client.
    KWayland::Client::Surface *clientBSurface =
        Test::createSurface(Test::waylandCompositor());
    QVERIFY(clientBSurface);
    KWayland::Client::XdgShellSurface *clientBShellSurface =
        Test::createXdgShellStableSurface(clientBSurface, clientBSurface);
    QVERIFY(clientBShellSurface);
    AbstractClient *clientB = Test::renderAndWaitForShown(clientBSurface, QSize(128, 128), Qt::green);
    QVERIFY(clientB);
    QVERIFY(clientB->isActive());
    QVERIFY(!clientB->keepBelow());

    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{clientA, clientB}));

    // Set the "keep-below" flag on the client B, it should go below other clients.
    {
        StackingUpdatesBlocker blocker(workspace());
        clientB->setKeepBelow(true);
    }

    QVERIFY(clientB->isActive());
    QVERIFY(clientB->keepBelow());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{clientB, clientA}));
}

WAYLANDTEST_MAIN(StackingOrderTest)
#include "stacking_order_test.moc"
