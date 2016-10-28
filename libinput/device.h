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

#include <QObject>
#include <QSizeF>
#include <QVector>

struct libinput_device;

namespace KWin
{
namespace LibInput
{

class Device : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.InputDevice")
    Q_PROPERTY(bool keyboard READ isKeyboard CONSTANT)
    Q_PROPERTY(bool alphaNumericKeyboard READ isAlphaNumericKeyboard CONSTANT)
    Q_PROPERTY(bool pointer READ isPointer CONSTANT)
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
    Q_PROPERTY(Qt::MouseButtons supportedButtons READ supportedButtons CONSTANT)
    Q_PROPERTY(int tapFingerCount READ tapFingerCount CONSTANT)
    Q_PROPERTY(bool tapToClickEnabledByDefault READ tapToClickEnabledByDefault CONSTANT)
    Q_PROPERTY(bool supportsDisableWhileTyping READ supportsDisableWhileTyping CONSTANT)
    Q_PROPERTY(bool supportsPointerAcceleration READ supportsPointerAcceleration CONSTANT)
    Q_PROPERTY(bool supportsLeftHanded READ supportsLeftHanded CONSTANT)
    Q_PROPERTY(bool supportsCalibrationMatrix READ supportsCalibrationMatrix CONSTANT)
    Q_PROPERTY(bool supportsDisableEvents READ supportsDisableEvents CONSTANT)
    Q_PROPERTY(bool supportsDisableEventsOnExternalMouse READ supportsDisableEventsOnExternalMouse CONSTANT)
    Q_PROPERTY(bool supportsMiddleEmulation READ supportsMiddleEmulation CONSTANT)
    Q_PROPERTY(bool supportsNaturalScroll READ supportsNaturalScroll CONSTANT)
    Q_PROPERTY(bool supportsScrollTwoFinger READ supportsScrollTwoFinger CONSTANT)
    Q_PROPERTY(bool supportsScrollEdge READ supportsScrollEdge CONSTANT)
    Q_PROPERTY(bool supportsScrollOnButtonDown READ supportsScrollOnButtonDown CONSTANT)
    Q_PROPERTY(bool middleEmulationEnabledByDefault READ middleEmulationEnabledByDefault CONSTANT)
    Q_PROPERTY(bool naturalScrollEnabledByDefault READ naturalScrollEnabledByDefault CONSTANT)
    Q_PROPERTY(bool scrollTwoFingerEnabledByDefault READ scrollTwoFingerEnabledByDefault CONSTANT)
    Q_PROPERTY(bool scrollEdgeEnabledByDefault READ scrollEdgeEnabledByDefault CONSTANT)
    Q_PROPERTY(bool scrollOnButtonDownEnabledByDefault READ scrollOnButtonDownEnabledByDefault CONSTANT)
    Q_PROPERTY(quint32 defaultScrollButton READ defaultScrollButton CONSTANT)
    Q_PROPERTY(bool middleEmulation READ isMiddleEmulation WRITE setMiddleEmulation NOTIFY middleEmulationChanged)
    Q_PROPERTY(bool leftHanded READ isLeftHanded WRITE setLeftHanded NOTIFY leftHandedChanged)
    Q_PROPERTY(bool naturalScroll READ isNaturalScroll WRITE setNaturalScroll NOTIFY naturalScrollChanged)
    Q_PROPERTY(bool scrollTwoFinger READ isScrollTwoFinger WRITE setScrollTwoFinger NOTIFY scrollMethodChanged)
    Q_PROPERTY(bool scrollEdge READ isScrollEdge WRITE setScrollEdge NOTIFY scrollMethodChanged)
    Q_PROPERTY(bool scrollOnButtonDown READ isScrollOnButtonDown WRITE setScrollOnButtonDown NOTIFY scrollMethodChanged)
    Q_PROPERTY(quint32 scrollButton READ scrollButton WRITE setScrollButton NOTIFY scrollButtonChanged)
    Q_PROPERTY(qreal pointerAcceleration READ pointerAcceleration WRITE setPointerAcceleration NOTIFY pointerAccelerationChanged)
    Q_PROPERTY(bool tapToClick READ isTapToClick WRITE setTapToClick NOTIFY tapToClickChanged)
    Q_PROPERTY(bool tapAndDragEnabledByDefault READ tapAndDragEnabledByDefault CONSTANT)
    Q_PROPERTY(bool tapAndDrag READ isTapAndDrag WRITE setTapAndDrag NOTIFY tapAndDragChanged)
    Q_PROPERTY(bool tapDragLockEnabledByDefault READ tapDragLockEnabledByDefault CONSTANT)
    Q_PROPERTY(bool tapDragLock READ isTapDragLock WRITE setTapDragLock NOTIFY tapDragLockChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
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
    bool middleEmulationEnabledByDefault() const {
        return m_middleEmulationEnabledByDefault;
    }
    bool naturalScrollEnabledByDefault() const {
        return m_naturalScrollEnabledByDefault;
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
    void setScrollTwoFinger(bool set);
    bool isScrollEdge() const {
        return m_scrollMethod & LIBINPUT_CONFIG_SCROLL_EDGE;
    }
    void setScrollEdge(bool set);
    bool isScrollOnButtonDown() const {
        return m_scrollMethod & LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
    }
    void setScrollOnButtonDown(bool set);
    quint32 scrollButton() const {
        return m_scrollButton;
    }
    void setScrollButton(quint32 button);

    bool isLeftHanded() const {
        return m_leftHanded;
    }
    /**
     * Sets the Device to left handed mode if @p set is @c true.
     * If @p set is @c false the device is set to right handed mode
     **/
    void setLeftHanded(bool set);

    qreal pointerAcceleration() const {
        return m_pointerAcceleration;
    }
    /**
     * @param acceleration mapped to range [-1,1] with -1 being the slowest, 1 being the fastest supported acceleration.
     **/
    void setPointerAcceleration(qreal acceleration);

    bool isEnabled() const {
        return m_enabled;
    }
    void setEnabled(bool enabled);

    libinput_device *device() const {
        return m_device;
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
    void leftHandedChanged();
    void pointerAccelerationChanged();
    void enabledChanged();
    void tapToClickChanged();
    void tapAndDragChanged();
    void tapDragLockChanged();
    void middleEmulationChanged();
    void naturalScrollChanged();
    void scrollMethodChanged();
    void scrollButtonChanged();

private:
    libinput_device *m_device;
    bool m_keyboard;
    bool m_alphaNumericKeyboard = false;
    bool m_pointer;
    bool m_touch;
    bool m_tabletTool;
    bool m_tabletPad;
    bool m_supportsGesture;
    QString m_name;
    QString m_sysName;
    QString m_outputName;
    QSizeF m_size;
    quint32 m_product;
    quint32 m_vendor;
    Qt::MouseButtons m_supportedButtons = Qt::NoButton;
    int m_tapFingerCount;
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
    bool m_middleEmulationEnabledByDefault;
    bool m_naturalScrollEnabledByDefault;
    enum libinput_config_scroll_method m_defaultScrollMethod;
    quint32 m_defaultScrollButton;
    bool m_middleEmulation;
    bool m_leftHanded;
    bool m_naturalScroll;
    enum libinput_config_scroll_method m_scrollMethod;
    quint32 m_scrollButton;
    qreal m_pointerAcceleration;
    bool m_enabled;

    static QVector<Device*> s_devices;
};

}
}

Q_DECLARE_METATYPE(KWin::LibInput::Device*)

#endif
