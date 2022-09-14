/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "drm_buffer.h"

namespace KWin
{

class DrmDumbBuffer : public DrmGpuBuffer
{
public:
    DrmDumbBuffer(DrmGpu *gpu, const QSize &size, uint32_t format, uint32_t handle, uint32_t stride, size_t bufferSize);
    ~DrmDumbBuffer() override;

    bool map(QImage::Format format = QImage::Format_RGB32);
    QImage *image() const;
    void *data() const;

    static std::shared_ptr<DrmDumbBuffer> createDumbBuffer(DrmGpu *gpu, const QSize &size, uint32_t format);

private:
    const size_t m_bufferSize;
    void *m_memory = nullptr;
    std::unique_ptr<QImage> m_image;
};

}
