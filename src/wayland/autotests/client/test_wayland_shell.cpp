/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
// Qt
#include <QtTest/QtTest>
// KWin
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/shell.h"
#include "../../src/client/surface.h"
#include "../../src/client/registry.h"
#include "../../src/server/buffer_interface.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/shell_interface.h"
#include "../../src/server/surface_interface.h"
// Wayland
#include <wayland-client-protocol.h>

class TestWaylandShell : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandShell(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreateMultiple();
    void testFullscreen();
    void testMaximize();
    void testToplevel();
    void testPing();
    void testTitle();
    void testWindowClass();
    void testDestroy();
    void testCast();

private:
    KWayland::Server::Display *m_display;
    KWayland::Server::CompositorInterface *m_compositorInterface;
    KWayland::Server::ShellInterface *m_shellInterface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::Shell *m_shell;
    KWayland::Client::EventQueue *m_queue;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-shell-0");

TestWaylandShell::TestWaylandShell(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_shellInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_shell(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestWaylandShell::init()
{
    using namespace KWayland::Server;
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    m_compositorInterface = m_display->createCompositor(m_display);
    QVERIFY(m_compositorInterface);
    m_compositorInterface->create();
    QVERIFY(m_compositorInterface->isValid());

    m_shellInterface = m_display->createShell(m_display);
    QVERIFY(m_shellInterface);
    m_shellInterface->create();
    QVERIFY(m_shellInterface->isValid());

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, SIGNAL(connected()));
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

    KWayland::Client::Registry registry;
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    QSignalSpy shellSpy(&registry, SIGNAL(shellAnnounced(quint32,quint32)));
    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(compositorSpy.wait());

    m_compositor = new KWayland::Client::Compositor(this);
    m_compositor->setup(registry.bindCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>()));
    QVERIFY(m_compositor->isValid());

    if (shellSpy.isEmpty()) {
        QVERIFY(shellSpy.wait());
    }
    m_shell = registry.createShell(shellSpy.first().first().value<quint32>(), shellSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shell->isValid());
}

void TestWaylandShell::cleanup()
{
    if (m_shell) {
        delete m_shell;
        m_shell = nullptr;
    }
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
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

    delete m_shellInterface;
    m_shellInterface = nullptr;

    delete m_compositorInterface;
    m_compositorInterface = nullptr;

    delete m_display;
    m_display = nullptr;
}

void TestWaylandShell::testCreateMultiple()
{
    using namespace KWayland::Server;
    using namespace KWayland::Client;

    QScopedPointer<Surface> s1(m_compositor->createSurface());
    QScopedPointer<Surface> s2(m_compositor->createSurface());
    QVERIFY(!s1.isNull());
    QVERIFY(s1->isValid());
    QVERIFY(!s2.isNull());
    QVERIFY(s2->isValid());

    QSignalSpy serverSurfaceSpy(m_shellInterface, SIGNAL(surfaceCreated(KWayland::Server::ShellSurfaceInterface*)));
    QVERIFY(serverSurfaceSpy.isValid());
    QScopedPointer<ShellSurface> surface1(m_shell->createSurface(s1.data()));
    QVERIFY(!surface1.isNull());
    QVERIFY(surface1->isValid());

    QVERIFY(serverSurfaceSpy.wait());
    QCOMPARE(serverSurfaceSpy.count(), 1);

    QScopedPointer<ShellSurface> surface2(m_shell->createSurface(s2.data()));
    QVERIFY(!surface2.isNull());
    QVERIFY(surface2->isValid());

    QVERIFY(serverSurfaceSpy.wait());
    QCOMPARE(serverSurfaceSpy.count(), 2);

    // try creating for one which already exist should not be possible
    QScopedPointer<ShellSurface> surface3(m_shell->createSurface(s2.data()));
    QVERIFY(!surface3.isNull());
    QVERIFY(surface3->isValid());

    QVERIFY(!serverSurfaceSpy.wait(100));
    QCOMPARE(serverSurfaceSpy.count(), 2);
}

void TestWaylandShell::testFullscreen()
{
    using namespace KWayland::Server;
    QScopedPointer<KWayland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(!s.isNull());
    QVERIFY(s->isValid());
    KWayland::Client::ShellSurface *surface = m_shell->createSurface(s.data(), m_shell);
    QSignalSpy sizeSpy(surface, SIGNAL(sizeChanged(QSize)));
    QVERIFY(sizeSpy.isValid());
    QCOMPARE(surface->size(), QSize());

    QSignalSpy serverSurfaceSpy(m_shellInterface, SIGNAL(surfaceCreated(KWayland::Server::ShellSurfaceInterface*)));
    QVERIFY(serverSurfaceSpy.isValid());
    QVERIFY(serverSurfaceSpy.wait());
    ShellSurfaceInterface *serverSurface = serverSurfaceSpy.first().first().value<ShellSurfaceInterface*>();
    QVERIFY(serverSurface);
    QVERIFY(serverSurface->parentResource());

    QSignalSpy fullscreenSpy(serverSurface, SIGNAL(fullscreenChanged(bool)));
    QVERIFY(fullscreenSpy.isValid());

    surface->setFullscreen();
    QVERIFY(fullscreenSpy.wait());
    QCOMPARE(fullscreenSpy.count(), 1);
    QVERIFY(fullscreenSpy.first().first().toBool());
    serverSurface->requestSize(QSize(1024, 768));

    QVERIFY(sizeSpy.wait());
    QCOMPARE(sizeSpy.count(), 1);
    QCOMPARE(sizeSpy.first().first().toSize(), QSize(1024, 768));
    QCOMPARE(surface->size(), QSize(1024, 768));

    // set back to toplevel
    fullscreenSpy.clear();
    wl_shell_surface_set_toplevel(*surface);
    QVERIFY(fullscreenSpy.wait());
    QCOMPARE(fullscreenSpy.count(), 1);
    QVERIFY(!fullscreenSpy.first().first().toBool());
}

void TestWaylandShell::testMaximize()
{
    using namespace KWayland::Server;
    QScopedPointer<KWayland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(!s.isNull());
    QVERIFY(s->isValid());
    KWayland::Client::ShellSurface *surface = m_shell->createSurface(s.data(), m_shell);
    QSignalSpy sizeSpy(surface, SIGNAL(sizeChanged(QSize)));
    QVERIFY(sizeSpy.isValid());
    QCOMPARE(surface->size(), QSize());

    QSignalSpy serverSurfaceSpy(m_shellInterface, SIGNAL(surfaceCreated(KWayland::Server::ShellSurfaceInterface*)));
    QVERIFY(serverSurfaceSpy.isValid());
    QVERIFY(serverSurfaceSpy.wait());
    ShellSurfaceInterface *serverSurface = serverSurfaceSpy.first().first().value<ShellSurfaceInterface*>();
    QVERIFY(serverSurface);
    QVERIFY(serverSurface->parentResource());

    QSignalSpy maximizedSpy(serverSurface, SIGNAL(maximizedChanged(bool)));
    QVERIFY(maximizedSpy.isValid());

    surface->setMaximized();
    QVERIFY(maximizedSpy.wait());
    QCOMPARE(maximizedSpy.count(), 1);
    QVERIFY(maximizedSpy.first().first().toBool());
    serverSurface->requestSize(QSize(1024, 768));

    QVERIFY(sizeSpy.wait());
    QCOMPARE(sizeSpy.count(), 1);
    QCOMPARE(sizeSpy.first().first().toSize(), QSize(1024, 768));
    QCOMPARE(surface->size(), QSize(1024, 768));

    // set back to toplevel
    maximizedSpy.clear();
    wl_shell_surface_set_toplevel(*surface);
    QVERIFY(maximizedSpy.wait());
    QCOMPARE(maximizedSpy.count(), 1);
    QVERIFY(!maximizedSpy.first().first().toBool());
}

void TestWaylandShell::testToplevel()
{
    using namespace KWayland::Server;
    QScopedPointer<KWayland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(!s.isNull());
    QVERIFY(s->isValid());
    KWayland::Client::ShellSurface *surface = m_shell->createSurface(s.data(), m_shell);
    QSignalSpy sizeSpy(surface, SIGNAL(sizeChanged(QSize)));
    QVERIFY(sizeSpy.isValid());
    QCOMPARE(surface->size(), QSize());

    QSignalSpy serverSurfaceSpy(m_shellInterface, SIGNAL(surfaceCreated(KWayland::Server::ShellSurfaceInterface*)));
    QVERIFY(serverSurfaceSpy.isValid());
    QVERIFY(serverSurfaceSpy.wait());
    ShellSurfaceInterface *serverSurface = serverSurfaceSpy.first().first().value<ShellSurfaceInterface*>();
    QVERIFY(serverSurface);
    QVERIFY(serverSurface->parentResource());

    QSignalSpy toplevelSpy(serverSurface, SIGNAL(toplevelChanged(bool)));
    QVERIFY(toplevelSpy.isValid());

    surface->setFullscreen();
    QVERIFY(toplevelSpy.wait());
    QCOMPARE(toplevelSpy.count(), 1);
    QVERIFY(!toplevelSpy.first().first().toBool());
    toplevelSpy.clear();
    surface->setToplevel();
    QVERIFY(toplevelSpy.wait());
    QCOMPARE(toplevelSpy.count(), 1);
    QVERIFY(toplevelSpy.first().first().toBool());
    serverSurface->requestSize(QSize(1024, 768));

    QVERIFY(sizeSpy.wait());
    QCOMPARE(sizeSpy.count(), 1);
    QCOMPARE(sizeSpy.first().first().toSize(), QSize(1024, 768));
    QCOMPARE(surface->size(), QSize(1024, 768));

    // set back to fullscreen
    toplevelSpy.clear();
    surface->setFullscreen();
    QVERIFY(toplevelSpy.wait());
    QCOMPARE(toplevelSpy.count(), 1);
    QVERIFY(!toplevelSpy.first().first().toBool());
}

void TestWaylandShell::testPing()
{
    using namespace KWayland::Server;
    QScopedPointer<KWayland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(!s.isNull());
    QVERIFY(s->isValid());
    KWayland::Client::ShellSurface *surface = m_shell->createSurface(s.data(), m_shell);
    QSignalSpy pingSpy(surface, SIGNAL(pinged()));

    QSignalSpy serverSurfaceSpy(m_shellInterface, SIGNAL(surfaceCreated(KWayland::Server::ShellSurfaceInterface*)));
    QVERIFY(serverSurfaceSpy.isValid());
    QVERIFY(serverSurfaceSpy.wait());
    ShellSurfaceInterface *serverSurface = serverSurfaceSpy.first().first().value<ShellSurfaceInterface*>();
    QVERIFY(serverSurface);

    QSignalSpy pingTimeoutSpy(serverSurface, SIGNAL(pingTimeout()));
    QVERIFY(pingTimeoutSpy.isValid());
    QSignalSpy pongSpy(serverSurface, SIGNAL(pongReceived()));
    QVERIFY(pongSpy.isValid());

    serverSurface->ping();
    QVERIFY(pingSpy.wait());
    wl_display_flush(m_connection->display());

    if (pongSpy.isEmpty()) {
        QVERIFY(pongSpy.wait());
    }
    QVERIFY(!pongSpy.isEmpty());
    QVERIFY(pingTimeoutSpy.isEmpty());

    // evil trick - timeout of zero will make it not get the pong
    serverSurface->setPingTimeout(0);
    pongSpy.clear();
    pingTimeoutSpy.clear();
    serverSurface->ping();
    QTest::qWait(100);
    if (pingTimeoutSpy.isEmpty()) {
        QVERIFY(pingTimeoutSpy.wait());
    }
    QCOMPARE(pingTimeoutSpy.count(), 1);
    QVERIFY(pongSpy.isEmpty());
}

void TestWaylandShell::testTitle()
{
    using namespace KWayland::Server;
    QScopedPointer<KWayland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(!s.isNull());
    QVERIFY(s->isValid());
    KWayland::Client::ShellSurface *surface = m_shell->createSurface(s.data(), m_shell);

    QSignalSpy serverSurfaceSpy(m_shellInterface, SIGNAL(surfaceCreated(KWayland::Server::ShellSurfaceInterface*)));
    QVERIFY(serverSurfaceSpy.isValid());
    QVERIFY(serverSurfaceSpy.wait());
    ShellSurfaceInterface *serverSurface = serverSurfaceSpy.first().first().value<ShellSurfaceInterface*>();
    QVERIFY(serverSurface);

    QSignalSpy titleSpy(serverSurface, SIGNAL(titleChanged(QString)));
    QVERIFY(titleSpy.isValid());
    QString testTitle = QStringLiteral("fooBar");
    QVERIFY(serverSurface->title().isNull());

    wl_shell_surface_set_title(*(const KWayland::Client::ShellSurface *)surface, testTitle.toUtf8().constData());
    QVERIFY(titleSpy.wait());
    QCOMPARE(serverSurface->title(), testTitle);
    QCOMPARE(titleSpy.first().first().toString(), testTitle);
}

void TestWaylandShell::testWindowClass()
{
    using namespace KWayland::Server;
    QScopedPointer<KWayland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(!s.isNull());
    QVERIFY(s->isValid());
    KWayland::Client::ShellSurface *surface = m_shell->createSurface(s.data(), m_shell);

    QSignalSpy serverSurfaceSpy(m_shellInterface, SIGNAL(surfaceCreated(KWayland::Server::ShellSurfaceInterface*)));
    QVERIFY(serverSurfaceSpy.isValid());
    QVERIFY(serverSurfaceSpy.wait());
    ShellSurfaceInterface *serverSurface = serverSurfaceSpy.first().first().value<ShellSurfaceInterface*>();
    QVERIFY(serverSurface);

    QSignalSpy windowClassSpy(serverSurface, SIGNAL(windowClassChanged(QByteArray)));
    QVERIFY(windowClassSpy.isValid());
    QByteArray testClass = QByteArrayLiteral("fooBar");
    QVERIFY(serverSurface->windowClass().isNull());

    wl_shell_surface_set_class(*surface, testClass.constData());
    QVERIFY(windowClassSpy.wait());
    QCOMPARE(serverSurface->windowClass(), testClass);
    QCOMPARE(windowClassSpy.first().first().toByteArray(), testClass);
}

void TestWaylandShell::testDestroy()
{
    using namespace KWayland::Client;
    QScopedPointer<Surface> s(m_compositor->createSurface());
    QVERIFY(!s.isNull());
    QVERIFY(s->isValid());
    ShellSurface *surface = m_shell->createSurface(s.data(), m_shell);
    QVERIFY(surface->isValid());

    connect(m_connection, &ConnectionThread::connectionDied, m_shell, &Shell::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, m_compositor, &Compositor::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, s.data(), &Surface::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, m_queue, &EventQueue::destroy);

    QSignalSpy connectionDiedSpy(m_connection, SIGNAL(connectionDied()));
    QVERIFY(connectionDiedSpy.isValid());
    delete m_display;
    m_display = nullptr;
    m_compositorInterface = nullptr;
    m_shellInterface = nullptr;
    QVERIFY(connectionDiedSpy.wait());

    QVERIFY(!m_shell->isValid());
    QVERIFY(!surface->isValid());

    m_shell->destroy();
    surface->destroy();
}

void TestWaylandShell::testCast()
{
    using namespace KWayland::Client;
    Registry registry;
    QSignalSpy shellSpy(&registry, SIGNAL(shellAnnounced(quint32,quint32)));
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(shellSpy.wait());

    Shell s;
    auto wlShell = registry.bindShell(shellSpy.first().first().value<quint32>(), shellSpy.first().last().value<quint32>());
    m_queue->addProxy(wlShell);
    QVERIFY(wlShell);
    QVERIFY(!s.isValid());
    s.setup(wlShell);
    QVERIFY(s.isValid());
    QCOMPARE((wl_shell*)s, wlShell);

    const Shell &s2(s);
    QCOMPARE((wl_shell*)s2, wlShell);
}

QTEST_GUILESS_MAIN(TestWaylandShell)
#include "test_wayland_shell.moc"
