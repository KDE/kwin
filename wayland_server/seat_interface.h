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
#ifndef KWIN_WAYLAND_SERVER_SEAT_INTERFACE_H
#define KWIN_WAYLAND_SERVER_SEAT_INTERFACE_H

#include <QHash>
#include <QObject>
#include <QPoint>

#include <wayland-server.h>

namespace KWin
{
namespace WaylandServer
{

class Display;
class KeyboardInterface;
class PointerInterface;
class SurfaceInterface;

class SeatInterface : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool pointer READ hasPointer WRITE setHasPointer NOTIFY hasPointerChanged)
    Q_PROPERTY(bool keyboard READ hasKeyboard WRITE setHasKeyboard NOTIFY hasKeyboardChanged)
    Q_PROPERTY(bool tourch READ hasTouch WRITE setHasTouch NOTIFY hasTouchChanged)
public:
    virtual ~SeatInterface();

    void create();
    void destroy();
    bool isValid() const {
        return m_seat != nullptr;
    }

    const QString &name() const {
        return m_name;
    }
    bool hasPointer() const {
        return m_pointer;
    }
    bool hasKeyboard() const {
        return m_keyboard;
    }
    bool hasTouch() const {
        return m_touch;
    }
    PointerInterface *pointer() {
        return m_pointerInterface;
    }
    KeyboardInterface *keyboard() {
        return m_keyboardInterface;
    }

    void setName(const QString &name);
    void setHasPointer(bool has);
    void setHasKeyboard(bool has);
    void setHasTouch(bool has);

Q_SIGNALS:
    void nameChanged(const QString&);
    void hasPointerChanged(bool);
    void hasKeyboardChanged(bool);
    void hasTouchChanged(bool);

private:
    friend class Display;
    explicit SeatInterface(Display *display, QObject *parent);

    static void bind(wl_client *client, void *data, uint32_t version, uint32_t id);
    static void unbind(wl_resource *r);

    // interface
    static void getPointerCallback(wl_client *client, wl_resource *resource, uint32_t id);
    static void getKeyboardCallback(wl_client *client, wl_resource *resource, uint32_t id);
    static void getTouchCallback(wl_client *client, wl_resource *resource, uint32_t id);

    static SeatInterface *cast(wl_resource *r);
    void bind(wl_client *client, uint32_t version, uint32_t id);
    void sendCapabilities(wl_resource *r);
    void sendName(wl_resource *r);

    Display *m_display;
    wl_global *m_seat;
    QString m_name;
    bool m_pointer;
    bool m_keyboard;
    bool m_touch;
    QList<wl_resource*> m_resources;
    PointerInterface *m_pointerInterface;
    KeyboardInterface *m_keyboardInterface;
    static const struct wl_seat_interface s_interface;
};

class PointerInterface : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QPoint globalPos READ globalPos WRITE setGlobalPos NOTIFY globalPosChanged)
public:
    virtual ~PointerInterface();

    void createInterface(wl_client *client, wl_resource *parentResource, uint32_t id);

    void updateTimestamp(quint32 time);
    void setGlobalPos(const QPoint &pos);
    const QPoint &globalPos() const {
        return m_globalPos;
    }
    void buttonPressed(quint32 button);
    void buttonReleased(quint32 button);
    bool isButtonPressed(quint32 button) const;
    quint32 buttonSerial(quint32 button) const;
    void axis(Qt::Orientation orientation, quint32 delta);

    void setFocusedSurface(SurfaceInterface *surface, const QPoint &surfacePosition = QPoint());
    void setFocusedSurfacePosition(const QPoint &surfacePosition);
    SurfaceInterface *focusedSurface() const {
        return m_focusedSurface.surface;
    }
    const QPoint &focusedSurfacePosition() const {
        return m_focusedSurface.offset;
    }

Q_SIGNALS:
    void globalPosChanged(const QPoint &pos);

private:
    friend class SeatInterface;
    explicit PointerInterface(Display *display, SeatInterface *parent);
    wl_resource *pointerForSurface(SurfaceInterface *surface) const;
    void surfaceDeleted();
    void updateButtonSerial(quint32 button, quint32 serial);
    enum class ButtonState {
        Released,
        Pressed
    };
    void updateButtonState(quint32 button, ButtonState state);

    static PointerInterface *cast(wl_resource *resource) {
        return reinterpret_cast<PointerInterface*>(wl_resource_get_user_data(resource));
    }

    static void unbind(wl_resource *resource);
    // interface
    static void setCursorCallback(wl_client *client, wl_resource *resource, uint32_t serial,
                                  wl_resource *surface, int32_t hotspot_x, int32_t hotspot_y);
    // since version 3
    static void releaseCallback(wl_client *client, wl_resource *resource);

    Display *m_display;
    SeatInterface *m_seat;
    struct ResourceData {
        wl_client *client = nullptr;
        wl_resource *pointer = nullptr;
    };
    QList<ResourceData> m_resources;
    quint32 m_eventTime;
    QPoint m_globalPos;
    struct FocusedSurface {
        SurfaceInterface *surface = nullptr;
        QPoint offset = QPoint();
        wl_resource *pointer = nullptr;
        quint32 serial = 0;
    };
    FocusedSurface m_focusedSurface;
    QHash<quint32, quint32> m_buttonSerials;
    QHash<quint32, ButtonState> m_buttonStates;

    static const struct wl_pointer_interface s_interface;
};

class KeyboardInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~KeyboardInterface();

    void createInterfae(wl_client *client, wl_resource *parentResource, uint32_t id);

    void updateTimestamp(quint32 time);
    void setKeymap(int fd, quint32 size);
    void keyPressed(quint32 key);
    void keyReleased(quint32 key);
    void updateModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group);

    void setFocusedSurface(SurfaceInterface *surface);
    SurfaceInterface *focusedSurface() const {
        return m_focusedSurface.surface;
    }

private:
    friend class SeatInterface;
    explicit KeyboardInterface(Display *display, SeatInterface *parent);
    void surfaceDeleted();
    wl_resource *keyboardForSurface(SurfaceInterface *surface) const;
    void sendKeymap(wl_resource *r);
    void sendKeymapToAll();
    void sendModifiers(wl_resource *r);
    enum class KeyState {
        Released,
        Pressed
    };
    void updateKey(quint32 key, KeyState state);

    static KeyboardInterface *cast(wl_resource *resource) {
        return reinterpret_cast<KeyboardInterface*>(wl_resource_get_user_data(resource));
    }

    static void unbind(wl_resource *resource);
    // since version 3
    static void releaseCallback(wl_client *client, wl_resource *resource);

    Display *m_display;
    SeatInterface *m_seat;
    struct ResourceData {
        wl_client *client = nullptr;
        wl_resource *keyboard = nullptr;
    };
    QList<ResourceData> m_resources;
    struct Keymap {
        int fd = -1;
        quint32 size = 0;
        bool xkbcommonCompatible = false;
    };
    Keymap m_keymap;
    struct Modifiers {
        quint32 depressed = 0;
        quint32 latched = 0;
        quint32 locked = 0;
        quint32 group = 0;
    };
    Modifiers m_modifiers;
    struct FocusedSurface {
        SurfaceInterface *surface = nullptr;
        wl_resource *keyboard = nullptr;
    };
    FocusedSurface m_focusedSurface;
    QHash<quint32, KeyState> m_keyStates;
    quint32 m_eventTime;

    static const struct wl_keyboard_interface s_interface;
};

}
}

#endif
