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
#ifndef WAYLAND_SERVER_POINTER_INTERFACE_H
#define WAYLAND_SERVER_POINTER_INTERFACE_H

#include <KWayland/Server/kwaylandserver_export.h>

#include "resource.h"

namespace KWayland
{
namespace Server
{

class Cursor;
class PointerGesturesUnstableV1Interface;
class RelativePointerManagerUnstableV1Interface;
class SeatInterface;
class SurfaceInterface;

enum class PointerAxisSource;

/**
 * @brief Resource for the wl_pointer interface.
 *
 * @see SeatInterface
 **/
class KWAYLANDSERVER_EXPORT PointerInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~PointerInterface();

    /**
     * @returns the focused SurfaceInterface on this pointer resource, if any.
     **/
    SurfaceInterface *focusedSurface() const;

    /**
     * The Cursor set on this PointerInterface. Might be @c null.
     * @since 5.3
     **/
    Cursor *cursor() const;

    /**
     * @returns The PointerInterface for the @p native resource.
     * @since 5.28
     **/
    static PointerInterface *get(wl_resource *native);

Q_SIGNALS:
    /**
     * Signal emitted whenever the Cursor changes.
     **/
    void cursorChanged();

private:
    void setFocusedSurface(SurfaceInterface *surface, quint32 serial);
    void buttonPressed(quint32 button, quint32 serial);
    void buttonReleased(quint32 button, quint32 serial);
    void axis(Qt::Orientation orientation, qreal delta, qint32 discreteDelta, PointerAxisSource source);
    void axis(Qt::Orientation orientation, quint32 delta);
    void relativeMotion(const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint64 microseconds);
    friend class SeatInterface;
    friend class RelativePointerManagerUnstableV1Interface;
    friend class PointerGesturesUnstableV1Interface;
    explicit PointerInterface(SeatInterface *parent, wl_resource *parentResource);
    class Private;
    Private *d_func() const;
};

/**
 * @brief Class encapsulating a Cursor image.
 *
 * @since 5.3
 **/
class KWAYLANDSERVER_EXPORT Cursor : public QObject
{
    Q_OBJECT
public:
    virtual ~Cursor();
    /**
     * The hotspot of the cursor image in surface-relative coordinates.
     **/
    QPoint hotspot() const;
    /**
     * The entered serial when the Cursor got set.
     **/
    quint32 enteredSerial() const;
    /**
     * The PointerInterface this Cursor belongs to.
     **/
    PointerInterface *pointer() const;
    /**
     * The SurfaceInterface for the image content of the Cursor.
     **/
    QPointer<SurfaceInterface> surface() const;

Q_SIGNALS:
    void hotspotChanged();
    void enteredSerialChanged();
    void surfaceChanged();
    void changed();

private:
    friend class PointerInterface;
    Cursor(PointerInterface *parent);
    class Private;
    const QScopedPointer<Private> d;
};

}
}

Q_DECLARE_METATYPE(KWayland::Server::PointerInterface*)

#endif
