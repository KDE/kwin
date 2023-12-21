/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "output.h"
#include "display.h"
#include "display_p.h"
#include "utils/resource.h"

#include "core/output.h"

#include "qwayland-server-wayland.h"

#include <QList>
#include <QPointer>
#include <QTimer>

#include <cmath>

namespace KWin
{
static const int s_version = 4;

class OutputInterfacePrivate : public QtWaylandServer::wl_output
{
public:
    explicit OutputInterfacePrivate(Display *display, OutputInterface *q, Output *handle);

    void sendScale(Resource *resource);
    void sendGeometry(Resource *resource);
    void sendMode(Resource *resource);
    void sendDone(Resource *resource);

    OutputInterface *q;
    QPointer<Display> display;
    QPointer<Output> handle;
    QSize physicalSize;
    QPoint globalPosition;
    QString manufacturer;
    QString model;
    int scale = 1;
    Output::SubPixel subPixel = Output::SubPixel::Unknown;
    OutputTransform transform = OutputTransform::Normal;
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

OutputInterfacePrivate::OutputInterfacePrivate(Display *display, OutputInterface *q, Output *handle)
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

static quint32 kwaylandServerTransformToWaylandTransform(OutputTransform transform)
{
    switch (transform.kind()) {
    case OutputTransform::Normal:
        return OutputInterfacePrivate::transform_normal;
    case OutputTransform::Rotate90:
        return OutputInterfacePrivate::transform_90;
    case OutputTransform::Rotate180:
        return OutputInterfacePrivate::transform_180;
    case OutputTransform::Rotate270:
        return OutputInterfacePrivate::transform_270;
    case OutputTransform::FlipX:
        return OutputInterfacePrivate::transform_flipped;
    case OutputTransform::FlipX90:
        return OutputInterfacePrivate::transform_flipped_90;
    case OutputTransform::FlipX180:
        return OutputInterfacePrivate::transform_flipped_180;
    case OutputTransform::FlipX270:
        return OutputInterfacePrivate::transform_flipped_270;
    default:
        Q_UNREACHABLE();
    }
}

static quint32 kwaylandServerSubPixelToWaylandSubPixel(Output::SubPixel subPixel)
{
    switch (subPixel) {
    case Output::SubPixel::Unknown:
        return OutputInterfacePrivate::subpixel_unknown;
    case Output::SubPixel::None:
        return OutputInterfacePrivate::subpixel_none;
    case Output::SubPixel::Horizontal_RGB:
        return OutputInterfacePrivate::subpixel_horizontal_rgb;
    case Output::SubPixel::Horizontal_BGR:
        return OutputInterfacePrivate::subpixel_horizontal_bgr;
    case Output::SubPixel::Vertical_RGB:
        return OutputInterfacePrivate::subpixel_vertical_rgb;
    case Output::SubPixel::Vertical_BGR:
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

OutputInterface::OutputInterface(Display *display, Output *handle, QObject *parent)
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

    connect(handle, &Output::geometryChanged, this, [this]() {
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

    connect(handle, &Output::scaleChanged, this, [this]() {
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

    connect(handle, &Output::transformChanged, this, [this]() {
        const OutputTransform transform = d->handle->transform();
        if (d->transform != transform) {
            d->transform = transform;
            const auto resources = d->resourceMap();
            for (const auto &resource : resources) {
                d->sendGeometry(resource);
            }
            scheduleDone();
        }
    });

    connect(handle, &Output::currentModeChanged, this, [this]() {
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

Output *OutputInterface::handle() const
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

QList<wl_resource *> OutputInterface::clientResources(wl_client *client) const
{
    const auto outputResources = d->resourceMap().values(client);
    QList<wl_resource *> ret;
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

} // namespace KWin

#include "wayland/moc_output.cpp"
