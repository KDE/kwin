/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef WAYLAND_SERVER_BUFFER_INTERFACE_H
#define WAYLAND_SERVER_BUFFER_INTERFACE_H

#include <QImage>
#include <QObject>

#include <KWayland/Server/kwaylandserver_export.h>

struct wl_resource;
struct wl_shm_buffer;

namespace KWayland
{
namespace Server
{
class SurfaceInterface;


class KWAYLANDSERVER_EXPORT BufferInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~BufferInterface();
    void ref();
    void unref();
    bool isReferenced() const;

    SurfaceInterface *surface() const;
    wl_shm_buffer *shmBuffer();
    wl_resource *resource() const;

    /**
     * Creates a QImage for the shared memory buffer.
     *
     * If the BufferInterface does not reference a shared memory buffer a null QImage is returned.
     *
     * The QImage shares the memory with the buffer and this constraints how the returned
     * QImage can be used and when this method can be invoked.
     *
     * It is not safe to have two shared memory QImages for different BufferInterfaces at
     * the same time. This method ensures that this does not happen and returns a null
     * QImage if a different BufferInterface's data is still mapped to a QImage. Please note
     * that this also applies to all implicitly data shared copies.
     *
     * In case it is needed to keep a copy, a deep copy has to be performed by using QImage::copy.
     *
     * As the underlying shared memory buffer is owned by a different client it is not safe to
     * write to the returned QImage. The image is a read-only buffer. If there is need to modify
     * the image, perform a deep copy.
     *
     **/
    QImage data();

    /**
     * Returns the size of this BufferInterface.
     * Note: only for shared memory buffers (shmBuffer) the size can be derived,
     * for other buffers it might be possible to derive the size if an EGL display
     * is set on Display otherwise the user of the BufferInterface has to use setSize to
     * provide the proper size.
     * @see setSize
     * @see Display::setEglDisplay
     * @since 5.3
     **/
    QSize size() const;
    /**
     * Sets the @p size for non shared memory buffers.
     * @see size
     * @see sizeChanged
     * @since 5.3
     **/
    void setSize(const QSize &size);

    static BufferInterface *get(wl_resource *r);

Q_SIGNALS:
    void aboutToBeDestroyed(KWayland::Server::BufferInterface*);
    /**
     * Emitted when the size of the Buffer changes.
     * @since 5.3
     **/
    void sizeChanged();

private:
    friend class SurfaceInterface;
    explicit BufferInterface(wl_resource *resource, SurfaceInterface *parent);
    class Private;
    QScopedPointer<Private> d;
};

}
}

Q_DECLARE_METATYPE(KWayland::Server::BufferInterface*)

#endif
