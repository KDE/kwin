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

QString InputDevice::sysName() const
{
    return {};
}

KWin::LEDs InputDevice::leds() const
{
    return {};
}

void InputDevice::setLeds(KWin::LEDs leds)
{
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
    return false;
}

bool InputDevice::isTabletModeSwitch() const
{
    return false;
}

bool InputDevice::isTabletPad() const
{
    return false;
}

bool InputDevice::isTabletTool() const
{
    return false;
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
    KWin::input()->addInputDevice(&m_inputDevice);
    const QLatin1String groupName("ButtonRebinds");
    connect(m_configWatcher.get(), &KConfigWatcher::configChanged, this, [this, groupName](const KConfigGroup &group) {
        if (group.parent().name() == groupName) {
            loadConfig(group.parent());
        } else if (group.parent().parent().name() == groupName) {
            loadConfig(group.parent().parent());
        }
    });
    loadConfig(m_configWatcher->config()->group(groupName));
}

ButtonRebindsFilter::~ButtonRebindsFilter()
{
    // on shutdown, input is destroyed before this filter
    if (KWin::input()) {
        KWin::input()->removeInputDevice(&m_inputDevice);
    }
}

void ButtonRebindsFilter::loadConfig(const KConfigGroup &group)
{
    Q_ASSERT(QLatin1String("ButtonRebinds") == group.name());
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
            insert(Pointer, {QString(), static_cast<uint>(mappedButton)}, action);
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
                insert(TabletPad, {tabletName, button}, entry);
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
                insert(TabletToolButtonType, {tabletToolName, button}, entry);
            }
        }
    }

    if (foundActions) {
        KWin::input()->installInputEventFilter(this);
    }
}

bool ButtonRebindsFilter::pointerEvent(KWin::MouseEvent *event, quint32 nativeButton)
{
    if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonRelease) {
        return false;
    }
    if (RebindScope::isRebinding()) {
        return false;
    }

    return send(Pointer, {{}, event->button()}, event->type() == QEvent::MouseButtonPress, event->timestamp());
}

bool ButtonRebindsFilter::tabletToolEvent(KWin::TabletEvent *event)
{
    if (RebindScope::isRebinding()) {
        return false;
    }
    m_tabletCursorPos = event->position();
    return false;
}

bool ButtonRebindsFilter::tabletPadButtonEvent(KWin::TabletPadButtonEvent *event)
{
    if (RebindScope::isRebinding()) {
        return false;
    }
    return send(TabletPad, {event->device->name(), event->button}, event->pressed, event->time);
}

bool ButtonRebindsFilter::tabletToolButtonEvent(KWin::TabletToolButtonEvent *event)
{
    if (RebindScope::isRebinding()) {
        return false;
    }
    m_tabletTool = event->tool;
    return send(TabletToolButtonType, {event->device->name(), event->button}, event->pressed, event->time);
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
    } else if (entry.first() == QLatin1String("Disabled")) {
        m_actions.at(type).insert(trigger, DisabledButton{});
    }
}

bool ButtonRebindsFilter::send(TriggerType type, const Trigger &trigger, bool pressed, std::chrono::microseconds timestamp)
{
    const auto &typeActions = m_actions.at(type);
    if (typeActions.isEmpty()) {
        return false;
    }

    const auto &action = typeActions[trigger];
    if (const QKeySequence *seq = std::get_if<QKeySequence>(&action)) {
        return sendKeySequence(*seq, pressed, timestamp);
    }
    if (const auto mb = std::get_if<MouseButton>(&action)) {
        if (pressed && type != Pointer) {
            sendMousePosition(m_tabletCursorPos, timestamp);
        }
        sendKeyModifiers(mb->modifiers, pressed, timestamp);
        return sendMouseButton(mb->button, pressed, timestamp);
    }
    if (const auto tb = std::get_if<TabletToolButton>(&action)) {
        return sendTabletToolButton(tb->button, pressed, timestamp);
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
        auto state = pressed ? KWin::InputDevice::KeyboardKeyPressed : KWin::InputDevice::KeyboardKeyReleased;
        Q_EMIT m_inputDevice.keyChanged(key, state, time, &m_inputDevice);
    };

    // handle modifier-only keys
    for (const auto &[keySymQt, keySymLinux] : s_modifierKeyTable) {
        if (key == keySymQt) {
            RebindScope scope;
            sendKey(keySymLinux);
            return true;
        }
    }

    QList<xkb_keysym_t> syms = KWin::Xkb::keysymsFromQtKey(keys[0]);

    // Use keysyms from the keypad if and only if KeypadModifier is set
    syms.erase(std::remove_if(syms.begin(), syms.end(), [keys](int sym) {
        bool onKeyPad = sym >= XKB_KEY_KP_Space && sym <= XKB_KEY_KP_Equal;
        if (keys[0] & Qt::KeypadModifier) {
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
    // KKeyServer returns upper case syms, lower it to not confuse modifiers handling
    std::optional<int> keyCode;
    std::optional<int> level;
    for (int sym : syms) {
        auto code = KWin::input()->keyboard()->xkb()->keycodeFromKeysym(sym);
        if (code) {
            keyCode = code->first;
            level = code->second;
            break;
        }
    }
    if (!keyCode) {
        qCWarning(KWIN_BUTTONREBINDS) << "Could not convert" << keys << "syms: " << syms << "to keycode";
        return false;
    }

    RebindScope scope;

    if (key & Qt::ShiftModifier || level == 1) {
        sendKey(KEY_LEFTSHIFT);
    }
    if (key & Qt::ControlModifier) {
        sendKey(KEY_LEFTCTRL);
    }
    if (key & Qt::AltModifier) {
        sendKey(KEY_LEFTALT);
    }
    if (key & Qt::MetaModifier) {
        sendKey(KEY_LEFTMETA);
    }

    sendKey(keyCode.value());
    return true;
}

bool ButtonRebindsFilter::sendKeyModifiers(const Qt::KeyboardModifiers &modifiers, bool pressed, std::chrono::microseconds time)
{
    if (modifiers == Qt::NoModifier) {
        return false;
    }

    auto sendKey = [this, pressed, time](xkb_keycode_t key) {
        auto state = pressed ? KWin::InputDevice::KeyboardKeyPressed : KWin::InputDevice::KeyboardKeyReleased;
        Q_EMIT m_inputDevice.keyChanged(key, state, time, &m_inputDevice);
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
    Q_EMIT m_inputDevice.pointerButtonChanged(button, KWin::InputDevice::PointerButtonState(pressed), time, &m_inputDevice);
    return true;
}

bool ButtonRebindsFilter::sendMousePosition(QPointF position, std::chrono::microseconds time)
{
    RebindScope scope;
    Q_EMIT m_inputDevice.pointerMotionAbsolute(position, time, &m_inputDevice);
    return true;
}

bool ButtonRebindsFilter::sendTabletToolButton(quint32 button, bool pressed, std::chrono::microseconds time)
{
    if (!m_tabletTool) {
        return false;
    }
    RebindScope scope;
    Q_EMIT m_inputDevice.tabletToolButtonEvent(button, pressed, m_tabletTool, time, &m_inputDevice);
    return true;
}

#include "moc_buttonrebindsfilter.cpp"
