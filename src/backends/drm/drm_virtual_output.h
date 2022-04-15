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
    enum class Type {
        Virtual,
        Placeholder,
    };

    DrmVirtualOutput(const QString &name, DrmGpu *gpu, const QSize &size, Type type);
    ~DrmVirtualOutput() override;

    bool present() override;
    DrmOutputLayer *outputLayer() const override;
    void recreateSurface();

private:
    void vblank(std::chrono::nanoseconds timestamp);
    void setDpmsMode(DpmsMode mode) override;
    void updateEnablement(bool enable) override;

    QSharedPointer<DrmOutputLayer> m_layer;
    bool m_pageFlipPending = true;

    SoftwareVsyncMonitor *m_vsyncMonitor;
};

}
