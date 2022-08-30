/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "output_interface.h"
#include "display.h"
#include "display_p.h"
#include "utils.h"
#include "utils/global.h"

#include "output.h"

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include <QPointer>
#include <QVector>

namespace KWaylandServer
{
static const int s_version = 3;

class OutputInterfacePrivate
{
public:
    explicit OutputInterfacePrivate(Display *display, OutputInterface *q, KWin::Output *handle);

    static OutputInterfacePrivate *fromResource(wl_resource *resource);

    void sendScale(wl_resource *resource);
    void sendGeometry(wl_resource *resource);
    void sendMode(wl_resource *resource);
    void sendDone(wl_resource *resource);

    void broadcastGeometry();

    static void output_bind_resource(wl_client *client, void *data, uint32_t version, uint32_t id);
    static void output_destroy_resource(wl_resource *resource);
    static void output_release(wl_client *client, wl_resource *resource);

    static constexpr struct wl_output_interface implementation = {
        .release = output_release,
    };

    OutputInterface *q;
    wl_global *global;
    QMultiMap<wl_client *, wl_resource *> resourceMap;
    QPointer<Display> display;
    QPointer<KWin::Output> handle;
    QSize physicalSize;
    QPoint globalPosition;
    QString manufacturer = QStringLiteral("org.kde.kwin");
    QString model = QStringLiteral("none");
    int scale = 1;
    KWin::Output::SubPixel subPixel = KWin::Output::SubPixel::Unknown;
    KWin::Output::Transform transform = KWin::Output::Transform::Normal;
    OutputInterface::Mode mode;
};

OutputInterfacePrivate *OutputInterfacePrivate::fromResource(wl_resource *resource)
{
    Q_ASSERT(wl_resource_instance_of(resource, &wl_output_interface, &implementation));
    return static_cast<OutputInterfacePrivate *>(wl_resource_get_user_data(resource));
}

void OutputInterfacePrivate::output_destroy_resource(wl_resource *resource)
{
    auto outputPrivate = OutputInterfacePrivate::fromResource(resource);
    if (outputPrivate) {
        outputPrivate->resourceMap.remove(wl_resource_get_client(resource), resource);
    }
}

void OutputInterfacePrivate::output_bind_resource(wl_client *client, void *data, uint32_t version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(client, &wl_output_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(resource, &implementation, data, output_destroy_resource);

    auto outputPrivate = static_cast<OutputInterfacePrivate *>(data);
    if (!outputPrivate) {
        return;
    }

    outputPrivate->resourceMap.insert(client, resource);

    outputPrivate->sendMode(resource);
    outputPrivate->sendScale(resource);
    outputPrivate->sendGeometry(resource);
    outputPrivate->sendDone(resource);

    Q_EMIT outputPrivate->q->bound(outputPrivate->display->getConnection(client), resource);
}

void OutputInterfacePrivate::output_release(wl_client *client, wl_resource *resource)
{
    wl_resource_destroy(resource);
}

OutputInterfacePrivate::OutputInterfacePrivate(Display *display, OutputInterface *q, KWin::Output *handle)
    : q(q)
    , display(display)
    , handle(handle)
{
    global = wl_global_create(*display, &wl_output_interface, s_version, this, output_bind_resource);
}

void OutputInterfacePrivate::sendMode(wl_resource *resource)
{
    wl_output_send_mode(resource, WL_OUTPUT_MODE_CURRENT, mode.size.width(), mode.size.height(), mode.refreshRate);
}

void OutputInterfacePrivate::sendScale(wl_resource *resource)
{
    if (wl_resource_get_version(resource) >= WL_OUTPUT_SCALE_SINCE_VERSION) {
        wl_output_send_scale(resource, scale);
    }
}

static quint32 kwaylandServerTransformToWaylandTransform(KWin::Output::Transform transform)
{
    switch (transform) {
    case KWin::Output::Transform::Normal:
        return WL_OUTPUT_TRANSFORM_NORMAL;
    case KWin::Output::Transform::Rotated90:
        return WL_OUTPUT_TRANSFORM_90;
    case KWin::Output::Transform::Rotated180:
        return WL_OUTPUT_TRANSFORM_180;
    case KWin::Output::Transform::Rotated270:
        return WL_OUTPUT_TRANSFORM_270;
    case KWin::Output::Transform::Flipped:
        return WL_OUTPUT_TRANSFORM_FLIPPED;
    case KWin::Output::Transform::Flipped90:
        return WL_OUTPUT_TRANSFORM_FLIPPED_90;
    case KWin::Output::Transform::Flipped180:
        return WL_OUTPUT_TRANSFORM_FLIPPED_180;
    case KWin::Output::Transform::Flipped270:
        return WL_OUTPUT_TRANSFORM_FLIPPED_270;
    default:
        Q_UNREACHABLE();
    }
}

static quint32 kwaylandServerSubPixelToWaylandSubPixel(KWin::Output::SubPixel subPixel)
{
    switch (subPixel) {
    case KWin::Output::SubPixel::Unknown:
        return WL_OUTPUT_SUBPIXEL_UNKNOWN;
    case KWin::Output::SubPixel::None:
        return WL_OUTPUT_SUBPIXEL_NONE;
    case KWin::Output::SubPixel::Horizontal_RGB:
        return WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB;
    case KWin::Output::SubPixel::Horizontal_BGR:
        return WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR;
    case KWin::Output::SubPixel::Vertical_RGB:
        return WL_OUTPUT_SUBPIXEL_VERTICAL_RGB;
    case KWin::Output::SubPixel::Vertical_BGR:
        return WL_OUTPUT_SUBPIXEL_VERTICAL_BGR;
    default:
        Q_UNREACHABLE();
    }
}

void OutputInterfacePrivate::sendGeometry(wl_resource *resource)
{
    wl_output_send_geometry(resource,
                            globalPosition.x(),
                            globalPosition.y(),
                            physicalSize.width(),
                            physicalSize.height(),
                            kwaylandServerSubPixelToWaylandSubPixel(subPixel),
                            manufacturer.toUtf8().constData(),
                            model.toUtf8().constData(),
                            kwaylandServerTransformToWaylandTransform(transform));
}

void OutputInterfacePrivate::sendDone(wl_resource *resource)
{
    if (wl_resource_get_version(resource) >= WL_OUTPUT_DONE_SINCE_VERSION) {
        wl_output_send_done(resource);
    }
}

void OutputInterfacePrivate::broadcastGeometry()
{
    for (wl_resource *resource : std::as_const(resourceMap)) {
        sendGeometry(resource);
    }
}

OutputInterface::OutputInterface(Display *display, KWin::Output *handle, QObject *parent)
    : QObject(parent)
    , d(new OutputInterfacePrivate(display, this, handle))
{
    DisplayPrivate *displayPrivate = DisplayPrivate::get(display);
    displayPrivate->outputs.append(this);

    connect(display, &Display::aboutToTerminate, this, [this]() {
        wl_global_destroy(d->global);
        d->global = nullptr;
    });
}

OutputInterface::~OutputInterface()
{
    Q_EMIT aboutToBeDestroyed();

    for (wl_resource *resource : std::as_const(d->resourceMap)) {
        wl_resource_set_user_data(resource, nullptr);
    }

    if (d->global) {
        wl_global_destroy_deferred(d->global);
    }

    if (d->display) {
        DisplayPrivate *displayPrivate = DisplayPrivate::get(d->display);
        displayPrivate->outputs.removeOne(this);
    }
}

KWin::Output *OutputInterface::handle() const
{
    return d->handle;
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

    for (wl_resource *resource : std::as_const(d->resourceMap)) {
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

    for (wl_resource *resource : std::as_const(d->resourceMap)) {
        d->sendScale(resource);
    }

    Q_EMIT scaleChanged(d->scale);
}

KWin::Output::SubPixel OutputInterface::subPixel() const
{
    return d->subPixel;
}

void OutputInterface::setSubPixel(KWin::Output::SubPixel subPixel)
{
    if (d->subPixel == subPixel) {
        return;
    }
    d->subPixel = subPixel;
    d->broadcastGeometry();
    Q_EMIT subPixelChanged(d->subPixel);
}

KWin::Output::Transform OutputInterface::transform() const
{
    return d->transform;
}

void OutputInterface::setTransform(KWin::Output::Transform transform)
{
    if (d->transform == transform) {
        return;
    }
    d->transform = transform;
    d->broadcastGeometry();
    Q_EMIT transformChanged(d->transform);
}

QList<wl_resource *> OutputInterface::clientResources(ClientConnection *client) const
{
    return d->resourceMap.values(client->client());
}

void OutputInterface::done()
{
    for (wl_resource *resource : std::as_const(d->resourceMap)) {
        d->sendDone(resource);
    }
}

void OutputInterface::done(wl_client *client)
{
    d->sendDone(d->resourceMap.value(client));
}

OutputInterface *OutputInterface::get(wl_resource *native)
{
    if (auto outputPrivate = OutputInterfacePrivate::fromResource(native)) {
        return outputPrivate->q;
    }
    return nullptr;
}

Display *OutputInterface::display() const
{
    return d->display;
}

} // namespace KWaylandServer
