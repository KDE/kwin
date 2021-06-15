/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2021 Méven Car <meven.car@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "waylandoutputdevicev2.h"
#include "wayland_server.h"
#include "KWaylandServer/outputdevicemode_v2_interface.h"

using namespace KWaylandServer;

namespace KWin
{

static KWaylandServer::OutputDeviceV2Interface::Transform kwinTransformToOutputDeviceTransform(AbstractWaylandOutput::Transform transform)
{
    return static_cast<KWaylandServer::OutputDeviceV2Interface::Transform>(transform);
}

static KWaylandServer::OutputDeviceV2Interface::SubPixel kwinSubPixelToOutputDeviceSubPixel(AbstractWaylandOutput::SubPixel subPixel)
{
    return static_cast<KWaylandServer::OutputDeviceV2Interface::SubPixel>(subPixel);
}

static KWaylandServer::OutputDeviceV2Interface::Capabilities kwinCapabilitiesToOutputDeviceCapabilities(AbstractWaylandOutput::Capabilities caps)
{
    KWaylandServer::OutputDeviceV2Interface::Capabilities ret;
    if (caps & AbstractWaylandOutput::Capability::Overscan) {
        ret |= KWaylandServer::OutputDeviceV2Interface::Capability::Overscan;
    }
    if (caps & AbstractWaylandOutput::Capability::Vrr) {
        ret |= KWaylandServer::OutputDeviceV2Interface::Capability::Vrr;
    }
    return ret;
}

static KWaylandServer::OutputDeviceV2Interface::VrrPolicy kwinVrrPolicyToOutputDeviceVrrPolicy(RenderLoop::VrrPolicy policy)
{
    return static_cast<KWaylandServer::OutputDeviceV2Interface::VrrPolicy>(policy);
}

WaylandOutputDevice::WaylandOutputDevice(AbstractWaylandOutput *output, QObject *parent)
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

    setModes(output);

    connect(output, &AbstractWaylandOutput::geometryChanged,
            this, &WaylandOutputDevice::handleGeometryChanged);
    connect(output, &AbstractWaylandOutput::scaleChanged,
            this, &WaylandOutputDevice::handleScaleChanged);
    connect(output, &AbstractWaylandOutput::enabledChanged,
            this, &WaylandOutputDevice::handleEnabledChanged);
    connect(output, &AbstractWaylandOutput::transformChanged,
            this, &WaylandOutputDevice::handleTransformChanged);
    connect(output, &AbstractWaylandOutput::currentModeChanged,
            this, &WaylandOutputDevice::handleModeChanged);
    connect(output, &AbstractWaylandOutput::capabilitiesChanged,
            this, &WaylandOutputDevice::handleCapabilitiesChanged);
    connect(output, &AbstractWaylandOutput::overscanChanged,
            this, &WaylandOutputDevice::handleOverscanChanged);
    connect(output, &AbstractWaylandOutput::vrrPolicyChanged,
            this, &WaylandOutputDevice::handleVrrPolicyChanged);
    connect(output, &AbstractWaylandOutput::modesChanged,
            this, &WaylandOutputDevice::handleModesChanged);
}

void WaylandOutputDevice::setModes(AbstractWaylandOutput *output)
{
    QList<OutputDeviceModeV2Interface *> deviceModes;

    const auto modes = output->modes();
    for (const AbstractWaylandOutput::Mode &mode : modes) {

        OutputDeviceModeV2Interface::ModeFlags flags;

        if (mode.flags & AbstractWaylandOutput::ModeFlag::Current) {
            flags |= OutputDeviceModeV2Interface::ModeFlag::Current;
        }
        if (mode.flags & AbstractWaylandOutput::ModeFlag::Preferred) {
            flags |= OutputDeviceModeV2Interface::ModeFlag::Preferred;
        }

        OutputDeviceModeV2Interface *deviceMode = new OutputDeviceModeV2Interface(waylandServer()->display(), mode.size, mode.refreshRate, flags );
        deviceModes << deviceMode;
    }
    m_outputDeviceV2->setModes(deviceModes);
}

void WaylandOutputDevice::handleModesChanged()
{
    setModes(m_platformOutput);
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

void WaylandOutputDevice::handleModeChanged()
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

} // namespace KWin
