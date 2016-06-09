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
#include "client.h"
#include "cursor.h"
#include "screenedge.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "shell_client.h"
#include <kwineffects.h>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

#include <KDecoration2/Decoration>

#include <netwm.h>
#include <xcb/xcb_icccm.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_struts-0");

class StrutsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testWaylandStruts_data();
    void testWaylandStruts();
    void testX11Struts_data();
    void testX11Struts();
    void test363804();

private:
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::ServerSideDecorationManager *m_deco = nullptr;
    KWayland::Client::Seat *m_seat = nullptr;
    KWayland::Client::ShmPool *m_shm = nullptr;
    KWayland::Client::Shell *m_shell = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    KWayland::Client::PlasmaShell *m_plasmaShell = nullptr;
    QThread *m_thread = nullptr;
};

void StrutsTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setOutputCount", Qt::DirectConnection, Q_ARG(int, 2));
    waylandServer()->init(s_socketName.toLocal8Bit());

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
    waylandServer()->initWorkspace();
}

void StrutsTest::init()
{
    using namespace KWayland::Client;
    // setup connection
    m_connection = new ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::connected);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    Registry registry;
    registry.setEventQueue(m_queue);
    QSignalSpy compositorSpy(&registry, &Registry::compositorAnnounced);
    QSignalSpy shmSpy(&registry, &Registry::shmAnnounced);
    QSignalSpy shellSpy(&registry, &Registry::shellAnnounced);
    QSignalSpy seatSpy(&registry, &Registry::seatAnnounced);
    QSignalSpy decorationSpy(&registry, &Registry::serverSideDecorationManagerAnnounced);
    QSignalSpy allAnnounced(&registry, &Registry::interfacesAnnounced);
    QVERIFY(allAnnounced.isValid());
    QVERIFY(shmSpy.isValid());
    QVERIFY(shellSpy.isValid());
    QVERIFY(compositorSpy.isValid());
    QVERIFY(seatSpy.isValid());
    QVERIFY(decorationSpy.isValid());
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(allAnnounced.wait());
    QVERIFY(!compositorSpy.isEmpty());
    QVERIFY(!shmSpy.isEmpty());
    QVERIFY(!shellSpy.isEmpty());
    QVERIFY(!seatSpy.isEmpty());
    QVERIFY(!decorationSpy.isEmpty());

    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);
    QVERIFY(m_compositor->isValid());
    m_shm = registry.createShmPool(shmSpy.first().first().value<quint32>(), shmSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shm->isValid());
    m_shell = registry.createShell(shellSpy.first().first().value<quint32>(), shellSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shell->isValid());
    m_seat = registry.createSeat(seatSpy.first().first().value<quint32>(), seatSpy.first().last().value<quint32>(), this);
    QVERIFY(m_seat->isValid());
    m_deco = registry.createServerSideDecorationManager(decorationSpy.first().first().value<quint32>(), decorationSpy.first().last().value<quint32>());
    QVERIFY(m_deco->isValid());
    m_plasmaShell = registry.createPlasmaShell(registry.interface(Registry::Interface::PlasmaShell).name,
                                               registry.interface(Registry::Interface::PlasmaShell).version,
                                               this);
    QVERIFY(m_plasmaShell);
    QSignalSpy hasPointerSpy(m_seat, &Seat::hasPointerChanged);
    QVERIFY(hasPointerSpy.isValid());
    QVERIFY(hasPointerSpy.wait());

    screens()->setCurrent(0);
    Cursor::setPos(QPoint(640, 512));
}

void StrutsTest::cleanup()
{
    delete m_compositor;
    m_compositor = nullptr;
    delete m_deco;
    m_deco = nullptr;
    delete m_seat;
    m_seat = nullptr;
    delete m_shm;
    m_shm = nullptr;
    delete m_shell;
    m_shell = nullptr;
    delete m_plasmaShell;
    m_plasmaShell = nullptr;
    delete m_queue;
    m_queue = nullptr;
    if (m_thread) {
        m_connection->deleteLater();
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
        m_connection = nullptr;
    }
    while (!waylandServer()->clients().isEmpty()) {
        QCoreApplication::instance()->processEvents(QEventLoop::WaitForMoreEvents);
    }
    QVERIFY(waylandServer()->clients().isEmpty());
}

void StrutsTest::testWaylandStruts_data()
{
    QTest::addColumn<QVector<QRect>>("windowGeometries");
    QTest::addColumn<QRect>("screen0Maximized");
    QTest::addColumn<QRect>("screen1Maximized");
    QTest::addColumn<QRect>("workArea");

    QTest::newRow("bottom/0") << QVector<QRect>{QRect(0, 992, 1280, 32)}    << QRect(0, 0, 1280, 992)   << QRect(1280, 0, 1280, 1024) << QRect(0, 0, 2560, 992);
    QTest::newRow("bottom/1") << QVector<QRect>{QRect(1280, 992, 1280, 32)} << QRect(0, 0, 1280, 1024)  << QRect(1280, 0, 1280, 992)  << QRect(0, 0, 2560, 992);
    QTest::newRow("top/0")    << QVector<QRect>{QRect(0, 0, 1280, 32)}      << QRect(0, 32, 1280, 992)  << QRect(1280, 0, 1280, 1024) << QRect(0, 32, 2560, 992);
    QTest::newRow("top/1")    << QVector<QRect>{QRect(1280, 0, 1280, 32)}   << QRect(0, 0, 1280, 1024)  << QRect(1280, 32, 1280, 992) << QRect(0, 32, 2560, 992);
    QTest::newRow("left/0")   << QVector<QRect>{QRect(0, 0, 32, 1024)}      << QRect(32, 0, 1248, 1024) << QRect(1280, 0, 1280, 1024) << QRect(32, 0, 2528, 1024);
    QTest::newRow("left/1")   << QVector<QRect>{QRect(1280, 0, 32, 1024)}   << QRect(0, 0, 1280, 1024)  << QRect(1312, 0, 1248, 1024) << QRect(0, 0, 2560, 1024);
    QTest::newRow("right/0")  << QVector<QRect>{QRect(1248, 0, 32, 1024)}   << QRect(0, 0, 1248, 1024)  << QRect(1280, 0, 1280, 1024) << QRect(0, 0, 2560, 1024);
    QTest::newRow("right/1")  << QVector<QRect>{QRect(2528, 0, 32, 1024)}   << QRect(0, 0, 1280, 1024)  << QRect(1280, 0, 1248, 1024) << QRect(0, 0, 2528, 1024);

    // same with partial panels not covering the whole area
    QTest::newRow("part bottom/0") << QVector<QRect>{QRect(100, 992, 1080, 32)}  << QRect(0, 0, 1280, 992)   << QRect(1280, 0, 1280, 1024) << QRect(0, 0, 2560, 992);
    QTest::newRow("part bottom/1") << QVector<QRect>{QRect(1380, 992, 1080, 32)} << QRect(0, 0, 1280, 1024)  << QRect(1280, 0, 1280, 992)  << QRect(0, 0, 2560, 992);
    QTest::newRow("part top/0")    << QVector<QRect>{QRect(100, 0, 1080, 32)}    << QRect(0, 32, 1280, 992)  << QRect(1280, 0, 1280, 1024) << QRect(0, 32, 2560, 992);
    QTest::newRow("part top/1")    << QVector<QRect>{QRect(1380, 0, 1080, 32)}   << QRect(0, 0, 1280, 1024)  << QRect(1280, 32, 1280, 992) << QRect(0, 32, 2560, 992);
    QTest::newRow("part left/0")   << QVector<QRect>{QRect(0, 100, 32, 824)}     << QRect(32, 0, 1248, 1024) << QRect(1280, 0, 1280, 1024) << QRect(32, 0, 2528, 1024);
    QTest::newRow("part left/1")   << QVector<QRect>{QRect(1280, 100, 32, 824)}  << QRect(0, 0, 1280, 1024)  << QRect(1312, 0, 1248, 1024) << QRect(0, 0, 2560, 1024);
    QTest::newRow("part right/0")  << QVector<QRect>{QRect(1248, 100, 32, 824)}  << QRect(0, 0, 1248, 1024)  << QRect(1280, 0, 1280, 1024) << QRect(0, 0, 2560, 1024);
    QTest::newRow("part right/1")  << QVector<QRect>{QRect(2528, 100, 32, 824)}  << QRect(0, 0, 1280, 1024)  << QRect(1280, 0, 1248, 1024) << QRect(0, 0, 2528, 1024);

    // multiple panels
    QTest::newRow("two bottom panels") << QVector<QRect>{QRect(100, 992, 1080, 32), QRect(1380, 984, 1080, 40)} << QRect(0, 0, 1280, 992) << QRect(1280, 0, 1280, 984) << QRect(0, 0, 2560, 984);
    QTest::newRow("two left panels") << QVector<QRect>{QRect(0, 10, 32, 390), QRect(0, 450, 40, 100)} << QRect(40, 0, 1240, 1024) << QRect(1280, 0, 1280, 1024) << QRect(40, 0, 2520, 1024);
}

void StrutsTest::testWaylandStruts()
{
    // this test verifies that struts on Wayland panels are handled correctly
    using namespace KWayland::Client;
    // no, struts yet
    QVERIFY(waylandServer()->clients().isEmpty());
    // first screen
    QCOMPARE(workspace()->clientArea(PlacementArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MovementArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeFullArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(FullScreenArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(ScreenArea, 0, 1), QRect(0, 0, 1280, 1024));
    // second screen
    QCOMPARE(workspace()->clientArea(PlacementArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MovementArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeFullArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(FullScreenArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(ScreenArea, 1, 1), QRect(1280, 0, 1280, 1024));
    // combined
    QCOMPARE(workspace()->clientArea(WorkArea, 0, 1), QRect(0, 0, 2560, 1024));
    QCOMPARE(workspace()->clientArea(FullArea, 0, 1), QRect(0, 0, 2560, 1024));

    QFETCH(QVector<QRect>, windowGeometries);
    // create the panels
    QSignalSpy windowCreatedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(windowCreatedSpy.isValid());
    for (auto it = windowGeometries.constBegin(), end = windowGeometries.constEnd(); it != end; it++) {
        const QRect windowGeometry = *it;
        Surface *surface = m_compositor->createSurface(m_compositor);
        ShellSurface *shellSurface = m_shell->createSurface(surface, surface);
        Q_UNUSED(shellSurface)
        PlasmaShellSurface *plasmaSurface = m_plasmaShell->createSurface(surface, surface);
        plasmaSurface->setPosition(windowGeometry.topLeft());
        plasmaSurface->setRole(PlasmaShellSurface::Role::Panel);

        // map the window
        QImage img(windowGeometry.size(), QImage::Format_RGB32);
        img.fill(Qt::red);
        surface->attachBuffer(m_shm->createBuffer(img));
        surface->damage(QRect(QPoint(0, 0), windowGeometry.size()));
        surface->commit(Surface::CommitFlag::None);

        QVERIFY(windowCreatedSpy.wait());
        QCOMPARE(windowCreatedSpy.count(), 1);
        auto c = windowCreatedSpy.first().first().value<ShellClient*>();
        QVERIFY(c);
        QVERIFY(!c->isActive());
        QCOMPARE(c->geometry(), windowGeometry);
        QVERIFY(c->isDock());
        QVERIFY(c->hasStrut());
        windowCreatedSpy.clear();
    }

    // some props are independent of struts - those first
    // screen 0
    QCOMPARE(workspace()->clientArea(MovementArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeFullArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(FullScreenArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(ScreenArea, 0, 1), QRect(0, 0, 1280, 1024));
    // screen 1
    QCOMPARE(workspace()->clientArea(MovementArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeFullArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(FullScreenArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(ScreenArea, 1, 1), QRect(1280, 0, 1280, 1024));
    // combined
    QCOMPARE(workspace()->clientArea(FullArea, 0, 1), QRect(0, 0, 2560, 1024));

    // now verify the actual updated client areas
    QTEST(workspace()->clientArea(PlacementArea, 0, 1), "screen0Maximized");
    QTEST(workspace()->clientArea(MaximizeArea, 0, 1), "screen0Maximized");
    QTEST(workspace()->clientArea(PlacementArea, 1, 1), "screen1Maximized");
    QTEST(workspace()->clientArea(MaximizeArea, 1, 1), "screen1Maximized");
    QTEST(workspace()->clientArea(WorkArea, 0, 1), "workArea");
}

void StrutsTest::testX11Struts_data()
{
    QTest::addColumn<QRect>("windowGeometry");
    QTest::addColumn<int>("leftStrut");
    QTest::addColumn<int>("rightStrut");
    QTest::addColumn<int>("topStrut");
    QTest::addColumn<int>("bottomStrut");
    QTest::addColumn<int>("leftStrutStart");
    QTest::addColumn<int>("leftStrutEnd");
    QTest::addColumn<int>("rightStrutStart");
    QTest::addColumn<int>("rightStrutEnd");
    QTest::addColumn<int>("topStrutStart");
    QTest::addColumn<int>("topStrutEnd");
    QTest::addColumn<int>("bottomStrutStart");
    QTest::addColumn<int>("bottomStrutEnd");
    QTest::addColumn<QRect>("screen0Maximized");
    QTest::addColumn<QRect>("screen1Maximized");
    QTest::addColumn<QRect>("workArea");

    QTest::newRow("bottom panel/no strut") << QRect(0, 980, 1280, 44)
                                           << 0 << 0 << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << QRect(0, 0, 1280, 1024)
                                           << QRect(1280, 0, 1280, 1024)
                                           << QRect(0, 0, 2560, 1024);
    QTest::newRow("bottom panel/strut") << QRect(0, 980, 1280, 44)
                                        << 0 << 0 << 0 << 44
                                        << 0 << 0
                                        << 0 << 0
                                        << 0 << 0
                                        << 0 << 1279
                                        << QRect(0, 0, 1280, 980)
                                        << QRect(1280, 0, 1280, 1024)
                                        << QRect(0, 0, 2560, 980);
    QTest::newRow("top panel/no strut") << QRect(0, 0, 1280, 44)
                                           << 0 << 0 << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << QRect(0, 0, 1280, 1024)
                                           << QRect(1280, 0, 1280, 1024)
                                           << QRect(0, 0, 2560, 1024);
    QTest::newRow("top panel/strut") << QRect(0, 0, 1280, 44)
                                           << 0 << 0 << 44 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 1279
                                           << 0 << 0
                                           << QRect(0, 44, 1280, 980)
                                           << QRect(1280, 0, 1280, 1024)
                                           << QRect(0, 44, 2560, 980);
    QTest::newRow("left panel/no strut") << QRect(0, 0, 60, 1024)
                                           << 0 << 0 << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << QRect(0, 0, 1280, 1024)
                                           << QRect(1280, 0, 1280, 1024)
                                           << QRect(0, 0, 2560, 1024);
    QTest::newRow("left panel/strut") << QRect(0, 0, 60, 1024)
                                           << 60 << 0 << 0 << 0
                                           << 0 << 1023
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << QRect(60, 0, 1220, 1024)
                                           << QRect(1280, 0, 1280, 1024)
                                           << QRect(60, 0, 2500, 1024);
    QTest::newRow("right panel/no strut") << QRect(0, 1220, 60, 1024)
                                           << 0 << 0 << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << QRect(0, 0, 1280, 1024)
                                           << QRect(1280, 0, 1280, 1024)
                                           << QRect(0, 0, 2560, 1024);
    QTest::newRow("right panel/strut") << QRect(0, 1220, 60, 1024)
                                           << 0 << 1340 << 0 << 0
                                           << 0 << 0
                                           << 0 << 1023
                                           << 0 << 0
                                           << 0 << 0
                                           << QRect(0, 0, /*1220*/1280, 1024)
                                           << QRect(1280, 0, 1280, 1024)
                                           << QRect(0, 0, 2560, 1024);
    // second screen
    QTest::newRow("bottom panel 1/no strut") << QRect(1280, 980, 1280, 44)
                                           << 0 << 0 << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << QRect(0, 0, 1280, 1024)
                                           << QRect(1280, 0, 1280, 1024)
                                           << QRect(0, 0, 2560, 1024);
    QTest::newRow("bottom panel 1/strut") << QRect(1280, 980, 1280, 44)
                                           << 0 << 0 << 0 << 44
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 1280 << 2559
                                           << QRect(0, 0, 1280, 1024)
                                           << QRect(1280, 0, 1280, 980)
                                           << QRect(0, 0, 2560, 980);
    QTest::newRow("top panel 1/no strut") << QRect(1280, 0, 1280, 44)
                                           << 0 << 0 << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << QRect(0, 0, 1280, 1024)
                                           << QRect(1280, 0, 1280, 1024)
                                           << QRect(0, 0, 2560, 1024);
    QTest::newRow("top panel 1 /strut") << QRect(1280, 0, 1280, 44)
                                           << 0 << 0 << 44 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 1280 << 2559
                                           << 0 << 0
                                           << QRect(0, 0, 1280, 1024)
                                           << QRect(1280, 44, 1280, 980)
                                           << QRect(0, 44, 2560, 980);
    QTest::newRow("left panel 1/no strut") << QRect(1280, 0, 60, 1024)
                                           << 0 << 0 << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << QRect(0, 0, 1280, 1024)
                                           << QRect(1280, 0, 1280, 1024)
                                           << QRect(0, 0, 2560, 1024);
    QTest::newRow("left panel 1/strut") << QRect(1280, 0, 60, 1024)
                                           << 1340 << 0 << 0 << 0
                                           << 0 << 1023
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << QRect(0, 0, 1280, 1024)
                                           << QRect(1280, 0, 1280, 1024)//QRect(1340, 0, 1220, 1024)
                                           << QRect(0, 0, 2560, 1024);
    // invalid struts
    QTest::newRow("bottom panel/ invalid strut") << QRect(0, 980, 1280, 44)
                                        << 1280 << 0 << 0 << 44
                                        << 980 << 1024
                                        << 0 << 0
                                        << 0 << 0
                                        << 0 << 1279
                                        << QRect(0, 0, 1280, 1024)
                                        << QRect(1280, 0, 1280, 1024)
                                        << QRect(0, 0, 2560, 1024);
    QTest::newRow("top panel/ invalid strut") << QRect(0, 0, 1280, 44)
                                           << 1280 << 0 << 44 << 0
                                           << 0 << 44
                                           << 0 << 0
                                           << 0 << 1279
                                           << 0 << 0
                                           << QRect(0, 0, 1280, 1024)
                                           << QRect(1280, 0, 1280, 1024)
                                           << QRect(0, 0, 2560, 1024);
    QTest::newRow("top panel/invalid strut 2") << QRect(0, 0, 1280, 44)
                                           << 0 << 0 << 1024 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 1279
                                           << 0 << 0
                                           << QRect(0, 0, 1280, 1024)
                                           << QRect(1280, 0, 1280, 1024)
                                           << QRect(0, 0, 2560, 1024);
}

struct XcbConnectionDeleter
{
    static inline void cleanup(xcb_connection_t *pointer)
    {
        xcb_disconnect(pointer);
    }
};

void StrutsTest::testX11Struts()
{
    // this test verifies that struts are applied correctly for X11 windows

    // no, struts yet
    // first screen
    QCOMPARE(workspace()->clientArea(PlacementArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MovementArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeFullArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(FullScreenArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(ScreenArea, 0, 1), QRect(0, 0, 1280, 1024));
    // second screen
    QCOMPARE(workspace()->clientArea(PlacementArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MovementArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeFullArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(FullScreenArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(ScreenArea, 1, 1), QRect(1280, 0, 1280, 1024));
    // combined
    QCOMPARE(workspace()->clientArea(WorkArea, 0, 1), QRect(0, 0, 2560, 1024));
    QCOMPARE(workspace()->clientArea(FullArea, 0, 1), QRect(0, 0, 2560, 1024));

    // create an xcb window
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));

    xcb_window_t w = xcb_generate_id(c.data());
    QFETCH(QRect, windowGeometry);
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
    NETWinInfo info(c.data(), w, rootWindow(), NET::WMAllProperties, NET::WM2AllProperties);
    info.setWindowType(NET::Dock);
    // set the extended strut
    QFETCH(int, leftStrut);
    QFETCH(int, rightStrut);
    QFETCH(int, topStrut);
    QFETCH(int, bottomStrut);
    QFETCH(int, leftStrutStart);
    QFETCH(int, leftStrutEnd);
    QFETCH(int, rightStrutStart);
    QFETCH(int, rightStrutEnd);
    QFETCH(int, topStrutStart);
    QFETCH(int, topStrutEnd);
    QFETCH(int, bottomStrutStart);
    QFETCH(int, bottomStrutEnd);
    NETExtendedStrut strut;
    strut.left_start = leftStrutStart;
    strut.left_end = leftStrutEnd;
    strut.left_width = leftStrut;
    strut.right_start = rightStrutStart;
    strut.right_end = rightStrutEnd;
    strut.right_width = rightStrut;
    strut.top_start = topStrutStart;
    strut.top_end = topStrutEnd;
    strut.top_width = topStrut;
    strut.bottom_start = bottomStrutStart;
    strut.bottom_end = bottomStrutEnd;
    strut.bottom_width = bottomStrut;
    info.setExtendedStrut(strut);
    xcb_map_window(c.data(), w);
    xcb_flush(c.data());

    // we should get a client for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    Client *client = windowCreatedSpy.first().first().value<Client*>();
    QVERIFY(client);
    QCOMPARE(client->window(), w);
    QVERIFY(!client->isDecorated());
    QCOMPARE(client->windowType(), NET::Dock);
    QCOMPARE(client->geometry(), windowGeometry);

    // this should have affected the client area
    // some props are independent of struts - those first
    // screen 0
    QCOMPARE(workspace()->clientArea(MovementArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeFullArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(FullScreenArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(ScreenArea, 0, 1), QRect(0, 0, 1280, 1024));
    // screen 1
    QCOMPARE(workspace()->clientArea(MovementArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeFullArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(FullScreenArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(ScreenArea, 1, 1), QRect(1280, 0, 1280, 1024));
    // combined
    QCOMPARE(workspace()->clientArea(FullArea, 0, 1), QRect(0, 0, 2560, 1024));

    // now verify the actual updated client areas
    QTEST(workspace()->clientArea(PlacementArea, 0, 1), "screen0Maximized");
    QTEST(workspace()->clientArea(MaximizeArea, 0, 1), "screen0Maximized");
    QTEST(workspace()->clientArea(PlacementArea, 1, 1), "screen1Maximized");
    QTEST(workspace()->clientArea(MaximizeArea, 1, 1), "screen1Maximized");
    QTEST(workspace()->clientArea(WorkArea, 0, 1), "workArea");

    // and destroy the window again
    xcb_unmap_window(c.data(), w);
    xcb_destroy_window(c.data(), w);
    xcb_flush(c.data());
    c.reset();

    QSignalSpy windowClosedSpy(client, &Client::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    QVERIFY(windowClosedSpy.wait());

    // now struts should be removed again
    QCOMPARE(workspace()->clientArea(PlacementArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MovementArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeFullArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(FullScreenArea, 0, 1), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(ScreenArea, 0, 1), QRect(0, 0, 1280, 1024));
    // second screen
    QCOMPARE(workspace()->clientArea(PlacementArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MovementArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeFullArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(FullScreenArea, 1, 1), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(ScreenArea, 1, 1), QRect(1280, 0, 1280, 1024));
    // combined
    QCOMPARE(workspace()->clientArea(WorkArea, 0, 1), QRect(0, 0, 2560, 1024));
    QCOMPARE(workspace()->clientArea(FullArea, 0, 1), QRect(0, 0, 2560, 1024));
}

void StrutsTest::test363804()
{
    // this test verifies the condition described in BUG 363804
    // two screens in a vertical setup, aligned to right border with panel on the bottom screen
    const QVector<QRect> geometries{QRect(0, 0, 1920, 1080), QRect(554, 1080, 1366, 768)};
    QMetaObject::invokeMethod(kwinApp()->platform(), "outputGeometriesChanged",
                              Qt::DirectConnection,
                              Q_ARG(QVector<QRect>, geometries));
    QCOMPARE(screens()->geometry(0), geometries.at(0));
    QCOMPARE(screens()->geometry(1), geometries.at(1));
    QCOMPARE(screens()->geometry(), QRect(0, 0, 1920, 1848));

    // create an xcb window
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));

    xcb_window_t w = xcb_generate_id(c.data());
    const QRect windowGeometry(554, 1812, 1366, 36);
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
    NETWinInfo info(c.data(), w, rootWindow(), NET::WMAllProperties, NET::WM2AllProperties);
    info.setWindowType(NET::Dock);
    NETExtendedStrut strut;
    strut.left_start = 0;
    strut.left_end = 0;
    strut.left_width = 0;
    strut.right_start = 0;
    strut.right_end = 0;
    strut.right_width = 0;
    strut.top_start = 0;
    strut.top_end = 0;
    strut.top_width = 0;
    strut.bottom_start = 554;
    strut.bottom_end = 1919;
    strut.bottom_width = 36;
    info.setExtendedStrut(strut);
    xcb_map_window(c.data(), w);
    xcb_flush(c.data());

    // we should get a client for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    Client *client = windowCreatedSpy.first().first().value<Client*>();
    QVERIFY(client);
    QCOMPARE(client->window(), w);
    QVERIFY(!client->isDecorated());
    QCOMPARE(client->windowType(), NET::Dock);
    QCOMPARE(client->geometry(), windowGeometry);

    // now verify the actual updated client areas
    QCOMPARE(workspace()->clientArea(PlacementArea, 0, 1), geometries.at(0));
    QCOMPARE(workspace()->clientArea(MaximizeArea, 0, 1), geometries.at(0));
    QEXPECT_FAIL("", "The actual bug", Continue);
    QCOMPARE(workspace()->clientArea(PlacementArea, 1, 1), QRect(554, 1080, 1366, 732));
    QEXPECT_FAIL("", "The actual bug", Continue);
    QCOMPARE(workspace()->clientArea(MaximizeArea, 1, 1), QRect(554, 1080, 1366, 732));
    QCOMPARE(workspace()->clientArea(WorkArea, 0, 1), QRect(0, 0, 1920, 1812));

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

WAYLANDTEST_MAIN(KWin::StrutsTest)
#include "struts_test.moc"
