/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2017 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "KWayland/Client/xdgforeign.h"
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/server_decoration.h"
#include "KWayland/Client/shell.h"
#include "KWayland/Client/shm_pool.h"
#include "KWayland/Client/xdgshell.h"
// Qt
#include <QCommandLineParser>
#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QThread>
using namespace KWayland::Client;

class XdgForeignTest : public QObject
{
    Q_OBJECT
public:
    explicit XdgForeignTest(QObject *parent = nullptr);
    virtual ~XdgForeignTest();

    void init();

private:
    void setupRegistry(Registry *registry);
    void render();
    QThread *m_connectionThread;
    ConnectionThread *m_connectionThreadObject;
    EventQueue *m_eventQueue = nullptr;
    Compositor *m_compositor = nullptr;
    XdgShell *m_shell = nullptr;
    XdgShellSurface *m_shellSurface = nullptr;
    ShmPool *m_shm = nullptr;
    Surface *m_surface = nullptr;

    XdgShellSurface *m_childShellSurface = nullptr;
    Surface *m_childSurface = nullptr;

    KWayland::Client::XdgExporter *m_exporter = nullptr;
    KWayland::Client::XdgImporter *m_importer = nullptr;
    KWayland::Client::XdgExported *m_exported = nullptr;
    KWayland::Client::XdgImported *m_imported = nullptr;
    KWayland::Client::ServerSideDecorationManager *m_decoration = nullptr;
};

XdgForeignTest::XdgForeignTest(QObject *parent)
    : QObject(parent)
    , m_connectionThread(new QThread(this))
    , m_connectionThreadObject(new ConnectionThread())
{
}

XdgForeignTest::~XdgForeignTest()
{
    m_connectionThread->quit();
    m_connectionThread->wait();
    m_connectionThreadObject->deleteLater();
}

void XdgForeignTest::init()
{
    connect(
        m_connectionThreadObject,
        &ConnectionThread::connected,
        this,
        [this] {
            m_eventQueue = new EventQueue(this);
            m_eventQueue->setup(m_connectionThreadObject);

            Registry *registry = new Registry(this);
            setupRegistry(registry);
        },
        Qt::QueuedConnection);
    m_connectionThreadObject->moveToThread(m_connectionThread);
    m_connectionThread->start();

    m_connectionThreadObject->initConnection();
}

void XdgForeignTest::setupRegistry(Registry *registry)
{
    connect(registry, &Registry::compositorAnnounced, this, [this, registry](quint32 name, quint32 version) {
        m_compositor = registry->createCompositor(name, version, this);
    });
    connect(registry, &Registry::xdgShellUnstableV5Announced, this, [this, registry](quint32 name, quint32 version) {
        m_shell = registry->createXdgShell(name, version, this);
    });
    connect(registry, &Registry::shmAnnounced, this, [this, registry](quint32 name, quint32 version) {
        m_shm = registry->createShmPool(name, version, this);
    });
    connect(registry, &Registry::exporterUnstableV2Announced, this, [this, registry](quint32 name, quint32 version) {
        m_exporter = registry->createXdgExporter(name, version, this);
        m_exporter->setEventQueue(m_eventQueue);
    });
    connect(registry, &Registry::importerUnstableV2Announced, this, [this, registry](quint32 name, quint32 version) {
        m_importer = registry->createXdgImporter(name, version, this);
        m_importer->setEventQueue(m_eventQueue);
    });
    connect(registry, &Registry::serverSideDecorationManagerAnnounced, this, [this, registry](quint32 name, quint32 version) {
        m_decoration = registry->createServerSideDecorationManager(name, version, this);
        m_decoration->setEventQueue(m_eventQueue);
    });
    connect(registry, &Registry::interfacesAnnounced, this, [this] {
        Q_ASSERT(m_compositor);
        Q_ASSERT(m_shell);
        Q_ASSERT(m_shm);
        Q_ASSERT(m_exporter);
        Q_ASSERT(m_importer);
        m_surface = m_compositor->createSurface(this);
        Q_ASSERT(m_surface);
        auto parentDeco = m_decoration->create(m_surface, this);
        Q_ASSERT(parentDeco);
        m_shellSurface = m_shell->createSurface(m_surface, this);
        Q_ASSERT(m_shellSurface);
        connect(m_shellSurface, &XdgShellSurface::sizeChanged, this, &XdgForeignTest::render);

        m_childSurface = m_compositor->createSurface(this);
        Q_ASSERT(m_childSurface);
        auto childDeco = m_decoration->create(m_childSurface, this);
        Q_ASSERT(childDeco);
        m_childShellSurface = m_shell->createSurface(m_childSurface, this);
        Q_ASSERT(m_childShellSurface);
        connect(m_childShellSurface, &XdgShellSurface::sizeChanged, this, &XdgForeignTest::render);

        m_exported = m_exporter->exportTopLevel(m_surface, this);
        Q_ASSERT(m_decoration);
        connect(m_exported, &XdgExported::done, this, [this]() {
            m_imported = m_importer->importTopLevel(m_exported->handle(), this);
            m_imported->setParentOf(m_childSurface);
        });
        render();
    });
    registry->setEventQueue(m_eventQueue);
    registry->create(m_connectionThreadObject);
    registry->setup();
}

void XdgForeignTest::render()
{
    QSize size = m_shellSurface->size().isValid() ? m_shellSurface->size() : QSize(500, 500);
    auto buffer = m_shm->getBuffer(size, size.width() * 4).toStrongRef();
    buffer->setUsed(true);
    QImage image(buffer->address(), size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 255, 255, 255));

    m_surface->attachBuffer(*buffer);
    m_surface->damage(QRect(QPoint(0, 0), size));
    m_surface->commit(Surface::CommitFlag::None);
    buffer->setUsed(false);

    size = m_childShellSurface->size().isValid() ? m_childShellSurface->size() : QSize(200, 200);
    buffer = m_shm->getBuffer(size, size.width() * 4).toStrongRef();
    buffer->setUsed(true);
    image = QImage(buffer->address(), size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));

    m_childSurface->attachBuffer(*buffer);
    m_childSurface->damage(QRect(QPoint(0, 0), size));
    m_childSurface->commit(Surface::CommitFlag::None);
    buffer->setUsed(false);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    XdgForeignTest client;

    client.init();

    return app.exec();
}

#include "xdgforeigntest.moc"
