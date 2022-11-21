/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "output_interface.h"
#include "display.h"
#include "display_p.h"
#include "utils.h"

#include "core/output.h"

#include "qwayland-server-wayland.h"

#include <QPointer>
#include <QTimer>
#include <QVector>

namespace KWaylandServer
{
static const int s_version = 4;

class OutputInterfacePrivate : public QtWaylandServer::wl_output
{
public:
    explicit OutputInterfacePrivate(Display *display, OutputInterface *q, KWin::Output *handle);

    void sendScale(Resource *resource);
    void sendGeometry(Resource *resource);
    void sendMode(Resource *resource);
    void sendDone(Resource *resource);

    OutputInterface *q;
    QPointer<Display> display;
    QPointer<KWin::Output> handle;
    QSize physicalSize;
    QPoint globalPosition;
    QString manufacturer;
    QString model;
    int scale = 1;
    KWin::Output::SubPixel subPixel = KWin::Output::SubPixel::Unknown;
    KWin::Output::Transform transform = KWin::Output::Transform::Normal;
    QSize modeSize;
    int refreshRate = 0;
    QString name;
    QString description;
    QTimer doneTimer;

private:
    void output_destroy_global() override;
    void output_bind_resource(Resource *resource) override;
    void output_release(Resource *resource) override;
};

OutputInterfacePrivate::OutputInterfacePrivate(Display *display, OutputInterface *q, KWin::Output *handle)
    : QtWaylandServer::wl_output(*display, s_version)
    , q(q)
    , display(display)
    , handle(handle)
{
}

void OutputInterfacePrivate::sendMode(Resource *resource)
{
    send_mode(resource->handle, mode_current, modeSize.width(), modeSize.height(), refreshRate);
}

void OutputInterfacePrivate::sendScale(Resource *resource)
{
    if (resource->version() >= WL_OUTPUT_SCALE_SINCE_VERSION) {
        send_scale(resource->handle, scale);
    }
}

static quint32 kwaylandServerTransformToWaylandTransform(KWin::Output::Transform transform)
{
    switch (transform) {
    case KWin::Output::Transform::Normal:
        return OutputInterfacePrivate::transform_normal;
    case KWin::Output::Transform::Rotated90:
        return OutputInterfacePrivate::transform_90;
    case KWin::Output::Transform::Rotated180:
        return OutputInterfacePrivate::transform_180;
    case KWin::Output::Transform::Rotated270:
        return OutputInterfacePrivate::transform_270;
    case KWin::Output::Transform::Flipped:
        return OutputInterfacePrivate::transform_flipped;
    case KWin::Output::Transform::Flipped90:
        return OutputInterfacePrivate::transform_flipped_90;
    case KWin::Output::Transform::Flipped180:
        return OutputInterfacePrivate::transform_flipped_180;
    case KWin::Output::Transform::Flipped270:
        return OutputInterfacePrivate::transform_flipped_270;
    default:
        Q_UNREACHABLE();
    }
}

static quint32 kwaylandServerSubPixelToWaylandSubPixel(KWin::Output::SubPixel subPixel)
{
    switch (subPixel) {
    case KWin::Output::SubPixel::Unknown:
        return OutputInterfacePrivate::subpixel_unknown;
    case KWin::Output::SubPixel::None:
        return OutputInterfacePrivate::subpixel_none;
    case KWin::Output::SubPixel::Horizontal_RGB:
        return OutputInterfacePrivate::subpixel_horizontal_rgb;
    case KWin::Output::SubPixel::Horizontal_BGR:
        return OutputInterfacePrivate::subpixel_horizontal_bgr;
    case KWin::Output::SubPixel::Vertical_RGB:
        return OutputInterfacePrivate::subpixel_vertical_rgb;
    case KWin::Output::SubPixel::Vertical_BGR:
        return OutputInterfacePrivate::subpixel_vertical_bgr;
    default:
        Q_UNREACHABLE();
    }
}

void OutputInterfacePrivate::sendGeometry(Resource *resource)
{
    send_geometry(resource->handle,
                  globalPosition.x(),
                  globalPosition.y(),
                  physicalSize.width(),
                  physicalSize.height(),
                  kwaylandServerSubPixelToWaylandSubPixel(subPixel),
                  manufacturer,
                  model,
                  kwaylandServerTransformToWaylandTransform(transform));
}

void OutputInterfacePrivate::sendDone(Resource *resource)
{
    if (resource->version() >= WL_OUTPUT_DONE_SINCE_VERSION) {
        send_done(resource->handle);
    }
}

void OutputInterfacePrivate::output_destroy_global()
{
    delete q;
}

void OutputInterfacePrivate::output_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void OutputInterfacePrivate::output_bind_resource(Resource *resource)
{
    if (isGlobalRemoved()) {
        return; // We are waiting for the wl_output global to be destroyed.
    }

    if (resource->version() >= WL_OUTPUT_NAME_SINCE_VERSION) {
        send_name(resource->handle, name);
    }
    if (resource->version() >= WL_OUTPUT_DESCRIPTION_SINCE_VERSION) {
        send_description(resource->handle, description);
    }

    sendMode(resource);
    sendScale(resource);
    sendGeometry(resource);
    sendDone(resource);

    Q_EMIT q->bound(display->getConnection(resource->client()), resource->handle);
}

OutputInterface::OutputInterface(Display *display, KWin::Output *handle, QObject *parent)
    : QObject(parent)
    , d(new OutputInterfacePrivate(display, this, handle))
{
    DisplayPrivate *displayPrivate = DisplayPrivate::get(display);
    displayPrivate->outputs.append(this);

    // Delay the done event to batch property updates.
    d->doneTimer.setSingleShot(true);
    d->doneTimer.setInterval(0);
    connect(&d->doneTimer, &QTimer::timeout, this, [this]() {
        const auto resources = d->resourceMap();
        for (const auto &resource : resources) {
            d->sendDone(resource);
        }
    });

    d->name = handle->name();
    d->description = handle->description();
    d->transform = handle->transform();
    d->manufacturer = handle->manufacturer();
    d->model = handle->model();
    d->physicalSize = handle->physicalSize();
    d->globalPosition = handle->geometry().topLeft();
    d->scale = std::ceil(handle->scale());
    d->modeSize = handle->modeSize();
    d->refreshRate = handle->refreshRate();
    d->subPixel = handle->subPixel();

    connect(handle, &KWin::Output::geometryChanged, this, [this]() {
        const QPoint position = d->handle->geometry().topLeft();
        if (d->globalPosition != position) {
            d->globalPosition = position;
            const auto resources = d->resourceMap();
            for (const auto &resource : resources) {
                d->sendGeometry(resource);
            }
            scheduleDone();
        }
    });

    connect(handle, &KWin::Output::scaleChanged, this, [this]() {
        const int scale = std::ceil(d->handle->scale());
        if (d->scale != scale) {
            d->scale = scale;
            const auto resources = d->resourceMap();
            for (const auto &resource : resources) {
                d->sendScale(resource);
            }
            scheduleDone();
        }
    });

    connect(handle, &KWin::Output::transformChanged, this, [this]() {
        const KWin::Output::Transform transform = d->handle->transform();
        if (d->transform != transform) {
            d->transform = transform;
            const auto resources = d->resourceMap();
            for (const auto &resource : resources) {
                d->sendGeometry(resource);
            }
            scheduleDone();
        }
    });

    connect(handle, &KWin::Output::currentModeChanged, this, [this]() {
        const QSize size = d->handle->modeSize();
        const int refreshRate = d->handle->refreshRate();
        if (d->modeSize != size || d->refreshRate != refreshRate) {
            d->modeSize = size;
            d->refreshRate = refreshRate;
            const auto resources = d->resourceMap();
            for (const auto &resource : resources) {
                d->sendMode(resource);
            }
            scheduleDone();
        }
    });
}

OutputInterface::~OutputInterface()
{
    remove();
}

Display *OutputInterface::display() const
{
    return d->display;
}

KWin::Output *OutputInterface::handle() const
{
    return d->handle;
}

bool OutputInterface::isRemoved() const
{
    return d->isGlobalRemoved();
}

void OutputInterface::remove()
{
    if (d->isGlobalRemoved()) {
        return;
    }

    d->doneTimer.stop();
    if (d->handle) {
        disconnect(d->handle, nullptr, this, nullptr);
    }

    if (d->display) {
        DisplayPrivate *displayPrivate = DisplayPrivate::get(d->display);
        displayPrivate->outputs.removeOne(this);
    }

    Q_EMIT removed();
    d->globalRemove();
}

QVector<wl_resource *> OutputInterface::clientResources(ClientConnection *client) const
{
    const auto outputResources = d->resourceMap().values(client->client());
    QVector<wl_resource *> ret;
    ret.reserve(outputResources.count());

    for (OutputInterfacePrivate::Resource *resource : outputResources) {
        ret.append(resource->handle);
    }

    return ret;
}

void OutputInterface::scheduleDone()
{
    if (!d->isGlobalRemoved()) {
        d->doneTimer.start();
    }
}

void OutputInterface::done(wl_client *client)
{
    if (!d->isGlobalRemoved()) {
        d->sendDone(d->resourceMap().value(client));
    }
}

OutputInterface *OutputInterface::get(wl_resource *native)
{
    if (auto outputPrivate = resource_cast<OutputInterfacePrivate *>(native)) {
        return outputPrivate->q;
    }
    return nullptr;
}

} // namespace KWaylandServer
