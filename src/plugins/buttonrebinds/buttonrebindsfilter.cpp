/*
    SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redono.de>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "buttonrebindsfilter.h"
#include "buttonrebinds_debug.h"

#include "cursor.h"
#include "input_event.h"
#include "keyboard_input.h"
#include "xkb.h"

#include <QMetaEnum>

#include <linux/input-event-codes.h>

#include <array>
#include <optional>
#include <utility>

#include <private/qxkbcommon_p.h>

// Tells us that we are already in a binding event
class RebindScope
{
    static uint s_scopes;

public:
    RebindScope()
    {
        s_scopes++;
    }
    ~RebindScope()
    {
        Q_ASSERT(s_scopes > 0);
        s_scopes--;
    }
    Q_DISABLE_COPY_MOVE(RebindScope)
    static bool isRebinding()
    {
        return s_scopes > 0;
    }
};
uint RebindScope::s_scopes = 0;

quint32 qHash(const Trigger &t)
{
    return qHash(t.device) * (t.button + 1);
}

QString InputDevice::name() const
{
    return QStringLiteral("Button rebinding device");
}

void InputDevice::setEnabled(bool enabled)
{
}

bool InputDevice::isEnabled() const
{
    return true;
}

bool InputDevice::isKeyboard() const
{
    return true;
}

bool InputDevice::isLidSwitch() const
{
    return false;
}

bool InputDevice::isPointer() const
{
    return true;
}

bool InputDevice::isTabletModeSwitch() const
{
    return false;
}

bool InputDevice::isTabletPad() const
{
    return true;
}

bool InputDevice::isTabletTool() const
{
    return true;
}

bool InputDevice::isTouch() const
{
    return false;
}

bool InputDevice::isTouchpad() const
{
    return false;
}

ButtonRebindsFilter::ButtonRebindsFilter()
    : KWin::InputEventFilter(KWin::InputFilterOrder::ButtonRebind)
    , m_configWatcher(KConfigWatcher::create(KSharedConfig::openConfig("kcminputrc")))
{
    const QLatin1String groupName("ButtonRebinds");
    connect(m_configWatcher.get(), &KConfigWatcher::configChanged, this, [this, groupName](const KConfigGroup &group) {
        // We want to get the top-most parent in the config file, since our ButtonRebinds configs tend to be very nested
        auto parent = group.parent();
        while (parent.name() != "<default>") {
            if (parent.name() == groupName) {
                loadConfig(parent);
                return;
            }
            parent = parent.parent();
        }
    });
    loadConfig(m_configWatcher->config()->group(groupName));
}

ButtonRebindsFilter::~ButtonRebindsFilter()
{
    // on shutdown, input is destroyed before this filter
    if (KWin::input() && m_inputDevice) {
        KWin::input()->removeInputDevice(m_inputDevice.get());
    }
}

void ButtonRebindsFilter::loadConfig(const KConfigGroup &group)
{
    Q_ASSERT(QLatin1String("ButtonRebinds") == group.name());
    if (m_inputDevice) {
        KWin::input()->removeInputDevice(m_inputDevice.get());
        m_inputDevice.reset();
    }
    KWin::input()->uninstallInputEventFilter(this);
    for (auto &action : m_actions) {
        action.clear();
    }

    bool foundActions = false;

    const auto mouseButtonEnum = QMetaEnum::fromType<Qt::MouseButtons>();
    const auto mouseGroup = group.group(QStringLiteral("Mouse"));
    const auto mouseGroupKeys = mouseGroup.keyList();
    for (const QString &configKey : mouseGroupKeys) {
        const int mappedButton = mouseButtonEnum.keyToValue(configKey.toLatin1());
        if (mappedButton != -1) {
            const auto action = mouseGroup.readEntry(configKey, QStringList());
            insert(Pointer, {QString(), static_cast<uint>(mappedButton), 0}, action);
            foundActions = true;
        }
    }

    const auto tabletsGroup = group.group(QStringLiteral("Tablet"));
    const auto tablets = tabletsGroup.groupList();
    for (const auto &tabletName : tablets) {
        const auto tabletGroup = tabletsGroup.group(tabletName);
        const auto tabletButtons = tabletGroup.keyList();
        for (const auto &buttonName : tabletButtons) {
            const auto entry = tabletGroup.readEntry(buttonName, QStringList());
            bool ok = false;
            const uint button = buttonName.toUInt(&ok);
            if (ok) {
                foundActions = true;
                insert(TabletPad, {tabletName, button, 0}, entry);
            }
        }
    }

    const auto tabletToolsGroup = group.group(QStringLiteral("TabletTool"));
    const auto tabletTools = tabletToolsGroup.groupList();
    for (const auto &tabletToolName : tabletTools) {
        const auto toolGroup = tabletToolsGroup.group(tabletToolName);
        const auto tabletToolButtons = toolGroup.keyList();
        for (const auto &buttonName : tabletToolButtons) {
            const auto entry = toolGroup.readEntry(buttonName, QStringList());
            bool ok = false;
            const uint button = buttonName.toUInt(&ok);
            if (ok) {
                foundActions = true;
                insert(TabletToolButtonType, {tabletToolName, button, 0}, entry);
            }
        }
    }

    const auto tabletDialsGroup = group.group(QStringLiteral("TabletDial"));
    const auto tabletDials = tabletDialsGroup.groupList();
    for (const auto &tabletDialName : tabletDials) {
        const auto dialGroup = tabletDialsGroup.group(tabletDialName);
        const auto tabletDialButtons = dialGroup.keyList();
        for (const auto &buttonName : tabletDialButtons) {
            const auto entry = dialGroup.readEntry(buttonName, QStringList());
            bool ok = false;
            const uint button = buttonName.toUInt(&ok);
            if (ok) {
                foundActions = true;
                insert(TabletDial, {tabletDialName, button, 0}, entry);
            }
        }
    }

    const auto tabletRingsGroup = group.group(QStringLiteral("TabletRing"));
    const auto tabletRings = tabletRingsGroup.groupList();
    for (const auto &tabletRingName : tabletRings) {
        const auto ringModesGroup = tabletRingsGroup.group(tabletRingName);
        for (const auto &modeGroupName : ringModesGroup.groupList()) {
            const auto ringGroup = ringModesGroup.group(modeGroupName);
            const auto tabletRingButtons = ringGroup.keyList();
            for (const auto &buttonName : tabletRingButtons) {
                const auto entry = ringGroup.readEntry(buttonName, QStringList());
                bool buttonOk = false;
                const uint button = buttonName.toUInt(&buttonOk);
                bool modeOk = false;
                const uint mode = modeGroupName.toUInt(&modeOk);
                if (buttonOk && modeOk) {
                    foundActions = true;
                    insert(TabletRing, {tabletRingName, button, mode}, entry);
                }
            }
        }
    }

    if (foundActions) {
        KWin::input()->installInputEventFilter(this);

        m_inputDevice = std::make_unique<InputDevice>();
        KWin::input()->addInputDevice(m_inputDevice.get());
    }
}

bool ButtonRebindsFilter::pointerButton(KWin::PointerButtonEvent *event)
{
    if (RebindScope::isRebinding()) {
        return false;
    }

    return send(Pointer, {{}, event->button, 0}, event->state == KWin::PointerButtonState::Pressed, 0, event->timestamp);
}

bool ButtonRebindsFilter::tabletToolProximityEvent(KWin::TabletToolProximityEvent *event)
{
    if (RebindScope::isRebinding()) {
        return false;
    }
    m_tabletCursorPos = event->position;
    return false;
}

bool ButtonRebindsFilter::tabletToolAxisEvent(KWin::TabletToolAxisEvent *event)
{
    if (RebindScope::isRebinding()) {
        return false;
    }
    m_tabletCursorPos = event->position;
    return false;
}

bool ButtonRebindsFilter::tabletToolTipEvent(KWin::TabletToolTipEvent *event)
{
    if (RebindScope::isRebinding()) {
        return false;
    }
    m_tabletCursorPos = event->position;
    return false;
}

bool ButtonRebindsFilter::tabletPadButtonEvent(KWin::TabletPadButtonEvent *event)
{
    if (RebindScope::isRebinding()) {
        return false;
    }
    return send(TabletPad, {event->device->name(), event->button, 0}, event->pressed, 0, event->time);
}

bool ButtonRebindsFilter::tabletToolButtonEvent(KWin::TabletToolButtonEvent *event)
{
    if (RebindScope::isRebinding()) {
        return false;
    }
    m_tabletTool = event->tool;
    return send(TabletToolButtonType, {event->device->name(), event->button, 0}, event->pressed, 0, event->time);
}

bool ButtonRebindsFilter::tabletPadDialEvent(KWin::TabletPadDialEvent *event)
{
    if (RebindScope::isRebinding()) {
        return false;
    }
    return send(TabletDial, {event->device->name(), static_cast<uint>(event->number), 0}, false, event->delta, event->time);
}

bool ButtonRebindsFilter::tabletPadRingEvent(KWin::TabletPadRingEvent *event)
{
    if (RebindScope::isRebinding()) {
        return false;
    }

    // We only get the ring's position in degrees
    // So we need to start when it's pressed, and stop when released (that's given as -1)
    // And only emit events with the delta passes a certain threshold
    if (event->position != -1) {
        if (m_initialRingPosition == -1) {
            m_initialRingPosition = event->position;
        }

        // The maximum number of degrees a device could ever go from one position to the other. This is completely arbitrary.
        // I would guarantee that most devices emit ring events in small, ~5 degree increments, but *we don't know* how big their increments are but it's safe to assume it's less than this.
        constexpr int maximumIncrement = 180;

        int delta = m_initialRingPosition - event->position;

        // If the delta is something crazy, and could never be feasibly emitted by the device, then it's probably
        // because we made a complete circle.
        // For example, going from 0->355. That's a delta of 5 degrees, not -355 degrees!
        if (abs(delta) > maximumIncrement) {
            if (delta < 0) {
                delta += 360;
            } else {
                delta -= 360;
            }
        }

        // Rings seem to have a minimum delta of 5 degrees, so we can assume that's one "unit".
        const bool sent = send(TabletRing, {event->device->name(), static_cast<uint>(event->number), event->mode}, false, delta * 120, event->time);
        // If accepted (that means the threshold is met) then we want to reset ourselves
        if (sent) {
            m_initialRingPosition = event->position;
        }
        return sent;
    } else {
        m_initialRingPosition = -1;
    }

    return false;
}

void ButtonRebindsFilter::insert(TriggerType type, const Trigger &trigger, const QStringList &entry)
{
    if (entry.empty()) {
        qCWarning(KWIN_BUTTONREBINDS) << "Failed to rebind to" << entry;
        return;
    }
    if (entry.first() == QLatin1String("Key")) {
        if (entry.size() != 2) {
            qCWarning(KWIN_BUTTONREBINDS) << "Invalid key" << entry;
            return;
        }

        const auto keys = QKeySequence::fromString(entry.at(1), QKeySequence::PortableText);
        if (!keys.isEmpty()) {
            m_actions.at(type).insert(trigger, keys);
        }
    } else if (entry.first() == QLatin1String("AxisKey")) {
        if (entry.size() != 4) {
            qCWarning(KWIN_BUTTONREBINDS) << "Invalid axis key" << entry;
            return;
        }

        const auto upKey = QKeySequence::fromString(entry.at(1), QKeySequence::PortableText);
        const auto downKey = QKeySequence::fromString(entry.at(2), QKeySequence::PortableText);
        const auto threshold = entry.at(3).toInt();

        if (!upKey.isEmpty() && !downKey.isEmpty()) {
            m_actions.at(type).insert(trigger, AxisKeybind{upKey, downKey, threshold});
        }
    } else if (entry.first() == QLatin1String("MouseButton")) {
        if (entry.size() < 2) {
            qCWarning(KWIN_BUTTONREBINDS) << "Invalid mouse button" << entry;
            return;
        }

        bool ok = false;
        MouseButton mb{entry[1].toUInt(&ok), {}};

        // Last bit is the keyboard mods
        if (entry.size() == 3) {
            const auto keyboardModsRaw = entry.last().toInt(&ok);
            mb.modifiers = Qt::KeyboardModifiers{keyboardModsRaw};
        }

        if (ok) {
            m_actions.at(type).insert(trigger, mb);
        } else {
            qCWarning(KWIN_BUTTONREBINDS) << "Could not convert" << entry << "into a mouse button";
        }
    } else if (entry.first() == QLatin1String("TabletToolButton")) {
        if (entry.size() != 2) {
            qCWarning(KWIN_BUTTONREBINDS)
                << "Invalid tablet tool button" << entry;
            return;
        }

        bool ok = false;
        const TabletToolButton tb{entry.last().toUInt(&ok)};
        if (ok) {
            m_actions.at(type).insert(trigger, tb);
        } else {
            qCWarning(KWIN_BUTTONREBINDS) << "Could not convert" << entry << "into a mouse button";
        }
    } else if (entry.first() == QLatin1String("Scroll")) {
        m_actions.at(type).insert(trigger, ScrollWheel{});
    } else if (entry.first() == QLatin1String("Disabled")) {
        m_actions.at(type).insert(trigger, DisabledButton{});
    }
}

bool ButtonRebindsFilter::send(TriggerType type, const Trigger &trigger, bool pressed, double delta, std::chrono::microseconds timestamp)
{
    const auto &typeActions = m_actions.at(type);
    if (typeActions.isEmpty()) {
        return false;
    }

    const auto &action = typeActions[trigger];
    if (const QKeySequence *seq = std::get_if<SingleKeybind>(&action)) {
        return sendKeySequence(*seq, pressed, timestamp);
    }
    if (const AxisKeybind *bind = std::get_if<AxisKeybind>(&action)) {
        if (std::abs(delta) < bind->threshold) {
            return false;
        }

        const auto sequence = delta > 0 ? bind->up : bind->down;

        bool sentKeyEvent = false;
        sentKeyEvent |= sendKeySequence(sequence, true, timestamp);
        sentKeyEvent |= sendKeySequence(sequence, false, timestamp);
        return sentKeyEvent;
    }
    if (const auto mb = std::get_if<MouseButton>(&action)) {
        bool sentMouseEvent = false;
        if (pressed && type != Pointer) {
            sentMouseEvent |= sendMousePosition(m_tabletCursorPos, timestamp);
        }
        sendKeyModifiers(mb->modifiers, pressed, timestamp);
        sentMouseEvent |= sendMouseButton(mb->button, pressed, timestamp);
        if (sentMouseEvent) {
            sendMouseFrame();
        }
        return sentMouseEvent;
    }
    if (const auto tb = std::get_if<TabletToolButton>(&action)) {
        return sendTabletToolButton(tb->button, pressed, timestamp);
    }
    if (std::get_if<ScrollWheel>(&action)) {
        bool sentMouseEvent = false;
        if (type != Pointer) {
            sentMouseEvent |= sendMousePosition(m_tabletCursorPos, timestamp);
        }
        sentMouseEvent |= sendScrollWheel(delta, timestamp);
        if (sentMouseEvent) {
            sendMouseFrame();
        }
        return sentMouseEvent;
    }
    if (std::get_if<DisabledButton>(&action)) {
        // Intentional, we don't want to anything to anybody
        return true;
    }
    return false;
}

static constexpr std::array<std::pair<int, int>, 4> s_modifierKeyTable = {
    std::pair(Qt::Key_Control, KEY_LEFTCTRL),
    std::pair(Qt::Key_Alt, KEY_LEFTALT),
    std::pair(Qt::Key_Shift, KEY_LEFTSHIFT),
    std::pair(Qt::Key_Meta, KEY_LEFTMETA),
};

bool ButtonRebindsFilter::sendKeySequence(const QKeySequence &keys, bool pressed, std::chrono::microseconds time)
{
    if (keys.isEmpty()) {
        return false;
    }

    const auto &key = keys[0];
    auto sendKey = [this, pressed, time](xkb_keycode_t key) {
        auto state = pressed ? KWin::KeyboardKeyState::Pressed : KWin::KeyboardKeyState::Released;
        Q_EMIT m_inputDevice->keyChanged(key, state, time, m_inputDevice.get());
    };

    // handle modifier-only keys
    for (const auto &[keySymQt, keySymLinux] : s_modifierKeyTable) {
        if (key == QKeyCombination::fromCombined(keySymQt)) {
            RebindScope scope;
            sendKey(keySymLinux);
            return true;
        }
    }

    QList<xkb_keysym_t> syms = KWin::Xkb::keysymsFromQtKey(keys[0]);

    // Use keysyms from the keypad if and only if KeypadModifier is set
    syms.erase(std::remove_if(syms.begin(), syms.end(), [keys](int sym) {
        bool onKeyPad = sym >= XKB_KEY_KP_Space && sym <= XKB_KEY_KP_Equal;
        if (keys[0].keyboardModifiers() & Qt::KeypadModifier) {
            return !onKeyPad;
        } else {
            return onKeyPad;
        }
    }),
               syms.end());

    if (syms.empty()) {
        qCWarning(KWIN_BUTTONREBINDS) << "Could not convert" << keys << "to keysym";
        return false;
    }
    std::optional<KWin::Xkb::KeyCode> code;
    // KKeyServer returns upper case syms, lower it to not confuse modifiers handling
    for (int sym : syms) {
        code = KWin::input()->keyboard()->xkb()->keycodeFromKeysym(sym);
        if (code) {
            break;
        }
    }
    if (!code) {
        qCWarning(KWIN_BUTTONREBINDS) << "Could not convert" << keys << "syms: " << syms << "to keycode";
        return false;
    }

    RebindScope scope;

    if (key.keyboardModifiers() & Qt::ShiftModifier || code->level == 1) {
        sendKey(KEY_LEFTSHIFT);
    }
    if (key.keyboardModifiers() & Qt::ControlModifier) {
        sendKey(KEY_LEFTCTRL);
    }
    if (key.keyboardModifiers() & Qt::AltModifier) {
        sendKey(KEY_LEFTALT);
    }
    if (key.keyboardModifiers() & Qt::MetaModifier) {
        sendKey(KEY_LEFTMETA);
    }

    sendKey(code->keyCode);
    return true;
}

bool ButtonRebindsFilter::sendKeyModifiers(const Qt::KeyboardModifiers &modifiers, bool pressed, std::chrono::microseconds time)
{
    if (modifiers == Qt::NoModifier) {
        return false;
    }

    auto sendKey = [this, pressed, time](xkb_keycode_t key) {
        auto state = pressed ? KWin::KeyboardKeyState::Pressed : KWin::KeyboardKeyState::Released;
        Q_EMIT m_inputDevice->keyChanged(key, state, time, m_inputDevice.get());
    };

    if (modifiers.testFlag(Qt::ShiftModifier)) {
        sendKey(KEY_LEFTSHIFT);
    }
    if (modifiers.testFlag(Qt::ControlModifier)) {
        sendKey(KEY_LEFTCTRL);
    }
    if (modifiers.testFlag(Qt::AltModifier)) {
        sendKey(KEY_LEFTALT);
    }
    if (modifiers.testFlag(Qt::MetaModifier)) {
        sendKey(KEY_LEFTMETA);
    }

    return true;
}

bool ButtonRebindsFilter::sendMouseButton(quint32 button, bool pressed, std::chrono::microseconds time)
{
    RebindScope scope;
    Q_EMIT m_inputDevice->pointerButtonChanged(button, KWin::PointerButtonState(pressed), time, m_inputDevice.get());
    return true;
}

bool ButtonRebindsFilter::sendMousePosition(QPointF position, std::chrono::microseconds time)
{
    RebindScope scope;
    Q_EMIT m_inputDevice->pointerMotionAbsolute(position, time, m_inputDevice.get());
    return true;
}

bool ButtonRebindsFilter::sendMouseFrame()
{
    RebindScope scope;
    Q_EMIT m_inputDevice->pointerFrame(m_inputDevice.get());
    return true;
}

bool ButtonRebindsFilter::sendTabletToolButton(quint32 button, bool pressed, std::chrono::microseconds time)
{
    if (!m_tabletTool) {
        return false;
    }
    RebindScope scope;
    Q_EMIT m_inputDevice->tabletToolButtonEvent(button, pressed, m_tabletTool, time, m_inputDevice.get());
    return true;
}

bool ButtonRebindsFilter::sendScrollWheel(double v120, std::chrono::microseconds time)
{
    RebindScope scope;
    Q_EMIT m_inputDevice->pointerAxisChanged(KWin::PointerAxis::Vertical, v120 > 0 ? 15 : -15, v120, KWin::PointerAxisSource::Wheel, false, time, m_inputDevice.get());
    Q_EMIT m_inputDevice->pointerFrame(m_inputDevice.get());
    return true;
}

#include "moc_buttonrebindsfilter.cpp"
