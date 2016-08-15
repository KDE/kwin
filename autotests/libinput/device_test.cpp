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

QTEST_GUILESS_MAIN(TestLibinputDevice)
#include "device_test.moc"
