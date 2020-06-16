/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "screencast_interface.h"
#include "display.h"
#include <QDebug>

#include "qwayland-server-zkde-screencast-unstable-v1.h"

using namespace KWaylandServer;

static int s_version = 1;

class KWaylandServer::ScreencastStreamInterfacePrivate : public QtWaylandServer::zkde_screencast_stream_unstable_v1
{
public:
    ScreencastStreamInterfacePrivate(ScreencastStreamInterface *q)
        : q(q)
    {}

    void zkde_screencast_stream_unstable_v1_destroy_resource(Resource *resource) override
    {
        Q_UNUSED(resource);
        if (!stopped) {
            Q_EMIT q->finished();
        }

        q->deleteLater();
    }
    
    void zkde_screencast_stream_unstable_v1_close(Resource * resource) override
    {
        Q_UNUSED(resource);
        Q_EMIT q->finished();
        stopped = true;
        wl_resource_destroy(resource->handle);
    }

    bool stopped = false;
    ScreencastStreamInterface *const q;
};

ScreencastStreamInterface::ScreencastStreamInterface(QObject* parent)
    : QObject(parent)
    , d(new ScreencastStreamInterfacePrivate(this))
{
}

ScreencastStreamInterface::~ScreencastStreamInterface() = default;

void ScreencastStreamInterface::sendCreated(quint32 nodeid)
{
    d->send_created(nodeid);
}

void ScreencastStreamInterface::sendFailed(const QString& error)
{
    d->send_failed(error);
}

void ScreencastStreamInterface::sendClosed()
{
    if (!d->stopped) {
        d->send_closed();
    }
}

class KWaylandServer::ScreencastInterfacePrivate : public QtWaylandServer::zkde_screencast_unstable_v1
{
public:
    ScreencastInterfacePrivate(Display *display, ScreencastInterface* q)
        : QtWaylandServer::zkde_screencast_unstable_v1(*display, s_version)
        , q(q)
    {
    }

    ScreencastStreamInterface *createStream(Resource *resource, quint32 streamid) const
    {
        auto stream = new ScreencastStreamInterface(q);
        stream->d->init(resource->client(), streamid, resource->version());
        return stream;
    }

    void zkde_screencast_unstable_v1_stream_output(Resource *resource, uint32_t streamid, struct ::wl_resource *output, uint32_t pointer) override
    {
        Q_EMIT q->outputScreencastRequested(createStream(resource, streamid), output, ScreencastInterface::CursorMode(pointer));
    }

    void zkde_screencast_unstable_v1_stream_window(Resource *resource, uint32_t streamid, const QString &uuid, uint32_t pointer) override
    {
        Q_EMIT q->windowScreencastRequested(createStream(resource, streamid), uuid, ScreencastInterface::CursorMode(pointer));
    }

    void zkde_screencast_unstable_v1_destroy(Resource * resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    ScreencastInterface *const q;
};

ScreencastInterface::ScreencastInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new ScreencastInterfacePrivate(display, this))
{
}

ScreencastInterface::~ScreencastInterface() = default;
