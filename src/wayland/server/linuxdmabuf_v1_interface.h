/*
    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2018 Fredrik Höglund <fredrik@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_LINUXDMABUF_INTERFACE_H
#define WAYLAND_SERVER_LINUXDMABUF_INTERFACE_H

#include "global.h"
#include "resource.h"

#include <KWaylandServer/kwaylandserver_export.h>

#include <QHash>
#include <QSet>
#include <QSize>

struct wl_buffer_interface;

namespace KWaylandServer
{
class BufferInterface;

/**
 * The base class for linux-dmabuf buffers
 *
 * Compositors should reimplement this class to store objects specific
 * to the underlying graphics stack.
 */
class KWAYLANDSERVER_EXPORT LinuxDmabufBuffer
{
public:
    LinuxDmabufBuffer();
    virtual ~LinuxDmabufBuffer() = default;

    /**
     * Returns the DRM format code for the buffer.
     */
    uint32_t format() const;
    /**
     * Returns the size of the buffer.
     */
    QSize size() const;

private:
    class Private;
    Private *d;

    friend class LinuxDmabufUnstableV1Buffer;
};

class KWAYLANDSERVER_EXPORT LinuxDmabufUnstableV1Buffer : public LinuxDmabufBuffer
{
public:
    /**
     * Creates a new Buffer.
     */
    LinuxDmabufUnstableV1Buffer(uint32_t format, const QSize &size);

    /**
     * Destroys the Buffer.
     */
    virtual ~LinuxDmabufUnstableV1Buffer();

private:
    class Private;
    QScopedPointer<Private> d;
};

/**
 * Represents the global zpw_linux_dmabuf_v1 interface.
 *
 * This interface provides a way for clients to create generic dmabuf based wl_buffers.
 */
class KWAYLANDSERVER_EXPORT LinuxDmabufUnstableV1Interface : public Global
{
    Q_OBJECT
public:
    explicit LinuxDmabufUnstableV1Interface(Display *display, QObject *parent = nullptr);

    enum Flag {
        YInverted           = (1 << 0),    /// Contents are y-inverted
        Interlaced          = (1 << 1),    /// Content is interlaced
        BottomFieldFirst    = (1 << 2)     /// Bottom field first
    };

    Q_DECLARE_FLAGS(Flags, Flag)

    /**
     * Represents a plane in a buffer
     */
    struct Plane {
        int fd;             /// The dmabuf file descriptor
        uint32_t offset;    /// The offset from the start of buffer
        uint32_t stride;    /// The distance from the start of a row to the next row in bytes
        uint64_t modifier;  /// The layout modifier
    };

    /**
     * The Iface class provides an interface from the LinuxDmabufInterface into the compositor
     */
    class Impl {
    public:
        Impl() = default;
        virtual ~Impl() = default;

        /**
         * Imports a linux-dmabuf buffer into the compositor.
         *
         * The parent LinuxDmabufUnstableV1Interface class takes ownership of returned
         * buffer objects.
         *
         * In return the returned buffer takes ownership of the file descriptor for each
         * plane.
         *
         * Note that it is the responsibility of the caller to close the file descriptors
         * when the import fails.
         *
         * @return The imported buffer on success, and nullptr otherwise.
         */
        virtual LinuxDmabufUnstableV1Buffer *importBuffer(const QVector<Plane> &planes,
                                                          uint32_t format,
                                                          const QSize &size,
                                                          Flags flags) = 0;
    };

    /**
     * Destroys the LinuxDmabufUnstableV1Interface.
     */
    virtual ~LinuxDmabufUnstableV1Interface();

    /**
     * Sets the compositor implementation for the dmabuf interface.
     *
     * The ownership is not transferred by this call.
     */
    void setImpl(Impl *impl);

    void setSupportedFormatsWithModifiers(QHash<uint32_t, QSet<uint64_t> > set);

    /**
     * Returns the LinuxDmabufInterface for the given resource.
     **/
    static LinuxDmabufUnstableV1Interface *get(wl_resource *native);

private:
    /**
     * Returns a pointer to the wl_buffer implementation for imported dmabufs.
     */
    static const struct wl_buffer_interface *bufferImplementation();

    friend class BufferInterface;

    class Private;
    Private *d_func() const;
};

}

Q_DECLARE_METATYPE(KWaylandServer::LinuxDmabufUnstableV1Interface*)
Q_DECLARE_OPERATORS_FOR_FLAGS(KWaylandServer::LinuxDmabufUnstableV1Interface::Flags)

#endif // WAYLAND_SERVER_LINUXDMABUF_INTERFACE_H
