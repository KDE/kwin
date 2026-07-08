/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fakeinputbackend.h"
#include "fakeinputdevice.h"
#include "keyboard_device.h"
#include "keyboard_input.h"
#include "wayland/display.h"

#include "wayland/qwayland-server-fake-input.h"

#include <linux/input-event-codes.h>

#include <QDebug>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(KWIN_LIBINPUT)
Q_LOGGING_CATEGORY(KWIN_FAKEINPUT, "kwin_fakeinput", QtWarningMsg)

namespace KWin
{

static const quint32 s_version = 6;

class FakeInputBackendPrivate : public QtWaylandServer::org_kde_kwin_fake_input
{
public:
    FakeInputBackendPrivate(FakeInputBackend *q, Display *display);

    using FakeInputDeviceMap = std::map<Resource *, std::unique_ptr<FakeInputDevice>>;

    FakeInputDevice *findDevice(Resource *resource);
    void removeDeviceByResource(Resource *resource);
    void removeDeviceByIterator(FakeInputDeviceMap::iterator it);
    static void sendKey(FakeInputDevice *device, uint32_t keyCode, KeyboardKeyState state);

    FakeInputBackend *q;
    Display *display;
    FakeInputDeviceMap devices;

protected:
    void org_kde_kwin_fake_input_bind_resource(Resource *resource) override;
    void org_kde_kwin_fake_input_destroy_resource(Resource *resource) override;
    void org_kde_kwin_fake_input_authenticate(Resource *resource, const QString &application, const QString &reason) override;
    void org_kde_kwin_fake_input_pointer_motion(Resource *resource, wl_fixed_t delta_x, wl_fixed_t delta_y) override;
    void org_kde_kwin_fake_input_button(Resource *resource, uint32_t button, uint32_t state) override;
    void org_kde_kwin_fake_input_axis(Resource *resource, uint32_t axis, wl_fixed_t value) override;
    void org_kde_kwin_fake_input_touch_down(Resource *resource, uint32_t id, wl_fixed_t x, wl_fixed_t y) override;
    void org_kde_kwin_fake_input_touch_motion(Resource *resource, uint32_t id, wl_fixed_t x, wl_fixed_t y) override;
    void org_kde_kwin_fake_input_touch_up(Resource *resource, uint32_t id) override;
    void org_kde_kwin_fake_input_touch_cancel(Resource *resource) override;
    void org_kde_kwin_fake_input_touch_frame(Resource *resource) override;
    void org_kde_kwin_fake_input_pointer_motion_absolute(Resource *resource, wl_fixed_t x, wl_fixed_t y) override;
    void org_kde_kwin_fake_input_keyboard_key(Resource *resource, uint32_t button, uint32_t state) override;
    void org_kde_kwin_fake_input_keyboard_keysym(Resource *resource, uint32_t keysym, uint32_t state) override;
    void org_kde_kwin_fake_input_destroy(Resource *resource) override;
};

static std::chrono::microseconds currentTime()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
}

FakeInputBackendPrivate::FakeInputBackendPrivate(FakeInputBackend *q, Display *display)
    : q(q)
    , display(display)
{
}

void FakeInputBackendPrivate::org_kde_kwin_fake_input_bind_resource(Resource *resource)
{
    auto device = new FakeInputDevice(q);
    devices[resource] = std::unique_ptr<FakeInputDevice>(device);
    Q_EMIT q->deviceAdded(device);
}

void FakeInputBackendPrivate::org_kde_kwin_fake_input_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void FakeInputBackendPrivate::org_kde_kwin_fake_input_destroy_resource(Resource *resource)
{
    removeDeviceByResource(resource);
}

FakeInputDevice *FakeInputBackendPrivate::findDevice(Resource *resource)
{
    return devices[resource].get();
}

void FakeInputBackendPrivate::removeDeviceByResource(Resource *resource)
{
    if (const auto it = devices.find(resource); it != devices.end()) {
        removeDeviceByIterator(it);
    }
}

void FakeInputBackendPrivate::removeDeviceByIterator(FakeInputDeviceMap::iterator it)
{
    const auto device = std::move(it->second);
    for (const auto button : device->pressedButtons) {
        Q_EMIT device->pointerButtonChanged(button, PointerButtonState::Released, currentTime(), device.get());
    }
    for (const auto key : device->pressedKeys) {
        Q_EMIT device->keyChanged(key, KeyboardKeyState::Released, currentTime(), device.get());
    }
    if (!device->activeTouches.empty()) {
        Q_EMIT device->touchCanceled(device.get());
    }
    devices.erase(it);
    Q_EMIT q->deviceRemoved(device.get());
}

void FakeInputBackendPrivate::org_kde_kwin_fake_input_authenticate(Resource *resource, const QString &application, const QString &reason)
{
    FakeInputDevice *device = findDevice(resource);
    if (device) {
        // TODO: make secure
        device->setAuthenticated(true);
    }
}

void FakeInputBackendPrivate::org_kde_kwin_fake_input_pointer_motion(Resource *resource, wl_fixed_t delta_x, wl_fixed_t delta_y)
{
    FakeInputDevice *device = findDevice(resource);
    if (!device->isAuthenticated()) {
        return;
    }
    const QPointF delta(wl_fixed_to_double(delta_x), wl_fixed_to_double(delta_y));

    Q_EMIT device->pointerMotion(delta, delta, currentTime(), device);
    Q_EMIT device->pointerFrame(device);
}

void FakeInputBackendPrivate::org_kde_kwin_fake_input_button(Resource *resource, uint32_t button, uint32_t state)
{
    FakeInputDevice *device = findDevice(resource);
    if (!device->isAuthenticated()) {
        return;
    }

    PointerButtonState nativeState;
    switch (state) {
    case WL_POINTER_BUTTON_STATE_PRESSED:
        nativeState = PointerButtonState::Pressed;
        if (device->pressedButtons.contains(button)) {
            return;
        }
        device->pressedButtons.insert(button);
        break;
    case WL_POINTER_BUTTON_STATE_RELEASED:
        nativeState = PointerButtonState::Released;
        if (!device->pressedButtons.remove(button)) {
            return;
        }
        break;
    default:
        return;
    }

    Q_EMIT device->pointerButtonChanged(button, nativeState, currentTime(), device);
    Q_EMIT device->pointerFrame(device);
}

void FakeInputBackendPrivate::org_kde_kwin_fake_input_axis(Resource *resource, uint32_t axis, wl_fixed_t value)
{
    FakeInputDevice *device = findDevice(resource);
    if (!device->isAuthenticated()) {
        return;
    }

    PointerAxis nativeAxis;
    switch (axis) {
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
        nativeAxis = PointerAxis::Horizontal;
        break;

    case WL_POINTER_AXIS_VERTICAL_SCROLL:
        nativeAxis = PointerAxis::Vertical;
        break;

    default:
        return;
    }

    Q_EMIT device->pointerAxisChanged(nativeAxis, wl_fixed_to_double(value), 0, PointerAxisSource::Unknown, false, currentTime(), device);
    Q_EMIT device->pointerFrame(device);
}

void FakeInputBackendPrivate::org_kde_kwin_fake_input_touch_down(Resource *resource, uint32_t id, wl_fixed_t x, wl_fixed_t y)
{
    FakeInputDevice *device = findDevice(resource);
    if (!device->isAuthenticated()) {
        return;
    }
    if (device->activeTouches.contains(id)) {
        return;
    }
    device->activeTouches.insert(id);
    Q_EMIT device->touchDown(id, QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)), currentTime(), device);
}

void FakeInputBackendPrivate::org_kde_kwin_fake_input_touch_motion(Resource *resource, uint32_t id, wl_fixed_t x, wl_fixed_t y)
{
    FakeInputDevice *device = findDevice(resource);
    if (!device->isAuthenticated()) {
        return;
    }
    if (!device->activeTouches.contains(id)) {
        return;
    }
    Q_EMIT device->touchMotion(id, QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)), currentTime(), device);
}

void FakeInputBackendPrivate::org_kde_kwin_fake_input_touch_up(Resource *resource, uint32_t id)
{
    FakeInputDevice *device = findDevice(resource);
    if (!device->isAuthenticated()) {
        return;
    }
    if (device->activeTouches.remove(id)) {
        Q_EMIT device->touchUp(id, currentTime(), device);
    }
}

void FakeInputBackendPrivate::org_kde_kwin_fake_input_touch_cancel(Resource *resource)
{
    FakeInputDevice *device = findDevice(resource);
    if (!device->isAuthenticated()) {
        return;
    }
    device->activeTouches.clear();
    Q_EMIT device->touchCanceled(device);
}

void FakeInputBackendPrivate::org_kde_kwin_fake_input_touch_frame(Resource *resource)
{
    FakeInputDevice *device = findDevice(resource);
    if (!device->isAuthenticated()) {
        return;
    }
    Q_EMIT device->touchFrame(device);
}

void FakeInputBackendPrivate::org_kde_kwin_fake_input_pointer_motion_absolute(Resource *resource, wl_fixed_t x, wl_fixed_t y)
{
    FakeInputDevice *device = findDevice(resource);
    if (!device->isAuthenticated()) {
        return;
    }

    Q_EMIT device->pointerMotionAbsolute(QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)), currentTime(), device);
    Q_EMIT device->pointerFrame(device);
}

void FakeInputBackendPrivate::org_kde_kwin_fake_input_keyboard_key(Resource *resource, uint32_t key, uint32_t state)
{
    FakeInputDevice *device = findDevice(resource);
    if (!device->isAuthenticated()) {
        return;
    }
    KeyboardKeyState nativeState;
    switch (state) {
    case WL_KEYBOARD_KEY_STATE_PRESSED:
        nativeState = KeyboardKeyState::Pressed;
        break;

    case WL_KEYBOARD_KEY_STATE_RELEASED:
        nativeState = KeyboardKeyState::Released;
        break;

    default:
        return;
    }
    sendKey(device, key, nativeState);
}

void FakeInputBackendPrivate::org_kde_kwin_fake_input_keyboard_keysym(Resource *resource, uint32_t keySym, uint32_t pressedState)
{
    FakeInputDevice *device = findDevice(resource);
    if (!device->isAuthenticated()) {
        return;
    }

    KeyboardKeyState nativeState;
    switch (pressedState) {
    case WL_KEYBOARD_KEY_STATE_PRESSED:
        nativeState = KeyboardKeyState::Pressed;
        break;
    case WL_KEYBOARD_KEY_STATE_RELEASED:
        nativeState = KeyboardKeyState::Released;
        break;
    default:
        return;
    }

    std::optional<KeyboardDevice::KeyCode> keyCode = device->keyboard()->keycodeFromKeysym(keySym);
    // grab the current modifier state, cache it, send our key with our own modifiers at a known state, then reset back
    xkb_state *state = device->keyboard()->state();
    xkb_mod_mask_t formerDepressed = xkb_state_serialize_mods(state, XKB_STATE_MODS_DEPRESSED);
    xkb_mod_mask_t formerLatched = xkb_state_serialize_mods(state, XKB_STATE_MODS_LATCHED);
    xkb_mod_mask_t formerLocked = xkb_state_serialize_mods(state, XKB_STATE_MODS_LOCKED);
    xkb_layout_index_t formerLayout = xkb_state_serialize_layout(state, XKB_STATE_LAYOUT_EFFECTIVE);

    if (keyCode) {
        // if the keysym is "F" we need to temporarily depress shift before the F key and most importantly unset it after
        // however we still want modifiers like pressing control and alt to still work.

        // In addition if a modifier key is received we want that modifier state to persist until released, so don't set/unset modifiers when forwarding
        static const QSet<xkb_keysym_t> modifierKeySyms = {
            XKB_KEY_Shift_L,
            XKB_KEY_Shift_R,
            XKB_KEY_Control_L,
            XKB_KEY_Control_R,
            XKB_KEY_Alt_L,
            XKB_KEY_Alt_R,
            XKB_KEY_Meta_L,
            XKB_KEY_Meta_R,
            XKB_KEY_Super_L,
            XKB_KEY_Super_R,
        };

        const bool isModifier = modifierKeySyms.contains(keySym);
        if (isModifier) {
            sendKey(device, keyCode->keyCode, nativeState);
        } else {
            device->keyboard()->updateModifiers(formerDepressed | keyCode->modifiers, formerLatched, formerLocked, formerLayout);
            sendKey(device, keyCode->keyCode, nativeState);
            device->keyboard()->updateModifiers(formerDepressed, formerLatched, formerLocked, formerLayout);
        }
        return;
    }

    // otherwise create a new keymap and send that one key
    // clients don't seem to like having a keymap change whilst a key is pressed
    // for now send a fake release with every press and ignore other releases. We can make the keymap resetting more lazy if it's an issue IRL
    if (nativeState == KeyboardKeyState::Pressed) {
        static const uint unmappedKeyCode = 247;
        bool keymapUpdated = device->keyboard()->updateToKeymapForKeySym(unmappedKeyCode, keySym);
        if (!keymapUpdated) {
            return;
        }
        sendKey(device, unmappedKeyCode, KeyboardKeyState::Pressed);

        for (quint32 key : std::as_const(device->pressedKeys)) {
            sendKey(device, key, KeyboardKeyState::Released);
        }
        device->keyboard()->updateKeymap(input()->keyboard()->globalDevice()->keymapContents());
        device->keyboard()->updateModifiers(formerDepressed, formerLatched, formerLocked, formerLayout);
    }
}

void FakeInputBackendPrivate::sendKey(FakeInputDevice *device, uint32_t keyCode, KeyboardKeyState state)
{
    switch (state) {
    case KeyboardKeyState::Pressed:
        if (device->pressedKeys.contains(keyCode)) {
            return;
        }
        device->pressedKeys.insert(keyCode);
        break;

    case KeyboardKeyState::Released:
        if (!device->pressedKeys.remove(keyCode)) {
            return;
        }
        break;
    default:
        return;
    }

    device->keyboard()->processKey(keyCode, state, currentTime(), device);
}

FakeInputBackend::FakeInputBackend(Display *display)
    : d(std::make_unique<FakeInputBackendPrivate>(this, display))
{
}

FakeInputBackend::~FakeInputBackend()
{
    while (!d->devices.empty()) {
        d->removeDeviceByIterator(d->devices.begin());
    }
}

void FakeInputBackend::initialize()
{
    d->init(*d->display, s_version);
}

} // namespace KWin

#include "moc_fakeinputbackend.cpp"
