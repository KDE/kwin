/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "display.h"
#include "clientbufferintegration.h"
#include "display_p.h"
#include "drmclientbuffer.h"
#include "output_interface.h"
#include "shmclientbuffer.h"
#include "utils/common.h"

#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QDebug>
#include <QRect>

namespace KWaylandServer
{
DisplayPrivate *DisplayPrivate::get(Display *display)
{
    return display->d.get();
}

DisplayPrivate::DisplayPrivate(Display *q)
    : q(q)
{
}

void DisplayPrivate::registerSocketName(const QString &socketName)
{
    socketNames.append(socketName);
    Q_EMIT q->socketNamesChanged();
}

Display::Display(QObject *parent)
    : QObject(parent)
    , d(new DisplayPrivate(this))
{
    d->display = wl_display_create();
    d->loop = wl_display_get_event_loop(d->display);
}

Display::~Display()
{
    wl_display_destroy_clients(d->display);
    wl_display_destroy(d->display);
}

bool Display::addSocketFileDescriptor(int fileDescriptor, const QString &name)
{
    if (wl_display_add_socket_fd(d->display, fileDescriptor)) {
        qCWarning(KWIN_CORE, "Failed to add %d fd to display", fileDescriptor);
        return false;
    }
    if (!name.isEmpty()) {
        d->registerSocketName(name);
    }
    return true;
}

bool Display::addSocketName(const QString &name)
{
    if (name.isEmpty()) {
        const char *socket = wl_display_add_socket_auto(d->display);
        if (!socket) {
            qCWarning(KWIN_CORE, "Failed to find a free display socket");
            return false;
        }
        d->registerSocketName(QString::fromUtf8(socket));
    } else {
        if (wl_display_add_socket(d->display, qPrintable(name))) {
            qCWarning(KWIN_CORE, "Failed to add %s socket to display", qPrintable(name));
            return false;
        }
        d->registerSocketName(name);
    }
    return true;
}

QStringList Display::socketNames() const
{
    return d->socketNames;
}

bool Display::start()
{
    if (d->running) {
        return true;
    }

    const int fileDescriptor = wl_event_loop_get_fd(d->loop);
    if (fileDescriptor == -1) {
        qCWarning(KWIN_CORE) << "Did not get the file descriptor for the event loop";
        return false;
    }

    d->socketNotifier = new QSocketNotifier(fileDescriptor, QSocketNotifier::Read, this);
    connect(d->socketNotifier, &QSocketNotifier::activated, this, &Display::dispatchEvents);

    QAbstractEventDispatcher *dispatcher = QCoreApplication::eventDispatcher();
    connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, this, &Display::flush);

    d->running = true;
    Q_EMIT runningChanged(true);

    return true;
}

void Display::dispatchEvents()
{
    if (wl_event_loop_dispatch(d->loop, 0) != 0) {
        qCWarning(KWIN_CORE) << "Error on dispatching Wayland event loop";
    }
}

void Display::flush()
{
    wl_display_flush_clients(d->display);
}

void Display::createShm()
{
    Q_ASSERT(d->display);
    new ShmClientBufferIntegration(this);
}

quint32 Display::nextSerial()
{
    return wl_display_next_serial(d->display);
}

quint32 Display::serial()
{
    return wl_display_get_serial(d->display);
}

bool Display::isRunning() const
{
    return d->running;
}

Display::operator wl_display *()
{
    return d->display;
}

Display::operator wl_display *() const
{
    return d->display;
}

QList<OutputInterface *> Display::outputs() const
{
    return d->outputs;
}

QList<OutputDeviceV2Interface *> Display::outputDevices() const
{
    return d->outputdevicesV2;
}

QVector<OutputInterface *> Display::outputsIntersecting(const QRect &rect) const
{
    QVector<OutputInterface *> outputs;
    for (auto *output : std::as_const(d->outputs)) {
        if (output->handle()->geometry().intersects(rect)) {
            outputs << output;
        }
    }
    return outputs;
}

QVector<SeatInterface *> Display::seats() const
{
    return d->seats;
}

ClientConnection *Display::getConnection(wl_client *client)
{
    Q_ASSERT(client);
    auto it = std::find_if(d->clients.constBegin(), d->clients.constEnd(), [client](ClientConnection *c) {
        return c->client() == client;
    });
    if (it != d->clients.constEnd()) {
        return *it;
    }
    // no ConnectionData yet, create it
    auto c = new ClientConnection(client, this);
    d->clients << c;
    connect(c, &ClientConnection::disconnected, this, [this](ClientConnection *c) {
        const int index = d->clients.indexOf(c);
        Q_ASSERT(index != -1);
        d->clients.remove(index);
        Q_ASSERT(d->clients.indexOf(c) == -1);
        Q_EMIT clientDisconnected(c);
    });
    Q_EMIT clientConnected(c);
    return c;
}

QVector<ClientConnection *> Display::connections() const
{
    return d->clients;
}

ClientConnection *Display::createClient(int fd)
{
    Q_ASSERT(fd != -1);
    Q_ASSERT(d->display);
    wl_client *c = wl_client_create(d->display, fd);
    if (!c) {
        return nullptr;
    }
    return getConnection(c);
}

void Display::setEglDisplay(void *display)
{
    if (d->eglDisplay != EGL_NO_DISPLAY) {
        qCWarning(KWIN_CORE) << "EGLDisplay cannot be changed";
        return;
    }
    d->eglDisplay = (EGLDisplay)display;
    new DrmClientBufferIntegration(this);
}

void *Display::eglDisplay() const
{
    return d->eglDisplay;
}

struct ClientBufferDestroyListener : wl_listener
{
    ClientBufferDestroyListener(Display *display, ClientBuffer *buffer);
    ~ClientBufferDestroyListener();

    Display *display;
};

void bufferDestroyCallback(wl_listener *listener, void *data)
{
    ClientBufferDestroyListener *destroyListener = static_cast<ClientBufferDestroyListener *>(listener);
    DisplayPrivate *displayPrivate = DisplayPrivate::get(destroyListener->display);

    ClientBuffer *buffer = displayPrivate->q->clientBufferForResource(static_cast<wl_resource *>(data));
    displayPrivate->unregisterClientBuffer(buffer);

    buffer->markAsDestroyed();
}

ClientBufferDestroyListener::ClientBufferDestroyListener(Display *display, ClientBuffer *buffer)
    : display(display)
{
    notify = bufferDestroyCallback;

    link.prev = nullptr;
    link.next = nullptr;

    wl_resource_add_destroy_listener(buffer->resource(), this);
}

ClientBufferDestroyListener::~ClientBufferDestroyListener()
{
    wl_list_remove(&link);
}

ClientBuffer *Display::clientBufferForResource(wl_resource *resource) const
{
    ClientBuffer *buffer = d->resourceToBuffer.value(resource);
    if (buffer) {
        return buffer;
    }

    for (ClientBufferIntegration *integration : std::as_const(d->bufferIntegrations)) {
        ClientBuffer *buffer = integration->createBuffer(resource);
        if (buffer) {
            d->registerClientBuffer(buffer);
            return buffer;
        }
    }
    return nullptr;
}

void DisplayPrivate::registerClientBuffer(ClientBuffer *buffer)
{
    resourceToBuffer.insert(buffer->resource(), buffer);
    bufferToListener.insert(buffer, new ClientBufferDestroyListener(q, buffer));
}

void DisplayPrivate::unregisterClientBuffer(ClientBuffer *buffer)
{
    Q_ASSERT_X(buffer->resource(), "unregisterClientBuffer", "buffer must have valid resource");
    resourceToBuffer.remove(buffer->resource());
    delete bufferToListener.take(buffer);
}

}
