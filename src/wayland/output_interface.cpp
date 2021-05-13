/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "output_interface.h"
#include "display_p.h"
#include "display.h"
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
    explicit OutputInterfacePrivate(Display *display, OutputInterface *q);

    void sendScale(Resource *resource);
    void sendGeometry(Resource *resource);
    void sendMode(Resource *resource);
    void sendDone(Resource *resource);

    void broadcastGeometry();

    OutputInterface *q;
    QPointer<Display> display;
    QSize physicalSize;
    QPoint globalPosition;
    QString manufacturer = QStringLiteral("org.kde.kwin");
    QString model = QStringLiteral("none");
    int scale = 1;
    OutputInterface::SubPixel subPixel = OutputInterface::SubPixel::Unknown;
    OutputInterface::Transform transform = OutputInterface::Transform::Normal;
    OutputInterface::Mode mode;
    struct {
        OutputInterface::DpmsMode mode = OutputInterface::DpmsMode::Off;
        bool supported = false;
    } dpms;

private:
    void output_destroy_global() override;
    void output_bind_resource(Resource *resource);
    void output_release(Resource *resource);
};

OutputInterfacePrivate::OutputInterfacePrivate(Display *display, OutputInterface *q)
    : QtWaylandServer::wl_output(*display, s_version)
    , q(q)
    , display(display)
{
}

void OutputInterfacePrivate::sendMode(Resource *resource)
{
    send_mode(resource->handle, mode_current, mode.size.width(), mode.size.height(), mode.refreshRate);
}

void OutputInterfacePrivate::sendScale(Resource *resource)
{
    if (resource->version() >= WL_OUTPUT_SCALE_SINCE_VERSION) {
        send_scale(resource->handle, scale);
    }
}

static quint32 kwaylandServerTransformToWaylandTransform(OutputInterface::Transform transform)
{
    switch (transform) {
    case OutputInterface::Transform::Normal:
        return OutputInterfacePrivate::transform_normal;
    case OutputInterface::Transform::Rotated90:
        return OutputInterfacePrivate::transform_90;
    case OutputInterface::Transform::Rotated180:
        return OutputInterfacePrivate::transform_180;
    case OutputInterface::Transform::Rotated270:
        return OutputInterfacePrivate::transform_270;
    case OutputInterface::Transform::Flipped:
        return OutputInterfacePrivate::transform_flipped;
    case OutputInterface::Transform::Flipped90:
        return OutputInterfacePrivate::transform_flipped_90;
    case OutputInterface::Transform::Flipped180:
        return OutputInterfacePrivate::transform_flipped_180;
    case OutputInterface::Transform::Flipped270:
        return OutputInterfacePrivate::transform_flipped_270;
    default:
        Q_UNREACHABLE();
    }
}

static quint32 kwaylandServerSubPixelToWaylandSubPixel(OutputInterface::SubPixel subPixel)
{
    switch (subPixel) {
    case OutputInterface::SubPixel::Unknown:
        return OutputInterfacePrivate::subpixel_unknown;
    case OutputInterface::SubPixel::None:
        return OutputInterfacePrivate::subpixel_none;
    case OutputInterface::SubPixel::HorizontalRGB:
        return OutputInterfacePrivate::subpixel_horizontal_rgb;
    case OutputInterface::SubPixel::HorizontalBGR:
        return OutputInterfacePrivate::subpixel_horizontal_bgr;
    case OutputInterface::SubPixel::VerticalRGB:
        return OutputInterfacePrivate::subpixel_vertical_rgb;
    case OutputInterface::SubPixel::VerticalBGR:
        return OutputInterfacePrivate::subpixel_vertical_bgr;
    default:
        Q_UNREACHABLE();
    }
}

void OutputInterfacePrivate::sendGeometry(Resource *resource)
{
    send_geometry(resource->handle, globalPosition.x(), globalPosition.y(),
                  physicalSize.width(), physicalSize.height(),
                  kwaylandServerSubPixelToWaylandSubPixel(subPixel),
                  manufacturer, model,
                  kwaylandServerTransformToWaylandTransform(transform));
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

OutputInterface::OutputInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new OutputInterfacePrivate(display, this))
{
    DisplayPrivate *displayPrivate = DisplayPrivate::get(display);
    displayPrivate->outputs.append(this);
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

    d->globalRemove();
    Q_EMIT removed();
}

QSize OutputInterface::pixelSize() const
{
    return d->mode.size;
}

int OutputInterface::refreshRate() const
{
    return d->mode.refreshRate;
}

OutputInterface::Mode OutputInterface::mode() const
{
    return d->mode;
}

void OutputInterface::setMode(const Mode &mode)
{
    if (d->mode.size == mode.size && d->mode.refreshRate == mode.refreshRate) {
        return;
    }

    d->mode = mode;

    const auto outputResources = d->resourceMap();
    for (OutputInterfacePrivate::Resource *resource : outputResources) {
        d->sendMode(resource);
    }

    Q_EMIT modeChanged();
    Q_EMIT refreshRateChanged(mode.refreshRate);
    Q_EMIT pixelSizeChanged(mode.size);
}

void OutputInterface::setMode(const QSize &size, int refreshRate)
{
    setMode({size, refreshRate});
}

QSize OutputInterface::physicalSize() const
{
    return d->physicalSize;
}

void OutputInterface::setPhysicalSize(const QSize &physicalSize)
{
    if (d->physicalSize == physicalSize) {
        return;
    }
    d->physicalSize = physicalSize;
    d->broadcastGeometry();
    Q_EMIT physicalSizeChanged(d->physicalSize);
}

QPoint OutputInterface::globalPosition() const
{
    return d->globalPosition;
}

void OutputInterface::setGlobalPosition(const QPoint &globalPos)
{
    if (d->globalPosition == globalPos) {
        return;
    }
    d->globalPosition = globalPos;
    Q_EMIT globalPositionChanged(d->globalPosition);
}

QString OutputInterface::manufacturer() const
{
    return d->manufacturer;
}

void OutputInterface::setManufacturer(const QString &manufacturer)
{
    if (d->manufacturer == manufacturer) {
        return;
    }
    d->manufacturer = manufacturer;
    d->broadcastGeometry();
    Q_EMIT manufacturerChanged(d->manufacturer);
}

QString OutputInterface::model() const
{
    return d->model;
}

void OutputInterface::setModel(const QString &model)
{
    if (d->model == model) {
        return;
    }
    d->model = model;
    d->broadcastGeometry();
    Q_EMIT modelChanged(d->model);
}

int OutputInterface::scale() const
{
    return d->scale;
}

void OutputInterface::setScale(int scale)
{
    if (d->scale == scale) {
        return;
    }
    d->scale = scale;

    const auto outputResources = d->resourceMap();
    for (OutputInterfacePrivate::Resource *resource : outputResources) {
        d->sendScale(resource);
    }

    Q_EMIT scaleChanged(d->scale);
}

OutputInterface::SubPixel OutputInterface::subPixel() const
{
    return d->subPixel;
}

void OutputInterface::setSubPixel(SubPixel subPixel)
{
    if (d->subPixel == subPixel) {
        return;
    }
    d->subPixel = subPixel;
    d->broadcastGeometry();
    Q_EMIT subPixelChanged(d->subPixel);
}

OutputInterface::Transform OutputInterface::transform() const
{
    return d->transform;
}

void OutputInterface::setTransform(Transform transform)
{
    if (d->transform == transform) {
        return;
    }
    d->transform = transform;
    d->broadcastGeometry();
    Q_EMIT transformChanged(d->transform);
}

void OutputInterface::setDpmsMode(OutputInterface::DpmsMode mode)
{
    if (d->dpms.mode == mode) {
        return;
    }
    d->dpms.mode = mode;
    Q_EMIT dpmsModeChanged();
}

void OutputInterface::setDpmsSupported(bool supported)
{
    if (d->dpms.supported == supported) {
        return;
    }
    d->dpms.supported = supported;
    Q_EMIT dpmsSupportedChanged();
}

OutputInterface::DpmsMode OutputInterface::dpmsMode() const
{
    return d->dpms.mode;
}

bool OutputInterface::isDpmsSupported() const
{
    return d->dpms.supported;
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

bool OutputInterface::isEnabled() const
{
    if (!d->dpms.supported) {
        return true;
    }
    return d->dpms.mode == DpmsMode::On;
}

void OutputInterface::done()
{
    const auto outputResources = d->resourceMap();
    for (OutputInterfacePrivate::Resource *resource : outputResources) {
        d->sendDone(resource);
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
