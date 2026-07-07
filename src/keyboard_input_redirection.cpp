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
#if KWIN_BUILD_SCREENLOCKER
#include <KScreenLocker/KsldApp>
#endif

#include <cmath>

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
    updateActiveKeyboard();
}

void KeyboardInputRedirection::init()
{
    Q_ASSERT(!m_inited);
    m_inited = true;

    input()->installInputEventSpy(new KeyStateChangedSpy);
    input()->installInputEventSpy(new ModifiersChangedSpy);

    connect(workspace(), &QObject::destroyed, this, [this] {
        m_inited = false;
    });
    connect(waylandServer(), &QObject::destroyed, this, [this] {
        m_inited = false;
    });
    connect(workspace(), &Workspace::windowActivated, this, [this] {
        disconnect(m_activeWindowSurfaceChangedConnection);
        if (auto window = workspace()->activeWindow()) {
            m_activeWindowSurfaceChangedConnection = connect(window, &Window::surfaceChanged, this, &KeyboardInputRedirection::update);
        } else {
            m_activeWindowSurfaceChangedConnection = QMetaObject::Connection();
        }
        update();
    });
#if KWIN_BUILD_SCREENLOCKER
    if (kwinApp()->supportsLockScreen()) {
        connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged, this, &KeyboardInputRedirection::update);
    }
#endif

    reconfigure();
}

void KeyboardInputRedirection::reconfigure()
{
    if (!m_inited) {
        return;
    }

    if (waylandServer()->seat()->keyboard()) {
        const auto config = kwinApp()->inputConfig()->group(QStringLiteral("Keyboard"));
        const int delay = config.readEntry("RepeatDelay", 600);
        const int rate = std::ceil(config.readEntry("RepeatRate", 25.0));
        const QString repeatMode = config.readEntry("KeyRepeat", "repeat");
        const bool enabled = repeatMode == QLatin1StringView("repeat");

        waylandServer()->seat()->keyboard()->setRepeatInfo(enabled ? rate : 0, delay);
    }
}

bool KeyboardInputRedirection::isInitialized() const
{
    return m_inited;
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

void KeyboardInputRedirection::setLastKeyboardInputDevice(InputDevice *device, std::chrono::microseconds time)
{
    if (!device || !device->keyboard() || device == m_input->m_lastKeyboardInputDevice) {
        return;
    }

    KeyboardInput *oldKeyboard = activeKeyboard();
    m_input->m_lastKeyboardInputDevice = device;

    KeyboardInput *newKeyboard = activeKeyboard();

    if (oldKeyboard->modifiers() != newKeyboard->modifiers()) {
        Q_EMIT m_input->keyboardModifiersChanged(oldKeyboard->modifiers(), newKeyboard->modifiers());
    }

    m_input->syncActiveKeyboardState(oldKeyboard, newKeyboard);
    waylandServer()->seat()->setTimestamp(time);
    updateActiveKeyboard();
    forwardModifiers();
    m_input->updateLeds(newKeyboard->xkb()->leds());

    Q_EMIT layoutChanged();
    Q_EMIT modifierStateChanged();
    Q_EMIT ledsChanged(newKeyboard->xkb()->leds());
}

void KeyboardInputRedirection::updateKeymap()
{
    if (!m_inited) {
        return;
    }
    auto *keyboard = activeKeyboard();

    const QByteArray keymap = keyboard->xkb()->keymapContents();
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

    auto *keyboard = activeKeyboard();
    if (!keyboard) {
        return;
    }

    const auto &modifierState = keyboard->xkb()->modifierState();
    seat->notifyKeyboardModifiers(modifierState.depressed,
                                  modifierState.latched,
                                  modifierState.locked,
                                  keyboard->xkb()->currentLayout());
}

void KeyboardInputRedirection::updateActiveKeyboard()
{
    KeyboardInput *keyboard = activeKeyboard();
    if (m_trackedKeyboard == keyboard) {
        return;
    }

    disconnect(m_keymapChangedConnection);
    m_trackedKeyboard = keyboard;

    if (!keyboard) {
        m_keymapChangedConnection = QMetaObject::Connection();
        return;
    }

    m_keymapChangedConnection = connect(keyboard->xkb(), &Xkb::keymapChanged, this, [this] {
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
