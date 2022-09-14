/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <config-kwin.h>

#include "backends/libinput/device.h"
#include "mock_libinput.h"

#include <KSharedConfig>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QtTest>

#include <linux/input.h>

using namespace KWin::LibInput;

class TestLibinputDevice : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testDeviceType_data();
    void testDeviceType();
    void testGestureSupport_data();
    void testGestureSupport();
    void testNames_data();
    void testNames();
    void testProduct();
    void testVendor();
    void testTapFingerCount();
    void testSize_data();
    void testSize();
    void testDefaultPointerAcceleration_data();
    void testDefaultPointerAcceleration();
    void testDefaultPointerAccelerationProfileFlat_data();
    void testDefaultPointerAccelerationProfileFlat();
    void testDefaultPointerAccelerationProfileAdaptive_data();
    void testDefaultPointerAccelerationProfileAdaptive();
    void testDefaultClickMethodAreas_data();
    void testDefaultClickMethodAreas();
    void testDefaultClickMethodClickfinger_data();
    void testDefaultClickMethodClickfinger();
    void testLeftHandedEnabledByDefault_data();
    void testLeftHandedEnabledByDefault();
    void testTapEnabledByDefault_data();
    void testTapEnabledByDefault();
    void testMiddleEmulationEnabledByDefault_data();
    void testMiddleEmulationEnabledByDefault();
    void testNaturalScrollEnabledByDefault_data();
    void testNaturalScrollEnabledByDefault();
    void testScrollTwoFingerEnabledByDefault_data();
    void testScrollTwoFingerEnabledByDefault();
    void testScrollEdgeEnabledByDefault_data();
    void testScrollEdgeEnabledByDefault();
    void testScrollOnButtonDownEnabledByDefault_data();
    void testScrollOnButtonDownEnabledByDefault();
    void testDisableWhileTypingEnabledByDefault_data();
    void testDisableWhileTypingEnabledByDefault();
    void testLmrTapButtonMapEnabledByDefault_data();
    void testLmrTapButtonMapEnabledByDefault();
    void testSupportsDisableWhileTyping_data();
    void testSupportsDisableWhileTyping();
    void testSupportsPointerAcceleration_data();
    void testSupportsPointerAcceleration();
    void testSupportsLeftHanded_data();
    void testSupportsLeftHanded();
    void testSupportsCalibrationMatrix_data();
    void testSupportsCalibrationMatrix();
    void testSupportsDisableEvents_data();
    void testSupportsDisableEvents();
    void testSupportsDisableEventsOnExternalMouse_data();
    void testSupportsDisableEventsOnExternalMouse();
    void testSupportsMiddleEmulation_data();
    void testSupportsMiddleEmulation();
    void testSupportsNaturalScroll_data();
    void testSupportsNaturalScroll();
    void testSupportsScrollTwoFinger_data();
    void testSupportsScrollTwoFinger();
    void testSupportsScrollEdge_data();
    void testSupportsScrollEdge();
    void testSupportsScrollOnButtonDown_data();
    void testSupportsScrollOnButtonDown();
    void testDefaultScrollButton_data();
    void testDefaultScrollButton();
    void testPointerAcceleration_data();
    void testPointerAcceleration();
    void testLeftHanded_data();
    void testLeftHanded();
    void testSupportedButtons_data();
    void testSupportedButtons();
    void testAlphaNumericKeyboard_data();
    void testAlphaNumericKeyboard();
    void testEnabled_data();
    void testEnabled();
    void testTapToClick_data();
    void testTapToClick();
    void testTapAndDragEnabledByDefault_data();
    void testTapAndDragEnabledByDefault();
    void testTapAndDrag_data();
    void testTapAndDrag();
    void testTapDragLockEnabledByDefault_data();
    void testTapDragLockEnabledByDefault();
    void testTapDragLock_data();
    void testTapDragLock();
    void testMiddleEmulation_data();
    void testMiddleEmulation();
    void testNaturalScroll_data();
    void testNaturalScroll();
    void testScrollFactor();
    void testScrollTwoFinger_data();
    void testScrollTwoFinger();
    void testScrollEdge_data();
    void testScrollEdge();
    void testScrollButtonDown_data();
    void testScrollButtonDown();
    void testScrollButton_data();
    void testScrollButton();
    void testDisableWhileTyping_data();
    void testDisableWhileTyping();
    void testLmrTapButtonMap_data();
    void testLmrTapButtonMap();
    void testLoadEnabled_data();
    void testLoadEnabled();
    void testLoadPointerAcceleration_data();
    void testLoadPointerAcceleration();
    void testLoadPointerAccelerationProfile_data();
    void testLoadPointerAccelerationProfile();
    void testLoadClickMethod_data();
    void testLoadClickMethod();
    void testLoadTapToClick_data();
    void testLoadTapToClick();
    void testLoadTapAndDrag_data();
    void testLoadTapAndDrag();
    void testLoadTapDragLock_data();
    void testLoadTapDragLock();
    void testLoadMiddleButtonEmulation_data();
    void testLoadMiddleButtonEmulation();
    void testLoadNaturalScroll_data();
    void testLoadNaturalScroll();
    void testLoadScrollMethod_data();
    void testLoadScrollMethod();
    void testLoadScrollButton_data();
    void testLoadScrollButton();
    void testLoadDisableWhileTyping_data();
    void testLoadDisableWhileTyping();
    void testLoadLmrTapButtonMap_data();
    void testLoadLmrTapButtonMap();
    void testLoadLeftHanded_data();
    void testLoadLeftHanded();
    void testOrientation_data();
    void testOrientation();
    void testCalibrationWithDefault();
    void testSwitch_data();
    void testSwitch();
};

namespace
{
template<typename T>
T dbusProperty(const QString &name, const char *property)
{
    QDBusInterface interface {
        QStringLiteral("org.kde.kwin.tests.libinputdevice"),
            QStringLiteral("/org/kde/KWin/InputDevice/") + name,
            QStringLiteral("org.kde.KWin.InputDevice")
    };
    return interface.property(property).value<T>();
}
}

void TestLibinputDevice::initTestCase()
{
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.kwin.tests.libinputdevice"));
}

void TestLibinputDevice::testDeviceType_data()
{
    QTest::addColumn<bool>("keyboard");
    QTest::addColumn<bool>("pointer");
    QTest::addColumn<bool>("touch");
    QTest::addColumn<bool>("tabletTool");
    QTest::addColumn<bool>("switchDevice");

    QTest::newRow("keyboard") << true << false << false << false << false;
    QTest::newRow("pointer") << false << true << false << false << false;
    QTest::newRow("touch") << false << false << true << false << false;
    QTest::newRow("keyboard/pointer") << true << true << false << false << false;
    QTest::newRow("keyboard/touch") << true << false << true << false << false;
    QTest::newRow("pointer/touch") << false << true << true << false << false;
    QTest::newRow("keyboard/pointer/touch") << true << true << true << false << false;
    QTest::newRow("tabletTool") << false << false << false << true << false;
    QTest::newRow("switch") << false << false << false << false << true;
}

void TestLibinputDevice::testDeviceType()
{
    // this test verifies that the device type is recognized correctly
    QFETCH(bool, keyboard);
    QFETCH(bool, pointer);
    QFETCH(bool, touch);
    QFETCH(bool, tabletTool);
    QFETCH(bool, switchDevice);

    libinput_device device;
    device.keyboard = keyboard;
    device.pointer = pointer;
    device.touch = touch;
    device.tabletTool = tabletTool;
    device.switchDevice = switchDevice;

    Device d(&device);
    QCOMPARE(d.isKeyboard(), keyboard);
    QCOMPARE(d.property("keyboard").toBool(), keyboard);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "keyboard"), keyboard);
    QCOMPARE(d.isPointer(), pointer);
    QCOMPARE(d.property("pointer").toBool(), pointer);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "pointer"), pointer);
    QCOMPARE(d.isTouch(), touch);
    QCOMPARE(d.property("touch").toBool(), touch);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "touch"), touch);
    QCOMPARE(d.isTabletPad(), false);
    QCOMPARE(d.property("tabletPad").toBool(), false);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "tabletPad"), false);
    QCOMPARE(d.isTabletTool(), tabletTool);
    QCOMPARE(d.property("tabletTool").toBool(), tabletTool);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "tabletTool"), tabletTool);
    QCOMPARE(d.isSwitch(), switchDevice);
    QCOMPARE(d.property("switchDevice").toBool(), switchDevice);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "switchDevice"), switchDevice);

    QCOMPARE(d.device(), &device);
}

void TestLibinputDevice::testGestureSupport_data()
{
    QTest::addColumn<bool>("supported");

    QTest::newRow("supported") << true;
    QTest::newRow("not supported") << false;
}

void TestLibinputDevice::testGestureSupport()
{
    // this test verifies whether the Device supports gestures
    QFETCH(bool, supported);
    libinput_device device;
    device.gestureSupported = supported;

    Device d(&device);
    QCOMPARE(d.supportsGesture(), supported);
    QCOMPARE(d.property("gestureSupport").toBool(), supported);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "gestureSupport"), supported);
}

void TestLibinputDevice::testNames_data()
{
    QTest::addColumn<QByteArray>("name");
    QTest::addColumn<QByteArray>("sysName");
    QTest::addColumn<QByteArray>("outputName");

    QTest::newRow("empty") << QByteArray() << QByteArrayLiteral("event1") << QByteArray();
    QTest::newRow("set") << QByteArrayLiteral("awesome test device") << QByteArrayLiteral("event0") << QByteArrayLiteral("hdmi0");
}

void TestLibinputDevice::testNames()
{
    // this test verifies the various name properties of the Device
    QFETCH(QByteArray, name);
    QFETCH(QByteArray, sysName);
    QFETCH(QByteArray, outputName);
    libinput_device device;
    device.name = name;
    device.sysName = sysName;
    device.outputName = outputName;

    Device d(&device);
    QCOMPARE(d.name().toUtf8(), name);
    QCOMPARE(d.property("name").toString().toUtf8(), name);
    QCOMPARE(dbusProperty<QString>(d.sysName(), "name"), name);
    QCOMPARE(d.sysName().toUtf8(), sysName);
    QCOMPARE(d.property("sysName").toString().toUtf8(), sysName);
    QCOMPARE(dbusProperty<QString>(d.sysName(), "sysName"), sysName);
    QCOMPARE(d.outputName().toUtf8(), outputName);
    QCOMPARE(d.property("outputName").toString().toUtf8(), outputName);
    QCOMPARE(dbusProperty<QString>(d.sysName(), "outputName"), outputName);
}

void TestLibinputDevice::testProduct()
{
    // this test verifies the product property
    libinput_device device;
    device.product = 100u;
    Device d(&device);
    QCOMPARE(d.product(), 100u);
    QCOMPARE(d.property("product").toUInt(), 100u);
    QCOMPARE(dbusProperty<quint32>(d.sysName(), "product"), 100u);
}

void TestLibinputDevice::testVendor()
{
    // this test verifies the vendor property
    libinput_device device;
    device.vendor = 200u;
    Device d(&device);
    QCOMPARE(d.vendor(), 200u);
    QCOMPARE(d.property("vendor").toUInt(), 200u);
    QCOMPARE(dbusProperty<quint32>(d.sysName(), "vendor"), 200u);
}

void TestLibinputDevice::testTapFingerCount()
{
    // this test verifies the tap finger count property
    libinput_device device;
    device.tapFingerCount = 3;
    Device d(&device);
    QCOMPARE(d.tapFingerCount(), 3);
    QCOMPARE(d.property("tapFingerCount").toInt(), 3);
    QCOMPARE(dbusProperty<int>(d.sysName(), "tapFingerCount"), 3);
}

void TestLibinputDevice::testSize_data()
{
    QTest::addColumn<QSizeF>("setSize");
    QTest::addColumn<int>("returnValue");
    QTest::addColumn<QSizeF>("expectedSize");

    QTest::newRow("10/20") << QSizeF(10.5, 20.2) << 0 << QSizeF(10.5, 20.2);
    QTest::newRow("failure") << QSizeF(10, 20) << 1 << QSizeF();
}

void TestLibinputDevice::testSize()
{
    // this test verifies that getting the size works correctly including failures
    QFETCH(QSizeF, setSize);
    QFETCH(int, returnValue);
    libinput_device device;
    device.deviceSize = setSize;
    device.deviceSizeReturnValue = returnValue;

    Device d(&device);
    QTEST(d.size(), "expectedSize");
    QTEST(d.property("size").toSizeF(), "expectedSize");
    QTEST(dbusProperty<QSizeF>(d.sysName(), "size"), "expectedSize");
}

void TestLibinputDevice::testLeftHandedEnabledByDefault_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testLeftHandedEnabledByDefault()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.leftHandedEnabledByDefault = enabled;

    Device d(&device);
    QCOMPARE(d.leftHandedEnabledByDefault(), enabled);
    QCOMPARE(d.property("leftHandedEnabledByDefault").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "leftHandedEnabledByDefault"), enabled);
}

void TestLibinputDevice::testTapEnabledByDefault_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testTapEnabledByDefault()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.tapEnabledByDefault = enabled;

    Device d(&device);
    QCOMPARE(d.tapToClickEnabledByDefault(), enabled);
    QCOMPARE(d.property("tapToClickEnabledByDefault").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "tapToClickEnabledByDefault"), enabled);
}

void TestLibinputDevice::testMiddleEmulationEnabledByDefault_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testMiddleEmulationEnabledByDefault()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.middleEmulationEnabledByDefault = enabled;

    Device d(&device);
    QCOMPARE(d.middleEmulationEnabledByDefault(), enabled);
    QCOMPARE(d.property("middleEmulationEnabledByDefault").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "middleEmulationEnabledByDefault"), enabled);
}

void TestLibinputDevice::testNaturalScrollEnabledByDefault_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testNaturalScrollEnabledByDefault()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.naturalScrollEnabledByDefault = enabled;

    Device d(&device);
    QCOMPARE(d.naturalScrollEnabledByDefault(), enabled);
    QCOMPARE(d.property("naturalScrollEnabledByDefault").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "naturalScrollEnabledByDefault"), enabled);
}

void TestLibinputDevice::testScrollTwoFingerEnabledByDefault_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testScrollTwoFingerEnabledByDefault()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.defaultScrollMethod = enabled ? LIBINPUT_CONFIG_SCROLL_2FG : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;

    Device d(&device);
    QCOMPARE(d.scrollTwoFingerEnabledByDefault(), enabled);
    QCOMPARE(d.property("scrollTwoFingerEnabledByDefault").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "scrollTwoFingerEnabledByDefault"), enabled);
}

void TestLibinputDevice::testScrollEdgeEnabledByDefault_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testScrollEdgeEnabledByDefault()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.defaultScrollMethod = enabled ? LIBINPUT_CONFIG_SCROLL_EDGE : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;

    Device d(&device);
    QCOMPARE(d.scrollEdgeEnabledByDefault(), enabled);
    QCOMPARE(d.property("scrollEdgeEnabledByDefault").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "scrollEdgeEnabledByDefault"), enabled);
}

void TestLibinputDevice::testDefaultPointerAccelerationProfileFlat_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testDefaultPointerAccelerationProfileFlat()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.defaultPointerAccelerationProfile = enabled ? LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT : LIBINPUT_CONFIG_ACCEL_PROFILE_NONE;

    Device d(&device);
    QCOMPARE(d.defaultPointerAccelerationProfileFlat(), enabled);
    QCOMPARE(d.property("defaultPointerAccelerationProfileFlat").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "defaultPointerAccelerationProfileFlat"), enabled);
}

void TestLibinputDevice::testDefaultPointerAccelerationProfileAdaptive_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testDefaultPointerAccelerationProfileAdaptive()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.defaultPointerAccelerationProfile = enabled ? LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE : LIBINPUT_CONFIG_ACCEL_PROFILE_NONE;

    Device d(&device);
    QCOMPARE(d.defaultPointerAccelerationProfileAdaptive(), enabled);
    QCOMPARE(d.property("defaultPointerAccelerationProfileAdaptive").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "defaultPointerAccelerationProfileAdaptive"), enabled);
}

void TestLibinputDevice::testDefaultClickMethodAreas_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::addRow("enabled") << true;
    QTest::addRow("disabled") << false;
}

void TestLibinputDevice::testDefaultClickMethodAreas()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.defaultClickMethod = enabled ? LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS : LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;

    Device d(&device);
    QCOMPARE(d.defaultClickMethodAreas(), enabled);
    QCOMPARE(d.property("defaultClickMethodAreas").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "defaultClickMethodAreas"), enabled);
}

void TestLibinputDevice::testDefaultClickMethodClickfinger_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::addRow("enabled") << true;
    QTest::addRow("disabled") << false;
}

void TestLibinputDevice::testDefaultClickMethodClickfinger()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.defaultClickMethod = enabled ? LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER : LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;

    Device d(&device);
    QCOMPARE(d.defaultClickMethodClickfinger(), enabled);
    QCOMPARE(d.property("defaultClickMethodClickfinger").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "defaultClickMethodClickfinger"), enabled);
}

void TestLibinputDevice::testScrollOnButtonDownEnabledByDefault_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testScrollOnButtonDownEnabledByDefault()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.defaultScrollMethod = enabled ? LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;

    Device d(&device);
    QCOMPARE(d.scrollOnButtonDownEnabledByDefault(), enabled);
    QCOMPARE(d.property("scrollOnButtonDownEnabledByDefault").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "scrollOnButtonDownEnabledByDefault"), enabled);
}

void TestLibinputDevice::testDefaultScrollButton_data()
{
    QTest::addColumn<quint32>("button");

    QTest::newRow("0") << 0u;
    QTest::newRow("BTN_LEFT") << quint32(BTN_LEFT);
    QTest::newRow("BTN_RIGHT") << quint32(BTN_RIGHT);
    QTest::newRow("BTN_MIDDLE") << quint32(BTN_MIDDLE);
    QTest::newRow("BTN_SIDE") << quint32(BTN_SIDE);
    QTest::newRow("BTN_EXTRA") << quint32(BTN_EXTRA);
    QTest::newRow("BTN_FORWARD") << quint32(BTN_FORWARD);
    QTest::newRow("BTN_BACK") << quint32(BTN_BACK);
    QTest::newRow("BTN_TASK") << quint32(BTN_TASK);
}

void TestLibinputDevice::testDefaultScrollButton()
{
    libinput_device device;
    QFETCH(quint32, button);
    device.defaultScrollButton = button;

    Device d(&device);
    QCOMPARE(d.defaultScrollButton(), button);
    QCOMPARE(d.property("defaultScrollButton").value<quint32>(), button);
    QCOMPARE(dbusProperty<quint32>(d.sysName(), "defaultScrollButton"), button);
}

void TestLibinputDevice::testSupportsDisableWhileTyping_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testSupportsDisableWhileTyping()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.supportsDisableWhileTyping = enabled;

    Device d(&device);
    QCOMPARE(d.supportsDisableWhileTyping(), enabled);
    QCOMPARE(d.property("supportsDisableWhileTyping").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "supportsDisableWhileTyping"), enabled);
}

void TestLibinputDevice::testSupportsPointerAcceleration_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testSupportsPointerAcceleration()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.supportsPointerAcceleration = enabled;

    Device d(&device);
    QCOMPARE(d.supportsPointerAcceleration(), enabled);
    QCOMPARE(d.property("supportsPointerAcceleration").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "supportsPointerAcceleration"), enabled);
}

void TestLibinputDevice::testSupportsLeftHanded_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testSupportsLeftHanded()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.supportsLeftHanded = enabled;

    Device d(&device);
    QCOMPARE(d.supportsLeftHanded(), enabled);
    QCOMPARE(d.property("supportsLeftHanded").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "supportsLeftHanded"), enabled);
}

void TestLibinputDevice::testSupportsCalibrationMatrix_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testSupportsCalibrationMatrix()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.supportsCalibrationMatrix = enabled;

    Device d(&device);
    QCOMPARE(d.supportsCalibrationMatrix(), enabled);
    QCOMPARE(d.property("supportsCalibrationMatrix").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "supportsCalibrationMatrix"), enabled);
}

void TestLibinputDevice::testSupportsDisableEvents_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testSupportsDisableEvents()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.supportsDisableEvents = enabled;

    Device d(&device);
    QCOMPARE(d.supportsDisableEvents(), enabled);
    QCOMPARE(d.property("supportsDisableEvents").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "supportsDisableEvents"), enabled);
}

void TestLibinputDevice::testSupportsDisableEventsOnExternalMouse_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testSupportsDisableEventsOnExternalMouse()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.supportsDisableEventsOnExternalMouse = enabled;

    Device d(&device);
    QCOMPARE(d.supportsDisableEventsOnExternalMouse(), enabled);
    QCOMPARE(d.property("supportsDisableEventsOnExternalMouse").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "supportsDisableEventsOnExternalMouse"), enabled);
}

void TestLibinputDevice::testSupportsMiddleEmulation_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testSupportsMiddleEmulation()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.supportsMiddleEmulation = enabled;

    Device d(&device);
    QCOMPARE(d.supportsMiddleEmulation(), enabled);
    QCOMPARE(d.property("supportsMiddleEmulation").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "supportsMiddleEmulation"), enabled);
}

void TestLibinputDevice::testSupportsNaturalScroll_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testSupportsNaturalScroll()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.supportsNaturalScroll = enabled;

    Device d(&device);
    QCOMPARE(d.supportsNaturalScroll(), enabled);
    QCOMPARE(d.property("supportsNaturalScroll").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "supportsNaturalScroll"), enabled);
}

void TestLibinputDevice::testSupportsScrollTwoFinger_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testSupportsScrollTwoFinger()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.supportedScrollMethods = enabled ? LIBINPUT_CONFIG_SCROLL_2FG : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;

    Device d(&device);
    QCOMPARE(d.supportsScrollTwoFinger(), enabled);
    QCOMPARE(d.property("supportsScrollTwoFinger").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "supportsScrollTwoFinger"), enabled);
}

void TestLibinputDevice::testSupportsScrollEdge_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testSupportsScrollEdge()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.supportedScrollMethods = enabled ? LIBINPUT_CONFIG_SCROLL_EDGE : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;

    Device d(&device);
    QCOMPARE(d.supportsScrollEdge(), enabled);
    QCOMPARE(d.property("supportsScrollEdge").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "supportsScrollEdge"), enabled);
}

void TestLibinputDevice::testSupportsScrollOnButtonDown_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testSupportsScrollOnButtonDown()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.supportedScrollMethods = enabled ? LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;

    Device d(&device);
    QCOMPARE(d.supportsScrollOnButtonDown(), enabled);
    QCOMPARE(d.property("supportsScrollOnButtonDown").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "supportsScrollOnButtonDown"), enabled);
}

void TestLibinputDevice::testDefaultPointerAcceleration_data()
{
    QTest::addColumn<qreal>("accel");

    QTest::newRow("-1.0") << -1.0;
    QTest::newRow("-0.5") << -0.5;
    QTest::newRow("0.0") << 0.0;
    QTest::newRow("0.3") << 0.3;
    QTest::newRow("1.0") << 1.0;
}

void TestLibinputDevice::testDefaultPointerAcceleration()
{
    QFETCH(qreal, accel);
    libinput_device device;
    device.defaultPointerAcceleration = accel;

    Device d(&device);
    QCOMPARE(d.defaultPointerAcceleration(), accel);
    QCOMPARE(d.property("defaultPointerAcceleration").toReal(), accel);
    QCOMPARE(dbusProperty<qreal>(d.sysName(), "defaultPointerAcceleration"), accel);
}

void TestLibinputDevice::testPointerAcceleration_data()
{
    QTest::addColumn<bool>("supported");
    QTest::addColumn<bool>("setShouldFail");
    QTest::addColumn<qreal>("accel");
    QTest::addColumn<qreal>("setAccel");
    QTest::addColumn<qreal>("expectedAccel");
    QTest::addColumn<bool>("expectedChanged");

    QTest::newRow("-1 -> 2.0") << true << false << -1.0 << 2.0 << 1.0 << true;
    QTest::newRow("0 -> -1.0") << true << false << 0.0 << -1.0 << -1.0 << true;
    QTest::newRow("1 -> 1") << true << false << 1.0 << 1.0 << 1.0 << false;
    QTest::newRow("unsupported") << false << false << 0.0 << 1.0 << 0.0 << false;
    QTest::newRow("set fails") << true << true << -1.0 << 1.0 << -1.0 << false;
}

void TestLibinputDevice::testPointerAcceleration()
{
    QFETCH(bool, supported);
    QFETCH(bool, setShouldFail);
    QFETCH(qreal, accel);
    libinput_device device;
    device.supportsPointerAcceleration = supported;
    device.pointerAcceleration = accel;
    device.setPointerAccelerationReturnValue = setShouldFail;

    Device d(&device);
    QCOMPARE(d.pointerAcceleration(), accel);
    QCOMPARE(d.property("pointerAcceleration").toReal(), accel);
    QCOMPARE(dbusProperty<qreal>(d.sysName(), "pointerAcceleration"), accel);

    QSignalSpy pointerAccelChangedSpy(&d, &Device::pointerAccelerationChanged);
    QFETCH(qreal, setAccel);
    d.setPointerAcceleration(setAccel);
    QTEST(d.pointerAcceleration(), "expectedAccel");
    QTEST(!pointerAccelChangedSpy.isEmpty(), "expectedChanged");
    QTEST(dbusProperty<qreal>(d.sysName(), "pointerAcceleration"), "expectedAccel");
}

void TestLibinputDevice::testLeftHanded_data()
{
    QTest::addColumn<bool>("supported");
    QTest::addColumn<bool>("setShouldFail");
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("setValue");
    QTest::addColumn<bool>("expectedValue");

    QTest::newRow("unsupported/true") << false << false << true << false << false;
    QTest::newRow("unsupported/false") << false << false << false << true << false;
    QTest::newRow("true -> false") << true << false << true << false << false;
    QTest::newRow("false -> true") << true << false << false << true << true;
    QTest::newRow("set fails") << true << true << true << false << true;
    QTest::newRow("true -> true") << true << false << true << true << true;
    QTest::newRow("false -> false") << true << false << false << false << false;
}

void TestLibinputDevice::testLeftHanded()
{
    QFETCH(bool, supported);
    QFETCH(bool, setShouldFail);
    QFETCH(bool, initValue);
    libinput_device device;
    device.supportsLeftHanded = supported;
    device.leftHanded = initValue;
    device.setLeftHandedReturnValue = setShouldFail;

    Device d(&device);
    QCOMPARE(d.isLeftHanded(), supported && initValue);
    QCOMPARE(d.property("leftHanded").toBool(), supported && initValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "leftHanded"), supported && initValue);

    QSignalSpy leftHandedChangedSpy(&d, &Device::leftHandedChanged);
    QFETCH(bool, setValue);
    d.setLeftHanded(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isLeftHanded(), expectedValue);
    QCOMPARE(leftHandedChangedSpy.isEmpty(), (supported && initValue) == expectedValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "leftHanded"), expectedValue);
}

void TestLibinputDevice::testSupportedButtons_data()
{
    QTest::addColumn<bool>("isPointer");
    QTest::addColumn<Qt::MouseButtons>("setButtons");
    QTest::addColumn<Qt::MouseButtons>("expectedButtons");

    QTest::newRow("left") << true << Qt::MouseButtons(Qt::LeftButton) << Qt::MouseButtons(Qt::LeftButton);
    QTest::newRow("right") << true << Qt::MouseButtons(Qt::RightButton) << Qt::MouseButtons(Qt::RightButton);
    QTest::newRow("middle") << true << Qt::MouseButtons(Qt::MiddleButton) << Qt::MouseButtons(Qt::MiddleButton);
    QTest::newRow("extra1") << true << Qt::MouseButtons(Qt::ExtraButton1) << Qt::MouseButtons(Qt::ExtraButton1);
    QTest::newRow("extra2") << true << Qt::MouseButtons(Qt::ExtraButton2) << Qt::MouseButtons(Qt::ExtraButton2);
    QTest::newRow("back") << true << Qt::MouseButtons(Qt::BackButton) << Qt::MouseButtons(Qt::BackButton);
    QTest::newRow("forward") << true << Qt::MouseButtons(Qt::ForwardButton) << Qt::MouseButtons(Qt::ForwardButton);
    QTest::newRow("task") << true << Qt::MouseButtons(Qt::TaskButton) << Qt::MouseButtons(Qt::TaskButton);

    QTest::newRow("no pointer/left") << false << Qt::MouseButtons(Qt::LeftButton) << Qt::MouseButtons();
    QTest::newRow("no pointer/right") << false << Qt::MouseButtons(Qt::RightButton) << Qt::MouseButtons();
    QTest::newRow("no pointer/middle") << false << Qt::MouseButtons(Qt::MiddleButton) << Qt::MouseButtons();
    QTest::newRow("no pointer/extra1") << false << Qt::MouseButtons(Qt::ExtraButton1) << Qt::MouseButtons();
    QTest::newRow("no pointer/extra2") << false << Qt::MouseButtons(Qt::ExtraButton2) << Qt::MouseButtons();
    QTest::newRow("no pointer/back") << false << Qt::MouseButtons(Qt::BackButton) << Qt::MouseButtons();
    QTest::newRow("no pointer/forward") << false << Qt::MouseButtons(Qt::ForwardButton) << Qt::MouseButtons();
    QTest::newRow("no pointer/task") << false << Qt::MouseButtons(Qt::TaskButton) << Qt::MouseButtons();

    QTest::newRow("all") << true
                         << Qt::MouseButtons(Qt::LeftButton | Qt::RightButton | Qt::MiddleButton | Qt::ExtraButton1 | Qt::ExtraButton2 | Qt::BackButton | Qt::ForwardButton | Qt::TaskButton)
                         << Qt::MouseButtons(Qt::LeftButton | Qt::RightButton | Qt::MiddleButton | Qt::ExtraButton1 | Qt::ExtraButton2 | Qt::BackButton | Qt::ForwardButton | Qt::TaskButton);
}

void TestLibinputDevice::testSupportedButtons()
{
    libinput_device device;
    QFETCH(bool, isPointer);
    device.pointer = isPointer;
    QFETCH(Qt::MouseButtons, setButtons);
    device.supportedButtons = setButtons;

    Device d(&device);
    QCOMPARE(d.isPointer(), isPointer);
    QTEST(d.supportedButtons(), "expectedButtons");
    QTEST(Qt::MouseButtons(dbusProperty<int>(d.sysName(), "supportedButtons")), "expectedButtons");
}

void TestLibinputDevice::testAlphaNumericKeyboard_data()
{
    QTest::addColumn<QVector<quint32>>("supportedKeys");
    QTest::addColumn<bool>("isAlpha");

    QVector<quint32> keys;

    for (int i = KEY_1; i <= KEY_0; i++) {
        keys << i;
        QByteArray row = QByteArrayLiteral("number");
        row.append(QByteArray::number(i));
        QTest::newRow(row.constData()) << keys << false;
    }
    for (int i = KEY_Q; i <= KEY_P; i++) {
        keys << i;
        QByteArray row = QByteArrayLiteral("alpha");
        row.append(QByteArray::number(i));
        QTest::newRow(row.constData()) << keys << false;
    }
    for (int i = KEY_A; i <= KEY_L; i++) {
        keys << i;
        QByteArray row = QByteArrayLiteral("alpha");
        row.append(QByteArray::number(i));
        QTest::newRow(row.constData()) << keys << false;
    }
    for (int i = KEY_Z; i < KEY_M; i++) {
        keys << i;
        QByteArray row = QByteArrayLiteral("alpha");
        row.append(QByteArray::number(i));
        QTest::newRow(row.constData()) << keys << false;
    }
    // adding a different key should not result in it becoming alphanumeric keyboard
    keys << KEY_SEMICOLON;
    QTest::newRow("semicolon") << keys << false;

    // last but not least the M which should turn everything on
    keys << KEY_M;
    QTest::newRow("alphanumeric") << keys << true;
}

void TestLibinputDevice::testAlphaNumericKeyboard()
{
    QFETCH(QVector<quint32>, supportedKeys);
    libinput_device device;
    device.keyboard = true;
    device.keys = supportedKeys;

    Device d(&device);
    QCOMPARE(d.isKeyboard(), true);
    QTEST(d.isAlphaNumericKeyboard(), "isAlpha");
    QTEST(dbusProperty<bool>(d.sysName(), "alphaNumericKeyboard"), "isAlpha");
}

void TestLibinputDevice::testEnabled_data()
{
    QTest::addColumn<bool>("supported");
    QTest::addColumn<bool>("setShouldFail");
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("setValue");
    QTest::addColumn<bool>("expectedValue");

    QTest::newRow("unsupported/true") << false << false << true << false << true;
    QTest::newRow("unsupported/false") << false << false << false << true << true;
    QTest::newRow("true -> false") << true << false << true << false << false;
    QTest::newRow("false -> true") << true << false << false << true << true;
    QTest::newRow("set fails") << true << true << true << false << true;
    QTest::newRow("true -> true") << true << false << true << true << true;
    QTest::newRow("false -> false") << true << false << false << false << false;
}

void TestLibinputDevice::testEnabled()
{
    libinput_device device;
    QFETCH(bool, supported);
    QFETCH(bool, setShouldFail);
    QFETCH(bool, initValue);
    device.supportsDisableEvents = supported;
    device.enabled = initValue;
    device.setEnableModeReturnValue = setShouldFail;

    Device d(&device);
    QCOMPARE(d.isEnabled(), !supported || initValue);
    QCOMPARE(d.property("enabled").toBool(), !supported || initValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "enabled"), !supported || initValue);

    QSignalSpy enabledChangedSpy(&d, &Device::enabledChanged);
    QFETCH(bool, setValue);
    d.setEnabled(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isEnabled(), expectedValue);

    QCOMPARE(dbusProperty<bool>(d.sysName(), "enabled"), expectedValue);
}

void TestLibinputDevice::testTapToClick_data()
{
    QTest::addColumn<int>("fingerCount");
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("setValue");
    QTest::addColumn<bool>("setShouldFail");
    QTest::addColumn<bool>("expectedValue");

    QTest::newRow("unsupported") << 0 << false << true << true << false;
    QTest::newRow("true -> false") << 1 << true << false << false << false;
    QTest::newRow("false -> true") << 2 << false << true << false << true;
    QTest::newRow("set fails") << 3 << true << false << true << true;
    QTest::newRow("true -> true") << 2 << true << true << false << true;
    QTest::newRow("false -> false") << 1 << false << false << false << false;
}

void TestLibinputDevice::testTapToClick()
{
    libinput_device device;
    QFETCH(int, fingerCount);
    QFETCH(bool, initValue);
    QFETCH(bool, setShouldFail);
    device.tapFingerCount = fingerCount;
    device.tapToClick = initValue;
    device.setTapToClickReturnValue = setShouldFail;

    Device d(&device);
    QCOMPARE(d.tapFingerCount(), fingerCount);
    QCOMPARE(d.isTapToClick(), initValue);
    QCOMPARE(d.property("tapToClick").toBool(), initValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "tapToClick"), initValue);

    QSignalSpy tapToClickChangedSpy(&d, &Device::tapToClickChanged);
    QFETCH(bool, setValue);
    d.setTapToClick(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isTapToClick(), expectedValue);
    QCOMPARE(tapToClickChangedSpy.isEmpty(), initValue == expectedValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "tapToClick"), expectedValue);
}

void TestLibinputDevice::testTapAndDragEnabledByDefault_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testTapAndDragEnabledByDefault()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.tapAndDragEnabledByDefault = enabled;

    Device d(&device);
    QCOMPARE(d.tapAndDragEnabledByDefault(), enabled);
    QCOMPARE(d.property("tapAndDragEnabledByDefault").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "tapAndDragEnabledByDefault"), enabled);
}

void TestLibinputDevice::testTapAndDrag_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("setValue");
    QTest::addColumn<bool>("setShouldFail");
    QTest::addColumn<bool>("expectedValue");

    QTest::newRow("true -> false") << true << false << false << false;
    QTest::newRow("false -> true") << false << true << false << true;
    QTest::newRow("set fails") << true << false << true << true;
    QTest::newRow("true -> true") << true << true << false << true;
    QTest::newRow("false -> false") << false << false << false << false;
}

void TestLibinputDevice::testTapAndDrag()
{
    libinput_device device;
    QFETCH(bool, initValue);
    QFETCH(bool, setShouldFail);
    device.tapAndDrag = initValue;
    device.setTapAndDragReturnValue = setShouldFail;

    Device d(&device);
    QCOMPARE(d.isTapAndDrag(), initValue);
    QCOMPARE(d.property("tapAndDrag").toBool(), initValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "tapAndDrag"), initValue);

    QSignalSpy tapAndDragChangedSpy(&d, &Device::tapAndDragChanged);
    QFETCH(bool, setValue);
    d.setTapAndDrag(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isTapAndDrag(), expectedValue);
    QCOMPARE(tapAndDragChangedSpy.isEmpty(), initValue == expectedValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "tapAndDrag"), expectedValue);
}

void TestLibinputDevice::testTapDragLockEnabledByDefault_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testTapDragLockEnabledByDefault()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.tapDragLockEnabledByDefault = enabled;

    Device d(&device);
    QCOMPARE(d.tapDragLockEnabledByDefault(), enabled);
    QCOMPARE(d.property("tapDragLockEnabledByDefault").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "tapDragLockEnabledByDefault"), enabled);
}

void TestLibinputDevice::testTapDragLock_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("setValue");
    QTest::addColumn<bool>("setShouldFail");
    QTest::addColumn<bool>("expectedValue");

    QTest::newRow("true -> false") << true << false << false << false;
    QTest::newRow("false -> true") << false << true << false << true;
    QTest::newRow("set fails") << true << false << true << true;
    QTest::newRow("true -> true") << true << true << false << true;
    QTest::newRow("false -> false") << false << false << false << false;
}

void TestLibinputDevice::testTapDragLock()
{
    libinput_device device;
    QFETCH(bool, initValue);
    QFETCH(bool, setShouldFail);
    device.tapDragLock = initValue;
    device.setTapDragLockReturnValue = setShouldFail;

    Device d(&device);
    QCOMPARE(d.isTapDragLock(), initValue);
    QCOMPARE(d.property("tapDragLock").toBool(), initValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "tapDragLock"), initValue);

    QSignalSpy tapDragLockChangedSpy(&d, &Device::tapDragLockChanged);
    QFETCH(bool, setValue);
    d.setTapDragLock(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isTapDragLock(), expectedValue);
    QCOMPARE(tapDragLockChangedSpy.isEmpty(), initValue == expectedValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "tapDragLock"), expectedValue);
}

void TestLibinputDevice::testMiddleEmulation_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("setValue");
    QTest::addColumn<bool>("setShouldFail");
    QTest::addColumn<bool>("expectedValue");
    QTest::addColumn<bool>("supportsMiddleButton");

    QTest::newRow("true -> false") << true << false << false << false << true;
    QTest::newRow("false -> true") << false << true << false << true << true;
    QTest::newRow("set fails") << true << false << true << true << true;
    QTest::newRow("true -> true") << true << true << false << true << true;
    QTest::newRow("false -> false") << false << false << false << false << true;

    QTest::newRow("false -> true, unsupported") << false << true << true << false << false;
}

void TestLibinputDevice::testMiddleEmulation()
{
    libinput_device device;
    QFETCH(bool, initValue);
    QFETCH(bool, setShouldFail);
    QFETCH(bool, supportsMiddleButton);
    device.supportsMiddleEmulation = supportsMiddleButton;
    device.middleEmulation = initValue;
    device.setMiddleEmulationReturnValue = setShouldFail;

    Device d(&device);
    QCOMPARE(d.isMiddleEmulation(), initValue);
    QCOMPARE(d.property("middleEmulation").toBool(), initValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "middleEmulation"), initValue);

    QSignalSpy middleEmulationChangedSpy(&d, &Device::middleEmulationChanged);
    QFETCH(bool, setValue);
    d.setMiddleEmulation(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isMiddleEmulation(), expectedValue);
    QCOMPARE(d.property("middleEmulation").toBool(), expectedValue);
    QCOMPARE(middleEmulationChangedSpy.isEmpty(), initValue == expectedValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "middleEmulation"), expectedValue);
}

void TestLibinputDevice::testNaturalScroll_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("setValue");
    QTest::addColumn<bool>("setShouldFail");
    QTest::addColumn<bool>("expectedValue");
    QTest::addColumn<bool>("supportsNaturalScroll");

    QTest::newRow("true -> false") << true << false << false << false << true;
    QTest::newRow("false -> true") << false << true << false << true << true;
    QTest::newRow("set fails") << true << false << true << true << true;
    QTest::newRow("true -> true") << true << true << false << true << true;
    QTest::newRow("false -> false") << false << false << false << false << true;

    QTest::newRow("false -> true, unsupported") << false << true << true << false << false;
}

void TestLibinputDevice::testNaturalScroll()
{
    libinput_device device;
    QFETCH(bool, initValue);
    QFETCH(bool, setShouldFail);
    QFETCH(bool, supportsNaturalScroll);
    device.supportsNaturalScroll = supportsNaturalScroll;
    device.naturalScroll = initValue;
    device.setNaturalScrollReturnValue = setShouldFail;

    Device d(&device);
    QCOMPARE(d.isNaturalScroll(), initValue);
    QCOMPARE(d.property("naturalScroll").toBool(), initValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "naturalScroll"), initValue);

    QSignalSpy naturalScrollChangedSpy(&d, &Device::naturalScrollChanged);
    QFETCH(bool, setValue);
    d.setNaturalScroll(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isNaturalScroll(), expectedValue);
    QCOMPARE(d.property("naturalScroll").toBool(), expectedValue);
    QCOMPARE(naturalScrollChangedSpy.isEmpty(), initValue == expectedValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "naturalScroll"), expectedValue);
}

void TestLibinputDevice::testScrollFactor()
{
    libinput_device device;

    qreal initValue = 1.0;

    Device d(&device);
    QCOMPARE(d.scrollFactor(), initValue);
    QCOMPARE(d.property("scrollFactor").toReal(), initValue);
    QCOMPARE(dbusProperty<qreal>(d.sysName(), "scrollFactor"), initValue);

    QSignalSpy scrollFactorChangedSpy(&d, &Device::scrollFactorChanged);

    qreal expectedValue = 2.0;

    d.setScrollFactor(expectedValue);
    QCOMPARE(d.scrollFactor(), expectedValue);
    QCOMPARE(d.property("scrollFactor").toReal(), expectedValue);
    QCOMPARE(scrollFactorChangedSpy.isEmpty(), false);
    QCOMPARE(dbusProperty<qreal>(d.sysName(), "scrollFactor"), expectedValue);
}

void TestLibinputDevice::testScrollTwoFinger_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("otherValue");
    QTest::addColumn<bool>("setValue");
    QTest::addColumn<bool>("setShouldFail");
    QTest::addColumn<bool>("expectedValue");
    QTest::addColumn<bool>("supportsScrollTwoFinger");

    QTest::newRow("true -> false") << true << false << false << false << false << true;
    QTest::newRow("other -> false") << false << true << false << false << false << true;
    QTest::newRow("false -> true") << false << false << true << false << true << true;
    QTest::newRow("set fails") << true << false << false << true << true << true;
    QTest::newRow("true -> true") << true << false << true << false << true << true;
    QTest::newRow("false -> false") << false << false << false << false << false << true;

    QTest::newRow("false -> true, unsupported") << false << false << true << true << false << false;
}

void TestLibinputDevice::testScrollTwoFinger()
{
    libinput_device device;
    QFETCH(bool, initValue);
    QFETCH(bool, otherValue);
    QFETCH(bool, setShouldFail);
    QFETCH(bool, supportsScrollTwoFinger);
    device.supportedScrollMethods = (supportsScrollTwoFinger ? LIBINPUT_CONFIG_SCROLL_2FG : LIBINPUT_CONFIG_SCROLL_NO_SCROLL) | LIBINPUT_CONFIG_SCROLL_EDGE;
    device.scrollMethod = initValue ? LIBINPUT_CONFIG_SCROLL_2FG : otherValue ? LIBINPUT_CONFIG_SCROLL_EDGE
                                                                              : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    device.setScrollMethodReturnValue = setShouldFail;

    Device d(&device);
    QCOMPARE(d.isScrollTwoFinger(), initValue);
    QCOMPARE(d.property("scrollTwoFinger").toBool(), initValue);
    QCOMPARE(d.property("scrollEdge").toBool(), otherValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "scrollTwoFinger"), initValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "scrollEdge"), otherValue);

    QSignalSpy scrollMethodChangedSpy(&d, &Device::scrollMethodChanged);
    QFETCH(bool, setValue);
    d.setScrollTwoFinger(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isScrollTwoFinger(), expectedValue);
    QCOMPARE(d.property("scrollTwoFinger").toBool(), expectedValue);
    QCOMPARE(scrollMethodChangedSpy.isEmpty(), initValue == expectedValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "scrollTwoFinger"), expectedValue);
}

void TestLibinputDevice::testScrollEdge_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("otherValue");
    QTest::addColumn<bool>("setValue");
    QTest::addColumn<bool>("setShouldFail");
    QTest::addColumn<bool>("expectedValue");
    QTest::addColumn<bool>("supportsScrollEdge");

    QTest::newRow("true -> false") << true << false << false << false << false << true;
    QTest::newRow("other -> false") << false << true << false << false << false << true;
    QTest::newRow("false -> true") << false << false << true << false << true << true;
    QTest::newRow("set fails") << true << false << false << true << true << true;
    QTest::newRow("true -> true") << true << false << true << false << true << true;
    QTest::newRow("false -> false") << false << false << false << false << false << true;

    QTest::newRow("false -> true, unsupported") << false << false << true << true << false << false;
}

void TestLibinputDevice::testScrollEdge()
{
    libinput_device device;
    QFETCH(bool, initValue);
    QFETCH(bool, otherValue);
    QFETCH(bool, setShouldFail);
    QFETCH(bool, supportsScrollEdge);
    device.supportedScrollMethods = (supportsScrollEdge ? LIBINPUT_CONFIG_SCROLL_EDGE : LIBINPUT_CONFIG_SCROLL_NO_SCROLL) | LIBINPUT_CONFIG_SCROLL_2FG;
    device.scrollMethod = initValue ? LIBINPUT_CONFIG_SCROLL_EDGE : otherValue ? LIBINPUT_CONFIG_SCROLL_2FG
                                                                               : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    device.setScrollMethodReturnValue = setShouldFail;

    Device d(&device);
    QCOMPARE(d.isScrollEdge(), initValue);
    QCOMPARE(d.property("scrollEdge").toBool(), initValue);
    QCOMPARE(d.property("scrollTwoFinger").toBool(), otherValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "scrollEdge"), initValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "scrollTwoFinger"), otherValue);

    QSignalSpy scrollMethodChangedSpy(&d, &Device::scrollMethodChanged);
    QFETCH(bool, setValue);
    d.setScrollEdge(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isScrollEdge(), expectedValue);
    QCOMPARE(d.property("scrollEdge").toBool(), expectedValue);
    QCOMPARE(scrollMethodChangedSpy.isEmpty(), initValue == expectedValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "scrollEdge"), expectedValue);
}

void TestLibinputDevice::testScrollButtonDown_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("otherValue");
    QTest::addColumn<bool>("setValue");
    QTest::addColumn<bool>("setShouldFail");
    QTest::addColumn<bool>("expectedValue");
    QTest::addColumn<bool>("supportsScrollButtonDown");

    QTest::newRow("true -> false") << true << false << false << false << false << true;
    QTest::newRow("other -> false") << false << true << false << false << false << true;
    QTest::newRow("false -> true") << false << false << true << false << true << true;
    QTest::newRow("set fails") << true << false << false << true << true << true;
    QTest::newRow("true -> true") << true << false << true << false << true << true;
    QTest::newRow("false -> false") << false << false << false << false << false << true;

    QTest::newRow("false -> true, unsupported") << false << false << true << true << false << false;
}

void TestLibinputDevice::testScrollButtonDown()
{
    libinput_device device;
    QFETCH(bool, initValue);
    QFETCH(bool, otherValue);
    QFETCH(bool, setShouldFail);
    QFETCH(bool, supportsScrollButtonDown);
    device.supportedScrollMethods = (supportsScrollButtonDown ? LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN : LIBINPUT_CONFIG_SCROLL_NO_SCROLL) | LIBINPUT_CONFIG_SCROLL_2FG;
    device.scrollMethod = initValue ? LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN : otherValue ? LIBINPUT_CONFIG_SCROLL_2FG
                                                                                         : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    device.setScrollMethodReturnValue = setShouldFail;

    Device d(&device);
    QCOMPARE(d.isScrollOnButtonDown(), initValue);
    QCOMPARE(d.property("scrollOnButtonDown").toBool(), initValue);
    QCOMPARE(d.property("scrollTwoFinger").toBool(), otherValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "scrollOnButtonDown"), initValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "scrollTwoFinger"), otherValue);

    QSignalSpy scrollMethodChangedSpy(&d, &Device::scrollMethodChanged);
    QFETCH(bool, setValue);
    d.setScrollOnButtonDown(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isScrollOnButtonDown(), expectedValue);
    QCOMPARE(d.property("scrollOnButtonDown").toBool(), expectedValue);
    QCOMPARE(scrollMethodChangedSpy.isEmpty(), initValue == expectedValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "scrollOnButtonDown"), expectedValue);
}

void TestLibinputDevice::testScrollButton_data()
{
    QTest::addColumn<quint32>("initValue");
    QTest::addColumn<quint32>("setValue");
    QTest::addColumn<quint32>("expectedValue");
    QTest::addColumn<bool>("setShouldFail");
    QTest::addColumn<bool>("scrollOnButton");

    QTest::newRow("BTN_LEFT -> BTN_RIGHT") << quint32(BTN_LEFT) << quint32(BTN_RIGHT) << quint32(BTN_RIGHT) << false << true;
    QTest::newRow("BTN_LEFT -> BTN_LEFT") << quint32(BTN_LEFT) << quint32(BTN_LEFT) << quint32(BTN_LEFT) << false << true;
    QTest::newRow("set should fail") << quint32(BTN_LEFT) << quint32(BTN_RIGHT) << quint32(BTN_LEFT) << true << true;
    QTest::newRow("not scroll on button") << quint32(BTN_LEFT) << quint32(BTN_RIGHT) << quint32(BTN_LEFT) << false << false;
}

void TestLibinputDevice::testScrollButton()
{
    libinput_device device;
    QFETCH(quint32, initValue);
    QFETCH(bool, setShouldFail);
    QFETCH(bool, scrollOnButton);
    device.scrollButton = initValue;
    device.supportedScrollMethods = scrollOnButton ? LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    device.setScrollButtonReturnValue = setShouldFail;

    Device d(&device);
    QCOMPARE(d.scrollButton(), initValue);
    QCOMPARE(d.property("scrollButton").value<quint32>(), initValue);
    QCOMPARE(dbusProperty<quint32>(d.sysName(), "scrollButton"), initValue);

    QSignalSpy scrollButtonChangedSpy(&d, &Device::scrollButtonChanged);
    QFETCH(quint32, setValue);
    d.setScrollButton(setValue);
    QFETCH(quint32, expectedValue);
    QCOMPARE(d.scrollButton(), expectedValue);
    QCOMPARE(d.property("scrollButton").value<quint32>(), expectedValue);
    QCOMPARE(scrollButtonChangedSpy.isEmpty(), initValue == expectedValue);
    QCOMPARE(dbusProperty<quint32>(d.sysName(), "scrollButton"), expectedValue);
}

void TestLibinputDevice::testDisableWhileTypingEnabledByDefault_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testDisableWhileTypingEnabledByDefault()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.disableWhileTypingEnabledByDefault = enabled ? LIBINPUT_CONFIG_DWT_ENABLED : LIBINPUT_CONFIG_DWT_DISABLED;

    Device d(&device);
    QCOMPARE(d.disableWhileTypingEnabledByDefault(), enabled);
    QCOMPARE(d.property("disableWhileTypingEnabledByDefault").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "disableWhileTypingEnabledByDefault"), enabled);
}

void TestLibinputDevice::testLmrTapButtonMapEnabledByDefault_data()
{
    QTest::addColumn<bool>("enabled");

    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void TestLibinputDevice::testLmrTapButtonMapEnabledByDefault()
{
    QFETCH(bool, enabled);
    libinput_device device;
    device.defaultTapButtonMap = enabled ? LIBINPUT_CONFIG_TAP_MAP_LMR : LIBINPUT_CONFIG_TAP_MAP_LRM;

    Device d(&device);
    QCOMPARE(d.lmrTapButtonMapEnabledByDefault(), enabled);
    QCOMPARE(d.property("lmrTapButtonMapEnabledByDefault").toBool(), enabled);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "lmrTapButtonMapEnabledByDefault"), enabled);
}

void TestLibinputDevice::testLmrTapButtonMap_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("setValue");
    QTest::addColumn<bool>("setShouldFail");
    QTest::addColumn<bool>("expectedValue");
    QTest::addColumn<int>("fingerCount");

    QTest::newRow("true -> false") << true << false << false << false << 3;
    QTest::newRow("false -> true") << false << true << false << true << 3;
    QTest::newRow("true -> false") << true << false << false << false << 2;
    QTest::newRow("false -> true") << false << true << false << true << 2;

    QTest::newRow("set fails") << true << false << true << true << 3;

    QTest::newRow("true -> true") << true << true << false << true << 3;
    QTest::newRow("false -> false") << false << false << false << false << 3;
    QTest::newRow("true -> true") << true << true << false << true << 2;
    QTest::newRow("false -> false") << false << false << false << false << 2;

    QTest::newRow("false -> true, fingerCount 0") << false << true << true << false << 0;

    // TODO: is this a fail in libinput?
    // QTest::newRow("false -> true, fingerCount 1") << false << true << true << false << 1;
}

void TestLibinputDevice::testLmrTapButtonMap()
{
    libinput_device device;
    QFETCH(bool, initValue);
    QFETCH(bool, setShouldFail);
    QFETCH(int, fingerCount);
    device.tapFingerCount = fingerCount;
    device.tapButtonMap = initValue ? LIBINPUT_CONFIG_TAP_MAP_LMR : LIBINPUT_CONFIG_TAP_MAP_LRM;
    device.setTapButtonMapReturnValue = setShouldFail;

    Device d(&device);
    QCOMPARE(d.lmrTapButtonMap(), initValue);
    QCOMPARE(d.property("lmrTapButtonMap").toBool(), initValue);

    QSignalSpy tapButtonMapChangedSpy(&d, &Device::tapButtonMapChanged);
    QFETCH(bool, setValue);
    d.setLmrTapButtonMap(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.lmrTapButtonMap(), expectedValue);
    QCOMPARE(d.property("lmrTapButtonMap").toBool(), expectedValue);
    QCOMPARE(tapButtonMapChangedSpy.isEmpty(), initValue == expectedValue);
}

void TestLibinputDevice::testDisableWhileTyping_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("setValue");
    QTest::addColumn<bool>("setShouldFail");
    QTest::addColumn<bool>("expectedValue");
    QTest::addColumn<bool>("supportsDisableWhileTyping");

    QTest::newRow("true -> false") << true << false << false << false << true;
    QTest::newRow("false -> true") << false << true << false << true << true;
    QTest::newRow("set fails") << true << false << true << true << true;
    QTest::newRow("true -> true") << true << true << false << true << true;
    QTest::newRow("false -> false") << false << false << false << false << true;

    QTest::newRow("false -> true, unsupported") << false << true << true << false << false;
}

void TestLibinputDevice::testDisableWhileTyping()
{
    libinput_device device;
    QFETCH(bool, initValue);
    QFETCH(bool, setShouldFail);
    QFETCH(bool, supportsDisableWhileTyping);
    device.supportsDisableWhileTyping = supportsDisableWhileTyping;
    device.disableWhileTyping = initValue ? LIBINPUT_CONFIG_DWT_ENABLED : LIBINPUT_CONFIG_DWT_DISABLED;
    device.setDisableWhileTypingReturnValue = setShouldFail;

    Device d(&device);
    QCOMPARE(d.isDisableWhileTyping(), initValue);
    QCOMPARE(d.property("disableWhileTyping").toBool(), initValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "disableWhileTyping"), initValue);

    QSignalSpy disableWhileTypingChangedSpy(&d, &Device::disableWhileTypingChanged);
    QFETCH(bool, setValue);
    d.setDisableWhileTyping(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isDisableWhileTyping(), expectedValue);
    QCOMPARE(d.property("disableWhileTyping").toBool(), expectedValue);
    QCOMPARE(disableWhileTypingChangedSpy.isEmpty(), initValue == expectedValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "disableWhileTyping"), expectedValue);
}

void TestLibinputDevice::testLoadEnabled_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("configValue");

    QTest::newRow("false -> true") << false << true;
    QTest::newRow("true -> false") << true << false;
    QTest::newRow("true -> true") << true << true;
    QTest::newRow("false -> false") << false << false;
}

void TestLibinputDevice::testLoadEnabled()
{
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup inputConfig(config, QStringLiteral("Test"));
    QFETCH(bool, configValue);
    QFETCH(bool, initValue);
    inputConfig.writeEntry("Enabled", configValue);

    libinput_device device;
    device.supportsDisableEvents = true;
    device.enabled = initValue;
    device.setEnableModeReturnValue = false;

    Device d(&device);
    QCOMPARE(d.isEnabled(), initValue);
    // no config group set, should not change
    d.loadConfiguration();
    QCOMPARE(d.isEnabled(), initValue);

    // set the group
    d.setConfig(inputConfig);
    d.loadConfiguration();
    QCOMPARE(d.isEnabled(), configValue);

    // and try to store
    if (configValue != initValue) {
        d.setEnabled(initValue);
        QCOMPARE(inputConfig.readEntry("Enabled", configValue), initValue);
    }
}

void TestLibinputDevice::testLoadPointerAcceleration_data()
{
    QTest::addColumn<qreal>("initValue");
    QTest::addColumn<qreal>("configValue");

    QTest::newRow("-0.2 -> 0.9") << -0.2 << 0.9;
    QTest::newRow("0.0 -> -1.0") << 0.0 << -1.0;
    QTest::newRow("0.123 -> -0.456") << 0.123 << -0.456;
    QTest::newRow("0.7 -> 0.7") << 0.7 << 0.7;
}

void TestLibinputDevice::testLoadPointerAcceleration()
{
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup inputConfig(config, QStringLiteral("Test"));
    QFETCH(qreal, configValue);
    QFETCH(qreal, initValue);
    inputConfig.writeEntry("PointerAcceleration", configValue);

    libinput_device device;
    device.supportsPointerAcceleration = true;
    device.pointerAcceleration = initValue;
    device.setPointerAccelerationReturnValue = false;

    Device d(&device);
    QCOMPARE(d.pointerAcceleration(), initValue);
    QCOMPARE(d.property("pointerAcceleration").toReal(), initValue);
    // no config group set, should not change
    d.loadConfiguration();
    QCOMPARE(d.pointerAcceleration(), initValue);
    QCOMPARE(d.property("pointerAcceleration").toReal(), initValue);

    // set the group
    d.setConfig(inputConfig);
    d.loadConfiguration();
    QCOMPARE(d.pointerAcceleration(), configValue);
    QCOMPARE(d.property("pointerAcceleration").toReal(), configValue);

    // and try to store
    if (configValue != initValue) {
        d.setPointerAcceleration(initValue);
        QCOMPARE(inputConfig.readEntry("PointerAcceleration", configValue), initValue);
    }
}

void TestLibinputDevice::testLoadPointerAccelerationProfile_data()
{
    QTest::addColumn<quint32>("initValue");
    QTest::addColumn<QString>("initValuePropNameString");
    QTest::addColumn<quint32>("configValue");
    QTest::addColumn<QString>("configValuePropNameString");

    QTest::newRow("pointerAccelerationProfileFlat -> pointerAccelerationProfileAdaptive")
        << (quint32)LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT << "pointerAccelerationProfileFlat"
        << (quint32)LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE << "pointerAccelerationProfileAdaptive";
    QTest::newRow("pointerAccelerationProfileAdaptive -> pointerAccelerationProfileFlat")
        << (quint32)LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE << "pointerAccelerationProfileAdaptive"
        << (quint32)LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT << "pointerAccelerationProfileFlat";
    QTest::newRow("pointerAccelerationProfileAdaptive -> pointerAccelerationProfileAdaptive")
        << (quint32)LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE << "pointerAccelerationProfileAdaptive" << (quint32)LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE << "pointerAccelerationProfileAdaptive";
}

void TestLibinputDevice::testLoadPointerAccelerationProfile()
{
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup inputConfig(config, QStringLiteral("Test"));
    QFETCH(quint32, initValue);
    QFETCH(quint32, configValue);
    QFETCH(QString, initValuePropNameString);
    QFETCH(QString, configValuePropNameString);

    QByteArray initValuePropName = initValuePropNameString.toLatin1();
    QByteArray configValuePropName = configValuePropNameString.toLatin1();

    inputConfig.writeEntry("PointerAccelerationProfile", configValue);

    libinput_device device;
    device.supportedPointerAccelerationProfiles = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT | LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
    device.pointerAccelerationProfile = (libinput_config_accel_profile)initValue;
    device.setPointerAccelerationProfileReturnValue = false;

    Device d(&device);
    QCOMPARE(d.property(initValuePropName).toBool(), true);
    QCOMPARE(d.property(configValuePropName).toBool(), initValue == configValue);
    // no config group set, should not change
    d.loadConfiguration();
    QCOMPARE(d.property(initValuePropName).toBool(), true);
    QCOMPARE(d.property(configValuePropName).toBool(), initValue == configValue);

    // set the group
    d.setConfig(inputConfig);
    d.loadConfiguration();
    QCOMPARE(d.property(initValuePropName).toBool(), initValue == configValue);
    QCOMPARE(d.property(configValuePropName).toBool(), true);
    QCOMPARE(dbusProperty<bool>(d.sysName(), initValuePropName), initValue == configValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), configValuePropName), true);

    // and try to store
    if (configValue != initValue) {
        d.setProperty(initValuePropName, true);
        QCOMPARE(inputConfig.readEntry("PointerAccelerationProfile", configValue), initValue);
    }
}

void TestLibinputDevice::testLoadClickMethod_data()
{
    QTest::addColumn<quint32>("initValue");
    QTest::addColumn<QString>("initValuePropNameString");
    QTest::addColumn<quint32>("configValue");
    QTest::addColumn<QString>("configValuePropNameString");

    QTest::newRow("clickMethodAreas -> clickMethodClickfinger")
        << static_cast<quint32>(LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS) << "clickMethodAreas"
        << static_cast<quint32>(LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER) << "clickMethodClickfinger";
    QTest::newRow("clickMethodClickfinger -> clickMethodAreas")
        << static_cast<quint32>(LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER) << "clickMethodClickfinger"
        << static_cast<quint32>(LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS) << "clickMethodAreas";
    QTest::newRow("clickMethodAreas -> clickMethodAreas")
        << static_cast<quint32>(LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS) << "clickMethodAreas"
        << static_cast<quint32>(LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS) << "clickMethodAreas";
    QTest::newRow("clickMethodClickfinger -> clickMethodClickfinger")
        << static_cast<quint32>(LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER) << "clickMethodClickfinger"
        << static_cast<quint32>(LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER) << "clickMethodClickfinger";
}

void TestLibinputDevice::testLoadClickMethod()
{
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup inputConfig(config, QStringLiteral("Test"));
    QFETCH(quint32, initValue);
    QFETCH(quint32, configValue);
    QFETCH(QString, initValuePropNameString);
    QFETCH(QString, configValuePropNameString);

    QByteArray initValuePropName = initValuePropNameString.toLatin1();
    QByteArray configValuePropName = configValuePropNameString.toLatin1();

    inputConfig.writeEntry("ClickMethod", configValue);

    libinput_device device;
    device.supportedClickMethods = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS | LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;
    device.clickMethod = (libinput_config_click_method)initValue;
    device.setClickMethodReturnValue = false;

    Device d(&device);
    QCOMPARE(d.property(initValuePropName).toBool(), true);
    QCOMPARE(d.property(configValuePropName).toBool(), initValue == configValue);
    // no config group set, should not change
    d.loadConfiguration();
    QCOMPARE(d.property(initValuePropName).toBool(), true);
    QCOMPARE(d.property(configValuePropName).toBool(), initValue == configValue);

    // set the group
    d.setConfig(inputConfig);
    d.loadConfiguration();
    QCOMPARE(d.property(initValuePropName).toBool(), initValue == configValue);
    QCOMPARE(d.property(configValuePropName).toBool(), true);
    QCOMPARE(dbusProperty<bool>(d.sysName(), initValuePropName), initValue == configValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), configValuePropName), true);

    // and try to store
    if (configValue != initValue) {
        d.setProperty(initValuePropName, true);
        QCOMPARE(inputConfig.readEntry("ClickMethod", configValue), initValue);
    }
}

void TestLibinputDevice::testLoadTapToClick_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("configValue");

    QTest::newRow("false -> true") << false << true;
    QTest::newRow("true -> false") << true << false;
    QTest::newRow("true -> true") << true << true;
    QTest::newRow("false -> false") << false << false;
}

void TestLibinputDevice::testLoadTapToClick()
{
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup inputConfig(config, QStringLiteral("Test"));
    QFETCH(bool, configValue);
    QFETCH(bool, initValue);
    inputConfig.writeEntry("TapToClick", configValue);

    libinput_device device;
    device.tapFingerCount = 2;
    device.tapToClick = initValue;
    device.setTapToClickReturnValue = false;

    Device d(&device);
    QCOMPARE(d.isTapToClick(), initValue);
    // no config group set, should not change
    d.loadConfiguration();
    QCOMPARE(d.isTapToClick(), initValue);

    // set the group
    d.setConfig(inputConfig);
    d.loadConfiguration();
    QCOMPARE(d.isTapToClick(), configValue);

    // and try to store
    if (configValue != initValue) {
        d.setTapToClick(initValue);
        QCOMPARE(inputConfig.readEntry("TapToClick", configValue), initValue);
    }
}

void TestLibinputDevice::testLoadTapAndDrag_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("configValue");

    QTest::newRow("false -> true") << false << true;
    QTest::newRow("true -> false") << true << false;
    QTest::newRow("true -> true") << true << true;
    QTest::newRow("false -> false") << false << false;
}

void TestLibinputDevice::testLoadTapAndDrag()
{
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup inputConfig(config, QStringLiteral("Test"));
    QFETCH(bool, configValue);
    QFETCH(bool, initValue);
    inputConfig.writeEntry("TapAndDrag", configValue);

    libinput_device device;
    device.tapAndDrag = initValue;
    device.setTapAndDragReturnValue = false;

    Device d(&device);
    QCOMPARE(d.isTapAndDrag(), initValue);
    // no config group set, should not change
    d.loadConfiguration();
    QCOMPARE(d.isTapAndDrag(), initValue);

    // set the group
    d.setConfig(inputConfig);
    d.loadConfiguration();
    QCOMPARE(d.isTapAndDrag(), configValue);

    // and try to store
    if (configValue != initValue) {
        d.setTapAndDrag(initValue);
        QCOMPARE(inputConfig.readEntry("TapAndDrag", configValue), initValue);
    }
}

void TestLibinputDevice::testLoadTapDragLock_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("configValue");

    QTest::newRow("false -> true") << false << true;
    QTest::newRow("true -> false") << true << false;
    QTest::newRow("true -> true") << true << true;
    QTest::newRow("false -> false") << false << false;
}

void TestLibinputDevice::testLoadTapDragLock()
{
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup inputConfig(config, QStringLiteral("Test"));
    QFETCH(bool, configValue);
    QFETCH(bool, initValue);
    inputConfig.writeEntry("TapDragLock", configValue);

    libinput_device device;
    device.tapDragLock = initValue;
    device.setTapDragLockReturnValue = false;

    Device d(&device);
    QCOMPARE(d.isTapDragLock(), initValue);
    // no config group set, should not change
    d.loadConfiguration();
    QCOMPARE(d.isTapDragLock(), initValue);

    // set the group
    d.setConfig(inputConfig);
    d.loadConfiguration();
    QCOMPARE(d.isTapDragLock(), configValue);

    // and try to store
    if (configValue != initValue) {
        d.setTapDragLock(initValue);
        QCOMPARE(inputConfig.readEntry("TapDragLock", configValue), initValue);
    }
}

void TestLibinputDevice::testLoadMiddleButtonEmulation_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("configValue");

    QTest::newRow("false -> true") << false << true;
    QTest::newRow("true -> false") << true << false;
    QTest::newRow("true -> true") << true << true;
    QTest::newRow("false -> false") << false << false;
}

void TestLibinputDevice::testLoadMiddleButtonEmulation()
{
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup inputConfig(config, QStringLiteral("Test"));
    QFETCH(bool, configValue);
    QFETCH(bool, initValue);
    inputConfig.writeEntry("MiddleButtonEmulation", configValue);

    libinput_device device;
    device.supportsMiddleEmulation = true;
    device.middleEmulation = initValue;
    device.setMiddleEmulationReturnValue = false;

    Device d(&device);
    QCOMPARE(d.isMiddleEmulation(), initValue);
    // no config group set, should not change
    d.loadConfiguration();
    QCOMPARE(d.isMiddleEmulation(), initValue);

    // set the group
    d.setConfig(inputConfig);
    d.loadConfiguration();
    QCOMPARE(d.isMiddleEmulation(), configValue);

    // and try to store
    if (configValue != initValue) {
        d.setMiddleEmulation(initValue);
        QCOMPARE(inputConfig.readEntry("MiddleButtonEmulation", configValue), initValue);
    }
}

void TestLibinputDevice::testLoadNaturalScroll_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("configValue");

    QTest::newRow("false -> true") << false << true;
    QTest::newRow("true -> false") << true << false;
    QTest::newRow("true -> true") << true << true;
    QTest::newRow("false -> false") << false << false;
}

void TestLibinputDevice::testLoadNaturalScroll()
{
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup inputConfig(config, QStringLiteral("Test"));
    QFETCH(bool, configValue);
    QFETCH(bool, initValue);
    inputConfig.writeEntry("NaturalScroll", configValue);

    libinput_device device;
    device.supportsNaturalScroll = true;
    device.naturalScroll = initValue;
    device.setNaturalScrollReturnValue = false;

    Device d(&device);
    QCOMPARE(d.isNaturalScroll(), initValue);
    // no config group set, should not change
    d.loadConfiguration();
    QCOMPARE(d.isNaturalScroll(), initValue);

    // set the group
    d.setConfig(inputConfig);
    d.loadConfiguration();
    QCOMPARE(d.isNaturalScroll(), configValue);

    // and try to store
    if (configValue != initValue) {
        d.setNaturalScroll(initValue);
        QCOMPARE(inputConfig.readEntry("NaturalScroll", configValue), initValue);
    }
}

void TestLibinputDevice::testLoadScrollMethod_data()
{
    QTest::addColumn<quint32>("initValue");
    QTest::addColumn<QString>("initValuePropNameString");
    QTest::addColumn<quint32>("configValue");
    QTest::addColumn<QString>("configValuePropNameString");

    QTest::newRow("scrollTwoFinger -> scrollEdge") << (quint32)LIBINPUT_CONFIG_SCROLL_2FG << "scrollTwoFinger" << (quint32)LIBINPUT_CONFIG_SCROLL_EDGE << "scrollEdge";
    QTest::newRow("scrollOnButtonDown -> scrollTwoFinger") << (quint32)LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN << "scrollOnButtonDown" << (quint32)LIBINPUT_CONFIG_SCROLL_2FG << "scrollTwoFinger";
    QTest::newRow("scrollEdge -> scrollEdge") << (quint32)LIBINPUT_CONFIG_SCROLL_EDGE << "scrollEdge" << (quint32)LIBINPUT_CONFIG_SCROLL_EDGE << "scrollEdge";
}

void TestLibinputDevice::testLoadScrollMethod()
{
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup inputConfig(config, QStringLiteral("Test"));
    QFETCH(quint32, initValue);
    QFETCH(quint32, configValue);
    QFETCH(QString, initValuePropNameString);
    QFETCH(QString, configValuePropNameString);

    QByteArray initValuePropName = initValuePropNameString.toLatin1();
    QByteArray configValuePropName = configValuePropNameString.toLatin1();

    inputConfig.writeEntry("ScrollMethod", configValue);

    libinput_device device;
    device.supportedScrollMethods = LIBINPUT_CONFIG_SCROLL_2FG | LIBINPUT_CONFIG_SCROLL_EDGE | LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
    device.scrollMethod = (libinput_config_scroll_method)initValue;
    device.setScrollMethodReturnValue = false;

    Device d(&device);
    QCOMPARE(d.property(initValuePropName).toBool(), true);
    QCOMPARE(d.property(configValuePropName).toBool(), initValue == configValue);
    // no config group set, should not change
    d.loadConfiguration();
    QCOMPARE(d.property(initValuePropName).toBool(), true);
    QCOMPARE(d.property(configValuePropName).toBool(), initValue == configValue);

    // set the group
    d.setConfig(inputConfig);
    d.loadConfiguration();
    QCOMPARE(d.property(initValuePropName).toBool(), initValue == configValue);
    QCOMPARE(d.property(configValuePropName).toBool(), true);

    // and try to store
    if (configValue != initValue) {
        d.setProperty(initValuePropName, true);
        QCOMPARE(inputConfig.readEntry("ScrollMethod", configValue), initValue);
    }
}

void TestLibinputDevice::testLoadScrollButton_data()
{
    QTest::addColumn<quint32>("initValue");
    QTest::addColumn<quint32>("configValue");

    QTest::newRow("BTN_LEFT -> BTN_RIGHT") << quint32(BTN_LEFT) << quint32(BTN_RIGHT);
    QTest::newRow("BTN_LEFT -> BTN_LEFT") << quint32(BTN_LEFT) << quint32(BTN_LEFT);
}

void TestLibinputDevice::testLoadScrollButton()
{
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup inputConfig(config, QStringLiteral("Test"));
    QFETCH(quint32, configValue);
    QFETCH(quint32, initValue);
    inputConfig.writeEntry("ScrollButton", configValue);

    libinput_device device;
    device.supportedScrollMethods = LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
    device.scrollMethod = LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
    device.scrollButton = initValue;
    device.setScrollButtonReturnValue = false;

    Device d(&device);
    QCOMPARE(d.isScrollOnButtonDown(), true);
    QCOMPARE(d.scrollButton(), initValue);
    // no config group set, should not change
    d.loadConfiguration();
    QCOMPARE(d.isScrollOnButtonDown(), true);
    QCOMPARE(d.scrollButton(), initValue);

    // set the group
    d.setConfig(inputConfig);
    d.loadConfiguration();
    QCOMPARE(d.isScrollOnButtonDown(), true);
    QCOMPARE(d.scrollButton(), configValue);

    // and try to store
    if (configValue != initValue) {
        d.setScrollButton(initValue);
        QCOMPARE(inputConfig.readEntry("ScrollButton", configValue), initValue);
    }
}

void TestLibinputDevice::testLoadLeftHanded_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("configValue");

    QTest::newRow("false -> true") << false << true;
    QTest::newRow("true -> false") << true << false;
    QTest::newRow("true -> true") << true << true;
    QTest::newRow("false -> false") << false << false;
}

void TestLibinputDevice::testLoadLeftHanded()
{
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup inputConfig(config, QStringLiteral("Test"));
    QFETCH(bool, configValue);
    QFETCH(bool, initValue);
    inputConfig.writeEntry("LeftHanded", configValue);

    libinput_device device;
    device.supportsLeftHanded = true;
    device.leftHanded = initValue;
    device.setLeftHandedReturnValue = false;

    Device d(&device);
    QCOMPARE(d.isLeftHanded(), initValue);
    // no config group set, should not change
    d.loadConfiguration();
    QCOMPARE(d.isLeftHanded(), initValue);

    // set the group
    d.setConfig(inputConfig);
    d.loadConfiguration();
    QCOMPARE(d.isLeftHanded(), configValue);

    // and try to store
    if (configValue != initValue) {
        d.setLeftHanded(initValue);
        QCOMPARE(inputConfig.readEntry("LeftHanded", configValue), initValue);
    }
}

void TestLibinputDevice::testLoadDisableWhileTyping_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("configValue");

    QTest::newRow("false -> true") << false << true;
    QTest::newRow("true -> false") << true << false;
    QTest::newRow("true -> true") << true << true;
    QTest::newRow("false -> false") << false << false;
}

void TestLibinputDevice::testLoadDisableWhileTyping()
{
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup inputConfig(config, QStringLiteral("Test"));
    QFETCH(bool, configValue);
    QFETCH(bool, initValue);
    inputConfig.writeEntry("DisableWhileTyping", configValue);

    libinput_device device;
    device.supportsDisableWhileTyping = true;
    device.disableWhileTyping = initValue ? LIBINPUT_CONFIG_DWT_ENABLED : LIBINPUT_CONFIG_DWT_DISABLED;
    device.setDisableWhileTypingReturnValue = false;

    Device d(&device);
    QCOMPARE(d.isDisableWhileTyping(), initValue);
    // no config group set, should not change
    d.loadConfiguration();
    QCOMPARE(d.isDisableWhileTyping(), initValue);

    // set the group
    d.setConfig(inputConfig);
    d.loadConfiguration();
    QCOMPARE(d.isDisableWhileTyping(), configValue);

    // and try to store
    if (configValue != initValue) {
        d.setDisableWhileTyping(initValue);
        QCOMPARE(inputConfig.readEntry("DisableWhileTyping", configValue), initValue);
    }
}

void TestLibinputDevice::testLoadLmrTapButtonMap_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("configValue");

    QTest::newRow("false -> true") << false << true;
    QTest::newRow("true -> false") << true << false;
    QTest::newRow("true -> true") << true << true;
    QTest::newRow("false -> false") << false << false;
}

void TestLibinputDevice::testLoadLmrTapButtonMap()
{
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup inputConfig(config, QStringLiteral("Test"));
    QFETCH(bool, configValue);
    QFETCH(bool, initValue);
    inputConfig.writeEntry("LmrTapButtonMap", configValue);

    libinput_device device;
    device.tapFingerCount = 3;
    device.tapButtonMap = initValue ? LIBINPUT_CONFIG_TAP_MAP_LMR : LIBINPUT_CONFIG_TAP_MAP_LRM;
    device.setTapButtonMapReturnValue = false;

    Device d(&device);
    QCOMPARE(d.lmrTapButtonMap(), initValue);
    // no config group set, should not change
    d.loadConfiguration();
    QCOMPARE(d.lmrTapButtonMap(), initValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "lmrTapButtonMap"), initValue);

    // set the group
    d.setConfig(inputConfig);
    d.loadConfiguration();
    QCOMPARE(d.lmrTapButtonMap(), configValue);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "lmrTapButtonMap"), configValue);

    // and try to store
    if (configValue != initValue) {
        d.setLmrTapButtonMap(initValue);
        QCOMPARE(inputConfig.readEntry("LmrTapButtonMap", configValue), initValue);
    }
}

void TestLibinputDevice::testOrientation_data()
{
    QTest::addColumn<Qt::ScreenOrientation>("orientation");
    QTest::addColumn<float>("m11");
    QTest::addColumn<float>("m12");
    QTest::addColumn<float>("m13");
    QTest::addColumn<float>("m21");
    QTest::addColumn<float>("m22");
    QTest::addColumn<float>("m23");
    QTest::addColumn<bool>("defaultIsIdentity");

    QTest::newRow("Primary") << Qt::PrimaryOrientation << 1.0f << 2.0f << 3.0f << 4.0f << 5.0f << 6.0f << false;
    QTest::newRow("Landscape") << Qt::LandscapeOrientation << 1.0f << 2.0f << 3.0f << 4.0f << 5.0f << 6.0f << false;
    QTest::newRow("Portrait") << Qt::PortraitOrientation << 0.0f << -1.0f << 1.0f << 1.0f << 0.0f << 0.0f << true;
    QTest::newRow("InvertedLandscape") << Qt::InvertedLandscapeOrientation << -1.0f << 0.0f << 1.0f << 0.0f << -1.0f << 1.0f << true;
    QTest::newRow("InvertedPortrait") << Qt::InvertedPortraitOrientation << 0.0f << 1.0f << 0.0f << -1.0f << 0.0f << 1.0f << true;
}

void TestLibinputDevice::testOrientation()
{
    libinput_device device;
    device.supportsCalibrationMatrix = true;
    device.defaultCalibrationMatrix = std::array<float, 6>{{1.0, 2.0, 3.0, 4.0, 5.0, 6.0}};
    QFETCH(bool, defaultIsIdentity);
    device.defaultCalibrationMatrixIsIdentity = defaultIsIdentity;
    Device d(&device);
    QFETCH(Qt::ScreenOrientation, orientation);
    d.setOrientation(orientation);
    QTEST(device.calibrationMatrix[0], "m11");
    QTEST(device.calibrationMatrix[1], "m12");
    QTEST(device.calibrationMatrix[2], "m13");
    QTEST(device.calibrationMatrix[3], "m21");
    QTEST(device.calibrationMatrix[4], "m22");
    QTEST(device.calibrationMatrix[5], "m23");
}

void TestLibinputDevice::testCalibrationWithDefault()
{
    libinput_device device;
    device.supportsCalibrationMatrix = true;
    device.defaultCalibrationMatrix = std::array<float, 6>{{2.0, 3.0, 0.0, 4.0, 5.0, 0.0}};
    device.defaultCalibrationMatrixIsIdentity = false;
    Device d(&device);
    d.setOrientation(Qt::PortraitOrientation);
    QCOMPARE(device.calibrationMatrix[0], 3.0f);
    QCOMPARE(device.calibrationMatrix[1], -2.0f);
    QCOMPARE(device.calibrationMatrix[2], 2.0f);
    QCOMPARE(device.calibrationMatrix[3], 5.0f);
    QCOMPARE(device.calibrationMatrix[4], -4.0f);
    QCOMPARE(device.calibrationMatrix[5], 4.0f);
}

void TestLibinputDevice::testSwitch_data()
{
    QTest::addColumn<bool>("lid");
    QTest::addColumn<bool>("tablet");

    QTest::newRow("lid") << true << false;
    QTest::newRow("tablet") << false << true;
}

void TestLibinputDevice::testSwitch()
{
    libinput_device device;
    device.switchDevice = true;
    QFETCH(bool, lid);
    QFETCH(bool, tablet);
    device.lidSwitch = lid;
    device.tabletModeSwitch = tablet;

    Device d(&device);
    QCOMPARE(d.isSwitch(), true);
    QCOMPARE(d.isLidSwitch(), lid);
    QCOMPARE(d.property("lidSwitch").toBool(), lid);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "lidSwitch"), lid);
    QCOMPARE(d.isTabletModeSwitch(), tablet);
    QCOMPARE(d.property("tabletModeSwitch").toBool(), tablet);
    QCOMPARE(dbusProperty<bool>(d.sysName(), "tabletModeSwitch"), tablet);
}

QTEST_GUILESS_MAIN(TestLibinputDevice)
#include "device_test.moc"
