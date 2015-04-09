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
#ifndef KWIN_DRM_BACKEND_H
#define KWIN_DRM_BACKEND_H
#include "abstract_backend.h"

#include <QImage>
#include <QSize>
#include <xf86drmMode.h>

namespace KWin
{

class Udev;

class DrmBuffer;

class KWIN_EXPORT DrmBackend : public AbstractBackend
{
    Q_OBJECT
public:
    explicit DrmBackend(QObject *parent = nullptr);
    virtual ~DrmBackend();

    Screens *createScreens(QObject *parent = nullptr) override;
    QPainterBackend *createQPainterBackend() override;

    void init();
    DrmBuffer *createBuffer(const QSize &size);
    void present(DrmBuffer *buffer);

    QSize size() const {
        // TODO: this is wrong
        return m_resolution;
    }
    int fd() const {
        return m_fd;
    }

Q_SIGNALS:
    void screensQueried();

private:
    void openDrm();
    void queryResources();
    QScopedPointer<Udev> m_udev;
    int m_fd = -1;
    int m_drmId = 0;
    // TODO: this is wrong
    QSize m_resolution;
    quint32 m_crtcId = 0;
    quint32 m_connector = 0;
    drmModeModeInfo m_mode;
};

class DrmBuffer
{
public:
    ~DrmBuffer();

    bool map();
    QImage *image() const {
        return m_image;
    }

private:
    friend class DrmBackend;
    DrmBuffer(DrmBackend *backend, const QSize &size);
    DrmBackend *m_backend;
    QSize m_size;
    quint32 m_handle = 0;
    quint32 m_bufferId = 0;
    quint32 m_stride = 0;
    quint64 m_bufferSize = 0;
    void *m_memory = nullptr;
    QImage *m_image = nullptr;
};

}

#endif

