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
#ifndef KWIN_LIBINPUT_DEVICE_H
#define KWIN_LIBINPUT_DEVICE_H

#include <libinput.h>

#include <KConfigGroup>

#include <QObject>
#include <QMatrix4x4>
#include <QSizeF>
#include <QVector>

struct libinput_device;

namespace KWin
{
namespace LibInput
{
enum class ConfigKey;

class Device : public QObject
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
    Q_PROPERTY(QString outputName READ outputName CONSTANT)
    Q_PROPERTY(QSizeF size READ size CONSTANT)
    Q_PROPERTY(quint32 product READ product CONSTANT)
    Q_PROPERTY(quint32 vendor READ vendor CONSTANT)
    Q_PROPERTY(bool supportsDisableEvents READ supportsDisableEvents CONSTANT)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    //
    // advanced
    Q_PROPERTY(Qt::MouseButtons supportedButtons READ supportedButtons CONSTANT)
    Q_PROPERTY(bool supportsCalibrationMatrix READ supportsCalibrationMatrix CONSTANT)

    Q_PROPERTY(bool supportsLeftHanded READ supportsLeftHanded CONSTANT)
    Q_PROPERTY(bool leftHandedEnabledByDefault READ leftHandedEnabledByDefault CONSTANT)
    Q_PROPERTY(bool leftHanded READ isLeftHanded WRITE setLeftHanded NOTIFY leftHandedChanged)

    Q_PROPERTY(bool supportsDisableEventsOnExternalMouse READ supportsDisableEventsOnExternalMouse CONSTANT)

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

    // switches
    Q_PROPERTY(bool switchDevice READ isSwitch CONSTANT)
    Q_PROPERTY(bool lidSwitch READ isLidSwitch CONSTANT)
    Q_PROPERTY(bool tabletModeSwitch READ isTabletModeSwitch CONSTANT)


public:
    explicit Device(libinput_device *device, QObject *parent = nullptr);
    virtual ~Device();

    bool isKeyboard() const {
        return m_keyboard;
    }
    bool isAlphaNumericKeyboard() const {
        return m_alphaNumericKeyboard;
    }
    bool isPointer() const {
        return m_pointer;
    }
    bool isTouchpad() const{
        return m_pointer &&
                // ignore all combined devices. E.g. a touchpad on a keyboard we don't want to toggle
                // as that would result in the keyboard going off as well
                !(m_keyboard || m_touch || m_tabletPad || m_tabletTool) &&
                // is this a touch pad? We don't really know, let's do some assumptions
                (m_tapFingerCount > 0  || m_supportsDisableWhileTyping || m_supportsDisableEventsOnExternalMouse);
    }
    bool isTouch() const {
        return m_touch;
    }
    bool isTabletTool() const {
        return m_tabletTool;
    }
    bool isTabletPad() const {
        return m_tabletPad;
    }
    bool supportsGesture() const {
        return m_supportsGesture;
    }
    QString name() const {
        return m_name;
    }
    QString sysName() const {
        return m_sysName;
    }
    QString outputName() const {
        return m_outputName;
    }
    QSizeF size() const {
        return m_size;
    }
    quint32 product() const {
        return m_product;
    }
    quint32 vendor() const {
        return m_vendor;
    }
    Qt::MouseButtons supportedButtons() const {
        return m_supportedButtons;
    }
    int tapFingerCount() const {
        return m_tapFingerCount;
    }
    bool tapToClickEnabledByDefault() const {
        return m_tapToClickEnabledByDefault;
    }
    bool isTapToClick() const {
        return m_tapToClick;
    }
    /**
     * Set the Device to tap to click if @p set is @c true.
     **/
    void setTapToClick(bool set);
    bool tapAndDragEnabledByDefault() const {
        return m_tapAndDragEnabledByDefault;
    }
    bool isTapAndDrag() const {
        return m_tapAndDrag;
    }
    void setTapAndDrag(bool set);
    bool tapDragLockEnabledByDefault() const {
        return m_tapDragLockEnabledByDefault;
    }
    bool isTapDragLock() const {
        return m_tapDragLock;
    }
    void setTapDragLock(bool set);
    bool supportsDisableWhileTyping() const {
        return m_supportsDisableWhileTyping;
    }
    bool disableWhileTypingEnabledByDefault() const {
        return m_disableWhileTypingEnabledByDefault;
    }
    bool supportsPointerAcceleration() const {
        return m_supportsPointerAcceleration;
    }
    bool supportsLeftHanded() const {
        return m_supportsLeftHanded;
    }
    bool supportsCalibrationMatrix() const {
        return m_supportsCalibrationMatrix;
    }
    bool supportsDisableEvents() const {
        return m_supportsDisableEvents;
    }
    bool supportsDisableEventsOnExternalMouse() const {
        return m_supportsDisableEventsOnExternalMouse;
    }
    bool supportsMiddleEmulation() const {
        return m_supportsMiddleEmulation;
    }
    bool supportsNaturalScroll() const {
        return m_supportsNaturalScroll;
    }
    bool supportsScrollTwoFinger() const {
        return (m_supportedScrollMethods & LIBINPUT_CONFIG_SCROLL_2FG);
    }
    bool supportsScrollEdge() const {
        return (m_supportedScrollMethods & LIBINPUT_CONFIG_SCROLL_EDGE);
    }
    bool supportsScrollOnButtonDown() const {
        return (m_supportedScrollMethods & LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN);
    }
    bool leftHandedEnabledByDefault() const {
        return m_leftHandedEnabledByDefault;
    }
    bool middleEmulationEnabledByDefault() const {
        return m_middleEmulationEnabledByDefault;
    }
    bool naturalScrollEnabledByDefault() const {
        return m_naturalScrollEnabledByDefault;
    }
    enum libinput_config_scroll_method defaultScrollMethod() const {
        return m_defaultScrollMethod;
    }
    quint32 defaultScrollMethodToInt() const {
        return (quint32) m_defaultScrollMethod;
    }
    bool scrollTwoFingerEnabledByDefault() const {
        return m_defaultScrollMethod == LIBINPUT_CONFIG_SCROLL_2FG;
    }
    bool scrollEdgeEnabledByDefault() const {
        return m_defaultScrollMethod == LIBINPUT_CONFIG_SCROLL_EDGE;
    }
    bool scrollOnButtonDownEnabledByDefault() const {
        return m_defaultScrollMethod == LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
    }
    bool supportsLmrTapButtonMap() const {
        return m_tapFingerCount > 1;
    }
    bool lmrTapButtonMapEnabledByDefault() const {
        return m_defaultTapButtonMap == LIBINPUT_CONFIG_TAP_MAP_LMR;
    }

    void setLmrTapButtonMap(bool set);
    bool lmrTapButtonMap() const {
        return m_tapButtonMap & LIBINPUT_CONFIG_TAP_MAP_LMR;
    }

    quint32 defaultScrollButton() const {
        return m_defaultScrollButton;
    }
    bool isMiddleEmulation() const {
        return m_middleEmulation;
    }
    void setMiddleEmulation(bool set);
    bool isNaturalScroll() const {
        return m_naturalScroll;
    }
    void setNaturalScroll(bool set);
    void setScrollMethod(bool set, enum libinput_config_scroll_method method);
    bool isScrollTwoFinger() const {
        return m_scrollMethod & LIBINPUT_CONFIG_SCROLL_2FG;
    }
    void setScrollTwoFinger(bool set) {
        setScrollMethod(set, LIBINPUT_CONFIG_SCROLL_2FG);
    }
    bool isScrollEdge() const {
        return m_scrollMethod & LIBINPUT_CONFIG_SCROLL_EDGE;
    }
    void setScrollEdge(bool set) {
        setScrollMethod(set, LIBINPUT_CONFIG_SCROLL_EDGE);
    }
    bool isScrollOnButtonDown() const {
        return m_scrollMethod & LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
    }
    void setScrollOnButtonDown(bool set) {
        setScrollMethod(set, LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN);
    }
    void activateScrollMethodFromInt(quint32 method) {
        setScrollMethod(true, (libinput_config_scroll_method) method);
    }
    quint32 scrollButton() const {
        return m_scrollButton;
    }
    void setScrollButton(quint32 button);

    void setDisableWhileTyping(bool set);
    bool isDisableWhileTyping() const {
        return m_disableWhileTyping;
    }
    bool isLeftHanded() const {
        return m_leftHanded;
    }
    /**
     * Sets the Device to left handed mode if @p set is @c true.
     * If @p set is @c false the device is set to right handed mode
     **/
    void setLeftHanded(bool set);

    qreal defaultPointerAcceleration() const {
        return m_defaultPointerAcceleration;
    }
    qreal pointerAcceleration() const {
        return m_pointerAcceleration;
    }
    /**
     * @param acceleration mapped to range [-1,1] with -1 being the slowest, 1 being the fastest supported acceleration.
     **/
    void setPointerAcceleration(qreal acceleration);
    void setPointerAccelerationFromString(QString acceleration) {
        setPointerAcceleration(acceleration.toDouble());
    }
    QString defaultPointerAccelerationToString() const {
        return QString::number(m_pointerAcceleration, 'f', 3);
    }
    bool supportsPointerAccelerationProfileFlat() const {
        return (m_supportedPointerAccelerationProfiles & LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT);
    }
    bool supportsPointerAccelerationProfileAdaptive() const {
        return (m_supportedPointerAccelerationProfiles & LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE);
    }
    bool defaultPointerAccelerationProfileFlat() const {
        return (m_defaultPointerAccelerationProfile & LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT);
    }
    bool defaultPointerAccelerationProfileAdaptive() const {
        return (m_defaultPointerAccelerationProfile & LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE);
    }
    bool pointerAccelerationProfileFlat() const {
        return (m_pointerAccelerationProfile & LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT);
    }
    bool pointerAccelerationProfileAdaptive() const {
        return (m_pointerAccelerationProfile & LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE);
    }
    void setPointerAccelerationProfile(bool set, enum  libinput_config_accel_profile profile);
    void setPointerAccelerationProfileFlat(bool set) {
        setPointerAccelerationProfile(set, LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT);
    }
    void setPointerAccelerationProfileAdaptive(bool set) {
        setPointerAccelerationProfile(set, LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE);
    }
    void setPointerAccelerationProfileFromInt(quint32 profile) {
        setPointerAccelerationProfile(true, (libinput_config_accel_profile) profile);
    }
    quint32 defaultPointerAccelerationProfileToInt() const {
        return (quint32) m_defaultPointerAccelerationProfile;
    }
    bool isEnabled() const {
        return m_enabled;
    }
    void setEnabled(bool enabled);

    libinput_device *device() const {
        return m_device;
    }

    /**
     * Sets the @p config to load the Device configuration from and to store each
     * successful Device configuration.
     **/
    void setConfig(const KConfigGroup &config) {
        m_config = config;
    }

    /**
     * The id of the screen in KWin identifiers. Set from KWin through @link setScreenId.
     **/
    int screenId() const {
        return m_screenId;
    }

    /**
     * Sets the KWin screen id for the device
     **/
    void setScreenId(int screenId) {
        m_screenId = screenId;
    }

    void setOrientation(Qt::ScreenOrientation orientation);

    /**
     * Loads the configuration and applies it to the Device
     **/
    void loadConfiguration();

    bool isSwitch() const {
        return m_switch;
    }

    bool isLidSwitch() const {
        return m_lidSwitch;
    }

    bool isTabletModeSwitch() const {
        return m_tabletSwitch;
    }

    /**
     * All created Devices
     **/
    static QVector<Device*> devices() {
        return s_devices;
    }
    /**
     * Gets the Device for @p native. @c null if there is no Device for @p native.
     **/
    static Device *getDevice(libinput_device *native);

Q_SIGNALS:
    void tapButtonMapChanged();
    void leftHandedChanged();
    void disableWhileTypingChanged();
    void pointerAccelerationChanged();
    void pointerAccelerationProfileChanged();
    void enabledChanged();
    void tapToClickChanged();
    void tapAndDragChanged();
    void tapDragLockChanged();
    void middleEmulationChanged();
    void naturalScrollChanged();
    void scrollMethodChanged();
    void scrollButtonChanged();

private:
    template <typename T>
    void writeEntry(const ConfigKey &key, const T &value);
    template <typename T, typename Setter>
    void readEntry(const QByteArray &key, const Setter &s, const T &defaultValue = T());
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
    QString m_name;
    QString m_sysName;
    QString m_outputName;
    QSizeF m_size;
    quint32 m_product;
    quint32 m_vendor;
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
    quint32 m_supportedPointerAccelerationProfiles;
    enum libinput_config_accel_profile m_defaultPointerAccelerationProfile;
    enum libinput_config_accel_profile m_pointerAccelerationProfile;
    bool m_enabled;

    KConfigGroup m_config;
    bool m_loading = false;

    int m_screenId = 0;
    Qt::ScreenOrientation m_orientation = Qt::PrimaryOrientation;
    QMatrix4x4 m_defaultCalibrationMatrix;

    static QVector<Device*> s_devices;
};

}
}

Q_DECLARE_METATYPE(KWin::LibInput::Device*)

#endif
