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
#include "surface_interface.h"
// Qt
#include <QHash>
// System
#include <fcntl.h>
#include <unistd.h>
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

    QString name;
    bool pointer = false;
    bool keyboard = false;
    bool touch = false;
    QList<wl_resource*> resources;
    PointerInterface *pointerInterface = nullptr;
    KeyboardInterface *keyboardInterface = nullptr;

    static SeatInterface *get(wl_resource *native) {
        auto s = cast(native);
        return s ? s->q : nullptr;
    }

private:
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
    d->pointerInterface = new PointerInterface(display, this);
    d->keyboardInterface = new KeyboardInterface(display, this);
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
    cast(resource)->pointerInterface->createInterface(client, resource, id);
}

void SeatInterface::Private::getKeyboardCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    cast(resource)->keyboardInterface->createInterfae(client, resource, id);
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

PointerInterface *SeatInterface::pointer()
{
    Q_D();
    return d->pointerInterface;
}

KeyboardInterface *SeatInterface::keyboard()
{
    Q_D();
    return d->keyboardInterface;
}

SeatInterface *SeatInterface::get(wl_resource *native)
{
    return Private::get(native);
}

SeatInterface::Private *SeatInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

/****************************************
 * PointerInterface
 ***************************************/

class PointerInterface::Private
{
public:
    Private(Display *display, SeatInterface *parent);
    void createInterface(wl_client *client, wl_resource *parentResource, uint32_t id);
    wl_resource *pointerForSurface(SurfaceInterface *surface) const;
    void surfaceDeleted();
    void updateButtonSerial(quint32 button, quint32 serial);
    enum class ButtonState {
        Released,
        Pressed
    };
    void updateButtonState(quint32 button, ButtonState state);

    Display *display;
    SeatInterface *seat;
    struct ResourceData {
        wl_client *client = nullptr;
        wl_resource *pointer = nullptr;
    };
    QList<ResourceData> resources;
    quint32 eventTime = 0;
    QPoint globalPos;
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

PointerInterface::Private::Private(Display *display, SeatInterface *parent)
    : display(display)
    , seat(parent)
{
}


const struct wl_pointer_interface PointerInterface::Private::s_interface = {
    setCursorCallback,
    releaseCallback
};

PointerInterface::PointerInterface(Display *display, SeatInterface *parent)
    : QObject(parent)
    , d(new Private(display, parent))
{
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
        if ((*it).client == surface->client()) {
            return (*it).pointer;
        }
    }
    return nullptr;
}

void PointerInterface::setFocusedSurface(SurfaceInterface *surface, const QPoint &surfacePosition)
{
    const quint32 serial = d->display->nextSerial();
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

    const QPoint pos = d->globalPos - surfacePosition;
    wl_pointer_send_enter(d->focusedSurface.pointer, d->focusedSurface.serial,
                          d->focusedSurface.surface->resource(),
                          wl_fixed_from_int(pos.x()), wl_fixed_from_int(pos.y()));
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

void PointerInterface::setGlobalPos(const QPoint &pos)
{
    if (d->globalPos == pos) {
        return;
    }
    d->globalPos = pos;
    if (d->focusedSurface.surface && d->focusedSurface.pointer) {
        const QPoint pos = d->globalPos - d->focusedSurface.offset;
        wl_pointer_send_motion(d->focusedSurface.pointer, d->eventTime,
                               wl_fixed_from_int(pos.x()), wl_fixed_from_int(pos.y()));
    }
    emit globalPosChanged(d->globalPos);
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
    const quint32 serial = d->display->nextSerial();
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
    const quint32 serial = d->display->nextSerial();
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

QPoint PointerInterface::globalPos() const
{
    return d->globalPos;
}

SurfaceInterface *PointerInterface::focusedSurface() const
{
    return d->focusedSurface.surface;
}

QPoint PointerInterface::focusedSurfacePosition() const
{
    return d->focusedSurface.offset;
}

/****************************************
 * KeyboardInterface
 ***************************************/

class KeyboardInterface::Private
{
public:
    Private(Display *d, SeatInterface *s);
    void createInterfae(wl_client *client, wl_resource *parentResource, uint32_t id);
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

    Display *display;
    SeatInterface *seat;
    struct ResourceData {
        wl_client *client = nullptr;
        wl_resource *keyboard = nullptr;
    };
    QList<ResourceData> resources;
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
    };
    Modifiers modifiers;
    struct FocusedSurface {
        SurfaceInterface *surface = nullptr;
        wl_resource *keyboard = nullptr;
    };
    FocusedSurface focusedSurface;
    QHash<quint32, KeyState> keyStates;
    quint32 eventTime = 0;
    QMetaObject::Connection destroyConnection;

private:
    static Private *cast(wl_resource *resource) {
        return reinterpret_cast<KeyboardInterface::Private*>(wl_resource_get_user_data(resource));
    }

    static void unbind(wl_resource *resource);
    // since version 3
    static void releaseCallback(wl_client *client, wl_resource *resource);

    static const struct wl_keyboard_interface s_interface;
};

KeyboardInterface::Private::Private(Display *d, SeatInterface *s)
    : display(d)
    , seat(s)
{
}

const struct wl_keyboard_interface KeyboardInterface::Private::s_interface {
    releaseCallback
};

KeyboardInterface::KeyboardInterface(Display *display, SeatInterface *parent)
    : QObject(parent)
    , d(new Private(display, parent))
{
}

KeyboardInterface::~KeyboardInterface()
{
    while (!d->resources.isEmpty()) {
        auto data = d->resources.takeLast();
        wl_resource_destroy(data.keyboard);
    }
}

void KeyboardInterface::createInterfae(wl_client *client, wl_resource *parentResource, uint32_t id)
{
    d->createInterfae(client, parentResource, id);
}

void KeyboardInterface::Private::createInterfae(wl_client *client, wl_resource *parentResource, uint32_t id)
{
    wl_resource *k = wl_resource_create(client, &wl_keyboard_interface, wl_resource_get_version(parentResource), id);
    if (!k) {
        wl_resource_post_no_memory(parentResource);
        return;
    }
    ResourceData data;
    data.client = client;
    data.keyboard = k;
    resources << data;

    wl_resource_set_implementation(k, &s_interface, this, unbind);

    sendKeymap(k);
}

void KeyboardInterface::Private::unbind(wl_resource *resource)
{
    auto k = cast(resource);
    auto it = std::find_if(k->resources.begin(), k->resources.end(),
        [resource](const ResourceData &data) {
            return data.keyboard == resource;
        }
    );
    if (it == k->resources.end()) {
        return;
    }
    if ((*it).keyboard == k->focusedSurface.keyboard) {
        QObject::disconnect(k->destroyConnection);
        k->focusedSurface = FocusedSurface();
    }
    k->resources.erase(it);
}

void KeyboardInterface::Private::surfaceDeleted()
{
    focusedSurface = FocusedSurface();
}

void KeyboardInterface::Private::releaseCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    unbind(resource);
}

void KeyboardInterface::setKeymap(int fd, quint32 size)
{
    d->keymap.xkbcommonCompatible = true;
    d->keymap.fd = fd;
    d->keymap.size = size;
    d->sendKeymapToAll();
}

void KeyboardInterface::Private::sendKeymap(wl_resource *r)
{
    if (keymap.xkbcommonCompatible) {
        wl_keyboard_send_keymap(r,
                                WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                                keymap.fd,
                                keymap.size);
    } else {
        int nullFd = open("/dev/null", O_RDONLY);
        wl_keyboard_send_keymap(r, WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP, nullFd, 0);
        close(nullFd);
    }
}

void KeyboardInterface::Private::sendKeymapToAll()
{
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        sendKeymap((*it).keyboard);
    }
}

void KeyboardInterface::Private::sendModifiers(wl_resource* r)
{
    wl_keyboard_send_modifiers(r, display->nextSerial(), modifiers.depressed, modifiers.latched, modifiers.locked, modifiers.group);
}

void KeyboardInterface::setFocusedSurface(SurfaceInterface *surface)
{
    const quint32 serial = d->display->nextSerial();
    if (d->focusedSurface.surface && d->focusedSurface.keyboard) {
        wl_keyboard_send_leave(d->focusedSurface.keyboard, serial, d->focusedSurface.surface->resource());
        disconnect(d->destroyConnection);
    }
    d->focusedSurface.keyboard = d->keyboardForSurface(surface);
    if (!d->focusedSurface.keyboard) {
        d->focusedSurface = Private::FocusedSurface();
        return;
    }
    d->focusedSurface.surface = surface;
    d->destroyConnection = connect(d->focusedSurface.surface, &QObject::destroyed, this, [this] { d->surfaceDeleted(); });

    wl_array keys;
    wl_array_init(&keys);
    for (auto it = d->keyStates.constBegin(); it != d->keyStates.constEnd(); ++it) {
        if (it.value() == Private::KeyState::Pressed) {
            continue;
        }
        uint32_t *k = reinterpret_cast<uint32_t*>(wl_array_add(&keys, sizeof(uint32_t)));
        *k = it.key();
    }
    wl_keyboard_send_enter(d->focusedSurface.keyboard, serial, d->focusedSurface.surface->resource(), &keys);
    wl_array_release(&keys);

    d->sendModifiers(d->focusedSurface.keyboard);
}

wl_resource *KeyboardInterface::Private::keyboardForSurface(SurfaceInterface *surface) const
{
    if (!surface) {
        return nullptr;
    }
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        if ((*it).client == surface->client()) {
            return (*it).keyboard;
        }
    }
    return nullptr;
}

void KeyboardInterface::keyPressed(quint32 key)
{
    d->updateKey(key, Private::KeyState::Pressed);
    if (d->focusedSurface.surface && d->focusedSurface.keyboard) {
        wl_keyboard_send_key(d->focusedSurface.keyboard, d->display->nextSerial(), d->eventTime, key, WL_KEYBOARD_KEY_STATE_PRESSED);
    }
}

void KeyboardInterface::keyReleased(quint32 key)
{
    d->updateKey(key, Private::KeyState::Released);
    if (d->focusedSurface.surface && d->focusedSurface.keyboard) {
        wl_keyboard_send_key(d->focusedSurface.keyboard, d->display->nextSerial(), d->eventTime, key, WL_KEYBOARD_KEY_STATE_RELEASED);
    }
}

void KeyboardInterface::Private::updateKey(quint32 key, KeyState state)
{
    auto it = keyStates.find(key);
    if (it == keyStates.end()) {
        keyStates.insert(key, state);
        return;
    }
    it.value() = state;
}

void KeyboardInterface::updateTimestamp(quint32 time)
{
    d->eventTime = time;
}

void KeyboardInterface::updateModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group)
{
    d->modifiers.depressed = depressed;
    d->modifiers.latched = latched;
    d->modifiers.locked = locked;
    d->modifiers.group = group;
    if (d->focusedSurface.surface && d->focusedSurface.keyboard) {
        d->sendModifiers(d->focusedSurface.keyboard);
    }
}

SurfaceInterface *KeyboardInterface::focusedSurface() const
{
    return d->focusedSurface.surface;
}

}
}
