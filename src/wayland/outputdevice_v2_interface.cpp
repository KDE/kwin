/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Méven Car <meven.car@enioka.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "outputdevice_v2_interface.h"

#include "display.h"
#include "display_p.h"
#include "utils.h"
#include "utils/common.h"

#include <QDebug>
#include <QPointer>
#include <QString>

#include <wayland-client-protocol.h>

#include "qwayland-server-kde-output-device-v2.h"

namespace KWaylandServer
{
static const quint32 s_version = 2;

class OutputDeviceV2InterfacePrivate : public QtWaylandServer::kde_output_device_v2
{
public:
    OutputDeviceV2InterfacePrivate(OutputDeviceV2Interface *q, Display *display);
    ~OutputDeviceV2InterfacePrivate() override;

    void updateGeometry();

    void sendGeometry(Resource *resource);
    wl_resource *sendNewMode(Resource *resource, OutputDeviceModeV2Interface *mode);
    void sendCurrentMode(Resource *resource);
    void sendDone(Resource *resource);
    void sendUuid(Resource *resource);
    void sendEdid(Resource *resource);
    void sendEnabled(Resource *resource);
    void sendScale(Resource *resource);
    void sendEisaId(Resource *resource);
    void sendName(Resource *resource);
    void sendSerialNumber(Resource *resource);
    void sendCapabilities(Resource *resource);
    void sendOverscan(Resource *resource);
    void sendVrrPolicy(Resource *resource);
    void sendRgbRange(Resource *resource);

    QSize physicalSize;
    QPoint globalPosition;
    QString manufacturer = QStringLiteral("org.kde.kwin");
    QString model = QStringLiteral("none");
    qreal scale = 1.0;
    QString serialNumber;
    QString eisaId;
    QString name;
    OutputDeviceV2Interface::SubPixel subPixel = OutputDeviceV2Interface::SubPixel::Unknown;
    OutputDeviceV2Interface::Transform transform = OutputDeviceV2Interface::Transform::Normal;

    QList<OutputDeviceModeV2Interface *> modes;
    OutputDeviceModeV2Interface *currentMode = nullptr;

    QByteArray edid;
    bool enabled = true;
    QUuid uuid;
    OutputDeviceV2Interface::Capabilities capabilities;
    uint32_t overscan = 0;
    OutputDeviceV2Interface::VrrPolicy vrrPolicy = OutputDeviceV2Interface::VrrPolicy::Automatic;
    OutputDeviceV2Interface::RgbRange rgbRange = OutputDeviceV2Interface::RgbRange::Automatic;

    QPointer<Display> display;
    OutputDeviceV2Interface *q;

private:
    int32_t toTransform() const;
    int32_t toSubPixel() const;

protected:
    void kde_output_device_v2_bind_resource(Resource *resource) override;
    void kde_output_device_v2_destroy_global() override;
};

class OutputDeviceModeV2InterfacePrivate : public QtWaylandServer::kde_output_device_mode_v2
{
public:
    struct ModeResource : Resource
    {
        OutputDeviceV2InterfacePrivate::Resource *output;
    };

    OutputDeviceModeV2InterfacePrivate(OutputDeviceModeV2Interface *q, const QSize &size, int refreshRate, OutputDeviceModeV2Interface::ModeFlags flags);
    ~OutputDeviceModeV2InterfacePrivate() override;

    Resource *createResource(OutputDeviceV2InterfacePrivate::Resource *output);
    Resource *findResource(OutputDeviceV2InterfacePrivate::Resource *output) const;

    void bindResource(wl_resource *resource);

    static OutputDeviceModeV2InterfacePrivate *get(OutputDeviceModeV2Interface *mode)
    {
        return mode->d.get();
    }

    OutputDeviceModeV2Interface *q;

    QSize m_size;
    int m_refreshRate = 60000;
    OutputDeviceModeV2Interface::ModeFlags m_flags;

protected:
    Resource *kde_output_device_mode_v2_allocate() override;
};

OutputDeviceV2InterfacePrivate::OutputDeviceV2InterfacePrivate(OutputDeviceV2Interface *q, Display *display)
    : QtWaylandServer::kde_output_device_v2(*display, s_version)
    , display(display)
    , q(q)
{
    DisplayPrivate *displayPrivate = DisplayPrivate::get(display);
    displayPrivate->outputdevicesV2.append(q);
}

OutputDeviceV2InterfacePrivate::~OutputDeviceV2InterfacePrivate()
{
    if (display) {
        DisplayPrivate *displayPrivate = DisplayPrivate::get(display);
        displayPrivate->outputdevicesV2.removeOne(q);
    }
}

OutputDeviceV2Interface::OutputDeviceV2Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new OutputDeviceV2InterfacePrivate(this, display))
{
}

OutputDeviceV2Interface::~OutputDeviceV2Interface()
{
    d->globalRemove();
}

void OutputDeviceV2Interface::remove()
{
    if (d->isGlobalRemoved()) {
        return;
    }

    if (d->display) {
        DisplayPrivate *displayPrivate = DisplayPrivate::get(d->display);
        displayPrivate->outputdevicesV2.removeOne(this);
    }

    d->globalRemove();
}

QSize OutputDeviceV2Interface::pixelSize() const
{
    if (d->currentMode == nullptr) {
        return QSize();
    }
    return d->currentMode->size();
}

int OutputDeviceV2Interface::refreshRate() const
{
    if (d->currentMode == nullptr) {
        return 60000;
    }
    return d->currentMode->refreshRate();
}

void OutputDeviceV2Interface::setCurrentMode(OutputDeviceModeV2Interface *mode)
{
    Q_ASSERT(d->modes.contains(mode));
    if (mode == d->currentMode) {
        return;
    }
    if (d->currentMode) {
        // another mode has the current flag - remove
        d->currentMode->setFlags(d->currentMode->flags() & ~uint(OutputDeviceModeV2Interface::ModeFlag::Current));
    }

    mode->setFlags(mode->flags() | OutputDeviceModeV2Interface::ModeFlag::Current);
    d->currentMode = mode;

    const auto clientResources = d->resourceMap();
    for (auto it = clientResources.begin(); it != clientResources.end(); ++it) {
        auto resource = *it;
        d->sendCurrentMode(resource);
        d->sendDone(resource);
    }
    d->updateGeometry();
}

bool OutputDeviceV2Interface::setCurrentMode(const QSize &size, int refreshRate)
{
    auto mode = std::find_if(d->modes.begin(), d->modes.end(), [size, refreshRate](OutputDeviceModeV2Interface *mode) {
        return mode->size() == size && mode->refreshRate() == refreshRate;
    });
    if (mode == d->modes.end()) {
        return false;
    }
    setCurrentMode(*mode);
    return true;
}

int32_t OutputDeviceV2InterfacePrivate::toTransform() const
{
    switch (transform) {
    case OutputDeviceV2Interface::Transform::Normal:
        return WL_OUTPUT_TRANSFORM_NORMAL;
    case OutputDeviceV2Interface::Transform::Rotated90:
        return WL_OUTPUT_TRANSFORM_90;
    case OutputDeviceV2Interface::Transform::Rotated180:
        return WL_OUTPUT_TRANSFORM_180;
    case OutputDeviceV2Interface::Transform::Rotated270:
        return WL_OUTPUT_TRANSFORM_270;
    case OutputDeviceV2Interface::Transform::Flipped:
        return WL_OUTPUT_TRANSFORM_FLIPPED;
    case OutputDeviceV2Interface::Transform::Flipped90:
        return WL_OUTPUT_TRANSFORM_FLIPPED_90;
    case OutputDeviceV2Interface::Transform::Flipped180:
        return WL_OUTPUT_TRANSFORM_FLIPPED_180;
    case OutputDeviceV2Interface::Transform::Flipped270:
        return WL_OUTPUT_TRANSFORM_FLIPPED_270;
    default:
        Q_UNREACHABLE();
    }
}

int32_t OutputDeviceV2InterfacePrivate::toSubPixel() const
{
    switch (subPixel) {
    case OutputDeviceV2Interface::SubPixel::Unknown:
        return WL_OUTPUT_SUBPIXEL_UNKNOWN;
    case OutputDeviceV2Interface::SubPixel::None:
        return WL_OUTPUT_SUBPIXEL_NONE;
    case OutputDeviceV2Interface::SubPixel::HorizontalRGB:
        return WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB;
    case OutputDeviceV2Interface::SubPixel::HorizontalBGR:
        return WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR;
    case OutputDeviceV2Interface::SubPixel::VerticalRGB:
        return WL_OUTPUT_SUBPIXEL_VERTICAL_RGB;
    case OutputDeviceV2Interface::SubPixel::VerticalBGR:
        return WL_OUTPUT_SUBPIXEL_VERTICAL_BGR;
    default:
        Q_UNREACHABLE();
    }
}

void OutputDeviceV2InterfacePrivate::kde_output_device_v2_destroy_global()
{
    delete q;
}

void OutputDeviceV2InterfacePrivate::kde_output_device_v2_bind_resource(Resource *resource)
{
    sendGeometry(resource);
    sendScale(resource);
    sendEisaId(resource);
    sendName(resource);
    sendSerialNumber(resource);

    for (OutputDeviceModeV2Interface *mode : std::as_const(modes)) {
        sendNewMode(resource, mode);
    }
    sendCurrentMode(resource);
    sendUuid(resource);
    sendEdid(resource);
    sendEnabled(resource);
    sendCapabilities(resource);
    sendOverscan(resource);
    sendVrrPolicy(resource);
    sendRgbRange(resource);
    sendDone(resource);
}

wl_resource *OutputDeviceV2InterfacePrivate::sendNewMode(Resource *resource, OutputDeviceModeV2Interface *mode)
{
    auto privateMode = OutputDeviceModeV2InterfacePrivate::get(mode);
    // bind mode to client
    const auto modeResource = privateMode->createResource(resource);

    send_mode(resource->handle, modeResource->handle);

    privateMode->bindResource(modeResource->handle);

    return modeResource->handle;
}

void OutputDeviceV2InterfacePrivate::sendCurrentMode(Resource *outputResource)
{
    const auto modeResource = OutputDeviceModeV2InterfacePrivate::get(currentMode)->findResource(outputResource);
    send_current_mode(outputResource->handle, modeResource->handle);
}

void OutputDeviceV2InterfacePrivate::sendGeometry(Resource *resource)
{
    send_geometry(resource->handle,
                  globalPosition.x(),
                  globalPosition.y(),
                  physicalSize.width(),
                  physicalSize.height(),
                  toSubPixel(),
                  manufacturer,
                  model,
                  toTransform());
}

void OutputDeviceV2InterfacePrivate::sendScale(Resource *resource)
{
    send_scale(resource->handle, wl_fixed_from_double(scale));
}

void OutputDeviceV2InterfacePrivate::sendSerialNumber(Resource *resource)
{
    send_serial_number(resource->handle, serialNumber);
}

void OutputDeviceV2InterfacePrivate::sendEisaId(Resource *resource)
{
    send_eisa_id(resource->handle, eisaId);
}

void OutputDeviceV2InterfacePrivate::sendName(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_NAME_SINCE_VERSION) {
        send_name(resource->handle, name);
    }
}

void OutputDeviceV2InterfacePrivate::sendDone(Resource *resource)
{
    send_done(resource->handle);
}

void OutputDeviceV2InterfacePrivate::updateGeometry()
{
    const auto clientResources = resourceMap();
    for (const auto &resource : clientResources) {
        sendGeometry(resource);
        sendDone(resource);
    }
}

void OutputDeviceV2Interface::setPhysicalSize(const QSize &arg)
{
    if (d->physicalSize == arg) {
        return;
    }
    d->physicalSize = arg;
}

void OutputDeviceV2Interface::setGlobalPosition(const QPoint &arg)
{
    if (d->globalPosition == arg) {
        return;
    }
    d->globalPosition = arg;
    d->updateGeometry();
}

void OutputDeviceV2Interface::setManufacturer(const QString &arg)
{
    if (d->manufacturer == arg) {
        return;
    }
    d->manufacturer = arg;
}

void OutputDeviceV2Interface::setModel(const QString &arg)
{
    if (d->model == arg) {
        return;
    }
    d->model = arg;
}

void OutputDeviceV2Interface::setSerialNumber(const QString &arg)
{
    if (d->serialNumber == arg) {
        return;
    }
    d->serialNumber = arg;
}

void OutputDeviceV2Interface::setEisaId(const QString &arg)
{
    if (d->eisaId == arg) {
        return;
    }
    d->eisaId = arg;
}

void OutputDeviceV2Interface::setName(const QString &arg)
{
    if (d->name == arg) {
        return;
    }
    d->name = arg;
}

void OutputDeviceV2Interface::setSubPixel(SubPixel arg)
{
    if (d->subPixel == arg) {
        return;
    }
    d->subPixel = arg;
    d->updateGeometry();
}

void OutputDeviceV2Interface::setTransform(Transform arg)
{
    if (d->transform == arg) {
        return;
    }
    d->transform = arg;
    d->updateGeometry();
}

void OutputDeviceV2Interface::setScale(qreal scale)
{
    if (qFuzzyCompare(d->scale, scale)) {
        return;
    }
    d->scale = scale;
    const auto clientResources = d->resourceMap();
    for (const auto &resource : clientResources) {
        d->sendScale(resource);
        d->sendDone(resource);
    }
}

QSize OutputDeviceV2Interface::physicalSize() const
{
    return d->physicalSize;
}

QPoint OutputDeviceV2Interface::globalPosition() const
{
    return d->globalPosition;
}

QString OutputDeviceV2Interface::manufacturer() const
{
    return d->manufacturer;
}

QString OutputDeviceV2Interface::model() const
{
    return d->model;
}

QString OutputDeviceV2Interface::serialNumber() const
{
    return d->serialNumber;
}

QString OutputDeviceV2Interface::eisaId() const
{
    return d->eisaId;
}

QString OutputDeviceV2Interface::name() const
{
    return d->name;
}

qreal OutputDeviceV2Interface::scale() const
{
    return d->scale;
}

OutputDeviceV2Interface::SubPixel OutputDeviceV2Interface::subPixel() const
{
    return d->subPixel;
}

OutputDeviceV2Interface::Transform OutputDeviceV2Interface::transform() const
{
    return d->transform;
}

void OutputDeviceV2Interface::setModes(const QList<OutputDeviceModeV2Interface *> &modes)
{
    if (modes.isEmpty()) {
        qCWarning(KWIN_CORE) << "Tried to set no modes for output";
        return;
    }

    const auto clientResources = d->resourceMap();

    const auto oldModes = d->modes;
    d->modes.clear();
    d->currentMode = nullptr;

    for (OutputDeviceModeV2Interface *outputDeviceMode : modes) {
        d->modes << outputDeviceMode;
        outputDeviceMode->setParent(this);

        for (auto resource : clientResources) {
            d->sendNewMode(resource, outputDeviceMode);
        }

        if (outputDeviceMode->flags().testFlag(OutputDeviceModeV2Interface::ModeFlag::Current)) {
            d->currentMode = outputDeviceMode;
        }
    }

    if (!d->currentMode) {
        d->currentMode = d->modes.at(0);
    }

    for (auto resource : clientResources) {
        d->sendCurrentMode(resource);
    }

    qDeleteAll(oldModes.crbegin(), oldModes.crend());

    for (auto resource : clientResources) {
        d->sendDone(resource);
    }
}

void OutputDeviceV2Interface::setEdid(const QByteArray &edid)
{
    d->edid = edid;
    const auto clientResources = d->resourceMap();
    for (const auto &resource : clientResources) {
        d->sendEdid(resource);
        d->sendDone(resource);
    }
}

QByteArray OutputDeviceV2Interface::edid() const
{
    return d->edid;
}

void OutputDeviceV2Interface::setEnabled(bool enabled)
{
    if (d->enabled != enabled) {
        d->enabled = enabled;
        const auto clientResources = d->resourceMap();
        for (const auto &resource : clientResources) {
            d->sendEnabled(resource);
            d->sendDone(resource);
        }
    }
}

bool OutputDeviceV2Interface::enabled() const
{
    return d->enabled;
}

void OutputDeviceV2Interface::setUuid(const QUuid &uuid)
{
    if (d->uuid != uuid) {
        d->uuid = uuid;
        const auto clientResources = d->resourceMap();
        for (const auto &resource : clientResources) {
            d->sendUuid(resource);
            d->sendDone(resource);
        }
    }
}

QUuid OutputDeviceV2Interface::uuid() const
{
    return d->uuid;
}

void OutputDeviceV2InterfacePrivate::sendEdid(Resource *resource)
{
    send_edid(resource->handle, QString::fromStdString(edid.toBase64().toStdString()));
}

void OutputDeviceV2InterfacePrivate::sendEnabled(Resource *resource)
{
    send_enabled(resource->handle, enabled);
}

void OutputDeviceV2InterfacePrivate::sendUuid(Resource *resource)
{
    send_uuid(resource->handle, uuid.toString(QUuid::WithoutBraces));
}

uint32_t OutputDeviceV2Interface::overscan() const
{
    return d->overscan;
}

OutputDeviceV2Interface::Capabilities OutputDeviceV2Interface::capabilities() const
{
    return d->capabilities;
}

void OutputDeviceV2Interface::setCapabilities(Capabilities cap)
{
    if (d->capabilities != cap) {
        d->capabilities = cap;
        const auto clientResources = d->resourceMap();
        for (const auto &resource : clientResources) {
            d->sendCapabilities(resource);
            d->sendDone(resource);
        }
    }
}

void OutputDeviceV2InterfacePrivate::sendCapabilities(Resource *resource)
{
    send_capabilities(resource->handle, static_cast<uint32_t>(capabilities));
}

void OutputDeviceV2Interface::setOverscan(uint32_t overscan)
{
    if (d->overscan != overscan) {
        d->overscan = overscan;
        const auto clientResources = d->resourceMap();
        for (const auto &resource : clientResources) {
            d->sendOverscan(resource);
            d->sendDone(resource);
        }
    }
}

void OutputDeviceV2InterfacePrivate::sendOverscan(Resource *resource)
{
    send_overscan(resource->handle, static_cast<uint32_t>(overscan));
}

void OutputDeviceV2InterfacePrivate::sendVrrPolicy(Resource *resource)
{
    send_vrr_policy(resource->handle, static_cast<uint32_t>(vrrPolicy));
}

OutputDeviceV2Interface::VrrPolicy OutputDeviceV2Interface::vrrPolicy() const
{
    return d->vrrPolicy;
}

void OutputDeviceV2Interface::setVrrPolicy(VrrPolicy policy)
{
    if (d->vrrPolicy != policy) {
        d->vrrPolicy = policy;
        const auto clientResources = d->resourceMap();
        for (const auto &resource : clientResources) {
            d->sendVrrPolicy(resource);
            d->sendDone(resource);
        }
    }
}

OutputDeviceV2Interface::RgbRange OutputDeviceV2Interface::rgbRange() const
{
    return d->rgbRange;
}

void OutputDeviceV2Interface::setRgbRange(RgbRange rgbRange)
{
    if (d->rgbRange != rgbRange) {
        d->rgbRange = rgbRange;
        const auto clientResources = d->resourceMap();
        for (const auto &resource : clientResources) {
            d->sendRgbRange(resource);
            d->sendDone(resource);
        }
    }
}

void OutputDeviceV2InterfacePrivate::sendRgbRange(Resource *resource)
{
    send_rgb_range(resource->handle, static_cast<uint32_t>(rgbRange));
}

wl_resource *OutputDeviceV2Interface::resource() const
{
    return d->resource()->handle;
}

OutputDeviceV2Interface *OutputDeviceV2Interface::get(wl_resource *native)
{
    if (auto devicePrivate = resource_cast<OutputDeviceV2InterfacePrivate *>(native)) {
        return devicePrivate->q;
    }
    return nullptr;
}

OutputDeviceModeV2InterfacePrivate::OutputDeviceModeV2InterfacePrivate(OutputDeviceModeV2Interface *q, const QSize &size, int refreshRate, OutputDeviceModeV2Interface::ModeFlags flags)
    : QtWaylandServer::kde_output_device_mode_v2()
    , q(q)
    , m_size(size)
    , m_refreshRate(refreshRate)
    , m_flags(flags)
{
}

OutputDeviceModeV2Interface::OutputDeviceModeV2Interface(const QSize &size, int refreshRate, ModeFlags flags, QObject *parent)
    : QObject(parent)
    , d(new OutputDeviceModeV2InterfacePrivate(this, size, refreshRate, flags))
{
}

OutputDeviceModeV2Interface::~OutputDeviceModeV2Interface() = default;

OutputDeviceModeV2InterfacePrivate::~OutputDeviceModeV2InterfacePrivate()
{
    const auto map = resourceMap();
    for (Resource *resource : map) {
        send_removed(resource->handle);
    }
}

OutputDeviceModeV2InterfacePrivate::Resource *OutputDeviceModeV2InterfacePrivate::createResource(OutputDeviceV2InterfacePrivate::Resource *output)
{
    const auto modeResource = static_cast<ModeResource *>(add(output->client(), output->version()));
    modeResource->output = output;
    return modeResource;
}

OutputDeviceModeV2InterfacePrivate::Resource *OutputDeviceModeV2InterfacePrivate::findResource(OutputDeviceV2InterfacePrivate::Resource *output) const
{
    const auto resources = resourceMap();
    for (const auto &resource : resources) {
        auto modeResource = static_cast<ModeResource *>(resource);
        if (modeResource->output == output) {
            return resource;
        }
    }
    return nullptr;
}

OutputDeviceModeV2InterfacePrivate::Resource *OutputDeviceModeV2InterfacePrivate::kde_output_device_mode_v2_allocate()
{
    return new ModeResource;
}

QSize OutputDeviceModeV2Interface::size() const
{
    return d->m_size;
}

int OutputDeviceModeV2Interface::refreshRate() const
{
    return d->m_refreshRate;
}

OutputDeviceModeV2Interface::ModeFlags OutputDeviceModeV2Interface::flags() const
{
    return d->m_flags;
}

void OutputDeviceModeV2Interface::setFlags(OutputDeviceModeV2Interface::ModeFlags flags)
{
    d->m_flags = flags;
}

void OutputDeviceModeV2InterfacePrivate::bindResource(wl_resource *resource)
{
    send_size(resource, m_size.width(), m_size.height());
    send_refresh(resource, m_refreshRate);

    if (m_flags.testFlag(OutputDeviceModeV2Interface::ModeFlag::Preferred)) {
        send_preferred(resource);
    }
}

OutputDeviceModeV2Interface *OutputDeviceModeV2Interface::get(wl_resource *native)
{
    if (auto devicePrivate = resource_cast<OutputDeviceModeV2InterfacePrivate *>(native)) {
        return devicePrivate->q;
    }
    return nullptr;
}

}
