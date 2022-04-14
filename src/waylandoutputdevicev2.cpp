/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2021 Méven Car <meven.car@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "waylandoutputdevicev2.h"
#include "wayland_server.h"

using namespace KWaylandServer;

namespace KWin
{

static KWaylandServer::OutputDeviceV2Interface::Transform kwinTransformToOutputDeviceTransform(AbstractOutput::Transform transform)
{
    return static_cast<KWaylandServer::OutputDeviceV2Interface::Transform>(transform);
}

static KWaylandServer::OutputDeviceV2Interface::SubPixel kwinSubPixelToOutputDeviceSubPixel(AbstractOutput::SubPixel subPixel)
{
    return static_cast<KWaylandServer::OutputDeviceV2Interface::SubPixel>(subPixel);
}

static KWaylandServer::OutputDeviceV2Interface::Capabilities kwinCapabilitiesToOutputDeviceCapabilities(AbstractOutput::Capabilities caps)
{
    KWaylandServer::OutputDeviceV2Interface::Capabilities ret;
    if (caps & AbstractOutput::Capability::Overscan) {
        ret |= KWaylandServer::OutputDeviceV2Interface::Capability::Overscan;
    }
    if (caps & AbstractOutput::Capability::Vrr) {
        ret |= KWaylandServer::OutputDeviceV2Interface::Capability::Vrr;
    }
    if (caps & AbstractOutput::Capability::RgbRange) {
        ret |= KWaylandServer::OutputDeviceV2Interface::Capability::RgbRange;
    }
    return ret;
}

static KWaylandServer::OutputDeviceV2Interface::VrrPolicy kwinVrrPolicyToOutputDeviceVrrPolicy(RenderLoop::VrrPolicy policy)
{
    return static_cast<KWaylandServer::OutputDeviceV2Interface::VrrPolicy>(policy);
}

static KWaylandServer::OutputDeviceV2Interface::RgbRange kwinRgbRangeToOutputDeviceRgbRange(AbstractOutput::RgbRange range)
{
    return static_cast<KWaylandServer::OutputDeviceV2Interface::RgbRange>(range);
}

WaylandOutputDevice::WaylandOutputDevice(AbstractOutput *output, QObject *parent)
    : QObject(parent)
    , m_platformOutput(output)
    , m_outputDeviceV2(new KWaylandServer::OutputDeviceV2Interface(waylandServer()->display()))
{
    m_outputDeviceV2->setManufacturer(output->manufacturer());
    m_outputDeviceV2->setEdid(output->edid());
    m_outputDeviceV2->setUuid(output->uuid());
    m_outputDeviceV2->setModel(output->model());
    m_outputDeviceV2->setPhysicalSize(output->physicalSize());
    m_outputDeviceV2->setGlobalPosition(output->geometry().topLeft());
    m_outputDeviceV2->setScale(output->scale());
    m_outputDeviceV2->setTransform(kwinTransformToOutputDeviceTransform(output->transform()));
    m_outputDeviceV2->setEisaId(output->eisaId());
    m_outputDeviceV2->setSerialNumber(output->serialNumber());
    m_outputDeviceV2->setSubPixel(kwinSubPixelToOutputDeviceSubPixel(output->subPixel()));
    m_outputDeviceV2->setOverscan(output->overscan());
    m_outputDeviceV2->setCapabilities(kwinCapabilitiesToOutputDeviceCapabilities(output->capabilities()));
    m_outputDeviceV2->setVrrPolicy(kwinVrrPolicyToOutputDeviceVrrPolicy(output->vrrPolicy()));
    m_outputDeviceV2->setRgbRange(kwinRgbRangeToOutputDeviceRgbRange(output->rgbRange()));
    m_outputDeviceV2->setName(output->name());

    updateModes(output);

    connect(output, &AbstractOutput::geometryChanged,
            this, &WaylandOutputDevice::handleGeometryChanged);
    connect(output, &AbstractOutput::scaleChanged,
            this, &WaylandOutputDevice::handleScaleChanged);
    connect(output, &AbstractOutput::enabledChanged,
            this, &WaylandOutputDevice::handleEnabledChanged);
    connect(output, &AbstractOutput::transformChanged,
            this, &WaylandOutputDevice::handleTransformChanged);
    connect(output, &AbstractOutput::currentModeChanged,
            this, &WaylandOutputDevice::handleCurrentModeChanged);
    connect(output, &AbstractOutput::capabilitiesChanged,
            this, &WaylandOutputDevice::handleCapabilitiesChanged);
    connect(output, &AbstractOutput::overscanChanged,
            this, &WaylandOutputDevice::handleOverscanChanged);
    connect(output, &AbstractOutput::vrrPolicyChanged,
            this, &WaylandOutputDevice::handleVrrPolicyChanged);
    connect(output, &AbstractOutput::modesChanged,
            this, &WaylandOutputDevice::handleModesChanged);
    connect(output, &AbstractOutput::rgbRangeChanged,
            this, &WaylandOutputDevice::handleRgbRangeChanged);
}

void WaylandOutputDevice::updateModes(AbstractOutput *output)
{
    QList<OutputDeviceModeV2Interface *> deviceModes;

    const auto modes = output->modes();
    deviceModes.reserve(modes.size());
    for (const AbstractOutput::Mode &mode : modes) {
        OutputDeviceModeV2Interface::ModeFlags flags;

        if (mode.flags & AbstractOutput::ModeFlag::Current) {
            flags |= OutputDeviceModeV2Interface::ModeFlag::Current;
        }
        if (mode.flags & AbstractOutput::ModeFlag::Preferred) {
            flags |= OutputDeviceModeV2Interface::ModeFlag::Preferred;
        }

        OutputDeviceModeV2Interface *deviceMode = new OutputDeviceModeV2Interface(mode.size, mode.refreshRate, flags);
        deviceModes << deviceMode;
    }
    m_outputDeviceV2->setModes(deviceModes);
}

void WaylandOutputDevice::handleModesChanged()
{
    updateModes(m_platformOutput);
}

void WaylandOutputDevice::handleGeometryChanged()
{
    m_outputDeviceV2->setGlobalPosition(m_platformOutput->geometry().topLeft());
}

void WaylandOutputDevice::handleScaleChanged()
{
    m_outputDeviceV2->setScale(m_platformOutput->scale());
}

void WaylandOutputDevice::handleEnabledChanged()
{
    m_outputDeviceV2->setEnabled(m_platformOutput->isEnabled());
}

void WaylandOutputDevice::handleTransformChanged()
{
    m_outputDeviceV2->setTransform(kwinTransformToOutputDeviceTransform(m_platformOutput->transform()));
}

void WaylandOutputDevice::handleCurrentModeChanged()
{
    m_outputDeviceV2->setCurrentMode(m_platformOutput->modeSize(), m_platformOutput->refreshRate());
}

void WaylandOutputDevice::handleCapabilitiesChanged()
{
    m_outputDeviceV2->setCapabilities(kwinCapabilitiesToOutputDeviceCapabilities(m_platformOutput->capabilities()));
}

void WaylandOutputDevice::handleOverscanChanged()
{
    m_outputDeviceV2->setOverscan(m_platformOutput->overscan());
}

void WaylandOutputDevice::handleVrrPolicyChanged()
{
    m_outputDeviceV2->setVrrPolicy(kwinVrrPolicyToOutputDeviceVrrPolicy(m_platformOutput->vrrPolicy()));
}

void WaylandOutputDevice::handleRgbRangeChanged()
{
    m_outputDeviceV2->setRgbRange(kwinRgbRangeToOutputDeviceRgbRange(m_platformOutput->rgbRange()));
}

} // namespace KWin
