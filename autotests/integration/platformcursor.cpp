/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "cursor.h"
#include "platform.h"
#include "wayland_server.h"

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_platform_cursor-0");

class PlatformCursorTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testPos();
};

void PlatformCursorTest::initTestCase()
{
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
}

void PlatformCursorTest::testPos()
{
    // this test verifies that the PlatformCursor of the QPA plugin forwards ::pos and ::setPos correctly
    // that is QCursor should work just like KWin::Cursor

    // cursor should be centered on screen
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(639, 511));
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(639, 511));

    // let's set the pos through QCursor API
    QCursor::setPos(QPoint(10, 10));
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(10, 10));
    QCOMPARE(QCursor::pos(), QPoint(10, 10));

    // and let's set the pos through Cursor API
    QCursor::setPos(QPoint(20, 20));
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(20, 20));
    QCOMPARE(QCursor::pos(), QPoint(20, 20));
}

}

WAYLANDTEST_MAIN(KWin::PlatformCursorTest)
#include "platformcursor.moc"
