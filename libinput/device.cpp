/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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

enum class ConfigKey {
    Enabled,
    LeftHanded,
    DisableWhileTyping,
    PointerAcceleration,
    PointerAccelerationProfile,
    TapToClick,
    LmrTapButtonMap,
    TapAndDrag,
    TapDragLock,
    MiddleButtonEmulation,
    NaturalScroll,
    ScrollMethod,
    ScrollButton,
    ClickMethod,
    ScrollFactor
};

struct ConfigData {
    explicit ConfigData(QByteArray _key, void (Device::*_setter)(bool), bool (Device::*_defaultValue)() const = nullptr)
        : key(_key)
    { booleanSetter.setter = _setter; booleanSetter.defaultValue = _defaultValue; }

    explicit ConfigData(QByteArray _key, void (Device::*_setter)(quint32), quint32 (Device::*_defaultValue)() const = nullptr)
        : key(_key)
    { quint32Setter.setter = _setter; quint32Setter.defaultValue = _defaultValue; }

    explicit ConfigData(QByteArray _key, void (Device::*_setter)(QString), QString (Device::*_defaultValue)() const = nullptr)
        : key(_key)
    { stringSetter.setter = _setter; stringSetter.defaultValue = _defaultValue; }

    explicit ConfigData(QByteArray _key, void (Device::*_setter)(qreal), qreal (Device::*_defaultValue)() const = nullptr)
        : key(_key)
    { qrealSetter.setter = _setter; qrealSetter.defaultValue = _defaultValue; }

    QByteArray key;

    struct {
        void (Device::*setter)(bool) = nullptr;
        bool (Device::*defaultValue)() const;
    } booleanSetter;

    struct {
        void (Device::*setter)(quint32) = nullptr;
        quint32 (Device::*defaultValue)() const;
    } quint32Setter;
    struct {
        void (Device::*setter)(QString) = nullptr;
        QString (Device::*defaultValue)() const;
    } stringSetter;
    struct {
        void (Device::*setter)(qreal) = nullptr;
        qreal (Device::*defaultValue)() const;
    } qrealSetter;
};

static const QMap<ConfigKey, ConfigData> s_configData {
    {ConfigKey::Enabled, ConfigData(QByteArrayLiteral("Enabled"), &Device::setEnabled)},
    {ConfigKey::LeftHanded, ConfigData(QByteArrayLiteral("LeftHanded"), &Device::setLeftHanded, &Device::leftHandedEnabledByDefault)},
    {ConfigKey::DisableWhileTyping, ConfigData(QByteArrayLiteral("DisableWhileTyping"), &Device::setDisableWhileTyping, &Device::disableWhileTypingEnabledByDefault)},
    {ConfigKey::PointerAcceleration, ConfigData(QByteArrayLiteral("PointerAcceleration"), &Device::setPointerAccelerationFromString, &Device::defaultPointerAccelerationToString)},
    {ConfigKey::PointerAccelerationProfile, ConfigData(QByteArrayLiteral("PointerAccelerationProfile"), &Device::setPointerAccelerationProfileFromInt, &Device::defaultPointerAccelerationProfileToInt)},
    {ConfigKey::TapToClick, ConfigData(QByteArrayLiteral("TapToClick"), &Device::setTapToClick, &Device::tapToClickEnabledByDefault)},
    {ConfigKey::TapAndDrag, ConfigData(QByteArrayLiteral("TapAndDrag"), &Device::setTapAndDrag, &Device::tapAndDragEnabledByDefault)},
    {ConfigKey::TapDragLock, ConfigData(QByteArrayLiteral("TapDragLock"), &Device::setTapDragLock, &Device::tapDragLockEnabledByDefault)},
    {ConfigKey::MiddleButtonEmulation, ConfigData(QByteArrayLiteral("MiddleButtonEmulation"), &Device::setMiddleEmulation, &Device::middleEmulationEnabledByDefault)},
    {ConfigKey::LmrTapButtonMap, ConfigData(QByteArrayLiteral("LmrTapButtonMap"), &Device::setLmrTapButtonMap, &Device::lmrTapButtonMapEnabledByDefault)},
    {ConfigKey::NaturalScroll, ConfigData(QByteArrayLiteral("NaturalScroll"), &Device::setNaturalScroll, &Device::naturalScrollEnabledByDefault)},
    {ConfigKey::ScrollMethod, ConfigData(QByteArrayLiteral("ScrollMethod"), &Device::activateScrollMethodFromInt, &Device::defaultScrollMethodToInt)},
    {ConfigKey::ScrollButton, ConfigData(QByteArrayLiteral("ScrollButton"), &Device::setScrollButton, &Device::defaultScrollButton)},
    {ConfigKey::ClickMethod, ConfigData(QByteArrayLiteral("ClickMethod"), &Device::setClickMethodFromInt, &Device::defaultClickMethodToInt)},
    {ConfigKey::ScrollFactor, ConfigData(QByteArrayLiteral("ScrollFactor"), &Device::setScrollFactor, &Device::scrollFactorDefault)}
};

namespace {
QMatrix4x4 defaultCalibrationMatrix(libinput_device *device)
{
    float matrix[6];
    const int ret = libinput_device_config_calibration_get_default_matrix(device, matrix);
    if (ret == 0) {
        return QMatrix4x4();
    }
    return QMatrix4x4{
        matrix[0], matrix[1], matrix[2], 0.0f,
        matrix[3], matrix[4], matrix[5], 0.0f,
        0.0f,  0.0f, 1.0f, 0.0f,
        0.0f,  0.0f, 0.0f, 1.0f
    };
}
}

Device::Device(libinput_device *device, QObject *parent)
    : QObject(parent)
    , m_device(device)
    , m_keyboard(libinput_device_has_capability(m_device, LIBINPUT_DEVICE_CAP_KEYBOARD))
    , m_pointer(libinput_device_has_capability(m_device, LIBINPUT_DEVICE_CAP_POINTER))
    , m_touch(libinput_device_has_capability(m_device, LIBINPUT_DEVICE_CAP_TOUCH))
    , m_tabletTool(libinput_device_has_capability(m_device, LIBINPUT_DEVICE_CAP_TABLET_TOOL))
    , m_tabletPad(libinput_device_has_capability(m_device, LIBINPUT_DEVICE_CAP_TABLET_PAD))
    , m_supportsGesture(libinput_device_has_capability(m_device, LIBINPUT_DEVICE_CAP_GESTURE))
    , m_switch(libinput_device_has_capability(m_device, LIBINPUT_DEVICE_CAP_SWITCH))
    , m_lidSwitch(m_switch ? libinput_device_switch_has_switch(m_device, LIBINPUT_SWITCH_LID) : false)
    , m_tabletSwitch(m_switch ? libinput_device_switch_has_switch(m_device, LIBINPUT_SWITCH_TABLET_MODE) : false)
    , m_name(QString::fromLocal8Bit(libinput_device_get_name(m_device)))
    , m_sysName(QString::fromLocal8Bit(libinput_device_get_sysname(m_device)))
    , m_outputName(QString::fromLocal8Bit(libinput_device_get_output_name(m_device)))
    , m_product(libinput_device_get_id_product(m_device))
    , m_vendor(libinput_device_get_id_vendor(m_device))
    , m_tapFingerCount(libinput_device_config_tap_get_finger_count(m_device))
    , m_defaultTapButtonMap(libinput_device_config_tap_get_default_button_map(m_device))
    , m_tapButtonMap(libinput_device_config_tap_get_button_map(m_device))
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
    , m_leftHandedEnabledByDefault(libinput_device_config_left_handed_get_default(m_device))
    , m_middleEmulationEnabledByDefault(libinput_device_config_middle_emulation_get_default_enabled(m_device) == LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED)
    , m_naturalScrollEnabledByDefault(libinput_device_config_scroll_get_default_natural_scroll_enabled(m_device))
    , m_defaultScrollMethod(libinput_device_config_scroll_get_default_method(m_device))
    , m_defaultScrollButton(libinput_device_config_scroll_get_default_button(m_device))
    , m_disableWhileTypingEnabledByDefault(libinput_device_config_dwt_get_default_enabled(m_device))
    , m_disableWhileTyping(m_supportsDisableWhileTyping ? libinput_device_config_dwt_get_enabled(m_device) == LIBINPUT_CONFIG_DWT_ENABLED : false)
    , m_middleEmulation(libinput_device_config_middle_emulation_get_enabled(m_device) == LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED)
    , m_leftHanded(m_supportsLeftHanded ? libinput_device_config_left_handed_get(m_device) : false)
    , m_naturalScroll(m_supportsNaturalScroll ? libinput_device_config_scroll_get_natural_scroll_enabled(m_device) : false)
    , m_scrollMethod(libinput_device_config_scroll_get_method(m_device))
    , m_scrollButton(libinput_device_config_scroll_get_button(m_device))
    , m_defaultPointerAcceleration(libinput_device_config_accel_get_default_speed(m_device))
    , m_pointerAcceleration(libinput_device_config_accel_get_speed(m_device))
    , m_scrollFactor(scrollFactorDefault())
    , m_supportedPointerAccelerationProfiles(libinput_device_config_accel_get_profiles(m_device))
    , m_defaultPointerAccelerationProfile(libinput_device_config_accel_get_default_profile(m_device))
    , m_pointerAccelerationProfile(libinput_device_config_accel_get_profile(m_device))
    , m_enabled(m_supportsDisableEvents ? libinput_device_config_send_events_get_mode(m_device) == LIBINPUT_CONFIG_SEND_EVENTS_ENABLED : true)
    , m_config()
    , m_defaultCalibrationMatrix(m_supportsCalibrationMatrix ? defaultCalibrationMatrix(m_device) : QMatrix4x4{})
    , m_supportedClickMethods(libinput_device_config_click_get_methods(m_device))
    , m_defaultClickMethod(libinput_device_config_click_get_default_method(m_device))
    , m_clickMethod(libinput_device_config_click_get_method(m_device))
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

template <typename T>
void Device::writeEntry(const ConfigKey &key, const T &value)
{
    if (!m_config.isValid()) {
        return;
    }
    if (m_loading) {
        return;
    }
    auto it = s_configData.find(key);
    Q_ASSERT(it != s_configData.end());
    m_config.writeEntry(it.value().key.constData(), value);
    m_config.sync();
}

template <typename T, typename Setter>
void Device::readEntry(const QByteArray &key, const Setter &s, const T &defaultValue)
{
    if (!s.setter) {
        return;
    }

    (this->*(s.setter))(m_config.readEntry(key.constData(), s.defaultValue ? (this->*(s.defaultValue))() : defaultValue));
}

void Device::loadConfiguration()
{
    if (!m_config.isValid()) {
        return;
    }
    m_loading = true;
    for (auto it = s_configData.begin(), end = s_configData.end(); it != end; ++it) {
        const auto key = it.value().key;
        if (!m_config.hasKey(key.constData())) {
            continue;
        }
        readEntry(key, it.value().booleanSetter, true);
        readEntry(key, it.value().quint32Setter, 0);
        readEntry(key, it.value().stringSetter, "");
        readEntry(key, it.value().qrealSetter, 1.0);
    };

    m_loading = false;
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
            writeEntry(ConfigKey::PointerAcceleration, QString::number(acceleration, 'f', 3));
        }
    }
}

void Device::setScrollButton(quint32 button)
{
    if (!(m_supportedScrollMethods & LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN)) {
        return;
    }
    if (libinput_device_config_scroll_set_button(m_device, button) == LIBINPUT_CONFIG_STATUS_SUCCESS) {
        if (m_scrollButton != button) {
            m_scrollButton = button;
            writeEntry(ConfigKey::ScrollButton, m_scrollButton);
            emit scrollButtonChanged();
        }
    }
}

void Device::setPointerAccelerationProfile(bool set, enum  libinput_config_accel_profile profile)
{
    if (!(m_supportedPointerAccelerationProfiles & profile)) {
        return;
    }
    if (!set) {
        profile = (profile == LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT) ? LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE : LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
        if (!(m_supportedPointerAccelerationProfiles & profile)) {
            return;
        }
    }

    if (libinput_device_config_accel_set_profile(m_device, profile) == LIBINPUT_CONFIG_STATUS_SUCCESS) {
        if (m_pointerAccelerationProfile != profile) {
            m_pointerAccelerationProfile = profile;
            emit pointerAccelerationProfileChanged();
            writeEntry(ConfigKey::PointerAccelerationProfile, (quint32) profile);
        }
    }
}

void Device::setClickMethod(bool set, enum libinput_config_click_method method)
{
    if (!(m_supportedClickMethods & method)) {
        return;
    }
    if (!set) {
        method = (method == LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS) ? LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER : LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
        if (!(m_supportedClickMethods & method)) {
            return;
        }
    }

    if (libinput_device_config_click_set_method(m_device, method) == LIBINPUT_CONFIG_STATUS_SUCCESS) {
        if (m_clickMethod != method) {
            m_clickMethod = method;
            emit clickMethodChanged();
            writeEntry(ConfigKey::ClickMethod, (quint32) method);
        }
    }
}

void Device::setScrollMethod(bool set, enum libinput_config_scroll_method method)
{
    if (!(m_supportedScrollMethods & method)) {
        return;
    }

    bool isCurrent = m_scrollMethod == method;
    if (!set) {
        if (isCurrent) {
            method = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
            isCurrent = false;
        } else {
            return;
        }
    }

    if (libinput_device_config_scroll_set_method(m_device, method) == LIBINPUT_CONFIG_STATUS_SUCCESS) {
        if (!isCurrent) {
            m_scrollMethod = method;
            emit scrollMethodChanged();
            writeEntry(ConfigKey::ScrollMethod, (quint32) method);
        }
    }
}

void Device::setLmrTapButtonMap(bool set)
{
    enum libinput_config_tap_button_map map = set ? LIBINPUT_CONFIG_TAP_MAP_LMR : LIBINPUT_CONFIG_TAP_MAP_LRM;

    if (m_tapFingerCount < 2) {
        return;
    }
    if (!set) {
        map = LIBINPUT_CONFIG_TAP_MAP_LRM;
    }

    if (libinput_device_config_tap_set_button_map(m_device, map) == LIBINPUT_CONFIG_STATUS_SUCCESS) {
        if (m_tapButtonMap != map) {
            m_tapButtonMap = map;
            writeEntry(ConfigKey::LmrTapButtonMap, set);
            emit tapButtonMapChanged();
        }
    }
}

int Device::stripsCount() const
{
    return libinput_device_tablet_pad_get_num_strips(m_device);
}

int Device::ringsCount() const
{
    return libinput_device_tablet_pad_get_num_rings(m_device);
}

#define CONFIG(method, condition, function, variable, key) \
void Device::method(bool set) \
{ \
    if (condition) { \
        return; \
    } \
    if (libinput_device_config_##function(m_device, set) == LIBINPUT_CONFIG_STATUS_SUCCESS) { \
        if (m_##variable != set) { \
            m_##variable = set; \
            writeEntry(ConfigKey::key, m_##variable); \
            emit variable##Changed(); \
        }\
    } \
}

CONFIG(setLeftHanded, !m_supportsLeftHanded, left_handed_set, leftHanded, LeftHanded)
CONFIG(setNaturalScroll, !m_supportsNaturalScroll, scroll_set_natural_scroll_enabled, naturalScroll, NaturalScroll)

#undef CONFIG

#define CONFIG(method, condition, function, enum, variable, key) \
void Device::method(bool set) \
{ \
    if (condition) { \
        return; \
    } \
    if (libinput_device_config_##function(m_device, set ? LIBINPUT_CONFIG_##enum##_ENABLED : LIBINPUT_CONFIG_##enum##_DISABLED) == LIBINPUT_CONFIG_STATUS_SUCCESS) { \
        if (m_##variable != set) { \
            m_##variable = set; \
            writeEntry(ConfigKey::key, m_##variable); \
            emit variable##Changed(); \
        }\
    } \
}

CONFIG(setEnabled, !m_supportsDisableEvents, send_events_set_mode, SEND_EVENTS, enabled, Enabled)
CONFIG(setDisableWhileTyping, !m_supportsDisableWhileTyping, dwt_set_enabled, DWT, disableWhileTyping, DisableWhileTyping)
CONFIG(setTapToClick, m_tapFingerCount == 0, tap_set_enabled, TAP, tapToClick, TapToClick)
CONFIG(setTapAndDrag, false, tap_set_drag_enabled, DRAG, tapAndDrag, TapAndDrag)
CONFIG(setTapDragLock, false, tap_set_drag_lock_enabled, DRAG_LOCK, tapDragLock, TapDragLock)
CONFIG(setMiddleEmulation, m_supportsMiddleEmulation == false, middle_emulation_set_enabled, MIDDLE_EMULATION, middleEmulation, MiddleButtonEmulation)

#undef CONFIG

void Device::setScrollFactor(qreal factor)
{
    if (m_scrollFactor != factor) {
        m_scrollFactor = factor;
        writeEntry(ConfigKey::ScrollFactor, m_scrollFactor);
        emit scrollFactorChanged();
    }
}

void Device::setOrientation(Qt::ScreenOrientation orientation)
{
    if (!m_supportsCalibrationMatrix) {
        return;
    }
    // 90 deg cw:
    static const QMatrix4x4 portraitMatrix{
        0.0f, -1.0f, 1.0f, 0.0f,
        1.0f,  0.0f, 0.0f, 0.0f,
        0.0f,  0.0f, 1.0f, 0.0f,
        0.0f,  0.0f, 0.0f, 1.0f
    };
    // 180 deg cw:
    static const QMatrix4x4 invertedLandscapeMatrix{
        -1.0f,  0.0f, 1.0f, 0.0f,
         0.0f, -1.0f, 1.0f, 0.0f,
         0.0f,  0.0f, 1.0f, 0.0f,
         0.0f,  0.0f, 0.0f, 1.0f
    };
    // 270 deg cw
    static const QMatrix4x4 invertedPortraitMatrix{
         0.0f, 1.0f, 0.0f, 0.0f,
        -1.0f, 0.0f, 1.0f, 0.0f,
         0.0f,  0.0f, 1.0f, 0.0f,
         0.0f,  0.0f, 0.0f, 1.0f
    };
    QMatrix4x4 matrix;
    switch (orientation) {
    case Qt::PortraitOrientation:
        matrix = portraitMatrix;
        break;
    case Qt::InvertedLandscapeOrientation:
        matrix = invertedLandscapeMatrix;
        break;
    case Qt::InvertedPortraitOrientation:
        matrix = invertedPortraitMatrix;
        break;
    case Qt::PrimaryOrientation:
    case Qt::LandscapeOrientation:
    default:
        break;
    }
    const auto combined = m_defaultCalibrationMatrix * matrix;
    const auto columnOrder = combined.constData();
    float m[6] = {
        columnOrder[0], columnOrder[4], columnOrder[8],
        columnOrder[1], columnOrder[5], columnOrder[9]
    };
    libinput_device_config_calibration_set_matrix(m_device, m);
}

}
}
