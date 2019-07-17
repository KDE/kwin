/********************************************************************
Copyright © 2019 Roman Gilg <subdiff@gmail.com>
Copyright © 2018 Fredrik Höglund <fredrik@kde.org>

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
#ifndef WAYLAND_SERVER_LINUXDMABUF_INTERFACE_H
#define WAYLAND_SERVER_LINUXDMABUF_INTERFACE_H

#include "global.h"
#include "resource.h"

#include <KWayland/Server/kwaylandserver_export.h>

#include <QHash>
#include <QSet>
#include <QSize>

struct wl_buffer_interface;

namespace KWayland
{
namespace Server
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
    virtual ~LinuxDmabufUnstableV1Buffer() = default;

private:
    class Private;
    Private *d;
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
     */
    void setImpl(Impl *impl);

    void setSupportedFormatsWithModifiers(QHash<uint32_t, QSet<uint64_t> > set);

    /**
     * Returns the LinuxDmabufInterface for the given resource.
     **/
    static LinuxDmabufUnstableV1Interface *get(wl_resource *native);

private:
    /**
     * @internal
     */
    explicit LinuxDmabufUnstableV1Interface(Display *display, QObject *parent = nullptr);

    /**
     * Returns a pointer to the wl_buffer implementation for imported dmabufs.
     */
    static const struct wl_buffer_interface *bufferImplementation();

    friend class Display;
    friend class BufferInterface;

    class Private;
    Private *d_func() const;
};

}
}

Q_DECLARE_METATYPE(KWayland::Server::LinuxDmabufUnstableV1Interface*)
Q_DECLARE_OPERATORS_FOR_FLAGS(KWayland::Server::LinuxDmabufUnstableV1Interface::Flags)

#endif // WAYLAND_SERVER_LINUXDMABUF_INTERFACE_H
