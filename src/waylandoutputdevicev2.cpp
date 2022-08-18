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

static KWaylandServer::OutputDeviceV2Interface::Transform kwinTransformToOutputDeviceTransform(Output::Transform transform)
{
    return static_cast<KWaylandServer::OutputDeviceV2Interface::Transform>(transform);
}

static KWaylandServer::OutputDeviceV2Interface::SubPixel kwinSubPixelToOutputDeviceSubPixel(Output::SubPixel subPixel)
{
    return static_cast<KWaylandServer::OutputDeviceV2Interface::SubPixel>(subPixel);
}

static KWaylandServer::OutputDeviceV2Interface::Capabilities kwinCapabilitiesToOutputDeviceCapabilities(Output::Capabilities caps)
{
    KWaylandServer::OutputDeviceV2Interface::Capabilities ret;
    if (caps & Output::Capability::Overscan) {
        ret |= KWaylandServer::OutputDeviceV2Interface::Capability::Overscan;
    }
    if (caps & Output::Capability::Vrr) {
        ret |= KWaylandServer::OutputDeviceV2Interface::Capability::Vrr;
    }
    if (caps & Output::Capability::RgbRange) {
        ret |= KWaylandServer::OutputDeviceV2Interface::Capability::RgbRange;
    }
    return ret;
}

static KWaylandServer::OutputDeviceV2Interface::VrrPolicy kwinVrrPolicyToOutputDeviceVrrPolicy(RenderLoop::VrrPolicy policy)
{
    return static_cast<KWaylandServer::OutputDeviceV2Interface::VrrPolicy>(policy);
}

static KWaylandServer::OutputDeviceV2Interface::RgbRange kwinRgbRangeToOutputDeviceRgbRange(Output::RgbRange range)
{
    return static_cast<KWaylandServer::OutputDeviceV2Interface::RgbRange>(range);
}

WaylandOutputDevice::WaylandOutputDevice(Output *output, QObject *parent)
    : QObject(parent)
    , m_platformOutput(output)
    , m_outputDeviceV2(new KWaylandServer::OutputDeviceV2Interface(waylandServer()->display(), output))
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

    connect(output, &Output::geometryChanged,
            this, &WaylandOutputDevice::handleGeometryChanged);
    connect(output, &Output::scaleChanged,
            this, &WaylandOutputDevice::handleScaleChanged);
    connect(output, &Output::enabledChanged,
            this, &WaylandOutputDevice::handleEnabledChanged);
    connect(output, &Output::transformChanged,
            this, &WaylandOutputDevice::handleTransformChanged);
    connect(output, &Output::currentModeChanged,
            this, &WaylandOutputDevice::handleCurrentModeChanged);
    connect(output, &Output::capabilitiesChanged,
            this, &WaylandOutputDevice::handleCapabilitiesChanged);
    connect(output, &Output::overscanChanged,
            this, &WaylandOutputDevice::handleOverscanChanged);
    connect(output, &Output::vrrPolicyChanged,
            this, &WaylandOutputDevice::handleVrrPolicyChanged);
    connect(output, &Output::modesChanged,
            this, &WaylandOutputDevice::handleModesChanged);
    connect(output, &Output::rgbRangeChanged,
            this, &WaylandOutputDevice::handleRgbRangeChanged);
}

void WaylandOutputDevice::updateModes(Output *output)
{
    QList<OutputDeviceModeV2Interface *> deviceModes;
    OutputDeviceModeV2Interface *currentMode = nullptr;

    const auto modes = output->modes();
    deviceModes.reserve(modes.size());
    for (const std::shared_ptr<OutputMode> &mode : modes) {
        OutputDeviceModeV2Interface::ModeFlags flags;
        if (mode->flags() & OutputMode::Flag::Preferred) {
            flags |= OutputDeviceModeV2Interface::ModeFlag::Preferred;
        }

        OutputDeviceModeV2Interface *deviceMode = new OutputDeviceModeV2Interface(mode, mode->size(), mode->refreshRate(), flags);
        deviceModes << deviceMode;

        if (output->currentMode() == mode) {
            currentMode = deviceMode;
        }
    }
    m_outputDeviceV2->setModes(deviceModes, currentMode);
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
    const auto modes = m_outputDeviceV2->modes();
    for (KWaylandServer::OutputDeviceModeV2Interface *mode : modes) {
        if (mode->handle().lock() == m_platformOutput->currentMode()) {
            m_outputDeviceV2->setCurrentMode(mode);
            break;
        }
    }
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
