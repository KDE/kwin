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

class DrmBuffer
{
public:
    DrmBuffer(int fd);
    virtual ~DrmBuffer() = default;

    virtual bool needsModeChange(DrmBuffer *b) const {Q_UNUSED(b) return false;}

    quint32 bufferId() const {
        return m_bufferId;
    }

    const QSize &size() const {
        return m_size;
    }

    virtual void releaseGbm() {}

    int fd() const {
        return m_fd;
    }

protected:
    quint32 m_bufferId = 0;
    QSize m_size;
    int m_fd;
};

class DrmDumbBuffer : public DrmBuffer
{
public:
    DrmDumbBuffer(int fd, const QSize &size);
    ~DrmDumbBuffer() override;

    bool needsModeChange(DrmBuffer *b) const override;

    bool map(QImage::Format format = QImage::Format_RGB32);
    quint32 handle() const {
        return m_handle;
    }
    QImage *image() const {
        return m_image;
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

