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
#ifndef WAYLAND_SERVER_SEAT_INTERFACE_P_H
#define WAYLAND_SERVER_SEAT_INTERFACE_P_H
// KWayland
#include "seat_interface.h"
#include "global_p.h"
// Qt
#include <QHash>
#include <QPointer>
#include <QVector>
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

class DataDeviceInterface;
class TextInputInterface;

class SeatInterface::Private : public Global::Private
{
public:
    Private(SeatInterface *q, Display *d);
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    void sendCapabilities(wl_resource *r);
    void sendName(wl_resource *r);
    QVector<PointerInterface *> pointersForSurface(SurfaceInterface *surface) const;
    QVector<KeyboardInterface *> keyboardsForSurface(SurfaceInterface *surface) const;
    TouchInterface *touchForSurface(SurfaceInterface *surface) const;
    DataDeviceInterface *dataDeviceForSurface(SurfaceInterface *surface) const;
    TextInputInterface *textInputForSurface(SurfaceInterface *surface) const;
    void registerDataDevice(DataDeviceInterface *dataDevice);
    void registerTextInput(TextInputInterface *textInput);
    void endDrag(quint32 serial);
    void cancelPreviousSelection(DataDeviceInterface *newlySelectedDataDevice);

    QString name;
    bool pointer = false;
    bool keyboard = false;
    bool touch = false;
    QList<wl_resource*> resources;
    quint32 timestamp = 0;
    QVector<PointerInterface*> pointers;
    QVector<KeyboardInterface*> keyboards;
    QVector<TouchInterface*> touchs;
    QVector<DataDeviceInterface*> dataDevices;
    QVector<TextInputInterface*> textInputs;
    DataDeviceInterface *currentSelection = nullptr;

    // Pointer related members
    struct Pointer {
        enum class State {
            Released,
            Pressed
        };
        QHash<quint32, quint32> buttonSerials;
        QHash<quint32, State> buttonStates;
        QPointF pos;
        struct Focus {
            SurfaceInterface *surface = nullptr;
            QVector<PointerInterface *> pointers;
            QMetaObject::Connection destroyConnection;
            QPointF offset = QPointF();
            QMatrix4x4 transformation;
            quint32 serial = 0;
        };
        Focus focus;
        QPointer<SurfaceInterface> gestureSurface;
    };
    Pointer globalPointer;
    void updatePointerButtonSerial(quint32 button, quint32 serial);
    void updatePointerButtonState(quint32 button, Pointer::State state);

    // Keyboard related members
    struct Keyboard {
        enum class State {
            Released,
            Pressed
        };
        QHash<quint32, State> states;
        struct Keymap {
            int fd = -1;
            quint32 size = 0;
            bool xkbcommonCompatible = false;
        };
        Keymap keymap;
        struct Modifiers {
            quint32 depressed = 0;
            quint32 latched = 0;
            quint32 locked = 0;
            quint32 group = 0;
            quint32 serial = 0;
        };
        Modifiers modifiers;
        struct Focus {
            SurfaceInterface *surface = nullptr;
            QVector<KeyboardInterface*> keyboards;
            QMetaObject::Connection destroyConnection;
            quint32 serial = 0;
            DataDeviceInterface *selection = nullptr;
        };
        Focus focus;
        quint32 lastStateSerial = 0;
        struct {
            qint32 charactersPerSecond = 0;
            qint32 delay = 0;
        } keyRepeat;
    };
    Keyboard keys;
    bool updateKey(quint32 key, Keyboard::State state);

    struct TextInput {
        struct Focus {
            SurfaceInterface *surface = nullptr;
            QMetaObject::Connection destroyConnection;
            quint32 serial = 0;
            TextInputInterface *textInput = nullptr;
        };
        Focus focus;
    };
    TextInput textInput;

    struct Touch {
        struct Focus {
            SurfaceInterface *surface = nullptr;
            TouchInterface *touch = nullptr;
            QMetaObject::Connection destroyConnection;
            QPointF offset = QPointF();
        };
        Focus focus;
        QVector<qint32> ids;
    };
    Touch touchInterface;

    struct Drag {
        enum class Mode {
            None,
            Pointer,
            Touch
        };
        Mode mode = Mode::None;
        DataDeviceInterface *source = nullptr;
        DataDeviceInterface *target = nullptr;
        SurfaceInterface *surface = nullptr;
        PointerInterface *sourcePointer = nullptr;
        QMatrix4x4 transformation;
        QMetaObject::Connection destroyConnection;
        QMetaObject::Connection dragSourceDestroyConnection;
    };
    Drag drag;

    static SeatInterface *get(wl_resource *native) {
        auto s = cast(native);
        return s ? s->q : nullptr;
    }

private:
    void getPointer(wl_client *client, wl_resource *resource, uint32_t id);
    void getKeyboard(wl_client *client, wl_resource *resource, uint32_t id);
    void getTouch(wl_client *client, wl_resource *resource, uint32_t id);
    void updateSelection(DataDeviceInterface *dataDevice, bool set);
    static Private *cast(wl_resource *r);
    static void unbind(wl_resource *r);

    // interface
    static void getPointerCallback(wl_client *client, wl_resource *resource, uint32_t id);
    static void getKeyboardCallback(wl_client *client, wl_resource *resource, uint32_t id);
    static void getTouchCallback(wl_client *client, wl_resource *resource, uint32_t id);
    static void releaseCallback(wl_client *client, wl_resource *resource);
    static const struct wl_seat_interface s_interface;
    static const quint32 s_version;
    static const qint32 s_pointerVersion;
    static const qint32 s_touchVersion;
    static const qint32 s_keyboardVersion;

    SeatInterface *q;
};

}
}

#endif
