/*
 * KWin - the KDE window manager
 * This file is part of the KDE project.
 *
 * SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "abstract_egl_drm_backend.h"

#include "drm_backend.h"
#include "drm_gpu.h"
#include "drm_output.h"

using namespace KWin;

AbstractEglDrmBackend::AbstractEglDrmBackend(DrmBackend *drmBackend, DrmGpu *gpu) : m_backend(drmBackend), m_gpu(gpu)
{
    m_gpu->setEglBackend(this);
    // Egl is always direct rendering.
    setIsDirectRendering(true);
    setSyncsToVBlank(true);
    connect(m_gpu, &DrmGpu::outputEnabled, this, &AbstractEglDrmBackend::addOutput);
    connect(m_gpu, &DrmGpu::outputDisabled, this, &AbstractEglDrmBackend::removeOutput);
}

AbstractEglDrmBackend::~AbstractEglDrmBackend()
{
    cleanup();
}

void AbstractEglDrmBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
}

bool AbstractEglDrmBackend::usesOverlayWindow() const
{
    return false;
}

bool AbstractEglDrmBackend::perScreenRendering() const
{
    return true;
}
