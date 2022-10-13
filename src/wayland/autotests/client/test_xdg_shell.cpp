/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

// Qt
#include <QtTest>
// client
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/output.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/shm_pool.h"
#include "KWayland/Client/surface.h"
#include "KWayland/Client/xdgshell.h"
// server
#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/output_interface.h"
#include "wayland/seat_interface.h"
#include "wayland/surface_interface.h"
#include "wayland/xdgshell_interface.h"

#include "../../tests/fakeoutput.h"

using namespace KWayland::Client;
using namespace KWaylandServer;

Q_DECLARE_METATYPE(Qt::MouseButton)

static const QString s_socketName = QStringLiteral("kwayland-test-xdg_shell-0");

class XdgShellTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void testCreateSurface();
    void testTitle();
    void testWindowClass();
    void testMaximize();
    void testMinimize();
    void testFullscreen();
    void testShowWindowMenu();
    void testMove();
    void testResize_data();
    void testResize();
    void testTransient();
    void testPing();
    void testClose();
    void testConfigureStates_data();
    void testConfigureStates();
    void testConfigureMultipleAcks();

private:
    std::unique_ptr<XdgShellInterface> m_xdgShellInterface;
    std::unique_ptr<Compositor> m_compositor;
    std::unique_ptr<XdgShell> m_xdgShell;
    std::unique_ptr<KWaylandServer::Display> m_display;
    std::unique_ptr<CompositorInterface> m_compositorInterface;
    std::unique_ptr<FakeOutput> m_output1Handle;
    std::unique_ptr<OutputInterface> m_output1Interface;
    std::unique_ptr<FakeOutput> m_output2Handle;
    std::unique_ptr<OutputInterface> m_output2Interface;
    std::unique_ptr<SeatInterface> m_seatInterface;
    std::unique_ptr<ConnectionThread> m_connection;
    std::unique_ptr<QThread> m_thread;
    std::unique_ptr<EventQueue> m_queue;
    std::unique_ptr<ShmPool> m_shmPool;
    std::unique_ptr<Output> m_output1;
    std::unique_ptr<Output> m_output2;
    std::unique_ptr<Seat> m_seat;
};

#define SURFACE                                                                                      \
    QSignalSpy xdgSurfaceCreatedSpy(m_xdgShellInterface.get(), &XdgShellInterface::toplevelCreated); \
    std::unique_ptr<Surface> surface(m_compositor->createSurface());                                 \
    std::unique_ptr<XdgShellSurface> xdgSurface(m_xdgShell->createSurface(surface.get()));           \
    QCOMPARE(xdgSurface->size(), QSize());                                                           \
    QVERIFY(xdgSurfaceCreatedSpy.wait());                                                            \
    auto serverXdgToplevel = xdgSurfaceCreatedSpy.first().first().value<XdgToplevelInterface *>();   \
    QVERIFY(serverXdgToplevel);

void XdgShellTest::init()
{
    m_display = std::make_unique<KWaylandServer::Display>();
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();
    m_output1Handle = std::make_unique<FakeOutput>();
    m_output1Handle->setMode(QSize(1024, 768), 60000);
    m_output1Interface = std::make_unique<OutputInterface>(m_display.get(), m_output1Handle.get());
    m_output1Interface->setMode(QSize(1024, 768));
    m_output2Handle = std::make_unique<FakeOutput>();
    m_output2Handle->setMode(QSize(1024, 768), 60000);
    m_output2Interface = std::make_unique<OutputInterface>(m_display.get(), m_output2Handle.get());
    m_output2Interface->setMode(QSize(1024, 768));
    m_seatInterface = std::make_unique<SeatInterface>(m_display.get());
    m_seatInterface->setHasKeyboard(true);
    m_seatInterface->setHasPointer(true);
    m_seatInterface->setHasTouch(true);
    m_compositorInterface = std::make_unique<CompositorInterface>(m_display.get());
    m_xdgShellInterface = std::make_unique<XdgShellInterface>(m_display.get());

    // setup connection
    m_connection = std::make_unique<KWayland::Client::ConnectionThread>();
    QSignalSpy connectedSpy(m_connection.get(), &ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = std::make_unique<QThread>();
    m_connection->moveToThread(m_thread.get());
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = std::make_unique<EventQueue>();
    m_queue->setup(m_connection.get());

    Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Registry::interfacesAnnounced);
    QSignalSpy interfaceAnnouncedSpy(&registry, &Registry::interfaceAnnounced);
    QSignalSpy outputAnnouncedSpy(&registry, &Registry::outputAnnounced);

    QSignalSpy xdgShellAnnouncedSpy(&registry, &Registry::xdgShellStableAnnounced);
    registry.setEventQueue(m_queue.get());
    registry.create(m_connection.get());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    QCOMPARE(outputAnnouncedSpy.count(), 2);
    m_output1.reset(registry.createOutput(outputAnnouncedSpy.first().at(0).value<quint32>(), outputAnnouncedSpy.first().at(1).value<quint32>()));
    m_output2.reset(registry.createOutput(outputAnnouncedSpy.last().at(0).value<quint32>(), outputAnnouncedSpy.last().at(1).value<quint32>()));

    m_shmPool.reset(registry.createShmPool(registry.interface(Registry::Interface::Shm).name, registry.interface(Registry::Interface::Shm).version));
    QVERIFY(m_shmPool);
    QVERIFY(m_shmPool->isValid());

    m_compositor.reset(registry.createCompositor(registry.interface(Registry::Interface::Compositor).name, registry.interface(Registry::Interface::Compositor).version));
    QVERIFY(m_compositor);
    QVERIFY(m_compositor->isValid());

    m_seat.reset(registry.createSeat(registry.interface(Registry::Interface::Seat).name, registry.interface(Registry::Interface::Seat).version));
    QVERIFY(m_seat);
    QVERIFY(m_seat->isValid());

    QCOMPARE(xdgShellAnnouncedSpy.count(), 1);

    m_xdgShell.reset(registry.createXdgShell(registry.interface(Registry::Interface::XdgShellStable).name,
                                             registry.interface(Registry::Interface::XdgShellStable).version));
    QVERIFY(m_xdgShell);
    QVERIFY(m_xdgShell->isValid());
}

void XdgShellTest::cleanup()
{
    m_xdgShell.reset();
    m_compositor.reset();
    m_shmPool.reset();
    m_output1.reset();
    m_output2.reset();
    m_seat.reset();
    m_queue.reset();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        m_thread.reset();
    }
    m_connection.reset();

    m_display.reset();
}

void XdgShellTest::testCreateSurface()
{
    // this test verifies that we can create a surface
    // first created the signal spies for the server
    QSignalSpy surfaceCreatedSpy(m_compositorInterface.get(), &CompositorInterface::surfaceCreated);
    QSignalSpy xdgSurfaceCreatedSpy(m_xdgShellInterface.get(), &XdgShellInterface::toplevelCreated);

    // create surface
    std::unique_ptr<Surface> surface(m_compositor->createSurface());
    QVERIFY(surface != nullptr);
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    // create shell surface
    std::unique_ptr<XdgShellSurface> xdgSurface(m_xdgShell->createSurface(surface.get()));
    QVERIFY(xdgSurface != nullptr);
    QVERIFY(xdgSurfaceCreatedSpy.wait());
    // verify base things
    auto serverToplevel = xdgSurfaceCreatedSpy.first().first().value<XdgToplevelInterface *>();
    QVERIFY(serverToplevel);
    QCOMPARE(serverToplevel->windowTitle(), QString());
    QCOMPARE(serverToplevel->windowClass(), QByteArray());
    QCOMPARE(serverToplevel->parentXdgToplevel(), nullptr);
    QCOMPARE(serverToplevel->surface(), serverSurface);

    // now let's destroy it
    QSignalSpy destroyedSpy(serverToplevel, &QObject::destroyed);
    xdgSurface.reset();
    QVERIFY(destroyedSpy.wait());
}

void XdgShellTest::testTitle()
{
    // this test verifies that we can change the title of a shell surface
    // first create surface
    SURFACE

    // should not have a title yet
    QCOMPARE(serverXdgToplevel->windowTitle(), QString());

    // lets' change the title
    QSignalSpy titleChangedSpy(serverXdgToplevel, &XdgToplevelInterface::windowTitleChanged);
    xdgSurface->setTitle(QStringLiteral("foo"));
    QVERIFY(titleChangedSpy.wait());
    QCOMPARE(titleChangedSpy.count(), 1);
    QCOMPARE(titleChangedSpy.first().first().toString(), QStringLiteral("foo"));
    QCOMPARE(serverXdgToplevel->windowTitle(), QStringLiteral("foo"));
}

void XdgShellTest::testWindowClass()
{
    // this test verifies that we can change the window class/app id of a shell surface
    // first create surface
    SURFACE

    // should not have a window class yet
    QCOMPARE(serverXdgToplevel->windowClass(), QByteArray());

    // let's change the window class
    QSignalSpy windowClassChanged(serverXdgToplevel, &XdgToplevelInterface::windowClassChanged);
    xdgSurface->setAppId(QByteArrayLiteral("org.kde.xdgsurfacetest"));
    QVERIFY(windowClassChanged.wait());
    QCOMPARE(windowClassChanged.count(), 1);
    QCOMPARE(windowClassChanged.first().first().toByteArray(), QByteArrayLiteral("org.kde.xdgsurfacetest"));
    QCOMPARE(serverXdgToplevel->windowClass(), QByteArrayLiteral("org.kde.xdgsurfacetest"));
}

void XdgShellTest::testMaximize()
{
    // this test verifies that the maximize/unmaximize calls work
    SURFACE

    QSignalSpy maximizeRequestedSpy(serverXdgToplevel, &XdgToplevelInterface::maximizeRequested);
    QSignalSpy unmaximizeRequestedSpy(serverXdgToplevel, &XdgToplevelInterface::unmaximizeRequested);

    xdgSurface->setMaximized(true);
    QVERIFY(maximizeRequestedSpy.wait());
    QCOMPARE(maximizeRequestedSpy.count(), 1);

    xdgSurface->setMaximized(false);
    QVERIFY(unmaximizeRequestedSpy.wait());
    QCOMPARE(unmaximizeRequestedSpy.count(), 1);
}

void XdgShellTest::testMinimize()
{
    // this test verifies that the minimize request is delivered
    SURFACE

    QSignalSpy minimizeRequestedSpy(serverXdgToplevel, &XdgToplevelInterface::minimizeRequested);

    xdgSurface->requestMinimize();
    QVERIFY(minimizeRequestedSpy.wait());
    QCOMPARE(minimizeRequestedSpy.count(), 1);
}

void XdgShellTest::testFullscreen()
{
    qRegisterMetaType<OutputInterface *>();
    // this test verifies going to/from fullscreen
    SURFACE

    QSignalSpy fullscreenRequestedSpy(serverXdgToplevel, &XdgToplevelInterface::fullscreenRequested);
    QSignalSpy unfullscreenRequestedSpy(serverXdgToplevel, &XdgToplevelInterface::unfullscreenRequested);

    // without an output
    xdgSurface->setFullscreen(true, nullptr);
    QVERIFY(fullscreenRequestedSpy.wait());
    QCOMPARE(fullscreenRequestedSpy.count(), 1);
    QVERIFY(!fullscreenRequestedSpy.last().at(0).value<OutputInterface *>());

    // unset
    xdgSurface->setFullscreen(false);
    QVERIFY(unfullscreenRequestedSpy.wait());
    QCOMPARE(unfullscreenRequestedSpy.count(), 1);

    // with outputs
    xdgSurface->setFullscreen(true, m_output1.get());
    QVERIFY(fullscreenRequestedSpy.wait());
    QCOMPARE(fullscreenRequestedSpy.count(), 2);
    QCOMPARE(fullscreenRequestedSpy.last().at(0).value<OutputInterface *>(), m_output1Interface.get());

    // now other output
    xdgSurface->setFullscreen(true, m_output2.get());
    QVERIFY(fullscreenRequestedSpy.wait());
    QCOMPARE(fullscreenRequestedSpy.count(), 3);
    QCOMPARE(fullscreenRequestedSpy.last().at(0).value<OutputInterface *>(), m_output2Interface.get());
}

void XdgShellTest::testShowWindowMenu()
{
    qRegisterMetaType<SeatInterface *>();
    // this test verifies that the show window menu request works
    SURFACE

    // hack: pretend that the xdg-surface had been configured
    serverXdgToplevel->sendConfigure(QSize(0, 0), XdgToplevelInterface::States());

    QSignalSpy windowMenuSpy(serverXdgToplevel, &XdgToplevelInterface::windowMenuRequested);

    // TODO: the serial needs to be a proper one
    xdgSurface->requestShowWindowMenu(m_seat.get(), 20, QPoint(30, 40));
    QVERIFY(windowMenuSpy.wait());
    QCOMPARE(windowMenuSpy.count(), 1);
    QCOMPARE(windowMenuSpy.first().at(0).value<SeatInterface *>(), m_seatInterface.get());
    QCOMPARE(windowMenuSpy.first().at(1).toPoint(), QPoint(30, 40));
    QCOMPARE(windowMenuSpy.first().at(2).value<quint32>(), 20u);
}

void XdgShellTest::testMove()
{
    qRegisterMetaType<SeatInterface *>();
    // this test verifies that the move request works
    SURFACE

    // hack: pretend that the xdg-surface had been configured
    serverXdgToplevel->sendConfigure(QSize(0, 0), XdgToplevelInterface::States());

    QSignalSpy moveSpy(serverXdgToplevel, &XdgToplevelInterface::moveRequested);

    // TODO: the serial needs to be a proper one
    xdgSurface->requestMove(m_seat.get(), 50);
    QVERIFY(moveSpy.wait());
    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(moveSpy.first().at(0).value<SeatInterface *>(), m_seatInterface.get());
    QCOMPARE(moveSpy.first().at(1).value<quint32>(), 50u);
}

void XdgShellTest::testResize_data()
{
    QTest::addColumn<Qt::Edges>("edges");
    QTest::addColumn<XdgToplevelInterface::ResizeAnchor>("anchor");

    QTest::newRow("none") << Qt::Edges() << XdgToplevelInterface::ResizeAnchor::None;
    QTest::newRow("top") << Qt::Edges(Qt::TopEdge) << XdgToplevelInterface::ResizeAnchor::Top;
    QTest::newRow("bottom") << Qt::Edges(Qt::BottomEdge) << XdgToplevelInterface::ResizeAnchor::Bottom;
    QTest::newRow("left") << Qt::Edges(Qt::LeftEdge) << XdgToplevelInterface::ResizeAnchor::Left;
    QTest::newRow("top left") << Qt::Edges(Qt::TopEdge | Qt::LeftEdge) << XdgToplevelInterface::ResizeAnchor::TopLeft;
    QTest::newRow("bottom left") << Qt::Edges(Qt::BottomEdge | Qt::LeftEdge) << XdgToplevelInterface::ResizeAnchor::BottomLeft;
    QTest::newRow("right") << Qt::Edges(Qt::RightEdge) << XdgToplevelInterface::ResizeAnchor::Right;
    QTest::newRow("top right") << Qt::Edges(Qt::TopEdge | Qt::RightEdge) << XdgToplevelInterface::ResizeAnchor::TopRight;
    QTest::newRow("bottom right") << Qt::Edges(Qt::BottomEdge | Qt::RightEdge) << XdgToplevelInterface::ResizeAnchor::BottomRight;
}

void XdgShellTest::testResize()
{
    qRegisterMetaType<SeatInterface *>();
    // this test verifies that the resize request works
    SURFACE

    // hack: pretend that the xdg-surface had been configured
    serverXdgToplevel->sendConfigure(QSize(0, 0), XdgToplevelInterface::States());

    QSignalSpy resizeSpy(serverXdgToplevel, &XdgToplevelInterface::resizeRequested);

    // TODO: the serial needs to be a proper one
    QFETCH(Qt::Edges, edges);
    xdgSurface->requestResize(m_seat.get(), 60, edges);
    QVERIFY(resizeSpy.wait());
    QCOMPARE(resizeSpy.count(), 1);
    QCOMPARE(resizeSpy.first().at(0).value<SeatInterface *>(), m_seatInterface.get());
    QTEST(resizeSpy.first().at(1).value<XdgToplevelInterface::ResizeAnchor>(), "anchor");
    QCOMPARE(resizeSpy.first().at(2).value<quint32>(), 60u);
}

void XdgShellTest::testTransient()
{
    // this test verifies that setting the transient for works
    SURFACE
    std::unique_ptr<Surface> surface2(m_compositor->createSurface());
    std::unique_ptr<XdgShellSurface> xdgSurface2(m_xdgShell->createSurface(surface2.get()));
    QVERIFY(xdgSurfaceCreatedSpy.wait());
    auto serverXdgToplevel2 = xdgSurfaceCreatedSpy.last().first().value<XdgToplevelInterface *>();
    QVERIFY(serverXdgToplevel2);

    QVERIFY(!serverXdgToplevel->parentXdgToplevel());
    QVERIFY(!serverXdgToplevel2->parentXdgToplevel());

    // now make xdsgSurface2 a transient for xdgSurface
    QSignalSpy transientForSpy(serverXdgToplevel2, &XdgToplevelInterface::parentXdgToplevelChanged);
    xdgSurface2->setTransientFor(xdgSurface.get());

    QVERIFY(transientForSpy.wait());
    QCOMPARE(transientForSpy.count(), 1);
    QCOMPARE(serverXdgToplevel2->parentXdgToplevel(), serverXdgToplevel);
    QVERIFY(!serverXdgToplevel->parentXdgToplevel());

    // unset the transient for
    xdgSurface2->setTransientFor(nullptr);
    QVERIFY(transientForSpy.wait());
    QCOMPARE(transientForSpy.count(), 2);
    QVERIFY(!serverXdgToplevel2->parentXdgToplevel());
    QVERIFY(!serverXdgToplevel->parentXdgToplevel());
}

void XdgShellTest::testPing()
{
    // this test verifies that a ping request is sent to the client
    SURFACE

    QSignalSpy pingSpy(m_xdgShellInterface.get(), &XdgShellInterface::pongReceived);

    quint32 serial = m_xdgShellInterface->ping(serverXdgToplevel->xdgSurface());
    QVERIFY(pingSpy.wait());
    QCOMPARE(pingSpy.count(), 1);
    QCOMPARE(pingSpy.takeFirst().at(0).value<quint32>(), serial);

    // test of a ping failure
    // disconnecting the connection thread to the queue will break the connection and pings will do a timeout
    disconnect(m_connection.get(), &ConnectionThread::eventsRead, m_queue.get(), &EventQueue::dispatch);
    m_xdgShellInterface->ping(serverXdgToplevel->xdgSurface());
    QSignalSpy pingDelayedSpy(m_xdgShellInterface.get(), &XdgShellInterface::pingDelayed);
    QVERIFY(pingDelayedSpy.wait());

    QSignalSpy pingTimeoutSpy(m_xdgShellInterface.get(), &XdgShellInterface::pingTimeout);
    QVERIFY(pingTimeoutSpy.wait());
}

void XdgShellTest::testClose()
{
    // this test verifies that a close request is sent to the client
    SURFACE

    QSignalSpy closeSpy(xdgSurface.get(), &XdgShellSurface::closeRequested);

    serverXdgToplevel->sendClose();
    QVERIFY(closeSpy.wait());
    QCOMPARE(closeSpy.count(), 1);

    QSignalSpy destroyedSpy(serverXdgToplevel, &XdgToplevelInterface::destroyed);
    xdgSurface.reset();
    QVERIFY(destroyedSpy.wait());
}

void XdgShellTest::testConfigureStates_data()
{
    QTest::addColumn<XdgToplevelInterface::States>("serverStates");
    QTest::addColumn<XdgShellSurface::States>("clientStates");

    const auto sa = XdgToplevelInterface::States(XdgToplevelInterface::State::Activated);
    const auto sm = XdgToplevelInterface::States(XdgToplevelInterface::State::Maximized);
    const auto sf = XdgToplevelInterface::States(XdgToplevelInterface::State::FullScreen);
    const auto sr = XdgToplevelInterface::States(XdgToplevelInterface::State::Resizing);

    const auto ca = XdgShellSurface::States(XdgShellSurface::State::Activated);
    const auto cm = XdgShellSurface::States(XdgShellSurface::State::Maximized);
    const auto cf = XdgShellSurface::States(XdgShellSurface::State::Fullscreen);
    const auto cr = XdgShellSurface::States(XdgShellSurface::State::Resizing);

    QTest::newRow("none") << XdgToplevelInterface::States() << XdgShellSurface::States();
    QTest::newRow("Active") << sa << ca;
    QTest::newRow("Maximize") << sm << cm;
    QTest::newRow("Fullscreen") << sf << cf;
    QTest::newRow("Resizing") << sr << cr;

    QTest::newRow("Active/Maximize") << (sa | sm) << (ca | cm);
    QTest::newRow("Active/Fullscreen") << (sa | sf) << (ca | cf);
    QTest::newRow("Active/Resizing") << (sa | sr) << (ca | cr);
    QTest::newRow("Maximize/Fullscreen") << (sm | sf) << (cm | cf);
    QTest::newRow("Maximize/Resizing") << (sm | sr) << (cm | cr);
    QTest::newRow("Fullscreen/Resizing") << (sf | sr) << (cf | cr);

    QTest::newRow("Active/Maximize/Fullscreen") << (sa | sm | sf) << (ca | cm | cf);
    QTest::newRow("Active/Maximize/Resizing") << (sa | sm | sr) << (ca | cm | cr);
    QTest::newRow("Maximize/Fullscreen|Resizing") << (sm | sf | sr) << (cm | cf | cr);

    QTest::newRow("Active/Maximize/Fullscreen/Resizing") << (sa | sm | sf | sr) << (ca | cm | cf | cr);
}

void XdgShellTest::testConfigureStates()
{
    qRegisterMetaType<XdgShellSurface::States>();
    // this test verifies that configure states works
    SURFACE

    QSignalSpy configureSpy(xdgSurface.get(), &XdgShellSurface::configureRequested);

    QFETCH(XdgToplevelInterface::States, serverStates);
    serverXdgToplevel->sendConfigure(QSize(0, 0), serverStates);
    QVERIFY(configureSpy.wait());
    QCOMPARE(configureSpy.count(), 1);
    QCOMPARE(configureSpy.first().at(0).toSize(), QSize(0, 0));
    QTEST(configureSpy.first().at(1).value<XdgShellSurface::States>(), "clientStates");
    QCOMPARE(configureSpy.first().at(2).value<quint32>(), m_display->serial());

    QSignalSpy ackSpy(serverXdgToplevel->xdgSurface(), &XdgSurfaceInterface::configureAcknowledged);

    xdgSurface->ackConfigure(configureSpy.first().at(2).value<quint32>());
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(ackSpy.wait());
    QCOMPARE(ackSpy.count(), 1);
    QCOMPARE(ackSpy.first().first().value<quint32>(), configureSpy.first().at(2).value<quint32>());
}

void XdgShellTest::testConfigureMultipleAcks()
{
    qRegisterMetaType<XdgShellSurface::States>();
    // this test verifies that with multiple configure requests the last acknowledged one acknowledges all
    SURFACE

    QSignalSpy configureSpy(xdgSurface.get(), &XdgShellSurface::configureRequested);
    QSignalSpy sizeChangedSpy(xdgSurface.get(), &XdgShellSurface::sizeChanged);
    QSignalSpy ackSpy(serverXdgToplevel->xdgSurface(), &XdgSurfaceInterface::configureAcknowledged);

    serverXdgToplevel->sendConfigure(QSize(10, 20), XdgToplevelInterface::States());
    const quint32 serial1 = m_display->serial();
    serverXdgToplevel->sendConfigure(QSize(20, 30), XdgToplevelInterface::States());
    const quint32 serial2 = m_display->serial();
    QVERIFY(serial1 != serial2);
    serverXdgToplevel->sendConfigure(QSize(30, 40), XdgToplevelInterface::States());
    const quint32 serial3 = m_display->serial();
    QVERIFY(serial1 != serial3);
    QVERIFY(serial2 != serial3);

    QVERIFY(configureSpy.wait());
    QCOMPARE(configureSpy.count(), 3);
    QCOMPARE(configureSpy.at(0).at(0).toSize(), QSize(10, 20));
    QCOMPARE(configureSpy.at(0).at(1).value<XdgShellSurface::States>(), XdgShellSurface::States());
    QCOMPARE(configureSpy.at(0).at(2).value<quint32>(), serial1);
    QCOMPARE(configureSpy.at(1).at(0).toSize(), QSize(20, 30));
    QCOMPARE(configureSpy.at(1).at(1).value<XdgShellSurface::States>(), XdgShellSurface::States());
    QCOMPARE(configureSpy.at(1).at(2).value<quint32>(), serial2);
    QCOMPARE(configureSpy.at(2).at(0).toSize(), QSize(30, 40));
    QCOMPARE(configureSpy.at(2).at(1).value<XdgShellSurface::States>(), XdgShellSurface::States());
    QCOMPARE(configureSpy.at(2).at(2).value<quint32>(), serial3);
    QCOMPARE(sizeChangedSpy.count(), 3);
    QCOMPARE(sizeChangedSpy.at(0).at(0).toSize(), QSize(10, 20));
    QCOMPARE(sizeChangedSpy.at(1).at(0).toSize(), QSize(20, 30));
    QCOMPARE(sizeChangedSpy.at(2).at(0).toSize(), QSize(30, 40));
    QCOMPARE(xdgSurface->size(), QSize(30, 40));

    xdgSurface->ackConfigure(serial3);
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(ackSpy.wait());
    QCOMPARE(ackSpy.count(), 1);
    QCOMPARE(ackSpy.last().first().value<quint32>(), serial3);

    // configure once more with a null size
    serverXdgToplevel->sendConfigure(QSize(0, 0), XdgToplevelInterface::States());
    // should not change size
    QVERIFY(configureSpy.wait());
    QCOMPARE(configureSpy.count(), 4);
    QCOMPARE(sizeChangedSpy.count(), 3);
    QCOMPARE(xdgSurface->size(), QSize(30, 40));
}

QTEST_GUILESS_MAIN(XdgShellTest)
#include "test_xdg_shell.moc"
