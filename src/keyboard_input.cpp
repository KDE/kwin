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
#include "keyboard_device.h"
#include "keyboard_layout.h"
#include "keyboard_repeat.h"
#include "wayland/datadevice.h"
#include "wayland/display.h"
#include "wayland/inputmethod_v1.h"
#include "wayland/keyboard.h"
#include "wayland/seat.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
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
    , m_activeDevice(new KeyboardDevice(kwinApp()->followLocale1()))
{
    connect(m_activeDevice.get(), &KeyboardDevice::ledsChanged, this, &KeyboardInputRedirection::ledsChanged);
    connect(m_activeDevice.get(), &KeyboardDevice::keymapChanged, this, &KeyboardInputRedirection::updateKeymap);
    connect(m_activeDevice.get(), &KeyboardDevice::modifierStateChanged, this, &KeyboardInputRedirection::forwardModifiers);
}

KeyboardInputRedirection::~KeyboardInputRedirection() = default;

KeyboardDevice *KeyboardInputRedirection::activeDevice() const
{
    return m_activeDevice.get();
}

Qt::KeyboardModifiers KeyboardInputRedirection::modifiers() const
{
    return m_activeDevice->modifiers();
}

Qt::KeyboardModifiers KeyboardInputRedirection::modifiersRelevantForGlobalShortcuts() const
{
    return m_activeDevice->modifiersRelevantForGlobalShortcuts();
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

void KeyboardInputRedirection::init()
{
    Q_ASSERT(!m_inited);
    m_inited = true;
    const auto config = kwinApp()->kxkbConfig();
    m_activeDevice->setNumLockConfig(kwinApp()->inputConfig());
    m_activeDevice->setConfig(config);

    waylandServer()->seat()->setHasKeyboard(true);

    m_keyStateChangedSpy = std::make_unique<KeyStateChangedSpy>(m_input);
    m_input->installInputEventSpy(m_keyStateChangedSpy.get());
    m_modifiersChangedSpy = std::make_unique<ModifiersChangedSpy>(m_input);
    m_input->installInputEventSpy(m_modifiersChangedSpy.get());
    m_keyboardLayout = new KeyboardLayout(m_activeDevice.get(), config);
    m_keyboardLayout->init();
    m_input->installInputEventSpy(m_keyboardLayout);

    m_keyRepeatSpy = std::make_unique<KeyboardRepeat>(m_activeDevice.get());
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

void KeyboardInputRedirection::forwardModifiers()
{
    if (!m_inited) {
        return;
    }
    auto seat = waylandServer()->seat();
    if (!seat || !seat->keyboard()) {
        return;
    }
    const auto &modifierState = m_activeDevice->modifierState();
    seat->notifyKeyboardModifiers(modifierState.depressed,
                                  modifierState.latched,
                                  modifierState.locked,
                                  m_activeDevice->currentLayout());
}

void KeyboardInputRedirection::updateKeymap(const QByteArray &keymap)
{
    if (!m_inited) {
        return;
    }
    const QByteArray currentKeymap = keymap.isEmpty() ? m_activeDevice->keymapContents() : keymap;
    if (currentKeymap.isEmpty()) {
        return;
    }

    auto seat = waylandServer()->seat();
    if (seat->keyboard()) {
        seat->keyboard()->setKeymap(currentKeymap);
    }
    if (auto *inputmethod = kwinApp()->inputMethod()) {
        if (auto *keyboardGrab = inputmethod->keyboardGrab()) {
            keyboardGrab->sendKeymap(currentKeymap);
        }
        inputmethod->forwardModifiers(InputMethod::Force);
    }
    forwardModifiers();
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

void KeyboardInputRedirection::processKey(uint32_t key, KeyboardKeyState state, std::chrono::microseconds time, InputDevice *device)
{
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

    const quint32 previousLayout = m_activeDevice->currentLayout();
    if (state != KeyboardKeyState::Repeated) {
        m_activeDevice->updateKey(key, state);
    }

    const xkb_keysym_t keySym = m_activeDevice->toKeysym(key);
    const Qt::KeyboardModifiers globalShortcutsModifiers = m_activeDevice->modifiersRelevantForGlobalShortcuts(key);

    KeyboardKeyEvent event{
        .device = device,
        .state = state,
        .key = m_activeDevice->toQtKey(keySym, key, globalShortcutsModifiers ? Qt::ControlModifier : Qt::KeyboardModifiers()),
        .nativeScanCode = key,
        .nativeVirtualKey = keySym,
        .text = m_activeDevice->toString(m_activeDevice->currentKeysym()),
        .modifiers = m_activeDevice->modifiers(),
        .modifiersRelevantForGlobalShortcuts = m_activeDevice->modifiersRelevantForGlobalShortcuts(key),
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
        this->forwardModifiers();
    }

    if (event.modifiersRelevantForGlobalShortcuts == Qt::KeyboardModifier::NoModifier && state != KeyboardKeyState::Released) {
        m_keyboardLayout->checkLayoutChange(previousLayout);
    }
}

}

#include "moc_keyboard_input.cpp"
