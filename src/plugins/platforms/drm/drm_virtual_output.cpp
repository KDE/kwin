/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_virtual_output.h"

#include "renderloop_p.h"
#include "softwarevsyncmonitor.h"
#include "drm_gpu.h"
#include "drm_backend.h"
#include "logging.h"
#if HAVE_GBM
#include "drm_buffer_gbm.h"
#endif

namespace KWin
{

DrmVirtualOutput::DrmVirtualOutput(DrmGpu *gpu)
    : DrmAbstractOutput(gpu)
    , m_vsyncMonitor(SoftwareVsyncMonitor::create(this))
{
    connect(m_vsyncMonitor, &VsyncMonitor::vblankOccurred, this, &DrmVirtualOutput::vblank);

    static int s_serial = 0;
    m_identifier = s_serial++;
    setName("Virtual-" + QString::number(m_identifier));
    m_modeIndex = 0;
    QVector<Mode> modes = {{{1920, 1080}, 60000, AbstractWaylandOutput::ModeFlags(AbstractWaylandOutput::ModeFlag::Current) | AbstractWaylandOutput::ModeFlag::Preferred, 0}};
    initialize(QByteArray("model_").append(QByteArray::number(m_identifier)),
               QByteArray("manufacturer_").append(QByteArray::number(m_identifier)),
               QByteArray("eisa_").append(QByteArray::number(m_identifier)),
               QByteArray("serial_").append(QByteArray::number(m_identifier)),
               modes[m_modeIndex].size, modes, QByteArray("EDID_").append(QByteArray::number(m_identifier)));
    m_renderLoop->setRefreshRate(modes[m_modeIndex].refreshRate);
}

DrmVirtualOutput::~DrmVirtualOutput()
{
}

bool DrmVirtualOutput::present(const QSharedPointer<DrmBuffer> &buffer, QRegion damagedRegion)
{
    Q_UNUSED(damagedRegion)

    m_currentBuffer = buffer;
    m_vsyncMonitor->arm();
    m_pageFlipPending = true;
    Q_EMIT outputChange(damagedRegion);
    return true;
}

void DrmVirtualOutput::vblank(std::chrono::nanoseconds timestamp)
{
    if (m_pageFlipPending) {
        RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(m_renderLoop);
        renderLoopPrivate->notifyFrameCompleted(timestamp, std::chrono::nanoseconds::zero());
    }
}

void DrmVirtualOutput::updateMode(int modeIndex)
{
    Q_UNUSED(modeIndex)
}

void DrmVirtualOutput::updateMode(uint32_t width, uint32_t height, uint32_t refreshRate)
{
    Q_UNUSED(width)
    Q_UNUSED(height)
    Q_UNUSED(refreshRate)
}

void DrmVirtualOutput::setDpmsMode(DpmsMode mode)
{
    setDpmsModeInternal(mode);
    m_dpmsEnabled = mode == DpmsMode::On;
}

bool DrmVirtualOutput::isDpmsEnabled() const
{
    return m_dpmsEnabled;
}

void DrmVirtualOutput::updateEnablement(bool enable)
{
    gpu()->platform()->enableOutput(this, enable);
}

QSize DrmVirtualOutput::sourceSize() const
{
    return pixelSize();
}

GbmBuffer *DrmVirtualOutput::currentBuffer() const
{
#if HAVE_GBM
    return dynamic_cast<GbmBuffer*>(m_currentBuffer.get());
#else
    return nullptr;
#endif
}

}
