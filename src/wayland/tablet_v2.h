/*
    SPDX-FileCopyrightText: 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QList>
#include <QObject>
#include <memory>

struct wl_resource;

namespace KWin
{
class ClientConnection;
class Display;
class InputDevice;
class InputDeviceTabletTool;
class SeatInterface;
class SurfaceInterface;
class TabletSurfaceCursorV2;
class TabletSurfaceCursorV2Private;
class TabletManagerV2InterfacePrivate;
class TabletSeatV2Interface;
class TabletSeatV2InterfacePrivate;
class TabletToolV2InterfacePrivate;
class TabletV2Interface;
class TabletV2InterfacePrivate;
class TabletPadV2Interface;
class TabletPadV2InterfacePrivate;
class TabletPadRingV2Interface;
class TabletPadRingV2InterfacePrivate;
class TabletPadStripV2Interface;
class TabletPadStripV2InterfacePrivate;
class TabletPadGroupV2Interface;
class TabletPadGroupV2InterfacePrivate;

/**
 * This is an implementation of wayland-protocols/unstable/tablet/tablet-unstable-v2.xml
 *
 * This class is just the means to get a @class TabletSeatInterface, which is
 * the class that will have all of the information we need.
 */

class KWIN_EXPORT TabletManagerV2Interface : public QObject
{
    Q_OBJECT
public:
    explicit TabletManagerV2Interface(Display *d, QObject *parent);
    virtual ~TabletManagerV2Interface();

    TabletSeatV2Interface *seat(SeatInterface *seat) const;

private:
    std::unique_ptr<TabletManagerV2InterfacePrivate> d;
};

class KWIN_EXPORT TabletSurfaceCursorV2 : public QObject
{
    Q_OBJECT
public:
    ~TabletSurfaceCursorV2() override;
    QPoint hotspot() const;
    quint32 enteredSerial() const;
    SurfaceInterface *surface() const;

Q_SIGNALS:
    void changed();

private:
    TabletSurfaceCursorV2();
    const std::unique_ptr<TabletSurfaceCursorV2Private> d;
    friend class TabletToolV2InterfacePrivate;
};

using TabletCursorSourceV2 = std::variant<TabletSurfaceCursorV2 *, QByteArray>;

class KWIN_EXPORT TabletToolV2Interface : public QObject
{
    Q_OBJECT
public:
    virtual ~TabletToolV2Interface();

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
        Wheel = 6, ///< Wheel axis
    };
    Q_ENUM(Capability)

    bool hasCapability(Capability capability) const;

    /**
     * Sets the surface the events will be sent to.
     *
     * Make sure the surface supports being sent events to.
     *
     * @see TabletV2Interface::isSurfaceSupported
     */
    void setCurrentSurface(SurfaceInterface *surface);
    SurfaceInterface *currentSurface() const;

    bool isClientSupported() const;

    quint32 proximitySerial() const;
    std::optional<quint32> downSerial() const;

    void sendProximityIn(TabletV2Interface *tablet);
    void sendProximityOut();
    void sendUp();
    void sendDown();
    void sendPressure(qreal pressure);
    void sendDistance(qreal distance);
    void sendTilt(qreal degreesX, qreal degreesY);
    void sendRotation(qreal degrees);
    void sendSlider(qreal position);
    void sendWheel(qint32 degrees, qint32 clicks);
    void sendButton(quint32 button, bool pressed);
    void sendFrame(quint32 time);
    void sendMotion(const QPointF &pos);

    static TabletToolV2Interface *get(wl_resource *resource);

Q_SIGNALS:
    void cursorChanged(const TabletCursorSourceV2 &cursor);

private:
    friend class TabletSeatV2InterfacePrivate;
    friend class TabletSeatV2Interface;
    explicit TabletToolV2Interface(Display *display,
                                   Type type,
                                   quint32 hsh,
                                   quint32 hsl,
                                   quint32 hih,
                                   quint32 hil,
                                   const QList<Capability> &capability);
    std::unique_ptr<TabletToolV2InterfacePrivate> d;
};

class KWIN_EXPORT TabletPadV2Interface : public QObject
{
    Q_OBJECT
public:
    virtual ~TabletPadV2Interface();

    TabletPadRingV2Interface *ring(uint at) const;
    TabletPadStripV2Interface *strip(uint at) const;
    void sendButton(std::chrono::microseconds time, quint32 button, bool pressed);

    void setCurrentSurface(SurfaceInterface *surface, TabletV2Interface *tablet);
    SurfaceInterface *currentSurface() const;

Q_SIGNALS:
    void feedback(KWin::ClientConnection *client, quint32 button, const QString &description, quint32 serial);

private:
    friend class TabletSeatV2Interface;
    friend class TabletSeatV2InterfacePrivate;
    explicit TabletPadV2Interface(const QString &path,
                                  quint32 buttons,
                                  quint32 rings,
                                  quint32 strips,
                                  quint32 modes,
                                  quint32 currentMode,
                                  Display *display,
                                  TabletSeatV2Interface *parent);
    std::unique_ptr<TabletPadV2InterfacePrivate> d;
};

class KWIN_EXPORT TabletPadRingV2Interface : public QObject
{
    Q_OBJECT
public:
    virtual ~TabletPadRingV2Interface();

    enum Source {
        SourceFinger = 1, // finger
    };
    Q_ENUM(Source)

    void sendSource(Source source);
    void sendAngle(qreal angle);
    void sendStop();
    void sendFrame(quint32 time);

private:
    friend class TabletPadGroupV2Interface;
    friend class TabletPadV2InterfacePrivate;
    friend class TabletSeatV2InterfacePrivate;
    explicit TabletPadRingV2Interface(TabletPadV2Interface *parent);
    std::unique_ptr<TabletPadRingV2InterfacePrivate> d;
};

class KWIN_EXPORT TabletPadStripV2Interface : public QObject
{
    Q_OBJECT
public:
    virtual ~TabletPadStripV2Interface();

    enum Source {
        SourceFinger = 1, // finger
    };

    void sendSource(Source source);
    void sendPosition(quint32 position);
    void sendFrame(quint32 time);
    void sendStop();

private:
    friend class TabletPadGroupV2Interface;
    friend class TabletPadV2InterfacePrivate;
    friend class TabletSeatV2InterfacePrivate;
    explicit TabletPadStripV2Interface(TabletPadV2Interface *parent);
    std::unique_ptr<TabletPadStripV2InterfacePrivate> d;
};

class KWIN_EXPORT TabletPadGroupV2Interface : public QObject
{
    Q_OBJECT
public:
    virtual ~TabletPadGroupV2Interface();

    void sendModeSwitch(quint32 time, quint32 serial, quint32 mode);

private:
    friend class TabletPadV2Interface;
    friend class TabletPadV2InterfacePrivate;
    friend class TabletSeatV2InterfacePrivate;
    explicit TabletPadGroupV2Interface(quint32 currentMode, TabletPadV2Interface *parent);
    std::unique_ptr<TabletPadGroupV2InterfacePrivate> d;
};

class KWIN_EXPORT TabletV2Interface : public QObject
{
    Q_OBJECT
public:
    virtual ~TabletV2Interface();

    /**
     * @returns true if the surface has been bound to the tablet.
     */
    bool isSurfaceSupported(SurfaceInterface *surface) const;

private:
    friend class TabletSeatV2Interface;
    friend class TabletSeatV2InterfacePrivate;
    friend class TabletPadV2Interface;
    friend class TabletToolV2Interface;
    explicit TabletV2Interface(quint32 vendorId, quint32 productId, const QString &name, const QStringList &paths, QObject *parent);
    std::unique_ptr<TabletV2InterfacePrivate> d;
};

class KWIN_EXPORT TabletSeatV2Interface : public QObject
{
    Q_OBJECT
public:
    virtual ~TabletSeatV2Interface();

    TabletV2Interface *addTablet(InputDevice *device);
    TabletPadV2Interface *addPad(InputDevice *device);
    TabletToolV2Interface *addTool(InputDeviceTabletTool *device);
    void remove(InputDevice *device);

    TabletToolV2Interface *tool(InputDeviceTabletTool *device) const;
    TabletV2Interface *tablet(InputDevice *device) const;
    TabletPadV2Interface *pad(InputDevice *device) const;
    TabletV2Interface *matchingTablet(TabletPadV2Interface *pad) const;

    bool isClientSupported(ClientConnection *client) const;

    bool hasImplicitGrab(quint32 serial) const;

private:
    friend class TabletManagerV2InterfacePrivate;
    explicit TabletSeatV2Interface(Display *display, QObject *parent);
    std::unique_ptr<TabletSeatV2InterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWin::TabletSeatV2Interface *)
