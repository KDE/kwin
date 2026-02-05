/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Yelsin Sepulveda <yelsin.sepulveda@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "emulatedinputdevice.h"
#include "gamecontroller_logging.h"
#include "inputmethod.h"
#include "main.h"
#include "xkb.h"

#include <QKeySequence>

namespace KWin
{

// Tells us that we are already in a binding event
class RebindScope
{
    static uint s_scopes;

public:
    explicit RebindScope()
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

static constexpr double s_deadzone = 0.25;

EmulatedInputDevice::EmulatedInputDevice(libevdev *device)
    : m_device(device)
    , m_leftStickMin(libevdev_get_abs_minimum(device, ABS_X), libevdev_get_abs_minimum(device, ABS_Y))
    , m_leftStickMax(libevdev_get_abs_maximum(device, ABS_X), libevdev_get_abs_maximum(device, ABS_Y))
    , m_rightStickMin(libevdev_get_abs_minimum(device, ABS_RX), libevdev_get_abs_minimum(device, ABS_RY))
    , m_rightStickMax(libevdev_get_abs_maximum(device, ABS_RX), libevdev_get_abs_maximum(device, ABS_RY))
{
    m_timer.setSingleShot(false);
    m_timer.setInterval(5);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &EmulatedInputDevice::handleAnalogStickInput);
}

EmulatedInputDevice::~EmulatedInputDevice()
{
}

void EmulatedInputDevice::emulateInputDevice(input_event &ev)
{
    if (ev.type == EV_KEY) {
        qCDebug(KWIN_GAMECONTROLLER) << "Face button pressed: Simulating User Activity";
        evkeyMapping(&ev);
    } else if (ev.type == EV_ABS) {
        qCDebug(KWIN_GAMECONTROLLER) << "Analog buttons pressed: Simulating User Activity";
        evabsMapping(&ev);
    }
}

void EmulatedInputDevice::evkeyMapping(input_event *ev)
{
    KeyboardKeyState state = ev->value ? KWin::KeyboardKeyState::Pressed : KWin::KeyboardKeyState::Released;
    const std::chrono::microseconds time = std::chrono::seconds(ev->time.tv_sec) + std::chrono::microseconds(ev->time.tv_usec);

    switch (ev->code) {
    case BTN_SOUTH: // A button → Enter
        sendKeySequence(QKeySequence(Qt::Key_Return), state, time);
        break;
    case BTN_EAST: // B button → Escape
        sendKeySequence(QKeySequence(Qt::Key_Escape), state, time);
        break;
    case BTN_NORTH: // X button → Virtual Keyboard
        // TO-DO
        // toggle Virtual Keyboard not working.. Activated but does not show on-screen
        if (state == KeyboardKeyState::Pressed) {
            EmulatedInputDevice::toggleVirtualKeyboard();
            qCDebug(KWIN_GAMECONTROLLER) << "On-Screen Keyboard toggled:" << kwinApp()->inputMethod()->isActive();
        }
        break;
    case BTN_WEST: // Y button → Space
        sendKeySequence(QKeySequence(Qt::Key_Space), state, time);
        break;
    case BTN_TL: // L button → Ctrl
        sendKeySequence(QKeySequence(Qt::Key_Control), state, time);
        break;
    case BTN_TR: // R button → Alt
        sendKeySequence(QKeySequence(Qt::Key_Alt), state, time);
        break;
    case BTN_START: // START button → Meta
        sendKeySequence(QKeySequence(Qt::Key_Meta), state, time);
        break;
    case BTN_SELECT: // SELECT
        break;
    // Add more button mappings here as needed
    default:
        break;
    }
}

void EmulatedInputDevice::evabsMapping(input_event *ev)
{
    const int maximumValue = libevdev_get_abs_maximum(m_device, ev->code);
    const PointerButtonState pointerState = maximumValue > 0 && ev->value >= maximumValue * 0.9 ? KWin::PointerButtonState::Pressed : KWin::PointerButtonState::Released;
    KeyboardKeyState keyState = ev->value ? KWin::KeyboardKeyState::Pressed : KWin::KeyboardKeyState::Released;
    const std::chrono::microseconds time = std::chrono::seconds(ev->time.tv_sec) + std::chrono::microseconds(ev->time.tv_usec);

    switch (ev->code) {
    // analog triggers
    case ABS_RZ: // R2 → left click
    case ABS_HAT2X: // right trigger on the steam controller
        if (pointerState != m_leftClick) {
            m_leftClick = pointerState;
            Q_EMIT pointerButtonChanged(BTN_LEFT, pointerState, time, this);
            Q_EMIT pointerFrame(this);
        }
        break;
    case ABS_Z: // L2 → right click
    case ABS_HAT2Y: // left trigger on the steam controller
        if (pointerState != m_rightClick) {
            m_rightClick = pointerState;
            Q_EMIT pointerButtonChanged(BTN_RIGHT, pointerState, time, this);
            Q_EMIT pointerFrame(this);
        }
        break;

    // D-pad
    case ABS_HAT0X:
        if (ev->value == -1) { // Left
            sendKeySequence(Qt::Key_Left, keyState, time);
        }
        if (ev->value == 1) { // Right
            sendKeySequence(Qt::Key_Right, keyState, time);
        }
        if (ev->value == 0) { // Release
            sendKeySequence(Qt::Key_Left, keyState, time);
            sendKeySequence(Qt::Key_Right, keyState, time);
        }
        break;
    case ABS_HAT0Y:
        if (ev->value == -1) { // Up
            sendKeySequence(Qt::Key_Up, keyState, time);
        }
        if (ev->value == 1) { // Down
            sendKeySequence(Qt::Key_Down, keyState, time);
        }
        if (ev->value == 0) { // Release
            sendKeySequence(Qt::Key_Up, keyState, time);
            sendKeySequence(Qt::Key_Down, keyState, time);
        }
        break;

    // analog/joy sticks
    case ABS_X:
    case ABS_Y:
    case ABS_RX:
    case ABS_RY:
        updateAnalogStick(ev);
        break;

    default:
        break;
    }
}

void EmulatedInputDevice::updateAnalogStick(input_event *ev)
{
    switch (ev->code) {
    case ABS_X: {
        const double halfRange = (m_leftStickMax.x() - m_leftStickMin.x()) / 2;
        if (halfRange > 0) {
            const double center = m_leftStickMin.x() + halfRange;
            const double value = (ev->value - center) / halfRange;
            m_leftStick.setX(std::abs(value) >= s_deadzone ? value : 0);
        } else {
            m_leftStick.setX(0);
        }
        break;
    }
    case ABS_Y: {
        const double halfRange = (m_leftStickMax.y() - m_leftStickMin.y()) / 2;
        if (halfRange > 0) {
            const double center = m_leftStickMin.y() + halfRange;
            const double value = (ev->value - center) / halfRange;
            m_leftStick.setY(std::abs(value) >= s_deadzone ? value : 0);
        } else {
            m_leftStick.setY(0);
        }
        break;
    }
    case ABS_RX: {
        const double halfRange = (m_rightStickMax.x() - m_rightStickMin.x()) / 2;
        if (halfRange > 0) {
            const double center = m_rightStickMin.x() + halfRange;
            const double value = (ev->value - center) / halfRange;
            m_rightStick.setX(std::abs(value) >= s_deadzone ? value : 0);
        } else {
            m_rightStick.setX(0);
        }
        break;
    }
    case ABS_RY: {
        const double halfRange = (m_rightStickMax.y() - m_rightStickMin.y()) / 2;
        if (halfRange > 0) {
            const double center = m_rightStickMin.y() + halfRange;
            const double value = (ev->value - center) / halfRange;
            m_rightStick.setY(std::abs(value) >= s_deadzone ? value : 0);
        } else {
            m_rightStick.setY(0);
        }
        break;
    }
    }

    if (m_leftStick.isNull() && m_rightStick.isNull()) {
        m_timer.stop();
    } else if (!m_timer.isActive()) {
        m_timer.start();
    }
}

void EmulatedInputDevice::handleAnalogStickInput()
{
    auto time = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch());

    const QPointF &stick = m_leftStick.manhattanLength() > m_rightStick.manhattanLength() ? m_leftStick : m_rightStick;

    // Calculate stick magnitude OR how far from center
    qreal magnitude = std::hypot(stick.x(), stick.y());
    magnitude = std::clamp(magnitude, -1.0, 1.0);

    // We use an exponential curve for speed.
    // Provides more precise control at low speeds
    // and faster movement at high deflection (magnitude)
    const qreal speedMultiplier = MOUSE_BASE_SPEED + (MOUSE_MAX_SPEED - MOUSE_BASE_SPEED) * std::pow(magnitude, SPEED_CURVE_EXPONENT);
    const QPointF delta = stick * speedMultiplier;
    if (!delta.isNull()) {
        Q_EMIT pointerMotion(delta, delta, time, this);
        Q_EMIT pointerFrame(this);
    }
}

static constexpr std::array<std::pair<int, int>, 4> s_modifierKeyTable = {
    std::pair(Qt::Key_Control, KEY_LEFTCTRL),
    std::pair(Qt::Key_Alt, KEY_LEFTALT),
    std::pair(Qt::Key_Shift, KEY_LEFTSHIFT),
    std::pair(Qt::Key_Meta, KEY_LEFTMETA),
};

bool EmulatedInputDevice::sendKeySequence(const QKeySequence &keys, KeyboardKeyState pressed, std::chrono::microseconds time)
{
    if (keys.isEmpty()) {
        return false;
    }

    const auto &key = keys[0];
    auto sendKey = [this, pressed, time](xkb_keycode_t key) {
        Q_EMIT keyChanged(key, pressed, time, this);
    };

    // handle modifier-only keys
    for (const auto &[keySymQt, keySymLinux] : s_modifierKeyTable) {
        if (key == QKeyCombination::fromCombined(keySymQt)) {
            RebindScope scope;
            sendKey(keySymLinux);
            return true;
        }
    }

    if (key == QKeyCombination::fromCombined(Qt::Key_Left)) {
        RebindScope scope;
        sendKey(KEY_LEFT);
        return true;
    }
    if (key == QKeyCombination::fromCombined(Qt::Key_Right)) {
        RebindScope scope;
        sendKey(KEY_RIGHT);
        return true;
    }
    if (key == QKeyCombination::fromCombined(Qt::Key_Up)) {
        RebindScope scope;
        sendKey(KEY_UP);
        return true;
    }
    if (key == QKeyCombination::fromCombined(Qt::Key_Down)) {
        RebindScope scope;
        sendKey(KEY_DOWN);
        return true;
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
        qCWarning(KWIN_GAMECONTROLLER) << "Could not convert" << keys << "to keysym";
        return false;
    }
    // KKeyServer returns upper case syms, lower it to not confuse modifiers handling
    std::optional<KWin::Xkb::KeyCode> code;
    for (int sym : syms) {
        code = KWin::input()->keyboard()->xkb()->keycodeFromKeysym(sym);
        if (code) {
            // started to cause errors all of a sudden
            // keyCode = code->first;
            // level = code->second;
            break;
        }
    }
    if (!code) {
        qCWarning(KWIN_GAMECONTROLLER) << "Could not convert" << keys << "syms:" << syms << "to keycode";
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

void EmulatedInputDevice::toggleVirtualKeyboard()
{
    if (!kwinApp()->inputMethod()->isActive()) {
        kwinApp()->inputMethod()->forceActivate();
        return;
    }
    kwinApp()->inputMethod()->setActive(false);
}

QString EmulatedInputDevice::name() const
{
    return QStringLiteral("KWin Virtual Input Device: Game Controller Desktop Mode");
}

void EmulatedInputDevice::setEnabled(bool enabled)
{
}

bool EmulatedInputDevice::isEnabled() const
{
    return true;
}

bool EmulatedInputDevice::isKeyboard() const
{
    return true;
}

bool EmulatedInputDevice::isLidSwitch() const
{
    return false;
}

bool EmulatedInputDevice::isPointer() const
{
    return true;
}

bool EmulatedInputDevice::isTabletModeSwitch() const
{
    return false;
}

bool EmulatedInputDevice::isTabletPad() const
{
    return false;
}

bool EmulatedInputDevice::isTabletTool() const
{
    return false;
}

bool EmulatedInputDevice::isTouch() const
{
    return false;
}

bool EmulatedInputDevice::isTouchpad() const
{
    return false;
}

} // namespace KWin

#include "moc_emulatedinputdevice.cpp"
