/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "keyboard_input.h"

#include "config-kwin.h"

#include "input_event.h"
#include "input_event_spy.h"
#include "inputmethod.h"
#include "keyboard_input_redirection.h"
#include "keyboard_layout.h"
#include "keyboard_repeat.h"
#include "main.h"
#include "wayland/display.h"
#include "wayland_server.h"
#include "window.h"
#include "xkb.h"
// Qt
#include <QKeyEvent>

namespace KWin
{

KeyboardInput::KeyboardInput(QObject *parent)
    : QObject(parent)
    , m_xkb(new Xkb(kwinApp()->followLocale1()))
{
    const auto config = kwinApp()->kxkbConfig();
    m_xkb->setNumLockConfig(kwinApp()->inputConfig());
    m_xkb->setConfig(config);

    m_keyRepeatSpy = std::make_unique<KeyboardRepeat>(m_xkb.get());
    connect(m_keyRepeatSpy.get(), &KeyboardRepeat::keyRepeat, this,
            std::bind(&KeyboardInput::processKey, this, std::placeholders::_1, KeyboardKeyState::Repeated, std::placeholders::_2, nullptr));

    connect(m_xkb.get(), &Xkb::ledsChanged, this, &KeyboardInput::ledsChanged);
}

KeyboardInput::~KeyboardInput() = default;

bool KeyboardInput::isInitialized() const
{
    return m_keyRepeatSpy != nullptr;
}

Xkb *KeyboardInput::xkb() const
{
    return m_xkb.get();
}

Qt::KeyboardModifiers KeyboardInput::modifiers() const
{
    return m_xkb->modifiers();
}

Qt::KeyboardModifiers KeyboardInput::modifiersRelevantForGlobalShortcuts() const
{
    return m_xkb->modifiersRelevantForGlobalShortcuts();
}

QList<uint32_t> KeyboardInput::pressedKeys() const
{
    return m_pressedKeys;
}

QList<uint32_t> KeyboardInput::filteredKeys() const
{
    return m_filteredKeys;
}

QList<uint32_t> KeyboardInput::unfilteredKeys() const
{
    QList<uint32_t> ret = m_pressedKeys;
    for (const uint32_t &key : m_filteredKeys) {
        ret.removeOne(key);
    }
    return ret;
}

void KeyboardInput::addFilteredKey(uint32_t key)
{
    if (!m_filteredKeys.contains(key)) {
        m_filteredKeys.append(key);
    }
}

static constexpr std::array s_modifierKeys = {
    Qt::Key_Control,
    Qt::Key_Alt,
    Qt::Key_AltGr,
    Qt::Key_Meta,
    Qt::Key_CapsLock,
    Qt::Key_NumLock,
    Qt::Key_Shift,
    Qt::Key_ScrollLock,
};

void KeyboardInput::processKey(uint32_t key, KeyboardKeyState state, std::chrono::microseconds time, InputDevice *device)
{
    if (device) {
        input()->keyboard()->setLastKeyboardInputDevice(device, time);
    }
    input()->setLastInputHandler(this);
    if (!m_keyRepeatSpy) {
        return;
    }

    m_keyRepeatSpy->keyboardKey(key, state, time);

    if (!waylandServer()->isKeyboardShortcutsInhibited()) {
        const bool ret = m_a11yKeyboardMonitor.processKey(key, state, time);
        if (ret) {
            return;
        }
    }

    if (state == KeyboardKeyState::Pressed) {
        if (!m_pressedKeys.contains(key)) {
            m_pressedKeys.append(key);
        }
    } else if (state == KeyboardKeyState::Released) {
        m_pressedKeys.removeOne(key);
    }

    const quint32 previousLayout = m_xkb->currentLayout();
    if (state != KeyboardKeyState::Repeated) {
        m_xkb->updateKey(key, state);
    }

    const xkb_keysym_t keySym = m_xkb->toKeysym(key);
    const Qt::KeyboardModifiers globalShortcutsModifiers = m_xkb->modifiersRelevantForGlobalShortcuts(key);

    KeyboardKeyEvent event{
        .device = device,
        .state = state,
        .key = m_xkb->toQtKey(keySym, key, globalShortcutsModifiers ? Qt::ControlModifier : Qt::KeyboardModifiers()),
        .nativeScanCode = key,
        .nativeVirtualKey = keySym,
        .text = m_xkb->toString(m_xkb->currentKeysym()),
        .modifiers = m_xkb->modifiers(),
        .modifiersRelevantForGlobalShortcuts = m_xkb->modifiersRelevantForGlobalShortcuts(key),
        .timestamp = time,
        .serial = waylandServer()->display()->nextSerial(),
    };
    if (state == KeyboardKeyState::Pressed && !std::ranges::contains(s_modifierKeys, event.key)) {
        input()->setLastInteractionSerial(event.serial);
        if (auto f = input()->keyboard()->pickFocus()) {
            f->setLastUsageSerial(event.serial);
        }
    }

    input()->processSpies(&InputEventSpy::keyboardKey, &event);
    input()->processFilters(&InputEventFilter::keyboardKey, &event);

    if (state == KeyboardKeyState::Released) {
        m_filteredKeys.removeOne(key);
    }

    bool forwardModifiers = true;

    if (auto *inputmethod = kwinApp()->inputMethod()) {
        inputmethod->forwardModifiers(InputMethod::NoForce);
        if (inputmethod->keyboardGrab()) {
            // when an input grab is established, we will forward the modifier explicitly as part of the grab
            forwardModifiers = false;
        }
    }

    if (forwardModifiers) {
        input()->keyboard()->forwardModifiers();
    }

    if (event.modifiersRelevantForGlobalShortcuts == Qt::KeyboardModifier::NoModifier && state != KeyboardKeyState::Released) {
        if (KeyboardLayout *keyboardLayout = input()->keyboardLayout()) {
            keyboardLayout->checkLayoutChange(previousLayout);
        }
    }
}

}

#include "moc_keyboard_input.cpp"
