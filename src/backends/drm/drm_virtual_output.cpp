/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_virtual_output.h"

#include "drm_backend.h"
#include "drm_gpu.h"
#include "drm_layer.h"
#include "drm_render_backend.h"
#include "logging.h"
#include "renderloop_p.h"
#include "softwarevsyncmonitor.h"

namespace KWin
{

DrmVirtualOutput::DrmVirtualOutput(const QString &name, DrmGpu *gpu, const QSize &size, Type type)
    : DrmAbstractOutput(gpu)
    , m_vsyncMonitor(SoftwareVsyncMonitor::create(this))
{
    connect(m_vsyncMonitor, &VsyncMonitor::vblankOccurred, this, &DrmVirtualOutput::vblank);

    auto mode = QSharedPointer<OutputMode>::create(size, 60000, OutputMode::Flag::Preferred);
    setModesInternal({mode}, mode);
    m_renderLoop->setRefreshRate(mode->refreshRate());

    setInformation(Information{
        .name = QStringLiteral("Virtual-") + name,
        .physicalSize = size,
        .placeholder = type == Type::Placeholder,
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
        RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(m_renderLoop);
        renderLoopPrivate->notifyFrameCompleted(timestamp);
    }
}

void DrmVirtualOutput::setDpmsMode(DpmsMode mode)
{
    setDpmsModeInternal(mode);
}

void DrmVirtualOutput::updateEnablement(bool enable)
{
    m_gpu->platform()->enableOutput(this, enable);
}

DrmOutputLayer *DrmVirtualOutput::outputLayer() const
{
    return m_layer.data();
}

void DrmVirtualOutput::recreateSurface()
{
    m_layer = m_gpu->platform()->renderBackend()->createLayer(this);
}

}
