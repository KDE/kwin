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
#include "deleted.h"
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
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

#include <KWayland/Server/seat_interface.h>

#include <linux/input.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_pointer_input-0");

class PointerInputTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testWarpingUpdatesFocus();
    void testWarpingGeneratesPointerMotion();
    void testUpdateFocusAfterScreenChange();
    void testModifierClickUnrestrictedMove_data();
    void testModifierClickUnrestrictedMove();

private:
    void render(KWayland::Client::Surface *surface, const QSize &size = QSize(100, 50));
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::Seat *m_seat = nullptr;
    KWayland::Client::ShmPool *m_shm = nullptr;
    KWayland::Client::Shell *m_shell = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    QThread *m_thread = nullptr;
};

void PointerInputTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    qRegisterMetaType<KWin::Deleted*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    waylandServer()->backend()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(waylandServer()->backend(), "setOutputCount", Qt::DirectConnection, Q_ARG(int, 2));
    waylandServer()->init(s_socketName.toLocal8Bit());

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
    waylandServer()->initWorkspace();
}

void PointerInputTest::init()
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

void PointerInputTest::cleanup()
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

void PointerInputTest::render(KWayland::Client::Surface *surface, const QSize &size)
{
    QImage img(size, QImage::Format_ARGB32);
    img.fill(Qt::blue);
    surface->attachBuffer(m_shm->createBuffer(img));
    surface->damage(QRect(QPoint(0, 0), size));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    m_connection->flush();
}

void PointerInputTest::testWarpingUpdatesFocus()
{
    // this test verifies that warping the pointer creates pointer enter and leave events
    using namespace KWayland::Client;
    // create pointer and signal spy for enter and leave signals
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &Pointer::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(pointer, &Pointer::left);
    QVERIFY(leftSpy.isValid());

    // create a window
    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface = m_compositor->createSurface(m_compositor);
    QVERIFY(surface);
    ShellSurface *shellSurface = m_shell->createSurface(surface, surface);
    QVERIFY(shellSurface);
    render(surface);
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window = workspace()->activeClient();
    QVERIFY(window);

    // currently there should not be a focused pointer surface
    QVERIFY(!waylandServer()->seat()->focusedPointerSurface());
    QVERIFY(!pointer->enteredSurface());

    // enter
    Cursor::setPos(QPoint(25, 25));
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 1);
    // window should have focus
    QCOMPARE(pointer->enteredSurface(), surface);
    // also on the server
    QCOMPARE(waylandServer()->seat()->focusedPointerSurface(), window->surface());

    // and out again
    Cursor::setPos(QPoint(250, 250));;
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.count(), 1);
    // there should not be a focused pointer surface anymore
    QVERIFY(!waylandServer()->seat()->focusedPointerSurface());
    QVERIFY(!pointer->enteredSurface());
}

void PointerInputTest::testWarpingGeneratesPointerMotion()
{
    // this test verifies that warping the pointer creates pointer motion events
    using namespace KWayland::Client;
    // create pointer and signal spy for enter and motion
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &Pointer::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy movedSpy(pointer, &Pointer::motion);
    QVERIFY(movedSpy.isValid());

    // create a window
    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface = m_compositor->createSurface(m_compositor);
    QVERIFY(surface);
    ShellSurface *shellSurface = m_shell->createSurface(surface, surface);
    QVERIFY(shellSurface);
    render(surface);
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window = workspace()->activeClient();
    QVERIFY(window);

    // enter
    waylandServer()->backend()->pointerMotion(QPointF(25, 25), 1);
    QVERIFY(enteredSpy.wait());
    // we get a move event together with the enter, that's actually wrong but also shouldn't harm
    QVERIFY(movedSpy.wait());
    QCOMPARE(movedSpy.count(), 1);
    QCOMPARE(movedSpy.first().first().toPointF(), QPointF(25, 25));

    // now warp
    Cursor::setPos(QPoint(26, 26));
    QVERIFY(movedSpy.wait());
    QCOMPARE(movedSpy.count(), 2);
    QCOMPARE(movedSpy.last().first().toPointF(), QPointF(26, 26));
}

void PointerInputTest::testUpdateFocusAfterScreenChange()
{
    // this test verifies that a pointer enter event is generated when the cursor changes to another
    // screen due to removal of screen
    using namespace KWayland::Client;
    // ensure cursor is on second screen
    Cursor::setPos(1500, 300);

    // create pointer and signal spy for enter and motion
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &Pointer::entered);
    QVERIFY(enteredSpy.isValid());

    // create a window
    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface = m_compositor->createSurface(m_compositor);
    QVERIFY(surface);
    ShellSurface *shellSurface = m_shell->createSurface(surface, surface);
    QVERIFY(shellSurface);
    render(surface, QSize(1280, 1024));
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window = workspace()->activeClient();
    QVERIFY(window);
    QVERIFY(!window->geometry().contains(Cursor::pos()));

    QSignalSpy screensChangedSpy(screens(), &Screens::changed);
    QVERIFY(screensChangedSpy.isValid());
    // now let's remove the screen containing the cursor
    QMetaObject::invokeMethod(waylandServer()->backend(), "setOutputCount", Qt::DirectConnection, Q_ARG(int, 1));
    QMetaObject::invokeMethod(waylandServer()->backend(), "sizeChanged", Qt::QueuedConnection);
    QVERIFY(screensChangedSpy.wait());
    QCOMPARE(screens()->count(), 1);

    // this should have warped the cursor
    QCOMPARE(Cursor::pos(), QPoint(639, 511));
    QVERIFY(window->geometry().contains(Cursor::pos()));

    // and we should get an enter event
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 1);
}

void PointerInputTest::testModifierClickUnrestrictedMove_data()
{
    QTest::addColumn<int>("modifierKey");
    QTest::addColumn<int>("mouseButton");
    QTest::addColumn<QString>("modKey");

    const QString alt = QStringLiteral("Alt");
    const QString meta = QStringLiteral("Meta");

    QTest::newRow("Left Alt + Left Click")    << KEY_LEFTALT  << BTN_LEFT   << alt;
    QTest::newRow("Left Alt + Right Click")   << KEY_LEFTALT  << BTN_RIGHT  << alt;
    QTest::newRow("Left Alt + Middle Click")  << KEY_LEFTALT  << BTN_MIDDLE << alt;
    QTest::newRow("Right Alt + Left Click")   << KEY_RIGHTALT << BTN_LEFT   << alt;
    QTest::newRow("Right Alt + Right Click")  << KEY_RIGHTALT << BTN_RIGHT  << alt;
    QTest::newRow("Right Alt + Middle Click") << KEY_RIGHTALT << BTN_MIDDLE << alt;
    // now everything with meta
    QTest::newRow("Left Meta + Left Click")    << KEY_LEFTMETA  << BTN_LEFT   << meta;
    QTest::newRow("Left Meta + Right Click")   << KEY_LEFTMETA  << BTN_RIGHT  << meta;
    QTest::newRow("Left Meta + Middle Click")  << KEY_LEFTMETA  << BTN_MIDDLE << meta;
    QTest::newRow("Right Meta + Left Click")   << KEY_RIGHTMETA << BTN_LEFT   << meta;
    QTest::newRow("Right Meta + Right Click")  << KEY_RIGHTMETA << BTN_RIGHT  << meta;
    QTest::newRow("Right Meta + Middle Click") << KEY_RIGHTMETA << BTN_MIDDLE << meta;
}

void PointerInputTest::testModifierClickUnrestrictedMove()
{
    // this test ensures that Alt+mouse button press triggers unrestricted move
    using namespace KWayland::Client;
    // create pointer and signal spy for button events
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy buttonSpy(pointer, &Pointer::buttonStateChanged);
    QVERIFY(buttonSpy.isValid());

    // first modify the config for this run
    QFETCH(QString, modKey);
    KConfigGroup group = kwinApp()->config()->group("MouseBindings");
    group.writeEntry("CommandAllKey", modKey);
    group.writeEntry("CommandAll1", "Move");
    group.writeEntry("CommandAll2", "Move");
    group.writeEntry("CommandAll3", "Move");
    group.sync();
    workspace()->slotReconfigure();

    // create a window
    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface = m_compositor->createSurface(m_compositor);
    QVERIFY(surface);
    ShellSurface *shellSurface = m_shell->createSurface(surface, surface);
    QVERIFY(shellSurface);
    render(surface);
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window = workspace()->activeClient();
    QVERIFY(window);

    // move cursor on window
    Cursor::setPos(window->geometry().center());

    // simulate modifier+click
    quint32 timestamp = 1;
    QFETCH(int, modifierKey);
    QFETCH(int, mouseButton);
    waylandServer()->backend()->keyboardKeyPressed(modifierKey, timestamp++);
    QVERIFY(!window->isMove());
    waylandServer()->backend()->pointerButtonPressed(mouseButton, timestamp++);
    QVERIFY(window->isMove());
    // release modifier should not change it
    waylandServer()->backend()->keyboardKeyReleased(modifierKey, timestamp++);
    QVERIFY(window->isMove());
    // but releasing the key should end move/resize
    waylandServer()->backend()->pointerButtonReleased(mouseButton, timestamp++);
    QVERIFY(!window->isMove());

    // all of that should not have triggered button events on the surface
    QCOMPARE(buttonSpy.count(), 0);
    // also waiting shouldn't give us the event
    QVERIFY(!buttonSpy.wait(100));
}

}

WAYLANDTEST_MAIN(KWin::PointerInputTest)
#include "pointer_input.moc"
