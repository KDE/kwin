/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "qwayland-xdg-decoration-unstable-v1.h"
#include "qwayland-xdg-session-management-v1.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/shm_pool.h"
#include "KWayland/Client/surface.h"
#include "KWayland/Client/xdgshell.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>

using namespace KWayland::Client;

class XdgDecorationManagerV1 : public QtWayland::zxdg_decoration_manager_v1
{
public:
    ~XdgDecorationManagerV1() override
    {
        destroy();
    }
};

class XdgToplevelDecorationV1 : public QtWayland::zxdg_toplevel_decoration_v1
{
public:
    ~XdgToplevelDecorationV1() override
    {
        destroy();
    }
};

class XdgSessionManagerV1 : public QtWayland::xdg_session_manager_v1
{
public:
    ~XdgSessionManagerV1() override
    {
        destroy();
    }
};

class XdgSessionV1 : public QtWayland::xdg_session_v1
{
public:
    ~XdgSessionV1() override
    {
        destroy();
    }

protected:
    void xdg_session_v1_created(const QString &id) override
    {
        qDebug() << "xdg_session_v1 created" << id;
    }

    void xdg_session_v1_restored() override
    {
        qDebug() << "xdg_session_v1 restored";
    }
};

class XdgToplevelSessionV1 : public QtWayland::xdg_toplevel_session_v1
{
public:
    ~XdgToplevelSessionV1() override
    {
        destroy();
    }

protected:
    void xdg_toplevel_session_v1_restored() override
    {
        qDebug() << "xdg_toplevel_session_v1 restored";
    }
};

class XdgSessionDemo : public QObject
{
    Q_OBJECT

public:
    explicit XdgSessionDemo(QObject *parent = nullptr);

    void start();

    void setSessionId(const QString &sessionId);
    void setToplevelId(const QString &toplevelId);

private:
    void handleConnectionEstablished();
    void handleConnectionTerminated();
    void handleInterfaceAnnounced(const QByteArray &interface, quint32 name, quint32 version);
    void handleInterfacesAnnounced();
    void handleConfigureRequested(const QSize &size, XdgShellSurface::States states, int serial);

private:
    void render(const QSize &size);

    ConnectionThread *m_connection;
    EventQueue *m_eventQueue = nullptr;
    Registry *m_registry = nullptr;
    Compositor *m_compositor = nullptr;
    ShmPool *m_shm = nullptr;
    Surface *m_surface = nullptr;
    XdgShell *m_shell = nullptr;
    XdgShellSurface *m_shellSurface = nullptr;
    std::unique_ptr<XdgSessionManagerV1> m_sessionManager;
    std::unique_ptr<XdgSessionV1> m_session;
    std::unique_ptr<XdgToplevelSessionV1> m_toplevelSession;
    std::unique_ptr<XdgDecorationManagerV1> m_decorationManager;
    std::unique_ptr<XdgToplevelDecorationV1> m_toplevelDecoration;

    QString m_sessionId;
    QString m_toplevelId;
};

XdgSessionDemo::XdgSessionDemo(QObject *parent)
    : QObject(parent)
    , m_connection(new ConnectionThread(this))
{
}

void XdgSessionDemo::start()
{
    connect(m_connection, &ConnectionThread::connected, this, &XdgSessionDemo::handleConnectionEstablished);
    connect(m_connection, &ConnectionThread::connectionDied, this, &XdgSessionDemo::handleConnectionTerminated);

    m_connection->initConnection();
}

void XdgSessionDemo::setSessionId(const QString &sessionId)
{
    m_sessionId = sessionId;
}

void XdgSessionDemo::setToplevelId(const QString &toplevelId)
{
    m_toplevelId = toplevelId;
}

void XdgSessionDemo::handleConnectionEstablished()
{
    m_eventQueue = new EventQueue(this);
    m_eventQueue->setup(m_connection);
    m_registry = new Registry(m_eventQueue);

    connect(m_registry, &Registry::interfaceAnnounced, this, &XdgSessionDemo::handleInterfaceAnnounced, Qt::QueuedConnection);
    connect(m_registry, &Registry::interfacesAnnounced, this, &XdgSessionDemo::handleInterfacesAnnounced, Qt::QueuedConnection);

    m_registry->setEventQueue(m_eventQueue);
    m_registry->create(m_connection);
    m_registry->setup();
}

void XdgSessionDemo::handleInterfaceAnnounced(const QByteArray &interface, quint32 name, quint32 version)
{
    if (interface == wl_compositor_interface.name) {
        m_compositor = m_registry->createCompositor(name, version, this);
    } else if (interface == wl_shm_interface.name) {
        m_shm = m_registry->createShmPool(name, version, this);
    } else if (interface == QByteArrayLiteral("xdg_wm_base")) {
        m_shell = m_registry->createXdgShell(name, version, this);
    } else if (interface == xdg_session_manager_v1_interface.name) {
        m_sessionManager = std::make_unique<XdgSessionManagerV1>();
        m_sessionManager->init(*m_registry, name, 1);
    } else if (interface == zxdg_decoration_manager_v1_interface.name) {
        m_decorationManager = std::make_unique<XdgDecorationManagerV1>();
        m_decorationManager->init(*m_registry, name, 1);
    }
}

void XdgSessionDemo::handleInterfacesAnnounced()
{
    if (!m_sessionManager) {
        qWarning() << "Compositor does not provide support for the xdg_session_manager_v1";
        return;
    }

    m_surface = m_compositor->createSurface(this);
    m_shellSurface = m_shell->createSurface(m_surface, this);

    m_session = std::make_unique<XdgSessionV1>();
    m_session->init(m_sessionManager->get_session(m_sessionId));

    m_toplevelSession = std::make_unique<XdgToplevelSessionV1>();
    m_toplevelSession->init(m_session->get_toplevel_session(*m_shellSurface, m_toplevelId));

    if (m_decorationManager) {
        m_toplevelDecoration = std::make_unique<XdgToplevelDecorationV1>();
        m_toplevelDecoration->init(m_decorationManager->get_toplevel_decoration(*m_shellSurface));
    }

    m_shellSurface->setTitle(QStringLiteral("xdg-session-v1 demo"));
    m_shellSurface->setMinSize(QSize(500, 500));

    m_surface->commit(Surface::CommitFlag::None);

    connect(m_shellSurface, &XdgShellSurface::configureRequested, this, &XdgSessionDemo::handleConfigureRequested);
    connect(m_shellSurface, &XdgShellSurface::closeRequested, qApp, &QCoreApplication::quit);
}

void XdgSessionDemo::handleConfigureRequested(const QSize &size, XdgShellSurface::States, int serial)
{
    m_shellSurface->ackConfigure(serial);

    QSize desiredSize = size;
    if (desiredSize.isEmpty()) {
        desiredSize = QSize(500, 500);
    }

    render(desiredSize);
}

void XdgSessionDemo::handleConnectionTerminated()
{
    qDebug() << "Wayland connection has died";

    m_toplevelDecoration.reset();
    m_decorationManager.reset();

    m_toplevelSession.reset();
    m_session.reset();
    m_sessionManager.reset();

    // xdg-shell objects.
    if (m_shellSurface) {
        m_shellSurface->destroy();
        m_shellSurface = nullptr;
    }
    if (m_shell) {
        m_shell->destroy();
        m_shell = nullptr;
    }

    // wl-compositor objects.
    if (m_surface) {
        m_surface->destroy();
        m_surface = nullptr;
    }
    if (m_compositor) {
        m_compositor->destroy();
        m_compositor = nullptr;
    }

    // wl-shm.
    if (m_shm) {
        m_shm->destroy();
        m_shm = nullptr;
    }

    // core objects.
    if (m_registry) {
        m_registry->destroy();
        m_registry = nullptr;
    }
    if (m_eventQueue) {
        m_eventQueue->destroy();
        m_eventQueue = nullptr;
    }
}

void XdgSessionDemo::render(const QSize &size)
{
    QSharedPointer<Buffer> buffer = m_shm->getBuffer(size, size.width() * 4).toStrongRef();
    buffer->setUsed(true);

    QImage image(buffer->address(), size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 255, 255, 255));

    QPainter painter(&image);
    painter.setBrush(Qt::red);
    painter.setPen(Qt::black);
    painter.drawRect(50, 50, 400, 400);

    m_surface->attachBuffer(*buffer);
    m_surface->damage(QRect(QPoint(0, 0), size));
    m_surface->commit(Surface::CommitFlag::None);

    buffer->setUsed(false);
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();

    QCommandLineOption sessionIdOption(QStringLiteral("session-id"),
                                       QStringLiteral("The session id"),
                                       QStringLiteral("session-id"));
    parser.addOption(sessionIdOption);

    QCommandLineOption toplevelIdOption(QStringLiteral("toplevel-id"),
                                        QStringLiteral("The toplevel id"),
                                        QStringLiteral("toplevel-id"));
    toplevelIdOption.setDefaultValue(QStringLiteral("main"));
    parser.addOption(toplevelIdOption);

    parser.process(app);

    XdgSessionDemo client;
    client.setSessionId(parser.value(sessionIdOption));
    client.setToplevelId(parser.value(toplevelIdOption));
    client.start();

    return app.exec();
}

#include "xdgsessiontest.moc"
