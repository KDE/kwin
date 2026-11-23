/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "keyboard_input.h"

#include "config-kwin.h"

#include "a11ymanager.h"
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

KeyboardInputRedirection::KeyboardInputRedirection(InputRedirection *parent)
    : QObject(parent)
    , m_input(parent)
    , m_xkb(new Xkb(kwinApp()->followLocale1()))
{
    connect(m_xkb.get(), &Xkb::ledsChanged, this, &KeyboardInputRedirection::ledsChanged);
    m_xkb->setSeat(waylandServer()->seat());
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
    return m_keyboardLayout.get();
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

void KeyboardInputRedirection::init()
{
    Q_ASSERT(!m_inited);
    m_inited = true;
    const auto config = kwinApp()->kxkbConfig();
    m_xkb->setNumLockConfig(kwinApp()->inputConfig());
    m_xkb->setConfig(config);

    waylandServer()->seat()->setHasKeyboard(true);

    m_keyStateChangedSpy = std::make_unique<KeyStateChangedSpy>(m_input);
    m_input->installInputEventSpy(m_keyStateChangedSpy.get());
    m_modifiersChangedSpy = std::make_unique<ModifiersChangedSpy>(m_input);
    m_input->installInputEventSpy(m_modifiersChangedSpy.get());

    m_keyboardLayout = std::make_unique<KeyboardLayout>(m_xkb.get(), config);
    m_keyboardLayout->init();
    m_input->installInputEventSpy(m_keyboardLayout.get());

    m_keyRepeatSpy = std::make_unique<KeyboardRepeat>(m_xkb.get());
    connect(m_keyRepeatSpy.get(), &KeyboardRepeat::keyRepeat, this,
            std::bind(&KeyboardInputRedirection::processKey, this, std::placeholders::_1, KeyboardKeyState::Repeated, std::placeholders::_2, nullptr));

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

void KeyboardInputRedirection::processKeySym(uint32_t sym, KeyboardKeyState state, std::chrono::microseconds time, InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!m_inited) {
        return;
    }

    // grab the current modifier state, cache it, send our key with our own modifiers at a known state, then reset back
    const auto modifierState = m_xkb->modifierState();
    const auto layout = m_xkb->currentLayout();

    std::optional<Xkb::KeyCode> keyCode = input()->keyboard()->xkb()->keycodeFromKeysym(sym);
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

        const bool isModifier = modifierKeySyms.contains(sym);
        if (isModifier) {
            processKey(keyCode->keyCode, state, time, device);
        } else {
            m_xkb->updateModifiers(modifierState.depressed | keyCode->modifiers, modifierState.latched, modifierState.locked, layout);
            processKey(keyCode->keyCode, state, time, device);
            m_xkb->updateModifiers(modifierState.depressed, modifierState.latched, modifierState.locked, layout);
        }
        return;
    }

    // clients don't seem to like having a keymap change whilst a key is pressed
    // for now send a fake release with every press and ignore other releases. We can make the keymap resetting more lazy if it's an issue IRL
    if (state == KeyboardKeyState::Pressed) {
        const bool keymapUpdated = m_xkb->updateToKeymapForKeySym(Xkb::scratchKeycode, sym);
        if (!keymapUpdated) {
            return;
        }
        processKeyInternal(Xkb::scratchKeycode, KeyboardKeyState::Pressed, time, device);
        processKeyInternal(Xkb::scratchKeycode, KeyboardKeyState::Released, time, device);
    }

    m_xkb->reconfigure();
    m_xkb->updateModifiers(modifierState.depressed, modifierState.latched, modifierState.locked, layout);
}

void KeyboardInputRedirection::processKey(uint32_t key, KeyboardKeyState state, std::chrono::microseconds time, InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!m_inited) {
        return;
    }

    processKeyInternal(key, state, time, device);
}

void KeyboardInputRedirection::processKeyInternal(uint32_t key, KeyboardKeyState state, std::chrono::microseconds time, InputDevice *device)
{
    m_xkb->flushKeymap();

    m_keyRepeatSpy->keyboardKey(key, state, time);

    if (!waylandServer()->isKeyboardShortcutsInhibited()) {
        const bool ret = kwinApp()->a11yManager()->processKey(key, state, time);
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

    bool forwardModifiers = true;

    if (auto *inputmethod = kwinApp()->inputMethod()) {
        inputmethod->forwardModifiers(InputMethod::NoForce);
        if (inputmethod->keyboardGrab()) {
            // when an input grab is established, we will forward the modifier explicitly as part of the grab
            forwardModifiers = false;
        }
    }

    if (forwardModifiers) {
        m_xkb->forwardModifiers();
    }

    if (event.modifiersRelevantForGlobalShortcuts == Qt::KeyboardModifier::NoModifier && state != KeyboardKeyState::Released) {
        m_keyboardLayout->checkLayoutChange(previousLayout);
    }
}

}

#include "moc_keyboard_input.cpp"
