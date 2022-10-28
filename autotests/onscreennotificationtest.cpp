/*
    SPDX-FileCopyrightText: 2016 Martin Graesslin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/

#include "onscreennotificationtest.h"

#include "input.h"
#include "onscreennotification.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QQmlEngine>
#include <QSignalSpy>
#include <QTest>

QTEST_MAIN(OnScreenNotificationTest);

namespace KWin
{

void InputRedirection::installInputEventSpy(InputEventSpy *spy)
{
}

void InputRedirection::uninstallInputEventSpy(InputEventSpy *spy)
{
}

InputRedirection *InputRedirection::s_self = nullptr;

}

using KWin::OnScreenNotification;

void OnScreenNotificationTest::show()
{
    OnScreenNotification notification;
    auto config = KSharedConfig::openConfig(QString(), KSharedConfig::SimpleConfig);
    KConfigGroup group = config->group("OnScreenNotification");
    group.writeEntry(QStringLiteral("QmlPath"), QString("/does/not/exist.qml"));
    group.sync();
    notification.setConfig(config);
    notification.setEngine(new QQmlEngine(&notification));
    notification.setMessage(QStringLiteral("Some text so that we see it in the test"));

    QSignalSpy visibleChangedSpy(&notification, &OnScreenNotification::visibleChanged);
    QCOMPARE(notification.isVisible(), false);
    notification.setVisible(true);
    QCOMPARE(notification.isVisible(), true);
    QCOMPARE(visibleChangedSpy.count(), 1);

    // show again should not trigger
    notification.setVisible(true);
    QCOMPARE(visibleChangedSpy.count(), 1);

    // timer should not have hidden
    QTest::qWait(500);
    QCOMPARE(notification.isVisible(), true);

    // hide again
    notification.setVisible(false);
    QCOMPARE(notification.isVisible(), false);
    QCOMPARE(visibleChangedSpy.count(), 2);

    // now show with timer
    notification.setTimeout(250);
    notification.setVisible(true);
    QCOMPARE(notification.isVisible(), true);
    QCOMPARE(visibleChangedSpy.count(), 3);
    QVERIFY(visibleChangedSpy.wait());
    QCOMPARE(notification.isVisible(), false);
    QCOMPARE(visibleChangedSpy.count(), 4);
}

void OnScreenNotificationTest::timeout()
{
    OnScreenNotification notification;
    QSignalSpy timeoutChangedSpy(&notification, &OnScreenNotification::timeoutChanged);
    QCOMPARE(notification.timeout(), 0);
    notification.setTimeout(1000);
    QCOMPARE(notification.timeout(), 1000);
    QCOMPARE(timeoutChangedSpy.count(), 1);
    notification.setTimeout(1000);
    QCOMPARE(timeoutChangedSpy.count(), 1);
    notification.setTimeout(0);
    QCOMPARE(notification.timeout(), 0);
    QCOMPARE(timeoutChangedSpy.count(), 2);
}

void OnScreenNotificationTest::iconName()
{
    OnScreenNotification notification;
    QSignalSpy iconNameChangedSpy(&notification, &OnScreenNotification::iconNameChanged);
    QCOMPARE(notification.iconName(), QString());
    notification.setIconName(QStringLiteral("foo"));
    QCOMPARE(notification.iconName(), QStringLiteral("foo"));
    QCOMPARE(iconNameChangedSpy.count(), 1);
    notification.setIconName(QStringLiteral("foo"));
    QCOMPARE(iconNameChangedSpy.count(), 1);
    notification.setIconName(QStringLiteral("bar"));
    QCOMPARE(notification.iconName(), QStringLiteral("bar"));
    QCOMPARE(iconNameChangedSpy.count(), 2);
}

void OnScreenNotificationTest::message()
{
    OnScreenNotification notification;
    QSignalSpy messageChangedSpy(&notification, &OnScreenNotification::messageChanged);
    QCOMPARE(notification.message(), QString());
    notification.setMessage(QStringLiteral("foo"));
    QCOMPARE(notification.message(), QStringLiteral("foo"));
    QCOMPARE(messageChangedSpy.count(), 1);
    notification.setMessage(QStringLiteral("foo"));
    QCOMPARE(messageChangedSpy.count(), 1);
    notification.setMessage(QStringLiteral("bar"));
    QCOMPARE(notification.message(), QStringLiteral("bar"));
    QCOMPARE(messageChangedSpy.count(), 2);
}
