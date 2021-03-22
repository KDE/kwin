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
    void sendMode(Resource *resource, const OutputInterface::Mode &mode);
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
    QList<OutputInterface::Mode> modes;
    struct {
        OutputInterface::DpmsMode mode = OutputInterface::DpmsMode::Off;
        bool supported = false;
    } dpms;

private:
    void output_bind_resource(Resource *resource);
    void output_release(Resource *resource);
};

OutputInterfacePrivate::OutputInterfacePrivate(Display *display, OutputInterface *q)
    : QtWaylandServer::wl_output(*display, s_version)
    , q(q)
    , display(display)
{
}

void OutputInterfacePrivate::sendMode(Resource *resource, const OutputInterface::Mode &mode)
{
    quint32 flags = 0;

    if (mode.flags & OutputInterface::ModeFlag::Current) {
        flags |= mode_current;
    }
    if (mode.flags & OutputInterface::ModeFlag::Preferred) {
        flags |= mode_preferred;
    }

    send_mode(resource->handle, flags, mode.size.width(), mode.size.height(), mode.refreshRate);
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

void OutputInterfacePrivate::output_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void OutputInterfacePrivate::output_bind_resource(Resource *resource)
{
    auto currentModeIt = modes.constEnd();
    for (auto it = modes.constBegin(); it != modes.constEnd(); ++it) {
        const OutputInterface::Mode &mode = *it;
        if (mode.flags.testFlag(OutputInterface::ModeFlag::Current)) {
            // needs to be sent as last mode
            currentModeIt = it;
            continue;
        }
        sendMode(resource, mode);
    }

    if (currentModeIt != modes.constEnd()) {
        sendMode(resource, *currentModeIt);
    }

    sendScale(resource);
    sendGeometry(resource);
    sendDone(resource);

    emit q->bound(display->getConnection(resource->client()), resource->handle);
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
    emit aboutToBeDestroyed();

    if (d->display) {
        DisplayPrivate *displayPrivate = DisplayPrivate::get(d->display);
        displayPrivate->outputs.removeOne(this);
    }

    d->globalRemove();
}

QSize OutputInterface::pixelSize() const
{
    auto it = std::find_if(d->modes.constBegin(), d->modes.constEnd(),
        [](const Mode &mode) {
            return mode.flags.testFlag(ModeFlag::Current);
        }
    );
    if (it == d->modes.constEnd()) {
        return QSize();
    }
    return (*it).size;
}

int OutputInterface::refreshRate() const
{
    auto it = std::find_if(d->modes.constBegin(), d->modes.constEnd(),
        [](const Mode &mode) {
            return mode.flags.testFlag(ModeFlag::Current);
        }
    );
    if (it == d->modes.constEnd()) {
        return 60000;
    }
    return (*it).refreshRate;
}

void OutputInterface::addMode(const QSize &size, OutputInterface::ModeFlags flags, int refreshRate)
{
    auto currentModeIt = std::find_if(d->modes.begin(), d->modes.end(),
        [](const Mode &mode) {
            return mode.flags.testFlag(ModeFlag::Current);
        }
    );
    if (currentModeIt == d->modes.end() && !flags.testFlag(ModeFlag::Current)) {
        // no mode with current flag - enforce
        flags |= ModeFlag::Current;
    }
    if (currentModeIt != d->modes.end() && flags.testFlag(ModeFlag::Current)) {
        // another mode has the current flag - remove
        (*currentModeIt).flags &= ~uint(ModeFlag::Current);
    }

    if (flags.testFlag(ModeFlag::Preferred)) {
        // remove from existing Preferred mode
        auto preferredIt = std::find_if(d->modes.begin(), d->modes.end(),
            [](const Mode &mode) {
                return mode.flags.testFlag(ModeFlag::Preferred);
            }
        );
        if (preferredIt != d->modes.end()) {
            (*preferredIt).flags &= ~uint(ModeFlag::Preferred);
        }
    }

    auto modeIt = std::find_if(d->modes.begin(), d->modes.end(),
        [size,refreshRate](const Mode &mode) {
            return mode.size == size && mode.refreshRate == refreshRate;
        }
    );

    if (modeIt != d->modes.end()) {
        if ((*modeIt).flags == flags) {
            // nothing to do
            return;
        }
        (*modeIt).flags = flags;
    } else {
        Mode mode;
        mode.size = size;
        mode.refreshRate = refreshRate;
        mode.flags = flags;
        modeIt = d->modes.insert(d->modes.end(), mode);
    }

    const auto outputResources = d->resourceMap();
    for (OutputInterfacePrivate::Resource *resource : outputResources) {
        d->sendMode(resource, *modeIt);
    }

    emit modesChanged();
    if (flags.testFlag(ModeFlag::Current)) {
        emit refreshRateChanged(refreshRate);
        emit pixelSizeChanged(size);
        emit currentModeChanged();
    }
}

void OutputInterface::setCurrentMode(const QSize &size, int refreshRate)
{
    auto currentModeIt = std::find_if(d->modes.begin(), d->modes.end(),
        [](const Mode &mode) {
            return mode.flags.testFlag(ModeFlag::Current);
        }
    );
    if (currentModeIt != d->modes.end()) {
        // another mode has the current flag - remove
        (*currentModeIt).flags &= ~uint(ModeFlag::Current);
    }

    auto existingModeIt = std::find_if(d->modes.begin(), d->modes.end(),
        [size,refreshRate](const Mode &mode) {
            return mode.size == size && mode.refreshRate == refreshRate;
        }
    );

    Q_ASSERT(existingModeIt != d->modes.end());
    (*existingModeIt).flags |= ModeFlag::Current;

    const auto outputResources = d->resourceMap();
    for (OutputInterfacePrivate::Resource *resource : outputResources) {
        d->sendMode(resource, *existingModeIt);
    }

    emit modesChanged();
    emit refreshRateChanged((*existingModeIt).refreshRate);
    emit pixelSizeChanged((*existingModeIt).size);
    emit currentModeChanged();
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
    emit physicalSizeChanged(d->physicalSize);
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
    emit globalPositionChanged(d->globalPosition);
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
    emit manufacturerChanged(d->manufacturer);
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
    emit modelChanged(d->model);
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

    emit scaleChanged(d->scale);
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
    emit subPixelChanged(d->subPixel);
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
    emit transformChanged(d->transform);
}

QList< OutputInterface::Mode > OutputInterface::modes() const
{
    return d->modes;
}

void OutputInterface::setDpmsMode(OutputInterface::DpmsMode mode)
{
    if (d->dpms.mode == mode) {
        return;
    }
    d->dpms.mode = mode;
    emit dpmsModeChanged();
}

void OutputInterface::setDpmsSupported(bool supported)
{
    if (d->dpms.supported == supported) {
        return;
    }
    d->dpms.supported = supported;
    emit dpmsSupportedChanged();
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
