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
#include <libinput.h>
#include "mock_libinput.h"

#include <linux/input.h>

int libinput_device_keyboard_has_key(struct libinput_device *device, uint32_t code)
{
    return device->keys.contains(code);
}

int libinput_device_has_capability(struct libinput_device *device, enum libinput_device_capability capability)
{
    switch (capability) {
    case LIBINPUT_DEVICE_CAP_KEYBOARD:
        return device->keyboard;
    case LIBINPUT_DEVICE_CAP_POINTER:
        return device->pointer;
    case LIBINPUT_DEVICE_CAP_TOUCH:
        return device->touch;
    case LIBINPUT_DEVICE_CAP_GESTURE:
        return device->gestureSupported;
    default:
        return 0;
    }
}

const char *libinput_device_get_name(struct libinput_device *device)
{
    return device->name.constData();
}

const char *libinput_device_get_sysname(struct libinput_device *device)
{
    return device->sysName.constData();
}

const char *libinput_device_get_output_name(struct libinput_device *device)
{
    return device->outputName.constData();
}

unsigned int libinput_device_get_id_product(struct libinput_device *device)
{
    return device->product;
}

unsigned int libinput_device_get_id_vendor(struct libinput_device *device)
{
    return device->vendor;
}

int libinput_device_config_tap_get_finger_count(struct libinput_device *device)
{
    return device->tapFingerCount;
}

enum libinput_config_tap_state libinput_device_config_tap_get_enabled(struct libinput_device *device)
{
    return LIBINPUT_CONFIG_TAP_DISABLED;
}

enum libinput_config_tap_state libinput_device_config_tap_get_default_enabled(struct libinput_device *device)
{
    if (device->tapEnabledByDefault) {
        return LIBINPUT_CONFIG_TAP_ENABLED;
    } else {
        return LIBINPUT_CONFIG_TAP_DISABLED;
    }
}

int libinput_device_config_dwt_is_available(struct libinput_device *device)
{
    return device->supportsDisableWhileTyping;
}

int libinput_device_config_accel_is_available(struct libinput_device *device)
{
    return device->supportsPointerAcceleration;
}

int libinput_device_config_calibration_has_matrix(struct libinput_device *device)
{
    return device->supportsCalibrationMatrix;
}

int libinput_device_config_left_handed_is_available(struct libinput_device *device)
{
    return device->supportsLeftHanded;
}

uint32_t libinput_device_config_send_events_get_modes(struct libinput_device *device)
{
    uint32_t modes = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
    if (device->supportsDisableEvents) {
        modes |= LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
    }
    if (device->supportsDisableEventsOnExternalMouse) {
        modes |= LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
    }
    return modes;
}

int libinput_device_config_left_handed_get(struct libinput_device *device)
{
    return device->leftHanded;
}

double libinput_device_config_accel_get_speed(struct libinput_device *device)
{
    return device->pointerAcceleration;
}

uint32_t libinput_device_config_send_events_get_mode(struct libinput_device *device)
{
    if (device->enabled) {
        return LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
    } else {
        // TODO: disabled on eternal mouse
        return LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
    }
}

struct libinput_device *libinput_device_ref(struct libinput_device *device)
{
    return device;
}

struct libinput_device *libinput_device_unref(struct libinput_device *device)
{
    return device;
}

int libinput_device_get_size(struct libinput_device *device, double *width, double *height)
{
    if (device->deviceSizeReturnValue) {
        return device->deviceSizeReturnValue;
    }
    if (width) {
        *width = device->deviceSize.width();
    }
    if (height) {
        *height = device->deviceSize.height();
    }
    return device->deviceSizeReturnValue;
}

int libinput_device_pointer_has_button(struct libinput_device *device, uint32_t code)
{
    switch (code) {
    case BTN_LEFT:
        return device->supportedButtons.testFlag(Qt::LeftButton);
    case BTN_MIDDLE:
        return device->supportedButtons.testFlag(Qt::MiddleButton);
    case BTN_RIGHT:
        return device->supportedButtons.testFlag(Qt::RightButton);
    case BTN_SIDE:
        return device->supportedButtons.testFlag(Qt::ExtraButton1);
    case BTN_EXTRA:
        return device->supportedButtons.testFlag(Qt::ExtraButton2);
    case BTN_BACK:
        return device->supportedButtons.testFlag(Qt::BackButton);
    case BTN_FORWARD:
        return device->supportedButtons.testFlag(Qt::ForwardButton);
    case BTN_TASK:
        return device->supportedButtons.testFlag(Qt::TaskButton);
    default:
        return 0;
    }
}

enum libinput_config_status libinput_device_config_left_handed_set(struct libinput_device *device, int left_handed)
{
    if (device->setLeftHandedReturnValue == 0) {
        device->leftHanded = left_handed;
        return LIBINPUT_CONFIG_STATUS_SUCCESS;
    }
    return LIBINPUT_CONFIG_STATUS_INVALID;
}

enum libinput_config_status libinput_device_config_accel_set_speed(struct libinput_device *device, double speed)
{
    if (device->setPointerAccelerationReturnValue == 0) {
        device->pointerAcceleration = speed;
        return LIBINPUT_CONFIG_STATUS_SUCCESS;
    }
    return LIBINPUT_CONFIG_STATUS_INVALID;
}

enum libinput_config_status libinput_device_config_send_events_set_mode(struct libinput_device *device, uint32_t mode)
{
    if (device->setEnableModeReturnValue == 0) {
        device->enabled = (mode == LIBINPUT_CONFIG_SEND_EVENTS_ENABLED);
        return LIBINPUT_CONFIG_STATUS_SUCCESS;
    }
    return LIBINPUT_CONFIG_STATUS_INVALID;
}
