/*
 * Copyright 2016  Martin Graesslin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "onscreennotificationtest.h"
#include "../onscreennotification.h"

#include <KSharedConfig>
#include <KConfigGroup>

#include <QQmlEngine>
#include <QSignalSpy>
#include <QTest>

QTEST_MAIN(OnScreenNotificationTest);

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
    QVERIFY(iconNameChangedSpy.isValid());
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
    QVERIFY(messageChangedSpy.isValid());
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

#include "onscreennotificationtest.moc"
