/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "drm_buffer.h"

#include <epoxy/egl.h>
#include <gbm.h>

struct gbm_bo;

namespace KWaylandServer
{
class ClientBuffer;
class LinuxDmaBufV1ClientBuffer;
}

namespace KWin
{

class GbmSurface;
class GLTexture;
class GbmSwapchain;

class GbmBuffer : public DrmGpuBuffer
{
public:
    GbmBuffer(DrmGpu *gpu, gbm_bo *bo, uint32_t flags);
    GbmBuffer(gbm_bo *bo, const std::shared_ptr<GbmSwapchain> &swapchain);
    GbmBuffer(DrmGpu *gpu, gbm_bo *bo, KWaylandServer::LinuxDmaBufV1ClientBuffer *clientBuffer, uint32_t flags);
    ~GbmBuffer() override;

    gbm_bo *bo() const;
    void *mappedData() const;
    KWaylandServer::ClientBuffer *clientBuffer() const;
    uint32_t flags() const;

    bool map(uint32_t flags);

    static std::shared_ptr<GbmBuffer> importBuffer(DrmGpu *gpu, KWaylandServer::LinuxDmaBufV1ClientBuffer *clientBuffer);
    static std::shared_ptr<GbmBuffer> importBuffer(DrmGpu *gpu, GbmBuffer *buffer, uint32_t flags = GBM_BO_USE_SCANOUT);

private:
    void createFds() override;

    gbm_bo *const m_bo;
    const std::weak_ptr<GbmSwapchain> m_swapchain;
    KWaylandServer::ClientBuffer *const m_clientBuffer = nullptr;
    const uint32_t m_flags;
    void *m_data = nullptr;
    void *m_mapping = nullptr;
};

}
