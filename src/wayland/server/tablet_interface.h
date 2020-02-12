/********************************************************************
Copyright 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

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
#ifndef WAYLAND_SERVER_TABLET_INTERFACE_H
#define WAYLAND_SERVER_TABLET_INTERFACE_H

#include <KWayland/Server/kwaylandserver_export.h>
#include <QVector>

#include "resource.h"

namespace KWayland
{
namespace Server
{
class TabletSeatInterface;
class Display;
class SeatInterface;
class SurfaceInterface;
class TabletInterface;
class TabletCursor;

/**
 * This is an implementation of wayland-protocols/unstable/tablet/tablet-unstable-v2.xml
 *
 * This class is just the means to get a @class TabletSeatInterface, which is
 * the class that will have all of the information we need.
 *
 * @since 5.69
 */

class KWAYLANDSERVER_EXPORT TabletManagerInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~TabletManagerInterface();

    TabletSeatInterface *seat(SeatInterface *seat) const;

private:
    friend class Display;
    explicit TabletManagerInterface(Display *d, QObject *parent);
    class Private;
    QScopedPointer<Private> d;
};

class KWAYLANDSERVER_EXPORT TabletToolInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~TabletToolInterface();

    enum Type {
        Pen = 0x140, ///< Pen
        Eraser = 0x141, ///< Eraser
        Brush = 0x142, ///< Brush
        Pencil = 0x143, ///< Pencil
        Airbrush = 0x144, ///< Airbrush
        Finger = 0x145, ///< Finger
        Mouse = 0x146, ///< Mouse
        Lens = 0x147, ///< Lens
        Totem
    };
    Q_ENUM(Type)

    enum Capability {
        Tilt = 1, ///< Tilt axeis
        Pressure = 2, ///< Pressure axis
        Distance = 3, ///< Distance axis
        Rotation = 4, ///< Z-rotation axis
        Slider = 5, ///< Slider axis
        Wheel = 6 ///< Wheel axis
    };
    Q_ENUM(Capability)

    /**
     * Sets the surface the events will be sent to.
     *
     * Make sure the surface supports being sent events to.
     *
     * @see TabletInterface::isSurfaceSupported
     */
    void setCurrentSurface(SurfaceInterface *surface);
    bool isClientSupported() const;

    void sendRemoved();
    void sendProximityIn(TabletInterface *tablet);
    void sendProximityOut();
    void sendUp();
    void sendDown();
    void sendPressure(quint32 pressure);
    void sendDistance(quint32 distance);
    void sendTilt(qreal degreesX, qreal degreesY);
    void sendRotation(qreal degrees);
    void sendSlider(qint32 position);
    void sendWheel(qint32 degrees, qint32 clicks);
    void sendButton(quint32 button, bool pressed);
    void sendFrame(quint32 time);
    void sendMotion(const QPointF &pos);

Q_SIGNALS:
    void cursorChanged(TabletCursor* cursor) const;

private:
    friend class TabletSeatInterface;
    explicit TabletToolInterface(Display *display, Type type, quint32 hsh, quint32 hsl, quint32 hih, quint32 hil, const QVector<Capability> &capability, QObject *parent);
    class Private;
    QScopedPointer<Private> d;
};

class KWAYLANDSERVER_EXPORT TabletCursor : public QObject
{
    Q_OBJECT
public:
    ~TabletCursor() override;
    QPoint hotspot() const;
    quint32 enteredSerial() const;
    SurfaceInterface* surface() const;

Q_SIGNALS:
    void changed();

private:
    friend class TabletToolInterface;
    TabletCursor();
    class Private;
    const QScopedPointer<Private> d;
};

class KWAYLANDSERVER_EXPORT TabletInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~TabletInterface();

    /**
     * @returns true if the surface has been bound to the tablet.
     */
    bool isSurfaceSupported(SurfaceInterface *surface) const;

    void sendRemoved();

private:
    friend class TabletSeatInterface;
    friend class TabletToolInterface;
    explicit TabletInterface(quint32 vendorId, quint32 productId, const QString &name, const QStringList &paths, QObject *parent);
    class Private;
    QScopedPointer<Private> d;
};

class KWAYLANDSERVER_EXPORT TabletSeatInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~TabletSeatInterface();

    TabletInterface *addTablet(quint32 vendorId, quint32 productId, const QString &sysname, const QString &name, const QStringList &paths);
    TabletToolInterface *addTool(TabletToolInterface::Type type, quint64 hardwareSerial, quint64 hardwareId, const QVector<TabletToolInterface::Capability> &capabilities);

    TabletToolInterface *toolByHardwareId(quint64 hardwareId) const;
    TabletToolInterface *toolByHardwareSerial(quint64 hardwareSerial) const;
    TabletInterface *tabletByName(const QString &sysname) const;

    void removeTablet(const QString &sysname);

private:
    friend class TabletManagerInterface;
    explicit TabletSeatInterface(Display *display, QObject *parent);
    class Private;
    QScopedPointer<Private> d;
};

}
}

Q_DECLARE_METATYPE(KWayland::Server::TabletSeatInterface *)

#endif
