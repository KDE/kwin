/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "atoms.h"
#include "main.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include "x11window.h"

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

    void testPreserveRelativeWindowStacking();

    void testToggleRaiseLowerInSingleLayer();
    void testToggleRaiseLowerInMultipleLayers();
};

void StackingOrderTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();

    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
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
    std::unique_ptr<KWayland::Client::Surface> parentSurface = Test::createSurface();
    QVERIFY(parentSurface);
    std::unique_ptr<Test::XdgToplevel> parentShellSurface(Test::createXdgToplevelSurface(parentSurface.get()));
    QVERIFY(parentShellSurface);
    Window *parent = Test::renderAndWaitForShown(parentSurface.get(), QSize(256, 256), Qt::blue);
    QVERIFY(parent);
    QVERIFY(parent->isActive());
    QVERIFY(!parent->isTransient());

    // Initially, the stacking order should contain only the parent window.
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{parent}));

    // Create the transient.
    std::unique_ptr<KWayland::Client::Surface> transientSurface = Test::createSurface();
    QVERIFY(transientSurface);
    std::unique_ptr<Test::XdgToplevel> transientShellSurface(Test::createXdgToplevelSurface(transientSurface.get()));
    QVERIFY(transientShellSurface);
    transientShellSurface->set_parent(parentShellSurface->object());
    Window *transient = Test::renderAndWaitForShown(transientSurface.get(), QSize(128, 128), Qt::red);
    QVERIFY(transient);
    QVERIFY(transient->isActive());
    QVERIFY(transient->isTransient());

    // The transient should be above the parent.
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{parent, transient}));

    // The transient still stays above the parent if we activate the latter.
    workspace()->activateWindow(parent);
    QTRY_VERIFY(parent->isActive());
    QTRY_VERIFY(!transient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{parent, transient}));
}

void StackingOrderTest::testRaiseTransient()
{
    // This test verifies that both the parent and the transient will be
    // raised if either one of them is activated.

    // Create the parent.
    std::unique_ptr<KWayland::Client::Surface> parentSurface = Test::createSurface();
    QVERIFY(parentSurface);
    std::unique_ptr<Test::XdgToplevel> parentShellSurface(Test::createXdgToplevelSurface(parentSurface.get()));
    QVERIFY(parentShellSurface);
    Window *parent = Test::renderAndWaitForShown(parentSurface.get(), QSize(256, 256), Qt::blue);
    QVERIFY(parent);
    QVERIFY(parent->isActive());
    QVERIFY(!parent->isTransient());

    // Initially, the stacking order should contain only the parent window.
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{parent}));

    // Create the transient.
    std::unique_ptr<KWayland::Client::Surface> transientSurface = Test::createSurface();
    QVERIFY(transientSurface);
    std::unique_ptr<Test::XdgToplevel> transientShellSurface(Test::createXdgToplevelSurface(transientSurface.get()));
    QVERIFY(transientShellSurface);
    transientShellSurface->set_parent(parentShellSurface->object());
    Window *transient = Test::renderAndWaitForShown(transientSurface.get(), QSize(128, 128), Qt::red);
    QVERIFY(transient);
    QTRY_VERIFY(transient->isActive());
    QVERIFY(transient->isTransient());

    // The transient should be above the parent.
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{parent, transient}));

    // Create a window that doesn't have any relationship to the parent or the transient.
    std::unique_ptr<KWayland::Client::Surface> anotherSurface = Test::createSurface();
    QVERIFY(anotherSurface);
    std::unique_ptr<Test::XdgToplevel> anotherShellSurface(Test::createXdgToplevelSurface(anotherSurface.get()));
    QVERIFY(anotherShellSurface);
    Window *anotherWindow = Test::renderAndWaitForShown(anotherSurface.get(), QSize(128, 128), Qt::green);
    QVERIFY(anotherWindow);
    QVERIFY(anotherWindow->isActive());
    QVERIFY(!anotherWindow->isTransient());

    // The newly created surface has to be above both the parent and the transient.
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{parent, transient, anotherWindow}));

    // If we activate the parent, the transient should be raised too.
    workspace()->activateWindow(parent);
    QTRY_VERIFY(parent->isActive());
    QTRY_VERIFY(!transient->isActive());
    QTRY_VERIFY(!anotherWindow->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{anotherWindow, parent, transient}));

    // Go back to the initial setup.
    workspace()->activateWindow(anotherWindow);
    QTRY_VERIFY(!parent->isActive());
    QTRY_VERIFY(!transient->isActive());
    QTRY_VERIFY(anotherWindow->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{parent, transient, anotherWindow}));

    // If we activate the transient, the parent should be raised too.
    workspace()->activateWindow(transient);
    QTRY_VERIFY(!parent->isActive());
    QTRY_VERIFY(transient->isActive());
    QTRY_VERIFY(!anotherWindow->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{anotherWindow, parent, transient}));
}

struct WindowUnrefDeleter
{
    void operator()(Window *d)
    {
        if (d != nullptr) {
            d->unref();
        }
    }
};

void StackingOrderTest::testDeletedTransient()
{
    // This test verifies that deleted transients are kept above their
    // old parents.

    // Create the parent.
    std::unique_ptr<KWayland::Client::Surface> parentSurface = Test::createSurface();
    QVERIFY(parentSurface);
    std::unique_ptr<Test::XdgToplevel> parentShellSurface(Test::createXdgToplevelSurface(parentSurface.get()));
    QVERIFY(parentShellSurface);
    Window *parent = Test::renderAndWaitForShown(parentSurface.get(), QSize(256, 256), Qt::blue);
    QVERIFY(parent);
    QVERIFY(parent->isActive());
    QVERIFY(!parent->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{parent}));

    // Create the first transient.
    std::unique_ptr<KWayland::Client::Surface> transient1Surface = Test::createSurface();
    QVERIFY(transient1Surface);
    std::unique_ptr<Test::XdgToplevel> transient1ShellSurface(Test::createXdgToplevelSurface(transient1Surface.get()));
    QVERIFY(transient1ShellSurface);
    transient1ShellSurface->set_parent(parentShellSurface->object());
    Window *transient1 = Test::renderAndWaitForShown(transient1Surface.get(), QSize(128, 128), Qt::red);
    QVERIFY(transient1);
    QTRY_VERIFY(transient1->isActive());
    QVERIFY(transient1->isTransient());
    QCOMPARE(transient1->transientFor(), parent);

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{parent, transient1}));

    // Create the second transient.
    std::unique_ptr<KWayland::Client::Surface> transient2Surface = Test::createSurface();
    QVERIFY(transient2Surface);
    std::unique_ptr<Test::XdgToplevel> transient2ShellSurface(Test::createXdgToplevelSurface(transient2Surface.get()));
    QVERIFY(transient2ShellSurface);
    transient2ShellSurface->set_parent(transient1ShellSurface->object());
    Window *transient2 = Test::renderAndWaitForShown(transient2Surface.get(), QSize(128, 128), Qt::red);
    QVERIFY(transient2);
    QTRY_VERIFY(transient2->isActive());
    QVERIFY(transient2->isTransient());
    QCOMPARE(transient2->transientFor(), transient1);

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{parent, transient1, transient2}));

    // Activate the parent, both transients have to be above it.
    workspace()->activateWindow(parent);
    QTRY_VERIFY(parent->isActive());
    QTRY_VERIFY(!transient1->isActive());
    QTRY_VERIFY(!transient2->isActive());

    // Close the top-most transient.
    connect(transient2, &Window::closed, transient2, &Window::ref);
    auto cleanup = qScopeGuard([transient2]() {
        transient2->unref();
    });

    QSignalSpy windowClosedSpy(transient2, &Window::closed);
    transient2ShellSurface.reset();
    transient2Surface.reset();
    QVERIFY(windowClosedSpy.wait());

    // The deleted transient still has to be above its old parent (transient1).
    QTRY_VERIFY(parent->isActive());
    QTRY_VERIFY(!transient1->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{parent, transient1, transient2}));
}

static xcb_window_t createGroupWindow(xcb_connection_t *conn,
                                      const QRect &geometry,
                                      xcb_window_t leaderWid = XCB_WINDOW_NONE)
{
    xcb_window_t wid = xcb_generate_id(conn);
    xcb_create_window(
        conn, // c
        XCB_COPY_FROM_PARENT, // depth
        wid, // wid
        rootWindow(), // parent
        geometry.x(), // x
        geometry.y(), // y
        geometry.width(), // width
        geometry.height(), // height
        0, // border_width
        XCB_WINDOW_CLASS_INPUT_OUTPUT, // _class
        XCB_COPY_FROM_PARENT, // visual
        0, // value_mask
        nullptr // value_list
    );

    xcb_size_hints_t sizeHints = {};
    xcb_icccm_size_hints_set_position(&sizeHints, 1, geometry.x(), geometry.y());
    xcb_icccm_size_hints_set_size(&sizeHints, 1, geometry.width(), geometry.height());
    xcb_icccm_set_wm_normal_hints(conn, wid, &sizeHints);

    if (leaderWid == XCB_WINDOW_NONE) {
        leaderWid = wid;
    }

    xcb_change_property(
        conn, // c
        XCB_PROP_MODE_REPLACE, // mode
        wid, // window
        atoms->wm_client_leader, // property
        XCB_ATOM_WINDOW, // type
        32, // format
        1, // data_len
        &leaderWid // data
    );

    return wid;
}

void StackingOrderTest::testGroupTransientIsAboveWindowGroup()
{
    // This test verifies that group transients are always above other
    // window group members.

    const QRect geometry = QRect(0, 0, 128, 128);

    Test::XcbConnectionPtr conn = Test::createX11Connection();

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);

    // Create the group leader.
    xcb_window_t leaderWid = createGroupWindow(conn.get(), geometry);
    xcb_map_window(conn.get(), leaderWid);
    xcb_flush(conn.get());

    QVERIFY(windowCreatedSpy.wait());
    X11Window *leader = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(leader);
    QVERIFY(leader->isActive());
    QCOMPARE(leader->window(), leaderWid);
    QVERIFY(!leader->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader}));

    // Create another group member.
    windowCreatedSpy.clear();
    xcb_window_t member1Wid = createGroupWindow(conn.get(), geometry, leaderWid);
    xcb_map_window(conn.get(), member1Wid);
    xcb_flush(conn.get());

    QVERIFY(windowCreatedSpy.wait());
    X11Window *member1 = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(member1);
    QVERIFY(member1->isActive());
    QCOMPARE(member1->window(), member1Wid);
    QCOMPARE(member1->group(), leader->group());
    QVERIFY(!member1->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader, member1}));

    // Create yet another group member.
    windowCreatedSpy.clear();
    xcb_window_t member2Wid = createGroupWindow(conn.get(), geometry, leaderWid);
    xcb_map_window(conn.get(), member2Wid);
    xcb_flush(conn.get());

    QVERIFY(windowCreatedSpy.wait());
    X11Window *member2 = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(member2);
    QVERIFY(member2->isActive());
    QCOMPARE(member2->window(), member2Wid);
    QCOMPARE(member2->group(), leader->group());
    QVERIFY(!member2->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader, member1, member2}));

    // Create a group transient.
    windowCreatedSpy.clear();
    xcb_window_t transientWid = createGroupWindow(conn.get(), geometry, leaderWid);
    xcb_icccm_set_wm_transient_for(conn.get(), transientWid, rootWindow());

    // Currently, we have some weird bug workaround: if a group transient
    // is a non-modal dialog, then it won't be kept above its window group.
    // We need to explicitly specify window type, otherwise the window type
    // will be deduced to _NET_WM_WINDOW_TYPE_DIALOG because we set transient
    // for before (the EWMH spec says to do that).
    xcb_atom_t net_wm_window_type = Xcb::Atom(
        QByteArrayLiteral("_NET_WM_WINDOW_TYPE"), false, conn.get());
    xcb_atom_t net_wm_window_type_normal = Xcb::Atom(
        QByteArrayLiteral("_NET_WM_WINDOW_TYPE_NORMAL"), false, conn.get());
    xcb_change_property(
        conn.get(), // c
        XCB_PROP_MODE_REPLACE, // mode
        transientWid, // window
        net_wm_window_type, // property
        XCB_ATOM_ATOM, // type
        32, // format
        1, // data_len
        &net_wm_window_type_normal // data
    );

    xcb_map_window(conn.get(), transientWid);
    xcb_flush(conn.get());

    QVERIFY(windowCreatedSpy.wait());
    X11Window *transient = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(transient);
    QVERIFY(transient->isActive());
    QCOMPARE(transient->window(), transientWid);
    QCOMPARE(transient->group(), leader->group());
    QVERIFY(transient->isTransient());
    QVERIFY(transient->groupTransient());
    QVERIFY(!transient->isDialog()); // See above why

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader, member1, member2, transient}));

    // If we activate any member of the window group, the transient will be above it.
    workspace()->activateWindow(leader);
    QTRY_VERIFY(leader->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{member1, member2, leader, transient}));

    workspace()->activateWindow(member1);
    QTRY_VERIFY(member1->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{member2, leader, member1, transient}));

    workspace()->activateWindow(member2);
    QTRY_VERIFY(member2->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader, member1, member2, transient}));

    workspace()->activateWindow(transient);
    QTRY_VERIFY(transient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader, member1, member2, transient}));
}

void StackingOrderTest::testRaiseGroupTransient()
{
    const QRect geometry = QRect(0, 0, 128, 128);

    Test::XcbConnectionPtr conn = Test::createX11Connection();

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);

    // Create the group leader.
    xcb_window_t leaderWid = createGroupWindow(conn.get(), geometry);
    xcb_map_window(conn.get(), leaderWid);
    xcb_flush(conn.get());

    QVERIFY(windowCreatedSpy.wait());
    X11Window *leader = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(leader);
    QVERIFY(leader->isActive());
    QCOMPARE(leader->window(), leaderWid);
    QVERIFY(!leader->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader}));

    // Create another group member.
    windowCreatedSpy.clear();
    xcb_window_t member1Wid = createGroupWindow(conn.get(), geometry, leaderWid);
    xcb_map_window(conn.get(), member1Wid);
    xcb_flush(conn.get());

    QVERIFY(windowCreatedSpy.wait());
    X11Window *member1 = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(member1);
    QVERIFY(member1->isActive());
    QCOMPARE(member1->window(), member1Wid);
    QCOMPARE(member1->group(), leader->group());
    QVERIFY(!member1->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader, member1}));

    // Create yet another group member.
    windowCreatedSpy.clear();
    xcb_window_t member2Wid = createGroupWindow(conn.get(), geometry, leaderWid);
    xcb_map_window(conn.get(), member2Wid);
    xcb_flush(conn.get());

    QVERIFY(windowCreatedSpy.wait());
    X11Window *member2 = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(member2);
    QVERIFY(member2->isActive());
    QCOMPARE(member2->window(), member2Wid);
    QCOMPARE(member2->group(), leader->group());
    QVERIFY(!member2->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader, member1, member2}));

    // Create a group transient.
    windowCreatedSpy.clear();
    xcb_window_t transientWid = createGroupWindow(conn.get(), geometry, leaderWid);
    xcb_icccm_set_wm_transient_for(conn.get(), transientWid, rootWindow());

    // Currently, we have some weird bug workaround: if a group transient
    // is a non-modal dialog, then it won't be kept above its window group.
    // We need to explicitly specify window type, otherwise the window type
    // will be deduced to _NET_WM_WINDOW_TYPE_DIALOG because we set transient
    // for before (the EWMH spec says to do that).
    xcb_atom_t net_wm_window_type = Xcb::Atom(
        QByteArrayLiteral("_NET_WM_WINDOW_TYPE"), false, conn.get());
    xcb_atom_t net_wm_window_type_normal = Xcb::Atom(
        QByteArrayLiteral("_NET_WM_WINDOW_TYPE_NORMAL"), false, conn.get());
    xcb_change_property(
        conn.get(), // c
        XCB_PROP_MODE_REPLACE, // mode
        transientWid, // window
        net_wm_window_type, // property
        XCB_ATOM_ATOM, // type
        32, // format
        1, // data_len
        &net_wm_window_type_normal // data
    );

    xcb_map_window(conn.get(), transientWid);
    xcb_flush(conn.get());

    QVERIFY(windowCreatedSpy.wait());
    X11Window *transient = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(transient);
    QVERIFY(transient->isActive());
    QCOMPARE(transient->window(), transientWid);
    QCOMPARE(transient->group(), leader->group());
    QVERIFY(transient->isTransient());
    QVERIFY(transient->groupTransient());
    QVERIFY(!transient->isDialog()); // See above why

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader, member1, member2, transient}));

    // Create a Wayland window that is not a member of the window group.
    std::unique_ptr<KWayland::Client::Surface> anotherSurface = Test::createSurface();
    QVERIFY(anotherSurface);
    std::unique_ptr<Test::XdgToplevel> anotherShellSurface(Test::createXdgToplevelSurface(anotherSurface.get()));
    QVERIFY(anotherShellSurface);
    Window *anotherWindow = Test::renderAndWaitForShown(anotherSurface.get(), QSize(128, 128), Qt::green);
    QVERIFY(anotherWindow);
    QVERIFY(anotherWindow->isActive());
    QVERIFY(!anotherWindow->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader, member1, member2, transient, anotherWindow}));

    // If we activate the leader, then only it and the transient have to be raised.
    workspace()->activateWindow(leader);
    QTRY_VERIFY(leader->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{member1, member2, anotherWindow, leader, transient}));

    // If another member of the window group is activated, then the transient will
    // be above that member and the leader.
    workspace()->activateWindow(member2);
    QTRY_VERIFY(member2->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{member1, anotherWindow, leader, member2, transient}));

    // FIXME: If we activate the transient, only it will be raised.
    workspace()->activateWindow(anotherWindow);
    QTRY_VERIFY(anotherWindow->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{member1, leader, member2, transient, anotherWindow}));

    workspace()->activateWindow(transient);
    QTRY_VERIFY(transient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{member1, leader, member2, anotherWindow, transient}));
}

void StackingOrderTest::testDeletedGroupTransient()
{
    // This test verifies that deleted group transients are kept above their
    // old window groups.

    const QRect geometry = QRect(0, 0, 128, 128);

    Test::XcbConnectionPtr conn = Test::createX11Connection();

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);

    // Create the group leader.
    xcb_window_t leaderWid = createGroupWindow(conn.get(), geometry);
    xcb_map_window(conn.get(), leaderWid);
    xcb_flush(conn.get());

    QVERIFY(windowCreatedSpy.wait());
    X11Window *leader = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(leader);
    QVERIFY(leader->isActive());
    QCOMPARE(leader->window(), leaderWid);
    QVERIFY(!leader->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader}));

    // Create another group member.
    windowCreatedSpy.clear();
    xcb_window_t member1Wid = createGroupWindow(conn.get(), geometry, leaderWid);
    xcb_map_window(conn.get(), member1Wid);
    xcb_flush(conn.get());

    QVERIFY(windowCreatedSpy.wait());
    X11Window *member1 = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(member1);
    QVERIFY(member1->isActive());
    QCOMPARE(member1->window(), member1Wid);
    QCOMPARE(member1->group(), leader->group());
    QVERIFY(!member1->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader, member1}));

    // Create yet another group member.
    windowCreatedSpy.clear();
    xcb_window_t member2Wid = createGroupWindow(conn.get(), geometry, leaderWid);
    xcb_map_window(conn.get(), member2Wid);
    xcb_flush(conn.get());

    QVERIFY(windowCreatedSpy.wait());
    X11Window *member2 = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(member2);
    QVERIFY(member2->isActive());
    QCOMPARE(member2->window(), member2Wid);
    QCOMPARE(member2->group(), leader->group());
    QVERIFY(!member2->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader, member1, member2}));

    // Create a group transient.
    windowCreatedSpy.clear();
    xcb_window_t transientWid = createGroupWindow(conn.get(), geometry, leaderWid);
    xcb_icccm_set_wm_transient_for(conn.get(), transientWid, rootWindow());

    // Currently, we have some weird bug workaround: if a group transient
    // is a non-modal dialog, then it won't be kept above its window group.
    // We need to explicitly specify window type, otherwise the window type
    // will be deduced to _NET_WM_WINDOW_TYPE_DIALOG because we set transient
    // for before (the EWMH spec says to do that).
    xcb_atom_t net_wm_window_type = Xcb::Atom(
        QByteArrayLiteral("_NET_WM_WINDOW_TYPE"), false, conn.get());
    xcb_atom_t net_wm_window_type_normal = Xcb::Atom(
        QByteArrayLiteral("_NET_WM_WINDOW_TYPE_NORMAL"), false, conn.get());
    xcb_change_property(
        conn.get(), // c
        XCB_PROP_MODE_REPLACE, // mode
        transientWid, // window
        net_wm_window_type, // property
        XCB_ATOM_ATOM, // type
        32, // format
        1, // data_len
        &net_wm_window_type_normal // data
    );

    xcb_map_window(conn.get(), transientWid);
    xcb_flush(conn.get());

    QVERIFY(windowCreatedSpy.wait());
    X11Window *transient = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(transient);
    QVERIFY(transient->isActive());
    QCOMPARE(transient->window(), transientWid);
    QCOMPARE(transient->group(), leader->group());
    QVERIFY(transient->isTransient());
    QVERIFY(transient->groupTransient());
    QVERIFY(!transient->isDialog()); // See above why

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader, member1, member2, transient}));

    // Unmap the transient.
    connect(transient, &Window::closed, transient, &Window::ref);
    auto cleanup = qScopeGuard([transient]() {
        transient->unref();
    });

    QSignalSpy windowClosedSpy(transient, &X11Window::closed);
    xcb_unmap_window(conn.get(), transientWid);
    xcb_flush(conn.get());
    QVERIFY(windowClosedSpy.wait());

    // The transient has to be above each member of the window group.
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader, member1, member2, transient}));
}

void StackingOrderTest::testDontKeepAboveNonModalDialogGroupTransients()
{
    // Bug 76026

    const QRect geometry = QRect(0, 0, 128, 128);

    Test::XcbConnectionPtr conn = Test::createX11Connection();

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);

    // Create the group leader.
    xcb_window_t leaderWid = createGroupWindow(conn.get(), geometry);
    xcb_map_window(conn.get(), leaderWid);
    xcb_flush(conn.get());

    QVERIFY(windowCreatedSpy.wait());
    X11Window *leader = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(leader);
    QVERIFY(leader->isActive());
    QCOMPARE(leader->window(), leaderWid);
    QVERIFY(!leader->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader}));

    // Create another group member.
    windowCreatedSpy.clear();
    xcb_window_t member1Wid = createGroupWindow(conn.get(), geometry, leaderWid);
    xcb_map_window(conn.get(), member1Wid);
    xcb_flush(conn.get());

    QVERIFY(windowCreatedSpy.wait());
    X11Window *member1 = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(member1);
    QVERIFY(member1->isActive());
    QCOMPARE(member1->window(), member1Wid);
    QCOMPARE(member1->group(), leader->group());
    QVERIFY(!member1->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader, member1}));

    // Create yet another group member.
    windowCreatedSpy.clear();
    xcb_window_t member2Wid = createGroupWindow(conn.get(), geometry, leaderWid);
    xcb_map_window(conn.get(), member2Wid);
    xcb_flush(conn.get());

    QVERIFY(windowCreatedSpy.wait());
    X11Window *member2 = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(member2);
    QVERIFY(member2->isActive());
    QCOMPARE(member2->window(), member2Wid);
    QCOMPARE(member2->group(), leader->group());
    QVERIFY(!member2->isTransient());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader, member1, member2}));

    // Create a group transient.
    windowCreatedSpy.clear();
    xcb_window_t transientWid = createGroupWindow(conn.get(), geometry, leaderWid);
    xcb_icccm_set_wm_transient_for(conn.get(), transientWid, rootWindow());
    xcb_map_window(conn.get(), transientWid);
    xcb_flush(conn.get());

    QVERIFY(windowCreatedSpy.wait());
    X11Window *transient = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(transient);
    QVERIFY(transient->isActive());
    QCOMPARE(transient->window(), transientWid);
    QCOMPARE(transient->group(), leader->group());
    QVERIFY(transient->isTransient());
    QVERIFY(transient->groupTransient());
    QVERIFY(transient->isDialog());
    QVERIFY(!transient->isModal());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader, member1, member2, transient}));

    workspace()->activateWindow(leader);
    QTRY_VERIFY(leader->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{member1, member2, transient, leader}));

    workspace()->activateWindow(member1);
    QTRY_VERIFY(member1->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{member2, transient, leader, member1}));

    workspace()->activateWindow(member2);
    QTRY_VERIFY(member2->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{transient, leader, member1, member2}));

    workspace()->activateWindow(transient);
    QTRY_VERIFY(transient->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{leader, member1, member2, transient}));
}

void StackingOrderTest::testKeepAbove()
{
    // This test verifies that "keep-above" windows are kept above other windows.

    // Create the first window.
    std::unique_ptr<KWayland::Client::Surface> surface1 = Test::createSurface();
    QVERIFY(surface1);
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    QVERIFY(shellSurface1);
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(128, 128), Qt::green);
    QVERIFY(window1);
    QVERIFY(window1->isActive());
    QVERIFY(!window1->keepAbove());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1}));

    // Create the second window.
    std::unique_ptr<KWayland::Client::Surface> surface2 = Test::createSurface();
    QVERIFY(surface2);
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    QVERIFY(shellSurface2);
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(128, 128), Qt::green);
    QVERIFY(window2);
    QVERIFY(window2->isActive());
    QVERIFY(!window2->keepAbove());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1, window2}));

    // Go to the initial test position.
    workspace()->activateWindow(window1);
    QTRY_VERIFY(window1->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window2, window1}));

    // Set the "keep-above" flag on the window2, it should go above other windows.
    {
        StackingUpdatesBlocker blocker(workspace());
        window2->setKeepAbove(true);
    }

    QVERIFY(window2->keepAbove());
    QVERIFY(!window2->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1, window2}));
}

void StackingOrderTest::testKeepBelow()
{
    // This test verifies that "keep-below" windows are kept below other windows.

    // Create the first window.
    std::unique_ptr<KWayland::Client::Surface> surface1 = Test::createSurface();
    QVERIFY(surface1);
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    QVERIFY(shellSurface1);
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(128, 128), Qt::green);
    QVERIFY(window1);
    QVERIFY(window1->isActive());
    QVERIFY(!window1->keepBelow());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1}));

    // Create the second window.
    std::unique_ptr<KWayland::Client::Surface> surface2 = Test::createSurface();
    QVERIFY(surface2);
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    QVERIFY(shellSurface2);
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(128, 128), Qt::green);
    QVERIFY(window2);
    QVERIFY(window2->isActive());
    QVERIFY(!window2->keepBelow());

    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window1, window2}));

    // Set the "keep-below" flag on the window2, it should go below other windows.
    {
        StackingUpdatesBlocker blocker(workspace());
        window2->setKeepBelow(true);
    }

    QVERIFY(window2->isActive());
    QVERIFY(window2->keepBelow());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{window2, window1}));
}

void StackingOrderTest::testPreserveRelativeWindowStacking()
{
    // This test verifies that raising a window doesn't affect the order of transient windows that are constrained
    // to be above it, see BUG: 477262

    const int windowsQuantity = 5;

    std::unique_ptr<KWayland::Client::Surface> surfaces[windowsQuantity];
    std::unique_ptr<Test::XdgToplevel> shellSurfaces[windowsQuantity];
    Window *windows[windowsQuantity];

    // Create 5 windows.
    for (int i = 0; i < windowsQuantity; i++) {
        surfaces[i] = Test::createSurface();
        QVERIFY(surfaces[i]);
        shellSurfaces[i] = Test::createXdgToplevelSurface(surfaces[i].get());
        QVERIFY(shellSurfaces[i]);
    }

    // link them into the following hierarchy:
    //      * 0 - parent to all
    //      * 1, 2, 3 - children of 0
    //      * 4 - child of 3
    shellSurfaces[1]->set_parent(shellSurfaces[0]->object());
    shellSurfaces[2]->set_parent(shellSurfaces[0]->object());
    shellSurfaces[3]->set_parent(shellSurfaces[0]->object());
    shellSurfaces[4]->set_parent(shellSurfaces[3]->object());

    for (int i = 0; i < windowsQuantity; i++) {
        windows[i] = Test::renderAndWaitForShown(surfaces[i].get(), QSize(128, 128), Qt::green);
        QVERIFY(windows[i]);
    }

    // verify initial windows order
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[0], windows[1], windows[2], windows[3], windows[4]}));

    // activate parent
    workspace()->activateWindow(windows[0]);
    // verify that order hasn't changed
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[0], windows[1], windows[2], windows[3], windows[4]}));

    // change stacking order
    workspace()->activateWindow(windows[2]);
    workspace()->activateWindow(windows[1]);
    // verify order
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[0], windows[3], windows[4], windows[2], windows[1]}));

    // activate parent
    workspace()->activateWindow(windows[0]);
    // verify that order hasn't changed
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[0], windows[3], windows[4], windows[2], windows[1]}));

    // activate child 3
    workspace()->activateWindow(windows[3]);
    // verify that both child 3 and 4 have been raised
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[0], windows[2], windows[1], windows[3], windows[4]}));

    // activate parent
    workspace()->activateWindow(windows[0]);
    // verify that order hasn't changed
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[0], windows[2], windows[1], windows[3], windows[4]}));

    // yet another check - add KeepAbove attribute to parent window (see BUG: 477262)
    windows[0]->setKeepAbove(true);
    // verify that order hasn't changed
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[0], windows[2], windows[1], windows[3], windows[4]}));
    // verify that child windows can still be restacked freely
    workspace()->activateWindow(windows[1]);
    workspace()->activateWindow(windows[2]);
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[0], windows[3], windows[4], windows[1], windows[2]}));
}

void StackingOrderTest::testToggleRaiseLowerInSingleLayer()
{
    // This test verifies that Toggle Raise Lower causes proper action (lowering or raising) to top-most,
    // bottom-most and middle-stacked windows, as well as this action doesn't transfer focus

    const int windowCount = 3;

    std::unique_ptr<KWayland::Client::Surface> surfaces[windowCount];
    std::unique_ptr<Test::XdgToplevel> shellSurfaces[windowCount];
    Window *windows[windowCount];

    // Create 3 windows.
    for (int i = 0; i < windowCount; i++) {
        surfaces[i] = Test::createSurface();
        QVERIFY(surfaces[i]);
        shellSurfaces[i] = Test::createXdgToplevelSurface(surfaces[i].get());
        QVERIFY(shellSurfaces[i]);
        windows[i] = Test::renderAndWaitForShown(surfaces[i].get(), QSize(128, 128), Qt::green);
        QVERIFY(windows[i]);
    }
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[0], windows[1], windows[2]}));

    // Activate window 0, verify effects.
    workspace()->activateWindow(windows[0]);
    QVERIFY(windows[0]->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[1], windows[2], windows[0]}));

    // TR&L (Toggle Raise Lower) top-most window 0.
    workspace()->raiseOrLowerWindow(windows[0]);

    // Verify that window 0 has been lowered and still has focus.
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[0], windows[1], windows[2]}));
    QVERIFY(windows[0]->isActive());

    // TR&L window 0, which is now bottom-most.
    workspace()->raiseOrLowerWindow(windows[0]);

    // verify that window 0 has been raised and still has focus.
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[1], windows[2], windows[0]}));
    QVERIFY(windows[0]->isActive());

    // TR&L window 2, which is in the middle.
    workspace()->raiseOrLowerWindow(windows[2]);
    // verify that it got raised and focus stays with window 0.
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[1], windows[0], windows[2]}));
    QVERIFY(!windows[2]->isActive());
    QVERIFY(windows[0]->isActive());

    // TR&L window 2, which is top-most now, but doesn't have focus.
    workspace()->raiseOrLowerWindow(windows[2]);
    // verify that it got lower and focus stays with window 0.
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[2], windows[1], windows[0]}));
    QVERIFY(!windows[2]->isActive());
    QVERIFY(windows[0]->isActive());
}

void StackingOrderTest::testToggleRaiseLowerInMultipleLayers()
{
    // This test verifies that Toggle Raise & Lower works independently within each window layer

    const int windowCount = 9;

    std::unique_ptr<KWayland::Client::Surface> surfaces[windowCount];
    std::unique_ptr<Test::XdgToplevel> shellSurfaces[windowCount];
    Window *windows[windowCount];

    // Create 9 windows, place them in 3 layers, move to the same position, so they overlap each other.
    for (int i = 0; i < windowCount; i++) {
        surfaces[i] = Test::createSurface();
        QVERIFY(surfaces[i]);
        shellSurfaces[i] = Test::createXdgToplevelSurface(surfaces[i].get());
        QVERIFY(shellSurfaces[i]);
        windows[i] = Test::renderAndWaitForShown(surfaces[i].get(), QSize(128, 128), Qt::green);
        QVERIFY(windows[i]);
        windows[i]->move(QPoint(0, 0));
    }
    windows[0]->setKeepBelow(true);
    windows[1]->setKeepBelow(true);
    windows[2]->setKeepBelow(true);
    windows[6]->setKeepAbove(true);
    windows[7]->setKeepAbove(true);
    windows[8]->setKeepAbove(true);

    // Verify initial stacking order
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[0], windows[1], windows[2],
                                                            windows[3], windows[4], windows[5],
                                                            windows[6], windows[7], windows[8]}));

    // TR&L window 0 (lowest in BelowLayer), verify that it got raised within its layer
    workspace()->raiseOrLowerWindow(windows[0]);
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[1], windows[2], windows[0],
                                                            windows[3], windows[4], windows[5],
                                                            windows[6], windows[7], windows[8]}));

    // TR&L window 0, verify that it got lowered within its layer
    workspace()->raiseOrLowerWindow(windows[0]);
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[0], windows[1], windows[2],
                                                            windows[3], windows[4], windows[5],
                                                            windows[6], windows[7], windows[8]}));

    // TR&L window 4 (middle in NormalLayer), verify that it got raised within its layer
    workspace()->raiseOrLowerWindow(windows[4]);
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[0], windows[1], windows[2],
                                                            windows[3], windows[5], windows[4],
                                                            windows[6], windows[7], windows[8]}));

    // TR&L window 4, verify that it got lowered within NormalLayer
    workspace()->raiseOrLowerWindow(windows[4]);
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[0], windows[1], windows[2],
                                                            windows[4], windows[3], windows[5],
                                                            windows[6], windows[7], windows[8]}));

    // TR&L window 8 (topmost in AboveLayer), verify that it got lowered within its layer
    workspace()->raiseOrLowerWindow(windows[8]);
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[0], windows[1], windows[2],
                                                            windows[4], windows[3], windows[5],
                                                            windows[8], windows[6], windows[7]}));

    // TR&L window 8, verify that it got raised within its layer
    workspace()->raiseOrLowerWindow(windows[8]);
    QCOMPARE(workspace()->stackingOrder(), (QList<Window *>{windows[0], windows[1], windows[2],
                                                            windows[4], windows[3], windows[5],
                                                            windows[6], windows[7], windows[8]}));
}

WAYLANDTEST_MAIN(StackingOrderTest)
#include "stacking_order_test.moc"
