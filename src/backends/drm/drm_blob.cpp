/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_blob.h"
#include "drm_gpu.h"

namespace KWin
{

DrmBlob::DrmBlob(DrmGpu *gpu, uint32_t blobId)
    : m_gpu(gpu)
    , m_blobId(blobId)
{
}

DrmBlob::~DrmBlob()
{
    if (m_blobId) {
        drmModeDestroyPropertyBlob(m_gpu->fd(), m_blobId);
    }
}

uint32_t DrmBlob::blobId() const
{
    return m_blobId;
}

std::shared_ptr<DrmBlob> DrmBlob::create(DrmGpu *gpu, const void *data, uint32_t dataSize)
{
    uint32_t id = 0;
    if (drmModeCreatePropertyBlob(gpu->fd(), data, dataSize, &id) == 0) {
        return std::make_shared<DrmBlob>(gpu, id);
    } else {
        return nullptr;
    }
}
}
