/*
    SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redono.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "buttonrebindsfilter.h"
#include "buttonrebinds_debug.h"

#include "cursor.h"
#include "input_event.h"
#include "keyboard_input.h"
#include "xkb.h"

#include <KKeyServer>

#include <QMetaEnum>

#include <linux/input-event-codes.h>

#include <optional>

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
    return QString();
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

bool InputDevice::isAlphaNumericKeyboard() const
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
    : KWin::Plugin()
    , KWin::InputEventFilter()
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

void ButtonRebindsFilter::loadConfig(const KConfigGroup &group)
{
    Q_ASSERT(QLatin1String("ButtonRebinds") == group.name());
    KWin::input()->uninstallInputEventFilter(this);
    for (auto &action : m_actions) {
        action.clear();
    }

    bool foundActions = false;
    const auto mouseButtonEnum = QMetaEnum::fromType<Qt::MouseButtons>();
    const auto mouseGroup = group.group("Mouse");
    for (int i = 1; i <= 24; ++i) {
        const QByteArray buttonName = QByteArray("ExtraButton") + QByteArray::number(i);
        if (mouseGroup.hasKey(buttonName.constData())) {
            const auto entry = mouseGroup.readEntry(buttonName.constData(), QStringList());
            const auto button = static_cast<quint32>(mouseButtonEnum.keyToValue(buttonName));
            insert(Pointer, {QString(), button}, entry);
            foundActions = true;
        }
    }

    const auto tabletsGroup = group.group("Tablet");
    const auto tablets = tabletsGroup.groupList();
    for (const auto &tabletName : tablets) {
        const auto tabletGroup = tabletsGroup.group(tabletName);
        const auto tabletButtons = tabletGroup.keyList();
        for (const auto &buttonName : tabletButtons) {
            const auto entry = tabletGroup.readEntry(buttonName, QStringList());
            bool ok;
            const uint button = buttonName.toUInt(&ok);
            if (ok) {
                foundActions = true;
                insert(TabletPad, {tabletName, button}, entry);
            }
        }
    }

    const auto tabletToolsGroup = group.group("TabletTool");
    const auto tabletTools = tabletToolsGroup.groupList();
    for (const auto &tabletToolName : tabletTools) {
        const auto toolGroup = tabletToolsGroup.group(tabletToolName);
        const auto tabletToolButtons = toolGroup.keyList();
        for (const auto &buttonName : tabletToolButtons) {
            const auto entry = toolGroup.readEntry(buttonName, QStringList());
            bool ok;
            const uint button = buttonName.toUInt(&ok);
            if (ok) {
                foundActions = true;
                insert(TabletTool, {tabletToolName, button}, entry);
            }
        }
    }

    if (foundActions) {
        KWin::input()->prependInputEventFilter(this);
    }
}

bool ButtonRebindsFilter::pointerEvent(QMouseEvent *event, quint32 nativeButton)
{
    if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonRelease) {
        return false;
    }

    return send(Pointer, {{}, event->button()}, event->type() == QEvent::MouseButtonPress, event->timestamp());
}

bool ButtonRebindsFilter::tabletPadButtonEvent(uint button, bool pressed, const KWin::TabletPadId &tabletPadId, uint time)
{
    return send(TabletPad, {tabletPadId.name, button}, pressed, time);
}

bool ButtonRebindsFilter::tabletToolButtonEvent(uint button, bool pressed, const KWin::TabletToolId &tabletToolId, uint time)
{
    Q_UNUSED(tabletToolId);
    return send(TabletTool, {{}, button}, pressed, time);
}

void ButtonRebindsFilter::insert(TriggerType type, const Trigger &trigger, const QStringList &entry)
{
    if (entry.size() != 2) {
        qCWarning(KWIN_BUTTONREBINDS) << "Failed to rebind to" << entry;
        return;
    }
    if (entry.first() == QLatin1String("Key")) {
        const auto keys = QKeySequence::fromString(entry.at(1), QKeySequence::PortableText);
        if (!keys.isEmpty()) {
            m_actions[type].insert(trigger, keys);
        }
    } else if (entry.first() == QLatin1String("MouseButton")) {
        bool ok;
        const auto mb = quint32(entry.last().toInt(&ok));
        if (ok) {
            m_actions[type].insert(trigger, mb);
        } else {
            qCWarning(KWIN_BUTTONREBINDS) << "Could not convert" << entry << "into a mouse button";
        }
    }
}

bool ButtonRebindsFilter::send(TriggerType type, const Trigger &trigger, bool pressed, uint timestamp)
{
    const auto &typeActions = m_actions[type];
    if (typeActions.isEmpty()) {
        return false;
    }

    const auto &action = typeActions[trigger];
    if (const QKeySequence *seq = std::get_if<QKeySequence>(&action))
        return sendKeySequence(*seq, pressed, timestamp);
    else if (const auto mb = std::get_if<quint32>(&action))
        return sendMouseButton(*mb, pressed, timestamp);
    return false;
}

bool ButtonRebindsFilter::sendKeySequence(const QKeySequence &keys, bool pressed, uint time)
{
    if (keys.isEmpty()) {
        return false;
    }
    const auto &key = keys[0];

    int sym;
    if (!KKeyServer::keyQtToSymX(keys[0], &sym)) {
        qCWarning(KWIN_BUTTONREBINDS) << "Could not convert" << keys << "to keysym";
        return false;
    }
    // KKeyServer returns upper case syms, lower it to not confuse modifiers handling
    auto keyCode = KWin::input()->keyboard()->xkb()->keycodeFromKeysym(sym);
    if (!keyCode) {
        qCWarning(KWIN_BUTTONREBINDS) << "Could not convert" << keys << "sym: " << sym << "to keycode";
        return false;
    }

    auto sendKey = [this, pressed, time](xkb_keycode_t key) {
        auto state = pressed ? KWin::InputRedirection::KeyboardKeyPressed : KWin::InputRedirection::KeyboardKeyReleased;
        Q_EMIT m_inputDevice.keyChanged(key, state, time, &m_inputDevice);
    };

    if (key & Qt::ShiftModifier) {
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

bool ButtonRebindsFilter::sendMouseButton(quint32 button, bool pressed, uint time)
{
    Q_EMIT m_inputDevice.pointerButtonChanged(button, KWin::InputRedirection::PointerButtonState(pressed), time, &m_inputDevice);
    return true;
}
