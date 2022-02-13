/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "abstract_wayland_output.h"
#include "drm_display_device.h"

namespace KWin
{

class DrmBackend;

class DrmAbstractOutput : public AbstractWaylandOutput, public DrmDisplayDevice
{
    Q_OBJECT
public:
    DrmAbstractOutput(DrmGpu *gpu);

    RenderLoop *renderLoop() const override;
    QRect renderGeometry() const override;
    void frameFailed() const override;
    void pageFlipped(std::chrono::nanoseconds timestamp) const override;

protected:
    friend class DrmGpu;

    RenderLoop *m_renderLoop;
};

}
