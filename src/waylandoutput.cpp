/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "waylandoutput.h"
#include "wayland_server.h"

#include <cmath>

namespace KWin
{

WaylandOutput::WaylandOutput(Output *output, QObject *parent)
    : QObject(parent)
    , m_platformOutput(output)
    , m_waylandOutput(new KWaylandServer::OutputInterface(waylandServer()->display()))
    , m_xdgOutputV1(waylandServer()->xdgOutputManagerV1()->createXdgOutput(m_waylandOutput.get(), m_waylandOutput.get()))
{
    const QRect geometry = m_platformOutput->geometry();

    m_waylandOutput->setTransform(output->transform());
    m_waylandOutput->setManufacturer(output->manufacturer());
    m_waylandOutput->setModel(output->model());
    m_waylandOutput->setPhysicalSize(output->physicalSize());
    m_waylandOutput->setDpmsMode(output->dpmsMode());
    m_waylandOutput->setDpmsSupported(output->capabilities() & Output::Capability::Dpms);
    m_waylandOutput->setGlobalPosition(geometry.topLeft());
    m_waylandOutput->setScale(std::ceil(output->scale()));
    m_waylandOutput->setMode(output->modeSize(), output->refreshRate());
    m_waylandOutput->setSubPixel(output->subPixel());

    m_xdgOutputV1->setName(output->name());
    m_xdgOutputV1->setDescription(output->description());
    m_xdgOutputV1->setLogicalPosition(geometry.topLeft());
    m_xdgOutputV1->setLogicalSize(geometry.size());

    m_waylandOutput->done();
    m_xdgOutputV1->done();

    // The dpms functionality is not part of the wl_output interface, but org_kde_kwin_dpms.
    connect(output, &Output::dpmsModeChanged,
            this, &WaylandOutput::handleDpmsModeChanged);
    connect(m_waylandOutput.get(), &KWaylandServer::OutputInterface::dpmsModeRequested,
            this, &WaylandOutput::handleDpmsModeRequested);

    // The timer is used to compress output updates so the wayland clients are not spammed.
    m_updateTimer.setSingleShot(true);
    connect(&m_updateTimer, &QTimer::timeout, this, &WaylandOutput::update);

    connect(output, &Output::currentModeChanged, this, &WaylandOutput::scheduleUpdate);
    connect(output, &Output::geometryChanged, this, &WaylandOutput::scheduleUpdate);
    connect(output, &Output::transformChanged, this, &WaylandOutput::scheduleUpdate);
    connect(output, &Output::scaleChanged, this, &WaylandOutput::scheduleUpdate);
}

KWaylandServer::OutputInterface *WaylandOutput::waylandOutput() const
{
    return m_waylandOutput.get();
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
    m_waylandOutput->setTransform(m_platformOutput->transform());
    m_waylandOutput->setMode(m_platformOutput->modeSize(), m_platformOutput->refreshRate());

    m_xdgOutputV1->setLogicalPosition(geometry.topLeft());
    m_xdgOutputV1->setLogicalSize(geometry.size());

    m_waylandOutput->done();
    m_xdgOutputV1->done();
}

void WaylandOutput::handleDpmsModeChanged()
{
    m_waylandOutput->setDpmsMode(m_platformOutput->dpmsMode());
}

void WaylandOutput::handleDpmsModeRequested(KWin::Output::DpmsMode dpmsMode)
{
    m_platformOutput->setDpmsMode(dpmsMode);
}

} // namespace KWin
