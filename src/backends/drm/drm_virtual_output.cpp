/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_virtual_output.h"

#include "core/renderloop_p.h"
#include "drm_backend.h"
#include "drm_gpu.h"
#include "drm_layer.h"
#include "drm_logging.h"
#include "drm_render_backend.h"
#include "softwarevsyncmonitor.h"

namespace KWin
{

DrmVirtualOutput::DrmVirtualOutput(const QString &name, DrmGpu *gpu, const QSize &size, qreal scale)
    : DrmAbstractOutput(gpu)
    , m_vsyncMonitor(SoftwareVsyncMonitor::create())
{
    connect(m_vsyncMonitor.get(), &VsyncMonitor::vblankOccurred, this, &DrmVirtualOutput::vblank);

    auto mode = std::make_shared<OutputMode>(size, 60000, OutputMode::Flag::Preferred);
    m_renderLoop->setRefreshRate(mode->refreshRate());

    setInformation(Information{
        .name = QStringLiteral("Virtual-") + name,
        .physicalSize = size,
    });

    setState(State{
        .scale = scale,
        .modes = {mode},
        .currentMode = mode,
    });

    recreateSurface();
}

DrmVirtualOutput::~DrmVirtualOutput()
{
}

bool DrmVirtualOutput::present()
{
    m_vsyncMonitor->arm();
    m_pageFlipPending = true;
    Q_EMIT outputChange(m_layer->currentDamage());
    return true;
}

void DrmVirtualOutput::vblank(std::chrono::nanoseconds timestamp)
{
    if (m_pageFlipPending) {
        DrmAbstractOutput::pageFlipped(timestamp);
    }
}

void DrmVirtualOutput::setDpmsMode(DpmsMode mode)
{
    State next = m_state;
    next.dpmsMode = mode;
    setState(next);
}

DrmOutputLayer *DrmVirtualOutput::primaryLayer() const
{
    return m_layer.get();
}

void DrmVirtualOutput::recreateSurface()
{
    m_layer = m_gpu->platform()->renderBackend()->createLayer(this);
}

}
