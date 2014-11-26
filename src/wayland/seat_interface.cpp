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
#include "seat_interface.h"
#include "global_p.h"
#include "display.h"
#include "keyboard_interface.h"
#include "pointer_interface.h"
#include "surface_interface.h"
// Qt
#include <QHash>
#include <QVector>
// Wayland
#include <wayland-server.h>
#ifndef WL_SEAT_NAME_SINCE_VERSION
#define WL_SEAT_NAME_SINCE_VERSION 2
#endif
// linux
#include <linux/input.h>

namespace KWayland
{

namespace Server
{

static const quint32 s_version = 3;

class SeatInterface::Private : public Global::Private
{
public:
    Private(SeatInterface *q, Display *d);
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    void sendCapabilities(wl_resource *r);
    void sendName(wl_resource *r);
    PointerInterface *pointerForSurface(SurfaceInterface *surface) const;
    KeyboardInterface *keyboardForSurface(SurfaceInterface *surface) const;

    QString name;
    bool pointer = false;
    bool keyboard = false;
    bool touch = false;
    QList<wl_resource*> resources;
    struct FocusedPointer {
        SurfaceInterface *surface = nullptr;
        PointerInterface *pointer = nullptr;
        QMetaObject::Connection destroyConnection;
        QPointF offset = QPointF();
        quint32 serial = 0;
    };
    FocusedPointer focusedPointer = FocusedPointer();
    quint32 timestamp = 0;
    QVector<PointerInterface*> pointers;
    QVector<KeyboardInterface*> keyboards;

    // Pointer related members
    struct Pointer {
        enum class ButtonState {
            Released,
            Pressed
        };
        QHash<quint32, quint32> buttonSerials;
        QHash<quint32, ButtonState> buttonStates;
        QPointF pos;
    };
    Pointer globalPointer;
    void updatePointerButtonSerial(quint32 button, quint32 serial);
    void updatePointerButtonState(quint32 button, Pointer::ButtonState state);

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
            KeyboardInterface *keyboard = nullptr;
            QMetaObject::Connection destroyConnection;
            quint32 serial = 0;
        };
        Focus focus;
        quint32 lastStateSerial = 0;
    };
    Keyboard keys;
    void updateKey(quint32 key, Keyboard::State state);

    static SeatInterface *get(wl_resource *native) {
        auto s = cast(native);
        return s ? s->q : nullptr;
    }

private:
    void getPointer(wl_client *client, wl_resource *resource, uint32_t id);
    void getKeyboard(wl_client *client, wl_resource *resource, uint32_t id);
    static Private *cast(wl_resource *r);
    static void unbind(wl_resource *r);

    // interface
    static void getPointerCallback(wl_client *client, wl_resource *resource, uint32_t id);
    static void getKeyboardCallback(wl_client *client, wl_resource *resource, uint32_t id);
    static void getTouchCallback(wl_client *client, wl_resource *resource, uint32_t id);
    static const struct wl_seat_interface s_interface;

    SeatInterface *q;
};

SeatInterface::Private::Private(SeatInterface *q, Display *display)
    : Global::Private(display, &wl_seat_interface, s_version)
    , q(q)
{
}

const struct wl_seat_interface SeatInterface::Private::s_interface = {
    getPointerCallback,
    getKeyboardCallback,
    getTouchCallback
};

SeatInterface::SeatInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
    Q_D();
    connect(this, &SeatInterface::nameChanged, this,
        [this, d] {
            for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
                d->sendName(*it);
            }
        }
    );
    auto sendCapabilitiesAll = [this, d] {
        for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
            d->sendCapabilities(*it);
        }
    };
    connect(this, &SeatInterface::hasPointerChanged,  this, sendCapabilitiesAll);
    connect(this, &SeatInterface::hasKeyboardChanged, this, sendCapabilitiesAll);
    connect(this, &SeatInterface::hasTouchChanged,    this, sendCapabilitiesAll);
}

SeatInterface::~SeatInterface()
{
    Q_D();
    while (!d->resources.isEmpty()) {
        wl_resource_destroy(d->resources.takeLast());
    }
}

void SeatInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    wl_resource *r = wl_resource_create(client, &wl_seat_interface, qMin(s_version, version), id);
    if (!r) {
        wl_client_post_no_memory(client);
        return;
    }
    resources << r;

    wl_resource_set_implementation(r, &s_interface, this, unbind);

    sendCapabilities(r);
    sendName(r);
}

void SeatInterface::Private::unbind(wl_resource *r)
{
    cast(r)->resources.removeAll(r);
}

void SeatInterface::Private::updatePointerButtonSerial(quint32 button, quint32 serial)
{
    auto it = globalPointer.buttonSerials.find(button);
    if (it == globalPointer.buttonSerials.end()) {
        globalPointer.buttonSerials.insert(button, serial);
        return;
    }
    it.value() = serial;
}

void SeatInterface::Private::updatePointerButtonState(quint32 button, Pointer::ButtonState state)
{
    auto it = globalPointer.buttonStates.find(button);
    if (it == globalPointer.buttonStates.end()) {
        globalPointer.buttonStates.insert(button, state);
        return;
    }
    it.value() = state;
}

void SeatInterface::Private::updateKey(quint32 key, Keyboard::State state)
{
    auto it = keys.states.find(key);
    if (it == keys.states.end()) {
        keys.states.insert(key, state);
        return;
    }
    it.value() = state;
}

void SeatInterface::Private::sendName(wl_resource *r)
{
    if (wl_resource_get_version(r) < WL_SEAT_NAME_SINCE_VERSION) {
        return;
    }
    wl_seat_send_name(r, name.toUtf8().constData());
}

void SeatInterface::Private::sendCapabilities(wl_resource *r)
{
    uint32_t capabilities = 0;
    if (pointer) {
        capabilities |= WL_SEAT_CAPABILITY_POINTER;
    }
    if (keyboard) {
        capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    if (touch) {
        capabilities |= WL_SEAT_CAPABILITY_TOUCH;
    }
    wl_seat_send_capabilities(r, capabilities);
}

SeatInterface::Private *SeatInterface::Private::cast(wl_resource *r)
{
    return r ? reinterpret_cast<SeatInterface::Private*>(wl_resource_get_user_data(r)) : nullptr;
}

PointerInterface *SeatInterface::Private::pointerForSurface(SurfaceInterface *surface) const
{
    if (!surface) {
        return nullptr;
    }

    for (auto it = pointers.begin(); it != pointers.end(); ++it) {
        if (wl_resource_get_client((*it)->resource()) == *surface->client()) {
            return (*it);
        }
    }
    return nullptr;
}

KeyboardInterface *SeatInterface::Private::keyboardForSurface(SurfaceInterface *surface) const
{
    if (!surface) {
        return nullptr;
    }

    for (auto it = keyboards.begin(); it != keyboards.end(); ++it) {
        if (wl_resource_get_client((*it)->resource()) == *surface->client()) {
            return (*it);
        }
    }
    return nullptr;
}

void SeatInterface::setHasKeyboard(bool has)
{
    Q_D();
    if (d->keyboard == has) {
        return;
    }
    d->keyboard = has;
    emit hasKeyboardChanged(d->keyboard);
}

void SeatInterface::setHasPointer(bool has)
{
    Q_D();
    if (d->pointer == has) {
        return;
    }
    d->pointer = has;
    emit hasPointerChanged(d->pointer);
}

void SeatInterface::setHasTouch(bool has)
{
    Q_D();
    if (d->touch == has) {
        return;
    }
    d->touch = has;
    emit hasTouchChanged(d->touch);
}

void SeatInterface::setName(const QString &name)
{
    Q_D();
    if (d->name == name) {
        return;
    }
    d->name = name;
    emit nameChanged(d->name);
}

void SeatInterface::Private::getPointerCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    cast(resource)->getPointer(client, resource, id);
}

void SeatInterface::Private::getPointer(wl_client *client, wl_resource *resource, uint32_t id)
{
    // TODO: only create if seat has pointer?
    PointerInterface *pointer = new PointerInterface(q, resource);
    pointer->create(display->getConnection(client), wl_resource_get_version(resource), id);
    if (!pointer->resource()) {
        wl_resource_post_no_memory(resource);
        delete pointer;
        return;
    }
    pointers << pointer;
    if (focusedPointer.surface && focusedPointer.surface->client()->client() == client) {
        // this is a pointer for the currently focused pointer surface
        if (!focusedPointer.pointer) {
            focusedPointer.pointer = pointer;
            pointer->setFocusedSurface(focusedPointer.surface, focusedPointer.serial);
        }
    }
    QObject::connect(pointer, &QObject::destroyed, q,
        [pointer,this] {
            pointers.removeAt(pointers.indexOf(pointer));
            if (focusedPointer.pointer == pointer) {
                focusedPointer.pointer = nullptr;
            }
        }
    );
    emit q->pointerCreated(pointer);
}

void SeatInterface::Private::getKeyboardCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    cast(resource)->getKeyboard(client, resource, id);
}

void SeatInterface::Private::getKeyboard(wl_client *client, wl_resource *resource, uint32_t id)
{
    // TODO: only create if seat has keyboard?
    KeyboardInterface *keyboard = new KeyboardInterface(q);
    keyboard->createInterfae(client, resource, id);
    keyboards << keyboard;
    if (keys.focus.surface && keys.focus.surface->client()->client() == client) {
        // this is a keyboard for the currently focused keyboard surface
        if (!keys.focus.keyboard) {
            keys.focus.keyboard = keyboard;
            keyboard->setFocusedSurface(keys.focus.surface, keys.focus.serial);
        }
    }
    QObject::connect(keyboard, &QObject::destroyed, q,
        [keyboard,this] {
            keyboards.removeAt(keyboards.indexOf(keyboard));
            if (keys.focus.keyboard == keyboard) {
                keys.focus.keyboard = nullptr;
            }
        }
    );
    emit q->keyboardCreated(keyboard);
}

void SeatInterface::Private::getTouchCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    Q_UNUSED(id)
}

QString SeatInterface::name() const
{
    Q_D();
    return d->name;
}

bool SeatInterface::hasPointer() const
{
    Q_D();
    return d->pointer;
}

bool SeatInterface::hasKeyboard() const
{
    Q_D();
    return d->keyboard;
}

bool SeatInterface::hasTouch() const
{
    Q_D();
    return d->touch;
}

SeatInterface *SeatInterface::get(wl_resource *native)
{
    return Private::get(native);
}

SeatInterface::Private *SeatInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

QPointF SeatInterface::pointerPos() const
{
    Q_D();
    return d->globalPointer.pos;
}

void SeatInterface::setPointerPos(const QPointF &pos)
{
    Q_D();
    if (d->globalPointer.pos == pos) {
        return;
    }
    d->globalPointer.pos = pos;
    emit pointerPosChanged(pos);
}

quint32 SeatInterface::timestamp() const
{
    Q_D();
    return d->timestamp;
}

void SeatInterface::setTimestamp(quint32 time)
{
    Q_D();
    if (d->timestamp == time) {
        return;
    }
    d->timestamp = time;
    emit timestampChanged(time);
}

SurfaceInterface *SeatInterface::focusedPointerSurface() const
{
    Q_D();
    return d->focusedPointer.surface;
}

void SeatInterface::setFocusedPointerSurface(SurfaceInterface *surface, const QPointF &surfacePosition)
{
    Q_D();
    const quint32 serial = d->display->nextSerial();
    if (d->focusedPointer.pointer) {
        d->focusedPointer.pointer->setFocusedSurface(nullptr, serial);
    }
    if (d->focusedPointer.surface) {
        disconnect(d->focusedPointer.destroyConnection);
    }
    d->focusedPointer = Private::FocusedPointer();
    d->focusedPointer.surface = surface;
    PointerInterface *p = d->pointerForSurface(surface);
    d->focusedPointer.pointer = p;
    if (d->focusedPointer.surface) {
        d->focusedPointer.destroyConnection = connect(surface, &QObject::destroyed, this,
            [this] {
                Q_D();
                d->focusedPointer = Private::FocusedPointer();
            }
        );
        d->focusedPointer.offset = surfacePosition;
        d->focusedPointer.serial = serial;
    }
    if (!p) {
        return;
    }
    p->setFocusedSurface(surface, serial);
}

PointerInterface *SeatInterface::focusedPointer() const
{
    Q_D();
    return d->focusedPointer.pointer;
}

void SeatInterface::setFocusedPointerSurfacePosition(const QPointF &surfacePosition)
{
    Q_D();
    if (d->focusedPointer.surface) {
        d->focusedPointer.offset = surfacePosition;
    }
}

QPointF SeatInterface::focusedPointerSurfacePosition() const
{
    Q_D();
    return d->focusedPointer.offset;
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

bool SeatInterface::isPointerButtonPressed(Qt::MouseButton button) const
{
    return isPointerButtonPressed(qtToWaylandButton(button));
}

bool SeatInterface::isPointerButtonPressed(quint32 button) const
{
    Q_D();
    auto it = d->globalPointer.buttonStates.constFind(button);
    if (it == d->globalPointer.buttonStates.constEnd()) {
        return false;
    }
    return it.value() == Private::Pointer::ButtonState::Pressed ? true : false;
}

void SeatInterface::pointerAxis(Qt::Orientation orientation, quint32 delta)
{
    Q_D();
    if (d->focusedPointer.pointer && d->focusedPointer.surface) {
        d->focusedPointer.pointer->axis(orientation, delta);
    }
}

void SeatInterface::pointerButtonPressed(Qt::MouseButton button)
{
    const quint32 nativeButton = qtToWaylandButton(button);
    if (nativeButton == 0) {
        return;
    }
    pointerButtonPressed(nativeButton);
}

void SeatInterface::pointerButtonPressed(quint32 button)
{
    Q_D();
    const quint32 serial = d->display->nextSerial();
    d->updatePointerButtonSerial(button, serial);
    d->updatePointerButtonState(button, Private::Pointer::ButtonState::Pressed);
    if (d->focusedPointer.pointer && d->focusedPointer.surface) {
        d->focusedPointer.pointer->buttonPressed(button, serial);
    }
}

void SeatInterface::pointerButtonReleased(Qt::MouseButton button)
{
    const quint32 nativeButton = qtToWaylandButton(button);
    if (nativeButton == 0) {
        return;
    }
    pointerButtonReleased(nativeButton);
}

void SeatInterface::pointerButtonReleased(quint32 button)
{
    Q_D();
    const quint32 serial = d->display->nextSerial();
    d->updatePointerButtonSerial(button, serial);
    d->updatePointerButtonState(button, Private::Pointer::ButtonState::Released);
    if (d->focusedPointer.pointer && d->focusedPointer.surface) {
        d->focusedPointer.pointer->buttonReleased(button, serial);
    }
}

quint32 SeatInterface::pointerButtonSerial(Qt::MouseButton button) const
{
    return pointerButtonSerial(qtToWaylandButton(button));
}

quint32 SeatInterface::pointerButtonSerial(quint32 button) const
{
    Q_D();
    auto it = d->globalPointer.buttonSerials.constFind(button);
    if (it == d->globalPointer.buttonSerials.constEnd()) {
        return 0;
    }
    return it.value();
}

void SeatInterface::keyPressed(quint32 key)
{
    Q_D();
    d->keys.lastStateSerial = d->display->nextSerial();
    d->updateKey(key, Private::Keyboard::State::Pressed);
    if (d->keys.focus.keyboard && d->keys.focus.surface) {
        d->keys.focus.keyboard->keyPressed(key, d->keys.lastStateSerial);
    }
}

void SeatInterface::keyReleased(quint32 key)
{
    Q_D();
    d->keys.lastStateSerial = d->display->nextSerial();
    d->updateKey(key, Private::Keyboard::State::Released);
    if (d->keys.focus.keyboard && d->keys.focus.surface) {
        d->keys.focus.keyboard->keyReleased(key, d->keys.lastStateSerial);
    }
}

SurfaceInterface *SeatInterface::focusedKeyboardSurface() const
{
    Q_D();
    return d->keys.focus.surface;
}

void SeatInterface::setFocusedKeyboardSurface(SurfaceInterface *surface)
{
    Q_D();
    const quint32 serial = d->display->nextSerial();
    if (d->keys.focus.keyboard) {
        d->keys.focus.keyboard->setFocusedSurface(nullptr, serial);
    }
    if (d->keys.focus.surface) {
        disconnect(d->keys.focus.destroyConnection);
    }
    d->keys.focus = Private::Keyboard::Focus();
    d->keys.focus.surface = surface;
    KeyboardInterface *k = d->keyboardForSurface(surface);
    d->keys.focus.keyboard = k;
    if (d->keys.focus.surface) {
        d->keys.focus.destroyConnection = connect(surface, &QObject::destroyed, this,
            [this] {
                Q_D();
                d->keys.focus = Private::Keyboard::Focus();
            }
        );
        d->keys.focus.serial = serial;
    }
    if (!k) {
        return;
    }
    k->setFocusedSurface(surface, serial);
}

void SeatInterface::setKeymap(int fd, quint32 size)
{
    Q_D();
    d->keys.keymap.xkbcommonCompatible = true;
    d->keys.keymap.fd = fd;
    d->keys.keymap.size = size;
    for (auto it = d->keyboards.constBegin(); it != d->keyboards.constEnd(); ++it) {
        (*it)->setKeymap(fd, size);
    }
}

void SeatInterface::updateKeyboardModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group)
{
    Q_D();
    const quint32 serial = d->display->nextSerial();
    d->keys.modifiers.serial = serial;
    d->keys.modifiers.depressed = depressed;
    d->keys.modifiers.latched = latched;
    d->keys.modifiers.locked = locked;
    d->keys.modifiers.group = group;
    if (d->keys.focus.keyboard && d->keys.focus.surface) {
        d->keys.focus.keyboard->updateModifiers(depressed, latched, locked, group, serial);
    }
}

bool SeatInterface::isKeymapXkbCompatible() const
{
    Q_D();
    return d->keys.keymap.xkbcommonCompatible;
}

int SeatInterface::keymapFileDescriptor() const
{
    Q_D();
    return d->keys.keymap.fd;
}

quint32 SeatInterface::keymapSize() const
{
    Q_D();
    return d->keys.keymap.size;
}

quint32 SeatInterface::depressedModifiers() const
{
    Q_D();
    return d->keys.modifiers.depressed;
}

quint32 SeatInterface::groupModifiers() const
{
    Q_D();
    return d->keys.modifiers.group;
}

quint32 SeatInterface::latchedModifiers() const
{
    Q_D();
    return d->keys.modifiers.latched;
}

quint32 SeatInterface::lockedModifiers() const
{
    Q_D();
    return d->keys.modifiers.locked;
}

quint32 SeatInterface::lastModifiersSerial() const
{
    Q_D();
    return d->keys.modifiers.serial;
}

QVector< quint32 > SeatInterface::pressedKeys() const
{
    Q_D();
    QVector<quint32> keys;
    for (auto it = d->keys.states.begin(); it != d->keys.states.end(); ++it) {
        if (it.value() == Private::Keyboard::State::Pressed) {
            keys << it.key();
        }
    }
    return keys;
}

KeyboardInterface *SeatInterface::focusedKeyboard() const
{
    Q_D();
    return d->keys.focus.keyboard;
}

}
}
