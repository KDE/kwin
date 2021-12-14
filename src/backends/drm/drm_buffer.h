/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_DRM_BUFFER_H
#define KWIN_DRM_BUFFER_H

#include <QImage>
#include <QSize>

namespace KWin
{

class DrmGpu;

class DrmBuffer
{
public:
    DrmBuffer(DrmGpu *gpu, uint32_t format, uint64_t modifier);
    virtual ~DrmBuffer() = default;

    virtual bool needsModeChange(DrmBuffer *b) const {Q_UNUSED(b) return false;}

    quint32 bufferId() const;
    const QSize &size() const;
    DrmGpu *gpu() const;
    uint32_t format() const;
    uint64_t modifier() const;

protected:
    quint32 m_bufferId = 0;
    QSize m_size;
    DrmGpu *m_gpu;
    uint32_t m_format;
    uint64_t m_modifier;
};

class DrmDumbBuffer : public DrmBuffer
{
public:
    DrmDumbBuffer(DrmGpu *gpu, const QSize &size, uint32_t drmFormat);
    ~DrmDumbBuffer() override;

    bool needsModeChange(DrmBuffer *b) const override;

    bool map(QImage::Format format = QImage::Format_RGB32);
    quint32 handle() const;
    QImage *image() const;
    void *data() const;
    quint32 stride() const;

private:
    quint32 m_handle = 0;
    quint64 m_bufferSize = 0;
    void *m_memory = nullptr;
    QImage *m_image = nullptr;
    quint32 m_stride = 0;
};

}

#endif

