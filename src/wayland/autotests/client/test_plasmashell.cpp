/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWayland
#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/plasmashell_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/plasmashell.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/surface.h"

using namespace KWaylandServer;

class TestPlasmaShell : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testRole_data();
    void testRole();
    void testPosition();
    void testSkipTaskbar();
    void testSkipSwitcher();
    void testPanelBehavior_data();
    void testPanelBehavior();
    void testAutoHidePanel();
    void testPanelTakesFocus();
    void testDisconnect();
    void testWhileDestroying();

private:
    KWaylandServer::Display *m_display = nullptr;
    CompositorInterface *m_compositorInterface = nullptr;
    PlasmaShellInterface *m_plasmaShellInterface = nullptr;

    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    QThread *m_thread = nullptr;
    KWayland::Client::Registry *m_registry = nullptr;
    KWayland::Client::PlasmaShell *m_plasmaShell = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-plasma-shell-0");

void TestPlasmaShell::init()
{
    delete m_display;
    m_display = new KWaylandServer::Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    m_display->createShm();

    m_plasmaShellInterface = new PlasmaShellInterface(m_display, m_display);

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new KWayland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    m_registry = new KWayland::Client::Registry();
    QSignalSpy interfacesAnnouncedSpy(m_registry, &KWayland::Client::Registry::interfaceAnnounced);

    QVERIFY(!m_registry->eventQueue());
    m_registry->setEventQueue(m_queue);
    QCOMPARE(m_registry->eventQueue(), m_queue);
    m_registry->create(m_connection);
    QVERIFY(m_registry->isValid());
    m_registry->setup();

    QVERIFY(interfacesAnnouncedSpy.wait());
#define CREATE(variable, factory, iface)                                                                                                                      \
    variable =                                                                                                                                                \
        m_registry->create##factory(m_registry->interface(KWayland::Client::Registry::Interface::iface).name, m_registry->interface(KWayland::Client::Registry::Interface::iface).version, this); \
    QVERIFY(variable);

    CREATE(m_compositor, Compositor, Compositor)
    CREATE(m_plasmaShell, PlasmaShell, PlasmaShell)

#undef CREATE
}

void TestPlasmaShell::cleanup()
{
#define DELETE(name)    \
    if (name) {         \
        delete name;    \
        name = nullptr; \
    }
    DELETE(m_plasmaShell)
    DELETE(m_compositor)
    DELETE(m_queue)
    DELETE(m_registry)
#undef DELETE
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    delete m_connection;
    m_connection = nullptr;

    delete m_display;
    m_display = nullptr;
}

void TestPlasmaShell::testRole_data()
{
    QTest::addColumn<KWayland::Client::PlasmaShellSurface::Role>("clientRole");
    QTest::addColumn<KWaylandServer::PlasmaShellSurfaceInterface::Role>("serverRole");

    QTest::newRow("desktop") << KWayland::Client::PlasmaShellSurface::Role::Desktop << PlasmaShellSurfaceInterface::Role::Desktop;
    QTest::newRow("osd") << KWayland::Client::PlasmaShellSurface::Role::OnScreenDisplay << PlasmaShellSurfaceInterface::Role::OnScreenDisplay;
    QTest::newRow("panel") << KWayland::Client::PlasmaShellSurface::Role::Panel << PlasmaShellSurfaceInterface::Role::Panel;
    QTest::newRow("notification") << KWayland::Client::PlasmaShellSurface::Role::Notification << PlasmaShellSurfaceInterface::Role::Notification;
    QTest::newRow("tooltip") << KWayland::Client::PlasmaShellSurface::Role::ToolTip << PlasmaShellSurfaceInterface::Role::ToolTip;
    QTest::newRow("criticalnotification") << KWayland::Client::PlasmaShellSurface::Role::CriticalNotification << PlasmaShellSurfaceInterface::Role::CriticalNotification;
    QTest::newRow("appletPopup") << KWayland::Client::PlasmaShellSurface::Role::AppletPopup << PlasmaShellSurfaceInterface::Role::AppletPopup;
}

void TestPlasmaShell::testRole()
{
    // this test verifies that setting the role on a plasma shell surface works

    // first create signal spies
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QSignalSpy plasmaSurfaceCreatedSpy(m_plasmaShellInterface, &PlasmaShellInterface::surfaceCreated);

    // create the surface
    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    // no PlasmaShellSurface for the Surface yet yet
    QVERIFY(!KWayland::Client::PlasmaShellSurface::get(s.get()));
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.get()));
    QCOMPARE(ps->role(), KWayland::Client::PlasmaShellSurface::Role::Normal);
    // now we should have a PlasmaShellSurface for
    QCOMPARE(KWayland::Client::PlasmaShellSurface::get(s.get()), ps.get());

    // try to create another PlasmaShellSurface for the same Surface, should return from cache
    QCOMPARE(m_plasmaShell->createSurface(s.get()), ps.get());

    // and get them on the server
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);
    QCOMPARE(surfaceCreatedSpy.count(), 1);

    // verify that we got a plasma shell surface
    auto sps = plasmaSurfaceCreatedSpy.first().first().value<PlasmaShellSurfaceInterface *>();
    QVERIFY(sps);
    QVERIFY(sps->surface());
    QCOMPARE(sps->surface(), surfaceCreatedSpy.first().first().value<SurfaceInterface *>());

    // default role should be normal
    QCOMPARE(sps->role(), PlasmaShellSurfaceInterface::Role::Normal);

    // now change it
    QSignalSpy roleChangedSpy(sps, &PlasmaShellSurfaceInterface::roleChanged);
    QFETCH(KWayland::Client::PlasmaShellSurface::Role, clientRole);
    ps->setRole(clientRole);
    QCOMPARE(ps->role(), clientRole);
    QVERIFY(roleChangedSpy.wait());
    QCOMPARE(roleChangedSpy.count(), 1);
    QTEST(sps->role(), "serverRole");

    // try changing again should not emit the signal
    ps->setRole(clientRole);
    QVERIFY(!roleChangedSpy.wait(100));

    // set role back to normal
    ps->setRole(KWayland::Client::PlasmaShellSurface::Role::Normal);
    QCOMPARE(ps->role(), KWayland::Client::PlasmaShellSurface::Role::Normal);
    QVERIFY(roleChangedSpy.wait());
    QCOMPARE(roleChangedSpy.count(), 2);
    QCOMPARE(sps->role(), PlasmaShellSurfaceInterface::Role::Normal);
}

void TestPlasmaShell::testPosition()
{
    // this test verifies that updating the position of a PlasmaShellSurface is properly passed to the server
    QSignalSpy plasmaSurfaceCreatedSpy(m_plasmaShellInterface, &PlasmaShellInterface::surfaceCreated);

    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.get()));
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);

    // verify that we got a plasma shell surface
    auto sps = plasmaSurfaceCreatedSpy.first().first().value<PlasmaShellSurfaceInterface *>();
    QVERIFY(sps);
    QVERIFY(sps->surface());

    // default position should not be set
    QVERIFY(!sps->isPositionSet());
    QCOMPARE(sps->position(), QPoint());

    // now let's try to change the position
    QSignalSpy positionChangedSpy(sps, &PlasmaShellSurfaceInterface::positionChanged);
    ps->setPosition(QPoint(1, 2));
    QVERIFY(positionChangedSpy.wait());
    QCOMPARE(positionChangedSpy.count(), 1);
    QVERIFY(sps->isPositionSet());
    QCOMPARE(sps->position(), QPoint(1, 2));

    // let's try to set same position, should not trigger an update
    ps->setPosition(QPoint(1, 2));
    QVERIFY(!positionChangedSpy.wait(100));
    // different point should work, though
    ps->setPosition(QPoint(3, 4));
    QVERIFY(positionChangedSpy.wait());
    QCOMPARE(positionChangedSpy.count(), 2);
    QCOMPARE(sps->position(), QPoint(3, 4));
}

void TestPlasmaShell::testSkipTaskbar()
{
    // this test verifies that sip taskbar is properly passed to server
    QSignalSpy plasmaSurfaceCreatedSpy(m_plasmaShellInterface, &PlasmaShellInterface::surfaceCreated);

    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.get()));
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);

    // verify that we got a plasma shell surface
    auto sps = plasmaSurfaceCreatedSpy.first().first().value<PlasmaShellSurfaceInterface *>();
    QVERIFY(sps);
    QVERIFY(sps->surface());
    QVERIFY(!sps->skipTaskbar());

    // now change
    QSignalSpy skipTaskbarChangedSpy(sps, &PlasmaShellSurfaceInterface::skipTaskbarChanged);
    ps->setSkipTaskbar(true);
    QVERIFY(skipTaskbarChangedSpy.wait());
    QVERIFY(sps->skipTaskbar());
    // setting to same again should not emit the signal
    ps->setSkipTaskbar(true);
    QEXPECT_FAIL("", "Should not be emitted if not changed", Continue);
    QVERIFY(!skipTaskbarChangedSpy.wait(100));
    QVERIFY(sps->skipTaskbar());

    // setting to false should change again
    ps->setSkipTaskbar(false);
    QVERIFY(skipTaskbarChangedSpy.wait());
    QVERIFY(!sps->skipTaskbar());
}

void TestPlasmaShell::testSkipSwitcher()
{
    // this test verifies that Skip Switcher is properly passed to server
    QSignalSpy plasmaSurfaceCreatedSpy(m_plasmaShellInterface, &PlasmaShellInterface::surfaceCreated);

    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.get()));
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);

    // verify that we got a plasma shell surface
    auto sps = plasmaSurfaceCreatedSpy.first().first().value<PlasmaShellSurfaceInterface *>();
    QVERIFY(sps);
    QVERIFY(sps->surface());
    QVERIFY(!sps->skipSwitcher());

    // now change
    QSignalSpy skipSwitcherChangedSpy(sps, &PlasmaShellSurfaceInterface::skipSwitcherChanged);
    ps->setSkipSwitcher(true);
    QVERIFY(skipSwitcherChangedSpy.wait());
    QVERIFY(sps->skipSwitcher());
    // setting to same again should not emit the signal
    ps->setSkipSwitcher(true);
    QEXPECT_FAIL("", "Should not be emitted if not changed", Continue);
    QVERIFY(!skipSwitcherChangedSpy.wait(100));
    QVERIFY(sps->skipSwitcher());

    // setting to false should change again
    ps->setSkipSwitcher(false);
    QVERIFY(skipSwitcherChangedSpy.wait());
    QVERIFY(!sps->skipSwitcher());
}

void TestPlasmaShell::testPanelBehavior_data()
{
    QTest::addColumn<KWayland::Client::PlasmaShellSurface::PanelBehavior>("client");
    QTest::addColumn<PlasmaShellSurfaceInterface::PanelBehavior>("server");

    QTest::newRow("autohide") << KWayland::Client::PlasmaShellSurface::PanelBehavior::AutoHide << PlasmaShellSurfaceInterface::PanelBehavior::AutoHide;
    QTest::newRow("can cover") << KWayland::Client::PlasmaShellSurface::PanelBehavior::WindowsCanCover << PlasmaShellSurfaceInterface::PanelBehavior::WindowsCanCover;
    QTest::newRow("go below") << KWayland::Client::PlasmaShellSurface::PanelBehavior::WindowsGoBelow << PlasmaShellSurfaceInterface::PanelBehavior::WindowsGoBelow;
}

void TestPlasmaShell::testPanelBehavior()
{
    // this test verifies that the panel behavior is properly passed to the server
    QSignalSpy plasmaSurfaceCreatedSpy(m_plasmaShellInterface, &PlasmaShellInterface::surfaceCreated);

    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.get()));
    ps->setRole(KWayland::Client::PlasmaShellSurface::Role::Panel);
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);

    // verify that we got a plasma shell surface
    auto sps = plasmaSurfaceCreatedSpy.first().first().value<PlasmaShellSurfaceInterface *>();
    QVERIFY(sps);
    QVERIFY(sps->surface());
    QCOMPARE(sps->panelBehavior(), PlasmaShellSurfaceInterface::PanelBehavior::AlwaysVisible);

    // now change the behavior
    QSignalSpy behaviorChangedSpy(sps, &PlasmaShellSurfaceInterface::panelBehaviorChanged);
    QFETCH(KWayland::Client::PlasmaShellSurface::PanelBehavior, client);
    ps->setPanelBehavior(client);
    QVERIFY(behaviorChangedSpy.wait());
    QTEST(sps->panelBehavior(), "server");

    // changing to same should not trigger the signal
    ps->setPanelBehavior(client);
    QVERIFY(!behaviorChangedSpy.wait(100));

    // but changing back to Always Visible should work
    ps->setPanelBehavior(KWayland::Client::PlasmaShellSurface::PanelBehavior::AlwaysVisible);
    QVERIFY(behaviorChangedSpy.wait());
    QCOMPARE(sps->panelBehavior(), PlasmaShellSurfaceInterface::PanelBehavior::AlwaysVisible);
}

void TestPlasmaShell::testAutoHidePanel()
{
    // this test verifies that auto-hiding panels work correctly
    QSignalSpy plasmaSurfaceCreatedSpy(m_plasmaShellInterface, &PlasmaShellInterface::surfaceCreated);

    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.get()));
    ps->setRole(KWayland::Client::PlasmaShellSurface::Role::Panel);
    ps->setPanelBehavior(KWayland::Client::PlasmaShellSurface::PanelBehavior::AutoHide);
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);
    auto sps = plasmaSurfaceCreatedSpy.first().first().value<PlasmaShellSurfaceInterface *>();
    QVERIFY(sps);
    QCOMPARE(sps->panelBehavior(), PlasmaShellSurfaceInterface::PanelBehavior::AutoHide);

    QSignalSpy autoHideRequestedSpy(sps, &PlasmaShellSurfaceInterface::panelAutoHideHideRequested);
    QSignalSpy autoHideShowRequestedSpy(sps, &PlasmaShellSurfaceInterface::panelAutoHideShowRequested);
    ps->requestHideAutoHidingPanel();
    QVERIFY(autoHideRequestedSpy.wait());
    QCOMPARE(autoHideRequestedSpy.count(), 1);
    QCOMPARE(autoHideShowRequestedSpy.count(), 0);

    QSignalSpy panelShownSpy(ps.get(), &KWayland::Client::PlasmaShellSurface::autoHidePanelShown);
    QSignalSpy panelHiddenSpy(ps.get(), &KWayland::Client::PlasmaShellSurface::autoHidePanelHidden);

    sps->hideAutoHidingPanel();
    QVERIFY(panelHiddenSpy.wait());
    QCOMPARE(panelHiddenSpy.count(), 1);
    QCOMPARE(panelShownSpy.count(), 0);

    ps->requestShowAutoHidingPanel();
    QVERIFY(autoHideShowRequestedSpy.wait());
    QCOMPARE(autoHideRequestedSpy.count(), 1);
    QCOMPARE(autoHideShowRequestedSpy.count(), 1);

    sps->showAutoHidingPanel();
    QVERIFY(panelShownSpy.wait());
    QCOMPARE(panelHiddenSpy.count(), 1);
    QCOMPARE(panelShownSpy.count(), 1);

    // change panel type
    ps->setPanelBehavior(KWayland::Client::PlasmaShellSurface::PanelBehavior::AlwaysVisible);
    // requesting auto hide should raise error
    QSignalSpy errorSpy(m_connection, &KWayland::Client::ConnectionThread::errorOccurred);
    ps->requestHideAutoHidingPanel();
    QVERIFY(errorSpy.wait());
}

void TestPlasmaShell::testPanelTakesFocus()
{
    // this test verifies that whether a panel wants to take focus is passed through correctly
    QSignalSpy plasmaSurfaceCreatedSpy(m_plasmaShellInterface, &PlasmaShellInterface::surfaceCreated);

    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.get()));
    ps->setRole(KWayland::Client::PlasmaShellSurface::Role::Panel);
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);
    auto sps = plasmaSurfaceCreatedSpy.first().first().value<PlasmaShellSurfaceInterface *>();
    QSignalSpy plasmaSurfaceTakesFocusSpy(sps, &PlasmaShellSurfaceInterface::panelTakesFocusChanged);

    QVERIFY(sps);
    QCOMPARE(sps->role(), PlasmaShellSurfaceInterface::Role::Panel);
    QCOMPARE(sps->panelTakesFocus(), false);

    ps->setPanelTakesFocus(true);
    m_connection->flush();
    QVERIFY(plasmaSurfaceTakesFocusSpy.wait());
    QCOMPARE(plasmaSurfaceTakesFocusSpy.count(), 1);
    QCOMPARE(sps->panelTakesFocus(), true);
    ps->setPanelTakesFocus(false);
    m_connection->flush();
    QVERIFY(plasmaSurfaceTakesFocusSpy.wait());
    QCOMPARE(plasmaSurfaceTakesFocusSpy.count(), 2);
    QCOMPARE(sps->panelTakesFocus(), false);
}

void TestPlasmaShell::testDisconnect()
{
    // this test verifies that a disconnect cleans up
    QSignalSpy plasmaSurfaceCreatedSpy(m_plasmaShellInterface, &PlasmaShellInterface::surfaceCreated);
    // create the surface
    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.get()));

    // and get them on the server
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);
    auto sps = plasmaSurfaceCreatedSpy.first().first().value<PlasmaShellSurfaceInterface *>();
    QVERIFY(sps);

    // disconnect
    QSignalSpy surfaceDestroyedSpy(sps, &QObject::destroyed);
    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    QCOMPARE(surfaceDestroyedSpy.count(), 0);
    QVERIFY(surfaceDestroyedSpy.wait());
    QCOMPARE(surfaceDestroyedSpy.count(), 1);

    s->destroy();
    ps->destroy();
    m_plasmaShell->destroy();
    m_compositor->destroy();
    m_registry->destroy();
    m_queue->destroy();
}

void TestPlasmaShell::testWhileDestroying()
{
    // this test tries to hit a condition that a Surface gets created with an ID which was already
    // used for a previous Surface. For each Surface we try to create a PlasmaShellSurface.
    // Even if there was a Surface in the past with the same ID, it should create the PlasmaShellSurface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    // create ShellSurface
    QSignalSpy shellSurfaceCreatedSpy(m_plasmaShellInterface, &PlasmaShellInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.get()));
    QVERIFY(shellSurfaceCreatedSpy.wait());

    // now try to create more surfaces
    QSignalSpy clientErrorSpy(m_connection, &KWayland::Client::ConnectionThread::errorOccurred);
    for (int i = 0; i < 100; i++) {
        s.reset();
        s.reset(m_compositor->createSurface());
        m_plasmaShell->createSurface(s.get(), this);
        QVERIFY(surfaceCreatedSpy.wait());
    }
    QVERIFY(clientErrorSpy.isEmpty());
    QVERIFY(!clientErrorSpy.wait(100));
    QVERIFY(clientErrorSpy.isEmpty());
}

QTEST_GUILESS_MAIN(TestPlasmaShell)
#include "test_plasmashell.moc"
