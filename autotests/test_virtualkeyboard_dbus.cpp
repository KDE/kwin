/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QTest>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingReply>
#include <QSignalSpy>

#include <virtualkeyboardinterface.h>
#include "virtualkeyboard_dbus.h"

using KWin::VirtualKeyboardDBus;

class VirtualKeyboardDBusTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testEnabled();
    void testRequestEnabled_data();
    void testRequestEnabled();
};

void VirtualKeyboardDBusTest::initTestCase()
{
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.kwin.testvirtualkeyboard"));
}

void VirtualKeyboardDBusTest::testEnabled()
{
    VirtualKeyboardDBus dbus;
    OrgKdeKwinVirtualKeyboardInterface iface(QStringLiteral("org.kde.kwin.testvirtualkeyboard"), QStringLiteral("/VirtualKeyboard"), QDBusConnection::sessionBus());
    QSignalSpy helperChangedSpy(&iface, &OrgKdeKwinVirtualKeyboardInterface::enabledChanged);
    QVERIFY(helperChangedSpy.isValid());

    QCOMPARE(dbus.isEnabled(), false);
    QCOMPARE(dbus.property("enabled").toBool(), false);
    QSignalSpy enabledChangedSpy(&dbus, &VirtualKeyboardDBus::enabledChanged);
    QVERIFY(enabledChangedSpy.isValid());

    QVERIFY(iface.isValid());
    QCOMPARE(iface.enabled(), false);

    dbus.setEnabled(true);
    QCOMPARE(enabledChangedSpy.count(), 1);
    QVERIFY(helperChangedSpy.wait());
    QCOMPARE(helperChangedSpy.count(), 1);
    QCOMPARE(dbus.isEnabled(), true);
    QCOMPARE(dbus.property("enabled").toBool(), true);
    QCOMPARE(iface.enabled(), true);

    // setting again to enabled should not change anything
    dbus.setEnabled(true);
    QCOMPARE(enabledChangedSpy.count(), 1);

    // back to false
    dbus.setEnabled(false);
    QCOMPARE(enabledChangedSpy.count(), 2);
    QVERIFY(helperChangedSpy.wait());
    QCOMPARE(helperChangedSpy.count(), 2);
    QCOMPARE(dbus.isEnabled(), false);
    QCOMPARE(dbus.property("enabled").toBool(), false);
    QCOMPARE(iface.enabled(), false);
}

void VirtualKeyboardDBusTest::testRequestEnabled_data()
{
    QTest::addColumn<QString>("method");
    QTest::addColumn<bool>("expectedResult");

    QTest::newRow("enable") << QStringLiteral("requestEnabled") << true;
    QTest::newRow("disable") << QStringLiteral("requestEnabled") << false;
}

void VirtualKeyboardDBusTest::testRequestEnabled()
{
    QFETCH(QString, method);
    QFETCH(bool, expectedResult);

    VirtualKeyboardDBus dbus;
    dbus.setEnabled(!expectedResult);
    connect(&dbus, &VirtualKeyboardDBus::enableRequested, &dbus, &VirtualKeyboardDBus::setEnabled);
    QSignalSpy activateRequestedSpy(&dbus, &VirtualKeyboardDBus::enabledChanged);
    QVERIFY(activateRequestedSpy.isValid());
    OrgKdeKwinVirtualKeyboardInterface iface(QStringLiteral("org.kde.kwin.testvirtualkeyboard"), QStringLiteral("/VirtualKeyboard"), QDBusConnection::sessionBus());
    
    auto pending = iface.requestEnabled(expectedResult);
    QVERIFY(!pending.isError());
//     activateRequestedSpy.wait();
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(dbus.isEnabled(), expectedResult);
    QCOMPARE(iface.enabled(), expectedResult);
}

QTEST_GUILESS_MAIN(VirtualKeyboardDBusTest)
#include "test_virtualkeyboard_dbus.moc"
