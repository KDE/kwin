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
#include "keyboard_layout.h"
#include "keyboard_repeat.h"
#include "wayland/datadevice.h"
#include "wayland/keyboard.h"
#include "wayland/seat.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include "xkb.h"
// screenlocker
#if KWIN_BUILD_SCREENLOCKER
#include <KScreenLocker/KsldApp>
#endif
#if KWIN_BUILD_TABBOX
#include "tabbox/tabbox.h"
#endif
// Frameworks
#include <KGlobalAccel>
// Qt
#include <QKeyEvent>

#include <cmath>

namespace KWin
{

KeyboardInputRedirection::KeyboardInputRedirection(InputRedirection *parent)
    : QObject(parent)
    , m_input(parent)
    , m_xkb(new Xkb(kwinApp()->followLocale1()))
{
    connect(m_xkb.get(), &Xkb::ledsChanged, this, &KeyboardInputRedirection::ledsChanged);
    if (waylandServer()) {
        m_xkb->setSeat(waylandServer()->seat());
    }
}

KeyboardInputRedirection::~KeyboardInputRedirection() = default;

Xkb *KeyboardInputRedirection::xkb() const
{
    return m_xkb.get();
}

Qt::KeyboardModifiers KeyboardInputRedirection::modifiers() const
{
    return m_xkb->modifiers();
}

Qt::KeyboardModifiers KeyboardInputRedirection::modifiersRelevantForGlobalShortcuts() const
{
    return m_xkb->modifiersRelevantForGlobalShortcuts();
}

KeyboardLayout *KeyboardInputRedirection::keyboardLayout() const
{
    return m_keyboardLayout;
}

QList<uint32_t> KeyboardInputRedirection::pressedKeys() const
{
    return m_pressedKeys;
}

QList<uint32_t> KeyboardInputRedirection::filteredKeys() const
{
    return m_filteredKeys;
}

QList<uint32_t> KeyboardInputRedirection::unfilteredKeys() const
{
    QList<uint32_t> ret = m_pressedKeys;
    for (const uint32_t &key : m_filteredKeys) {
        ret.removeOne(key);
    }
    return ret;
}

void KeyboardInputRedirection::addFilteredKey(uint32_t key)
{
    if (!m_filteredKeys.contains(key)) {
        m_filteredKeys.append(key);
    }
}

class KeyStateChangedSpy : public InputEventSpy
{
public:
    KeyStateChangedSpy(InputRedirection *input)
        : m_input(input)
    {
    }

    void keyboardKey(KeyboardKeyEvent *event) override
    {
        if (event->state == KeyboardKeyState::Repeated) {
            return;
        }
        Q_EMIT m_input->keyStateChanged(event->nativeScanCode, event->state);
    }

private:
    InputRedirection *m_input;
};

class ModifiersChangedSpy : public InputEventSpy
{
public:
    ModifiersChangedSpy(InputRedirection *input)
        : m_input(input)
        , m_modifiers()
    {
    }

    void keyboardKey(KeyboardKeyEvent *event) override
    {
        if (event->state == KeyboardKeyState::Repeated) {
            return;
        }

        const Qt::KeyboardModifiers mods = event->modifiers;
        if (mods == m_modifiers) {
            return;
        }
        Q_EMIT m_input->keyboardModifiersChanged(mods, m_modifiers);
        m_modifiers = mods;
    }

private:
    InputRedirection *m_input;
    Qt::KeyboardModifiers m_modifiers;
};

void KeyboardInputRedirection::init()
{
    Q_ASSERT(!m_inited);
    m_inited = true;
    const auto config = kwinApp()->kxkbConfig();
    m_xkb->setNumLockConfig(kwinApp()->inputConfig());
    m_xkb->setConfig(config);

    waylandServer()->seat()->setHasKeyboard(true);

    m_input->installInputEventSpy(new KeyStateChangedSpy(m_input));
    m_modifiersChangedSpy = new ModifiersChangedSpy(m_input);
    m_input->installInputEventSpy(m_modifiersChangedSpy);
    m_keyboardLayout = new KeyboardLayout(m_xkb.get(), config);
    m_keyboardLayout->init();
    m_input->installInputEventSpy(m_keyboardLayout);

    KeyboardRepeat *keyRepeatSpy = new KeyboardRepeat(m_xkb.get());
    connect(keyRepeatSpy, &KeyboardRepeat::keyRepeat, this,
            std::bind(&KeyboardInputRedirection::processKey, this, std::placeholders::_1, KeyboardKeyState::Repeated, std::placeholders::_2, nullptr));
    m_input->installInputEventSpy(keyRepeatSpy);

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
        // when the clients will repeat the character or turn repeat key events into an accent character selection, we want
        // to tell the clients that we are indeed repeating keys.
        const bool enabled = repeatMode == QLatin1String("accent") || repeatMode == QLatin1String("repeat");

        waylandServer()->seat()->keyboard()->setRepeatInfo(enabled ? rate : 0, delay);
    }
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
    if (!m_inited) {
        return;
    }
    auto seat = waylandServer()->seat();

    // TODO: this needs better integration
    Window *found = pickFocus();
    if (found && found->surface()) {
        if (found->surface() != seat->focusedKeyboardSurface()) {
            seat->setFocusedKeyboardSurface(found->surface(), unfilteredKeys());
        }
    } else {
        seat->setFocusedKeyboardSurface(nullptr);
    }
}

void KeyboardInputRedirection::processKey(uint32_t key, KeyboardKeyState state, std::chrono::microseconds time, InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!m_inited) {
        return;
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
    };

    m_input->processSpies(std::bind(&InputEventSpy::keyboardKey, std::placeholders::_1, &event));
    m_input->processFilters(std::bind(&InputEventFilter::keyboardKey, std::placeholders::_1, &event));

    if (state == KeyboardKeyState::Released) {
        m_filteredKeys.removeOne(key);
    }

    m_xkb->forwardModifiers();
    if (auto *inputmethod = kwinApp()->inputMethod()) {
        inputmethod->forwardModifiers(InputMethod::NoForce);
    }

    if (event.modifiersRelevantForGlobalShortcuts == Qt::KeyboardModifier::NoModifier && state != KeyboardKeyState::Released) {
        m_keyboardLayout->checkLayoutChange(previousLayout);
    }
}

}

#include "moc_keyboard_input.cpp"
