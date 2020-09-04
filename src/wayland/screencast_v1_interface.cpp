/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "screencast_v1_interface.h"
#include "display.h"
#include "output_interface.h"

#include <QDebug>

#include "qwayland-server-zkde-screencast-unstable-v1.h"

namespace KWaylandServer
{

static int s_version = 1;

class ScreencastStreamV1InterfacePrivate : public QtWaylandServer::zkde_screencast_stream_unstable_v1
{
public:
    ScreencastStreamV1InterfacePrivate(ScreencastStreamV1Interface *q)
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

    void zkde_screencast_stream_unstable_v1_close(Resource *resource) override
    {
        Q_UNUSED(resource);
        Q_EMIT q->finished();
        stopped = true;
        wl_resource_destroy(resource->handle);
    }

    bool stopped = false;
    ScreencastStreamV1Interface *const q;
};

ScreencastStreamV1Interface::ScreencastStreamV1Interface(QObject *parent)
    : QObject(parent)
    , d(new ScreencastStreamV1InterfacePrivate(this))
{
}

ScreencastStreamV1Interface::~ScreencastStreamV1Interface() = default;

void ScreencastStreamV1Interface::sendCreated(quint32 nodeid)
{
    d->send_created(nodeid);
}

void ScreencastStreamV1Interface::sendFailed(const QString &error)
{
    d->send_failed(error);
}

void ScreencastStreamV1Interface::sendClosed()
{
    if (!d->stopped) {
        d->send_closed();
    }
}

class ScreencastV1InterfacePrivate : public QtWaylandServer::zkde_screencast_unstable_v1
{
public:
    ScreencastV1InterfacePrivate(Display *display, ScreencastV1Interface *q)
        : QtWaylandServer::zkde_screencast_unstable_v1(*display, s_version)
        , q(q)
    {
    }

    ScreencastStreamV1Interface *createStream(Resource *resource, quint32 streamid) const
    {
        auto stream = new ScreencastStreamV1Interface(q);
        stream->d->init(resource->client(), streamid, resource->version());
        return stream;
    }

    void zkde_screencast_unstable_v1_stream_output(Resource *resource, uint32_t streamid, struct ::wl_resource *output, uint32_t pointer) override
    {
        Q_EMIT q->outputScreencastRequested(createStream(resource, streamid), OutputInterface::get(output), ScreencastV1Interface::CursorMode(pointer));
    }

    void zkde_screencast_unstable_v1_stream_window(Resource *resource, uint32_t streamid, const QString &uuid, uint32_t pointer) override
    {
        Q_EMIT q->windowScreencastRequested(createStream(resource, streamid), uuid, ScreencastV1Interface::CursorMode(pointer));
    }

    void zkde_screencast_unstable_v1_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    ScreencastV1Interface *const q;
};

ScreencastV1Interface::ScreencastV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new ScreencastV1InterfacePrivate(display, this))
{
}

ScreencastV1Interface::~ScreencastV1Interface() = default;

} // namespace KWaylandServer
