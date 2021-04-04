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

} // namespace KWin
