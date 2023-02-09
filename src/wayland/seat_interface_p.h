/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

// KWayland
#include "seat_interface.h"
// Qt
#include <QHash>
#include <QMap>
#include <QPointer>
#include <QVector>

#include <optional>

#include "qwayland-server-wayland.h"

namespace KWaylandServer
{
class AbstractDataSource;
class DataDeviceInterface;
class DataSourceInterface;
class DataControlDeviceV1Interface;
class TextInputV1Interface;
class TextInputV2Interface;
class TextInputV3Interface;
class PrimarySelectionDeviceV1Interface;
class DragAndDropIcon;

class SeatInterfacePrivate : public QtWaylandServer::wl_seat
{
public:
    // exported for unit tests
    KWIN_EXPORT static SeatInterfacePrivate *get(SeatInterface *seat);
    SeatInterfacePrivate(SeatInterface *q, Display *display);

    void sendCapabilities();
    QVector<DataDeviceInterface *> dataDevicesForSurface(SurfaceInterface *surface) const;
    void registerPrimarySelectionDevice(PrimarySelectionDeviceV1Interface *primarySelectionDevice);
    void registerDataDevice(DataDeviceInterface *dataDevice);
    void registerDataControlDevice(DataControlDeviceV1Interface *dataDevice);
    void endDrag();
    void cancelDrag();

    SeatInterface *q;
    QPointer<Display> display;
    QString name;
    std::chrono::milliseconds timestamp = std::chrono::milliseconds::zero();
    quint32 capabilities = 0;
    std::unique_ptr<KeyboardInterface> keyboard;
    std::unique_ptr<PointerInterface> pointer;
    std::unique_ptr<TouchInterface> touch;
    QVector<DataDeviceInterface *> dataDevices;
    QVector<PrimarySelectionDeviceV1Interface *> primarySelectionDevices;
    QVector<DataControlDeviceV1Interface *> dataControlDevices;

    QPointer<TextInputV1Interface> textInputV1;
    // TextInput v2
    QPointer<TextInputV2Interface> textInputV2;
    QPointer<TextInputV3Interface> textInputV3;

    SurfaceInterface *focusedTextInputSurface = nullptr;
    QMetaObject::Connection focusedSurfaceDestroyConnection;

    // the last thing copied into the clipboard content
    AbstractDataSource *currentSelection = nullptr;
    AbstractDataSource *currentPrimarySelection = nullptr;

    // Pointer related members
    struct Pointer
    {
        enum class State {
            Released,
            Pressed,
        };
        QHash<quint32, quint32> buttonSerials;
        QHash<quint32, State> buttonStates;
        QPointF pos;
        struct Focus
        {
            SurfaceInterface *surface = nullptr;
            QMetaObject::Connection destroyConnection;
            QPointF offset = QPointF();
            QMatrix4x4 transformation;
            quint32 serial = 0;
        };
        Focus focus;
    };
    Pointer globalPointer;
    void updatePointerButtonSerial(quint32 button, quint32 serial);
    void updatePointerButtonState(quint32 button, Pointer::State state);

    // Keyboard related members
    struct Keyboard
    {
        struct Focus
        {
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
    struct Touch
    {
        struct Focus
        {
            SurfaceInterface *surface = nullptr;
            QMetaObject::Connection destroyConnection;
            QPointF offset = QPointF();
            QPointF firstTouchPos;
            QMatrix4x4 transformation;
        };
        Focus focus;
        QMap<qint32, quint32> ids;
    };
    Touch globalTouch;

    struct Drag
    {
        enum class Mode {
            None,
            Pointer,
            Touch,
        };
        Mode mode = Mode::None;
        AbstractDataSource *source = nullptr;
        QPointer<SurfaceInterface> surface;
        QPointer<AbstractDropHandler> target;
        QPointer<DragAndDropIcon> dragIcon;
        QMatrix4x4 transformation;
        std::optional<quint32> dragImplicitGrabSerial;
        QMetaObject::Connection dragSourceDestroyConnection;
    };
    Drag drag;

protected:
    void seat_bind_resource(Resource *resource) override;
    void seat_get_pointer(Resource *resource, uint32_t id) override;
    void seat_get_keyboard(Resource *resource, uint32_t id) override;
    void seat_get_touch(Resource *resource, uint32_t id) override;
    void seat_release(Resource *resource) override;

private:
    void updateSelection(DataDeviceInterface *dataDevice);
    void updatePrimarySelection(PrimarySelectionDeviceV1Interface *primarySelectionDevice);
};

} // namespace KWaylandServer
