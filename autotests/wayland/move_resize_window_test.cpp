
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
#include "cursor.h"
#include "effects.h"
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

class MoveResizeWindowTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testMove();
    void testPackTo_data();
    void testPackTo();
    void testPackAgainstClient_data();
    void testPackAgainstClient();

private:
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::ShmPool *m_shm = nullptr;
    KWayland::Client::Shell *m_shell = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    QThread *m_thread = nullptr;
};

void MoveResizeWindowTest::initTestCase()
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
    QCOMPARE(screens()->count(), 1);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
}

void MoveResizeWindowTest::init()
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

    screens()->setCurrent(0);
}

void MoveResizeWindowTest::cleanup()
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

void MoveResizeWindowTest::testMove()
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
    QSignalSpy geometryChangedSpy(c, &AbstractClient::geometryChanged);
    QVERIFY(geometryChangedSpy.isValid());
    QSignalSpy startMoveResizedSpy(c, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(startMoveResizedSpy.isValid());
    QSignalSpy moveResizedChangedSpy(c, &AbstractClient::moveResizedChanged);
    QVERIFY(moveResizedChangedSpy.isValid());
    QSignalSpy clientStepUserMovedResizedSpy(c, &AbstractClient::clientStepUserMovedResized);
    QVERIFY(clientStepUserMovedResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(c, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    // effects signal handlers
    QSignalSpy windowStartUserMovedResizedSpy(effects, &EffectsHandler::windowStartUserMovedResized);
    QVERIFY(windowStartUserMovedResizedSpy.isValid());
    QSignalSpy windowStepUserMovedResizedSpy(effects, &EffectsHandler::windowStepUserMovedResized);
    QVERIFY(windowStepUserMovedResizedSpy.isValid());
    QSignalSpy windowFinishUserMovedResizedSpy(effects, &EffectsHandler::windowFinishUserMovedResized);
    QVERIFY(windowFinishUserMovedResizedSpy.isValid());

    // begin move
    QVERIFY(workspace()->getMovingClient() == nullptr);
    QCOMPARE(c->isMove(), false);
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->getMovingClient(), c);
    QCOMPARE(startMoveResizedSpy.count(), 1);
    QCOMPARE(moveResizedChangedSpy.count(), 1);
    QCOMPARE(windowStartUserMovedResizedSpy.count(), 1);
    QCOMPARE(c->isMove(), true);
    QCOMPARE(c->geometryRestore(), QRect(0, 0, 100, 50));

    // send some key events, not going through input redirection
    const QPoint cursorPos = Cursor::pos();
    c->keyPressEvent(Qt::Key_Right);
    c->updateMoveResize(Cursor::pos());
    QCOMPARE(Cursor::pos(), cursorPos + QPoint(8, 0));
    QEXPECT_FAIL("", "First event is ignored", Continue);
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);

    c->keyPressEvent(Qt::Key_Right);
    c->updateMoveResize(Cursor::pos());
    QCOMPARE(Cursor::pos(), cursorPos + QPoint(16, 0));
    QEXPECT_FAIL("", "First event is ignored", Continue);
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 2);
    QEXPECT_FAIL("", "First event is ignored", Continue);
    QCOMPARE(windowStepUserMovedResizedSpy.count(), 2);

    c->keyPressEvent(Qt::Key_Down | Qt::ALT);
    c->updateMoveResize(Cursor::pos());
    QEXPECT_FAIL("", "First event is ignored", Continue);
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 3);
    QCOMPARE(c->geometry(), QRect(16, 32, 100, 50));
    QCOMPARE(Cursor::pos(), cursorPos + QPoint(16, 32));

    // let's end
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 0);
    c->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(moveResizedChangedSpy.count(), 2);
    QCOMPARE(windowFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(c->geometry(), QRect(16, 32, 100, 50));
    QCOMPARE(c->isMove(), false);
    QVERIFY(workspace()->getMovingClient() == nullptr);
}

void MoveResizeWindowTest::testPackTo_data()
{
    QTest::addColumn<QString>("methodCall");
    QTest::addColumn<QRect>("expectedGeometry");

    QTest::newRow("left")  << QStringLiteral("slotWindowPackLeft")  << QRect(0, 487, 100, 50);
    QTest::newRow("up")    << QStringLiteral("slotWindowPackUp")    << QRect(590, 0, 100, 50);
    QTest::newRow("right") << QStringLiteral("slotWindowPackRight") << QRect(1180, 487, 100, 50);
    QTest::newRow("down")  << QStringLiteral("slotWindowPackDown")  << QRect(590, 974, 100, 50);
}

void MoveResizeWindowTest::testPackTo()
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

    // let's place it centered
    Placement::self()->placeCentered(c, QRect(0, 0, 1280, 1024));
    QCOMPARE(c->geometry(), QRect(590, 487, 100, 50));

    QFETCH(QString, methodCall);
    QMetaObject::invokeMethod(workspace(), methodCall.toLocal8Bit().constData());
    QTEST(c->geometry(), "expectedGeometry");
}

void MoveResizeWindowTest::testPackAgainstClient_data()
{
    QTest::addColumn<QString>("methodCall");
    QTest::addColumn<QRect>("expectedGeometry");

    QTest::newRow("left")  << QStringLiteral("slotWindowPackLeft")  << QRect(10, 487, 100, 50);
    QTest::newRow("up")    << QStringLiteral("slotWindowPackUp")    << QRect(590, 10, 100, 50);
    QTest::newRow("right") << QStringLiteral("slotWindowPackRight") << QRect(1170, 487, 100, 50);
    QTest::newRow("down")  << QStringLiteral("slotWindowPackDown")  << QRect(590, 964, 100, 50);
}

void MoveResizeWindowTest::testPackAgainstClient()
{
    using namespace KWayland::Client;

    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());

    QScopedPointer<Surface> surface1(m_compositor->createSurface());
    QVERIFY(!surface1.isNull());
    QScopedPointer<Surface> surface2(m_compositor->createSurface());
    QVERIFY(!surface2.isNull());
    QScopedPointer<Surface> surface3(m_compositor->createSurface());
    QVERIFY(!surface3.isNull());
    QScopedPointer<Surface> surface4(m_compositor->createSurface());
    QVERIFY(!surface4.isNull());

    QScopedPointer<ShellSurface> shellSurface1(m_shell->createSurface(surface1.data()));
    QVERIFY(!shellSurface1.isNull());
    QScopedPointer<ShellSurface> shellSurface2(m_shell->createSurface(surface2.data()));
    QVERIFY(!shellSurface2.isNull());
    QScopedPointer<ShellSurface> shellSurface3(m_shell->createSurface(surface3.data()));
    QVERIFY(!shellSurface3.isNull());
    QScopedPointer<ShellSurface> shellSurface4(m_shell->createSurface(surface4.data()));
    QVERIFY(!shellSurface4.isNull());
    auto renderWindow = [this, &clientAddedSpy] (Surface *surface, const QString &methodCall, const QRect &expectedGeometry) {
        // let's render
        QImage img(QSize(10, 10), QImage::Format_ARGB32);
        img.fill(Qt::blue);
        surface->attachBuffer(m_shm->createBuffer(img));
        surface->damage(QRect(0, 0, 10, 10));
        surface->commit(Surface::CommitFlag::None);

        m_connection->flush();
        QVERIFY(clientAddedSpy.wait());
        AbstractClient *c = workspace()->activeClient();
        QVERIFY(c);
        QCOMPARE(clientAddedSpy.first().first().value<ShellClient*>(), c);
        QCOMPARE(c->geometry().size(), QSize(10, 10));
        clientAddedSpy.clear();
        // let's place it centered
        Placement::self()->placeCentered(c, QRect(0, 0, 1280, 1024));
        QCOMPARE(c->geometry(), QRect(635, 507, 10, 10));
        QMetaObject::invokeMethod(workspace(), methodCall.toLocal8Bit().constData());
        QCOMPARE(c->geometry(), expectedGeometry);
    };
    renderWindow(surface1.data(), QStringLiteral("slotWindowPackLeft"),  QRect(0, 507, 10, 10));
    renderWindow(surface2.data(), QStringLiteral("slotWindowPackUp"),    QRect(635, 0, 10, 10));
    renderWindow(surface3.data(), QStringLiteral("slotWindowPackRight"), QRect(1270, 507, 10, 10));
    renderWindow(surface4.data(), QStringLiteral("slotWindowPackDown"),  QRect(635, 1014, 10, 10));

    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<ShellSurface> shellSurface(m_shell->createSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
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
    // let's place it centered
    Placement::self()->placeCentered(c, QRect(0, 0, 1280, 1024));
    QCOMPARE(c->geometry(), QRect(590, 487, 100, 50));

    QFETCH(QString, methodCall);
    QMetaObject::invokeMethod(workspace(), methodCall.toLocal8Bit().constData());
    QTEST(c->geometry(), "expectedGeometry");
}

}

WAYLANTEST_MAIN(KWin::MoveResizeWindowTest)
#include "move_resize_window_test.moc"
