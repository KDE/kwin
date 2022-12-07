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

#include "core/output.h"

#include <QDebug>
#include <QPointer>
#include <QString>

#include "qwayland-server-kde-output-device-v2.h"

using namespace KWin;

namespace KWaylandServer
{

static const quint32 s_version = 2;

static QtWaylandServer::kde_output_device_v2::transform kwinTransformToOutputDeviceTransform(Output::Transform transform)
{
    return static_cast<QtWaylandServer::kde_output_device_v2::transform>(transform);
}

static QtWaylandServer::kde_output_device_v2::subpixel kwinSubPixelToOutputDeviceSubPixel(Output::SubPixel subPixel)
{
    return static_cast<QtWaylandServer::kde_output_device_v2::subpixel>(subPixel);
}

static uint32_t kwinCapabilitiesToOutputDeviceCapabilities(Output::Capabilities caps)
{
    uint32_t ret = 0;
    if (caps & Output::Capability::Overscan) {
        ret |= QtWaylandServer::kde_output_device_v2::capability_overscan;
    }
    if (caps & Output::Capability::Vrr) {
        ret |= QtWaylandServer::kde_output_device_v2::capability_vrr;
    }
    if (caps & Output::Capability::RgbRange) {
        ret |= QtWaylandServer::kde_output_device_v2::capability_rgb_range;
    }
    return ret;
}

static QtWaylandServer::kde_output_device_v2::vrr_policy kwinVrrPolicyToOutputDeviceVrrPolicy(RenderLoop::VrrPolicy policy)
{
    return static_cast<QtWaylandServer::kde_output_device_v2::vrr_policy>(policy);
}

static QtWaylandServer::kde_output_device_v2::rgb_range kwinRgbRangeToOutputDeviceRgbRange(Output::RgbRange range)
{
    return static_cast<QtWaylandServer::kde_output_device_v2::rgb_range>(range);
}

class OutputDeviceV2InterfacePrivate : public QtWaylandServer::kde_output_device_v2
{
public:
    OutputDeviceV2InterfacePrivate(OutputDeviceV2Interface *q, Display *display, KWin::Output *handle);
    ~OutputDeviceV2InterfacePrivate() override;

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

    OutputDeviceV2Interface *q;
    QPointer<Display> m_display;
    KWin::Output *m_handle;
    QSize m_physicalSize;
    QPoint m_globalPosition;
    QString m_manufacturer = QStringLiteral("org.kde.kwin");
    QString m_model = QStringLiteral("none");
    qreal m_scale = 1.0;
    QString m_serialNumber;
    QString m_eisaId;
    QString m_name;
    subpixel m_subPixel = subpixel_unknown;
    transform m_transform = transform_normal;
    QList<OutputDeviceModeV2Interface *> m_modes;
    OutputDeviceModeV2Interface *m_currentMode = nullptr;
    QByteArray m_edid;
    bool m_enabled = true;
    QUuid m_uuid;
    uint32_t m_capabilities = 0;
    uint32_t m_overscan = 0;
    vrr_policy m_vrrPolicy = vrr_policy_automatic;
    rgb_range m_rgbRange = rgb_range_automatic;

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

    OutputDeviceModeV2InterfacePrivate(OutputDeviceModeV2Interface *q, std::shared_ptr<KWin::OutputMode> handle);
    ~OutputDeviceModeV2InterfacePrivate() override;

    Resource *createResource(OutputDeviceV2InterfacePrivate::Resource *output);
    Resource *findResource(OutputDeviceV2InterfacePrivate::Resource *output) const;

    void bindResource(wl_resource *resource);

    static OutputDeviceModeV2InterfacePrivate *get(OutputDeviceModeV2Interface *mode)
    {
        return mode->d.get();
    }

    OutputDeviceModeV2Interface *q;
    std::weak_ptr<KWin::OutputMode> m_handle;
    QSize m_size;
    int m_refreshRate = 60000;
    bool m_preferred = false;

protected:
    Resource *kde_output_device_mode_v2_allocate() override;
};

OutputDeviceV2InterfacePrivate::OutputDeviceV2InterfacePrivate(OutputDeviceV2Interface *q, Display *display, KWin::Output *handle)
    : QtWaylandServer::kde_output_device_v2(*display, s_version)
    , q(q)
    , m_display(display)
    , m_handle(handle)
{
    DisplayPrivate *displayPrivate = DisplayPrivate::get(display);
    displayPrivate->outputdevicesV2.append(q);
}

OutputDeviceV2InterfacePrivate::~OutputDeviceV2InterfacePrivate()
{
    if (m_display) {
        DisplayPrivate *displayPrivate = DisplayPrivate::get(m_display);
        displayPrivate->outputdevicesV2.removeOne(q);
    }
}

OutputDeviceV2Interface::OutputDeviceV2Interface(Display *display, KWin::Output *handle, QObject *parent)
    : QObject(parent)
    , d(new OutputDeviceV2InterfacePrivate(this, display, handle))
{
    updateEnabled();
    updateManufacturer();
    updateEdid();
    updateUuid();
    updateModel();
    updatePhysicalSize();
    updateGlobalPosition();
    updateScale();
    updateTransform();
    updateEisaId();
    updateSerialNumber();
    updateSubPixel();
    updateOverscan();
    updateCapabilities();
    updateVrrPolicy();
    updateRgbRange();
    updateName();
    updateModes();

    connect(handle, &Output::geometryChanged,
            this, &OutputDeviceV2Interface::updateGlobalPosition);
    connect(handle, &Output::scaleChanged,
            this, &OutputDeviceV2Interface::updateScale);
    connect(handle, &Output::enabledChanged,
            this, &OutputDeviceV2Interface::updateEnabled);
    connect(handle, &Output::transformChanged,
            this, &OutputDeviceV2Interface::updateTransform);
    connect(handle, &Output::currentModeChanged,
            this, &OutputDeviceV2Interface::updateCurrentMode);
    connect(handle, &Output::capabilitiesChanged,
            this, &OutputDeviceV2Interface::updateCapabilities);
    connect(handle, &Output::overscanChanged,
            this, &OutputDeviceV2Interface::updateOverscan);
    connect(handle, &Output::vrrPolicyChanged,
            this, &OutputDeviceV2Interface::updateVrrPolicy);
    connect(handle, &Output::modesChanged,
            this, &OutputDeviceV2Interface::updateModes);
    connect(handle, &Output::rgbRangeChanged,
            this, &OutputDeviceV2Interface::updateRgbRange);
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

    if (d->m_display) {
        DisplayPrivate *displayPrivate = DisplayPrivate::get(d->m_display);
        displayPrivate->outputdevicesV2.removeOne(this);
    }

    d->globalRemove();
}

KWin::Output *OutputDeviceV2Interface::handle() const
{
    return d->m_handle;
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

    for (OutputDeviceModeV2Interface *mode : std::as_const(m_modes)) {
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
    const auto modeResource = OutputDeviceModeV2InterfacePrivate::get(m_currentMode)->findResource(outputResource);
    send_current_mode(outputResource->handle, modeResource->handle);
}

void OutputDeviceV2InterfacePrivate::sendGeometry(Resource *resource)
{
    send_geometry(resource->handle,
                  m_globalPosition.x(),
                  m_globalPosition.y(),
                  m_physicalSize.width(),
                  m_physicalSize.height(),
                  m_subPixel,
                  m_manufacturer,
                  m_model,
                  m_transform);
}

void OutputDeviceV2InterfacePrivate::sendScale(Resource *resource)
{
    send_scale(resource->handle, wl_fixed_from_double(m_scale));
}

void OutputDeviceV2InterfacePrivate::sendSerialNumber(Resource *resource)
{
    send_serial_number(resource->handle, m_serialNumber);
}

void OutputDeviceV2InterfacePrivate::sendEisaId(Resource *resource)
{
    send_eisa_id(resource->handle, m_eisaId);
}

void OutputDeviceV2InterfacePrivate::sendName(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_NAME_SINCE_VERSION) {
        send_name(resource->handle, m_name);
    }
}

void OutputDeviceV2InterfacePrivate::sendDone(Resource *resource)
{
    send_done(resource->handle);
}

void OutputDeviceV2InterfacePrivate::sendEdid(Resource *resource)
{
    send_edid(resource->handle, QString::fromStdString(m_edid.toBase64().toStdString()));
}

void OutputDeviceV2InterfacePrivate::sendEnabled(Resource *resource)
{
    send_enabled(resource->handle, m_enabled);
}

void OutputDeviceV2InterfacePrivate::sendUuid(Resource *resource)
{
    send_uuid(resource->handle, m_uuid.toString(QUuid::WithoutBraces));
}

void OutputDeviceV2InterfacePrivate::sendCapabilities(Resource *resource)
{
    send_capabilities(resource->handle, m_capabilities);
}

void OutputDeviceV2InterfacePrivate::sendOverscan(Resource *resource)
{
    send_overscan(resource->handle, m_overscan);
}

void OutputDeviceV2InterfacePrivate::sendVrrPolicy(Resource *resource)
{
    send_vrr_policy(resource->handle, m_vrrPolicy);
}

void OutputDeviceV2InterfacePrivate::sendRgbRange(Resource *resource)
{
    send_rgb_range(resource->handle, m_rgbRange);
}

void OutputDeviceV2Interface::updateGeometry()
{
    const auto clientResources = d->resourceMap();
    for (const auto &resource : clientResources) {
        d->sendGeometry(resource);
        d->sendDone(resource);
    }
}

void OutputDeviceV2Interface::updatePhysicalSize()
{
    d->m_physicalSize = d->m_handle->physicalSize();
}

void OutputDeviceV2Interface::updateGlobalPosition()
{
    const QPoint arg = d->m_handle->geometry().topLeft();
    if (d->m_globalPosition == arg) {
        return;
    }
    d->m_globalPosition = arg;
    updateGeometry();
}

void OutputDeviceV2Interface::updateManufacturer()
{
    d->m_manufacturer = d->m_handle->manufacturer();
}

void OutputDeviceV2Interface::updateModel()
{
    d->m_model = d->m_handle->model();
}

void OutputDeviceV2Interface::updateSerialNumber()
{
    d->m_serialNumber = d->m_handle->serialNumber();
}

void OutputDeviceV2Interface::updateEisaId()
{
    d->m_eisaId = d->m_handle->eisaId();
}

void OutputDeviceV2Interface::updateName()
{
    d->m_name = d->m_handle->name();
}

void OutputDeviceV2Interface::updateSubPixel()
{
    const auto arg = kwinSubPixelToOutputDeviceSubPixel(d->m_handle->subPixel());
    if (d->m_subPixel != arg) {
        d->m_subPixel = arg;
        updateGeometry();
    }
}

void OutputDeviceV2Interface::updateTransform()
{
    const auto arg = kwinTransformToOutputDeviceTransform(d->m_handle->transform());
    if (d->m_transform != arg) {
        d->m_transform = arg;
        updateGeometry();
    }
}

void OutputDeviceV2Interface::updateScale()
{
    const qreal scale = d->m_handle->scale();
    if (qFuzzyCompare(d->m_scale, scale)) {
        return;
    }
    d->m_scale = scale;
    const auto clientResources = d->resourceMap();
    for (const auto &resource : clientResources) {
        d->sendScale(resource);
        d->sendDone(resource);
    }
}

void OutputDeviceV2Interface::updateModes()
{
    const auto oldModes = d->m_modes;
    d->m_modes.clear();
    d->m_currentMode = nullptr;

    const auto clientResources = d->resourceMap();
    const auto nativeModes = d->m_handle->modes();

    for (const std::shared_ptr<OutputMode> &mode : nativeModes) {
        OutputDeviceModeV2Interface *deviceMode = new OutputDeviceModeV2Interface(mode, this);
        d->m_modes.append(deviceMode);

        if (d->m_handle->currentMode() == mode) {
            d->m_currentMode = deviceMode;
        }

        for (auto resource : clientResources) {
            d->sendNewMode(resource, deviceMode);
        }
    }

    for (auto resource : clientResources) {
        d->sendCurrentMode(resource);
    }

    qDeleteAll(oldModes.crbegin(), oldModes.crend());

    for (auto resource : clientResources) {
        d->sendDone(resource);
    }
}

void OutputDeviceV2Interface::updateCurrentMode()
{
    for (OutputDeviceModeV2Interface *mode : std::as_const(d->m_modes)) {
        if (mode->handle().lock() == d->m_handle->currentMode()) {
            if (d->m_currentMode != mode) {
                d->m_currentMode = mode;
                const auto clientResources = d->resourceMap();
                for (auto resource : clientResources) {
                    d->sendCurrentMode(resource);
                    d->sendDone(resource);
                }
                updateGeometry();
            }
            return;
        }
    }
}

void OutputDeviceV2Interface::updateEdid()
{
    d->m_edid = d->m_handle->edid();
    const auto clientResources = d->resourceMap();
    for (const auto &resource : clientResources) {
        d->sendEdid(resource);
        d->sendDone(resource);
    }
}

void OutputDeviceV2Interface::updateEnabled()
{
    bool enabled = d->m_handle->isEnabled();
    if (d->m_enabled != enabled) {
        d->m_enabled = enabled;
        const auto clientResources = d->resourceMap();
        for (const auto &resource : clientResources) {
            d->sendEnabled(resource);
            d->sendDone(resource);
        }
    }
}

void OutputDeviceV2Interface::updateUuid()
{
    const QUuid uuid = d->m_handle->uuid();
    if (d->m_uuid != uuid) {
        d->m_uuid = uuid;
        const auto clientResources = d->resourceMap();
        for (const auto &resource : clientResources) {
            d->sendUuid(resource);
            d->sendDone(resource);
        }
    }
}

void OutputDeviceV2Interface::updateCapabilities()
{
    const uint32_t cap = kwinCapabilitiesToOutputDeviceCapabilities(d->m_handle->capabilities());
    if (d->m_capabilities != cap) {
        d->m_capabilities = cap;
        const auto clientResources = d->resourceMap();
        for (const auto &resource : clientResources) {
            d->sendCapabilities(resource);
            d->sendDone(resource);
        }
    }
}

void OutputDeviceV2Interface::updateOverscan()
{
    const uint32_t overscan = d->m_handle->overscan();
    if (d->m_overscan != overscan) {
        d->m_overscan = overscan;
        const auto clientResources = d->resourceMap();
        for (const auto &resource : clientResources) {
            d->sendOverscan(resource);
            d->sendDone(resource);
        }
    }
}

void OutputDeviceV2Interface::updateVrrPolicy()
{
    const auto policy = kwinVrrPolicyToOutputDeviceVrrPolicy(d->m_handle->vrrPolicy());
    if (d->m_vrrPolicy != policy) {
        d->m_vrrPolicy = policy;
        const auto clientResources = d->resourceMap();
        for (const auto &resource : clientResources) {
            d->sendVrrPolicy(resource);
            d->sendDone(resource);
        }
    }
}

void OutputDeviceV2Interface::updateRgbRange()
{
    const auto rgbRange = kwinRgbRangeToOutputDeviceRgbRange(d->m_handle->rgbRange());
    if (d->m_rgbRange != rgbRange) {
        d->m_rgbRange = rgbRange;
        const auto clientResources = d->resourceMap();
        for (const auto &resource : clientResources) {
            d->sendRgbRange(resource);
            d->sendDone(resource);
        }
    }
}

OutputDeviceV2Interface *OutputDeviceV2Interface::get(wl_resource *native)
{
    if (auto devicePrivate = resource_cast<OutputDeviceV2InterfacePrivate *>(native); devicePrivate && !devicePrivate->isGlobalRemoved()) {
        return devicePrivate->q;
    }
    return nullptr;
}

OutputDeviceModeV2InterfacePrivate::OutputDeviceModeV2InterfacePrivate(OutputDeviceModeV2Interface *q, std::shared_ptr<KWin::OutputMode> handle)
    : QtWaylandServer::kde_output_device_mode_v2()
    , q(q)
    , m_handle(handle)
    , m_size(handle->size())
    , m_refreshRate(handle->refreshRate())
    , m_preferred(handle->flags() & OutputMode::Flag::Preferred)
{
}

OutputDeviceModeV2Interface::OutputDeviceModeV2Interface(std::shared_ptr<KWin::OutputMode> handle, QObject *parent)
    : QObject(parent)
    , d(new OutputDeviceModeV2InterfacePrivate(this, handle))
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

std::weak_ptr<KWin::OutputMode> OutputDeviceModeV2Interface::handle() const
{
    return d->m_handle;
}

void OutputDeviceModeV2InterfacePrivate::bindResource(wl_resource *resource)
{
    send_size(resource, m_size.width(), m_size.height());
    send_refresh(resource, m_refreshRate);

    if (m_preferred) {
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
