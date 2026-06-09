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
    const QLatin1StringView groupName("Mouse");
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

MouseKeysFilter::~MouseKeysFilter()
{
    if (m_inputDevice) {
        KWin::input()->removeInputDevice(m_inputDevice.get());
    }
}

void MouseKeysFilter::loadConfig(const KConfigGroup &group)
{
    const bool newEnabled = group.readEntry<bool>("MouseKeys", false);

    if (m_enabled && !newEnabled) {
        KWin::input()->uninstallInputEventFilter(this);
        KWin::input()->removeInputDevice(m_inputDevice.get());
        m_inputDevice = nullptr;
        m_delayTimer.stop();
        m_repeatTimer.stop();
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

        m_heldKeys.clear();
        m_mouseButton = BTN_LEFT;
        m_currentStep = 0;
    }
}

QPointF MouseKeysFilter::currentDelta() const
{
    static constexpr int step = 5;

    QPointF delta(0, 0);
    for (const auto &key : m_heldKeys) {
        switch (key) {
        case KEY_KP1:
            delta += {-step, step};
            break;
        case KEY_KP2:
            delta += {0, step};
            break;
        case KEY_KP3:
            delta += {step, step};
            break;
        case KEY_KP4:
            delta += {-step, 0};
            break;
        case KEY_KP6:
            delta += {step, 0};
            break;
        case KEY_KP7:
            delta += {-step, -step};
            break;
        case KEY_KP8:
            delta += {0, -step};
            break;
        case KEY_KP9:
            delta += {step, -step};
            break;
        }
    }

    return delta;
}

void MouseKeysFilter::delayTriggered()
{
    m_repeatTimer.start();
    movePointer(currentDelta());
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

    movePointer(currentDelta() * deltaFactorForStep(m_currentStep));
}

void MouseKeysFilter::movePointer(QPointF delta)
{
    const auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
    Q_EMIT m_inputDevice->pointerMotion(delta, delta, time, m_inputDevice.get());
    Q_EMIT m_inputDevice->pointerFrame(m_inputDevice.get());
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
        if (event->state == KWin::KeyboardKeyState::Pressed) {
            m_heldKeys.insert(event->nativeScanCode);
            if (m_heldKeys.size() == 1) {
                m_delayTimer.start();
            }

            movePointer(currentDelta());
        } else if (event->state == KWin::KeyboardKeyState::Released) {
            if (m_heldKeys.remove(event->nativeScanCode)) {
                if (m_heldKeys.isEmpty()) {
                    m_delayTimer.stop();
                    m_repeatTimer.stop();
                    m_currentStep = 0;
                }
            }
        }

        return true;
    }

    if (event->nativeScanCode == KEY_KP5) {
        const auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
        if (event->state == KWin::KeyboardKeyState::Pressed) {
            Q_EMIT m_inputDevice->pointerButtonChanged(m_mouseButton, KWin::PointerButtonState::Pressed, time, m_inputDevice.get());
            Q_EMIT m_inputDevice->pointerFrame(m_inputDevice.get());
        } else if (event->state == KWin::KeyboardKeyState::Released) {
            Q_EMIT m_inputDevice->pointerButtonChanged(m_mouseButton, KWin::PointerButtonState::Released, time, m_inputDevice.get());
            Q_EMIT m_inputDevice->pointerFrame(m_inputDevice.get());
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
