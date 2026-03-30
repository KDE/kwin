/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "screencast_v2.h"
#include "clientconnection.h"
#include "display.h"
#include "output.h"

#include "qwayland-server-kde-screencast-v2.h"

#include <QPointer>

namespace KWin
{

static constexpr ScreencastStreamV2Interface::CursorMode cursorModeFromPointerMode(uint32_t pointer_mode)
{
    using enum QtWaylandServer::kde_screencast_manager_v2::pointer_mode;
    using enum ScreencastStreamV2Interface::CursorMode;

    switch (pointer_mode) {
    case pointer_mode_hidden:
        return Hidden;
    case pointer_mode_embedded:
    default:
        return Embedded;
    case pointer_mode_metadata:
        return Metadata;
    }
}

static int s_version = 1;

class ScreencastStreamV2InterfacePrivate : public QtWaylandServer::kde_screencast_stream_v2
{
public:
    ScreencastStreamV2InterfacePrivate(ScreencastStreamV2Interface *q)
        : q(q)
    {
    }

    void kde_screencast_stream_v2_destroy_resource(Resource *resource) override
    {
        if (!stopped) {
            Q_EMIT q->finished();
        }
        delete q;
    }

    void kde_screencast_stream_v2_destroy(Resource *resource) override
    {
        Q_EMIT q->finished();
        stopped = true;
        wl_resource_destroy(resource->handle);
    }

    bool stopped = false;
    ScreencastStreamV2Interface *const q;
};

ScreencastStreamV2Interface::ScreencastStreamV2Interface()
    : d(std::make_unique<ScreencastStreamV2InterfacePrivate>(this))
{
}

ScreencastStreamV2Interface::~ScreencastStreamV2Interface() = default;

void ScreencastStreamV2Interface::sendCreated(quint32 nodeid, quint64 objectSerial)
{
    d->send_created(nodeid, objectSerial >> 32, objectSerial);
}

void ScreencastStreamV2Interface::sendFailed(const QString &error)
{
    d->send_failed(error);
}

void ScreencastStreamV2Interface::sendClosed()
{
    if (!d->stopped) {
        d->send_closed();
    }
}

ClientConnection *ScreencastStreamV2Interface::connection() const
{
    return ClientConnection::get(d->resource()->client());
}

class ScreencastOutputParamsV2Private : public QtWaylandServer::kde_screencast_output_params_v2
{
public:
    ScreencastOutputParamsV2Private(ScreencastManagerV2Interface *manager, const QString &output)
        : manager(manager)
        , output(output)
    {
    }

    QPointer<ScreencastManagerV2Interface> manager;
    const QString output;
    ScreencastStreamV2Interface::CursorMode cursorMode = ScreencastStreamV2Interface::CursorMode::Embedded;

    void kde_screencast_output_params_v2_set_pointer_mode(Resource *resource, uint32_t pointer_mode) override
    {
        cursorMode = cursorModeFromPointerMode(pointer_mode);
    }
    void kde_screencast_output_params_v2_create_stream(Resource *resource, uint32_t stream) override
    {
        auto s = new ScreencastStreamV2Interface();
        s->d->init(resource->client(), stream, resource->version());
        wl_resource_destroy(resource->handle);

        if (!manager) {
            s->sendFailed(QStringLiteral("Manager already destroyed!"));
            return;
        }
        Q_EMIT manager->outputScreencastRequested(s, ScreencastOutputParamsV2(this));
    }
};

ScreencastOutputParamsV2::ScreencastOutputParamsV2(ScreencastOutputParamsV2Private *d)
    : d(d)
{
}

ScreencastOutputParamsV2::~ScreencastOutputParamsV2() = default;

ScreencastStreamV2Interface::CursorMode ScreencastOutputParamsV2::cursorMode() const
{
    return d->cursorMode;
}

QString ScreencastOutputParamsV2::output() const
{
    return d->output;
}

class ScreencastWindowParamsV2Private : public QtWaylandServer::kde_screencast_window_params_v2
{
public:
    ScreencastWindowParamsV2Private(ScreencastManagerV2Interface *manager, const QString &window)
        : manager(manager)
        , window(window)
    {
    }

    QPointer<ScreencastManagerV2Interface> manager;
    const QString window;
    ScreencastStreamV2Interface::CursorMode cursorMode = ScreencastStreamV2Interface::CursorMode::Embedded;

    void kde_screencast_window_params_v2_set_pointer_mode(Resource *resource, uint32_t pointer_mode) override
    {
        cursorMode = cursorModeFromPointerMode(pointer_mode);
    }
    void kde_screencast_window_params_v2_create_stream(Resource *resource, uint32_t stream) override
    {
        auto s = new ScreencastStreamV2Interface();
        s->d->init(resource->client(), stream, resource->version());
        wl_resource_destroy(resource->handle);

        if (!manager) {
            s->sendFailed(QStringLiteral("Manager already destroyed!"));
            return;
        }
        Q_EMIT manager->windowScreencastRequested(s, ScreencastWindowParamsV2(this));
    }
};

ScreencastWindowParamsV2::ScreencastWindowParamsV2(ScreencastWindowParamsV2Private *d)
    : d(d)
{
}

ScreencastWindowParamsV2::~ScreencastWindowParamsV2() = default;

QString ScreencastWindowParamsV2::window() const
{
    return d->window;
}

ScreencastStreamV2Interface::CursorMode ScreencastWindowParamsV2::cursorMode() const
{
    return d->cursorMode;
}

class ScreencastRegionParamsV2Private : public QtWaylandServer::kde_screencast_region_params_v2
{
public:
    ScreencastRegionParamsV2Private(ScreencastManagerV2Interface *manager, const QRect &region)
        : manager(manager)
        , region(region)
    {
    }

    QPointer<ScreencastManagerV2Interface> manager;
    const QRect region;
    ScreencastStreamV2Interface::CursorMode cursorMode = ScreencastStreamV2Interface::CursorMode::Embedded;
    double scale = 0;

    void kde_screencast_region_params_v2_set_pointer_mode(Resource *resource, uint32_t pointer_mode) override
    {
        cursorMode = cursorModeFromPointerMode(pointer_mode);
    }
    void kde_screencast_region_params_v2_set_scale(Resource *resource, wl_fixed_t scale) override
    {
        scale = wl_fixed_to_double(scale);
    }
    void kde_screencast_region_params_v2_create_stream(Resource *resource, uint32_t stream) override
    {
        auto s = new ScreencastStreamV2Interface();
        s->d->init(resource->client(), stream, resource->version());
        wl_resource_destroy(resource->handle);

        if (!manager) {
            s->sendFailed(QStringLiteral("Manager already destroyed!"));
            return;
        }
        Q_EMIT manager->regionScreencastRequested(s, ScreencastRegionParamsV2(this));
    }
};

ScreencastRegionParamsV2::ScreencastRegionParamsV2(ScreencastRegionParamsV2Private *d)
    : d(d)
{
}

ScreencastRegionParamsV2::~ScreencastRegionParamsV2() = default;

QRect ScreencastRegionParamsV2::region() const
{
    return d->region;
}

ScreencastStreamV2Interface::CursorMode ScreencastRegionParamsV2::cursorMode() const
{
    return d->cursorMode;
}

double ScreencastRegionParamsV2::scale() const
{
    return d->scale;
}

class ScreencastVirtualOutputParamsV2Private : public QtWaylandServer::kde_screencast_virtual_output_params_v2
{
public:
    ScreencastVirtualOutputParamsV2Private(ScreencastManagerV2Interface *manager, const QSize &size, double scale)
        : manager(manager)
        , size(size)
        , scale(scale)
    {
    }

    QPointer<ScreencastManagerV2Interface> manager;
    const QSize size;
    const double scale;
    ScreencastStreamV2Interface::CursorMode cursorMode = ScreencastStreamV2Interface::CursorMode::Embedded;
    QString name;
    QString description;

    void kde_screencast_virtual_output_params_v2_set_pointer_mode(Resource *resource, uint32_t pointer_mode) override
    {
        cursorMode = cursorModeFromPointerMode(pointer_mode);
    }

    void kde_screencast_virtual_output_params_v2_set_name(Resource *resource, const QString &name) override
    {
        this->name = name;
    }

    void kde_screencast_virtual_output_params_v2_set_description(Resource *resource, const QString &description) override
    {
        this->description = description;
    }

    void kde_screencast_virtual_output_params_v2_create_stream(Resource *resource, uint32_t stream) override
    {
        auto s = new ScreencastStreamV2Interface();
        s->d->init(resource->client(), stream, resource->version());
        wl_resource_destroy(resource->handle);

        if (!manager) {
            s->sendFailed(QStringLiteral("Manager already destroyed!"));
            return;
        }
        Q_EMIT manager->virtualOutputScreencastRequested(s, ScreencastVirtualOutputParamsV2(this));
    }
};

ScreencastVirtualOutputParamsV2::ScreencastVirtualOutputParamsV2(ScreencastVirtualOutputParamsV2Private *d)
    : d(d)
{
}

ScreencastVirtualOutputParamsV2::~ScreencastVirtualOutputParamsV2() = default;

QSize ScreencastVirtualOutputParamsV2::size() const
{
    return d->size;
}

double ScreencastVirtualOutputParamsV2::scale() const
{
    return d->scale;
}

QString ScreencastVirtualOutputParamsV2::name() const
{
    return d->name;
}

QString ScreencastVirtualOutputParamsV2::description() const
{
    return d->description;
}

ScreencastStreamV2Interface::CursorMode ScreencastVirtualOutputParamsV2::cursorMode() const
{
    return d->cursorMode;
}

class ScreencastManagerV2InterfacePrivate : public QtWaylandServer::kde_screencast_manager_v2
{
public:
    ScreencastManagerV2InterfacePrivate(Display *display, ScreencastManagerV2Interface *q)
        : QtWaylandServer::kde_screencast_manager_v2(*display, s_version)
        , q(q)
    {
    }

    void kde_screencast_manager_v2_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void kde_screencast_manager_v2_stream_window(Resource *resource, uint32_t params, const QString &window_uuid) override
    {
        auto windowParams = new ScreencastWindowParamsV2Private(q, window_uuid);
        windowParams->init(resource->client(), params, resource->version());
    }

    void kde_screencast_manager_v2_stream_output(Resource *resource, uint32_t params, const QString &output) override
    {
        auto outputParams = new ScreencastOutputParamsV2Private(q, output);
        outputParams->init(resource->client(), params, resource->version());
    }

    void kde_screencast_manager_v2_stream_region(Resource *resource, uint32_t params, int32_t x, int32_t y, uint32_t width, uint32_t height) override
    {
        auto regionParams = new ScreencastRegionParamsV2Private(q, QRect(x, y, width, height));
        regionParams->init(resource->client(), params, resource->version());
    }

    void kde_screencast_manager_v2_stream_virtual_output(Resource *resource, uint32_t params, int32_t width, int32_t height, wl_fixed_t scale) override
    {
    }

    ScreencastManagerV2Interface *const q;
};

ScreencastManagerV2Interface::ScreencastManagerV2Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<ScreencastManagerV2InterfacePrivate>(display, this))
{
}

ScreencastManagerV2Interface::~ScreencastManagerV2Interface() = default;

} // namespace KWin
