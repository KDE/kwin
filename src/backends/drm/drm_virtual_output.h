/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "drm_abstract_output.h"

#include <QObject>
#include <QRect>

namespace KWin
{

class SoftwareVsyncMonitor;
class VirtualBackend;
class DrmPipelineLayer;

class DrmVirtualOutput : public DrmAbstractOutput
{
    Q_OBJECT

public:
    DrmVirtualOutput(const QString &name, DrmGpu *gpu, const QSize &size, qreal scale);
    ~DrmVirtualOutput() override;

    bool present() override;
    DrmOutputLayer *primaryLayer() const override;
    void recreateSurface();

private:
    void vblank(std::chrono::nanoseconds timestamp);
    void setDpmsMode(DpmsMode mode) override;

    std::shared_ptr<DrmOutputLayer> m_layer;
    bool m_pageFlipPending = true;

    std::unique_ptr<SoftwareVsyncMonitor> m_vsyncMonitor;
};

}
