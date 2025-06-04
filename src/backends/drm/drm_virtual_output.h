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
    explicit DrmVirtualOutput(DrmBackend *backend, const QString &name, const QString &description, const QSize &size, qreal scale);
    ~DrmVirtualOutput() override;

    bool present(const QList<OutputLayer *> &layersToUpdate, const std::shared_ptr<OutputFrame> &frame) override;
    DrmOutputLayer *primaryLayer() const override;
    DrmOutputLayer *cursorLayer() const override;
    void recreateSurface();

private:
    void vblank(std::chrono::nanoseconds timestamp);
    void setDpmsMode(DpmsMode mode) override;

    DrmBackend *const m_backend;
    std::shared_ptr<DrmOutputLayer> m_layer;
    std::shared_ptr<OutputFrame> m_frame;

    std::unique_ptr<SoftwareVsyncMonitor> m_vsyncMonitor;
};

}
