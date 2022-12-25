/*
    SPDX-FileCopyrightText: 2022 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "mousekeys.h"
#include "mousekeys_debug.h"

#include <cmath>

QString MouseKeysInputDevice::name() const
{
    return QStringLiteral("Mouse keys device");
}

KWin::LEDs MouseKeysInputDevice::leds() const
{
    return {};
}

void MouseKeysInputDevice::setLeds(KWin::LEDs leds)
{
}

void MouseKeysInputDevice::setEnabled(bool enabled)
{
}

bool MouseKeysInputDevice::isEnabled() const
{
    return true;
}

bool MouseKeysInputDevice::isKeyboard() const
{
    return false;
}

bool MouseKeysInputDevice::isLidSwitch() const
{
    return false;
}

bool MouseKeysInputDevice::isPointer() const
{
    return true;
}

bool MouseKeysInputDevice::isTabletModeSwitch() const
{
    return false;
}

bool MouseKeysInputDevice::isTabletPad() const
{
    return false;
}

bool MouseKeysInputDevice::isTabletTool() const
{
    return false;
}

bool MouseKeysInputDevice::isTouch() const
{
    return false;
}

bool MouseKeysInputDevice::isTouchpad() const
{
    return false;
}

MouseKeysFilter::MouseKeysFilter()
    : KWin::Plugin()
    , KWin::InputEventFilter(KWin::InputFilterOrder::MouseKeys)
    , m_configWatcher(KConfigWatcher::create(KSharedConfig::openConfig("kaccessrc")))
{
    const QLatin1String groupName("Mouse");
    connect(m_configWatcher.get(), &KConfigWatcher::configChanged, this, [this, groupName](const KConfigGroup &group) {
        if (group.name() == groupName) {
            loadConfig(group);
        }
    });
    loadConfig(m_configWatcher->config()->group(groupName));

    m_delayTimer.setSingleShot(true);
    connect(&m_delayTimer, &QTimer::timeout, this, &MouseKeysFilter::delayTriggered);
    connect(&m_repeatTimer, &QTimer::timeout, this, &MouseKeysFilter::repeatTriggered);
}

void MouseKeysFilter::loadConfig(const KConfigGroup &group)
{
    bool newEnabled = group.readEntry<bool>("MouseKeys", false);

    if (m_enabled && !newEnabled) {
        KWin::input()->uninstallInputEventFilter(this);
        KWin::input()->removeInputDevice(m_inputDevice.get());
        m_inputDevice = nullptr;
    }

    if (!m_enabled && newEnabled) {
        m_inputDevice = std::make_unique<MouseKeysInputDevice>();
        KWin::input()->addInputDevice(m_inputDevice.get());
        KWin::input()->installInputEventFilter(this);
    }

    m_enabled = newEnabled;
    m_stepsToMax = group.readEntry<int>("MKTimeToMax", 30);
    m_curve = group.readEntry<int>("MKCurve", 0);
    m_maxSpeed = group.readEntry<int>("MKMaxSpeed", 30);
    m_delay = group.readEntry<int>("MKDelay", 160);
    m_interval = group.readEntry<int>("MKInterval", 40);

    if (m_enabled) {
        m_delayTimer.setInterval(m_delay);
        m_repeatTimer.setInterval(m_interval);

        m_keyStates[KEY_KP1] = false;
        m_keyStates[KEY_KP2] = false;
        m_keyStates[KEY_KP3] = false;
        m_keyStates[KEY_KP4] = false;
        m_keyStates[KEY_KP6] = false;
        m_keyStates[KEY_KP7] = false;
        m_keyStates[KEY_KP8] = false;
        m_keyStates[KEY_KP9] = false;
        m_mouseButton = BTN_LEFT;
        m_currentKey = 0;
        m_currentStep = 0;
    }
}

static QPointF deltaForKey(int key)
{
    static constexpr int delta = 5;

    switch (key) {
    case KEY_KP1:
        return {-delta, delta};
    case KEY_KP2:
        return {0, delta};
    case KEY_KP3:
        return {delta, delta};
    case KEY_KP4:
        return {-delta, 0};
    case KEY_KP6:
        return {delta, 0};
    case KEY_KP7:
        return {-delta, -delta};
    case KEY_KP8:
        return {0, -delta};
    case KEY_KP9:
        return {delta, -delta};
    }

    return {0, 0};
}

void MouseKeysFilter::delayTriggered()
{
    m_repeatTimer.start();
    movePointer(deltaForKey(m_currentKey));
}

double MouseKeysFilter::deltaFactorForStep(int step) const
{
    // The algorithm to compute the delta is described in the XKB library
    // specification (http://www.xfree86.org/current/XKBlib.pdf), section 10.5

    if (step > m_stepsToMax) {
        return m_maxSpeed;
    }

    const double curveFactor = 1 + ((double)m_curve / 1000);

    return (m_maxSpeed / std::pow(m_stepsToMax, curveFactor)) * std::pow(step, curveFactor);
}

void MouseKeysFilter::repeatTriggered()
{
    ++m_currentStep;

    movePointer(deltaForKey(m_currentKey) * deltaFactorForStep(m_currentStep));
}

void MouseKeysFilter::handleKeyEvent(KWin::KeyboardKeyEvent *event)
{
    if (!m_keyStates[event->nativeScanCode] && event->state == KWin::KeyboardKeyState::Pressed && m_currentKey == 0) {
        m_keyStates[event->nativeScanCode] = true;
        m_delayTimer.start();
        m_currentKey = event->nativeScanCode;

        movePointer(deltaForKey(event->nativeScanCode));

    } else if (m_keyStates[event->nativeScanCode] && event->state == KWin::KeyboardKeyState::Released && event->nativeScanCode == m_currentKey) {
        m_keyStates[event->nativeScanCode] = false;
        m_delayTimer.stop();
        m_repeatTimer.stop();
        m_currentKey = 0;
        m_currentStep = 0;
    }
}

void MouseKeysFilter::movePointer(QPointF delta)
{
    const auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
    Q_EMIT m_inputDevice->pointerMotion(delta, delta, time, m_inputDevice.get());
}

bool MouseKeysFilter::keyboardKey(KWin::KeyboardKeyEvent *event)
{
    if (!m_enabled) {
        return false;
    }

    if (event->nativeScanCode == KEY_KP1
        || event->nativeScanCode == KEY_KP2
        || event->nativeScanCode == KEY_KP3
        || event->nativeScanCode == KEY_KP4
        || event->nativeScanCode == KEY_KP6
        || event->nativeScanCode == KEY_KP7
        || event->nativeScanCode == KEY_KP8
        || event->nativeScanCode == KEY_KP9) {

        handleKeyEvent(event);

        return true;
    }

    if (event->nativeScanCode == KEY_KP5) {
        const auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
        if (event->state == KWin::KeyboardKeyState::Pressed) {
            Q_EMIT m_inputDevice->pointerButtonChanged(m_mouseButton, KWin::PointerButtonState::Pressed, time, m_inputDevice.get());
        } else {
            Q_EMIT m_inputDevice->pointerButtonChanged(m_mouseButton, KWin::PointerButtonState::Released, time, m_inputDevice.get());
        }
        return true;
    }

    if (event->nativeScanCode == KEY_KPSLASH) {
        m_mouseButton = BTN_LEFT;
        return true;
    }

    if (event->nativeScanCode == KEY_KPASTERISK) {
        m_mouseButton = BTN_MIDDLE;
        return true;
    }

    if (event->nativeScanCode == KEY_KPMINUS) {
        m_mouseButton = BTN_RIGHT;
        return true;
    }

    return false;
}
