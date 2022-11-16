/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "core/outputbackend.h"
#include "main.h"
#include "virtualkeyboard_dbus.h"
#include "wayland_server.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingReply>
#include <QSignalSpy>
#include <QTest>

#include <virtualkeyboardinterface.h>

using KWin::VirtualKeyboardDBus;
using namespace KWin;
using namespace KWin::Test;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_virtualkeyboarddbus-0");

class VirtualKeyboardDBusTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testEnabled();
    void testRequestEnabled_data();
    void testRequestEnabled();
    void init();
    void cleanup();
};

void VirtualKeyboardDBusTest::initTestCase()
{
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.kwin.testvirtualkeyboard"));
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    static_cast<WaylandTestApplication *>(kwinApp())->setInputMethodServerToStart("internal");
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());

    QVERIFY(setupWaylandConnection(AdditionalWaylandInterface::Seat | AdditionalWaylandInterface::InputMethodV1 | AdditionalWaylandInterface::TextInputManagerV2 | AdditionalWaylandInterface::TextInputManagerV3));
}

void VirtualKeyboardDBusTest::init()
{
    kwinApp()->inputMethod()->setEnabled(false);
}

void VirtualKeyboardDBusTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void VirtualKeyboardDBusTest::testEnabled()
{
    VirtualKeyboardDBus dbus(KWin::kwinApp()->inputMethod());
    OrgKdeKwinVirtualKeyboardInterface iface(QStringLiteral("org.kde.kwin.testvirtualkeyboard"), QStringLiteral("/VirtualKeyboard"), QDBusConnection::sessionBus());
    QSignalSpy helperChangedSpy(&iface, &OrgKdeKwinVirtualKeyboardInterface::enabledChanged);

    QCOMPARE(dbus.isEnabled(), false);
    QCOMPARE(dbus.property("enabled").toBool(), false);
    QSignalSpy enabledChangedSpy(&dbus, &VirtualKeyboardDBus::enabledChanged);

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

    QTest::newRow("enable") << QStringLiteral("setEnabled") << true;
    QTest::newRow("disable") << QStringLiteral("setEnabled") << false;
}

void VirtualKeyboardDBusTest::testRequestEnabled()
{
    QFETCH(QString, method);
    QFETCH(bool, expectedResult);

    VirtualKeyboardDBus dbus(KWin::kwinApp()->inputMethod());
    OrgKdeKwinVirtualKeyboardInterface iface(QStringLiteral("org.kde.kwin.testvirtualkeyboard"), QStringLiteral("/VirtualKeyboard"), QDBusConnection::sessionBus());

    iface.setEnabled(expectedResult);
    QTRY_COMPARE(iface.enabled(), expectedResult);
}

WAYLANDTEST_MAIN(VirtualKeyboardDBusTest)
#include "test_virtualkeyboard_dbus.moc"
