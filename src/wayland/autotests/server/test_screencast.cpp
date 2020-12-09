/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

// Qt
#include <QHash>
#include <QThread>
#include <QtTest>

#include <wayland-client.h>

// WaylandServer
#include "../../src/server/compositor_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/seat_interface.h"
#include "../../src/server/screencast_v1_interface.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/compositor.h>

#include "qwayland-zkde-screencast-unstable-v1.h"

class ScreencastStreamV1 : public QObject, public QtWayland::zkde_screencast_stream_unstable_v1
{
    Q_OBJECT

public:
    ScreencastStreamV1(::zkde_screencast_stream_unstable_v1 *obj, QObject *parent)
        : QObject(parent)
        , zkde_screencast_stream_unstable_v1(obj)
    {

    }

    void zkde_screencast_stream_unstable_v1_created(uint32_t node) override {
        Q_EMIT created(node);
    }

Q_SIGNALS:
    void created(quint32 node);
};

class ScreencastV1 : public QObject, public QtWayland::zkde_screencast_unstable_v1
{
    Q_OBJECT

public:
    ScreencastV1(QObject *parent)
        : QObject(parent)
    {
    }

    ScreencastStreamV1 *createWindowStream(const QString &uuid) {
        return new ScreencastStreamV1(stream_window(uuid, 2), this);
    }
};

class TestScreencastV1Interface : public QObject
{
    Q_OBJECT

public:
    TestScreencastV1Interface()
    {
    }

    ~TestScreencastV1Interface() override;

private Q_SLOTS:
    void initTestCase();
    void testCreate();

private:
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue = nullptr;
    ScreencastV1 *m_screencast = nullptr;

    KWaylandServer::ScreencastV1Interface *m_screencastInterface = nullptr;

    QPointer<KWaylandServer::ScreencastStreamV1Interface> m_triggered = nullptr;
    QThread *m_thread;
    KWaylandServer::Display *m_display = nullptr;
};

static const QString s_socketName = QStringLiteral("kwin-wayland-server-screencast-test-0");

void TestScreencastV1Interface::initTestCase()
{
    delete m_display;
    m_display = new KWaylandServer::Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &KWayland::Client::ConnectionThread::connected);
    QVERIFY(connectedSpy.isValid());
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

    QSignalSpy screencastSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    QVERIFY(screencastSpy.isValid());
    m_screencastInterface = new KWaylandServer::ScreencastV1Interface(m_display, this);
    connect(m_screencastInterface, &KWaylandServer::ScreencastV1Interface::windowScreencastRequested, this, [this] (KWaylandServer::ScreencastStreamV1Interface *stream, const QString &winid) {
        Q_UNUSED(winid);
        stream->sendCreated(123);
        m_triggered = stream;
    });

    connect(&registry, &KWayland::Client::Registry::interfaceAnnounced, this, [this, &registry] (const QByteArray &interfaceName, quint32 name, quint32 version) {
        if (interfaceName != "zkde_screencast_unstable_v1")
            return;
        Q_ASSERT(version == 1);
        m_screencast = new ScreencastV1(this);
        m_screencast->init(&*registry, name, version);
    });
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());

    QVERIFY(m_screencastInterface);
    QVERIFY(m_screencast || screencastSpy.wait());
    QVERIFY(m_screencast);
}

TestScreencastV1Interface::~TestScreencastV1Interface()
{
    delete m_queue;
    m_queue = nullptr;

    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    m_connection->deleteLater();
    m_connection = nullptr;

    delete m_display;
}

void TestScreencastV1Interface::testCreate()
{
    auto stream = m_screencast->createWindowStream("3");
    QVERIFY(stream);

    QSignalSpy spyWorking(stream, &ScreencastStreamV1::created);
    QVERIFY(spyWorking.count() || spyWorking.wait());
    QVERIFY(m_triggered);

    QSignalSpy spyStop(m_triggered, &KWaylandServer::ScreencastStreamV1Interface::finished);
    stream->close();
    QVERIFY(spyStop.count() || spyStop.wait());
}

QTEST_GUILESS_MAIN(TestScreencastV1Interface)

#include "test_screencast.moc"
