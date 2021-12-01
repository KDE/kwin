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

namespace KWin
{
static int s_serial = 0;
DrmVirtualOutput::DrmVirtualOutput(DrmGpu *gpu, const QSize &size)
    : DrmVirtualOutput(QString::number(s_serial++), gpu, size)
{
}

DrmVirtualOutput::DrmVirtualOutput(const QString &name, DrmGpu *gpu, const QSize &size)
    : DrmAbstractOutput(gpu)
    , m_vsyncMonitor(SoftwareVsyncMonitor::create(this))
{
    connect(m_vsyncMonitor, &VsyncMonitor::vblankOccurred, this, &DrmVirtualOutput::vblank);

    setName("Virtual-" + name);
    m_modeIndex = 0;
    QVector<Mode> modes = {{size, 60000, AbstractWaylandOutput::ModeFlags(AbstractWaylandOutput::ModeFlag::Current) | AbstractWaylandOutput::ModeFlag::Preferred, 0}};
    initialize(QLatin1String("model_") + name,
               QLatin1String("manufacturer_") + name,
               QLatin1String("eisa_") + name,
               QLatin1String("serial_") + name,
               modes[m_modeIndex].size,
               modes,
               QByteArray("EDID_") + name.toUtf8());
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
        renderLoopPrivate->notifyFrameCompleted(timestamp);
    }
}

void DrmVirtualOutput::setDpmsMode(DpmsMode mode)
{
    setDpmsModeInternal(mode);
}

void DrmVirtualOutput::updateEnablement(bool enable)
{
    gpu()->platform()->enableOutput(this, enable);
}

QSize DrmVirtualOutput::bufferSize() const
{
    return pixelSize();
}

QSize DrmVirtualOutput::sourceSize() const
{
    return pixelSize();
}

bool DrmVirtualOutput::isFormatSupported(uint32_t drmFormat) const
{
    Q_UNUSED(drmFormat);
    return true;
}

QVector<uint64_t> DrmVirtualOutput::supportedModifiers(uint32_t drmFormat) const
{
    Q_UNUSED(drmFormat);
    // empty list -> implicit modifiers are used / modifier is freely chosen by gbm
    return {};
}

int DrmVirtualOutput::gammaRampSize() const
{
    return 200;
}

bool DrmVirtualOutput::setGammaRamp(const GammaRamp &gamma)
{
    Q_UNUSED(gamma);
    return true;
}

bool DrmVirtualOutput::needsSoftwareTransformation() const
{
    return false;
}

int DrmVirtualOutput::maxBpc() const
{
    return 8;
}

}
