/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "drm_render_backend.h"
#include "qpainter/qpainterbackend.h"

#include <QList>
#include <QObject>

namespace KWin
{

class DrmBackend;
class DrmAbstractOutput;
class DrmQPainterLayer;
class DrmPipeline;

class DrmQPainterBackend : public QPainterBackend, public DrmRenderBackend
{
    Q_OBJECT
public:
    DrmQPainterBackend(DrmBackend *backend);
    ~DrmQPainterBackend();

    DrmDevice *drmDevice() const override;

    OutputLayer *primaryLayer(Output *output) override;
    OutputLayer *cursorLayer(Output *output) override;

    std::unique_ptr<DrmPipelineLayer> createDrmPlaneLayer(DrmPlane *plane) override;
    std::unique_ptr<DrmPipelineLayer> createDrmPlaneLayer(DrmGpu *gpu, DrmPlane::TypeIndex type) override;
    std::unique_ptr<DrmOutputLayer> createLayer(DrmVirtualOutput *output) override;

private:
    DrmBackend *m_backend;
};
}
