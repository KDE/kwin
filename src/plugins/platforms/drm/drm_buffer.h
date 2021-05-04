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
    DrmBuffer(DrmGpu *gpu);
    virtual ~DrmBuffer() = default;

    virtual bool needsModeChange(DrmBuffer *b) const {Q_UNUSED(b) return false;}

    virtual void releaseGbm() {}

    quint32 bufferId() const {
        return m_bufferId;
    }

    const QSize &size() const {
        return m_size;
    }

    DrmGpu *gpu() const {
        return m_gpu;
    }

protected:
    quint32 m_bufferId = 0;
    QSize m_size;
    DrmGpu *m_gpu;
};

class DrmDumbBuffer : public DrmBuffer
{
public:
    DrmDumbBuffer(DrmGpu *gpu, const QSize &size);
    ~DrmDumbBuffer() override;

    bool needsModeChange(DrmBuffer *b) const override;

    bool map(QImage::Format format = QImage::Format_RGB32);
    quint32 handle() const {
        return m_handle;
    }
    QImage *image() const {
        return m_image;
    }
    void *data() const {
        return m_memory;
    }
    quint32 stride() const {
        return m_stride;
    }

private:
    quint32 m_handle = 0;
    quint64 m_bufferSize = 0;
    void *m_memory = nullptr;
    QImage *m_image = nullptr;
    quint32 m_stride = 0;
};

}

#endif

