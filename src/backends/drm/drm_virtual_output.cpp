/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_virtual_output.h"

#include "core/renderbackend.h"
#include "drm_backend.h"
#include "drm_gpu.h"
#include "drm_layer.h"
#include "drm_render_backend.h"
#include "utils/softwarevsyncmonitor.h"

namespace KWin
{

DrmVirtualOutput::DrmVirtualOutput(DrmBackend *backend, const QString &name, const QString &description, const QSize &size, qreal scale)
    : m_backend(backend)
    , m_vsyncMonitor(SoftwareVsyncMonitor::create())
{
    connect(m_vsyncMonitor.get(), &VsyncMonitor::vblankOccurred, this, &DrmVirtualOutput::vblank);

    auto mode = std::make_shared<OutputMode>(size, 60000, OutputMode::Flag::Preferred);
    m_renderLoop->setRefreshRate(mode->refreshRate());

    setInformation(Information{
        .name = QStringLiteral("Virtual-") + name,
        .model = description,
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

bool DrmVirtualOutput::present(const QList<OutputLayer *> &layersToUpdate, const std::shared_ptr<OutputFrame> &frame)
{
    m_frame = frame;
    m_vsyncMonitor->arm();
    Q_EMIT outputChange(frame->damage());
    return true;
}

void DrmVirtualOutput::vblank(std::chrono::nanoseconds timestamp)
{
    if (m_frame) {
        m_frame->presented(timestamp, PresentationMode::VSync);
        m_frame.reset();
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

DrmOutputLayer *DrmVirtualOutput::cursorLayer() const
{
    return nullptr;
}

void DrmVirtualOutput::recreateSurface()
{
    m_layer = m_backend->renderBackend()->createLayer(this);
}

}

#include "moc_drm_virtual_output.cpp"
