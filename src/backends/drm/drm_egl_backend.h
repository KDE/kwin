/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "drm_plane.h"
#include "opengl/eglbackend.h"

#include <QHash>
#include <QPointer>
#include <optional>

namespace KWin
{

struct DmaBufAttributes;
class BackendOutput;
class DrmOutput;
class DumbSwapchain;
class DrmBackend;
class DrmGpu;
class DrmPipelineLayer;
class DrmOutputLayer;
class DrmPipeline;
class EglContext;
class EglDisplay;

/**
 * @brief OpenGL Backend using Egl on a GBM surface.
 */
class EglGbmBackend : public EglBackend
{
    Q_OBJECT

public:
    EglGbmBackend(RenderDevice *device);
    ~EglGbmBackend() override;

    QList<OutputLayer *> compatibleOutputLayers(BackendOutput *output) override;
    bool init() override;
};

} // namespace
