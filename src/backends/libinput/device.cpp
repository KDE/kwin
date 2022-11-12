/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "device.h"

#include <config-kwin.h>

#include "core/output.h"
#include "core/outputbackend.h"
#include "libinput_logging.h"
#include "main.h"
#include "mousebuttons.h"
#include "pointer_input.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>

#include <linux/input.h>

QDBusArgument &operator<<(QDBusArgument &argument, const QMatrix4x4 &matrix)
{
    argument.beginArray(qMetaTypeId<double>());
    for (quint8 row = 0; row < 4; ++row) {
        for (quint8 col = 0; col < 4; ++col) {
            argument << matrix(row, col);
        }
    }
    argument.endArray();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, QMatrix4x4 &matrix)
{
    argument.beginArray();
    for (quint8 row = 0; row < 4; ++row) {
        for (quint8 col = 0; col < 4; ++col) {
            double val;
            argument >> val;
            matrix(row, col) = val;
        }
    }
    argument.endArray();
    return argument;
}

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
    ScrollFactor,
    Orientation,
    Calibration,
    OutputName,
    OutputArea,
};

struct ConfigDataBase
{
    ConfigDataBase(const QByteArray &_key)
        : key(_key)
    {
    }
    virtual ~ConfigDataBase() = default;

    QByteArray key;
    virtual void read(Device * /*device*/, const KConfigGroup & /*values*/) const = 0;
};

template<typename T>
struct ConfigData : public ConfigDataBase
{
    using SetterFunction = std::function<void(Device *, const T &)>;
    using DefaultValueFunction = std::function<T(Device *)>;

    explicit ConfigData(const QByteArray &_key, const SetterFunction &_setter, const DefaultValueFunction &_defaultValue)
        : ConfigDataBase(_key)
        , setterFunction(_setter)
        , defaultValueFunction(_defaultValue)
    {
    }

    void read(Device *device, const KConfigGroup &values) const override
    {
        if (!setterFunction || !defaultValueFunction) {
            return;
        }

        setterFunction(device, values.readEntry(key.constData(), defaultValueFunction(device)));
    }

    SetterFunction setterFunction;
    DefaultValueFunction defaultValueFunction;
};

// Template specializations for some specific config types that can't be handled
// through plain readEntry.
//
// This uses tagged types to avoid specialising the general type since we
// directly call the getters/setters.

using DeviceOrientation = Qt::ScreenOrientation;

template<>
struct ConfigData<DeviceOrientation> : public ConfigDataBase
{
    explicit ConfigData()
        : ConfigDataBase(QByteArrayLiteral("Orientation"))
    {
    }

    void read(Device *device, const KConfigGroup &values) const override
    {
        int defaultValue = device->defaultOrientation();
        device->setOrientation(static_cast<Qt::ScreenOrientation>(values.readEntry(key.constData(), defaultValue)));
    }
};

using CalibrationMatrix = QMatrix4x4;

template<>
struct ConfigData<CalibrationMatrix> : public ConfigDataBase
{
    explicit ConfigData()
        : ConfigDataBase(QByteArrayLiteral("CalibrationMatrix"))
    {
    }

    void read(Device *device, const KConfigGroup &values) const override
    {
        if (values.hasKey(key.constData())) {
            auto list = values.readEntry(key.constData(), QList<float>());
            if (list.size() == 16) {
                device->setCalibrationMatrix(QMatrix4x4{list.toVector().constData()});
                return;
            }
        }

        device->setCalibrationMatrix(device->defaultCalibrationMatrix());
    }
};

static const QMap<ConfigKey, std::shared_ptr<ConfigDataBase>> s_configData{
    {ConfigKey::Enabled, std::make_shared<ConfigData<bool>>(QByteArrayLiteral("Enabled"), &Device::setEnabled, &Device::isEnabledByDefault)},
    {ConfigKey::LeftHanded, std::make_shared<ConfigData<bool>>(QByteArrayLiteral("LeftHanded"), &Device::setLeftHanded, &Device::leftHandedEnabledByDefault)},
    {ConfigKey::DisableWhileTyping, std::make_shared<ConfigData<bool>>(QByteArrayLiteral("DisableWhileTyping"), &Device::setDisableWhileTyping, &Device::disableWhileTypingEnabledByDefault)},
    {ConfigKey::PointerAcceleration, std::make_shared<ConfigData<QString>>(QByteArrayLiteral("PointerAcceleration"), &Device::setPointerAccelerationFromString, &Device::defaultPointerAccelerationToString)},
    {ConfigKey::PointerAccelerationProfile, std::make_shared<ConfigData<quint32>>(QByteArrayLiteral("PointerAccelerationProfile"), &Device::setPointerAccelerationProfileFromInt, &Device::defaultPointerAccelerationProfileToInt)},
    {ConfigKey::TapToClick, std::make_shared<ConfigData<bool>>(QByteArrayLiteral("TapToClick"), &Device::setTapToClick, &Device::tapToClickEnabledByDefault)},
    {ConfigKey::TapAndDrag, std::make_shared<ConfigData<bool>>(QByteArrayLiteral("TapAndDrag"), &Device::setTapAndDrag, &Device::tapAndDragEnabledByDefault)},
    {ConfigKey::TapDragLock, std::make_shared<ConfigData<bool>>(QByteArrayLiteral("TapDragLock"), &Device::setTapDragLock, &Device::tapDragLockEnabledByDefault)},
    {ConfigKey::MiddleButtonEmulation, std::make_shared<ConfigData<bool>>(QByteArrayLiteral("MiddleButtonEmulation"), &Device::setMiddleEmulation, &Device::middleEmulationEnabledByDefault)},
    {ConfigKey::LmrTapButtonMap, std::make_shared<ConfigData<bool>>(QByteArrayLiteral("LmrTapButtonMap"), &Device::setLmrTapButtonMap, &Device::lmrTapButtonMapEnabledByDefault)},
    {ConfigKey::NaturalScroll, std::make_shared<ConfigData<bool>>(QByteArrayLiteral("NaturalScroll"), &Device::setNaturalScroll, &Device::naturalScrollEnabledByDefault)},
    {ConfigKey::ScrollMethod, std::make_shared<ConfigData<quint32>>(QByteArrayLiteral("ScrollMethod"), &Device::activateScrollMethodFromInt, &Device::defaultScrollMethodToInt)},
    {ConfigKey::ScrollButton, std::make_shared<ConfigData<quint32>>(QByteArrayLiteral("ScrollButton"), &Device::setScrollButton, &Device::defaultScrollButton)},
    {ConfigKey::ClickMethod, std::make_shared<ConfigData<quint32>>(QByteArrayLiteral("ClickMethod"), &Device::setClickMethodFromInt, &Device::defaultClickMethodToInt)},
    {ConfigKey::ScrollFactor, std::make_shared<ConfigData<qreal>>(QByteArrayLiteral("ScrollFactor"), &Device::setScrollFactor, &Device::scrollFactorDefault)},
    {ConfigKey::Orientation, std::make_shared<ConfigData<DeviceOrientation>>()},
    {ConfigKey::Calibration, std::make_shared<ConfigData<CalibrationMatrix>>()},
    {ConfigKey::OutputName, std::make_shared<ConfigData<QString>>(QByteArrayLiteral("OutputName"), &Device::setOutputName, &Device::defaultOutputName)},
    {ConfigKey::OutputArea, std::make_shared<ConfigData<QRectF>>(QByteArrayLiteral("OutputArea"), &Device::setOutputArea, &Device::defaultOutputArea)},
};

namespace
{
QMatrix4x4 getMatrix(libinput_device *device, std::function<int(libinput_device *, float[6])> getter)
{
    float matrix[6];
    if (!getter(device, matrix)) {
        return {};
    }
    return QMatrix4x4{
        matrix[0], matrix[1], matrix[2], 0.0f,
        matrix[3], matrix[4], matrix[5], 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};
}

bool setOrientedCalibrationMatrix(libinput_device *device, QMatrix4x4 matrix, Qt::ScreenOrientation orientation)
{
    // 90 deg cw
    static const QMatrix4x4 portraitMatrix{
        0.0f, -1.0f, 1.0f, 0.0f,
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};
    // 180 deg cw
    static const QMatrix4x4 invertedLandscapeMatrix{
        -1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, -1.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};
    // 270 deg cw
    static const QMatrix4x4 invertedPortraitMatrix{
        0.0f, 1.0f, 0.0f, 0.0f,
        -1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};

    switch (orientation) {
    case Qt::PortraitOrientation:
        matrix *= portraitMatrix;
        break;
    case Qt::InvertedLandscapeOrientation:
        matrix *= invertedLandscapeMatrix;
        break;
    case Qt::InvertedPortraitOrientation:
        matrix *= invertedPortraitMatrix;
        break;
    case Qt::PrimaryOrientation:
    case Qt::LandscapeOrientation:
    default:
        break;
    }

    float data[6]{matrix(0, 0), matrix(0, 1), matrix(0, 2), matrix(1, 0), matrix(1, 1), matrix(1, 2)};
    return libinput_device_config_calibration_set_matrix(device, data) == LIBINPUT_CONFIG_STATUS_SUCCESS;
}
}

Device::Device(libinput_device *device, QObject *parent)
    : InputDevice(parent)
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
    , m_scrollFactor(1.0)
    , m_supportedPointerAccelerationProfiles(libinput_device_config_accel_get_profiles(m_device))
    , m_defaultPointerAccelerationProfile(libinput_device_config_accel_get_default_profile(m_device))
    , m_pointerAccelerationProfile(libinput_device_config_accel_get_profile(m_device))
    , m_enabled(m_supportsDisableEvents ? libinput_device_config_send_events_get_mode(m_device) == LIBINPUT_CONFIG_SEND_EVENTS_ENABLED : true)
    , m_config()
    , m_defaultCalibrationMatrix(getMatrix(m_device, &libinput_device_config_calibration_get_default_matrix))
    , m_calibrationMatrix(getMatrix(m_device, &libinput_device_config_calibration_get_matrix))
    , m_supportedClickMethods(libinput_device_config_click_get_methods(m_device))
    , m_defaultClickMethod(libinput_device_config_click_get_default_method(m_device))
    , m_clickMethod(libinput_device_config_click_get_method(m_device))
{
    libinput_device_ref(m_device);
    libinput_device_set_user_data(m_device, this);

    qreal width = 0;
    qreal height = 0;
    if (libinput_device_get_size(m_device, &width, &height) == 0) {
        m_size = QSizeF(width, height);
    }
    if (m_pointer) {
        // 0x120 is the first joystick Button
        for (int button = BTN_LEFT; button < 0x120; ++button) {
            if (libinput_device_pointer_has_button(m_device, button)) {
                m_supportedButtons |= buttonToQtMouseButton(button);
            }
        }
    }

    if (m_keyboard) {
        m_alphaNumericKeyboard = checkAlphaNumericKeyboard(m_device);
    }

    if (m_supportsCalibrationMatrix && m_calibrationMatrix != m_defaultCalibrationMatrix) {
        float matrix[]{m_defaultCalibrationMatrix(0, 0),
                       m_defaultCalibrationMatrix(0, 1),
                       m_defaultCalibrationMatrix(0, 2),
                       m_defaultCalibrationMatrix(1, 0),
                       m_defaultCalibrationMatrix(1, 1),
                       m_defaultCalibrationMatrix(1, 2)};
        libinput_device_config_calibration_set_matrix(m_device, matrix);
        m_calibrationMatrix = m_defaultCalibrationMatrix;
    }

    qDBusRegisterMetaType<QMatrix4x4>();

    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kde/KWin/InputDevice/") + m_sysName,
                                                 QStringLiteral("org.kde.KWin.InputDevice"),
                                                 this,
                                                 QDBusConnection::ExportAllProperties);
}

Device::~Device()
{
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/org/kde/KWin/InputDevice/") + m_sysName);
    libinput_device_set_user_data(m_device, nullptr);
    libinput_device_unref(m_device);
}

Device *Device::get(libinput_device *native)
{
    return static_cast<Device *>(libinput_device_get_user_data(native));
}

template<typename T>
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
    m_config.writeEntry(it.value()->key.constData(), value);
    m_config.sync();
}

void Device::loadConfiguration()
{
    if (!m_config.isValid() && !m_defaultConfig.isValid()) {
        return;
    }

    m_loading = true;
    for (auto it = s_configData.begin(), end = s_configData.end(); it != end; ++it) {
        (*it)->read(this, m_config);
    };

    m_loading = false;
}

void Device::setPointerAcceleration(qreal acceleration)
{
    if (!m_supportsPointerAcceleration) {
        return;
    }
    acceleration = std::clamp(acceleration, -1.0, 1.0);
    if (libinput_device_config_accel_set_speed(m_device, acceleration) == LIBINPUT_CONFIG_STATUS_SUCCESS) {
        if (m_pointerAcceleration != acceleration) {
            m_pointerAcceleration = acceleration;
            Q_EMIT pointerAccelerationChanged();
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
            Q_EMIT scrollButtonChanged();
        }
    }
}

void Device::setPointerAccelerationProfile(bool set, enum libinput_config_accel_profile profile)
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
            Q_EMIT pointerAccelerationProfileChanged();
            writeEntry(ConfigKey::PointerAccelerationProfile, (quint32)profile);
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
            Q_EMIT clickMethodChanged();
            writeEntry(ConfigKey::ClickMethod, (quint32)method);
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
            Q_EMIT scrollMethodChanged();
            writeEntry(ConfigKey::ScrollMethod, (quint32)method);
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
            Q_EMIT tapButtonMapChanged();
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

void *Device::groupUserData() const
{
    auto deviceGroup = libinput_device_get_device_group(m_device);
    return libinput_device_group_get_user_data(deviceGroup);
}

#define CONFIG(method, condition, function, variable, key)                                        \
    void Device::method(bool set)                                                                 \
    {                                                                                             \
        if (condition) {                                                                          \
            return;                                                                               \
        }                                                                                         \
        if (libinput_device_config_##function(m_device, set) == LIBINPUT_CONFIG_STATUS_SUCCESS) { \
            if (m_##variable != set) {                                                            \
                m_##variable = set;                                                               \
                writeEntry(ConfigKey::key, m_##variable);                                         \
                Q_EMIT variable##Changed();                                                       \
            }                                                                                     \
        }                                                                                         \
    }

CONFIG(setLeftHanded, !m_supportsLeftHanded, left_handed_set, leftHanded, LeftHanded)
CONFIG(setNaturalScroll, !m_supportsNaturalScroll, scroll_set_natural_scroll_enabled, naturalScroll, NaturalScroll)

#undef CONFIG

#define CONFIG(method, condition, function, enum, variable, key)                                                                                                         \
    void Device::method(bool set)                                                                                                                                        \
    {                                                                                                                                                                    \
        if (condition) {                                                                                                                                                 \
            return;                                                                                                                                                      \
        }                                                                                                                                                                \
        if (libinput_device_config_##function(m_device, set ? LIBINPUT_CONFIG_##enum##_ENABLED : LIBINPUT_CONFIG_##enum##_DISABLED) == LIBINPUT_CONFIG_STATUS_SUCCESS) { \
            if (m_##variable != set) {                                                                                                                                   \
                m_##variable = set;                                                                                                                                      \
                writeEntry(ConfigKey::key, m_##variable);                                                                                                                \
                Q_EMIT variable##Changed();                                                                                                                              \
            }                                                                                                                                                            \
        }                                                                                                                                                                \
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
        Q_EMIT scrollFactorChanged();
    }
}

void Device::setCalibrationMatrix(const QMatrix4x4 &matrix)
{
    if (!m_supportsCalibrationMatrix || m_calibrationMatrix == matrix) {
        return;
    }

    if (setOrientedCalibrationMatrix(m_device, matrix, m_orientation)) {
        QList<float> list;
        list.reserve(16);
        for (uchar row = 0; row < 4; ++row) {
            for (uchar col = 0; col < 4; ++col) {
                list << matrix(row, col);
            }
        }
        writeEntry(ConfigKey::Calibration, list);
        m_calibrationMatrix = matrix;
        Q_EMIT calibrationMatrixChanged();
    }
}

void Device::setOrientation(Qt::ScreenOrientation orientation)
{
    if (!m_supportsCalibrationMatrix || m_orientation == orientation) {
        return;
    }

    if (setOrientedCalibrationMatrix(m_device, m_calibrationMatrix, orientation)) {
        writeEntry(ConfigKey::Orientation, static_cast<int>(orientation));
        m_orientation = orientation;
        Q_EMIT orientationChanged();
    }
}

void Device::setOutputName(const QString &name)
{
#ifndef KWIN_BUILD_TESTING
    if (name == m_outputName) {
        return;
    }

    setOutput(nullptr);
    auto outputs = kwinApp()->outputBackend()->outputs();
    for (int i = 0; i < outputs.count(); ++i) {
        if (!outputs[i]->isEnabled()) {
            continue;
        }
        if (outputs[i]->name() == name) {
            setOutput(outputs[i]);
            break;
        }
    }

    m_outputName = name;
    writeEntry(ConfigKey::OutputName, name);
    Q_EMIT outputNameChanged();
#endif
}

Output *Device::output() const
{
    return m_output;
}

void Device::setOutput(Output *output)
{
    m_output = output;
}

static libinput_led toLibinputLEDS(LEDs leds)
{
    quint32 libinputLeds = 0;
    if (leds.testFlag(LED::NumLock)) {
        libinputLeds = libinputLeds | LIBINPUT_LED_NUM_LOCK;
    }
    if (leds.testFlag(LED::CapsLock)) {
        libinputLeds = libinputLeds | LIBINPUT_LED_CAPS_LOCK;
    }
    if (leds.testFlag(LED::ScrollLock)) {
        libinputLeds = libinputLeds | LIBINPUT_LED_SCROLL_LOCK;
    }
    return libinput_led(libinputLeds);
}

LEDs Device::leds() const
{
    return m_leds;
}

void Device::setLeds(LEDs leds)
{
    if (m_leds != leds) {
        m_leds = leds;
        libinput_device_led_update(m_device, toLibinputLEDS(m_leds));
    }
}

bool Device::supportsOutputArea() const
{
    return m_tabletTool;
}

QRectF Device::defaultOutputArea() const
{
    return QRectF(0, 0, 1, 1);
}

QRectF Device::outputArea() const
{
    return m_outputArea;
}

void Device::setOutputArea(const QRectF &outputArea)
{
    if (m_outputArea != outputArea) {
        m_outputArea = outputArea;
        writeEntry(ConfigKey::OutputArea, m_outputArea);
        Q_EMIT outputAreaChanged();
    }
}
}
}
