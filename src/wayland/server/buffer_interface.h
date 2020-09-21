/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_BUFFER_INTERFACE_H
#define WAYLAND_SERVER_BUFFER_INTERFACE_H

#include <QImage>
#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

struct wl_resource;
struct wl_shm_buffer;

namespace KWaylandServer
{
class Display;
class LinuxDmabufBuffer;

/**
 * @brief Reference counted representation of a Wayland buffer on Server side.
 *
 * This class encapsulates a rendering buffer which is normally attached to a SurfaceInterface.
 * A client should not render to a Wayland buffer as long as the buffer gets used by the server.
 * The server signals whether it's still used. This class provides a convenience access for this
 * functionality by performing reference counting and deleting the BufferInterface instance
 * automatically once it is no longer accessed.
 *
 * The BufferInterface is referenced as long as it is attached to a SurfaceInterface. If one wants
 * to keep access to the BufferInterface for a longer time ensure to call ref on first usage and
 * unref again once access to it is no longer needed.
 *
 * In Wayland the buffer is an abstract concept and a buffer might represent multiple different
 * concrete buffer techniques. This class has direct support for shared memory buffers built and
 * provides access to the native buffer for different (e.g. EGL/drm) buffers.
 *
 * If the EGL display has been registered in the Display the BufferInterface can also provide
 * some information about an EGL/drm buffer.
 *
 * For shared memory buffers a direct conversion to a memory-mapped QImage possible using the
 * data method. Please refer to the documentation for notes on the restrictions when using the
 * shared memory-mapped QImages.
 *
 * @see Display
 * @see SurfaceInterace
 **/
class KWAYLANDSERVER_EXPORT BufferInterface : public QObject
{
    Q_OBJECT

public:
    BufferInterface(Display *display, wl_resource *resource);
    virtual ~BufferInterface();
    /**
     * Reference the BufferInterface.
     *
     * As long as the reference counting has not reached @c 0 the BufferInterface is valid
     * and blocked for usage by the client.
     *
     * @see unref
     * @see isReferenced
     **/
    void ref();
    /**
     * Unreference the BufferInterface.
     *
     * If the reference counting reached @c 0 the BufferInterface is released, so that the
     * client can use it again.
     *
     * @see ref
     * @see isReferenced
     **/
    void unref();
    /**
     * @returns whether the BufferInterface is currently referenced
     *
     * @see ref
     * @see unref
     **/
    bool isReferenced() const;

    /**
     * @returns The native wl_shm_buffer if the BufferInterface represents a shared memory buffer, otherwise @c nullptr.
     **/
    wl_shm_buffer *shmBuffer();
    /**
     * Returns a pointer to the LinuxDmabufBuffer when the buffer is a dmabuf buffer, and nullptr otherwise.
     */
    LinuxDmabufBuffer *linuxDmabufBuffer();
    /**
     * @returns the native wl_resource wrapped by this BufferInterface.
     **/
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
     * Returns the width of the buffer in device pixels.
     */
    int width() const;
    /**
     * Returns the height of the buffer in device pixels.
     */
    int height() const;
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

    /**
     * Returns whether the format of the BufferInterface has an alpha channel.
     * For shared memory buffers returns @c true for format @c WL_SHM_FORMAT_ARGB8888,
     * for all other formats returns @c false.
     *
     * For EGL buffers returns @c true for format @c EGL_TEXTURE_RGBA, for all other formats
     * returns @c false.
     *
     * If the format cannot be queried the default value (@c false) is returned.
     *
     * @since 5.4
     **/
    bool hasAlphaChannel() const;

    static BufferInterface *get(Display *display, wl_resource *r);

Q_SIGNALS:
    void aboutToBeDestroyed(KWaylandServer::BufferInterface*);
    /**
     * Emitted when the size of the Buffer changes.
     * @since 5.3
     **/
    void sizeChanged();

private:
    class Private;
    QScopedPointer<Private> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::BufferInterface*)

#endif
