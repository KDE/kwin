/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "keyboard_input.h"

#include <config-kwin.h>

#include "input_event.h"
#include "input_event_spy.h"
#include "inputmethod.h"
#include "keyboard_layout.h"
#include "keyboard_repeat.h"
#include "modifier_only_shortcuts.h"
#include "wayland/datadevice_interface.h"
#include "wayland/keyboard_interface.h"
#include "wayland/seat_interface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include "xkb.h"
// screenlocker
#if KWIN_BUILD_SCREENLOCKER
#include <KScreenLocker/KsldApp>
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

class KeyStateChangedSpy : public InputEventSpy
{
public:
    KeyStateChangedSpy(InputRedirection *input)
        : m_input(input)
    {
    }

    void keyEvent(KeyEvent *event) override
    {
        if (event->isAutoRepeat()) {
            return;
        }
        Q_EMIT m_input->keyStateChanged(event->nativeScanCode(), event->type() == QEvent::KeyPress ? InputRedirection::KeyboardKeyPressed : InputRedirection::KeyboardKeyReleased);
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

    void keyEvent(KeyEvent *event) override
    {
        if (event->isAutoRepeat()) {
            return;
        }
        const Qt::KeyboardModifiers mods = event->modifiers();
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
    m_xkb->setNumLockConfig(InputConfig::self()->inputConfig());
    m_xkb->setConfig(config);

    // Workaround for QTBUG-54371: if there is no real keyboard Qt doesn't request virtual keyboard
    waylandServer()->seat()->setHasKeyboard(true);
    // connect(m_input, &InputRedirection::hasAlphaNumericKeyboardChanged,
    //         waylandServer()->seat(), &KWaylandServer::SeatInterface::setHasKeyboard);

    m_input->installInputEventSpy(new KeyStateChangedSpy(m_input));
    m_modifiersChangedSpy = new ModifiersChangedSpy(m_input);
    m_input->installInputEventSpy(m_modifiersChangedSpy);
    m_keyboardLayout = new KeyboardLayout(m_xkb.get(), config);
    m_keyboardLayout->init();
    m_input->installInputEventSpy(m_keyboardLayout);

    if (waylandServer()->hasGlobalShortcutSupport()) {
        m_input->installInputEventSpy(new ModifierOnlyShortcuts);
    }

    KeyboardRepeat *keyRepeatSpy = new KeyboardRepeat(m_xkb.get());
    connect(keyRepeatSpy, &KeyboardRepeat::keyRepeat, this,
            std::bind(&KeyboardInputRedirection::processKey, this, std::placeholders::_1, InputRedirection::KeyboardKeyAutoRepeat, std::placeholders::_2, nullptr));
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
    if (waylandServer()->hasScreenLockerIntegration()) {
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
        const auto config = InputConfig::self()->inputConfig()->group(QStringLiteral("Keyboard"));
        const int delay = config.readEntry("RepeatDelay", 660);
        const int rate = std::ceil(config.readEntry("RepeatRate", 25.0));
        const QString repeatMode = config.readEntry("KeyRepeat", "repeat");
        // when the clients will repeat the character or turn repeat key events into an accent character selection, we want
        // to tell the clients that we are indeed repeating keys.
        const bool enabled = repeatMode == QLatin1String("accent") || repeatMode == QLatin1String("repeat");

        waylandServer()->seat()->keyboard()->setRepeatInfo(enabled ? rate : 0, delay);
    }
}

void KeyboardInputRedirection::update()
{
    if (!m_inited) {
        return;
    }
    auto seat = waylandServer()->seat();
    // TODO: this needs better integration
    Window *found = nullptr;
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
                found = t;
                break;
            } while (it != stacking.begin());
        }
    } else if (!input()->isSelectingWindow()) {
        found = workspace()->activeWindow();
    }
    if (found && found->surface()) {
        if (found->surface() != seat->focusedKeyboardSurface()) {
            seat->setFocusedKeyboardSurface(found->surface());
        }
    } else {
        seat->setFocusedKeyboardSurface(nullptr);
    }
}

void KeyboardInputRedirection::processKey(uint32_t key, InputRedirection::KeyboardKeyState state, std::chrono::microseconds time, InputDevice *device)
{
    QEvent::Type type;
    bool autoRepeat = false;
    switch (state) {
    case InputRedirection::KeyboardKeyAutoRepeat:
        autoRepeat = true;
        // fall through
    case InputRedirection::KeyboardKeyPressed:
        type = QEvent::KeyPress;
        break;
    case InputRedirection::KeyboardKeyReleased:
        type = QEvent::KeyRelease;
        break;
    default:
        Q_UNREACHABLE();
    }

    const quint32 previousLayout = m_xkb->currentLayout();
    if (!autoRepeat) {
        m_xkb->updateKey(key, state);
    }

    const xkb_keysym_t keySym = m_xkb->currentKeysym();
    const Qt::KeyboardModifiers globalShortcutsModifiers = m_xkb->modifiersRelevantForGlobalShortcuts(key);
    KeyEvent event(type,
                   m_xkb->toQtKey(keySym, key, globalShortcutsModifiers ? Qt::ControlModifier : Qt::KeyboardModifiers()),
                   m_xkb->modifiers(),
                   key,
                   keySym,
                   m_xkb->toString(keySym),
                   autoRepeat,
                   time,
                   device);
    event.setModifiersRelevantForGlobalShortcuts(globalShortcutsModifiers);

    m_input->processSpies(std::bind(&InputEventSpy::keyEvent, std::placeholders::_1, &event));
    if (!m_inited) {
        return;
    }
    input()->setLastInputHandler(this);
    m_input->processFilters(std::bind(&InputEventFilter::keyEvent, std::placeholders::_1, &event));

    m_xkb->forwardModifiers();
    if (auto *inputmethod = kwinApp()->inputMethod()) {
        inputmethod->forwardModifiers(InputMethod::NoForce);
    }

    if (event.modifiersRelevantForGlobalShortcuts() == Qt::KeyboardModifier::NoModifier && type != QEvent::KeyRelease) {
        m_keyboardLayout->checkLayoutChange(previousLayout);
    }
}

}
