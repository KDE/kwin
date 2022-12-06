/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_output.h"
#include "virtual_backend.h"

#include "core/renderloop_p.h"
#include "softwarevsyncmonitor.h"

namespace KWin
{

VirtualOutput::VirtualOutput(VirtualBackend *parent)
    : Output(parent)
    , m_backend(parent)
    , m_renderLoop(std::make_unique<RenderLoop>())
    , m_vsyncMonitor(SoftwareVsyncMonitor::create())
{
    connect(m_vsyncMonitor.get(), &VsyncMonitor::vblankOccurred, this, &VirtualOutput::vblank);

    static int identifier = -1;
    m_identifier = ++identifier;
    setInformation(Information{
        .name = QStringLiteral("Virtual-%1").arg(identifier),
    });
}

VirtualOutput::~VirtualOutput()
{
}

RenderLoop *VirtualOutput::renderLoop() const
{
    return m_renderLoop.get();
}

SoftwareVsyncMonitor *VirtualOutput::vsyncMonitor() const
{
    return m_vsyncMonitor.get();
}

void VirtualOutput::init(const QPoint &logicalPosition, const QSize &pixelSize, qreal scale)
{
    const int refreshRate = 60000; // TODO: Make the refresh rate configurable.
    m_renderLoop->setRefreshRate(refreshRate);
    m_vsyncMonitor->setRefreshRate(refreshRate);

    auto mode = std::make_shared<OutputMode>(pixelSize, m_vsyncMonitor->refreshRate());

    setState(State{
        .position = logicalPosition,
        .scale = scale,
        .modes = {mode},
        .currentMode = mode,
    });
}

void VirtualOutput::updateEnabled(bool enabled)
{
    State next = m_state;
    next.enabled = enabled;
    setState(next);
}

void VirtualOutput::vblank(std::chrono::nanoseconds timestamp)
{
    RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(m_renderLoop.get());
    renderLoopPrivate->notifyFrameCompleted(timestamp);
}

}
