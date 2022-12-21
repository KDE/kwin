/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef MOCK_LIBINPUT_H
#define MOCK_LIBINPUT_H
#include <libinput.h>

#include <QByteArray>
#include <QPointF>
#include <QSizeF>
#include <QVector>

#include <array>
#include <chrono>

struct libinput_device
{
    void *userData = nullptr;
    bool keyboard = false;
    bool pointer = false;
    bool touch = false;
    bool tabletTool = false;
    bool gestureSupported = false;
    bool switchDevice = false;
    QByteArray name;
    QByteArray sysName = QByteArrayLiteral("event0");
    QByteArray outputName;
    quint32 product = 0;
    quint32 vendor = 0;
    int tapFingerCount = 0;
    QSizeF deviceSize;
    int deviceSizeReturnValue = 0;
    bool tapEnabledByDefault = false;
    bool tapToClick = false;
    bool tapAndDragEnabledByDefault = false;
    bool tapAndDrag = false;
    bool tapDragLockEnabledByDefault = false;
    bool tapDragLock = false;
    bool supportsDisableWhileTyping = false;
    bool supportsPointerAcceleration = false;
    bool supportsLeftHanded = false;
    bool supportsCalibrationMatrix = false;
    bool supportsDisableEvents = false;
    bool supportsDisableEventsOnExternalMouse = false;
    bool supportsMiddleEmulation = false;
    bool supportsNaturalScroll = false;
    quint32 supportedScrollMethods = 0;
    bool middleEmulationEnabledByDefault = false;
    bool middleEmulation = false;
    enum libinput_config_tap_button_map defaultTapButtonMap = LIBINPUT_CONFIG_TAP_MAP_LRM;
    enum libinput_config_tap_button_map tapButtonMap = LIBINPUT_CONFIG_TAP_MAP_LRM;
    int setTapButtonMapReturnValue = 0;
    enum libinput_config_dwt_state disableWhileTypingEnabledByDefault = LIBINPUT_CONFIG_DWT_DISABLED;
    enum libinput_config_dwt_state disableWhileTyping = LIBINPUT_CONFIG_DWT_DISABLED;
    int setDisableWhileTypingReturnValue = 0;
    qreal defaultPointerAcceleration = 0.0;
    qreal pointerAcceleration = 0.0;
    int setPointerAccelerationReturnValue = 0;
    bool leftHandedEnabledByDefault = false;
    bool leftHanded = false;
    int setLeftHandedReturnValue = 0;
    bool naturalScrollEnabledByDefault = false;
    bool naturalScroll = false;
    int setNaturalScrollReturnValue = 0;
    enum libinput_config_scroll_method defaultScrollMethod = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    enum libinput_config_scroll_method scrollMethod = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    int setScrollMethodReturnValue = 0;
    quint32 defaultScrollButton = 0;
    quint32 scrollButton = 0;
    int setScrollButtonReturnValue = 0;
    Qt::MouseButtons supportedButtons;
    QVector<quint32> keys;
    bool enabled = true;
    int setEnableModeReturnValue = 0;
    int setTapToClickReturnValue = 0;
    int setTapAndDragReturnValue = 0;
    int setTapDragLockReturnValue = 0;
    int setMiddleEmulationReturnValue = 0;
    quint32 supportedPointerAccelerationProfiles = 0;
    enum libinput_config_accel_profile defaultPointerAccelerationProfile = LIBINPUT_CONFIG_ACCEL_PROFILE_NONE;
    enum libinput_config_accel_profile pointerAccelerationProfile = LIBINPUT_CONFIG_ACCEL_PROFILE_NONE;
    bool setPointerAccelerationProfileReturnValue = 0;
    std::array<float, 6> defaultCalibrationMatrix{{1.0f, 0.0f, 0.0f,
                                                   0.0f, 1.0f, 0.0f}};
    std::array<float, 6> calibrationMatrix{{1.0f, 0.0f, 0.0f,
                                            0.0f, 1.0f, 0.0f}};
    bool defaultCalibrationMatrixIsIdentity = true;
    bool calibrationMatrixIsIdentity = true;

    bool lidSwitch = false;
    bool tabletModeSwitch = false;
    quint32 supportedClickMethods = 0;
    enum libinput_config_click_method defaultClickMethod = LIBINPUT_CONFIG_CLICK_METHOD_NONE;
    enum libinput_config_click_method clickMethod = LIBINPUT_CONFIG_CLICK_METHOD_NONE;
    bool setClickMethodReturnValue = 0;
    uint32_t buttonCount = 0;
    uint32_t stripCount = 0;
    uint32_t ringCount = 0;
};

struct libinput_event
{
    virtual ~libinput_event()
    {
    }
    libinput_device *device = nullptr;
    libinput_event_type type = LIBINPUT_EVENT_NONE;
    std::chrono::microseconds time = std::chrono::microseconds::zero();
};

struct libinput_event_keyboard : libinput_event
{
    libinput_event_keyboard()
    {
        type = LIBINPUT_EVENT_KEYBOARD_KEY;
    }
    libinput_key_state state = LIBINPUT_KEY_STATE_RELEASED;
    quint32 key = 0;
};

struct libinput_event_pointer : libinput_event
{
    libinput_button_state buttonState = LIBINPUT_BUTTON_STATE_RELEASED;
    quint32 button = 0;
    bool verticalAxis = false;
    bool horizontalAxis = false;
    qreal horizontalScrollValue = 0.0;
    qreal verticalScrollValue = 0.0;
    qreal horizontalScrollValueV120 = 0.0;
    qreal verticalScrollValueV120 = 0.0;
    QPointF delta;
    QPointF absolutePos;
};

struct libinput_event_touch : libinput_event
{
    qint32 slot = -1;
    QPointF absolutePos;
};

struct libinput_event_gesture : libinput_event
{
    int fingerCount = 0;
    bool cancelled = false;
    QPointF delta = QPointF(0, 0);
    qreal scale = 0.0;
    qreal angleDelta = 0.0;
};

struct libinput_event_switch : libinput_event
{
    enum class State {
        Off,
        On
    };
    State state = State::Off;
};

struct libinput
{
    int refCount = 1;
    QByteArray seat;
    int assignSeatRetVal = 0;
};

#endif
