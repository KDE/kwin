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
#include "mock_libinput.h"
#include "../../libinput/device.h"
#include <config-kwin.h>

#include <KSharedConfig>

#include <QtTest/QtTest>

#include <linux/input.h>

using namespace KWin::LibInput;

class TestLibinputDevice : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testStaticGetter();
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
    void testScrollTwoFinger_data();
    void testScrollTwoFinger();
    void testScrollEdge_data();
    void testScrollEdge();
    void testScrollButtonDown_data();
    void testScrollButtonDown();
    void testScrollButton_data();
    void testScrollButton();
    void testLoadEnabled_data();
    void testLoadEnabled();
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
    void testLoadScrollTwoFinger_data();
    void testLoadScrollTwoFinger();
    void testLoadScrollEdge_data();
    void testLoadScrollEdge();
    void testLoadScrollOnButton_data();
    void testLoadScrollOnButton();
    void testLoadScrollButton_data();
    void testLoadScrollButton();
    void testLoadLeftHanded_data();
    void testLoadLeftHanded();
};

void TestLibinputDevice::testStaticGetter()
{
    // this test verifies that the static getter for Device works as expected
    QVERIFY(Device::devices().isEmpty());

    // create some device
    libinput_device device1;
    libinput_device device2;
    // at the moment not yet known to Device
    QVERIFY(!Device::getDevice(&device1));
    QVERIFY(!Device::getDevice(&device2));
    QVERIFY(Device::devices().isEmpty());

    // now create a Device for one
    Device *d1 = new Device(&device1);
    QCOMPARE(Device::devices().count(), 1);
    QCOMPARE(Device::devices().first(), d1);
    QCOMPARE(Device::getDevice(&device1), d1);
    QVERIFY(!Device::getDevice(&device2));

    // and a second Device
    Device *d2 = new Device(&device2);
    QCOMPARE(Device::devices().count(), 2);
    QCOMPARE(Device::devices().first(), d1);
    QCOMPARE(Device::devices().last(), d2);
    QCOMPARE(Device::getDevice(&device1), d1);
    QCOMPARE(Device::getDevice(&device2), d2);

    // now delete d1
    delete d1;
    QCOMPARE(Device::devices().count(), 1);
    QCOMPARE(Device::devices().first(), d2);
    QCOMPARE(Device::getDevice(&device2), d2);
    QVERIFY(!Device::getDevice(&device1));

    // and delete d2
    delete d2;
    QVERIFY(!Device::getDevice(&device1));
    QVERIFY(!Device::getDevice(&device2));
    QVERIFY(Device::devices().isEmpty());
}

void TestLibinputDevice::testDeviceType_data()
{
    QTest::addColumn<bool>("keyboard");
    QTest::addColumn<bool>("pointer");
    QTest::addColumn<bool>("touch");
    QTest::addColumn<bool>("tabletTool");

    QTest::newRow("keyboard") << true << false << false << false;
    QTest::newRow("pointer") << false << true << false << false;
    QTest::newRow("touch") << false << false << true << false;
    QTest::newRow("keyboard/pointer") << true << true << false << false;
    QTest::newRow("keyboard/touch") << true << false << true << false;
    QTest::newRow("pointer/touch") << false << true << true << false;
    QTest::newRow("keyboard/pointer/touch") << true << true << true << false;
    QTest::newRow("tabletTool") << false << false << false << true;
}

void TestLibinputDevice::testDeviceType()
{
    // this test verifies that the device type is recognized correctly
    QFETCH(bool, keyboard);
    QFETCH(bool, pointer);
    QFETCH(bool, touch);
    QFETCH(bool, tabletTool);

    libinput_device device;
    device.keyboard = keyboard;
    device.pointer = pointer;
    device.touch = touch;
    device.tabletTool = tabletTool;

    Device d(&device);
    QCOMPARE(d.isKeyboard(), keyboard);
    QCOMPARE(d.property("keyboard").toBool(), keyboard);
    QCOMPARE(d.isPointer(), pointer);
    QCOMPARE(d.property("pointer").toBool(), pointer);
    QCOMPARE(d.isTouch(), touch);
    QCOMPARE(d.property("touch").toBool(), touch);
    QCOMPARE(d.isTabletPad(), false);
    QCOMPARE(d.property("tabletPad").toBool(), false);
    QCOMPARE(d.isTabletTool(), tabletTool);
    QCOMPARE(d.property("tabletTool").toBool(), tabletTool);

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
    QCOMPARE(d.sysName().toUtf8(), sysName);
    QCOMPARE(d.property("sysName").toString().toUtf8(), sysName);
    QCOMPARE(d.outputName().toUtf8(), outputName);
    QCOMPARE(d.property("outputName").toString().toUtf8(), outputName);
}

void TestLibinputDevice::testProduct()
{
    // this test verifies the product property
    libinput_device device;
    device.product = 100u;
    Device d(&device);
    QCOMPARE(d.product(), 100u);
    QCOMPARE(d.property("product").toUInt(), 100u);
}

void TestLibinputDevice::testVendor()
{
    // this test verifies the vendor property
    libinput_device device;
    device.vendor = 200u;
    Device d(&device);
    QCOMPARE(d.vendor(), 200u);
    QCOMPARE(d.property("vendor").toUInt(), 200u);
}

void TestLibinputDevice::testTapFingerCount()
{
    // this test verifies the tap finger count property
    libinput_device device;
    device.tapFingerCount = 3;
    Device d(&device);
    QCOMPARE(d.tapFingerCount(), 3);
    QCOMPARE(d.property("tapFingerCount").toInt(), 3);
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

    QSignalSpy pointerAccelChangedSpy(&d, &Device::pointerAccelerationChanged);
    QVERIFY(pointerAccelChangedSpy.isValid());
    QFETCH(qreal, setAccel);
    d.setPointerAcceleration(setAccel);
    QTEST(d.pointerAcceleration(), "expectedAccel");
    QTEST(!pointerAccelChangedSpy.isEmpty(), "expectedChanged");
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
    QTest::newRow("false -> true") << true << false << false << true  << true;
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

    QSignalSpy leftHandedChangedSpy(&d, &Device::leftHandedChanged);
    QVERIFY(leftHandedChangedSpy.isValid());
    QFETCH(bool, setValue);
    d.setLeftHanded(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isLeftHanded(), expectedValue);
    QCOMPARE(leftHandedChangedSpy.isEmpty(), (supported && initValue) == expectedValue);
}

void TestLibinputDevice::testSupportedButtons_data()
{
    QTest::addColumn<bool>("isPointer");
    QTest::addColumn<Qt::MouseButtons>("setButtons");
    QTest::addColumn<Qt::MouseButtons>("expectedButtons");

    QTest::newRow("left")    << true << Qt::MouseButtons(Qt::LeftButton)    << Qt::MouseButtons(Qt::LeftButton);
    QTest::newRow("right")   << true << Qt::MouseButtons(Qt::RightButton)   << Qt::MouseButtons(Qt::RightButton);
    QTest::newRow("middle")  << true << Qt::MouseButtons(Qt::MiddleButton)  << Qt::MouseButtons(Qt::MiddleButton);
    QTest::newRow("extra1")  << true << Qt::MouseButtons(Qt::ExtraButton1)  << Qt::MouseButtons(Qt::ExtraButton1);
    QTest::newRow("extra2")  << true << Qt::MouseButtons(Qt::ExtraButton2)  << Qt::MouseButtons(Qt::ExtraButton2);
    QTest::newRow("back")    << true << Qt::MouseButtons(Qt::BackButton)    << Qt::MouseButtons(Qt::BackButton);
    QTest::newRow("forward") << true << Qt::MouseButtons(Qt::ForwardButton) << Qt::MouseButtons(Qt::ForwardButton);
    QTest::newRow("task")    << true << Qt::MouseButtons(Qt::TaskButton)    << Qt::MouseButtons(Qt::TaskButton);

    QTest::newRow("no pointer/left")    << false << Qt::MouseButtons(Qt::LeftButton)    << Qt::MouseButtons();
    QTest::newRow("no pointer/right")   << false << Qt::MouseButtons(Qt::RightButton)   << Qt::MouseButtons();
    QTest::newRow("no pointer/middle")  << false << Qt::MouseButtons(Qt::MiddleButton)  << Qt::MouseButtons();
    QTest::newRow("no pointer/extra1")  << false << Qt::MouseButtons(Qt::ExtraButton1)  << Qt::MouseButtons();
    QTest::newRow("no pointer/extra2")  << false << Qt::MouseButtons(Qt::ExtraButton2)  << Qt::MouseButtons();
    QTest::newRow("no pointer/back")    << false << Qt::MouseButtons(Qt::BackButton)    << Qt::MouseButtons();
    QTest::newRow("no pointer/forward") << false << Qt::MouseButtons(Qt::ForwardButton) << Qt::MouseButtons();
    QTest::newRow("no pointer/task")    << false << Qt::MouseButtons(Qt::TaskButton)    << Qt::MouseButtons();

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
    QTest::newRow("false -> true") << true << false << false << true  << true;
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

    QSignalSpy enabledChangedSpy(&d, &Device::enabledChanged);
    QVERIFY(enabledChangedSpy.isValid());
    QFETCH(bool, setValue);
    d.setEnabled(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isEnabled(), expectedValue);
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

    QSignalSpy tapToClickChangedSpy(&d, &Device::tapToClickChanged);
    QVERIFY(tapToClickChangedSpy.isValid());
    QFETCH(bool, setValue);
    d.setTapToClick(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isTapToClick(), expectedValue);
    QCOMPARE(tapToClickChangedSpy.isEmpty(), initValue == expectedValue);
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

    QSignalSpy tapAndDragChangedSpy(&d, &Device::tapAndDragChanged);
    QVERIFY(tapAndDragChangedSpy.isValid());
    QFETCH(bool, setValue);
    d.setTapAndDrag(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isTapAndDrag(), expectedValue);
    QCOMPARE(tapAndDragChangedSpy.isEmpty(), initValue == expectedValue);
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

    QSignalSpy tapDragLockChangedSpy(&d, &Device::tapDragLockChanged);
    QVERIFY(tapDragLockChangedSpy.isValid());
    QFETCH(bool, setValue);
    d.setTapDragLock(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isTapDragLock(), expectedValue);
    QCOMPARE(tapDragLockChangedSpy.isEmpty(), initValue == expectedValue);
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

    QSignalSpy middleEmulationChangedSpy(&d, &Device::middleEmulationChanged);
    QVERIFY(middleEmulationChangedSpy.isValid());
    QFETCH(bool, setValue);
    d.setMiddleEmulation(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isMiddleEmulation(), expectedValue);
    QCOMPARE(d.property("middleEmulation").toBool(), expectedValue);
    QCOMPARE(middleEmulationChangedSpy.isEmpty(), initValue == expectedValue);
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

    QSignalSpy naturalScrollChangedSpy(&d, &Device::naturalScrollChanged);
    QVERIFY(naturalScrollChangedSpy.isValid());
    QFETCH(bool, setValue);
    d.setNaturalScroll(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isNaturalScroll(), expectedValue);
    QCOMPARE(d.property("naturalScroll").toBool(), expectedValue);
    QCOMPARE(naturalScrollChangedSpy.isEmpty(), initValue == expectedValue);
}

void TestLibinputDevice::testScrollTwoFinger_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("setValue");
    QTest::addColumn<bool>("setShouldFail");
    QTest::addColumn<bool>("expectedValue");
    QTest::addColumn<bool>("supportsScrollTwoFinger");

    QTest::newRow("true -> false") << true << false << false << false << true;
    QTest::newRow("false -> true") << false << true << false << true << true;
    QTest::newRow("set fails") << true << false << true << true << true;
    QTest::newRow("true -> true") << true << true << false << true << true;
    QTest::newRow("false -> false") << false << false << false << false << true;

    QTest::newRow("false -> true, unsupported") << false << true << true << false << false;
}

void TestLibinputDevice::testScrollTwoFinger()
{
    libinput_device device;
    QFETCH(bool, initValue);
    QFETCH(bool, setShouldFail);
    QFETCH(bool, supportsScrollTwoFinger);
    device.supportedScrollMethods = supportsScrollTwoFinger ? LIBINPUT_CONFIG_SCROLL_2FG : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    device.scrollMethod = initValue ? LIBINPUT_CONFIG_SCROLL_2FG : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    device.setScrollMethodReturnValue = setShouldFail;

    Device d(&device);
    QCOMPARE(d.isScrollTwoFinger(), initValue);
    QCOMPARE(d.property("scrollTwoFinger").toBool(), initValue);

    QSignalSpy scrollMethodChangedSpy(&d, &Device::scrollMethodChanged);
    QVERIFY(scrollMethodChangedSpy.isValid());
    QFETCH(bool, setValue);
    d.setScrollTwoFinger(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isScrollTwoFinger(), expectedValue);
    QCOMPARE(d.property("scrollTwoFinger").toBool(), expectedValue);
    QCOMPARE(scrollMethodChangedSpy.isEmpty(), initValue == expectedValue);
}

void TestLibinputDevice::testScrollEdge_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("setValue");
    QTest::addColumn<bool>("setShouldFail");
    QTest::addColumn<bool>("expectedValue");
    QTest::addColumn<bool>("supportsScrollEdge");

    QTest::newRow("true -> false") << true << false << false << false << true;
    QTest::newRow("false -> true") << false << true << false << true << true;
    QTest::newRow("set fails") << true << false << true << true << true;
    QTest::newRow("true -> true") << true << true << false << true << true;
    QTest::newRow("false -> false") << false << false << false << false << true;

    QTest::newRow("false -> true, unsupported") << false << true << true << false << false;
}

void TestLibinputDevice::testScrollEdge()
{
    libinput_device device;
    QFETCH(bool, initValue);
    QFETCH(bool, setShouldFail);
    QFETCH(bool, supportsScrollEdge);
    device.supportedScrollMethods = supportsScrollEdge ? LIBINPUT_CONFIG_SCROLL_EDGE : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    device.scrollMethod = initValue ? LIBINPUT_CONFIG_SCROLL_EDGE : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    device.setScrollMethodReturnValue = setShouldFail;

    Device d(&device);
    QCOMPARE(d.isScrollEdge(), initValue);
    QCOMPARE(d.property("scrollEdge").toBool(), initValue);

    QSignalSpy scrollMethodChangedSpy(&d, &Device::scrollMethodChanged);
    QVERIFY(scrollMethodChangedSpy.isValid());
    QFETCH(bool, setValue);
    d.setScrollEdge(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isScrollEdge(), expectedValue);
    QCOMPARE(d.property("scrollEdge").toBool(), expectedValue);
    QCOMPARE(scrollMethodChangedSpy.isEmpty(), initValue == expectedValue);
}

void TestLibinputDevice::testScrollButtonDown_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("setValue");
    QTest::addColumn<bool>("setShouldFail");
    QTest::addColumn<bool>("expectedValue");
    QTest::addColumn<bool>("supportsScrollButtonDown");

    QTest::newRow("true -> false") << true << false << false << false << true;
    QTest::newRow("false -> true") << false << true << false << true << true;
    QTest::newRow("set fails") << true << false << true << true << true;
    QTest::newRow("true -> true") << true << true << false << true << true;
    QTest::newRow("false -> false") << false << false << false << false << true;

    QTest::newRow("false -> true, unsupported") << false << true << true << false << false;
}

void TestLibinputDevice::testScrollButtonDown()
{
    libinput_device device;
    QFETCH(bool, initValue);
    QFETCH(bool, setShouldFail);
    QFETCH(bool, supportsScrollButtonDown);
    device.supportedScrollMethods = supportsScrollButtonDown ? LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    device.scrollMethod = initValue ? LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    device.setScrollMethodReturnValue = setShouldFail;

    Device d(&device);
    QCOMPARE(d.isScrollOnButtonDown(), initValue);
    QCOMPARE(d.property("scrollOnButtonDown").toBool(), initValue);

    QSignalSpy scrollMethodChangedSpy(&d, &Device::scrollMethodChanged);
    QVERIFY(scrollMethodChangedSpy.isValid());
    QFETCH(bool, setValue);
    d.setScrollOnButtonDown(setValue);
    QFETCH(bool, expectedValue);
    QCOMPARE(d.isScrollOnButtonDown(), expectedValue);
    QCOMPARE(d.property("scrollOnButtonDown").toBool(), expectedValue);
    QCOMPARE(scrollMethodChangedSpy.isEmpty(), initValue == expectedValue);
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

    QSignalSpy scrollButtonChangedSpy(&d, &Device::scrollButtonChanged);
    QVERIFY(scrollButtonChangedSpy.isValid());
    QFETCH(quint32, setValue);
    d.setScrollButton(setValue);
    QFETCH(quint32, expectedValue);
    QCOMPARE(d.scrollButton(), expectedValue);
    QCOMPARE(d.property("scrollButton").value<quint32>(), expectedValue);
    QCOMPARE(scrollButtonChangedSpy.isEmpty(), initValue == expectedValue);
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

void TestLibinputDevice::testLoadScrollTwoFinger_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("configValue");

    QTest::newRow("false -> true") << false << true;
    QTest::newRow("true -> false") << true << false;
    QTest::newRow("true -> true") << true << true;
    QTest::newRow("false -> false") << false << false;
}

void TestLibinputDevice::testLoadScrollTwoFinger()
{
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup inputConfig(config, QStringLiteral("Test"));
    QFETCH(bool, configValue);
    QFETCH(bool, initValue);
    inputConfig.writeEntry("ScrollTwoFinger", configValue);

    libinput_device device;
    device.supportedScrollMethods = LIBINPUT_CONFIG_SCROLL_2FG;
    device.scrollMethod = initValue ? LIBINPUT_CONFIG_SCROLL_2FG : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    device.setScrollMethodReturnValue = false;

    Device d(&device);
    QCOMPARE(d.isScrollTwoFinger(), initValue);
    // no config group set, should not change
    d.loadConfiguration();
    QCOMPARE(d.isScrollTwoFinger(), initValue);

    // set the group
    d.setConfig(inputConfig);
    d.loadConfiguration();
    QCOMPARE(d.isScrollTwoFinger(), configValue);

    // and try to store
    if (configValue != initValue) {
        d.setScrollTwoFinger(initValue);
        QCOMPARE(inputConfig.readEntry("ScrollTwoFinger", configValue), initValue);
    }
}

void TestLibinputDevice::testLoadScrollEdge_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("configValue");

    QTest::newRow("false -> true") << false << true;
    QTest::newRow("true -> false") << true << false;
    QTest::newRow("true -> true") << true << true;
    QTest::newRow("false -> false") << false << false;
}

void TestLibinputDevice::testLoadScrollEdge()
{
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup inputConfig(config, QStringLiteral("Test"));
    QFETCH(bool, configValue);
    QFETCH(bool, initValue);
    inputConfig.writeEntry("ScrollEdge", configValue);

    libinput_device device;
    device.supportedScrollMethods = LIBINPUT_CONFIG_SCROLL_EDGE;
    device.scrollMethod = initValue ? LIBINPUT_CONFIG_SCROLL_EDGE : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    device.setScrollMethodReturnValue = false;

    Device d(&device);
    QCOMPARE(d.isScrollEdge(), initValue);
    // no config group set, should not change
    d.loadConfiguration();
    QCOMPARE(d.isScrollEdge(), initValue);

    // set the group
    d.setConfig(inputConfig);
    d.loadConfiguration();
    QCOMPARE(d.isScrollEdge(), configValue);

    // and try to store
    if (configValue != initValue) {
        d.setScrollEdge(initValue);
        QCOMPARE(inputConfig.readEntry("ScrollEdge", configValue), initValue);
    }
}

void TestLibinputDevice::testLoadScrollOnButton_data()
{
    QTest::addColumn<bool>("initValue");
    QTest::addColumn<bool>("configValue");

    QTest::newRow("false -> true") << false << true;
    QTest::newRow("true -> false") << true << false;
    QTest::newRow("true -> true") << true << true;
    QTest::newRow("false -> false") << false << false;
}

void TestLibinputDevice::testLoadScrollOnButton()
{
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup inputConfig(config, QStringLiteral("Test"));
    QFETCH(bool, configValue);
    QFETCH(bool, initValue);
    inputConfig.writeEntry("ScrollOnButton", configValue);

    libinput_device device;
    device.supportedScrollMethods = LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
    device.scrollMethod = initValue ? LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    device.setScrollMethodReturnValue = false;

    Device d(&device);
    QCOMPARE(d.isScrollOnButtonDown(), initValue);
    // no config group set, should not change
    d.loadConfiguration();
    QCOMPARE(d.isScrollOnButtonDown(), initValue);

    // set the group
    d.setConfig(inputConfig);
    d.loadConfiguration();
    QCOMPARE(d.isScrollOnButtonDown(), configValue);

    // and try to store
    if (configValue != initValue) {
        d.setScrollOnButtonDown(initValue);
        QCOMPARE(inputConfig.readEntry("ScrollOnButton", configValue), initValue);
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

QTEST_GUILESS_MAIN(TestLibinputDevice)
#include "device_test.moc"
