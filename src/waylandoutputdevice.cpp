/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "waylandoutputdevice.h"
#include "wayland_server.h"

namespace KWin
{

static KWaylandServer::OutputDeviceInterface::Transform kwinTransformToOutputDeviceTransform(AbstractWaylandOutput::Transform transform)
{
    return static_cast<KWaylandServer::OutputDeviceInterface::Transform>(transform);
}

static KWaylandServer::OutputDeviceInterface::SubPixel kwinSubPixelToOutputDeviceSubPixel(AbstractWaylandOutput::SubPixel subPixel)
{
    return static_cast<KWaylandServer::OutputDeviceInterface::SubPixel>(subPixel);
}

static KWaylandServer::OutputDeviceInterface::Capabilities kwinCapabilitiesToOutputDeviceCapabilities(AbstractWaylandOutput::Capabilities caps)
{
    KWaylandServer::OutputDeviceInterface::Capabilities ret;
    if (caps & AbstractWaylandOutput::Capability::Overscan) {
        ret |= KWaylandServer::OutputDeviceInterface::Capability::Overscan;
    }
    return ret;
}

WaylandOutputDevice::WaylandOutputDevice(AbstractWaylandOutput *output, QObject *parent)
    : QObject(parent)
    , m_platformOutput(output)
    , m_outputDevice(new KWaylandServer::OutputDeviceInterface(waylandServer()->display(), this))
{
    m_outputDevice->setManufacturer(output->manufacturer());
    m_outputDevice->setEdid(output->edid());
    m_outputDevice->setUuid(output->uuid());
    m_outputDevice->setModel(output->model());
    m_outputDevice->setPhysicalSize(output->physicalSize());
    m_outputDevice->setGlobalPosition(output->geometry().topLeft());
    m_outputDevice->setScaleF(output->scale());
    m_outputDevice->setTransform(kwinTransformToOutputDeviceTransform(output->transform()));
    m_outputDevice->setEisaId(output->eisaId());
    m_outputDevice->setSerialNumber(output->serialNumber());
    m_outputDevice->setSubPixel(kwinSubPixelToOutputDeviceSubPixel(output->subPixel()));
    m_outputDevice->setOverscan(output->overscan());
    m_outputDevice->setCapabilities(kwinCapabilitiesToOutputDeviceCapabilities(output->capabilities()));

    const auto modes = output->modes();
    for (const AbstractWaylandOutput::Mode &mode : modes) {
        KWaylandServer::OutputDeviceInterface::Mode deviceMode;
        deviceMode.size = mode.size;
        deviceMode.refreshRate = mode.refreshRate;
        deviceMode.id = mode.id;

        if (mode.flags & AbstractWaylandOutput::ModeFlag::Current) {
            deviceMode.flags |= KWaylandServer::OutputDeviceInterface::ModeFlag::Current;
        }
        if (mode.flags & AbstractWaylandOutput::ModeFlag::Preferred) {
            deviceMode.flags |= KWaylandServer::OutputDeviceInterface::ModeFlag::Preferred;
        }

        m_outputDevice->addMode(deviceMode);
    }

    connect(output, &AbstractWaylandOutput::geometryChanged,
            this, &WaylandOutputDevice::handleGeometryChanged);
    connect(output, &AbstractWaylandOutput::scaleChanged,
            this, &WaylandOutputDevice::handleScaleChanged);
    connect(output, &AbstractWaylandOutput::enabledChanged,
            this, &WaylandOutputDevice::handleEnabledChanged);
    connect(output, &AbstractWaylandOutput::transformChanged,
            this, &WaylandOutputDevice::handleTransformChanged);
    connect(output, &AbstractWaylandOutput::modeChanged,
            this, &WaylandOutputDevice::handleModeChanged);
    connect(output, &AbstractWaylandOutput::capabilitiesChanged,
            this, &WaylandOutputDevice::handleCapabilitiesChanged);
    connect(output, &AbstractWaylandOutput::overscanChanged,
            this, &WaylandOutputDevice::handleOverscanChanged);
}

void WaylandOutputDevice::handleGeometryChanged()
{
    m_outputDevice->setGlobalPosition(m_platformOutput->geometry().topLeft());
}

void WaylandOutputDevice::handleScaleChanged()
{
    m_outputDevice->setScaleF(m_platformOutput->scale());
}

void WaylandOutputDevice::handleEnabledChanged()
{
    if (m_platformOutput->isEnabled()) {
        m_outputDevice->setEnabled(KWaylandServer::OutputDeviceInterface::Enablement::Enabled);
    } else {
        m_outputDevice->setEnabled(KWaylandServer::OutputDeviceInterface::Enablement::Disabled);
    }
}

void WaylandOutputDevice::handleTransformChanged()
{
    m_outputDevice->setTransform(kwinTransformToOutputDeviceTransform(m_platformOutput->transform()));
}

void WaylandOutputDevice::handleModeChanged()
{
    m_outputDevice->setCurrentMode(m_platformOutput->modeSize(), m_platformOutput->refreshRate());
}

void WaylandOutputDevice::handleCapabilitiesChanged()
{
    m_outputDevice->setCapabilities(kwinCapabilitiesToOutputDeviceCapabilities(m_platformOutput->capabilities()));
}

void WaylandOutputDevice::handleOverscanChanged()
{
    m_outputDevice->setOverscan(m_platformOutput->overscan());
}

} // namespace KWin
