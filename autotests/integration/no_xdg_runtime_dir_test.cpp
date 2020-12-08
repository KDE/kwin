/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "wayland_server.h"

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_no_xdg_runtime_dir-0");

class NoXdgRuntimeDirTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testInitFails();
};

void NoXdgRuntimeDirTest::initTestCase()
{
    qunsetenv("XDG_RUNTIME_DIR");
}

void NoXdgRuntimeDirTest::testInitFails()
{
    // this test verifies that without an XDG_RUNTIME_DIR the WaylandServer fails to start
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QVERIFY(!waylandServer()->start());
}

WAYLANDTEST_MAIN(NoXdgRuntimeDirTest)
#include "no_xdg_runtime_dir_test.moc"
