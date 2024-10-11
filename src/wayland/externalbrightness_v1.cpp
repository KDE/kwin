/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 */
#include "externalbrightness_v1.h"
#include "core/output.h"
#include "display.h"

namespace KWin
{

static constexpr uint32_t s_version = 2;

ExternalBrightnessV1::ExternalBrightnessV1(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::kde_external_brightness_v1(*display, s_version)
{
}

void ExternalBrightnessV1::kde_external_brightness_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ExternalBrightnessV1::kde_external_brightness_v1_create_brightness_control(Resource *resource, uint32_t id)
{
    new ExternalBrightnessDeviceV1(this, resource->client(), id, resource->version());
}

void ExternalBrightnessV1::addDevice(ExternalBrightnessDeviceV1 *device)
{
    m_devices.push_back(device);
    Q_EMIT devicesChanged();
}

void ExternalBrightnessV1::removeDevice(ExternalBrightnessDeviceV1 *device)
{
    m_devices.removeOne(device);
    Q_EMIT devicesChanged();
}

QList<BrightnessDevice *> ExternalBrightnessV1::devices() const
{
    return m_devices;
}

ExternalBrightnessDeviceV1::ExternalBrightnessDeviceV1(ExternalBrightnessV1 *global, wl_client *client, uint32_t id, uint32_t version)
    : QtWaylandServer::kde_external_brightness_device_v1(client, id, version)
    , m_global(global)
{
}

ExternalBrightnessDeviceV1::~ExternalBrightnessDeviceV1()
{
    if (m_global) {
        m_global->removeDevice(this);
    }
}

void ExternalBrightnessDeviceV1::setBrightness(double brightness)
{
    m_observedBrightness.reset();

    const uint32_t minBrightness = m_internal ? 1 : 0; // some laptop screens turn off at brightness 0
    const uint32_t val = std::round(std::lerp(minBrightness, m_maxBrightness, std::clamp(brightness, 0.0, 1.0)));
    send_requested_brightness(val);
}

std::optional<double> ExternalBrightnessDeviceV1::observedBrightness() const
{
    std::optional<double> fractional = std::nullopt;
    if (m_observedBrightness.has_value()) {
        const uint32_t minBrightness = m_internal ? 1 : 0; // some laptop screens turn off at brightness 0
        fractional = std::clamp((*m_observedBrightness - minBrightness) / static_cast<double>(m_maxBrightness - minBrightness), 0.0, 1.0);
    }
    return fractional;
}

bool ExternalBrightnessDeviceV1::isInternal() const
{
    return m_internal;
}

QByteArray ExternalBrightnessDeviceV1::edidBeginning() const
{
    return m_edidBeginning;
}

int ExternalBrightnessDeviceV1::brightnessSteps() const
{
    return m_maxBrightness - (m_internal ? 1 : 0);
}

void ExternalBrightnessDeviceV1::kde_external_brightness_device_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void ExternalBrightnessDeviceV1::kde_external_brightness_device_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ExternalBrightnessDeviceV1::kde_external_brightness_device_v1_set_internal(Resource *resource, uint32_t internal)
{
    m_internal = internal == 1;
}

void ExternalBrightnessDeviceV1::kde_external_brightness_device_v1_set_edid(Resource *resource, const QString &edid)
{
    m_edidBeginning = QByteArray::fromBase64(edid.toUtf8());
}

void ExternalBrightnessDeviceV1::kde_external_brightness_device_v1_set_max_brightness(Resource *resource, uint32_t value)
{
    m_maxBrightness = value;
}

void ExternalBrightnessDeviceV1::kde_external_brightness_device_v1_set_observed_brightness(Resource *resource, uint32_t value)
{
    m_observedBrightness = value;
}

void ExternalBrightnessDeviceV1::kde_external_brightness_device_v1_commit(Resource *resource)
{
    if (!m_done) {
        m_done = true;
        if (m_global) {
            m_global->addDevice(this);
        }
    }
}
}
