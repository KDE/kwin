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

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

//screenlocker
#include <KScreenLocker/KsldApp>

#include <linux/input.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_lock_screen-0");

class LockScreenTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testPointer();
    void testPointerButton();
    void testPointerAxis();
    void testScreenEdge();

private:
    void unlock();
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::Seat *m_seat = nullptr;
    KWayland::Client::ShmPool *m_shm = nullptr;
    KWayland::Client::Shell *m_shell = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    QThread *m_thread = nullptr;
};

void LockScreenTest::unlock()
{
    using namespace ScreenLocker;
    const auto children = KSldApp::self()->children();
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (qstrcmp((*it)->metaObject()->className(), "LogindIntegration") != 0) {
            continue;
        }
        QMetaObject::invokeMethod(*it, "requestUnlock");
        break;
    }
}

void LockScreenTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    waylandServer()->backend()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(waylandServer()->backend(), "setOutputCount", Qt::DirectConnection, Q_ARG(int, 2));
    waylandServer()->init(s_socketName.toLocal8Bit());

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
    waylandServer()->initWorkspace();
}

void LockScreenTest::init()
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

    screens()->setCurrent(0);
    Cursor::setPos(QPoint(640, 512));
}

void LockScreenTest::cleanup()
{
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

void LockScreenTest::testPointer()
{
    using namespace KWayland::Client;

    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());

    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<Pointer> pointer(m_seat->createPointer());
    QVERIFY(!pointer.isNull());
    QSignalSpy enteredSpy(pointer.data(), &Pointer::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(pointer.data(), &Pointer::left);
    QVERIFY(leftSpy.isValid());

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

    // first move cursor into the center of the window
    quint32 timestamp = 1;
    waylandServer()->backend()->pointerMotion(c->geometry().center(), timestamp++);
    QVERIFY(enteredSpy.wait());

    QVERIFY(!waylandServer()->isScreenLocked());
    QSignalSpy lockStateChangedSpy(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged);
    QVERIFY(lockStateChangedSpy.isValid());
    ScreenLocker::KSldApp::self()->lock(ScreenLocker::EstablishLock::Immediate);
    QCOMPARE(lockStateChangedSpy.count(), 1);
    QVERIFY(waylandServer()->isScreenLocked());

    QEXPECT_FAIL("", "Adding the lock screen window should send left event", Continue);
    QVERIFY(leftSpy.wait(100));
    QEXPECT_FAIL("", "Adding the lock screen window should send left event", Continue);
    QCOMPARE(leftSpy.count(), 1);

    // simulate moving out in and out again
    waylandServer()->backend()->pointerMotion(c->geometry().bottomRight() + QPoint(100, 100), timestamp++);
    waylandServer()->backend()->pointerMotion(c->geometry().center(), timestamp++);
    waylandServer()->backend()->pointerMotion(c->geometry().bottomRight() + QPoint(100, 100), timestamp++);
    QEXPECT_FAIL("", "Adding the lock screen window should send left event", Continue);
    QVERIFY(!leftSpy.wait(100));
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(enteredSpy.count(), 1);

    // go back on the window
    waylandServer()->backend()->pointerMotion(c->geometry().center(), timestamp++);
    // and unlock
    QCOMPARE(lockStateChangedSpy.count(), 1);
    unlock();
    if (lockStateChangedSpy.count() < 2) {
        QVERIFY(lockStateChangedSpy.wait());
    }
    QCOMPARE(lockStateChangedSpy.count(), 2);
    QVERIFY(!waylandServer()->isScreenLocked());

    QEXPECT_FAIL("", "Focus doesn't move back on surface removal", Continue);
    QVERIFY(enteredSpy.wait(100));
    QEXPECT_FAIL("", "Focus doesn't move back on surface removal", Continue);
    QCOMPARE(enteredSpy.count(), 2);
    // move on the window
    waylandServer()->backend()->pointerMotion(c->geometry().center() + QPoint(100, 100), timestamp++);
    waylandServer()->backend()->pointerMotion(c->geometry().center(), timestamp++);
    QEXPECT_FAIL("", "Focus doesn't move back on surface removal", Continue);
    QVERIFY(!enteredSpy.wait(100));
    QCOMPARE(enteredSpy.count(), 2);
}

void LockScreenTest::testPointerButton()
{
    using namespace KWayland::Client;

    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());

    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<Pointer> pointer(m_seat->createPointer());
    QVERIFY(!pointer.isNull());
    QSignalSpy buttonChangedSpy(pointer.data(), &Pointer::buttonStateChanged);
    QVERIFY(buttonChangedSpy.isValid());

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

    // first move cursor into the center of the window
    quint32 timestamp = 1;
    waylandServer()->backend()->pointerMotion(c->geometry().center(), timestamp++);
    // and simulate a click
    waylandServer()->backend()->pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(buttonChangedSpy.wait());
    waylandServer()->backend()->pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(buttonChangedSpy.wait());

    QVERIFY(!waylandServer()->isScreenLocked());
    QSignalSpy lockStateChangedSpy(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged);
    QVERIFY(lockStateChangedSpy.isValid());
    ScreenLocker::KSldApp::self()->lock(ScreenLocker::EstablishLock::Immediate);
    QCOMPARE(lockStateChangedSpy.count(), 1);
    QVERIFY(waylandServer()->isScreenLocked());

    // and simulate a click
    waylandServer()->backend()->pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(!buttonChangedSpy.wait(100));
    waylandServer()->backend()->pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(!buttonChangedSpy.wait(100));

    // and unlock
    QCOMPARE(lockStateChangedSpy.count(), 1);
    unlock();
    if (lockStateChangedSpy.count() < 2) {
        QVERIFY(lockStateChangedSpy.wait());
    }
    QCOMPARE(lockStateChangedSpy.count(), 2);
    QVERIFY(!waylandServer()->isScreenLocked());


    // and click again
    waylandServer()->backend()->pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(buttonChangedSpy.wait());
    waylandServer()->backend()->pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(buttonChangedSpy.wait());
}

void LockScreenTest::testPointerAxis()
{
    using namespace KWayland::Client;

    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());

    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<Pointer> pointer(m_seat->createPointer());
    QVERIFY(!pointer.isNull());
    QSignalSpy axisChangedSpy(pointer.data(), &Pointer::axisChanged);
    QVERIFY(axisChangedSpy.isValid());

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

    // first move cursor into the center of the window
    quint32 timestamp = 1;
    waylandServer()->backend()->pointerMotion(c->geometry().center(), timestamp++);
    // and simulate axis
    waylandServer()->backend()->pointerAxisHorizontal(5.0, timestamp++);
    QVERIFY(axisChangedSpy.wait());

    QVERIFY(!waylandServer()->isScreenLocked());
    QSignalSpy lockStateChangedSpy(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged);
    QVERIFY(lockStateChangedSpy.isValid());
    ScreenLocker::KSldApp::self()->lock(ScreenLocker::EstablishLock::Immediate);
    QCOMPARE(lockStateChangedSpy.count(), 1);
    QVERIFY(waylandServer()->isScreenLocked());

    // and simulate axis
    waylandServer()->backend()->pointerAxisHorizontal(5.0, timestamp++);
    QVERIFY(!axisChangedSpy.wait(100));
    waylandServer()->backend()->pointerAxisVertical(5.0, timestamp++);
    QVERIFY(!axisChangedSpy.wait(100));

    // and unlock
    QCOMPARE(lockStateChangedSpy.count(), 1);
    unlock();
    if (lockStateChangedSpy.count() < 2) {
        QVERIFY(lockStateChangedSpy.wait());
    }
    QCOMPARE(lockStateChangedSpy.count(), 2);
    QVERIFY(!waylandServer()->isScreenLocked());

    // and move axis again
    waylandServer()->backend()->pointerAxisHorizontal(5.0, timestamp++);
    QVERIFY(axisChangedSpy.wait());
    waylandServer()->backend()->pointerAxisVertical(5.0, timestamp++);
    QVERIFY(axisChangedSpy.wait());
}

void LockScreenTest::testScreenEdge()
{
    QSignalSpy screenEdgeSpy(ScreenEdges::self(), &ScreenEdges::approaching);
    QVERIFY(screenEdgeSpy.isValid());
    QCOMPARE(screenEdgeSpy.count(), 0);

    quint32 timestamp = 1;
    waylandServer()->backend()->pointerMotion(QPoint(5, 5), timestamp++);
    QCOMPARE(screenEdgeSpy.count(), 1);

    QVERIFY(!waylandServer()->isScreenLocked());
    QSignalSpy lockStateChangedSpy(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged);
    QVERIFY(lockStateChangedSpy.isValid());
    ScreenLocker::KSldApp::self()->lock(ScreenLocker::EstablishLock::Immediate);
    QCOMPARE(lockStateChangedSpy.count(), 1);
    QVERIFY(waylandServer()->isScreenLocked());

    waylandServer()->backend()->pointerMotion(QPoint(4, 4), timestamp++);
    QCOMPARE(screenEdgeSpy.count(), 1);

    // and unlock
    QCOMPARE(lockStateChangedSpy.count(), 1);
    unlock();
    if (lockStateChangedSpy.count() < 2) {
        QVERIFY(lockStateChangedSpy.wait());
    }
    QCOMPARE(lockStateChangedSpy.count(), 2);
    QVERIFY(!waylandServer()->isScreenLocked());

    waylandServer()->backend()->pointerMotion(QPoint(5, 5), timestamp++);
    QCOMPARE(screenEdgeSpy.count(), 2);
}

}

WAYLANTEST_MAIN(KWin::LockScreenTest)
#include "lockscreen.moc"
