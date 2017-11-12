/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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
    ~DrmDumbBuffer();

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

