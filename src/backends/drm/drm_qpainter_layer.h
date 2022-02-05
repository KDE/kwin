/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "drm_layer.h"

namespace KWin
{

class DumbSwapchain;

class DrmQPainterLayer : public DrmLayer {
public:
    DrmQPainterLayer(DrmAbstractOutput *output);

    std::optional<QRegion> startRendering() override;
    bool endRendering(const QRegion &damagedRegion) override;
    bool scanout(SurfaceItem *surfaceItem) override;
    QSharedPointer<DrmBuffer> testBuffer() override;
    QSharedPointer<DrmBuffer> currentBuffer() const override;
    DrmAbstractOutput *output() const override;

private:
    bool doesSwapchainFit() const;

    QSharedPointer<DumbSwapchain> m_swapchain;
    DrmAbstractOutput *const m_output;
};

}
