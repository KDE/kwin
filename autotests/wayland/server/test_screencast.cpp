/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

// Qt
#include <QHash>
#include <QSignalSpy>
#include <QTest>
#include <QThread>

#include <wayland-client.h>

// WaylandServer
#include "wayland/compositor.h"
#include "wayland/display.h"
#include "wayland/screencast_v2.h"
#include "wayland/seat.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/seat.h>

#include "qwayland-kde-screencast-v2.h"

class ScreencastStreamV2 : public QObject, public QtWayland::kde_screencast_stream_v2
{
    Q_OBJECT

public:
    ScreencastStreamV2(::kde_screencast_stream_v2 *obj, QObject *parent)
        : QObject(parent)
        , kde_screencast_stream_v2(obj)
    {
    }
    ~ScreencastStreamV2()
    {
    }

    void kde_screencast_stream_v2_created(uint32_t node, uint32_t object_serial_hi, uint32_t object_serial_lo) override
    {
        Q_EMIT created(node, quint64(object_serial_hi) << 32 | object_serial_lo);
    }

Q_SIGNALS:
    void created(quint32 node, quint64 object_serial);
};

class ScreencastManagerV2 : public QObject, public QtWayland::kde_screencast_manager_v2
{
    Q_OBJECT

public:
    ScreencastManagerV2(QObject *parent)
        : QObject(parent)
    {
    }

    ScreencastStreamV2 *createWindowStream(const QString &uuid)
    {
        auto params = stream_window(uuid.toUtf8().constData());
        return new ScreencastStreamV2(kde_screencast_window_params_v2_create_stream(params), this);
    }
};

class TestScreencastV2Interface : public QObject
{
    Q_OBJECT

public:
    TestScreencastV2Interface()
    {
    }

    ~TestScreencastV2Interface() override;

private Q_SLOTS:
    void initTestCase();
    void testCreate();

private:
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue = nullptr;
    ScreencastManagerV2 *m_screencast = nullptr;

    KWin::ScreencastManagerV2Interface *m_screencastInterface = nullptr;

    QPointer<KWin::ScreencastStreamV2Interface> m_triggered = nullptr;
    QThread *m_thread;
    KWin::Display *m_display = nullptr;
};

void TestScreencastV2Interface::initTestCase()
{
    delete m_display;
    m_display = new KWin::Display(this);
    m_display->addSocketName(qAppName());
    m_display->start();
    QVERIFY(m_display->isRunning());

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(qAppName());

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
    m_screencastInterface = new KWin::ScreencastManagerV2Interface(m_display, this);
    connect(m_screencastInterface, &KWin::ScreencastManagerV2Interface::windowScreencastRequested, this, [this](KWin::ScreencastStreamV2Interface *stream) {
        stream->sendCreated(123, 456);
        m_triggered = stream;
    });

    connect(&registry, &KWayland::Client::Registry::interfaceAnnounced, this, [this, &registry](const QByteArray &interfaceName, quint32 name, quint32 version) {
        if (interfaceName != ScreencastManagerV2::interface()->name) {
            return;
        }
        m_screencast = new ScreencastManagerV2(this);
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

TestScreencastV2Interface::~TestScreencastV2Interface()
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

void TestScreencastV2Interface::testCreate()
{
    auto stream = m_screencast->createWindowStream("3");
    QVERIFY(stream);

    QSignalSpy spyWorking(stream, &ScreencastStreamV2::created);
    QVERIFY(spyWorking.count() || spyWorking.wait());
    QVERIFY(m_triggered);

    QSignalSpy spyStop(m_triggered, &KWin::ScreencastStreamV2Interface::finished);
    stream->destroy();
    QVERIFY(spyStop.count() || spyStop.wait());
}

QTEST_GUILESS_MAIN(TestScreencastV2Interface)

#include "test_screencast.moc"
