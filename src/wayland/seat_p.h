/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

// KWayland
#include "seat.h"
// Qt
#include <QHash>
#include <QList>
#include <QMap>
#include <QPointer>

#include <optional>

#include "qwayland-server-wayland.h"

namespace KWin
{
class AbstractDataSource;
class DataDeviceInterface;
class DataSourceInterface;
class DataControlDeviceV1Interface;
class TextInputV1Interface;
class TextInputV2Interface;
class TextInputV3Interface;
class PrimarySelectionDeviceV1Interface;
class PrimarySelectionSourceV1Interface;
class DragAndDropIcon;

class SeatInterfacePrivate : public QtWaylandServer::wl_seat
{
public:
    // exported for unit tests
    KWIN_EXPORT static SeatInterfacePrivate *get(SeatInterface *seat);
    SeatInterfacePrivate(SeatInterface *q, Display *display, const QString &name);

    void sendCapabilities();
    QList<DataDeviceInterface *> dataDevicesForSurface(SurfaceInterface *surface) const;
    void registerPrimarySelectionDevice(PrimarySelectionDeviceV1Interface *primarySelectionDevice);
    void registerDataDevice(DataDeviceInterface *dataDevice);
    void registerDataControlDevice(DataControlDeviceV1Interface *dataDevice);
    void endDrag();
    void cancelDrag();
    bool dragInhibitsPointer(SurfaceInterface *surface) const;

    SeatInterface *q;
    QPointer<Display> display;
    QString name;
    std::chrono::milliseconds timestamp = std::chrono::milliseconds::zero();
    quint32 capabilities = 0;
    std::unique_ptr<KeyboardInterface> keyboard;
    std::unique_ptr<PointerInterface> pointer;
    std::unique_ptr<TouchInterface> touch;
    QList<DataDeviceInterface *> dataDevices;
    QList<PrimarySelectionDeviceV1Interface *> primarySelectionDevices;
    QList<DataControlDeviceV1Interface *> dataControlDevices;

    QPointer<TextInputV1Interface> textInputV1;
    // TextInput v2
    QPointer<TextInputV2Interface> textInputV2;
    QPointer<TextInputV3Interface> textInputV3;

    SurfaceInterface *focusedTextInputSurface = nullptr;
    QMetaObject::Connection focusedSurfaceDestroyConnection;

    // the last thing copied into the clipboard content
    AbstractDataSource *currentSelection = nullptr;
    quint32 currentSelectionSerial = 0;
    AbstractDataSource *currentPrimarySelection = nullptr;
    quint32 currentPrimarySelectionSerial = 0;

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
            QList<DataDeviceInterface *> selections;
            QList<PrimarySelectionDeviceV1Interface *> primarySelections;
        };
        Focus focus;
    };
    Keyboard globalKeyboard;

    // Touch related members
    struct Touch
    {
        struct Interaction
        {
            Interaction()
            {
            }
            Q_DISABLE_COPY(Interaction)

            ~Interaction()
            {
                QObject::disconnect(destroyConnection);
            }

            SurfaceInterface *surface = nullptr;
            QMetaObject::Connection destroyConnection;
            QPointF firstTouchPos;
            uint refs = 0;
        };
        std::unordered_map<SurfaceInterface *, std::unique_ptr<Interaction>> focus;

        std::map<qint32, std::unique_ptr<TouchPoint>> ids;
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
    void updateSelection(DataSourceInterface *dataSource, quint32 serial);
    void updatePrimarySelection(PrimarySelectionSourceV1Interface *dataSource, quint32);
};

} // namespace KWin
