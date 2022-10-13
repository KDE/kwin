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
#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/screencast_v1_interface.h"
#include "wayland/seat_interface.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/seat.h>

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

    void zkde_screencast_stream_unstable_v1_created(uint32_t node) override
    {
        Q_EMIT created(node);
    }

Q_SIGNALS:
    void created(quint32 node);
};

class ScreencastV1 : public QObject, public QtWayland::zkde_screencast_unstable_v1
{
    Q_OBJECT

public:
    ScreencastStreamV1 *createWindowStream(const QString &uuid)
    {
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
    std::unique_ptr<KWayland::Client::ConnectionThread> m_connection;
    std::unique_ptr<KWayland::Client::EventQueue> m_queue;
    std::unique_ptr<ScreencastV1> m_screencast;

    std::unique_ptr<KWaylandServer::ScreencastV1Interface> m_screencastInterface;

    QPointer<KWaylandServer::ScreencastStreamV1Interface> m_triggered = nullptr;
    std::unique_ptr<QThread> m_thread;
    std::unique_ptr<KWaylandServer::Display> m_display;
};

static const QString s_socketName = QStringLiteral("kwin-wayland-server-screencast-test-0");

void TestScreencastV1Interface::initTestCase()
{
    m_display = std::make_unique<KWaylandServer::Display>();
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    // setup connection
    m_connection = std::make_unique<KWayland::Client::ConnectionThread>();
    QSignalSpy connectedSpy(m_connection.get(), &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = std::make_unique<QThread>();
    m_connection->moveToThread(m_thread.get());
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = std::make_unique<KWayland::Client::EventQueue>();
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection.get());
    QVERIFY(m_queue->isValid());

    KWayland::Client::Registry registry;

    QSignalSpy screencastSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    m_screencastInterface = std::make_unique<KWaylandServer::ScreencastV1Interface>(m_display.get());
    connect(m_screencastInterface.get(),
            &KWaylandServer::ScreencastV1Interface::windowScreencastRequested,
            this,
            [this](KWaylandServer::ScreencastStreamV1Interface *stream, const QString &winid) {
                Q_UNUSED(winid);
                stream->sendCreated(123);
                m_triggered = stream;
            });

    connect(&registry,
            &KWayland::Client::Registry::interfaceAnnounced,
            this,
            [this, &registry](const QByteArray &interfaceName, quint32 name, quint32 version) {
                if (interfaceName != "zkde_screencast_unstable_v1")
                    return;
                m_screencast = std::make_unique<ScreencastV1>();
                m_screencast->init(&*registry, name, version);
            });
    registry.setEventQueue(m_queue.get());
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
    m_queue.reset();

    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        m_thread.reset();
    }
    m_connection.reset();
    m_display.reset();
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
