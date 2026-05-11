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
#include "wayland/display.h"
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

KeyboardInput::KeyboardInput(InputRedirection *input, QObject *parent)
    : QObject(parent ? parent : input)
    , m_input(input)
    , m_xkb(new Xkb(kwinApp()->followLocale1()))
{
    connect(m_xkb.get(), &Xkb::ledsChanged, this, &KeyboardInput::ledsChanged);
    m_xkb->setSeat(waylandServer()->seat());
}

KeyboardInput::~KeyboardInput() = default;

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

KeyboardLayout *KeyboardInput::keyboardLayout() const
{
    return m_input->keyboardLayout();
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

void KeyboardInput::init()
{
    Q_ASSERT(!m_inited);
    m_inited = true;
    const auto config = kwinApp()->kxkbConfig();
    m_xkb->setNumLockConfig(kwinApp()->inputConfig());
    m_xkb->setConfig(config);

    waylandServer()->seat()->setHasKeyboard(true);

    m_keyRepeatSpy = new KeyboardRepeat(m_xkb.get());
    m_keyRepeatSpy->setParent(this);
    connect(m_keyRepeatSpy, &KeyboardRepeat::keyRepeat, this,
            std::bind(&KeyboardInput::processKey, this, std::placeholders::_1, KeyboardKeyState::Repeated, std::placeholders::_2, nullptr));

    connect(workspace(), &QObject::destroyed, this, [this] {
        m_inited = false;
    });
    connect(waylandServer(), &QObject::destroyed, this, [this] {
        m_inited = false;
    });
    connect(workspace(), &Workspace::windowActivated, this, [this] {
        disconnect(m_activeWindowSurfaceChangedConnection);
        if (auto window = workspace()->activeWindow()) {
            m_activeWindowSurfaceChangedConnection = connect(window, &Window::surfaceChanged, this, &KeyboardInput::update);
        } else {
            m_activeWindowSurfaceChangedConnection = QMetaObject::Connection();
        }
        update();
    });
#if KWIN_BUILD_SCREENLOCKER
    if (kwinApp()->supportsLockScreen()) {
        connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged, this, &KeyboardInput::update);
    }
#endif

    reconfigure();
}

void KeyboardInput::reconfigure()
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

Window *KeyboardInput::pickFocus() const
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

void KeyboardInput::update()
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
        m_input->setLastKeyboardInputDevice(device, time);
    }
    input()->setLastInputHandler(this);
    if (!m_inited) {
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
        if (auto f = pickFocus()) {
            f->setLastUsageSerial(event.serial);
        }
    }

    m_input->processSpies(&InputEventSpy::keyboardKey, &event);
    m_input->processFilters(&InputEventFilter::keyboardKey, &event);

    if (state == KeyboardKeyState::Released) {
        m_filteredKeys.removeOne(key);
    }

    m_xkb->forwardModifiers();
    if (auto *inputmethod = kwinApp()->inputMethod()) {
        inputmethod->forwardModifiers(InputMethod::NoForce);
    }

    if (event.modifiersRelevantForGlobalShortcuts == Qt::KeyboardModifier::NoModifier && state != KeyboardKeyState::Released) {
        if (KeyboardLayout *keyboardLayout = m_input->keyboardLayout()) {
            keyboardLayout->checkLayoutChange(previousLayout);
        }
    }
}

}

#include "moc_keyboard_input.cpp"
