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

class PointerInterface::Private
{
public:
    Private(SeatInterface *parent);
    void createInterface(wl_client *client, wl_resource *parentResource, uint32_t id);
    wl_resource *pointerForSurface(SurfaceInterface *surface) const;
    void surfaceDeleted();
    void updateButtonSerial(quint32 button, quint32 serial);
    enum class ButtonState {
        Released,
        Pressed
    };
    void updateButtonState(quint32 button, ButtonState state);

    SeatInterface *seat;
    struct ResourceData {
        wl_client *client = nullptr;
        wl_resource *pointer = nullptr;
    };
    QList<ResourceData> resources;
    quint32 eventTime = 0;
    struct FocusedSurface {
        SurfaceInterface *surface = nullptr;
        QPoint offset = QPoint();
        wl_resource *pointer = nullptr;
        quint32 serial = 0;
    };
    FocusedSurface focusedSurface;
    QHash<quint32, quint32> buttonSerials;
    QHash<quint32, ButtonState> buttonStates;
    QMetaObject::Connection destroyConnection;

private:
    static PointerInterface::Private *cast(wl_resource *resource) {
        return reinterpret_cast<PointerInterface::Private*>(wl_resource_get_user_data(resource));
    }

    static void unbind(wl_resource *resource);
    // interface
    static void setCursorCallback(wl_client *client, wl_resource *resource, uint32_t serial,
                                  wl_resource *surface, int32_t hotspot_x, int32_t hotspot_y);
    // since version 3
    static void releaseCallback(wl_client *client, wl_resource *resource);

    static const struct wl_pointer_interface s_interface;
};

PointerInterface::Private::Private(SeatInterface *parent)
    : seat(parent)
{
}


const struct wl_pointer_interface PointerInterface::Private::s_interface = {
    setCursorCallback,
    releaseCallback
};

PointerInterface::PointerInterface(SeatInterface *parent)
    : QObject(parent)
    , d(new Private(parent))
{
    connect(parent, &SeatInterface::pointerPosChanged, [this](const QPointF &pos) {
        if (d->focusedSurface.surface && d->focusedSurface.pointer) {
            const QPointF pos = d->seat->pointerPos() - d->focusedSurface.offset;
            wl_pointer_send_motion(d->focusedSurface.pointer, d->eventTime,
                                   wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
        }
    });
    connect(parent, &SeatInterface::pointerPosChanged, this, &PointerInterface::globalPosChanged);
}

PointerInterface::~PointerInterface()
{
    while (!d->resources.isEmpty()) {
        auto data = d->resources.takeLast();
        wl_resource_destroy(data.pointer);
    }
}

void PointerInterface::createInterface(wl_client *client, wl_resource *parentResource, uint32_t id)
{
    d->createInterface(client, parentResource, id);
}

void PointerInterface::Private::createInterface(wl_client* client, wl_resource* parentResource, uint32_t id)
{
    wl_resource *p = wl_resource_create(client, &wl_pointer_interface, wl_resource_get_version(parentResource), id);
    if (!p) {
        wl_resource_post_no_memory(parentResource);
        return;
    }
    ResourceData data;
    data.client = client;
    data.pointer = p;
    resources << data;

    wl_resource_set_implementation(p, &s_interface, this, unbind);
}

wl_resource *PointerInterface::Private::pointerForSurface(SurfaceInterface *surface) const
{
    if (!surface) {
        return nullptr;
    }
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        if ((*it).client == *surface->client()) {
            return (*it).pointer;
        }
    }
    return nullptr;
}

void PointerInterface::setFocusedSurface(SurfaceInterface *surface, const QPoint &surfacePosition)
{
    const quint32 serial = d->seat->display()->nextSerial();
    if (d->focusedSurface.surface && d->focusedSurface.pointer) {
        wl_pointer_send_leave(d->focusedSurface.pointer, serial, d->focusedSurface.surface->resource());
        disconnect(d->destroyConnection);
    }
    d->focusedSurface.pointer = d->pointerForSurface(surface);
    if (!d->focusedSurface.pointer) {
        d->focusedSurface = Private::FocusedSurface();
        return;
    }
    d->focusedSurface.surface = surface;
    d->focusedSurface.offset = surfacePosition;
    d->focusedSurface.serial = serial;
    d->destroyConnection = connect(d->focusedSurface.surface, &QObject::destroyed, this, [this] { d->surfaceDeleted(); });

    const QPointF pos = d->seat->pointerPos() - surfacePosition;
    wl_pointer_send_enter(d->focusedSurface.pointer, d->focusedSurface.serial,
                          d->focusedSurface.surface->resource(),
                          wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
}

void PointerInterface::Private::surfaceDeleted()
{
    focusedSurface = FocusedSurface();
}

void PointerInterface::setFocusedSurfacePosition(const QPoint &surfacePosition)
{
    if (!d->focusedSurface.surface) {
        return;
    }
    d->focusedSurface.offset = surfacePosition;
}

void PointerInterface::setGlobalPos(const QPointF &pos)
{
    d->seat->setPointerPos(pos);
}

void PointerInterface::updateTimestamp(quint32 time)
{
    d->eventTime = time;
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
    const quint32 serial = d->seat->display()->nextSerial();
    d->updateButtonSerial(button, serial);
    d->updateButtonState(button, Private::ButtonState::Pressed);
    if (!d->focusedSurface.surface || !d->focusedSurface.pointer) {
        return;
    }
    wl_pointer_send_button(d->focusedSurface.pointer, serial, d->eventTime, button, WL_POINTER_BUTTON_STATE_PRESSED);
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
    const quint32 serial = d->seat->display()->nextSerial();
    d->updateButtonSerial(button, serial);
    d->updateButtonState(button, Private::ButtonState::Released);
    if (!d->focusedSurface.surface || !d->focusedSurface.pointer) {
        return;
    }
    wl_pointer_send_button(d->focusedSurface.pointer, serial, d->eventTime, button, WL_POINTER_BUTTON_STATE_RELEASED);
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
    if (!d->focusedSurface.surface || !d->focusedSurface.pointer) {
        return;
    }
    wl_pointer_send_axis(d->focusedSurface.pointer, d->eventTime,
                         (orientation == Qt::Vertical) ? WL_POINTER_AXIS_VERTICAL_SCROLL : WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                         wl_fixed_from_int(delta));
}

void PointerInterface::Private::unbind(wl_resource *resource)
{
    auto p = cast(resource);
    auto it = std::find_if(p->resources.begin(), p->resources.end(),
        [resource](const ResourceData &data) {
            return data.pointer == resource;
        }
    );
    if (it == p->resources.end()) {
        return;
    }
    if ((*it).pointer == p->focusedSurface.pointer) {
        QObject::disconnect(p->destroyConnection);
        p->focusedSurface = FocusedSurface();
    }
    p->resources.erase(it);
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
    return d->seat->pointerPos();
}

SurfaceInterface *PointerInterface::focusedSurface() const
{
    return d->focusedSurface.surface;
}

QPoint PointerInterface::focusedSurfacePosition() const
{
    return d->focusedSurface.offset;
}

}
}
