/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/subcompositor.h"
#include "../../src/server/display.h"
#include "../../src/server/subcompositor_interface.h"

class TestSubCompositor : public QObject
{
    Q_OBJECT
public:
    explicit TestSubCompositor(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testDestroy();
    void testCast();

private:
    KWaylandServer::Display *m_display;
    KWaylandServer::SubCompositorInterface *m_subcompositorInterface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::SubCompositor *m_subCompositor;
    KWayland::Client::EventQueue *m_queue;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-subcompositor-0");

TestSubCompositor::TestSubCompositor(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_subcompositorInterface(nullptr)
    , m_connection(nullptr)
    , m_subCompositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestSubCompositor::init()
{
    using namespace KWaylandServer;
    delete m_display;
    m_display = new Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

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
    QSignalSpy subCompositorSpy(&registry, SIGNAL(subCompositorAnnounced(quint32,quint32)));
    QVERIFY(subCompositorSpy.isValid());
    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_subcompositorInterface = new SubCompositorInterface(m_display, m_display);
    QVERIFY(m_subcompositorInterface);

    QVERIFY(subCompositorSpy.wait());
    m_subCompositor = registry.createSubCompositor(subCompositorSpy.first().first().value<quint32>(), subCompositorSpy.first().last().value<quint32>(), this);
}

void TestSubCompositor::cleanup()
{
    if (m_subCompositor) {
        delete m_subCompositor;
        m_subCompositor = nullptr;
    }
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    delete m_connection;
    m_connection = nullptr;

    delete m_display;
    m_display = nullptr;
}

void TestSubCompositor::testDestroy()
{
    using namespace KWayland::Client;
    connect(m_connection, &ConnectionThread::connectionDied, m_subCompositor, &SubCompositor::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, m_queue, &EventQueue::destroy);
    QVERIFY(m_subCompositor->isValid());

    QSignalSpy connectionDiedSpy(m_connection, SIGNAL(connectionDied()));
    QVERIFY(connectionDiedSpy.isValid());
    delete m_display;
    m_display = nullptr;
    QVERIFY(connectionDiedSpy.wait());

    // now the pool should be destroyed;
    QVERIFY(!m_subCompositor->isValid());

    // calling destroy again should not fail
    m_subCompositor->destroy();
}

void TestSubCompositor::testCast()
{
    using namespace KWayland::Client;
    Registry registry;
    QSignalSpy subCompositorSpy(&registry, SIGNAL(subCompositorAnnounced(quint32,quint32)));
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    QVERIFY(subCompositorSpy.wait());

    SubCompositor c;
    auto wlSubComp = registry.bindSubCompositor(subCompositorSpy.first().first().value<quint32>(), subCompositorSpy.first().last().value<quint32>());
    c.setup(wlSubComp);
    QCOMPARE((wl_subcompositor*)c, wlSubComp);

    const SubCompositor &c2(c);
    QCOMPARE((wl_subcompositor*)c2, wlSubComp);
}

QTEST_GUILESS_MAIN(TestSubCompositor)
#include "test_wayland_subcompositor.moc"
