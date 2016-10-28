/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "device.h"

#include <QDBusConnection>

#include <linux/input.h>

#include <config-kwin.h>

namespace KWin
{
namespace LibInput
{

static bool checkAlphaNumericKeyboard(libinput_device *device)
{
    for (uint i = KEY_1; i <= KEY_0; i++) {
        if (libinput_device_keyboard_has_key(device, i) == 0) {
            return false;
        }
    }
    for (uint i = KEY_Q; i <= KEY_P; i++) {
        if (libinput_device_keyboard_has_key(device, i) == 0) {
            return false;
        }
    }
    for (uint i = KEY_A; i <= KEY_L; i++) {
        if (libinput_device_keyboard_has_key(device, i) == 0) {
            return false;
        }
    }
    for (uint i = KEY_Z; i <= KEY_M; i++) {
        if (libinput_device_keyboard_has_key(device, i) == 0) {
            return false;
        }
    }
    return true;
}

QVector<Device*> Device::s_devices;

Device *Device::getDevice(libinput_device *native)
{
    auto it = std::find_if(s_devices.constBegin(), s_devices.constEnd(),
        [native] (const Device *d) {
            return d->device() == native;
        }
    );
    if (it != s_devices.constEnd()) {
        return *it;
    }
    return nullptr;
}

Device::Device(libinput_device *device, QObject *parent)
    : QObject(parent)
    , m_device(device)
    , m_keyboard(libinput_device_has_capability(m_device, LIBINPUT_DEVICE_CAP_KEYBOARD))
    , m_pointer(libinput_device_has_capability(m_device, LIBINPUT_DEVICE_CAP_POINTER))
    , m_touch(libinput_device_has_capability(m_device, LIBINPUT_DEVICE_CAP_TOUCH))
    , m_tabletTool(libinput_device_has_capability(m_device, LIBINPUT_DEVICE_CAP_TABLET_TOOL))
#if 0
    // next libinput version
    , m_tabletPad(libinput_device_has_capability(m_device, LIBINPUT_DEVICE_CAP_TABLET_PAD))
#else
    , m_tabletPad(false)
#endif
    , m_supportsGesture(libinput_device_has_capability(m_device, LIBINPUT_DEVICE_CAP_GESTURE))
    , m_name(QString::fromLocal8Bit(libinput_device_get_name(m_device)))
    , m_sysName(QString::fromLocal8Bit(libinput_device_get_sysname(m_device)))
    , m_outputName(QString::fromLocal8Bit(libinput_device_get_output_name(m_device)))
    , m_product(libinput_device_get_id_product(m_device))
    , m_vendor(libinput_device_get_id_vendor(m_device))
    , m_tapFingerCount(libinput_device_config_tap_get_finger_count(m_device))
    , m_tapToClickEnabledByDefault(libinput_device_config_tap_get_default_enabled(m_device) == LIBINPUT_CONFIG_TAP_ENABLED)
    , m_tapToClick(libinput_device_config_tap_get_enabled(m_device))
    , m_tapAndDragEnabledByDefault(libinput_device_config_tap_get_default_drag_enabled(m_device))
    , m_tapAndDrag(libinput_device_config_tap_get_drag_enabled(m_device))
    , m_tapDragLockEnabledByDefault(libinput_device_config_tap_get_default_drag_lock_enabled(m_device))
    , m_tapDragLock(libinput_device_config_tap_get_drag_lock_enabled(m_device))
    , m_supportsDisableWhileTyping(libinput_device_config_dwt_is_available(m_device))
    , m_supportsPointerAcceleration(libinput_device_config_accel_is_available(m_device))
    , m_supportsLeftHanded(libinput_device_config_left_handed_is_available(m_device))
    , m_supportsCalibrationMatrix(libinput_device_config_calibration_has_matrix(m_device))
    , m_supportsDisableEvents(libinput_device_config_send_events_get_modes(m_device) & LIBINPUT_CONFIG_SEND_EVENTS_DISABLED)
    , m_supportsDisableEventsOnExternalMouse(libinput_device_config_send_events_get_modes(m_device) & LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE)
    , m_supportsMiddleEmulation(libinput_device_config_middle_emulation_is_available(m_device))
    , m_supportsNaturalScroll(libinput_device_config_scroll_has_natural_scroll(m_device))
    , m_supportedScrollMethods(libinput_device_config_scroll_get_methods(m_device))
    , m_middleEmulationEnabledByDefault(libinput_device_config_middle_emulation_get_default_enabled(m_device) == LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED)
    , m_naturalScrollEnabledByDefault(libinput_device_config_scroll_get_default_natural_scroll_enabled(m_device))
    , m_defaultScrollMethod(libinput_device_config_scroll_get_default_method(m_device))
    , m_defaultScrollButton(libinput_device_config_scroll_get_default_button(m_device))
    , m_middleEmulation(libinput_device_config_middle_emulation_get_enabled(m_device) == LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED)
    , m_leftHanded(m_supportsLeftHanded ? libinput_device_config_left_handed_get(m_device) : false)
    , m_naturalScroll(m_supportsNaturalScroll ? libinput_device_config_scroll_get_natural_scroll_enabled(m_device) : false)
    , m_scrollMethod(libinput_device_config_scroll_get_method(m_device))
    , m_scrollButton(libinput_device_config_scroll_get_button(m_device))
    , m_pointerAcceleration(libinput_device_config_accel_get_speed(m_device))
    , m_enabled(m_supportsDisableEvents ? libinput_device_config_send_events_get_mode(m_device) == LIBINPUT_CONFIG_SEND_EVENTS_ENABLED : true)
{
    libinput_device_ref(m_device);

    qreal width = 0;
    qreal height = 0;
    if (libinput_device_get_size(m_device, &width, &height) == 0) {
        m_size = QSizeF(width, height);
    }
    if (m_pointer) {
        if (libinput_device_pointer_has_button(m_device, BTN_LEFT) == 1) {
            m_supportedButtons |= Qt::LeftButton;
        }
        if (libinput_device_pointer_has_button(m_device, BTN_MIDDLE) == 1) {
            m_supportedButtons |= Qt::MiddleButton;
        }
        if (libinput_device_pointer_has_button(m_device, BTN_RIGHT) == 1) {
            m_supportedButtons |= Qt::RightButton;
        }
        if (libinput_device_pointer_has_button(m_device, BTN_SIDE) == 1) {
            m_supportedButtons |= Qt::ExtraButton1;
        }
        if (libinput_device_pointer_has_button(m_device, BTN_EXTRA) == 1) {
            m_supportedButtons |= Qt::ExtraButton2;
        }
        if (libinput_device_pointer_has_button(m_device, BTN_BACK) == 1) {
            m_supportedButtons |= Qt::BackButton;
        }
        if (libinput_device_pointer_has_button(m_device, BTN_FORWARD) == 1) {
            m_supportedButtons |= Qt::ForwardButton;
        }
        if (libinput_device_pointer_has_button(m_device, BTN_TASK) == 1) {
            m_supportedButtons |= Qt::TaskButton;
        }
    }
    if (m_keyboard) {
        m_alphaNumericKeyboard = checkAlphaNumericKeyboard(m_device);
    }

    s_devices << this;
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kde/KWin/InputDevice/") + m_sysName,
                                                 QStringLiteral("org.kde.KWin.InputDevice"),
                                                 this,
                                                 QDBusConnection::ExportAllProperties
    );
}

Device::~Device()
{
    s_devices.removeOne(this);
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/org/kde/KWin/InputDevice/") + m_sysName);
    libinput_device_unref(m_device);
}

void Device::setLeftHanded(bool set)
{
    if (!m_supportsLeftHanded) {
        return;
    }
    if (libinput_device_config_left_handed_set(m_device, set) == LIBINPUT_CONFIG_STATUS_SUCCESS) {
        if (m_leftHanded != set) {
            m_leftHanded = set;
            emit leftHandedChanged();
        }
    }
}

void Device::setPointerAcceleration(qreal acceleration)
{
    if (!m_supportsPointerAcceleration) {
        return;
    }
    acceleration = qBound(-1.0, acceleration, 1.0);
    if (libinput_device_config_accel_set_speed(m_device, acceleration) == LIBINPUT_CONFIG_STATUS_SUCCESS) {
        if (m_pointerAcceleration != acceleration) {
            m_pointerAcceleration = acceleration;
            emit pointerAccelerationChanged();
        }
    }
}

void Device::setNaturalScroll(bool set)
{
    if (!m_supportsNaturalScroll) {
        return;
    }
    if (libinput_device_config_scroll_set_natural_scroll_enabled(m_device, set) == LIBINPUT_CONFIG_STATUS_SUCCESS) {
        if (m_naturalScroll != set) {
            m_naturalScroll = set;
            emit naturalScrollChanged();
        }
    }
}

void Device::setScrollMethod(bool set, enum libinput_config_scroll_method method)
{
    if (!(m_supportedScrollMethods & method)) {
        return;
    }
    if (set) {
        if (m_scrollMethod == method) {
            return;
        }
    } else {
        if (m_scrollMethod != method) {
            return;
        }
        method = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    }

    if (libinput_device_config_scroll_set_method(m_device, method) == LIBINPUT_CONFIG_STATUS_SUCCESS) {
        m_scrollMethod = method;
        emit scrollMethodChanged();
    }
}

void Device::setScrollTwoFinger(bool set) {
    setScrollMethod(set, LIBINPUT_CONFIG_SCROLL_2FG);
}

void Device::setScrollEdge(bool set) {
    setScrollMethod(set, LIBINPUT_CONFIG_SCROLL_EDGE);
}

void Device::setScrollOnButtonDown(bool set) {
    setScrollMethod(set, LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN);
}

void Device::setScrollButton(quint32 button)
{
    if (!(m_supportedScrollMethods & LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN)) {
        return;
    }
    if (libinput_device_config_scroll_set_button(m_device, button) == LIBINPUT_CONFIG_STATUS_SUCCESS) {
        if (m_scrollButton != button) {
            m_scrollButton = button;
            emit scrollButtonChanged();
        }
    }
}

#define CONFIG(method, condition, function, enum, variable) \
void Device::method(bool set) \
{ \
    if (condition) { \
        return; \
    } \
    if (libinput_device_config_##function(m_device, set ? LIBINPUT_CONFIG_##enum##_ENABLED : LIBINPUT_CONFIG_##enum##_DISABLED) == LIBINPUT_CONFIG_STATUS_SUCCESS) { \
        if (m_##variable != set) { \
            m_##variable = set; \
            emit variable##Changed(); \
        }\
    } \
}

CONFIG(setEnabled, !m_supportsDisableEvents, send_events_set_mode, SEND_EVENTS, enabled)
CONFIG(setTapToClick, m_tapFingerCount == 0, tap_set_enabled, TAP, tapToClick)
CONFIG(setTapAndDrag, false, tap_set_drag_enabled, DRAG, tapAndDrag)
CONFIG(setTapDragLock, false, tap_set_drag_lock_enabled, DRAG_LOCK, tapDragLock)
CONFIG(setMiddleEmulation, m_supportsMiddleEmulation == false, middle_emulation_set_enabled, MIDDLE_EMULATION, middleEmulation)

#undef CONFIG

}
}
