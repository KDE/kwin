/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include <memory>
#include <stdint.h>

namespace KWin
{

class DrmGpu;

class DrmBlob
{
public:
    DrmBlob(DrmGpu *gpu, uint32_t blobId);
    ~DrmBlob();

    uint32_t blobId() const;

    static std::shared_ptr<DrmBlob> create(DrmGpu *gpu, const void *data, uint32_t dataSize);

protected:
    DrmGpu *const m_gpu;
    const uint32_t m_blobId;
};

}
