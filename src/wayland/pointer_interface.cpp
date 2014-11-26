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
#include "pointer_interface.h"
#include "resource_p.h"
#include "seat_interface.h"
#include "display.h"
#include "surface_interface.h"
// Qt
#include <QHash>
// Wayland
#include <wayland-server.h>
// linux
#include <linux/input.h>

namespace KWayland
{

namespace Server
{

class PointerInterface::Private : public Resource::Private
{
public:
    Private(SeatInterface *parent, wl_resource *parentResource, PointerInterface *q);
    void updateButtonSerial(quint32 button, quint32 serial);
    enum class ButtonState {
        Released,
        Pressed
    };
    void updateButtonState(quint32 button, ButtonState state);

    SeatInterface *seat;
    SurfaceInterface *focusedSurface = nullptr;
    QHash<quint32, quint32> buttonSerials;
    QHash<quint32, ButtonState> buttonStates;
    QMetaObject::Connection destroyConnection;

private:
    // interface
    static void setCursorCallback(wl_client *client, wl_resource *resource, uint32_t serial,
                                  wl_resource *surface, int32_t hotspot_x, int32_t hotspot_y);
    // since version 3
    static void releaseCallback(wl_client *client, wl_resource *resource);

    static const struct wl_pointer_interface s_interface;
};

PointerInterface::Private::Private(SeatInterface *parent, wl_resource *parentResource, PointerInterface *q)
    : Resource::Private(q, parent, parentResource, &wl_pointer_interface, &s_interface)
    , seat(parent)
{
}


const struct wl_pointer_interface PointerInterface::Private::s_interface = {
    setCursorCallback,
    releaseCallback
};

PointerInterface::PointerInterface(SeatInterface *parent, wl_resource *parentResource)
    : Resource(new Private(parent, parentResource, this), parent)
{
    connect(parent, &SeatInterface::pointerPosChanged, [this] {
        Q_D();
        if (d->focusedSurface) {
            const QPointF pos = d->seat->pointerPos() - d->seat->focusedPointerSurfacePosition();
            wl_pointer_send_motion(d->resource, d->seat->timestamp(),
                                   wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
        }
    });
    connect(parent, &SeatInterface::pointerPosChanged, this, &PointerInterface::globalPosChanged);
}

PointerInterface::~PointerInterface() = default;

void PointerInterface::setFocusedSurface(SurfaceInterface *surface, quint32 serial)
{
    Q_D();
    Q_ASSERT(d->resource);
    if (d->focusedSurface) {
        wl_pointer_send_leave(d->resource, serial, d->focusedSurface->resource());
        disconnect(d->destroyConnection);
    }
    if (!surface) {
        d->focusedSurface = nullptr;
        return;
    }
    d->focusedSurface = surface;
    d->destroyConnection = connect(d->focusedSurface, &QObject::destroyed, this,
        [this] {
            Q_D();
            d->focusedSurface = nullptr;
        }
    );

    const QPointF pos = d->seat->pointerPos() - d->seat->focusedPointerSurfacePosition();
    wl_pointer_send_enter(d->resource, serial,
                          d->focusedSurface->resource(),
                          wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
}

void PointerInterface::setGlobalPos(const QPointF &pos)
{
    Q_D();
    d->seat->setPointerPos(pos);
}

static quint32 qtToWaylandButton(Qt::MouseButton button)
{
    static const QHash<Qt::MouseButton, quint32> s_buttons({
        {Qt::LeftButton, BTN_LEFT},
        {Qt::RightButton, BTN_RIGHT},
        {Qt::MiddleButton, BTN_MIDDLE},
        {Qt::ExtraButton1, BTN_BACK},    // note: QtWayland maps BTN_SIDE
        {Qt::ExtraButton2, BTN_FORWARD}, // note: QtWayland maps BTN_EXTRA
        {Qt::ExtraButton3, BTN_TASK},    // note: QtWayland maps BTN_FORWARD
        {Qt::ExtraButton4, BTN_EXTRA},   // note: QtWayland maps BTN_BACK
        {Qt::ExtraButton5, BTN_SIDE},    // note: QtWayland maps BTN_TASK
        {Qt::ExtraButton6, BTN_TASK + 1},
        {Qt::ExtraButton7, BTN_TASK + 2},
        {Qt::ExtraButton8, BTN_TASK + 3},
        {Qt::ExtraButton9, BTN_TASK + 4},
        {Qt::ExtraButton10, BTN_TASK + 5},
        {Qt::ExtraButton11, BTN_TASK + 6},
        {Qt::ExtraButton12, BTN_TASK + 7},
        {Qt::ExtraButton13, BTN_TASK + 8}
        // further mapping not possible, 0x120 is BTN_JOYSTICK
    });
    return s_buttons.value(button, 0);
};

void PointerInterface::buttonPressed(quint32 button)
{
    Q_D();
    const quint32 serial = d->seat->display()->nextSerial();
    d->updateButtonSerial(button, serial);
    d->updateButtonState(button, Private::ButtonState::Pressed);
    if (!d->focusedSurface) {
        return;
    }
    wl_pointer_send_button(d->resource, serial, d->seat->timestamp(), button, WL_POINTER_BUTTON_STATE_PRESSED);
}

void PointerInterface::buttonPressed(Qt::MouseButton button)
{
    const quint32 nativeButton = qtToWaylandButton(button);
    if (nativeButton == 0) {
        return;
    }
    buttonPressed(nativeButton);
}

void PointerInterface::buttonReleased(quint32 button)
{
    Q_D();
    const quint32 serial = d->seat->display()->nextSerial();
    d->updateButtonSerial(button, serial);
    d->updateButtonState(button, Private::ButtonState::Released);
    if (!d->focusedSurface) {
        return;
    }
    wl_pointer_send_button(d->resource, serial, d->seat->timestamp(), button, WL_POINTER_BUTTON_STATE_RELEASED);
}

void PointerInterface::buttonReleased(Qt::MouseButton button)
{
    const quint32 nativeButton = qtToWaylandButton(button);
    if (nativeButton == 0) {
        return;
    }
    buttonReleased(nativeButton);
}

void PointerInterface::Private::updateButtonSerial(quint32 button, quint32 serial)
{
    auto it = buttonSerials.find(button);
    if (it == buttonSerials.end()) {
        buttonSerials.insert(button, serial);
        return;
    }
    it.value() = serial;
}

quint32 PointerInterface::buttonSerial(quint32 button) const
{
    Q_D();
    auto it = d->buttonSerials.constFind(button);
    if (it == d->buttonSerials.constEnd()) {
        return 0;
    }
    return it.value();
}

quint32 PointerInterface::buttonSerial(Qt::MouseButton button) const
{
    return buttonSerial(qtToWaylandButton(button));
}

void PointerInterface::Private::updateButtonState(quint32 button, ButtonState state)
{
    auto it = buttonStates.find(button);
    if (it == buttonStates.end()) {
        buttonStates.insert(button, state);
        return;
    }
    it.value() = state;
}

bool PointerInterface::isButtonPressed(quint32 button) const
{
    Q_D();
    auto it = d->buttonStates.constFind(button);
    if (it == d->buttonStates.constEnd()) {
        return false;
    }
    return it.value() == Private::ButtonState::Pressed ? true : false;
}

bool PointerInterface::isButtonPressed(Qt::MouseButton button) const
{
    const quint32 nativeButton = qtToWaylandButton(button);
    if (nativeButton == 0) {
        return false;
    }
    return isButtonPressed(nativeButton);
}

void PointerInterface::axis(Qt::Orientation orientation, quint32 delta)
{
    Q_D();
    if (!d->focusedSurface) {
        return;
    }
    wl_pointer_send_axis(d->resource, d->seat->timestamp(),
                         (orientation == Qt::Vertical) ? WL_POINTER_AXIS_VERTICAL_SCROLL : WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                         wl_fixed_from_int(delta));
}

void PointerInterface::Private::setCursorCallback(wl_client *client, wl_resource *resource, uint32_t serial,
                                         wl_resource *surface, int32_t hotspot_x, int32_t hotspot_y)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    Q_UNUSED(serial)
    Q_UNUSED(surface)
    Q_UNUSED(hotspot_x)
    Q_UNUSED(hotspot_y)
    // TODO: implement
}

void PointerInterface::Private::releaseCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    unbind(resource);
}

QPointF PointerInterface::globalPos() const
{
    Q_D();
    return d->seat->pointerPos();
}

PointerInterface::Private *PointerInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
}
