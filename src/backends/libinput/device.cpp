/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "device.h"

#include "config-kwin.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "libinput_logging.h"
#include "main.h"
#include "mousebuttons.h"
#include "pointer_input.h"

#include <QCryptographicHash>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>

#include <linux/input.h>

namespace KWin
{
namespace LibInput
{
static const QRectF s_identityRect = QRectF(0, 0, 1, 1);

TabletTool::TabletTool(libinput_tablet_tool *handle)
    : m_handle(libinput_tablet_tool_ref(handle))
{
}

TabletTool::~TabletTool()
{
    libinput_tablet_tool_unref(m_handle);
}

libinput_tablet_tool *TabletTool::handle() const
{
    return m_handle;
}

quint64 TabletTool::serialId() const
{
    return libinput_tablet_tool_get_serial(m_handle);
}

quint64 TabletTool::uniqueId() const
{
    return libinput_tablet_tool_get_tool_id(m_handle);
}

TabletTool::Type TabletTool::type() const
{
    switch (libinput_tablet_tool_get_type(m_handle)) {
    case LIBINPUT_TABLET_TOOL_TYPE_PEN:
        return Type::Pen;
    case LIBINPUT_TABLET_TOOL_TYPE_ERASER:
        return Type::Eraser;
    case LIBINPUT_TABLET_TOOL_TYPE_BRUSH:
        return Type::Brush;
    case LIBINPUT_TABLET_TOOL_TYPE_PENCIL:
        return Type::Pencil;
    case LIBINPUT_TABLET_TOOL_TYPE_AIRBRUSH:
        return Type::Airbrush;
    case LIBINPUT_TABLET_TOOL_TYPE_MOUSE:
        return Type::Mouse;
    case LIBINPUT_TABLET_TOOL_TYPE_LENS:
        return Type::Lens;
    case LIBINPUT_TABLET_TOOL_TYPE_TOTEM:
        return Type::Totem;
    default:
        return Type();
    }
}

QList<TabletTool::Capability> TabletTool::capabilities() const
{
    QList<Capability> capabilities;
    if (libinput_tablet_tool_has_pressure(m_handle)) {
        capabilities << Capability::Pressure;
    }
    if (libinput_tablet_tool_has_distance(m_handle)) {
        capabilities << Capability::Distance;
    }
    if (libinput_tablet_tool_has_rotation(m_handle)) {
        capabilities << Capability::Rotation;
    }
    if (libinput_tablet_tool_has_tilt(m_handle)) {
        capabilities << Capability::Tilt;
    }
    if (libinput_tablet_tool_has_slider(m_handle)) {
        capabilities << Capability::Slider;
    }
    if (libinput_tablet_tool_has_wheel(m_handle)) {
        capabilities << Capability::Wheel;
    }
    return capabilities;
}

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
    DisableEventsOnExternalMouse,
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
    MapToWorkspace,
    TabletToolPressureCurve,
    TabletToolPressureRangeMin,
    TabletToolPressureRangeMax,
    InputArea,
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
        device->setCalibrationMatrix(values.readEntry(key.constData(), device->defaultCalibrationMatrix()));
    }
};

static const QMap<ConfigKey, std::shared_ptr<ConfigDataBase>> s_configData{
    {ConfigKey::Enabled, std::make_shared<ConfigData<bool>>(QByteArrayLiteral("Enabled"), &Device::setEnabled, &Device::isEnabledByDefault)},
    {ConfigKey::DisableEventsOnExternalMouse, std::make_shared<ConfigData<bool>>(QByteArrayLiteral("DisableEventsOnExternalMouse"), &Device::setDisableEventsOnExternalMouse, &Device::disableEventsOnExternalMouseEnabledByDefault)},
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
    {ConfigKey::TabletToolPressureCurve, std::make_shared<ConfigData<QString>>(QByteArrayLiteral("TabletToolPressureCurve"), &Device::setPressureCurve, &Device::defaultPressureCurve)},
    {ConfigKey::OutputName, std::make_shared<ConfigData<QString>>(QByteArrayLiteral("OutputName"), &Device::setOutputName, &Device::defaultOutputName)},
    {ConfigKey::OutputArea, std::make_shared<ConfigData<QRectF>>(QByteArrayLiteral("OutputArea"), &Device::setOutputArea, &Device::defaultOutputArea)},
    {ConfigKey::MapToWorkspace, std::make_shared<ConfigData<bool>>(QByteArrayLiteral("MapToWorkspace"), &Device::setMapToWorkspace, &Device::defaultMapToWorkspace)},
    {ConfigKey::TabletToolPressureRangeMin, std::make_shared<ConfigData<double>>(QByteArrayLiteral("TabletToolPressureRangeMin"), &Device::setPressureRangeMin, &Device::defaultPressureRangeMin)},
    {ConfigKey::TabletToolPressureRangeMax, std::make_shared<ConfigData<double>>(QByteArrayLiteral("TabletToolPressureRangeMax"), &Device::setPressureRangeMax, &Device::defaultPressureRangeMax)},
    {ConfigKey::InputArea, std::make_shared<ConfigData<QRectF>>(QByteArrayLiteral("InputArea"), &Device::setInputArea, &Device::defaultInputArea)},
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
    , m_touchpad(m_pointer && udev_device_get_property_value(libinput_device_get_udev_device(m_device), "ID_INPUT_TOUCHPAD"))
    , m_name(QString::fromLocal8Bit(libinput_device_get_name(m_device)))
    , m_sysName(QString::fromLocal8Bit(libinput_device_get_sysname(m_device)))
    , m_sysPath(QString::fromLocal8Bit(udev_device_get_syspath(libinput_device_get_udev_device(m_device))))
    , m_outputName(QString::fromLocal8Bit(libinput_device_get_output_name(m_device)))
    , m_product(libinput_device_get_id_product(m_device))
    , m_vendor(libinput_device_get_id_vendor(m_device))
    , m_tapFingerCount(libinput_device_config_tap_get_finger_count(m_device))
    , m_defaultTapButtonMap(libinput_device_config_tap_get_default_button_map(m_device))
    , m_tapButtonMap(libinput_device_config_tap_get_button_map(m_device))
    , m_tapToClickEnabledByDefault(true)
    , m_tapToClick(libinput_device_config_tap_get_enabled(m_device))
    , m_tapAndDragEnabledByDefault(true)
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
    , m_enabled(m_supportsDisableEvents ? (libinput_device_config_send_events_get_mode(m_device) & LIBINPUT_CONFIG_SEND_EVENTS_DISABLED) == 0 : true)
    , m_disableEventsOnExternalMouseEnabledByDefault(m_supportsDisableEventsOnExternalMouse && (libinput_device_config_send_events_get_default_mode(m_device) & LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE))
    , m_disableEventsOnExternalMouse(m_supportsDisableEventsOnExternalMouse && (libinput_device_config_send_events_get_mode(m_device) & LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE))
    , m_config()
    , m_defaultCalibrationMatrix(getMatrix(m_device, &libinput_device_config_calibration_get_default_matrix))
    , m_calibrationMatrix(getMatrix(m_device, &libinput_device_config_calibration_get_matrix))
    , m_pressureCurve(deserializePressureCurve(defaultPressureCurve()))
    , m_supportedClickMethods(libinput_device_config_click_get_methods(m_device))
    , m_defaultClickMethod(libinput_device_config_click_get_default_method(m_device))
    , m_clickMethod(libinput_device_config_click_get_method(m_device))
    , m_outputArea(s_identityRect)
    , m_supportsPressureRange(false)
    , m_pressureRangeMin(0.0)
    , m_pressureRangeMax(1.0)
    , m_defaultPressureRangeMin(0.0)
    , m_defaultPressureRangeMax(1.0)
    , m_inputArea(s_identityRect)
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

    if (supportsInputArea() && m_inputArea != defaultInputArea()) {
#if HAVE_LIBINPUT_INPUT_AREA
        const libinput_config_area_rectangle rect{
            .x1 = m_inputArea.topLeft().x(),
            .y1 = m_inputArea.topLeft().y(),
            .x2 = m_inputArea.bottomRight().x(),
            .y2 = m_inputArea.bottomRight().y(),
        };
        libinput_device_config_area_set_rectangle(m_device, &rect);
#endif
    }

    libinput_device_group *group = libinput_device_get_device_group(device);
    m_deviceGroupId = QCryptographicHash::hash(QString::asprintf("%p", group).toLatin1(), QCryptographicHash::Sha1).toBase64();

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

void *Device::group() const
{
    return libinput_device_get_device_group(m_device);
}

int Device::tabletPadButtonCount() const
{
    return libinput_device_tablet_pad_get_num_buttons(m_device);
}

QList<InputDeviceTabletPadModeGroup> Device::modeGroups() const
{
    QList<InputDeviceTabletPadModeGroup> result;

    int numGroups = libinput_device_tablet_pad_get_num_mode_groups(m_device);

    for (int groupIndex = 0; groupIndex < numGroups; ++groupIndex) {
        libinput_tablet_pad_mode_group *group = libinput_device_tablet_pad_get_mode_group(m_device, groupIndex);
        int modeCount = libinput_tablet_pad_mode_group_get_num_modes(group);

        QList<int> buttons;
        int totalButtons = libinput_device_tablet_pad_get_num_buttons(m_device);
        for (int buttonIndex = 0; buttonIndex < totalButtons; ++buttonIndex) {
            if (libinput_tablet_pad_mode_group_has_button(group, buttonIndex)) {
                buttons << buttonIndex;
            }
        }

        QList<int> rings;
        int totalRings = libinput_device_tablet_pad_get_num_rings(m_device);
        for (int ringIndex = 0; ringIndex < totalRings; ++ringIndex) {
            if (libinput_tablet_pad_mode_group_has_ring(group, ringIndex)) {
                rings << ringIndex;
            }
        }

        QList<int> strips;
        int totalStrips = libinput_device_tablet_pad_get_num_strips(m_device);
        for (int stripIndex = 0; stripIndex < totalStrips; ++stripIndex) {
            if (libinput_tablet_pad_mode_group_has_strip(group, stripIndex)) {
                strips << stripIndex;
            }
        }

        result << InputDeviceTabletPadModeGroup{
            .modeCount = modeCount,
            .buttons = buttons,
            .rings = rings,
            .strips = strips,
        };
    }
    return result;
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

CONFIG(setDisableWhileTyping, !m_supportsDisableWhileTyping, dwt_set_enabled, DWT, disableWhileTyping, DisableWhileTyping)
CONFIG(setTapToClick, m_tapFingerCount == 0, tap_set_enabled, TAP, tapToClick, TapToClick)
CONFIG(setTapAndDrag, false, tap_set_drag_enabled, DRAG, tapAndDrag, TapAndDrag)
CONFIG(setTapDragLock, false, tap_set_drag_lock_enabled, DRAG_LOCK, tapDragLock, TapDragLock)
CONFIG(setMiddleEmulation, m_supportsMiddleEmulation == false, middle_emulation_set_enabled, MIDDLE_EMULATION, middleEmulation, MiddleButtonEmulation)

#undef CONFIG

void Device::setEnabled(bool set)
{
    if (!m_supportsDisableEvents) {
        return;
    }
    const auto enabledMode = (m_supportsDisableEventsOnExternalMouse && m_disableEventsOnExternalMouse) ? LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE : LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
    const auto mode = set ? enabledMode : LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;

    if (libinput_device_config_send_events_set_mode(m_device, mode) == LIBINPUT_CONFIG_STATUS_SUCCESS) {
        if (m_enabled != set) {
            m_enabled = set;
            writeEntry(ConfigKey::Enabled, m_enabled);
            Q_EMIT enabledChanged();
        }
    }
}

void Device::setDisableEventsOnExternalMouse(bool set)
{
    if (!m_supportsDisableEventsOnExternalMouse) {
        return;
    }
    const auto enabledMode = set ? LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE : LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;

    if (!m_enabled || libinput_device_config_send_events_set_mode(m_device, enabledMode) == LIBINPUT_CONFIG_STATUS_SUCCESS) {
        if (m_disableEventsOnExternalMouse != set) {
            m_disableEventsOnExternalMouse = set;
            writeEntry(ConfigKey::DisableEventsOnExternalMouse, m_disableEventsOnExternalMouse);
            Q_EMIT disableEventsOnExternalMouseChanged();
        }
    }
}

void Device::setScrollFactor(qreal factor)
{
    if (m_scrollFactor != factor) {
        m_scrollFactor = factor;
        writeEntry(ConfigKey::ScrollFactor, m_scrollFactor);
        Q_EMIT scrollFactorChanged();
    }
}

void Device::setCalibrationMatrix(const QString &value)
{
    const auto matrix = deserializeMatrix(value);
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

QString Device::defaultPressureCurve() const
{
    QEasingCurve curve(QEasingCurve::Type::BezierSpline);
    curve.addCubicBezierSegment(QPointF{0.0f, 0.0f}, QPointF{1.0f, 1.0f}, QPointF{1.0f, 1.0f});
    return serializePressureCurve(curve);
}

QEasingCurve Device::pressureCurve() const
{
    return m_pressureCurve;
}

QString Device::serializedPressureCurve() const
{
    return serializePressureCurve(m_pressureCurve);
}

void Device::setPressureCurve(const QString &curve)
{
    const auto easingCurve = deserializePressureCurve(curve);
    if (m_pressureCurve != easingCurve) {
        writeEntry(ConfigKey::TabletToolPressureCurve, curve);
        m_pressureCurve = easingCurve;
        Q_EMIT pressureCurveChanged();
    }
}

QString Device::serializePressureCurve(const QEasingCurve &curve)
{
    // We only care about the first two points. toCubicSpline adds the end point as the third, but to us that's always (1,1).
    const auto points = curve.toCubicSpline().first(2);
    QString serializedString;
    for (const QPointF &pair : points) {
        serializedString += QString::number(pair.x());
        serializedString += ',';
        serializedString += QString::number(pair.y());
        serializedString += ';';
    }

    return serializedString;
}

QEasingCurve Device::deserializePressureCurve(const QString &curve)
{
    const QStringList data = curve.split(';');

    QList<QPointF> points;
    for (const QString &pair : data) {
        if (pair.indexOf(',') > -1) {
            points.append({pair.section(',', 0, 0).toDouble(),
                           pair.section(',', 1, 1).toDouble()});
        }
    }

    auto easingCurve = QEasingCurve(QEasingCurve::Type::BezierSpline);

    // We only support 2 points
    if (points.size() >= 2) {
        easingCurve.addCubicBezierSegment(points.at(0), points.at(1), QPointF{1.0f, 1.0f});
    }
    return easingCurve;
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
    if (leds.testFlag(LED::Compose)) {
        libinputLeds = libinputLeds | LIBINPUT_LED_COMPOSE;
    }
    if (leds.testFlag(LED::Kana)) {
        libinputLeds = libinputLeds | LIBINPUT_LED_KANA;
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
    return s_identityRect;
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

void Device::setMapToWorkspace(bool mapToWorkspace)
{
    if (m_mapToWorkspace != mapToWorkspace) {
        m_mapToWorkspace = mapToWorkspace;
        writeEntry(ConfigKey::MapToWorkspace, m_mapToWorkspace);
        Q_EMIT mapToWorkspaceChanged();
    }
}

bool Device::supportsPressureRange() const
{
    return m_supportsPressureRange;
}

void Device::setSupportsPressureRange(const bool supported)
{
    if (m_supportsPressureRange != supported) {
        m_supportsPressureRange = supported;
        Q_EMIT supportsPressureRangeChanged();
    }
}

double Device::pressureRangeMin() const
{
    return m_pressureRangeMin;
}

void Device::setPressureRangeMin(const double value)
{
    if (m_pressureRangeMin != value) {
        m_pressureRangeMin = value;
        writeEntry(ConfigKey::TabletToolPressureRangeMin, m_pressureRangeMin);
        Q_EMIT pressureRangeMinChanged();
    }
}

double Device::pressureRangeMax() const
{
    return m_pressureRangeMax;
}

void Device::setPressureRangeMax(const double value)
{
    if (m_pressureRangeMax != value) {
        m_pressureRangeMax = value;
        writeEntry(ConfigKey::TabletToolPressureRangeMax, m_pressureRangeMax);
        Q_EMIT pressureRangeMaxChanged();
    }
}

double Device::defaultPressureRangeMin() const
{
    return m_defaultPressureRangeMin;
}

double Device::defaultPressureRangeMax() const
{
    return m_defaultPressureRangeMax;
}

bool Device::supportsInputArea() const
{
#if HAVE_LIBINPUT_INPUT_AREA
    return true;
#else
    return false;
#endif
}

QRectF Device::inputArea() const
{
    return m_inputArea;
}

void Device::setInputArea(const QRectF &inputArea)
{
    if (m_inputArea != inputArea) {
        m_inputArea = inputArea;

#if HAVE_LIBINPUT_INPUT_AREA
        const libinput_config_area_rectangle rect{
            .x1 = m_inputArea.topLeft().x(),
            .y1 = m_inputArea.topLeft().y(),
            .x2 = m_inputArea.bottomRight().x(),
            .y2 = m_inputArea.bottomRight().y(),
        };
        libinput_device_config_area_set_rectangle(m_device, &rect);
#endif

        writeEntry(ConfigKey::InputArea, m_inputArea);
        Q_EMIT inputAreaChanged();
    }
}

QRectF Device::defaultInputArea() const
{
    return s_identityRect;
}

QString Device::serializeMatrix(const QMatrix4x4 &matrix)
{
    QString result;
    for (int i = 0; i < 16; i++) {
        result.append(QString::number(matrix.constData()[i]));
        if (i != 15) {
            result.append(QLatin1Char(','));
        }
    }
    return result;
}

QMatrix4x4 Device::deserializeMatrix(const QString &matrix)
{
    const auto items = QStringView(matrix).split(QLatin1Char(','));
    if (items.size() == 16) {
        QList<float> data;
        data.reserve(16);
        std::ranges::transform(std::as_const(items), std::back_inserter(data), [](const QStringView &item) {
            return item.toFloat();
        });

        return QMatrix4x4{data.constData()};
    }

    return QMatrix4x4{};
}
}
}

#include "moc_device.cpp"
