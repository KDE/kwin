/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "output_interface.h"
#include "display.h"
#include "display_p.h"
#include "utils.h"

#include "qwayland-server-wayland.h"

#include <QPointer>
#include <QVector>

namespace KWaylandServer
{
static const int s_version = 3;

class OutputInterfacePrivate : public QtWaylandServer::wl_output
{
public:
    explicit OutputInterfacePrivate(Display *display, OutputInterface *q, KWin::Output *output);

    void sendScale(Resource *resource);
    void sendGeometry(Resource *resource);
    void sendMode(Resource *resource);
    void sendDone(Resource *resource);

    void broadcastGeometry();

    OutputInterface *q;
    QPointer<Display> display;
    QPointer<KWin::Output> output;

private:
    void output_destroy_global() override;
    void output_bind_resource(Resource *resource) override;
    void output_release(Resource *resource) override;
};

OutputInterfacePrivate::OutputInterfacePrivate(Display *display, OutputInterface *q, KWin::Output *output)
    : QtWaylandServer::wl_output(*display, s_version)
    , q(q)
    , display(display)
    , output(output)
{
}

void OutputInterface::update()
{
    const auto outputResources = d->resourceMap();
    for (OutputInterfacePrivate::Resource *resource : outputResources) {
        d->sendGeometry(resource);
        d->sendScale(resource);
        d->sendMode(resource);
    }
}

void OutputInterfacePrivate::sendMode(Resource *resource)
{
    auto mode = output->currentMode();
    send_mode(resource->handle, mode_current, mode->size().width(), mode->size().height(), mode->refreshRate());
}

void OutputInterfacePrivate::sendScale(Resource *resource)
{
    if (resource->version() >= WL_OUTPUT_SCALE_SINCE_VERSION) {
        send_scale(resource->handle, std::ceil(output->scale()));
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
                  output->geometry().x(),
                  output->geometry().y(),
                  output->physicalSize().width(),
                  output->physicalSize().height(),
                  kwaylandServerSubPixelToWaylandSubPixel(output->subPixel()),
                  output->manufacturer(),
                  output->model(),
                  kwaylandServerTransformToWaylandTransform(output->transform()));
}

void OutputInterfacePrivate::sendDone(Resource *resource)
{
    if (resource->version() >= WL_OUTPUT_DONE_SINCE_VERSION) {
        send_done(resource->handle);
    }
}

void OutputInterfacePrivate::broadcastGeometry()
{
    const auto outputResources = resourceMap();
    for (Resource *resource : outputResources) {
        sendGeometry(resource);
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

    sendMode(resource);
    sendScale(resource);
    sendGeometry(resource);
    sendDone(resource);

    Q_EMIT q->bound(display->getConnection(resource->client()), resource->handle);
}

OutputInterface::OutputInterface(Display *display, KWin::Output *output)
    : d(new OutputInterfacePrivate(display, this, output))
{
    DisplayPrivate *displayPrivate = DisplayPrivate::get(display);
    displayPrivate->outputs.append(this);
    update();
}

OutputInterface::~OutputInterface()
{
    remove();
}

void OutputInterface::remove()
{
    if (d->isGlobalRemoved()) {
        return;
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

void OutputInterface::done()
{
    const auto outputResources = d->resourceMap();
    for (OutputInterfacePrivate::Resource *resource : outputResources) {
        d->sendDone(resource);
    }
}

void OutputInterface::done(wl_client *client)
{
    d->sendDone(d->resourceMap().value(client));
}

KWin::Output *OutputInterface::output() const
{
    return d->output;
}

OutputInterface *OutputInterface::get(wl_resource *native)
{
    if (auto outputPrivate = resource_cast<OutputInterfacePrivate *>(native)) {
        return outputPrivate->q;
    }
    return nullptr;
}

} // namespace KWaylandServer
