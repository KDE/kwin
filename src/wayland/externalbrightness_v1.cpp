/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 */
#include "externalbrightness_v1.h"
#include "core/output.h"
#include "display.h"

namespace KWin
{

static constexpr uint32_t s_version = 1;

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

ExternalBrightnessDeviceV1::ExternalBrightnessDeviceV1(ExternalBrightnessV1 *global, wl_client *client, uint32_t version, uint32_t id)
    : QtWaylandServer::kde_external_brightness_device_v1(client, version, id)
    , m_global(global)
{
}

ExternalBrightnessDeviceV1::~ExternalBrightnessDeviceV1()
{
    if (m_global) {
        Q_EMIT m_global->deviceRemoved(this);
    }
}

void ExternalBrightnessDeviceV1::setBrightness(double brightness)
{
    uint32_t val = std::clamp<int64_t>(std::round(brightness * m_maxBrightness), 0, m_maxBrightness);
    if (val != m_currentBrightness) {
        m_currentBrightness = val;
        send_change_brightness_to(m_currentBrightness);
    }
}

bool ExternalBrightnessDeviceV1::isInternal() const
{
    return m_internal;
}

QByteArray ExternalBrightnessDeviceV1::edidBeginning() const
{
    return m_edidBeginning;
}

void ExternalBrightnessDeviceV1::kde_external_brightness_device_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void ExternalBrightnessDeviceV1::kde_external_brightness_device_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ExternalBrightnessDeviceV1::kde_external_brightness_device_v1_add_internal(Resource *resource, uint32_t internal)
{
    m_internal = internal == 1;
}

void ExternalBrightnessDeviceV1::kde_external_brightness_device_v1_add_edid(Resource *resource, const QString &edid)
{
    m_edidBeginning = QByteArray::fromBase64(edid.toUtf8());
}

void ExternalBrightnessDeviceV1::kde_external_brightness_device_v1_notify_max_brightness(Resource *resource, uint32_t value)
{
    m_maxBrightness = value;
}

void ExternalBrightnessDeviceV1::kde_external_brightness_device_v1_setup_done(Resource *resource)
{
    if (!m_done) {
        m_done = true;
        if (m_global) {
            Q_EMIT m_global->deviceAdded(this);
        }
    }
}

void ExternalBrightnessDeviceV1::kde_external_brightness_device_v1_notify_brightness(Resource *resource, uint32_t value)
{
    m_currentBrightness = value;
}

}
