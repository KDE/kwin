/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_output.h"
#include "renderloop_p.h"
#include "softwarevsyncmonitor.h"

namespace KWin
{

VirtualOutput::VirtualOutput(QObject *parent)
    : AbstractWaylandOutput()
    , m_renderLoop(new RenderLoop(this))
    , m_vsyncMonitor(SoftwareVsyncMonitor::create(this))
{
    Q_UNUSED(parent);

    connect(m_vsyncMonitor, &VsyncMonitor::vblankOccurred, this, &VirtualOutput::vblank);

    static int identifier = -1;
    identifier++;
    setName("Virtual-" + QString::number(identifier));
}

VirtualOutput::~VirtualOutput()
{
}

RenderLoop *VirtualOutput::renderLoop() const
{
    return m_renderLoop;
}

SoftwareVsyncMonitor *VirtualOutput::vsyncMonitor() const
{
    return m_vsyncMonitor;
}

void VirtualOutput::init(const QPoint &logicalPosition, const QSize &pixelSize)
{
    const int refreshRate = 60000; // TODO: Make the refresh rate configurable.
    m_renderLoop->setRefreshRate(refreshRate);
    m_vsyncMonitor->setRefreshRate(refreshRate);

    KWaylandServer::OutputDeviceInterface::Mode mode;
    mode.id = 0;
    mode.size = pixelSize;
    mode.flags = KWaylandServer::OutputDeviceInterface::ModeFlag::Current;
    mode.refreshRate = refreshRate;
    initInterfaces("model_TODO", "manufacturer_TODO", "UUID_TODO", pixelSize, { mode }, {});
    setGeometry(QRect(logicalPosition, pixelSize));
}

void VirtualOutput::setGeometry(const QRect &geo)
{
    // TODO: set mode to have updated pixelSize
    setGlobalPos(geo.topLeft());
}

void VirtualOutput::vblank(std::chrono::nanoseconds timestamp)
{
    RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(m_renderLoop);
    renderLoopPrivate->notifyFrameCompleted(timestamp);
}

}
