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
    // this test verifies that without an XDG_RUNTIME_DIR the WaylandServer fials to init
    QVERIFY(!waylandServer()->init(s_socketName.toLocal8Bit()));
}

WAYLANDTEST_MAIN(NoXdgRuntimeDirTest)
#include "no_xdg_runtime_dir_test.moc"
