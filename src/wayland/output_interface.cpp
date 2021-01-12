/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "output_interface.h"
#include "display_p.h"
#include "global_p.h"
#include "display.h"

#include <QVector>

#include <wayland-server.h>

namespace KWaylandServer
{

class OutputInterface::Private : public Global::Private
{
public:
    struct ResourceData {
        wl_resource *resource;
        uint32_t version;
    };
    Private(OutputInterface *q, Display *d);
    ~Private();
    void sendMode(wl_resource *resource, const Mode &mode);
    void sendDone(const ResourceData &data);
    void updateGeometry();
    void updateScale();

    QSize physicalSize;
    QPoint globalPosition;
    QString manufacturer = QStringLiteral("org.kde.kwin");
    QString model = QStringLiteral("none");
    int scale = 1;
    SubPixel subPixel = SubPixel::Unknown;
    Transform transform = Transform::Normal;
    QList<Mode> modes;
    QList<ResourceData> resources;
    struct {
        DpmsMode mode = DpmsMode::Off;
        bool supported = false;
    } dpms;

    static OutputInterface *get(wl_resource *native);

private:
    static Private *cast(wl_resource *native);
    static void releaseCallback(wl_client *client, wl_resource *resource);
    static void unbind(wl_resource *resource);
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    int32_t toTransform() const;
    int32_t toSubPixel() const;
    void sendGeometry(wl_resource *resource);
    void sendScale(const ResourceData &data);

    OutputInterface *q;
    static QVector<Private*> s_privates;
    static const struct wl_output_interface s_interface;
    static const quint32 s_version;
};

QVector<OutputInterface::Private*> OutputInterface::Private::s_privates;
const quint32 OutputInterface::Private::s_version = 3;

OutputInterface::Private::Private(OutputInterface *q, Display *d)
    : Global::Private(d, &wl_output_interface, s_version)
    , q(q)
{
    DisplayPrivate *displayPrivate = DisplayPrivate::get(display);
    displayPrivate->outputs.append(q);

    s_privates << this;
}

OutputInterface::Private::~Private()
{
    if (display) {
        DisplayPrivate *displayPrivate = DisplayPrivate::get(display);
        displayPrivate->outputs.removeOne(q);
    }

    s_privates.removeAll(this);
}

#ifndef K_DOXYGEN
const struct wl_output_interface OutputInterface::Private::s_interface = {
    releaseCallback
};
#endif

void OutputInterface::Private::releaseCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client);
    unbind(resource);
}

OutputInterface *OutputInterface::Private::get(wl_resource *native)
{
    if (Private *p = cast(native)) {
        return p->q;
    }
    return nullptr;
}

OutputInterface::Private *OutputInterface::Private::cast(wl_resource *native)
{
    for (auto it = s_privates.constBegin(); it != s_privates.constEnd(); ++it) {
        const auto &resources = (*it)->resources;
        auto rit = std::find_if(resources.constBegin(), resources.constEnd(), [native] (const ResourceData &data) { return data.resource == native; });
        if (rit != resources.constEnd()) {
            return (*it);
        }
    }
    return nullptr;
}

OutputInterface::OutputInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
    Q_D();
    connect(this, &OutputInterface::currentModeChanged, this,
        [this] {
            Q_D();
            auto currentModeIt = std::find_if(d->modes.constBegin(), d->modes.constEnd(), [](const Mode &mode) { return mode.flags.testFlag(ModeFlag::Current); });
            if (currentModeIt == d->modes.constEnd()) {
                return;
            }
            for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
                d->sendMode((*it).resource, *currentModeIt);
                d->sendDone(*it);
            }
            wl_display_flush_clients(*(d->display));
        }
    );
    connect(this, &OutputInterface::subPixelChanged,       this, [d] { d->updateGeometry(); });
    connect(this, &OutputInterface::transformChanged,      this, [d] { d->updateGeometry(); });
    connect(this, &OutputInterface::globalPositionChanged, this, [d] { d->updateGeometry(); });
    connect(this, &OutputInterface::modelChanged,          this, [d] { d->updateGeometry(); });
    connect(this, &OutputInterface::manufacturerChanged,   this, [d] { d->updateGeometry(); });
    connect(this, &OutputInterface::scaleChanged,          this, [d] { d->updateScale(); });
}

OutputInterface::~OutputInterface() = default;

QSize OutputInterface::pixelSize() const
{
    Q_D();
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
    Q_D();
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
    Q_ASSERT(!isValid());
    Q_D();

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

    auto existingModeIt = std::find_if(d->modes.begin(), d->modes.end(),
        [size,refreshRate](const Mode &mode) {
            return mode.size == size && mode.refreshRate == refreshRate;
        }
    );
    auto emitChanges = [this,flags,size,refreshRate] {
        emit modesChanged();
        if (flags.testFlag(ModeFlag::Current)) {
            emit refreshRateChanged(refreshRate);
            emit pixelSizeChanged(size);
            emit currentModeChanged();
        }
    };
    if (existingModeIt != d->modes.end()) {
        if ((*existingModeIt).flags == flags) {
            // nothing to do
            return;
        }
        (*existingModeIt).flags = flags;
        emitChanges();
        return;
    }
    Mode mode;
    mode.size = size;
    mode.refreshRate = refreshRate;
    mode.flags = flags;
    d->modes << mode;
    emitChanges();
}

void OutputInterface::setCurrentMode(const QSize &size, int refreshRate)
{
    Q_D();
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
    emit modesChanged();
    emit refreshRateChanged((*existingModeIt).refreshRate);
    emit pixelSizeChanged((*existingModeIt).size);
    emit currentModeChanged();
}

int32_t OutputInterface::Private::toTransform() const
{
    switch (transform) {
    case Transform::Normal:
        return WL_OUTPUT_TRANSFORM_NORMAL;
    case Transform::Rotated90:
        return WL_OUTPUT_TRANSFORM_90;
    case Transform::Rotated180:
        return WL_OUTPUT_TRANSFORM_180;
    case Transform::Rotated270:
        return WL_OUTPUT_TRANSFORM_270;
    case Transform::Flipped:
        return WL_OUTPUT_TRANSFORM_FLIPPED;
    case Transform::Flipped90:
        return WL_OUTPUT_TRANSFORM_FLIPPED_90;
    case Transform::Flipped180:
        return WL_OUTPUT_TRANSFORM_FLIPPED_180;
    case Transform::Flipped270:
        return WL_OUTPUT_TRANSFORM_FLIPPED_270;
    }
    abort();
}

int32_t OutputInterface::Private::toSubPixel() const
{
    switch (subPixel) {
    case SubPixel::Unknown:
        return WL_OUTPUT_SUBPIXEL_UNKNOWN;
    case SubPixel::None:
        return WL_OUTPUT_SUBPIXEL_NONE;
    case SubPixel::HorizontalRGB:
        return WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB;
    case SubPixel::HorizontalBGR:
        return WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR;
    case SubPixel::VerticalRGB:
        return WL_OUTPUT_SUBPIXEL_VERTICAL_RGB;
    case SubPixel::VerticalBGR:
        return WL_OUTPUT_SUBPIXEL_VERTICAL_BGR;
    }
    abort();
}

void OutputInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&wl_output_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_user_data(resource, this);
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    ResourceData r;
    r.resource = resource;
    r.version = version;
    resources << r;

    sendGeometry(resource);
    sendScale(r);

    auto currentModeIt = modes.constEnd();
    for (auto it = modes.constBegin(); it != modes.constEnd(); ++it) {
        const Mode &mode = *it;
        if (mode.flags.testFlag(ModeFlag::Current)) {
            // needs to be sent as last mode
            currentModeIt = it;
            continue;
        }
        sendMode(resource, mode);
    }

    if (currentModeIt != modes.constEnd()) {
        sendMode(resource, *currentModeIt);
    }

    sendDone(r);
    c->flush();

    emit q->bound(display->getConnection(client), r.resource);
}

void OutputInterface::Private::unbind(wl_resource *resource)
{
    Private *o = cast(resource);
    if (!o) {
        return;
    }
    auto it = std::find_if(o->resources.begin(), o->resources.end(), [resource](const ResourceData &r) { return r.resource == resource; });
    if (it != o->resources.end()) {
        o->resources.erase(it);
    }
}

void OutputInterface::Private::sendMode(wl_resource *resource, const Mode &mode)
{
    int32_t flags = 0;
    if (mode.flags.testFlag(ModeFlag::Current)) {
        flags |= WL_OUTPUT_MODE_CURRENT;
    }
    if (mode.flags.testFlag(ModeFlag::Preferred)) {
        flags |= WL_OUTPUT_MODE_PREFERRED;
    }
    wl_output_send_mode(resource,
                        flags,
                        mode.size.width(),
                        mode.size.height(),
                        mode.refreshRate);

}

void OutputInterface::Private::sendGeometry(wl_resource *resource)
{
    wl_output_send_geometry(resource,
                            globalPosition.x(),
                            globalPosition.y(),
                            physicalSize.width(),
                            physicalSize.height(),
                            toSubPixel(),
                            qPrintable(manufacturer),
                            qPrintable(model),
                            toTransform());
}

void OutputInterface::Private::sendScale(const ResourceData &data)
{
    if (data.version < 2) {
        return;
    }
    wl_output_send_scale(data.resource, scale);
}

void OutputInterface::Private::sendDone(const ResourceData &data)
{
    if (data.version < 2) {
        return;
    }
    wl_output_send_done(data.resource);
}

void OutputInterface::Private::updateGeometry()
{
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        sendGeometry((*it).resource);
        sendDone(*it);
    }
}

void OutputInterface::Private::updateScale()
{
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        sendScale(*it);
        sendDone(*it);
    }
}

#define SETTER(setterName, type, argumentName) \
    void OutputInterface::setterName(type arg) \
    { \
        Q_D(); \
        if (d->argumentName == arg) { \
            return; \
        } \
        d->argumentName = arg; \
        emit argumentName##Changed(d->argumentName); \
    }

SETTER(setPhysicalSize, const QSize&, physicalSize)
SETTER(setGlobalPosition, const QPoint&, globalPosition)
SETTER(setManufacturer, const QString&, manufacturer)
SETTER(setModel, const QString&, model)
SETTER(setScale, int, scale)
SETTER(setSubPixel, SubPixel, subPixel)
SETTER(setTransform, Transform, transform)

#undef SETTER

QSize OutputInterface::physicalSize() const
{
    Q_D();
    return d->physicalSize;
}

QPoint OutputInterface::globalPosition() const
{
    Q_D();
    return d->globalPosition;
}

QString OutputInterface::manufacturer() const
{
    Q_D();
    return d->manufacturer;
}

QString OutputInterface::model() const
{
    Q_D();
    return d->model;
}

int OutputInterface::scale() const
{
    Q_D();
    return d->scale;
}

OutputInterface::SubPixel OutputInterface::subPixel() const
{
    Q_D();
    return d->subPixel;
}

OutputInterface::Transform OutputInterface::transform() const
{
    Q_D();
    return d->transform;
}

QList< OutputInterface::Mode > OutputInterface::modes() const
{
    Q_D();
    return d->modes;
}

void OutputInterface::setDpmsMode(OutputInterface::DpmsMode mode)
{
    Q_D();
    if (d->dpms.mode == mode) {
        return;
    }
    d->dpms.mode = mode;
    emit dpmsModeChanged();
}

void OutputInterface::setDpmsSupported(bool supported)
{
    Q_D();
    if (d->dpms.supported == supported) {
        return;
    }
    d->dpms.supported = supported;
    emit dpmsSupportedChanged();
}

OutputInterface::DpmsMode OutputInterface::dpmsMode() const
{
    Q_D();
    return d->dpms.mode;
}

bool OutputInterface::isDpmsSupported() const
{
    Q_D();
    return d->dpms.supported;
}

QVector<wl_resource *> OutputInterface::clientResources(ClientConnection *client) const
{
    Q_D();
    QVector<wl_resource *> ret;
    for (auto it = d->resources.constBegin(), end = d->resources.constEnd(); it != end; ++it) {
        if (wl_resource_get_client((*it).resource) == client->client()) {
            ret << (*it).resource;
        }
    }
    return ret;
}

bool OutputInterface::isEnabled() const
{
    Q_D();
    if (!d->dpms.supported) {
        return true;
    }
    return d->dpms.mode == DpmsMode::On;
}

OutputInterface *OutputInterface::get(wl_resource* native)
{
    return Private::get(native);
}

OutputInterface::Private *OutputInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
