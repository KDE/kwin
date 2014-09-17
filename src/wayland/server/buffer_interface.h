/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_WAYLAND_SERVER_BUFFER_INTERFACE_H
#define KWIN_WAYLAND_SERVER_BUFFER_INTERFACE_H

#include <QImage>
#include <QObject>

#include <kwaylandserver_export.h>

struct wl_resource;
struct wl_shm_buffer;

namespace KWin
{
namespace WaylandServer
{
class SurfaceInterface;


class KWAYLANDSERVER_EXPORT BufferInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~BufferInterface();
    void ref();
    void unref();
    bool isReferenced() const {
        return m_refCount > 0;
    }

    SurfaceInterface *surface() const {
        return m_surface;
    }
    wl_shm_buffer *shmBuffer() {
        return m_shmBuffer;
    }

    QImage data();

private:
    friend class SurfaceInterface;
    explicit BufferInterface(wl_resource *resource, SurfaceInterface *parent);
    QImage::Format format() const;
    void createImage();
    void releaseImage();
    wl_resource *m_buffer;
    wl_shm_buffer *m_shmBuffer;
    SurfaceInterface *m_surface;
    int m_refCount;
    QImage m_image;
};

}
}

#endif
