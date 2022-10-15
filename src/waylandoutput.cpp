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
    , m_xdgOutputV1(waylandServer()->xdgOutputManagerV1()->createXdgOutput(m_waylandOutput.get(), m_waylandOutput.get()))
{
    const QRect geometry = m_platformOutput->geometry();
    m_xdgOutputV1->setName(output->name());
    m_xdgOutputV1->setDescription(output->description());
    m_xdgOutputV1->setLogicalPosition(geometry.topLeft());
    m_xdgOutputV1->setLogicalSize(geometry.size());

    m_xdgOutputV1->done();
    m_waylandOutput->scheduleDone();

    connect(output, &Output::geometryChanged, this, &WaylandOutput::update);
}

void WaylandOutput::update()
{
    const QRect geometry = m_platformOutput->geometry();

    m_xdgOutputV1->setLogicalPosition(geometry.topLeft());
    m_xdgOutputV1->setLogicalSize(geometry.size());

    m_xdgOutputV1->done();
    m_waylandOutput->scheduleDone();
}

} // namespace KWin
