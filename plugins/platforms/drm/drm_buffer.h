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

struct gbm_bo;
struct gbm_surface;

namespace KWin
{

class DrmBackend;

class DrmBuffer
{
public:
    ~DrmBuffer();

    bool map(QImage::Format format = QImage::Format_RGB32);
    QImage *image() const {
        return m_image;
    }
    quint32 handle() const {
        return m_handle;
    }
    const QSize &size() const {
        return m_size;
    }
    quint32 bufferId() const {
        return m_bufferId;
    }
    quint32 stride() const {
        return m_stride;
    }
    gbm_bo *gbm() const {
        return m_bo;
    }
    bool isGbm() const {
        return m_bo != nullptr;
    }
    bool deleteAfterPageFlip() const {
        return m_deleteAfterPageFlip;
    }
    void releaseGbm();

private:
    friend class DrmBackend;
    DrmBuffer(DrmBackend *backend, const QSize &size);
    DrmBuffer(DrmBackend *backend, gbm_surface *surface);
    DrmBackend *m_backend;
    gbm_surface *m_surface = nullptr;
    gbm_bo *m_bo = nullptr;
    QSize m_size;
    quint32 m_handle = 0;
    quint32 m_bufferId = 0;
    quint32 m_stride = 0;
    quint64 m_bufferSize = 0;
    void *m_memory = nullptr;
    QImage *m_image = nullptr;
    bool m_deleteAfterPageFlip = false;
};

}

#endif

