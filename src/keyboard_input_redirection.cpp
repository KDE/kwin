/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "keyboard_input_redirection.h"

#include "input.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "inputmethod.h"
#include "keyboard_input.h"
#include "wayland/inputmethod_v1.h"
#include "wayland/keyboard.h"
#include "wayland/seat.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include "xkb.h"

#if KWIN_BUILD_TABBOX
#include "tabbox/tabbox.h"
#endif

namespace KWin
{

class KeyStateChangedSpy : public InputEventSpy
{
public:
    void keyboardKey(KeyboardKeyEvent *event) override
    {
        if (event->state == KeyboardKeyState::Repeated) {
            return;
        }
        Q_EMIT input()->keyStateChanged(event->nativeScanCode, event->state);
    }
};

class ModifiersChangedSpy : public InputEventSpy
{
public:
    void keyboardKey(KeyboardKeyEvent *event) override
    {
        if (event->state == KeyboardKeyState::Repeated) {
            return;
        }

        const Qt::KeyboardModifiers modifiers = event->modifiers;
        if (modifiers == m_modifiers) {
            return;
        }
        Q_EMIT input()->keyboardModifiersChanged(modifiers, m_modifiers);
        m_modifiers = modifiers;
    }

private:
    Qt::KeyboardModifiers m_modifiers;
};

KeyboardInputRedirection::KeyboardInputRedirection(InputRedirection *input)
    : QObject(input)
    , m_input(input)
{
    waylandServer()->seat()->setHasKeyboard(true);
    m_globalKeyboard.reset(new KeyboardInput(this));
    trackKeyboard(m_globalKeyboard.get());
}

void KeyboardInputRedirection::init()
{
    input()->installInputEventSpy(new KeyStateChangedSpy);
    input()->installInputEventSpy(new ModifiersChangedSpy);
}

KeyboardInput *KeyboardInputRedirection::activeKeyboard() const
{
    if (m_input->m_lastKeyboardInputDevice && m_input->m_lastKeyboardInputDevice->keyboard()) {
        return m_input->m_lastKeyboardInputDevice->keyboard();
    }

    return m_globalKeyboard.get();
}

Xkb *KeyboardInputRedirection::xkb() const
{
    return activeKeyboard()->xkb();
}

Qt::KeyboardModifiers KeyboardInputRedirection::modifiers() const
{
    return activeKeyboard()->modifiers();
}

Qt::KeyboardModifiers KeyboardInputRedirection::modifiersRelevantForGlobalShortcuts() const
{
    return activeKeyboard()->modifiersRelevantForGlobalShortcuts();
}

QList<uint32_t> KeyboardInputRedirection::pressedKeys() const
{
    return activeKeyboard()->pressedKeys();
}

QList<uint32_t> KeyboardInputRedirection::filteredKeys() const
{
    return activeKeyboard()->filteredKeys();
}

void KeyboardInputRedirection::addFilteredKey(uint32_t key)
{
    activeKeyboard()->addFilteredKey(key);
}

QList<uint32_t> KeyboardInputRedirection::unfilteredKeys() const
{
    return activeKeyboard()->unfilteredKeys();
}

void KeyboardInputRedirection::updateKeymap()
{
    const QByteArray keymap = activeKeyboard()->xkb()->keymapContents();
    if (keymap.isEmpty()) {
        return;
    }

    if (auto *seatKeyboard = waylandServer()->seat()->keyboard()) {
        seatKeyboard->setKeymap(keymap);
    }

    if (auto *inputMethod = kwinApp()->inputMethod()) {
        if (auto *keyboardGrab = inputMethod->keyboardGrab()) {
            keyboardGrab->sendKeymap(keymap);
        }
    }
}

void KeyboardInputRedirection::forwardModifiers()
{
    auto *seat = waylandServer()->seat();
    if (!seat || !seat->keyboard()) {
        return;
    }

    auto keyboard = activeKeyboard();

    const auto &modifierState = keyboard->xkb()->modifierState();
    seat->notifyKeyboardModifiers(modifierState.depressed,
                                  modifierState.latched,
                                  modifierState.locked,
                                  keyboard->xkb()->currentLayout());
}

void KeyboardInputRedirection::trackKeyboard(KeyboardInput *keyboard)
{
    if (!keyboard || m_trackedKeyboards.contains(keyboard)) {
        return;
    }
    m_trackedKeyboards.insert(keyboard);

    connect(keyboard->xkb(), &Xkb::keymapChanged, this, [this, keyboard] {
        if (activeKeyboard() != keyboard) {
            return;
        }
        updateKeymap();
        forwardModifiers();
        Q_EMIT layoutChanged();
    });
}

Window *KeyboardInputRedirection::pickFocus() const
{
    if (waylandServer()->isScreenLocked()) {
        const QList<Window *> &stacking = Workspace::self()->stackingOrder();
        if (!stacking.isEmpty()) {
            auto it = stacking.end();
            do {
                --it;
                Window *t = (*it);
                if (t->isDeleted()) {
                    // a deleted window doesn't get mouse events
                    continue;
                }
                if (!t->isLockScreen()) {
                    continue;
                }
                if (!t->readyForPainting()) {
                    continue;
                }
                return t;
            } while (it != stacking.begin());
        }
        return nullptr;
    }

    if (input()->isSelectingWindow()) {
        return nullptr;
    }

#if KWIN_BUILD_TABBOX
    if (workspace()->tabbox()->isGrabbed()) {
        return nullptr;
    }
#endif

    return workspace()->activeWindow();
}

void KeyboardInputRedirection::update()
{
    auto seat = waylandServer()->seat();

    // TODO: this needs better integration
    Window *found = pickFocus();
    if (found && found->surface()) {
        if (found->surface() != seat->focusedKeyboardSurface()) {
            seat->setFocusedKeyboardSurface(found->surface(), activeKeyboard()->unfilteredKeys());
        }
    } else {
        seat->setFocusedKeyboardSurface(nullptr);
    }
}

std::shared_ptr<KeyboardInput> KeyboardInputRedirection::globalKeyboard() const
{
    return m_globalKeyboard;
}

QSet<KeyboardInput *> KeyboardInputRedirection::keyboards() const
{
    // QSet as multiple devices may share the same keyboard object
    QSet<KeyboardInput*> keyboards;
    for (auto device : input()->devices()) {
        if (device->keyboard()) {
            keyboards << device->keyboard();
        }
    }
    return keyboards;
}

}
