/********************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
// Qt
#include <QtTest/QtTest>
// WaylandServer
#include "../../src/server/display.h"

using namespace KWayland::Server;

class NoXdgRuntimeDirTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testCreate();
};

void NoXdgRuntimeDirTest::initTestCase()
{
    qunsetenv("XDG_RUNTIME_DIR");
}

void NoXdgRuntimeDirTest::testCreate()
{
    // this test verifies that not having an XDG_RUNTIME_DIR is handled gracefully
    // the server cannot start, but should not crash
    const QString testSocketName = QStringLiteral("kwayland-test-no-xdg-runtime-dir-0");
    Display display;
    QSignalSpy runningSpy(&display, &Display::runningChanged);
    QVERIFY(runningSpy.isValid());
    display.setSocketName(testSocketName);
    QVERIFY(!display.isRunning());
    display.start();
    QVERIFY(!display.isRunning());

    // call into dispatchEvents should not crash
    display.dispatchEvents();
}

QTEST_GUILESS_MAIN(NoXdgRuntimeDirTest)
#include "test_no_xdg_runtime_dir.moc"
