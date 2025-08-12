/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/inputdevice.h"

#include <libinput.h>

#include <KConfigGroup>
#include <QEasingCurve>

#include <QList>
#include <QMatrix4x4>
#include <QObject>
#include <QPointer>
#include <QSizeF>

struct libinput_device;

namespace KWin
{
class LogicalOutput;

namespace LibInput
{
enum class ConfigKey;

class TabletTool : public InputDeviceTabletTool
{
    Q_OBJECT

public:
    explicit TabletTool(libinput_tablet_tool *handle);
    ~TabletTool() override;

    libinput_tablet_tool *handle() const;

    quint64 serialId() const override;
    quint64 uniqueId() const override;

    Type type() const override;
    QList<Capability> capabilities() const override;

private:
    libinput_tablet_tool *const m_handle;
};

class KWIN_EXPORT Device : public InputDevice
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.InputDevice")
    //
    // general
    Q_PROPERTY(bool keyboard READ isKeyboard CONSTANT)
    Q_PROPERTY(bool alphaNumericKeyboard READ isAlphaNumericKeyboard CONSTANT)
    Q_PROPERTY(bool pointer READ isPointer CONSTANT)
    Q_PROPERTY(bool touchpad READ isTouchpad CONSTANT)
    Q_PROPERTY(bool touch READ isTouch CONSTANT)
    Q_PROPERTY(bool tabletTool READ isTabletTool CONSTANT)
    Q_PROPERTY(bool tabletPad READ isTabletPad CONSTANT)
    Q_PROPERTY(bool gestureSupport READ supportsGesture CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString sysName READ sysName CONSTANT)
    Q_PROPERTY(QString outputName READ outputName WRITE setOutputName NOTIFY outputNameChanged)
    Q_PROPERTY(QSizeF size READ size CONSTANT)
    Q_PROPERTY(quint32 product READ product CONSTANT)
    Q_PROPERTY(quint32 vendor READ vendor CONSTANT)
    Q_PROPERTY(bool supportsDisableEvents READ supportsDisableEvents CONSTANT)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool enabledByDefault READ isEnabledByDefault CONSTANT)
    //
    // advanced
    Q_PROPERTY(int supportedButtons READ supportedButtons CONSTANT)
    Q_PROPERTY(bool supportsCalibrationMatrix READ supportsCalibrationMatrix CONSTANT)
    Q_PROPERTY(QString defaultCalibrationMatrix READ defaultCalibrationMatrix CONSTANT)
    Q_PROPERTY(QString calibrationMatrix READ serializedCalibrationMatrix WRITE setCalibrationMatrix NOTIFY calibrationMatrixChanged)
    Q_PROPERTY(Qt::ScreenOrientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    Q_PROPERTY(int orientationDBus READ orientation WRITE setOrientationDBus NOTIFY orientationChanged)

    Q_PROPERTY(bool supportsLeftHanded READ supportsLeftHanded CONSTANT)
    Q_PROPERTY(bool leftHandedEnabledByDefault READ leftHandedEnabledByDefault CONSTANT)
    Q_PROPERTY(bool leftHanded READ isLeftHanded WRITE setLeftHanded NOTIFY leftHandedChanged)

    Q_PROPERTY(bool supportsDisableEventsOnExternalMouse READ supportsDisableEventsOnExternalMouse CONSTANT)
    Q_PROPERTY(bool disableEventsOnExternalMouseEnabledByDefault READ disableEventsOnExternalMouseEnabledByDefault CONSTANT)
    Q_PROPERTY(bool disableEventsOnExternalMouse READ isDisableEventsOnExternalMouse WRITE setDisableEventsOnExternalMouse NOTIFY disableEventsOnExternalMouseChanged)

    Q_PROPERTY(bool supportsDisableWhileTyping READ supportsDisableWhileTyping CONSTANT)
    Q_PROPERTY(bool disableWhileTypingEnabledByDefault READ disableWhileTypingEnabledByDefault CONSTANT)
    Q_PROPERTY(bool disableWhileTyping READ isDisableWhileTyping WRITE setDisableWhileTyping NOTIFY disableWhileTypingChanged)
    //
    // acceleration speed and profile
    Q_PROPERTY(bool supportsPointerAcceleration READ supportsPointerAcceleration CONSTANT)
    Q_PROPERTY(qreal defaultPointerAcceleration READ defaultPointerAcceleration CONSTANT)
    Q_PROPERTY(qreal pointerAcceleration READ pointerAcceleration WRITE setPointerAcceleration NOTIFY pointerAccelerationChanged)

    Q_PROPERTY(bool supportsPointerAccelerationProfileFlat READ supportsPointerAccelerationProfileFlat CONSTANT)
    Q_PROPERTY(bool defaultPointerAccelerationProfileFlat READ defaultPointerAccelerationProfileFlat CONSTANT)
    Q_PROPERTY(bool pointerAccelerationProfileFlat READ pointerAccelerationProfileFlat WRITE setPointerAccelerationProfileFlat NOTIFY pointerAccelerationProfileChanged)

    Q_PROPERTY(bool supportsPointerAccelerationProfileAdaptive READ supportsPointerAccelerationProfileAdaptive CONSTANT)
    Q_PROPERTY(bool defaultPointerAccelerationProfileAdaptive READ defaultPointerAccelerationProfileAdaptive CONSTANT)
    Q_PROPERTY(bool pointerAccelerationProfileAdaptive READ pointerAccelerationProfileAdaptive WRITE setPointerAccelerationProfileAdaptive NOTIFY pointerAccelerationProfileChanged)
    //
    // tapping
    Q_PROPERTY(int tapFingerCount READ tapFingerCount CONSTANT)
    Q_PROPERTY(bool tapToClickEnabledByDefault READ tapToClickEnabledByDefault CONSTANT)
    Q_PROPERTY(bool tapToClick READ isTapToClick WRITE setTapToClick NOTIFY tapToClickChanged)

    Q_PROPERTY(bool supportsLmrTapButtonMap READ supportsLmrTapButtonMap CONSTANT)
    Q_PROPERTY(bool lmrTapButtonMapEnabledByDefault READ lmrTapButtonMapEnabledByDefault CONSTANT)
    Q_PROPERTY(bool lmrTapButtonMap READ lmrTapButtonMap WRITE setLmrTapButtonMap NOTIFY tapButtonMapChanged)

    Q_PROPERTY(bool tapAndDragEnabledByDefault READ tapAndDragEnabledByDefault CONSTANT)
    Q_PROPERTY(bool tapAndDrag READ isTapAndDrag WRITE setTapAndDrag NOTIFY tapAndDragChanged)
    Q_PROPERTY(bool tapDragLockEnabledByDefault READ tapDragLockEnabledByDefault CONSTANT)
    Q_PROPERTY(bool tapDragLock READ isTapDragLock WRITE setTapDragLock NOTIFY tapDragLockChanged)

    Q_PROPERTY(bool supportsMiddleEmulation READ supportsMiddleEmulation CONSTANT)
    Q_PROPERTY(bool middleEmulationEnabledByDefault READ middleEmulationEnabledByDefault CONSTANT)
    Q_PROPERTY(bool middleEmulation READ isMiddleEmulation WRITE setMiddleEmulation NOTIFY middleEmulationChanged)
    //
    // scrolling
    Q_PROPERTY(bool supportsNaturalScroll READ supportsNaturalScroll CONSTANT)
    Q_PROPERTY(bool naturalScrollEnabledByDefault READ naturalScrollEnabledByDefault CONSTANT)
    Q_PROPERTY(bool naturalScroll READ isNaturalScroll WRITE setNaturalScroll NOTIFY naturalScrollChanged)

    Q_PROPERTY(bool supportsScrollTwoFinger READ supportsScrollTwoFinger CONSTANT)
    Q_PROPERTY(bool scrollTwoFingerEnabledByDefault READ scrollTwoFingerEnabledByDefault CONSTANT)
    Q_PROPERTY(bool scrollTwoFinger READ isScrollTwoFinger WRITE setScrollTwoFinger NOTIFY scrollMethodChanged)

    Q_PROPERTY(bool supportsScrollEdge READ supportsScrollEdge CONSTANT)
    Q_PROPERTY(bool scrollEdgeEnabledByDefault READ scrollEdgeEnabledByDefault CONSTANT)
    Q_PROPERTY(bool scrollEdge READ isScrollEdge WRITE setScrollEdge NOTIFY scrollMethodChanged)

    Q_PROPERTY(bool supportsScrollOnButtonDown READ supportsScrollOnButtonDown CONSTANT)
    Q_PROPERTY(bool scrollOnButtonDownEnabledByDefault READ scrollOnButtonDownEnabledByDefault CONSTANT)
    Q_PROPERTY(quint32 defaultScrollButton READ defaultScrollButton CONSTANT)
    Q_PROPERTY(bool scrollOnButtonDown READ isScrollOnButtonDown WRITE setScrollOnButtonDown NOTIFY scrollMethodChanged)
    Q_PROPERTY(quint32 scrollButton READ scrollButton WRITE setScrollButton NOTIFY scrollButtonChanged)

    Q_PROPERTY(qreal scrollFactor READ scrollFactor WRITE setScrollFactor NOTIFY scrollFactorChanged)
    //
    // switches
    Q_PROPERTY(bool switchDevice READ isSwitch CONSTANT)
    Q_PROPERTY(bool lidSwitch READ isLidSwitch CONSTANT)
    Q_PROPERTY(bool tabletModeSwitch READ isTabletModeSwitch CONSTANT)

    // Click Methods
    Q_PROPERTY(bool supportsClickMethodAreas READ supportsClickMethodAreas CONSTANT)
    Q_PROPERTY(bool defaultClickMethodAreas READ defaultClickMethodAreas CONSTANT)
    Q_PROPERTY(bool clickMethodAreas READ isClickMethodAreas WRITE setClickMethodAreas NOTIFY clickMethodChanged)

    Q_PROPERTY(bool supportsClickMethodClickfinger READ supportsClickMethodClickfinger CONSTANT)
    Q_PROPERTY(bool defaultClickMethodClickfinger READ defaultClickMethodClickfinger CONSTANT)
    Q_PROPERTY(bool clickMethodClickfinger READ isClickMethodClickfinger WRITE setClickMethodClickfinger NOTIFY clickMethodChanged)

    Q_PROPERTY(bool supportsOutputArea READ supportsOutputArea CONSTANT)
    Q_PROPERTY(QRectF defaultOutputArea READ defaultOutputArea CONSTANT)
    Q_PROPERTY(QRectF outputArea READ outputArea WRITE setOutputArea NOTIFY outputAreaChanged)
    Q_PROPERTY(bool defaultMapToWorkspace READ defaultMapToWorkspace CONSTANT)
    Q_PROPERTY(bool mapToWorkspace READ isMapToWorkspace WRITE setMapToWorkspace NOTIFY mapToWorkspaceChanged)
    Q_PROPERTY(QString deviceGroupId READ deviceGroupId CONSTANT)
    Q_PROPERTY(QString defaultPressureCurve READ defaultPressureCurve CONSTANT)
    Q_PROPERTY(QString pressureCurve READ serializedPressureCurve WRITE setPressureCurve NOTIFY pressureCurveChanged)
    Q_PROPERTY(quint32 tabletPadButtonCount READ tabletPadButtonCount CONSTANT)
    Q_PROPERTY(quint32 tabletPadDialCount READ tabletPadDialCount CONSTANT)
    Q_PROPERTY(quint32 tabletPadRingCount READ tabletPadRingCount CONSTANT)
    Q_PROPERTY(quint32 tabletPadStripCount READ tabletPadStripCount CONSTANT)
    Q_PROPERTY(bool supportsInputArea READ supportsInputArea CONSTANT)
    Q_PROPERTY(QRectF defaultInputArea READ defaultInputArea CONSTANT)
    Q_PROPERTY(QRectF inputArea READ inputArea WRITE setInputArea NOTIFY inputAreaChanged)
    Q_PROPERTY(QList<unsigned int> numModes READ numModes CONSTANT)
    Q_PROPERTY(QList<unsigned int> currentModes READ currentModes NOTIFY currentModesChanged)

    Q_PROPERTY(bool supportsPressureRange READ supportsPressureRange NOTIFY supportsPressureRangeChanged)
    Q_PROPERTY(double pressureRangeMin READ pressureRangeMin WRITE setPressureRangeMin NOTIFY pressureRangeMinChanged)
    Q_PROPERTY(double pressureRangeMax READ pressureRangeMax WRITE setPressureRangeMax NOTIFY pressureRangeMaxChanged)
    Q_PROPERTY(double defaultPressureRangeMin READ defaultPressureRangeMin CONSTANT)
    Q_PROPERTY(double defaultPressureRangeMax READ defaultPressureRangeMax CONSTANT)

    Q_PROPERTY(bool tabletToolIsRelative READ tabletToolIsRelative WRITE setTabletToolRelative NOTIFY tabletToolRelativeChanged)
    Q_PROPERTY(bool supportsRotation READ supportsRotation CONSTANT)
    /// rotation angle, as 0 to 360 degrees
    Q_PROPERTY(uint32_t rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(uint32_t defaultRotation READ defaultRotation CONSTANT)
    Q_PROPERTY(bool isVirtual READ isVirtual CONSTANT)

public:
    explicit Device(libinput_device *device, QObject *parent = nullptr);
    ~Device() override;

    bool isKeyboard() const override
    {
        return m_keyboard;
    }
    /**
     * Note that this has a lot of false positives
     */
    bool isAlphaNumericKeyboard() const
    {
        return m_alphaNumericKeyboard;
    }
    bool isPointer() const override
    {
        return m_pointer;
    }
    bool isTouchpad() const override
    {
        return m_touchpad &&
            // ignore all combined devices. E.g. a touchpad on a keyboard we don't want to toggle
            // as that would result in the keyboard going off as well
            !(m_keyboard || m_touch || m_tabletPad || m_tabletTool);
    }
    bool isTouch() const override
    {
        return m_touch;
    }
    bool isTabletTool() const override
    {
        return m_tabletTool;
    }
    bool isTabletPad() const override
    {
        return m_tabletPad;
    }
    bool supportsGesture() const
    {
        return m_supportsGesture;
    }
    QString name() const override
    {
        return m_name;
    }
    QString sysName() const
    {
        return m_sysName;
    }
    QString sysPath() const override
    {
        return m_sysPath;
    }
    QString outputName() const override
    {
        return m_outputName;
    }
    QSizeF size() const
    {
        return m_size;
    }
    quint32 product() const override
    {
        return m_product;
    }
    quint32 vendor() const override
    {
        return m_vendor;
    }
    quint32 busType() const override
    {
        return m_busType;
    }
    void *group() const override;
    Qt::MouseButtons supportedButtons() const
    {
        return m_supportedButtons;
    }
    int tapFingerCount() const
    {
        return m_tapFingerCount;
    }
    bool tapToClickEnabledByDefault() const
    {
        return defaultValue("TapToClick", m_tapToClickEnabledByDefault);
    }
    bool isTapToClick() const
    {
        return m_tapToClick;
    }
    /**
     * Set the Device to tap to click if @p set is @c true.
     */
    void setTapToClick(bool set);
    bool tapAndDragEnabledByDefault() const
    {
        return defaultValue("TapAndDrag", m_tapAndDragEnabledByDefault);
    }
    bool isTapAndDrag() const
    {
        return m_tapAndDrag;
    }
    void setTapAndDrag(bool set);
    bool tapDragLockEnabledByDefault() const
    {
        return defaultValue("TapDragLock", m_tapDragLockEnabledByDefault);
    }
    bool isTapDragLock() const
    {
        return m_tapDragLock;
    }
    void setTapDragLock(bool set);
    bool supportsDisableWhileTyping() const
    {
        return m_supportsDisableWhileTyping;
    }
    bool disableWhileTypingEnabledByDefault() const
    {
        return defaultValue("DisableWhileTyping", m_disableWhileTypingEnabledByDefault);
    }
    bool supportsPointerAcceleration() const
    {
        return m_supportsPointerAcceleration;
    }
    bool supportsLeftHanded() const
    {
        return m_supportsLeftHanded;
    }
    bool supportsCalibrationMatrix() const
    {
        return m_supportsCalibrationMatrix;
    }
    bool supportsDisableEvents() const
    {
        return m_supportsDisableEvents;
    }
    bool supportsDisableEventsOnExternalMouse() const
    {
        return m_supportsDisableEventsOnExternalMouse;
    }
    bool supportsMiddleEmulation() const
    {
        return m_supportsMiddleEmulation;
    }
    bool supportsNaturalScroll() const
    {
        return m_supportsNaturalScroll;
    }
    bool supportsScrollTwoFinger() const
    {
        return (m_supportedScrollMethods & LIBINPUT_CONFIG_SCROLL_2FG);
    }
    bool supportsScrollEdge() const
    {
        return (m_supportedScrollMethods & LIBINPUT_CONFIG_SCROLL_EDGE);
    }
    bool supportsScrollOnButtonDown() const
    {
        return (m_supportedScrollMethods & LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN);
    }
    bool leftHandedEnabledByDefault() const
    {
        return defaultValue("LeftHanded", m_leftHandedEnabledByDefault);
    }
    bool middleEmulationEnabledByDefault() const
    {
        return defaultValue("MiddleButtonEmulation", m_middleEmulationEnabledByDefault);
    }
    bool naturalScrollEnabledByDefault() const
    {
        return defaultValue("NaturalScroll", m_naturalScrollEnabledByDefault);
    }
    enum libinput_config_scroll_method defaultScrollMethod() const
    {
        quint32 defaultScrollMethod = defaultValue("ScrollMethod", static_cast<quint32>(m_defaultScrollMethod));
        return static_cast<libinput_config_scroll_method>(defaultScrollMethod);
    }
    quint32 defaultScrollMethodToInt() const
    {
        return static_cast<quint32>(defaultScrollMethod());
    }
    bool scrollTwoFingerEnabledByDefault() const
    {
        return defaultScrollMethod() == LIBINPUT_CONFIG_SCROLL_2FG;
    }
    bool scrollEdgeEnabledByDefault() const
    {
        return defaultScrollMethod() == LIBINPUT_CONFIG_SCROLL_EDGE;
    }
    bool scrollOnButtonDownEnabledByDefault() const
    {
        return defaultScrollMethod() == LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
    }
    bool supportsLmrTapButtonMap() const
    {
        return m_tapFingerCount > 1;
    }
    bool lmrTapButtonMapEnabledByDefault() const
    {
        quint32 lmrButtonMap = defaultValue("LmrTapButtonMap", static_cast<quint32>(m_defaultTapButtonMap));
        return lmrButtonMap == LIBINPUT_CONFIG_TAP_MAP_LMR;
    }

    void setLmrTapButtonMap(bool set);
    bool lmrTapButtonMap() const
    {
        return m_tapButtonMap & LIBINPUT_CONFIG_TAP_MAP_LMR;
    }

    quint32 defaultScrollButton() const
    {
        return m_defaultScrollButton;
    }
    bool isMiddleEmulation() const
    {
        return m_middleEmulation;
    }
    void setMiddleEmulation(bool set);
    bool isNaturalScroll() const
    {
        return m_naturalScroll;
    }
    void setNaturalScroll(bool set);
    void setScrollMethod(bool set, enum libinput_config_scroll_method method);
    bool isScrollTwoFinger() const
    {
        return m_scrollMethod & LIBINPUT_CONFIG_SCROLL_2FG;
    }
    void setScrollTwoFinger(bool set)
    {
        setScrollMethod(set, LIBINPUT_CONFIG_SCROLL_2FG);
    }
    bool isScrollEdge() const
    {
        return m_scrollMethod & LIBINPUT_CONFIG_SCROLL_EDGE;
    }
    void setScrollEdge(bool set)
    {
        setScrollMethod(set, LIBINPUT_CONFIG_SCROLL_EDGE);
    }
    bool isScrollOnButtonDown() const
    {
        return m_scrollMethod & LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
    }
    void setScrollOnButtonDown(bool set)
    {
        setScrollMethod(set, LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN);
    }
    void activateScrollMethodFromInt(quint32 method)
    {
        setScrollMethod(true, (libinput_config_scroll_method)method);
    }
    quint32 scrollButton() const
    {
        return m_scrollButton;
    }
    void setScrollButton(quint32 button);

    qreal scrollFactorDefault() const
    {
        return defaultValue("ScrollFactor", 1.0);
    }
    qreal scrollFactor() const
    {
        return m_scrollFactor;
    }
    void setScrollFactor(qreal factor);

    void setDisableWhileTyping(bool set);
    bool isDisableWhileTyping() const
    {
        return m_disableWhileTyping;
    }
    bool isLeftHanded() const
    {
        return m_leftHanded;
    }
    /**
     * Sets the Device to left handed mode if @p set is @c true.
     * If @p set is @c false the device is set to right handed mode
     */
    void setLeftHanded(bool set);

    QString defaultCalibrationMatrix() const
    {
        auto list = defaultValue("CalibrationMatrix", QList<float>{});
        if (list.size() == 16) {
            return serializeMatrix(QMatrix4x4{list.constData()});
        }

        return serializeMatrix(m_defaultCalibrationMatrix);
    }
    QMatrix4x4 calibrationMatrix() const
    {
        return m_calibrationMatrix;
    }
    void setCalibrationMatrix(const QString &value);
    QString serializedCalibrationMatrix() const
    {
        return serializeMatrix(m_calibrationMatrix);
    }

    static QString serializeMatrix(const QMatrix4x4 &matrix);
    static QMatrix4x4 deserializeMatrix(const QString &matrix);

    QString defaultPressureCurve() const;
    QEasingCurve pressureCurve() const;
    QString serializedPressureCurve() const;
    void setPressureCurve(const QString &curve);

    static QString serializePressureCurve(const QEasingCurve &curve);
    static QEasingCurve deserializePressureCurve(const QString &curve);

    Qt::ScreenOrientation defaultOrientation() const
    {
        quint32 orientation = defaultValue("Orientation", static_cast<quint32>(Qt::PrimaryOrientation));
        return static_cast<Qt::ScreenOrientation>(orientation);
    }
    Qt::ScreenOrientation orientation() const
    {
        return m_orientation;
    }
    void setOrientation(Qt::ScreenOrientation orientation);
    void setOrientationDBus(int orientation)
    {
        setOrientation(Qt::ScreenOrientation(orientation));
    }

    qreal defaultPointerAcceleration() const
    {
        return m_defaultPointerAcceleration;
    }
    qreal pointerAcceleration() const
    {
        return m_pointerAcceleration;
    }
    /**
     * @param acceleration mapped to range [-1,1] with -1 being the slowest, 1 being the fastest supported acceleration.
     */
    void setPointerAcceleration(qreal acceleration);
    void setPointerAccelerationFromString(const QString &acceleration)
    {
        setPointerAcceleration(acceleration.toDouble());
    }
    QString defaultPointerAccelerationToString() const
    {
        return QString::number(m_pointerAcceleration, 'f', 3);
    }
    bool supportsPointerAccelerationProfileFlat() const
    {
        return (m_supportedPointerAccelerationProfiles & LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT);
    }
    bool supportsPointerAccelerationProfileAdaptive() const
    {
        return (m_supportedPointerAccelerationProfiles & LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE);
    }
    bool defaultPointerAccelerationProfileFlat() const
    {
        return (m_defaultPointerAccelerationProfile & LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT);
    }
    bool defaultPointerAccelerationProfileAdaptive() const
    {
        return (m_defaultPointerAccelerationProfile & LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE);
    }
    bool pointerAccelerationProfileFlat() const
    {
        return (m_pointerAccelerationProfile & LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT);
    }
    bool pointerAccelerationProfileAdaptive() const
    {
        return (m_pointerAccelerationProfile & LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE);
    }
    void setPointerAccelerationProfile(bool set, enum libinput_config_accel_profile profile);
    void setPointerAccelerationProfileFlat(bool set)
    {
        setPointerAccelerationProfile(set, LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT);
    }
    void setPointerAccelerationProfileAdaptive(bool set)
    {
        setPointerAccelerationProfile(set, LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE);
    }
    void setPointerAccelerationProfileFromInt(quint32 profile)
    {
        setPointerAccelerationProfile(true, (libinput_config_accel_profile)profile);
    }
    quint32 defaultPointerAccelerationProfileToInt() const
    {
        return defaultValue("PointerAccelerationProfile", static_cast<quint32>(m_defaultPointerAccelerationProfile));
    }
    bool supportsClickMethodAreas() const
    {
        return (m_supportedClickMethods & LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS);
    }
    bool defaultClickMethodAreas() const
    {
        return (defaultClickMethod() == LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS);
    }
    bool isClickMethodAreas() const
    {
        return (m_clickMethod == LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS);
    }
    bool supportsClickMethodClickfinger() const
    {
        return (m_supportedClickMethods & LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER);
    }
    bool defaultClickMethodClickfinger() const
    {
        return (defaultClickMethod() == LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER);
    }
    bool isClickMethodClickfinger() const
    {
        return (m_clickMethod == LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER);
    }
    void setClickMethod(bool set, enum libinput_config_click_method method);
    void setClickMethodAreas(bool set)
    {
        setClickMethod(set, LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS);
    }
    void setClickMethodClickfinger(bool set)
    {
        setClickMethod(set, LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER);
    }
    void setClickMethodFromInt(quint32 method)
    {
        setClickMethod(true, (libinput_config_click_method)method);
    }
    libinput_config_click_method defaultClickMethod() const
    {
        return static_cast<libinput_config_click_method>(defaultClickMethodToInt());
    }
    quint32 defaultClickMethodToInt() const
    {
        return defaultValue("ClickMethod", static_cast<quint32>(m_defaultClickMethod));
    }

    bool isEnabled() const override
    {
        return m_enabled;
    }
    void setEnabled(bool enabled) override;

    bool isEnabledByDefault() const
    {
        return defaultValue("Enabled", true);
    }
    bool disableEventsOnExternalMouseEnabledByDefault() const
    {
        return defaultValue("DisableEventsOnExternalMouse", m_disableEventsOnExternalMouseEnabledByDefault);
    }
    bool isDisableEventsOnExternalMouse() const
    {
        return m_disableEventsOnExternalMouse;
    }
    void setDisableEventsOnExternalMouse(bool set);

    libinput_device *device() const
    {
        return m_device;
    }

    /**
     * Sets the @p config to load the Device configuration from and to store each
     * successful Device configuration.
     */
    void setConfig(const KConfigGroup &config)
    {
        m_config = config;
    }

    void setDefaultConfig(const KConfigGroup &config)
    {
        m_defaultConfig = config;
    }

    /**
     * Used to deserialize monitor data from KConfig when initializing a device
     */
    void setOutputName(const QString &uuid) override;
    QString defaultOutputName() const
    {
        return {};
    }

    /**
     * Loads the configuration and applies it to the Device
     */
    void loadConfiguration();

    bool isSwitch() const
    {
        return m_switch;
    }

    bool isLidSwitch() const override
    {
        return m_lidSwitch;
    }

    bool isTabletModeSwitch() const override
    {
        return m_tabletSwitch;
    }

    int tabletPadButtonCount() const override;
    int tabletPadDialCount() const override;
    int tabletPadRingCount() const override;
    int tabletPadStripCount() const override;
    QList<InputDeviceTabletPadModeGroup> modeGroups() const override;

    LogicalOutput *output() const;
    void setOutput(LogicalOutput *output);

    LEDs leds() const override;
    void setLeds(LEDs leds) override;

    QRectF defaultOutputArea() const;
    bool supportsOutputArea() const;
    QRectF outputArea() const;
    void setOutputArea(const QRectF &outputArea);

    bool defaultMapToWorkspace() const
    {
        return defaultValue("MapToWorkspace", false);
    }

    bool isMapToWorkspace() const
    {
        return m_mapToWorkspace;
    }

    void setMapToWorkspace(bool mapToWorkspace);

    QString deviceGroupId() const
    {
        return m_deviceGroupId;
    }

    bool supportsPressureRange() const;
    void setSupportsPressureRange(bool supported);
    double pressureRangeMin() const;
    void setPressureRangeMin(double value);
    double pressureRangeMax() const;
    void setPressureRangeMax(double value);
    double defaultPressureRangeMin() const;
    double defaultPressureRangeMax() const;

    bool supportsInputArea() const;
    QRectF inputArea() const;
    void setInputArea(const QRectF &inputArea);
    QRectF defaultInputArea() const;

    bool tabletToolIsRelative() const override
    {
        return m_tabletToolIsRelative;
    }

    void setTabletToolRelative(bool relative);

    bool defaultTabletToolIsRelative() const
    {
        return defaultValue("TabletToolRelativeMode", false);
    }

    QList<unsigned int> numModes() const;
    QList<unsigned int> currentModes() const;

    bool supportsRotation() const;
    uint32_t rotation() const;
    void setRotation(uint32_t degrees_cw);
    uint32_t defaultRotation() const;
    bool isVirtual() const;

    /**
     * Gets the Device for @p native. @c null if there is no Device for @p native.
     */
    static Device *get(libinput_device *native);

Q_SIGNALS:
    void tapButtonMapChanged();
    void calibrationMatrixChanged();
    void orientationChanged();
    void outputNameChanged();
    void leftHandedChanged();
    void disableWhileTypingChanged();
    void pointerAccelerationChanged();
    void pointerAccelerationProfileChanged();
    void enabledChanged();
    void disableEventsOnExternalMouseChanged();
    void tapToClickChanged();
    void tapAndDragChanged();
    void tapDragLockChanged();
    void middleEmulationChanged();
    void naturalScrollChanged();
    void scrollMethodChanged();
    void scrollButtonChanged();
    void scrollFactorChanged();
    void clickMethodChanged();
    void outputAreaChanged();
    void mapToWorkspaceChanged();
    void pressureCurveChanged();
    void supportsPressureRangeChanged();
    void pressureRangeMinChanged();
    void pressureRangeMaxChanged();
    void inputAreaChanged();
    void tabletToolRelativeChanged();
    void rotationChanged();
    void currentModesChanged();

private:
    template<typename T>
    void writeEntry(const ConfigKey &key, const T &value);

    template<typename T>
    T defaultValue(const char *key, const T &fallback) const
    {
        if (m_defaultConfig.isValid() && m_defaultConfig.hasKey(key)) {
            return m_defaultConfig.readEntry(key, fallback);
        }

        return fallback;
    }

    libinput_device *m_device;
    bool m_keyboard;
    bool m_alphaNumericKeyboard = false;
    bool m_pointer;
    bool m_touch;
    bool m_tabletTool;
    bool m_tabletPad;
    bool m_supportsGesture;
    bool m_switch = false;
    bool m_lidSwitch = false;
    bool m_tabletSwitch = false;
    bool m_touchpad = false;
    QString m_name;
    QString m_sysName;
    QString m_sysPath;
    QString m_outputName;
    QSizeF m_size;
    quint32 m_product;
    quint32 m_vendor;
    quint32 m_busType;
    Qt::MouseButtons m_supportedButtons = Qt::NoButton;
    int m_tapFingerCount;
    enum libinput_config_tap_button_map m_defaultTapButtonMap;
    enum libinput_config_tap_button_map m_tapButtonMap;
    bool m_tapToClickEnabledByDefault;
    bool m_tapToClick;
    bool m_tapAndDragEnabledByDefault;
    bool m_tapAndDrag;
    bool m_tapDragLockEnabledByDefault;
    bool m_tapDragLock;
    bool m_supportsDisableWhileTyping;
    bool m_supportsPointerAcceleration;
    bool m_supportsLeftHanded;
    bool m_supportsCalibrationMatrix;
    bool m_supportsDisableEvents;
    bool m_supportsDisableEventsOnExternalMouse;
    bool m_supportsMiddleEmulation;
    bool m_supportsNaturalScroll;
    quint32 m_supportedScrollMethods;
    bool m_supportsScrollEdge;
    bool m_supportsScrollOnButtonDown;
    bool m_leftHandedEnabledByDefault;
    bool m_middleEmulationEnabledByDefault;
    bool m_naturalScrollEnabledByDefault;
    enum libinput_config_scroll_method m_defaultScrollMethod;
    quint32 m_defaultScrollButton;
    bool m_disableWhileTypingEnabledByDefault;
    bool m_disableWhileTyping;
    bool m_middleEmulation;
    bool m_leftHanded;
    bool m_naturalScroll;
    enum libinput_config_scroll_method m_scrollMethod;
    quint32 m_scrollButton;
    qreal m_defaultPointerAcceleration;
    qreal m_pointerAcceleration;
    qreal m_scrollFactor;
    quint32 m_supportedPointerAccelerationProfiles;
    enum libinput_config_accel_profile m_defaultPointerAccelerationProfile;
    enum libinput_config_accel_profile m_pointerAccelerationProfile;
    bool m_enabled;
    bool m_disableEventsOnExternalMouseEnabledByDefault;
    bool m_disableEventsOnExternalMouse;

    KConfigGroup m_config;
    KConfigGroup m_defaultConfig;
    bool m_loading = false;

    QPointer<LogicalOutput> m_output;
    Qt::ScreenOrientation m_orientation = Qt::PrimaryOrientation;
    QMatrix4x4 m_defaultCalibrationMatrix;
    QMatrix4x4 m_calibrationMatrix;
    QEasingCurve m_pressureCurve;
    quint32 m_supportedClickMethods;
    enum libinput_config_click_method m_defaultClickMethod;
    enum libinput_config_click_method m_clickMethod;

    LEDs m_leds;
    QRectF m_outputArea;
    bool m_mapToWorkspace = false;
    QString m_deviceGroupId;

    bool m_supportsPressureRange;
    double m_pressureRangeMin;
    double m_pressureRangeMax;
    double m_defaultPressureRangeMin;
    double m_defaultPressureRangeMax;

    QRectF m_inputArea;
    bool m_tabletToolIsRelative = false;
    QList<unsigned int> m_currentModes;
    bool m_isVirtual = false;
};

}
}

Q_DECLARE_METATYPE(KWin::LibInput::Device *)
