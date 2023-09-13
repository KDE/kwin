/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fakeinputdevice.h"
#include "wayland/fakeinput.h"

namespace KWin
{
static int s_lastDeviceId = 0;

FakeInputDevice::FakeInputDevice(FakeInputDeviceInterface *device, QObject *parent)
    : InputDevice(parent)
    , m_name(QStringLiteral("Fake Input Device %1").arg(++s_lastDeviceId))
{
    connect(device, &FakeInputDeviceInterface::authenticationRequested, this, [device](const QString &application, const QString &reason) {
        // TODO: make secure
        device->setAuthentication(true);
    });
    connect(device, &FakeInputDeviceInterface::pointerMotionRequested, this, [this](const QPointF &delta) {
        const auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
        Q_EMIT pointerMotion(delta, delta, time, this);
        Q_EMIT pointerFrame(this);
    });
    connect(device, &FakeInputDeviceInterface::pointerMotionAbsoluteRequested, this, [this](const QPointF &pos) {
        const auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
        Q_EMIT pointerMotionAbsolute(pos, time, this);
        Q_EMIT pointerFrame(this);
    });
    connect(device, &FakeInputDeviceInterface::pointerButtonPressRequested, this, [this](quint32 button) {
        const auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
        Q_EMIT pointerButtonChanged(button, InputRedirection::PointerButtonPressed, time, this);
        Q_EMIT pointerFrame(this);
    });
    connect(device, &FakeInputDeviceInterface::pointerButtonReleaseRequested, this, [this](quint32 button) {
        const auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
        Q_EMIT pointerButtonChanged(button, InputRedirection::PointerButtonReleased, time, this);
        Q_EMIT pointerFrame(this);
    });
    connect(device, &FakeInputDeviceInterface::pointerAxisRequested, this, [this](Qt::Orientation orientation, qreal delta) {
        InputRedirection::PointerAxis axis;
        switch (orientation) {
        case Qt::Horizontal:
            axis = InputRedirection::PointerAxisHorizontal;
            break;
        case Qt::Vertical:
            axis = InputRedirection::PointerAxisVertical;
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
        const auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
        Q_EMIT pointerAxisChanged(axis, delta, 0, InputRedirection::PointerAxisSourceUnknown, time, this);
        Q_EMIT pointerFrame(this);
    });
    connect(device, &FakeInputDeviceInterface::touchDownRequested, this, [this](qint32 id, const QPointF &pos) {
        const auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
        Q_EMIT touchDown(id, pos, time, this);
    });
    connect(device, &FakeInputDeviceInterface::touchMotionRequested, this, [this](qint32 id, const QPointF &pos) {
        const auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
        Q_EMIT touchMotion(id, pos, time, this);
    });
    connect(device, &FakeInputDeviceInterface::touchUpRequested, this, [this](qint32 id) {
        const auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
        Q_EMIT touchUp(id, time, this);
    });
    connect(device, &FakeInputDeviceInterface::touchCancelRequested, this, [this]() {
        Q_EMIT touchCanceled(this);
    });
    connect(device, &FakeInputDeviceInterface::touchFrameRequested, this, [this]() {
        Q_EMIT touchFrame(this);
    });
    connect(device, &FakeInputDeviceInterface::keyboardKeyPressRequested, this, [this](quint32 button) {
        const auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
        Q_EMIT keyChanged(button, InputRedirection::KeyboardKeyPressed, time, this);
    });
    connect(device, &FakeInputDeviceInterface::keyboardKeyReleaseRequested, this, [this](quint32 button) {
        const auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
        Q_EMIT keyChanged(button, InputRedirection::KeyboardKeyReleased, time, this);
    });
}

QString FakeInputDevice::sysName() const
{
    return QString();
}

QString FakeInputDevice::name() const
{
    return m_name;
}

bool FakeInputDevice::isEnabled() const
{
    return true;
}

void FakeInputDevice::setEnabled(bool enabled)
{
}

LEDs FakeInputDevice::leds() const
{
    return LEDs();
}

void FakeInputDevice::setLeds(LEDs leds)
{
}

bool FakeInputDevice::isKeyboard() const
{
    return true;
}

bool FakeInputDevice::isAlphaNumericKeyboard() const
{
    return true;
}

bool FakeInputDevice::isPointer() const
{
    return true;
}

bool FakeInputDevice::isTouchpad() const
{
    return false;
}

bool FakeInputDevice::isTouch() const
{
    return true;
}

bool FakeInputDevice::isTabletTool() const
{
    return false;
}

bool FakeInputDevice::isTabletPad() const
{
    return false;
}

bool FakeInputDevice::isTabletModeSwitch() const
{
    return false;
}

bool FakeInputDevice::isLidSwitch() const
{
    return false;
}

} // namespace KWin

#include "moc_fakeinputdevice.cpp"
