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

#include <QtTest/QtTest>

using namespace KWin::LibInput;

class TestLibinputDevice : public QObject
{
    Q_OBJECT
private Q_SLOTS:
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
    void testTapEnabledByDefault_data();
    void testTapEnabledByDefault();
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
    void testPointerAcceleration_data();
    void testPointerAcceleration();
    void testLeftHanded_data();
    void testLeftHanded();
};

void TestLibinputDevice::testDeviceType_data()
{
    QTest::addColumn<bool>("keyboard");
    QTest::addColumn<bool>("pointer");
    QTest::addColumn<bool>("touch");

    QTest::newRow("keyboard") << true << false << false;
    QTest::newRow("pointer") << false << true << false;
    QTest::newRow("touch") << false << false << true;
    QTest::newRow("keyboard/pointer") << true << true << false;
    QTest::newRow("keyboard/touch") << true << false << true;
    QTest::newRow("pointer/touch") << false << true << true;
    QTest::newRow("keyboard/pointer/touch") << true << true << true;
}

void TestLibinputDevice::testDeviceType()
{
    // this test verifies that the device type is recognized correctly
    QFETCH(bool, keyboard);
    QFETCH(bool, pointer);
    QFETCH(bool, touch);

    libinput_device device;
    device.keyboard = keyboard;
    device.pointer = pointer;
    device.touch = touch;

    Device d(&device);
    QCOMPARE(d.isKeyboard(), keyboard);
    QCOMPARE(d.property("keyboard").toBool(), keyboard);
    QCOMPARE(d.isPointer(), pointer);
    QCOMPARE(d.property("pointer").toBool(), pointer);
    QCOMPARE(d.isTouch(), touch);
    QCOMPARE(d.property("touch").toBool(), touch);
    QCOMPARE(d.isTabletPad(), false);
    QCOMPARE(d.property("tabletPad").toBool(), false);
    QCOMPARE(d.isTabletTool(), false);
    QCOMPARE(d.property("tabletTool").toBool(), false);

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

    QTest::newRow("empty") << QByteArray() << QByteArray() << QByteArray();
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
    QCOMPARE(d.tapEnabledByDefault(), enabled);
    QCOMPARE(d.property("tapEnabledByDefault").toBool(), enabled);
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

QTEST_GUILESS_MAIN(TestLibinputDevice)
#include "device_test.moc"
