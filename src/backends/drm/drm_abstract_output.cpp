/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_abstract_output.h"
#include "core/renderbackend.h"
#include "drm_backend.h"
#include "drm_gpu.h"
#include "drm_layer.h"

namespace KWin
{

DrmAbstractOutput::DrmAbstractOutput()
    : m_renderLoop(std::make_unique<RenderLoop>(this))
{
}

RenderLoop *DrmAbstractOutput::renderLoop() const
{
    return m_renderLoop.get();
}

DrmOutputLayer *DrmAbstractOutput::overlayLayer() const
{
    return nullptr;
}
}

#include "moc_drm_abstract_output.cpp"
