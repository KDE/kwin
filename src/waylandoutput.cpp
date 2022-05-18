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
    , m_waylandOutput(new KWaylandServer::OutputInterface(waylandServer()->display(), output))
    , m_xdgOutputV1(waylandServer()->xdgOutputManagerV1()->createXdgOutput(m_waylandOutput.data(), m_waylandOutput.data()))
{
    const QRect geometry = m_platformOutput->geometry();

    m_xdgOutputV1->setName(output->name());
    m_xdgOutputV1->setDescription(output->description());
    m_xdgOutputV1->setLogicalPosition(geometry.topLeft());
    m_xdgOutputV1->setLogicalSize(geometry.size());

    m_waylandOutput->done();
    m_xdgOutputV1->done();

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
    return m_waylandOutput.data();
}

void WaylandOutput::scheduleUpdate()
{
    m_updateTimer.start();
}

void WaylandOutput::update()
{
    const QRect geometry = m_platformOutput->geometry();

    m_waylandOutput->update();

    m_xdgOutputV1->setLogicalPosition(geometry.topLeft());
    m_xdgOutputV1->setLogicalSize(geometry.size());

    m_waylandOutput->done();
    m_xdgOutputV1->done();
}

} // namespace KWin
