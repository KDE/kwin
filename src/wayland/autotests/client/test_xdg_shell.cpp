/********************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "test_xdg_shell.h"

XdgShellTest::XdgShellTest(XdgShellInterfaceVersion version):
    m_version(version)
{}

static const QString s_socketName = QStringLiteral("kwayland-test-xdg_shell-0");

void XdgShellTest::init()
{
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();
    m_o1Interface = m_display->createOutput(m_display);
    m_o1Interface->addMode(QSize(1024, 768));
    m_o1Interface->create();
    m_o2Interface = m_display->createOutput(m_display);
    m_o2Interface->addMode(QSize(1024, 768));
    m_o2Interface->create();
    m_seatInterface = m_display->createSeat(m_display);
    m_seatInterface->setHasKeyboard(true);
    m_seatInterface->setHasPointer(true);
    m_seatInterface->setHasTouch(true);
    m_seatInterface->create();
    m_compositorInterface = m_display->createCompositor(m_display);
    m_compositorInterface->create();
    m_xdgShellInterface = m_display->createXdgShell(m_version, m_display);
    QCOMPARE(m_xdgShellInterface->interfaceVersion(), m_version);
    m_xdgShellInterface->create();

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::connected);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new EventQueue(this);
    m_queue->setup(m_connection);

    Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy interfaceAnnouncedSpy(&registry, &Registry::interfaceAnnounced);
    QVERIFY(interfaceAnnouncedSpy.isValid());
    QSignalSpy outputAnnouncedSpy(&registry, &Registry::outputAnnounced);
    QVERIFY(outputAnnouncedSpy.isValid());

    auto shellAnnouncedSignal =  m_version == XdgShellInterfaceVersion::UnstableV5 ?
    &Registry::xdgShellUnstableV5Announced : &Registry::xdgShellUnstableV6Announced;

    QSignalSpy xdgShellAnnouncedSpy(&registry, shellAnnouncedSignal);
    QVERIFY(xdgShellAnnouncedSpy.isValid());
    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    QCOMPARE(outputAnnouncedSpy.count(), 2);
    m_output1 = registry.createOutput(outputAnnouncedSpy.first().at(0).value<quint32>(), outputAnnouncedSpy.first().at(1).value<quint32>(), this);
    m_output2 = registry.createOutput(outputAnnouncedSpy.last().at(0).value<quint32>(), outputAnnouncedSpy.last().at(1).value<quint32>(), this);

    m_shmPool = registry.createShmPool(registry.interface(Registry::Interface::Shm).name, registry.interface(Registry::Interface::Shm).version, this);
    QVERIFY(m_shmPool);
    QVERIFY(m_shmPool->isValid());

    m_compositor = registry.createCompositor(registry.interface(Registry::Interface::Compositor).name, registry.interface(Registry::Interface::Compositor).version, this);
    QVERIFY(m_compositor);
    QVERIFY(m_compositor->isValid());

    m_seat = registry.createSeat(registry.interface(Registry::Interface::Seat).name, registry.interface(Registry::Interface::Seat).version, this);
    QVERIFY(m_seat);
    QVERIFY(m_seat->isValid());

    QCOMPARE(xdgShellAnnouncedSpy.count(), 1);

    Registry::Interface iface = m_version == XdgShellInterfaceVersion::UnstableV5 ? Registry::Interface::XdgShellUnstableV5 : Registry::Interface::XdgShellUnstableV6;

    m_xdgShell = registry.createXdgShell(registry.interface(iface).name,
                                         registry.interface(iface).version,
                                         this);
    QVERIFY(m_xdgShell);
    QVERIFY(m_xdgShell->isValid());
}

void XdgShellTest::cleanup()
{
#define CLEANUP(variable) \
    if (variable) { \
        delete variable; \
        variable = nullptr; \
    }
    CLEANUP(m_xdgShell)
    CLEANUP(m_compositor)
    CLEANUP(m_shmPool)
    CLEANUP(m_output1)
    CLEANUP(m_output2)
    CLEANUP(m_seat)
    CLEANUP(m_queue)
    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }

    CLEANUP(m_compositorInterface)
    CLEANUP(m_xdgShellInterface)
    CLEANUP(m_o1Interface);
    CLEANUP(m_o2Interface);
    CLEANUP(m_seatInterface);
    CLEANUP(m_display)
#undef CLEANUP
}

void XdgShellTest::testCreateSurface()
{
    // this test verifies that we can create a surface
    // first created the signal spies for the server
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QSignalSpy xdgSurfaceCreatedSpy(m_xdgShellInterface, &XdgShellInterface::surfaceCreated);
    QVERIFY(xdgSurfaceCreatedSpy.isValid());

    // create surface
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(!surface.isNull());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
    QVERIFY(serverSurface);

    // create shell surface
    QScopedPointer<XdgShellSurface> xdgSurface(m_xdgShell->createSurface(surface.data()));
    QVERIFY(!xdgSurface.isNull());
    QVERIFY(xdgSurfaceCreatedSpy.wait());
    // verify base things
    auto serverXdgSurface = xdgSurfaceCreatedSpy.first().first().value<XdgShellSurfaceInterface*>();
    QVERIFY(serverXdgSurface);
    QCOMPARE(serverXdgSurface->isConfigurePending(), false);
    QCOMPARE(serverXdgSurface->title(), QString());
    QCOMPARE(serverXdgSurface->windowClass(), QByteArray());
    QCOMPARE(serverXdgSurface->isTransient(), false);
    QCOMPARE(serverXdgSurface->transientFor(), QPointer<XdgShellSurfaceInterface>());
    QCOMPARE(serverXdgSurface->surface(), serverSurface);

    // now let's destroy it
    QSignalSpy destroyedSpy(serverXdgSurface, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    xdgSurface.reset();
    QVERIFY(destroyedSpy.wait());
}

void XdgShellTest::testTitle()
{
    // this test verifies that we can change the title of a shell surface
    // first create surface
    SURFACE

    // should not have a title yet
    QCOMPARE(serverXdgSurface->title(), QString());

    // lets' change the title
    QSignalSpy titleChangedSpy(serverXdgSurface, &XdgShellSurfaceInterface::titleChanged);
    QVERIFY(titleChangedSpy.isValid());
    xdgSurface->setTitle(QStringLiteral("foo"));
    QVERIFY(titleChangedSpy.wait());
    QCOMPARE(titleChangedSpy.count(), 1);
    QCOMPARE(titleChangedSpy.first().first().toString(), QStringLiteral("foo"));
    QCOMPARE(serverXdgSurface->title(), QStringLiteral("foo"));
}

void XdgShellTest::testWindowClass()
{
    // this test verifies that we can change the window class/app id of a shell surface
    // first create surface
    SURFACE

    // should not have a window class yet
    QCOMPARE(serverXdgSurface->windowClass(), QByteArray());

    // let's change the window class
    QSignalSpy windowClassChanged(serverXdgSurface, &XdgShellSurfaceInterface::windowClassChanged);
    QVERIFY(windowClassChanged.isValid());
    xdgSurface->setAppId(QByteArrayLiteral("org.kde.xdgsurfacetest"));
    QVERIFY(windowClassChanged.wait());
    QCOMPARE(windowClassChanged.count(), 1);
    QCOMPARE(windowClassChanged.first().first().toByteArray(), QByteArrayLiteral("org.kde.xdgsurfacetest"));
    QCOMPARE(serverXdgSurface->windowClass(), QByteArrayLiteral("org.kde.xdgsurfacetest"));
}

void XdgShellTest::testMaximize()
{
    // this test verifies that the maximize/unmaximize calls work
    SURFACE

    QSignalSpy maximizeRequestedSpy(serverXdgSurface, &XdgShellSurfaceInterface::maximizedChanged);
    QVERIFY(maximizeRequestedSpy.isValid());

    xdgSurface->setMaximized(true);
    QVERIFY(maximizeRequestedSpy.wait());
    QCOMPARE(maximizeRequestedSpy.count(), 1);
    QCOMPARE(maximizeRequestedSpy.last().first().toBool(), true);

    xdgSurface->setMaximized(false);
    QVERIFY(maximizeRequestedSpy.wait());
    QCOMPARE(maximizeRequestedSpy.count(), 2);
    QCOMPARE(maximizeRequestedSpy.last().first().toBool(), false);
}

void XdgShellTest::testMinimize()
{
    // this test verifies that the minimize request is delivered
    SURFACE

    QSignalSpy minimizeRequestedSpy(serverXdgSurface, &XdgShellSurfaceInterface::minimizeRequested);
    QVERIFY(minimizeRequestedSpy.isValid());

    xdgSurface->requestMinimize();
    QVERIFY(minimizeRequestedSpy.wait());
    QCOMPARE(minimizeRequestedSpy.count(), 1);
}

void XdgShellTest::testFullscreen()
{
    qRegisterMetaType<OutputInterface*>();
    // this test verifies going to/from fullscreen
    QSignalSpy xdgSurfaceCreatedSpy(m_xdgShellInterface, &XdgShellInterface::surfaceCreated);
    QVERIFY(xdgSurfaceCreatedSpy.isValid());
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QScopedPointer<XdgShellSurface> xdgSurface(m_xdgShell->createSurface(surface.data()));
    QVERIFY(xdgSurfaceCreatedSpy.wait());
    auto serverXdgSurface = xdgSurfaceCreatedSpy.first().first().value<XdgShellSurfaceInterface*>();
    QVERIFY(serverXdgSurface);

    QSignalSpy fullscreenSpy(serverXdgSurface, &XdgShellSurfaceInterface::fullscreenChanged);
    QVERIFY(fullscreenSpy.isValid());

    // without an output
    xdgSurface->setFullscreen(true, nullptr);
    QVERIFY(fullscreenSpy.wait());
    QCOMPARE(fullscreenSpy.count(), 1);
    QCOMPARE(fullscreenSpy.last().at(0).toBool(), true);
    QVERIFY(!fullscreenSpy.last().at(1).value<OutputInterface*>());

    // unset
    xdgSurface->setFullscreen(false);
    QVERIFY(fullscreenSpy.wait());
    QCOMPARE(fullscreenSpy.count(), 2);
    QCOMPARE(fullscreenSpy.last().at(0).toBool(), false);
    QVERIFY(!fullscreenSpy.last().at(1).value<OutputInterface*>());

    // with outputs
    xdgSurface->setFullscreen(true, m_output1);
    QVERIFY(fullscreenSpy.wait());
    QCOMPARE(fullscreenSpy.count(), 3);
    QCOMPARE(fullscreenSpy.last().at(0).toBool(), true);
    QCOMPARE(fullscreenSpy.last().at(1).value<OutputInterface*>(), m_o1Interface);

    // now other output
    xdgSurface->setFullscreen(true, m_output2);
    QVERIFY(fullscreenSpy.wait());
    QCOMPARE(fullscreenSpy.count(), 4);
    QCOMPARE(fullscreenSpy.last().at(0).toBool(), true);
    QCOMPARE(fullscreenSpy.last().at(1).value<OutputInterface*>(), m_o2Interface);
}

void XdgShellTest::testShowWindowMenu()
{
    qRegisterMetaType<SeatInterface*>();
    // this test verifies that the show window menu request works
    SURFACE

    QSignalSpy windowMenuSpy(serverXdgSurface, &XdgShellSurfaceInterface::windowMenuRequested);
    QVERIFY(windowMenuSpy.isValid());

    // TODO: the serial needs to be a proper one
    xdgSurface->requestShowWindowMenu(m_seat, 20, QPoint(30, 40));
    QVERIFY(windowMenuSpy.wait());
    QCOMPARE(windowMenuSpy.count(), 1);
    QCOMPARE(windowMenuSpy.first().at(0).value<SeatInterface*>(), m_seatInterface);
    QCOMPARE(windowMenuSpy.first().at(1).value<quint32>(), 20u);
    QCOMPARE(windowMenuSpy.first().at(2).toPoint(), QPoint(30, 40));
}

void XdgShellTest::testMove()
{
    qRegisterMetaType<SeatInterface*>();
    // this test verifies that the move request works
    SURFACE

    QSignalSpy moveSpy(serverXdgSurface, &XdgShellSurfaceInterface::moveRequested);
    QVERIFY(moveSpy.isValid());

    // TODO: the serial needs to be a proper one
    xdgSurface->requestMove(m_seat, 50);
    QVERIFY(moveSpy.wait());
    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(moveSpy.first().at(0).value<SeatInterface*>(), m_seatInterface);
    QCOMPARE(moveSpy.first().at(1).value<quint32>(), 50u);
}

void XdgShellTest::testResize_data()
{
    QTest::addColumn<Qt::Edges>("edges");

    QTest::newRow("none")         << Qt::Edges();
    QTest::newRow("top")          << Qt::Edges(Qt::TopEdge);
    QTest::newRow("bottom")       << Qt::Edges(Qt::BottomEdge);
    QTest::newRow("left")         << Qt::Edges(Qt::LeftEdge);
    QTest::newRow("top left")     << Qt::Edges(Qt::TopEdge | Qt::LeftEdge);
    QTest::newRow("bottom left")  << Qt::Edges(Qt::BottomEdge | Qt::LeftEdge);
    QTest::newRow("right")        << Qt::Edges(Qt::RightEdge);
    QTest::newRow("top right")    << Qt::Edges(Qt::TopEdge | Qt::RightEdge);
    QTest::newRow("bottom right") << Qt::Edges(Qt::BottomEdge | Qt::RightEdge);
}

void XdgShellTest::testResize()
{
    qRegisterMetaType<SeatInterface*>();
    // this test verifies that the resize request works
    SURFACE

    QSignalSpy resizeSpy(serverXdgSurface, &XdgShellSurfaceInterface::resizeRequested);
    QVERIFY(resizeSpy.isValid());

    // TODO: the serial needs to be a proper one
    QFETCH(Qt::Edges, edges);
    xdgSurface->requestResize(m_seat, 60, edges);
    QVERIFY(resizeSpy.wait());
    QCOMPARE(resizeSpy.count(), 1);
    QCOMPARE(resizeSpy.first().at(0).value<SeatInterface*>(), m_seatInterface);
    QCOMPARE(resizeSpy.first().at(1).value<quint32>(), 60u);
    QCOMPARE(resizeSpy.first().at(2).value<Qt::Edges>(), edges);
}

void XdgShellTest::testTransient()
{
    // this test verifies that setting the transient for works
    SURFACE
    QScopedPointer<Surface> surface2(m_compositor->createSurface());
    QScopedPointer<XdgShellSurface> xdgSurface2(m_xdgShell->createSurface(surface2.data()));
    QVERIFY(xdgSurfaceCreatedSpy.wait());
    auto serverXdgSurface2 = xdgSurfaceCreatedSpy.last().first().value<XdgShellSurfaceInterface*>();
    QVERIFY(serverXdgSurface2);

    QVERIFY(!serverXdgSurface->isTransient());
    QVERIFY(!serverXdgSurface2->isTransient());

    // now make xdsgSurface2 a transient for xdgSurface
    QSignalSpy transientForSpy(serverXdgSurface2, &XdgShellSurfaceInterface::transientForChanged);
    QVERIFY(transientForSpy.isValid());
    xdgSurface2->setTransientFor(xdgSurface.data());

    QVERIFY(transientForSpy.wait());
    QCOMPARE(transientForSpy.count(), 1);
    QVERIFY(serverXdgSurface2->isTransient());
    QCOMPARE(serverXdgSurface2->transientFor().data(), serverXdgSurface);
    QVERIFY(!serverXdgSurface->isTransient());

    // unset the transient for
    xdgSurface2->setTransientFor(nullptr);
    QVERIFY(transientForSpy.wait());
    QCOMPARE(transientForSpy.count(), 2);
    QVERIFY(!serverXdgSurface2->isTransient());
    QVERIFY(serverXdgSurface2->transientFor().isNull());
    QVERIFY(!serverXdgSurface->isTransient());
}

void XdgShellTest::testPing()
{
    // this test verifies that a ping request is sent to the client
    SURFACE

    QSignalSpy pingSpy(m_xdgShellInterface, &XdgShellInterface::pongReceived);
    QVERIFY(pingSpy.isValid());

    quint32 serial = m_xdgShellInterface->ping(serverXdgSurface);
    QVERIFY(pingSpy.wait());
    QCOMPARE(pingSpy.count(), 1);
    QCOMPARE(pingSpy.takeFirst().at(0).value<quint32>(), serial);

    // test of a ping failure
    // disconnecting the connection thread to the queue will break the connection and pings will do a timeout
    disconnect(m_connection, &ConnectionThread::eventsRead, m_queue, &EventQueue::dispatch);
    m_xdgShellInterface->ping(serverXdgSurface);
    QSignalSpy pingDelayedSpy(m_xdgShellInterface, &XdgShellInterface::pingDelayed);
    QVERIFY(pingDelayedSpy.wait());

    QSignalSpy pingTimeoutSpy(m_xdgShellInterface, &XdgShellInterface::pingTimeout);
    QVERIFY(pingTimeoutSpy.wait());
}

void XdgShellTest::testClose()
{
    // this test verifies that a close request is sent to the client
    SURFACE

    QSignalSpy closeSpy(xdgSurface.data(), &XdgShellSurface::closeRequested);
    QVERIFY(closeSpy.isValid());

    serverXdgSurface->close();
    QVERIFY(closeSpy.wait());
    QCOMPARE(closeSpy.count(), 1);

    QSignalSpy destroyedSpy(serverXdgSurface, &XdgShellSurfaceInterface::destroyed);
    QVERIFY(destroyedSpy.isValid());
    xdgSurface.reset();
    QVERIFY(destroyedSpy.wait());
}

void XdgShellTest::testConfigureStates_data()
{
    QTest::addColumn<XdgShellSurfaceInterface::States>("serverStates");
    QTest::addColumn<XdgShellSurface::States>("clientStates");

    const auto sa = XdgShellSurfaceInterface::States(XdgShellSurfaceInterface::State::Activated);
    const auto sm = XdgShellSurfaceInterface::States(XdgShellSurfaceInterface::State::Maximized);
    const auto sf = XdgShellSurfaceInterface::States(XdgShellSurfaceInterface::State::Fullscreen);
    const auto sr = XdgShellSurfaceInterface::States(XdgShellSurfaceInterface::State::Resizing);

    const auto ca = XdgShellSurface::States(XdgShellSurface::State::Activated);
    const auto cm = XdgShellSurface::States(XdgShellSurface::State::Maximized);
    const auto cf = XdgShellSurface::States(XdgShellSurface::State::Fullscreen);
    const auto cr = XdgShellSurface::States(XdgShellSurface::State::Resizing);

    QTest::newRow("none")       << XdgShellSurfaceInterface::States()   << XdgShellSurface::States();
    QTest::newRow("Active")     << sa << ca;
    QTest::newRow("Maximize")   << sm << cm;
    QTest::newRow("Fullscreen") << sf << cf;
    QTest::newRow("Resizing")   << sr << cr;

    QTest::newRow("Active/Maximize")       << (sa | sm) << (ca | cm);
    QTest::newRow("Active/Fullscreen")     << (sa | sf) << (ca | cf);
    QTest::newRow("Active/Resizing")       << (sa | sr) << (ca | cr);
    QTest::newRow("Maximize/Fullscreen")   << (sm | sf) << (cm | cf);
    QTest::newRow("Maximize/Resizing")     << (sm | sr) << (cm | cr);
    QTest::newRow("Fullscreen/Resizing")   << (sf | sr) << (cf | cr);

    QTest::newRow("Active/Maximize/Fullscreen")   << (sa | sm | sf) << (ca | cm | cf);
    QTest::newRow("Active/Maximize/Resizing")     << (sa | sm | sr) << (ca | cm | cr);
    QTest::newRow("Maximize/Fullscreen|Resizing") << (sm | sf | sr) << (cm | cf | cr);

    QTest::newRow("Active/Maximize/Fullscreen/Resizing")   << (sa | sm | sf | sr) << (ca | cm | cf | cr);
}

void XdgShellTest::testConfigureStates()
{
    qRegisterMetaType<XdgShellSurface::States>();
    // this test verifies that configure states works
    SURFACE

    QSignalSpy configureSpy(xdgSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureSpy.isValid());

    QFETCH(XdgShellSurfaceInterface::States, serverStates);
    serverXdgSurface->configure(serverStates);
    QVERIFY(configureSpy.wait());
    QCOMPARE(configureSpy.count(), 1);
    QCOMPARE(configureSpy.first().at(0).toSize(), QSize(0, 0));
    QTEST(configureSpy.first().at(1).value<XdgShellSurface::States>(), "clientStates");
    QCOMPARE(configureSpy.first().at(2).value<quint32>(), m_display->serial());

    QSignalSpy ackSpy(serverXdgSurface, &XdgShellSurfaceInterface::configureAcknowledged);
    QVERIFY(ackSpy.isValid());

    xdgSurface->ackConfigure(configureSpy.first().at(2).value<quint32>());
    QVERIFY(ackSpy.wait());
    QCOMPARE(ackSpy.count(), 1);
    QCOMPARE(ackSpy.first().first().value<quint32>(), configureSpy.first().at(2).value<quint32>());
}

void XdgShellTest::testConfigureMultipleAcks()
{
    qRegisterMetaType<XdgShellSurface::States>();
    // this test verifies that with multiple configure requests the last acknowledged one acknowledges all
    SURFACE

    QSignalSpy configureSpy(xdgSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureSpy.isValid());
    QSignalSpy sizeChangedSpy(xdgSurface.data(), &XdgShellSurface::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());
    QSignalSpy ackSpy(serverXdgSurface, &XdgShellSurfaceInterface::configureAcknowledged);
    QVERIFY(ackSpy.isValid());

    serverXdgSurface->configure(XdgShellSurfaceInterface::States(), QSize(10, 20));
    const quint32 serial1 = m_display->serial();
    serverXdgSurface->configure(XdgShellSurfaceInterface::States(), QSize(20, 30));
    const quint32 serial2 = m_display->serial();
    QVERIFY(serial1 != serial2);
    serverXdgSurface->configure(XdgShellSurfaceInterface::States(), QSize(30, 40));
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
    QVERIFY(ackSpy.wait());
    QCOMPARE(ackSpy.count(), 3);
    QCOMPARE(ackSpy.at(0).first().value<quint32>(), serial1);
    QCOMPARE(ackSpy.at(1).first().value<quint32>(), serial2);
    QCOMPARE(ackSpy.at(2).first().value<quint32>(), serial3);

    // configure once more with a null size
    serverXdgSurface->configure(XdgShellSurfaceInterface::States());
    // should not change size
    QVERIFY(configureSpy.wait());
    QCOMPARE(configureSpy.count(), 4);
    QCOMPARE(sizeChangedSpy.count(), 3);
    QCOMPARE(xdgSurface->size(), QSize(30, 40));
}

#include "test_xdg_shell.moc"
