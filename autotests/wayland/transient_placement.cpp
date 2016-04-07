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
#include "abstract_backend.h"
#include "abstract_client.h"
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
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/touch.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/surface_interface.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_transient_placement-0");

class TransientPlacementTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testSimplePosition_data();
    void testSimplePosition();
    void testDecorationPosition_data();
    void testDecorationPosition();

private:
    AbstractClient *showWindow(const QSize &size, bool decorated = false, KWayland::Client::Surface *parent = nullptr, const QPoint &offset = QPoint());
    KWayland::Client::Surface *surfaceForClient(AbstractClient *c) const;
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::ServerSideDecorationManager *m_deco = nullptr;
    KWayland::Client::Seat *m_seat = nullptr;
    KWayland::Client::ShmPool *m_shm = nullptr;
    KWayland::Client::Shell *m_shell = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    QThread *m_thread = nullptr;
};

void TransientPlacementTest::initTestCase()
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

void TransientPlacementTest::init()
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
    QSignalSpy allAnnounced(&registry, &Registry::interfacesAnnounced);
    QVERIFY(allAnnounced.isValid());
    QVERIFY(shmSpy.isValid());
    QVERIFY(shellSpy.isValid());
    QVERIFY(compositorSpy.isValid());
    QVERIFY(seatSpy.isValid());
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(allAnnounced.wait());
    QVERIFY(!compositorSpy.isEmpty());
    QVERIFY(!shmSpy.isEmpty());
    QVERIFY(!shellSpy.isEmpty());
    QVERIFY(!seatSpy.isEmpty());

    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);
    QVERIFY(m_compositor->isValid());
    m_shm = registry.createShmPool(shmSpy.first().first().value<quint32>(), shmSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shm->isValid());
    m_shell = registry.createShell(shellSpy.first().first().value<quint32>(), shellSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shell->isValid());
    m_seat = registry.createSeat(seatSpy.first().first().value<quint32>(), seatSpy.first().last().value<quint32>(), this);
    QVERIFY(m_seat->isValid());
    QSignalSpy hasPointerSpy(m_seat, &Seat::hasPointerChanged);
    QVERIFY(hasPointerSpy.isValid());
    QVERIFY(hasPointerSpy.wait());

    m_deco = registry.createServerSideDecorationManager(registry.interface(Registry::Interface::ServerSideDecorationManager).name, registry.interface(Registry::Interface::ServerSideDecorationManager).version, this);
    QVERIFY(m_deco->isValid());

    screens()->setCurrent(0);
    Cursor::setPos(QPoint(640, 512));
}

void TransientPlacementTest::cleanup()
{
    delete m_deco;
    m_deco = nullptr;
    delete m_compositor;
    m_compositor = nullptr;
    delete m_seat;
    m_seat = nullptr;
    delete m_shm;
    m_shm = nullptr;
    delete m_shell;
    m_shell = nullptr;
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
}

AbstractClient *TransientPlacementTest::showWindow(const QSize &size, bool decorated, KWayland::Client::Surface *parent, const QPoint &offset)
{
    using namespace KWayland::Client;
#define VERIFY(statement) \
    if (!QTest::qVerify((statement), #statement, "", __FILE__, __LINE__))\
        return nullptr;
#define COMPARE(actual, expected) \
    if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__))\
        return nullptr;
    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    VERIFY(clientAddedSpy.isValid());

    Surface *surface = m_compositor->createSurface(m_compositor);
    VERIFY(surface);
    ShellSurface *shellSurface = m_shell->createSurface(surface, surface);
    VERIFY(shellSurface);
    if (parent) {
        shellSurface->setTransient(parent, offset);
    }
    if (decorated) {
        auto deco = m_deco->create(surface, surface);
        QSignalSpy decoSpy(deco, &ServerSideDecoration::modeChanged);
        VERIFY(decoSpy.isValid());
        VERIFY(decoSpy.wait());
        deco->requestMode(ServerSideDecoration::Mode::Server);
        VERIFY(decoSpy.wait());
        COMPARE(deco->mode(), ServerSideDecoration::Mode::Server);
    }
    // let's render
    QImage img(size, QImage::Format_ARGB32);
    img.fill(Qt::blue);
    surface->attachBuffer(m_shm->createBuffer(img));
    surface->damage(QRect(QPoint(0, 0), size));
    surface->commit(Surface::CommitFlag::None);

    m_connection->flush();
    VERIFY(clientAddedSpy.wait());
    AbstractClient *c = workspace()->activeClient();
    VERIFY(c);
    COMPARE(clientAddedSpy.first().first().value<ShellClient*>(), c);

#undef VERIFY
#undef COMPARE

    return c;
}

KWayland::Client::Surface *TransientPlacementTest::surfaceForClient(AbstractClient *c) const
{
    const auto &surfaces = KWayland::Client::Surface::all();
    auto it = std::find_if(surfaces.begin(), surfaces.end(), [c] (KWayland::Client::Surface *s) { return s->id() == c->surface()->id(); });
    if (it != surfaces.end()) {
        return *it;
    }
    return nullptr;
}

void TransientPlacementTest::testSimplePosition_data()
{
    QTest::addColumn<QSize>("parentSize");
    QTest::addColumn<QPoint>("parentPosition");
    QTest::addColumn<QSize>("transientSize");
    QTest::addColumn<QPoint>("transientOffset");
    QTest::addColumn<QRect>("expectedGeometry");

    QTest::newRow("0/0") << QSize(640, 512) << QPoint(0, 0) << QSize(10, 100) << QPoint(0, 0) << QRect(0, 0, 10, 100);
    QTest::newRow("bottomRight") << QSize(640, 512) << QPoint(0, 0) << QSize(10, 100) << QPoint(639, 511) << QRect(639, 511, 10, 100);
    QTest::newRow("offset") << QSize(640, 512) << QPoint(200, 300) << QSize(100, 10) << QPoint(320, 256) << QRect(520, 556, 100, 10);
    QTest::newRow("right border") << QSize(1280, 1024) << QPoint(0, 0) << QSize(10, 100) << QPoint(1279, 50) << QRect(1269, 50, 10, 100);
    QTest::newRow("bottom border") << QSize(1280, 1024) << QPoint(0, 0) << QSize(10, 100) << QPoint(512, 1020) << QRect(512, 920, 10, 100);
    QTest::newRow("bottom right") << QSize(1280, 1024) << QPoint(0, 0) << QSize(10, 100) << QPoint(1279, 1020) << QRect(1269, 920, 10, 100);
    QTest::newRow("top border") << QSize(1280, 1024) << QPoint(0, -100) << QSize(10, 100) << QPoint(512, 50) << QRect(512, 0, 10, 100);
    QTest::newRow("left border") << QSize(1280, 1024) << QPoint(-100, 0) << QSize(100, 10) << QPoint(50, 512) << QRect(0, 512, 100, 10);
    QTest::newRow("top left") << QSize(1280, 1024) << QPoint(-100, -100) << QSize(100, 100) << QPoint(50, 50) << QRect(0, 0, 100, 100);
    QTest::newRow("bottom left") << QSize(1280, 1024) << QPoint(-100, 0) << QSize(100, 100) << QPoint(50, 1000) << QRect(0, 900, 100, 100);
}

void TransientPlacementTest::testSimplePosition()
{
    // this test verifies that the position of a transient window is taken from the passed position
    // there are no further constraints like window too large to fit screen, cascading transients, etc
    // some test cases also verify that the transient fits on the screen
    QFETCH(QSize, parentSize);
    AbstractClient *parent = showWindow(parentSize);
    QVERIFY(parent->clientPos().isNull());
    QVERIFY(!parent->isDecorated());
    QFETCH(QPoint, parentPosition);
    parent->move(parentPosition);
    QFETCH(QSize, transientSize);
    QFETCH(QPoint, transientOffset);
    AbstractClient *transient = showWindow(transientSize, false, surfaceForClient(parent), transientOffset);
    QVERIFY(transient);
    QVERIFY(!transient->isDecorated());
    QVERIFY(transient->hasTransientPlacementHint());
    QTEST(transient->geometry(), "expectedGeometry");
}

void TransientPlacementTest::testDecorationPosition_data()
{
    QTest::addColumn<QSize>("parentSize");
    QTest::addColumn<QPoint>("parentPosition");
    QTest::addColumn<QSize>("transientSize");
    QTest::addColumn<QPoint>("transientOffset");
    QTest::addColumn<QRect>("expectedGeometry");

    QTest::newRow("0/0") << QSize(640, 512) << QPoint(0, 0) << QSize(10, 100) << QPoint(0, 0) << QRect(0, 0, 10, 100);
    QTest::newRow("bottomRight") << QSize(640, 512) << QPoint(0, 0) << QSize(10, 100) << QPoint(639, 511) << QRect(639, 511, 10, 100);
    QTest::newRow("offset") << QSize(640, 512) << QPoint(200, 300) << QSize(100, 10) << QPoint(320, 256) << QRect(520, 556, 100, 10);
}

void TransientPlacementTest::testDecorationPosition()
{
    // this test verifies that a transient window is correctly placed if the parent window has a
    // server side decoration
    QFETCH(QSize, parentSize);
    AbstractClient *parent = showWindow(parentSize, true);
    QVERIFY(!parent->clientPos().isNull());
    QVERIFY(parent->isDecorated());
    QFETCH(QPoint, parentPosition);
    parent->move(parentPosition);
    QFETCH(QSize, transientSize);
    QFETCH(QPoint, transientOffset);
    AbstractClient *transient = showWindow(transientSize, false, surfaceForClient(parent), transientOffset);
    QVERIFY(transient);
    QVERIFY(!transient->isDecorated());
    QVERIFY(transient->hasTransientPlacementHint());
    QFETCH(QRect, expectedGeometry);
    expectedGeometry.translate(parent->clientPos());
    QCOMPARE(transient->geometry(), expectedGeometry);
}

}

WAYLANDTEST_MAIN(KWin::TransientPlacementTest)
#include "transient_placement.moc"
