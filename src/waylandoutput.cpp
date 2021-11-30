/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "waylandoutput.h"
#include "wayland_server.h"

#include <cmath>

namespace KWin
{

static KWaylandServer::OutputInterface::Transform kwinTransformToOutputTransform(AbstractWaylandOutput::Transform transform)
{
    switch (transform) {
    case AbstractWaylandOutput::Transform::Normal:
        return KWaylandServer::OutputInterface::Transform::Normal;
    case AbstractWaylandOutput::Transform::Rotated90:
        return KWaylandServer::OutputInterface::Transform::Rotated90;
    case AbstractWaylandOutput::Transform::Rotated180:
        return KWaylandServer::OutputInterface::Transform::Rotated180;
    case AbstractWaylandOutput::Transform::Rotated270:
        return KWaylandServer::OutputInterface::Transform::Rotated270;
    case AbstractWaylandOutput::Transform::Flipped:
        return KWaylandServer::OutputInterface::Transform::Flipped;
    case AbstractWaylandOutput::Transform::Flipped90:
        return KWaylandServer::OutputInterface::Transform::Flipped90;
    case AbstractWaylandOutput::Transform::Flipped180:
        return KWaylandServer::OutputInterface::Transform::Flipped180;
    case AbstractWaylandOutput::Transform::Flipped270:
        return KWaylandServer::OutputInterface::Transform::Flipped270;
    default:
        Q_UNREACHABLE();
    }
}

static KWaylandServer::OutputInterface::SubPixel kwinSubPixelToOutputSubPixel(AbstractWaylandOutput::SubPixel subPixel)
{
    switch (subPixel) {
    case AbstractWaylandOutput::SubPixel::Unknown:
        return KWaylandServer::OutputInterface::SubPixel::Unknown;
    case AbstractWaylandOutput::SubPixel::None:
        return KWaylandServer::OutputInterface::SubPixel::None;
    case AbstractWaylandOutput::SubPixel::Horizontal_RGB:
        return KWaylandServer::OutputInterface::SubPixel::HorizontalRGB;
    case AbstractWaylandOutput::SubPixel::Horizontal_BGR:
        return KWaylandServer::OutputInterface::SubPixel::HorizontalBGR;
    case AbstractWaylandOutput::SubPixel::Vertical_RGB:
        return KWaylandServer::OutputInterface::SubPixel::VerticalRGB;
    case AbstractWaylandOutput::SubPixel::Vertical_BGR:
        return KWaylandServer::OutputInterface::SubPixel::VerticalBGR;
    default:
        Q_UNREACHABLE();
    }
}

static KWaylandServer::OutputInterface::DpmsMode kwinDpmsModeToOutputDpmsMode(AbstractWaylandOutput::DpmsMode dpmsMode)
{
    switch (dpmsMode) {
    case AbstractWaylandOutput::DpmsMode::Off:
        return KWaylandServer::OutputInterface::DpmsMode::Off;
    case AbstractWaylandOutput::DpmsMode::On:
        return KWaylandServer::OutputInterface::DpmsMode::On;
    case AbstractWaylandOutput::DpmsMode::Standby:
        return KWaylandServer::OutputInterface::DpmsMode::Standby;
    case AbstractWaylandOutput::DpmsMode::Suspend:
        return KWaylandServer::OutputInterface::DpmsMode::Suspend;
    default:
        Q_UNREACHABLE();
    }
}

static AbstractWaylandOutput::DpmsMode outputDpmsModeToKWinDpmsMode(KWaylandServer::OutputInterface::DpmsMode dpmsMode)
{
    switch (dpmsMode) {
    case KWaylandServer::OutputInterface::DpmsMode::Off:
        return AbstractWaylandOutput::DpmsMode::Off;
    case KWaylandServer::OutputInterface::DpmsMode::On:
        return AbstractWaylandOutput::DpmsMode::On;
    case KWaylandServer::OutputInterface::DpmsMode::Standby:
        return AbstractWaylandOutput::DpmsMode::Standby;
    case KWaylandServer::OutputInterface::DpmsMode::Suspend:
        return AbstractWaylandOutput::DpmsMode::Suspend;
    default:
        Q_UNREACHABLE();
    }
}

WaylandOutput::WaylandOutput(AbstractWaylandOutput *output, QObject *parent)
    : QObject(parent)
    , m_platformOutput(output)
    , m_waylandOutput(new KWaylandServer::OutputInterface(waylandServer()->display()))
    , m_xdgOutputV1(waylandServer()->xdgOutputManagerV1()->createXdgOutput(m_waylandOutput.data(), m_waylandOutput.data()))
{
    const QRect geometry = m_platformOutput->geometry();

    m_waylandOutput->setTransform(kwinTransformToOutputTransform(output->transform()));
    m_waylandOutput->setManufacturer(output->manufacturer());
    m_waylandOutput->setModel(output->model());
    m_waylandOutput->setPhysicalSize(output->physicalSize());
    m_waylandOutput->setDpmsMode(kwinDpmsModeToOutputDpmsMode(output->dpmsMode()));
    m_waylandOutput->setDpmsSupported(output->capabilities() & AbstractWaylandOutput::Capability::Dpms);
    m_waylandOutput->setGlobalPosition(geometry.topLeft());
    m_waylandOutput->setScale(std::ceil(output->scale()));
    m_waylandOutput->setMode(output->modeSize(), output->refreshRate());
    m_waylandOutput->setSubPixel(kwinSubPixelToOutputSubPixel(output->subPixel()));

    m_xdgOutputV1->setName(output->name());
    m_xdgOutputV1->setDescription(output->description());
    m_xdgOutputV1->setLogicalPosition(geometry.topLeft());
    m_xdgOutputV1->setLogicalSize(geometry.size());

    m_waylandOutput->done();
    m_xdgOutputV1->done();

    // The dpms functionality is not part of the wl_output interface, but org_kde_kwin_dpms.
    connect(output, &AbstractWaylandOutput::dpmsModeChanged,
            this, &WaylandOutput::handleDpmsModeChanged);
    connect(m_waylandOutput.data(), &KWaylandServer::OutputInterface::dpmsModeRequested,
            this, &WaylandOutput::handleDpmsModeRequested);

    // The timer is used to compress output updates so the wayland clients are not spammed.
    m_updateTimer.setSingleShot(true);
    connect(&m_updateTimer, &QTimer::timeout, this, &WaylandOutput::update);

    connect(output, &AbstractWaylandOutput::currentModeChanged, this, &WaylandOutput::scheduleUpdate);
    connect(output, &AbstractWaylandOutput::geometryChanged, this, &WaylandOutput::scheduleUpdate);
    connect(output, &AbstractWaylandOutput::transformChanged, this, &WaylandOutput::scheduleUpdate);
    connect(output, &AbstractWaylandOutput::scaleChanged, this, &WaylandOutput::scheduleUpdate);
}

KWaylandServer::OutputInterface *WaylandOutput::waylandOutput() const
{
    return m_waylandOutput.data();
}

void WaylandOutput::scheduleUpdate()
{
    m_updateTimer.start();
}

void WaylandOutput::update()
{
    const QRect geometry = m_platformOutput->geometry();

    m_waylandOutput->setGlobalPosition(geometry.topLeft());
    m_waylandOutput->setScale(std::ceil(m_platformOutput->scale()));
    m_waylandOutput->setTransform(kwinTransformToOutputTransform(m_platformOutput->transform()));
    m_waylandOutput->setMode(m_platformOutput->modeSize(), m_platformOutput->refreshRate());

    m_xdgOutputV1->setLogicalPosition(geometry.topLeft());
    m_xdgOutputV1->setLogicalSize(geometry.size());

    m_waylandOutput->done();
    m_xdgOutputV1->done();
}

void WaylandOutput::handleDpmsModeChanged()
{
    m_waylandOutput->setDpmsMode(kwinDpmsModeToOutputDpmsMode(m_platformOutput->dpmsMode()));
}

void WaylandOutput::handleDpmsModeRequested(KWaylandServer::OutputInterface::DpmsMode dpmsMode)
{
    m_platformOutput->setDpmsMode(outputDpmsModeToKWinDpmsMode(dpmsMode));
}

} // namespace KWin
