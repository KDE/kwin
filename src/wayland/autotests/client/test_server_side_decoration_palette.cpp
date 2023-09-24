/*
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QSignalSpy>
#include <QTest>
// KWin
#include "wayland/compositor.h"
#include "wayland/display.h"
#include "wayland/server_decoration_palette.h"

#include "qwayland-server-decoration-palette.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/surface.h"

class ServerSideDecorationPaletteManager : public QtWayland::org_kde_kwin_server_decoration_palette_manager
{
};

class ServerSideDecorationPalette : public QObject, public QtWayland::org_kde_kwin_server_decoration_palette
{
    Q_OBJECT

public:
    ~ServerSideDecorationPalette()
    {
        release();
    }
};

class TestServerSideDecorationPalette : public QObject
{
    Q_OBJECT
public:
    explicit TestServerSideDecorationPalette(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreateAndSet();

private:
    KWin::Display *m_display;
    KWin::CompositorInterface *m_compositorInterface;
    KWin::ServerSideDecorationPaletteManagerInterface *m_paletteManagerInterface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    ServerSideDecorationPaletteManager *m_paletteManager;
    KWayland::Client::EventQueue *m_queue;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-decopalette-0");

TestServerSideDecorationPalette::TestServerSideDecorationPalette(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestServerSideDecorationPalette::init()
{
    using namespace KWin;
    delete m_display;
    m_display = new KWin::Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &KWayland::Client::ConnectionThread::connected);
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

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    m_paletteManagerInterface = new ServerSideDecorationPaletteManagerInterface(m_display, m_display);

    KWayland::Client::Registry registry;

    connect(&registry, &KWayland::Client::Registry::interfaceAnnounced, this, [this, &registry](const QByteArray &interfaceName, quint32 name, quint32 version) {
        if (interfaceName == org_kde_kwin_server_decoration_palette_manager_interface.name) {
            m_paletteManager = new ServerSideDecorationPaletteManager();
            m_paletteManager->init(registry.registry(), name, version);
        }
    });

    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    QSignalSpy compositorSpy(&registry, &KWayland::Client::Registry::compositorAnnounced);

    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    QVERIFY(m_compositor);
    QVERIFY(m_paletteManager);
}

void TestServerSideDecorationPalette::cleanup()
{
#define CLEANUP(variable)   \
    if (variable) {         \
        delete variable;    \
        variable = nullptr; \
    }
    CLEANUP(m_compositor)
    CLEANUP(m_paletteManager)
    CLEANUP(m_queue)
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
    CLEANUP(m_compositorInterface)
    CLEANUP(m_paletteManagerInterface)
    CLEANUP(m_display)
#undef CLEANUP
}

void TestServerSideDecorationPalette::testCreateAndSet()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &KWin::CompositorInterface::surfaceCreated);

    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<KWin::SurfaceInterface *>();
    QSignalSpy paletteCreatedSpy(m_paletteManagerInterface, &KWin::ServerSideDecorationPaletteManagerInterface::paletteCreated);

    QVERIFY(!m_paletteManagerInterface->paletteForSurface(serverSurface));

    auto palette = std::make_unique<ServerSideDecorationPalette>();
    palette->init(m_paletteManager->create(*surface.get()));
    QVERIFY(paletteCreatedSpy.wait());
    auto paletteInterface = paletteCreatedSpy.first().first().value<KWin::ServerSideDecorationPaletteInterface *>();
    QCOMPARE(m_paletteManagerInterface->paletteForSurface(serverSurface), paletteInterface);

    QCOMPARE(paletteInterface->palette(), QString());

    QSignalSpy changedSpy(paletteInterface, &KWin::ServerSideDecorationPaletteInterface::paletteChanged);
    palette->set_palette(QStringLiteral("foobar"));
    QVERIFY(changedSpy.wait());
    QCOMPARE(paletteInterface->palette(), QString("foobar"));

    // and destroy
    QSignalSpy destroyedSpy(paletteInterface, &QObject::destroyed);
    palette.reset();
    QVERIFY(destroyedSpy.wait());
    QVERIFY(!m_paletteManagerInterface->paletteForSurface(serverSurface));
}

QTEST_GUILESS_MAIN(TestServerSideDecorationPalette)
#include "test_server_side_decoration_palette.moc"
