/*
    SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redono.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "buttonrebindsfilter.h"
#include "buttonrebinds_debug.h"

#include "input_event.h"
#include "keyboard_input.h"

#include <KKeyServer>

#include <QMetaEnum>

#include <linux/input-event-codes.h>

#include <optional>

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
    Q_UNUSED(leds)
}

void InputDevice::setEnabled(bool enabled)
{
    Q_UNUSED(enabled)
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

static std::optional<int> keycodeFromKeysym(xkb_keysym_t keysym)
{
    auto xkb = KWin::input()->keyboard()->xkb();
    auto layout = xkb_state_serialize_layout(xkb->state(), XKB_STATE_LAYOUT_EFFECTIVE);
    const xkb_keycode_t max = xkb_keymap_max_keycode(xkb->keymap());
    for (xkb_keycode_t keycode = xkb_keymap_min_keycode(xkb->keymap()); keycode < max; keycode++) {
        uint levelCount = xkb_keymap_num_levels_for_key(xkb->keymap(), keycode, layout);
        for (uint currentLevel = 0; currentLevel < levelCount; currentLevel++) {
            const xkb_keysym_t *syms;
            uint num_syms = xkb_keymap_key_get_syms_by_level(xkb->keymap(), keycode, layout, currentLevel, &syms);
            for (uint sym = 0; sym < num_syms; sym++) {
                if (syms[sym] == keysym) {
                    return {keycode - 8};
                }
            }
        }
    }
    return {};
}

ButtonRebindsFilter::ButtonRebindsFilter()
    : KWin::Plugin()
    , KWin::InputEventFilter()
    , m_configWatcher(KConfigWatcher::create(KSharedConfig::openConfig("kcminputrc")))
{
    KWin::input()->addInputDevice(&m_inputDevice);
    const QLatin1String groupName("ButtonRebinds");
    connect(m_configWatcher.get(), &KConfigWatcher::configChanged, this, [this, groupName](const KConfigGroup &group) {
        if (group.parent().name() != groupName) {
            return;
        }
        loadConfig(group.parent());
    });
    loadConfig(m_configWatcher->config()->group(groupName));
}

void ButtonRebindsFilter::loadConfig(const KConfigGroup &group)
{
    KWin::input()->uninstallInputEventFilter(this);
    m_mouseMapping.clear();
    auto mouseButtonEnum = QMetaEnum::fromType<Qt::MouseButtons>();
    auto mouseGroup = group.group("Mouse");
    for (int i = 1; i <= 24; ++i) {
        const QByteArray buttonName = QByteArray("ExtraButton") + QByteArray::number(i);
        auto entry = mouseGroup.readEntry(buttonName.constData(), QStringList());
        if (entry.size() == 2 && entry.first() == QLatin1String("Key")) {
            auto keys = QKeySequence::fromString(entry.at(1), QKeySequence::PortableText);
            if (!keys.isEmpty()) {
                m_mouseMapping.insert(static_cast<Qt::MouseButton>(mouseButtonEnum.keyToValue(buttonName)), keys);
            }
        }
    }
    if (m_mouseMapping.size() != 0) {
        KWin::input()->prependInputEventFilter(this);
    }
}

bool ButtonRebindsFilter::pointerEvent(QMouseEvent *event, quint32 nativeButton)
{
    Q_UNUSED(nativeButton);

    if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonRelease) {
        return false;
    }

    const QKeySequence keys = m_mouseMapping.value(event->button());
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
    auto keyCode = keycodeFromKeysym(sym);
    if (!keyCode) {
        qCWarning(KWIN_BUTTONREBINDS) << "Could not convert" << keys << "sym: " << sym << "to keycode";
        return false;
    }

    auto sendKey = [this, event](xkb_keycode_t key) {
        auto state = event->type() == QEvent::MouseButtonPress ? KWin::InputRedirection::KeyboardKeyPressed : KWin::InputRedirection::KeyboardKeyReleased;
        Q_EMIT m_inputDevice.keyChanged(key, state, event->timestamp(), &m_inputDevice);
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
        sendKey(XKB_KEY_Super_L);
    }

    sendKey(keyCode.value());

    return true;
}
