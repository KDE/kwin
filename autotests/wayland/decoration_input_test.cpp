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
#include <KWayland/Client/registry.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

#include <KDecoration2/Decoration>

#include <linux/input.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_decoration_input-0");

class DecorationInputTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testAxis();
    void testDoubleClick();
    void testHover();
    void testPressToMove();

private:
    AbstractClient *showWindow();
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::ServerSideDecorationManager *m_deco = nullptr;
    KWayland::Client::Seat *m_seat = nullptr;
    KWayland::Client::ShmPool *m_shm = nullptr;
    KWayland::Client::Shell *m_shell = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    QThread *m_thread = nullptr;
};

#define MOTION(target) \
    waylandServer()->backend()->pointerMotion(target, timestamp++)

#define PRESS \
    waylandServer()->backend()->pointerButtonPressed(BTN_LEFT, timestamp++)

#define RELEASE \
    waylandServer()->backend()->pointerButtonReleased(BTN_LEFT, timestamp++)

AbstractClient *DecorationInputTest::showWindow()
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
    auto deco = m_deco->create(surface, surface);
    QSignalSpy decoSpy(deco, &ServerSideDecoration::modeChanged);
    VERIFY(decoSpy.isValid());
    VERIFY(decoSpy.wait());
    deco->requestMode(ServerSideDecoration::Mode::Server);
    VERIFY(decoSpy.wait());
    COMPARE(deco->mode(), ServerSideDecoration::Mode::Server);
    // let's render
    QImage img(QSize(500, 50), QImage::Format_ARGB32);
    img.fill(Qt::blue);
    surface->attachBuffer(m_shm->createBuffer(img));
    surface->damage(QRect(0, 0, 500, 50));
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

void DecorationInputTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    waylandServer()->backend()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(waylandServer()->backend(), "setOutputCount", Qt::DirectConnection, Q_ARG(int, 2));
    waylandServer()->init(s_socketName.toLocal8Bit());

    // change some options
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group(QStringLiteral("MouseBindings")).writeEntry("CommandTitlebarWheel", QStringLiteral("above/below"));
    config->group(QStringLiteral("Windows")).writeEntry("TitlebarDoubleClickCommand", QStringLiteral("OnAllDesktops"));
    config->group(QStringLiteral("Desktops")).writeEntry("Number", 2);
    config->sync();

    kwinApp()->setConfig(config);

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
    waylandServer()->initWorkspace();
}

void DecorationInputTest::init()
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
    QSignalSpy hasPointerSpy(m_seat, &Seat::hasPointerChanged);
    QVERIFY(hasPointerSpy.isValid());
    QVERIFY(hasPointerSpy.wait());

    screens()->setCurrent(0);
    Cursor::setPos(QPoint(640, 512));
}

void DecorationInputTest::cleanup()
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

void DecorationInputTest::testAxis()
{
    AbstractClient *c = showWindow();
    QVERIFY(c);
    QVERIFY(c->isDecorated());
    QVERIFY(!c->noBorder());
    QCOMPARE(c->titlebarPosition(), AbstractClient::PositionTop);
    QVERIFY(!c->keepAbove());
    QVERIFY(!c->keepBelow());

    quint32 timestamp = 1;
    MOTION(QPoint(c->geometry().center().x(), c->clientPos().y() / 2));

    // TODO: mouse wheel direction looks wrong to me
    // simulate wheel
    waylandServer()->backend()->pointerAxisVertical(5.0, timestamp++);
    QVERIFY(c->keepBelow());
    QVERIFY(!c->keepAbove());
    waylandServer()->backend()->pointerAxisVertical(-5.0, timestamp++);
    QVERIFY(!c->keepBelow());
    QVERIFY(!c->keepAbove());
    waylandServer()->backend()->pointerAxisVertical(-5.0, timestamp++);
    QVERIFY(!c->keepBelow());
    QVERIFY(c->keepAbove());
}

void KWin::DecorationInputTest::testDoubleClick()
{
    AbstractClient *c = showWindow();
    QVERIFY(c);
    QVERIFY(c->isDecorated());
    QVERIFY(!c->noBorder());
    QVERIFY(!c->isOnAllDesktops());
    quint32 timestamp = 1;
    MOTION(QPoint(c->geometry().center().x(), c->clientPos().y() / 2));

    // double click
    PRESS;
    RELEASE;
    PRESS;
    RELEASE;
    QVERIFY(c->isOnAllDesktops());
    // double click again
    PRESS;
    RELEASE;
    QEXPECT_FAIL("", "Tripple click triggers another double click", Continue);
    QVERIFY(c->isOnAllDesktops());
    PRESS;
    RELEASE;
    QEXPECT_FAIL("", "Tripple click triggers another double click", Continue);
    QVERIFY(!c->isOnAllDesktops());
}

void DecorationInputTest::testHover()
{
    AbstractClient *c = showWindow();
    QVERIFY(c);
    QVERIFY(c->isDecorated());
    QVERIFY(!c->noBorder());

    // our left border is moved out of the visible area, so move the window to a better place
    c->move(QPoint(20, 0));

    quint32 timestamp = 1;
    MOTION(QPoint(c->geometry().center().x(), c->clientPos().y() / 2));
    QCOMPARE(c->cursor(), Qt::ArrowCursor);

    MOTION(QPoint(20, 0));
    QCOMPARE(c->cursor(), Qt::SizeFDiagCursor);
    MOTION(QPoint(c->geometry().x() + c->geometry().width() / 2, 0));
    QCOMPARE(c->cursor(), Qt::SizeVerCursor);
    MOTION(QPoint(c->geometry().x() + c->geometry().width() - 1, 0));
    QCOMPARE(c->cursor(), Qt::SizeBDiagCursor);
    MOTION(QPoint(c->geometry().x() + c->geometry().width() - 1, c->height() / 2));
    QCOMPARE(c->cursor(), Qt::SizeHorCursor);
    MOTION(QPoint(c->geometry().x() + c->geometry().width() - 1, c->height() - 1));
    QCOMPARE(c->cursor(), Qt::SizeFDiagCursor);
    MOTION(QPoint(c->geometry().x() + c->geometry().width() / 2, c->height() - 1));
    QCOMPARE(c->cursor(), Qt::SizeVerCursor);
    MOTION(QPoint(c->geometry().x(), c->height() - 1));
    QCOMPARE(c->cursor(), Qt::SizeBDiagCursor);
    MOTION(QPoint(c->geometry().x(), c->height() / 2));
    QCOMPARE(c->cursor(), Qt::SizeHorCursor);

    MOTION(c->geometry().center());
    QEXPECT_FAIL("", "Cursor not set back on leave", Continue);
    QCOMPARE(c->cursor(), Qt::ArrowCursor);
}

void DecorationInputTest::testPressToMove()
{
    AbstractClient *c = showWindow();
    QVERIFY(c);
    QVERIFY(c->isDecorated());
    QVERIFY(!c->noBorder());
    QSignalSpy startMoveResizedSpy(c, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(startMoveResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(c, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    quint32 timestamp = 1;
    MOTION(QPoint(c->geometry().center().x(), c->clientPos().y() / 2));
    QCOMPARE(c->cursor(), Qt::ArrowCursor);

    PRESS;
    QVERIFY(!c->isMove());
    MOTION(QPoint(c->geometry().center().x() + 10, c->clientPos().y() / 2));
    const int oldX = c->x();
    QTRY_VERIFY(c->isMove());
    QCOMPARE(startMoveResizedSpy.count(), 1);

    RELEASE;
    QTRY_VERIFY(!c->isMove());
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QEXPECT_FAIL("", "Just trigger move doesn't move the window", Continue);
    QCOMPARE(c->x(), oldX + 10);

    // again
    PRESS;
    QVERIFY(!c->isMove());
    MOTION(QPoint(c->geometry().center().x() + 20, c->clientPos().y() / 2));
    QTRY_VERIFY(c->isMove());
    QCOMPARE(startMoveResizedSpy.count(), 2);
    MOTION(QPoint(c->geometry().center().x() + 30, c->clientPos().y() / 2));

    RELEASE;
    QTRY_VERIFY(!c->isMove());
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 2);
    QCOMPARE(c->x(), oldX + 20);
}

}

WAYLANDTEST_MAIN(KWin::DecorationInputTest)
#include "decoration_input_test.moc"
