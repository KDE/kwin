/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_SEAT_INTERFACE_P_H
#define WAYLAND_SERVER_SEAT_INTERFACE_P_H
// KWayland
#include "seat_interface.h"
#include "global_p.h"
// Qt
#include <QHash>
#include <QMap>
#include <QPointer>
#include <QVector>
// Wayland
#include <wayland-server.h>

namespace KWaylandServer
{

class AbstractDataSource;
class DataDeviceInterface;
class DataSourceInterface;
class DataControlDeviceV1Interface;
class TextInputV2Interface;
class TextInputV3Interface;
class PrimarySelectionDeviceV1Interface;

class SeatInterface::Private : public Global::Private
{
public:
    Private(SeatInterface *q, Display *d);
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    void sendCapabilities(wl_resource *r);
    void sendName(wl_resource *r);
    QVector<PointerInterface *> pointersForSurface(SurfaceInterface *surface) const;
    QVector<KeyboardInterface *> keyboardsForSurface(SurfaceInterface *surface) const;
    QVector<TouchInterface *> touchsForSurface(SurfaceInterface *surface) const;
    QVector<DataDeviceInterface *> dataDevicesForSurface(SurfaceInterface *surface) const;
    void registerPrimarySelectionDevice(PrimarySelectionDeviceV1Interface *primarySelectionDevice);
    void registerDataDevice(DataDeviceInterface *dataDevice);
    void registerDataControlDevice(DataControlDeviceV1Interface *dataDevice);
    void endDrag(quint32 serial);
    void cancelDrag(quint32 serial);
    quint32 nextSerial() const;

    QString name;
    bool pointer = false;
    bool touch = false;
    QList<wl_resource*> resources;
    quint32 timestamp = 0;
    QVector<PointerInterface*> pointers;
    QScopedPointer<KeyboardInterface> keyboard;
    QVector<TouchInterface*> touchs;
    QVector<DataDeviceInterface*> dataDevices;
    QVector<PrimarySelectionDeviceV1Interface*> primarySelectionDevices;
    QVector<DataControlDeviceV1Interface*> dataControlDevices;

    // TextInput v2
    QPointer<TextInputV2Interface> textInputV2;
    QPointer<TextInputV3Interface> textInputV3;

    SurfaceInterface *focusedTextInputSurface = nullptr;
    QMetaObject::Connection focusedSurfaceDestroyConnection;

    // the last thing copied into the clipboard content
    AbstractDataSource *currentSelection = nullptr;
    AbstractDataSource *currentPrimarySelection = nullptr;

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
        struct Focus {
            SurfaceInterface *surface = nullptr;
            QMetaObject::Connection destroyConnection;
            quint32 serial = 0;
            QVector<DataDeviceInterface *> selections;
            QVector<PrimarySelectionDeviceV1Interface *> primarySelections;
        };
        Focus focus;
    };
    Keyboard globalKeyboard;

    // Touch related members
    struct Touch {
        struct Focus {
            SurfaceInterface *surface = nullptr;
            QVector<TouchInterface*> touchs;
            QMetaObject::Connection destroyConnection;
            QPointF offset = QPointF();
            QPointF firstTouchPos;
        };
        Focus focus;
        QMap<qint32, quint32> ids;
    };
    Touch globalTouch;

    struct Drag {
        enum class Mode {
            None,
            Pointer,
            Touch
        };
        Mode mode = Mode::None;
        DataDeviceInterface *source = nullptr;
        QPointer<DataDeviceInterface> target;
        SurfaceInterface *surface = nullptr;
        PointerInterface *sourcePointer = nullptr;
        TouchInterface *sourceTouch = nullptr;
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
    void updateSelection(DataDeviceInterface *dataDevice);
    void updatePrimarySelection(PrimarySelectionDeviceV1Interface *primarySelectionDevice);
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

#endif
