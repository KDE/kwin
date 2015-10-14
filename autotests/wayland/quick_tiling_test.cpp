/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "shell_client.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

Q_DECLARE_METATYPE(KWin::AbstractClient::QuickTileMode)
Q_DECLARE_METATYPE(KWin::MaximizeMode)

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_quick_tiling-0");

class QuickTilingTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testQuickTiling_data();
    void testQuickTiling();
    void testQuickMaximizing_data();
    void testQuickMaximizing();

private:
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::ShmPool *m_shm = nullptr;
    KWayland::Client::Shell *m_shell = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    QThread *m_thread = nullptr;
};

void QuickTilingTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    qRegisterMetaType<KWin::MaximizeMode>("MaximizeMode");
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    waylandServer()->backend()->setInitialWindowSize(QSize(1280, 1024));
    waylandServer()->init(s_socketName.toLocal8Bit());
    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
}

void QuickTilingTest::init()
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
    QSignalSpy allAnnounced(&registry, &Registry::interfacesAnnounced);
    QVERIFY(allAnnounced.isValid());
    QVERIFY(shmSpy.isValid());
    QVERIFY(shellSpy.isValid());
    QVERIFY(compositorSpy.isValid());
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(allAnnounced.wait());
    QVERIFY(!compositorSpy.isEmpty());
    QVERIFY(!shmSpy.isEmpty());
    QVERIFY(!shellSpy.isEmpty());

    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);
    QVERIFY(m_compositor->isValid());
    m_shm = registry.createShmPool(shmSpy.first().first().value<quint32>(), shmSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shm->isValid());
    m_shell = registry.createShell(shellSpy.first().first().value<quint32>(), shellSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shell->isValid());
}

void QuickTilingTest::cleanup()
{
    delete m_compositor;
    m_compositor = nullptr;
    delete m_shm;
    m_shm = nullptr;
    delete m_shell;
    m_shell = nullptr;
    delete m_queue;
    m_queue = nullptr;
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }

}

void QuickTilingTest::testQuickTiling_data()
{
    QTest::addColumn<AbstractClient::QuickTileMode>("mode");
    QTest::addColumn<QRect>("expectedGeometry");

#define FLAG(name) AbstractClient::QuickTileMode(AbstractClient::QuickTile##name)

    QTest::newRow("left")   << FLAG(Left)   << QRect(0, 0, 640, 1024);
    QTest::newRow("top")    << FLAG(Top)    << QRect(0, 0, 1280, 512);
    QTest::newRow("right")  << FLAG(Right)  << QRect(640, 0, 640, 1024);
    QTest::newRow("bottom") << FLAG(Bottom) << QRect(0, 512, 1280, 512);

    QTest::newRow("top left")     << (FLAG(Left)  | FLAG(Top))    << QRect(0, 0, 640, 512);
    QTest::newRow("top right")    << (FLAG(Right) | FLAG(Top))    << QRect(640, 0, 640, 512);
    QTest::newRow("bottom left")  << (FLAG(Left)  | FLAG(Bottom)) << QRect(0, 512, 640, 512);
    QTest::newRow("bottom right") << (FLAG(Right) | FLAG(Bottom)) << QRect(640, 512, 640, 512);

    QTest::newRow("maximize") << FLAG(Maximize) << QRect(0, 0, 1280, 1024);

#undef FLAG
}

void QuickTilingTest::testQuickTiling()
{
    using namespace KWayland::Client;

    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());

    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<ShellSurface> shellSurface(m_shell->createSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QSignalSpy sizeChangeSpy(shellSurface.data(), &ShellSurface::sizeChanged);
    QVERIFY(sizeChangeSpy.isValid());
    // let's render
    QImage img(QSize(100, 50), QImage::Format_ARGB32);
    img.fill(Qt::blue);
    surface->attachBuffer(m_shm->createBuffer(img));
    surface->damage(QRect(0, 0, 100, 50));
    surface->commit(Surface::CommitFlag::None);

    m_connection->flush();
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *c = workspace()->activeClient();
    QVERIFY(c);
    QCOMPARE(clientAddedSpy.first().first().value<ShellClient*>(), c);
    QCOMPARE(c->geometry(), QRect(0, 0, 100, 50));
    QCOMPARE(c->quickTileMode(), AbstractClient::QuickTileNone);
    QSignalSpy quickTileChangedSpy(c, &AbstractClient::quickTileModeChanged);
    QVERIFY(quickTileChangedSpy.isValid());
    QSignalSpy geometryChangedSpy(c, &AbstractClient::geometryChanged);
    QVERIFY(geometryChangedSpy.isValid());

    QFETCH(AbstractClient::QuickTileMode, mode);
    QFETCH(QRect, expectedGeometry);
    c->setQuickTileMode(mode, true);
    QCOMPARE(quickTileChangedSpy.count(), 1);
    // at this point the geometry did not yet change
    QCOMPARE(c->geometry(), QRect(0, 0, 100, 50));
    // but quick tile mode already changed
    QCOMPARE(c->quickTileMode(), mode);

    // but we got requested a new geometry
    QVERIFY(sizeChangeSpy.wait());
    QCOMPARE(sizeChangeSpy.count(), 1);
    QCOMPARE(sizeChangeSpy.first().first().toSize(), expectedGeometry.size());

    // attach a new image
    img = QImage(expectedGeometry.size(), QImage::Format_ARGB32);
    img.fill(Qt::red);
    surface->attachBuffer(m_shm->createBuffer(img));
    surface->damage(QRect(QPoint(0, 0), expectedGeometry.size()));
    surface->commit(Surface::CommitFlag::None);
    m_connection->flush();

    QVERIFY(geometryChangedSpy.wait());
    QCOMPARE(geometryChangedSpy.count(), 1);
    QCOMPARE(c->geometry(), expectedGeometry);
}

void QuickTilingTest::testQuickMaximizing_data()
{
    QTest::addColumn<AbstractClient::QuickTileMode>("mode");

#define FLAG(name) AbstractClient::QuickTileMode(AbstractClient::QuickTile##name)

    QTest::newRow("maximize") << FLAG(Maximize);
    QTest::newRow("none") << FLAG(None);

#undef FLAG
}

void QuickTilingTest::testQuickMaximizing()
{
    using namespace KWayland::Client;

    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());

    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<ShellSurface> shellSurface(m_shell->createSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QSignalSpy sizeChangeSpy(shellSurface.data(), &ShellSurface::sizeChanged);
    QVERIFY(sizeChangeSpy.isValid());
    // let's render
    QImage img(QSize(100, 50), QImage::Format_ARGB32);
    img.fill(Qt::blue);
    surface->attachBuffer(m_shm->createBuffer(img));
    surface->damage(QRect(0, 0, 100, 50));
    surface->commit(Surface::CommitFlag::None);

    m_connection->flush();
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *c = workspace()->activeClient();
    QVERIFY(c);
    QCOMPARE(clientAddedSpy.first().first().value<ShellClient*>(), c);
    QCOMPARE(c->geometry(), QRect(0, 0, 100, 50));
    QCOMPARE(c->quickTileMode(), AbstractClient::QuickTileNone);
    QCOMPARE(c->maximizeMode(), MaximizeRestore);
    QSignalSpy quickTileChangedSpy(c, &AbstractClient::quickTileModeChanged);
    QVERIFY(quickTileChangedSpy.isValid());
    QSignalSpy geometryChangedSpy(c, &AbstractClient::geometryChanged);
    QVERIFY(geometryChangedSpy.isValid());
    QSignalSpy maximizeChangedSpy1(c, SIGNAL(clientMaximizedStateChanged(KWin::AbstractClient*,MaximizeMode)));
    QVERIFY(maximizeChangedSpy1.isValid());
    QSignalSpy maximizeChangedSpy2(c, SIGNAL(clientMaximizedStateChanged(KWin::AbstractClient*,bool,bool)));
    QVERIFY(maximizeChangedSpy2.isValid());

    c->setQuickTileMode(AbstractClient::QuickTileMaximize, true);
    QCOMPARE(quickTileChangedSpy.count(), 1);
    QCOMPARE(maximizeChangedSpy1.count(), 1);
    QCOMPARE(maximizeChangedSpy1.first().first().value<KWin::AbstractClient*>(), c);
    QCOMPARE(maximizeChangedSpy1.first().last().value<KWin::MaximizeMode>(), MaximizeFull);
    QCOMPARE(maximizeChangedSpy2.count(), 1);
    QCOMPARE(maximizeChangedSpy2.first().first().value<KWin::AbstractClient*>(), c);
    QCOMPARE(maximizeChangedSpy2.first().at(1).toBool(), true);
    QCOMPARE(maximizeChangedSpy2.first().at(2).toBool(), true);
    // at this point the geometry did not yet change
    QCOMPARE(c->geometry(), QRect(0, 0, 100, 50));
    // but quick tile mode already changed
    QCOMPARE(c->quickTileMode(), AbstractClient::QuickTileMaximize);
    QCOMPARE(c->maximizeMode(), MaximizeFull);
    QCOMPARE(c->geometryRestore(), QRect(0, 0, 100, 50));

    // but we got requested a new geometry
    QVERIFY(sizeChangeSpy.wait());
    QCOMPARE(sizeChangeSpy.count(), 1);
    QCOMPARE(sizeChangeSpy.first().first().toSize(), QSize(1280, 1024));

    // attach a new image
    img = QImage(QSize(1280, 1024), QImage::Format_ARGB32);
    img.fill(Qt::red);
    surface->attachBuffer(m_shm->createBuffer(img));
    surface->damage(QRect(0, 0, 1280, 1024));
    surface->commit(Surface::CommitFlag::None);
    m_connection->flush();

    QVERIFY(geometryChangedSpy.wait());
    QCOMPARE(geometryChangedSpy.count(), 1);
    QCOMPARE(c->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(c->geometryRestore(), QRect(0, 0, 100, 50));

    // go back to quick tile none
    QFETCH(AbstractClient::QuickTileMode, mode);
    c->setQuickTileMode(mode, true);
    QCOMPARE(quickTileChangedSpy.count(), 2);
    QCOMPARE(maximizeChangedSpy1.count(), 2);
    QCOMPARE(maximizeChangedSpy1.last().first().value<KWin::AbstractClient*>(), c);
    QCOMPARE(maximizeChangedSpy1.last().last().value<KWin::MaximizeMode>(), MaximizeRestore);
    QCOMPARE(maximizeChangedSpy2.count(), 2);
    QCOMPARE(maximizeChangedSpy2.last().first().value<KWin::AbstractClient*>(), c);
    QCOMPARE(maximizeChangedSpy2.last().at(1).toBool(), false);
    QCOMPARE(maximizeChangedSpy2.last().at(2).toBool(), false);
    QCOMPARE(c->quickTileMode(), AbstractClient::QuickTileNone);
    QCOMPARE(c->maximizeMode(), MaximizeRestore);
    // geometry not yet changed
    QCOMPARE(c->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(c->geometryRestore(), QRect(0, 0, 100, 50));
    // we got requested a new geometry
    QVERIFY(sizeChangeSpy.wait());
    QCOMPARE(sizeChangeSpy.count(), 2);
    QCOMPARE(sizeChangeSpy.last().first().toSize(), QSize(100, 50));

    // render again
    img = QImage(QSize(100, 50), QImage::Format_ARGB32);
    img.fill(Qt::yellow);
    surface->attachBuffer(m_shm->createBuffer(img));
    surface->damage(QRect(0, 0, 100, 50));
    surface->commit(Surface::CommitFlag::None);
    m_connection->flush();

    QVERIFY(geometryChangedSpy.wait());
    QCOMPARE(geometryChangedSpy.count(), 2);
    QCOMPARE(c->geometry(), QRect(0, 0, 100, 50));
    QCOMPARE(c->geometryRestore(), QRect(0, 0, 100, 50));
}

}

WAYLANTEST_MAIN(KWin::QuickTilingTest)
#include "quick_tiling_test.moc"
