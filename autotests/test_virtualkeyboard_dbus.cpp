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

#include "../virtualkeyboard_dbus.h"

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

class DbusPropertyHelper : public QObject
{
    Q_OBJECT
public:
    DbusPropertyHelper()
        : QObject(nullptr)
    {
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.kde.kwin.testvirtualkeyboard"),
            QStringLiteral("/VirtualKeyboard"),
            QStringLiteral("org.kde.kwin.VirtualKeyboard"),
            QStringLiteral("enabledChanged"),
            this,
            SLOT(slotEnabledChanged()));
    }
    ~DbusPropertyHelper() override = default;

Q_SIGNALS:
    void enabledChanged();

private Q_SLOTS:
    void slotEnabledChanged() {
        emit enabledChanged();
    }
};

void VirtualKeyboardDBusTest::initTestCase()
{
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.kwin.testvirtualkeyboard"));
}

void VirtualKeyboardDBusTest::testEnabled()
{
    VirtualKeyboardDBus dbus;
    DbusPropertyHelper helper;
    QSignalSpy helperChangedSpy(&helper, &DbusPropertyHelper::enabledChanged);
    QVERIFY(helperChangedSpy.isValid());

    QCOMPARE(dbus.isEnabled(), false);
    QCOMPARE(dbus.property("enabled").toBool(), false);
    QSignalSpy enabledChangedSpy(&dbus, &VirtualKeyboardDBus::enabledChanged);
    QVERIFY(enabledChangedSpy.isValid());

    auto readProperty = [] (bool enabled) {
        const QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kwin.testvirtualkeyboard"),
                                                                    QStringLiteral("/VirtualKeyboard"),
                                                                    QStringLiteral("org.kde.kwin.VirtualKeyboard"),
                                                                    QStringLiteral("isEnabled"));
        const auto reply = QDBusConnection::sessionBus().call(message);
        QCOMPARE(reply.type(), QDBusMessage::ReplyMessage);
        QCOMPARE(reply.arguments().count(), 1);
        QCOMPARE(reply.arguments().first().toBool(), enabled);
    };
    readProperty(false);

    dbus.setEnabled(true);
    QCOMPARE(enabledChangedSpy.count(), 1);
    QVERIFY(helperChangedSpy.wait());
    QCOMPARE(helperChangedSpy.count(), 1);
    QCOMPARE(dbus.isEnabled(), true);
    QCOMPARE(dbus.property("enabled").toBool(), true);
    readProperty(true);

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
    readProperty(false);
}

void VirtualKeyboardDBusTest::testRequestEnabled_data()
{
    QTest::addColumn<QString>("method");
    QTest::addColumn<bool>("expectedResult");

    QTest::newRow("enable") << QStringLiteral("enable") << true;
    QTest::newRow("disable") << QStringLiteral("disable") << false;
}

void VirtualKeyboardDBusTest::testRequestEnabled()
{
    VirtualKeyboardDBus dbus;
    QSignalSpy activateRequestedSpy(&dbus, &VirtualKeyboardDBus::activateRequested);
    QVERIFY(activateRequestedSpy.isValid());
    QFETCH(QString, method);
    const QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kwin.testvirtualkeyboard"),
                                                                QStringLiteral("/VirtualKeyboard"),
                                                                QStringLiteral("org.kde.kwin.VirtualKeyboard"),
                                                                method);
    QDBusConnection::sessionBus().asyncCall(message);
    QTRY_COMPARE(activateRequestedSpy.count(), 1);
    QTEST(activateRequestedSpy.first().first().toBool(), "expectedResult");
}

QTEST_GUILESS_MAIN(VirtualKeyboardDBusTest)
#include "test_virtualkeyboard_dbus.moc"
