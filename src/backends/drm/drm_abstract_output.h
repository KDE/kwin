/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/output.h"

namespace KWin
{

class DrmBackend;
class DrmGpu;
class DrmOutputLayer;
class OutputFrame;

class DrmAbstractOutput : public Output
{
    Q_OBJECT
public:
    DrmAbstractOutput(DrmGpu *gpu);

    RenderLoop *renderLoop() const override;
    void frameFailed() const;
    void pageFlipped(std::chrono::nanoseconds timestamp, PresentationMode mode);
    DrmGpu *gpu() const;

    virtual bool present(const std::shared_ptr<OutputFrame> &frame) = 0;
    virtual DrmOutputLayer *primaryLayer() const = 0;
    virtual DrmOutputLayer *cursorLayer() const = 0;

    void updateEnabled(bool enabled);

protected:
    friend class DrmGpu;

    std::unique_ptr<RenderLoop> m_renderLoop;
    std::shared_ptr<OutputFrame> m_frame;
    DrmGpu *const m_gpu;
};

}
