/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
// Qt
#include <QtTest/QtTest>
// WaylandServer
#include "../../wayland_server/display.h"
#include "../../wayland_server/output_interface.h"

using namespace KWin::WaylandServer;

class TestWaylandServerDisplay : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testSocketName();
    void testStartStop();
    void testAddRemoveOutput();
};

void TestWaylandServerDisplay::testSocketName()
{
    Display display;
    QSignalSpy changedSpy(&display, SIGNAL(socketNameChanged(QString)));
    QVERIFY(changedSpy.isValid());
    QCOMPARE(display.socketName(), QStringLiteral("wayland-0"));
    const QString testSName = QStringLiteral("fooBar");
    display.setSocketName(testSName);
    QCOMPARE(display.socketName(), testSName);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(changedSpy.first().first().toString(), testSName);

    // changing to same name again should not emit signal
    display.setSocketName(testSName);
    QCOMPARE(changedSpy.count(), 1);
}

void TestWaylandServerDisplay::testStartStop()
{
    const QString testSocketName = QStringLiteral("kwin-wayland-server-display-test-0");
    QDir runtimeDir(qgetenv("XDG_RUNTIME_DIR"));
    QVERIFY(runtimeDir.exists());
    QVERIFY(!runtimeDir.exists(testSocketName));

    Display display;
    QSignalSpy runningSpy(&display, SIGNAL(runningChanged(bool)));
    QVERIFY(runningSpy.isValid());
    display.setSocketName(testSocketName);
    QVERIFY(!display.isRunning());
    display.start();
//     QVERIFY(runningSpy.wait());
    QCOMPARE(runningSpy.count(), 1);
    QVERIFY(runningSpy.first().first().toBool());
    QVERIFY(display.isRunning());
    QVERIFY(runtimeDir.exists(testSocketName));

    display.terminate();
    QVERIFY(!display.isRunning());
    QCOMPARE(runningSpy.count(), 2);
    QVERIFY(runningSpy.first().first().toBool());
    QVERIFY(!runningSpy.last().first().toBool());
    QVERIFY(!runtimeDir.exists(testSocketName));
}

void TestWaylandServerDisplay::testAddRemoveOutput()
{
    Display display;
    display.setSocketName(QStringLiteral("kwin-wayland-server-display-test-output-0"));
    display.start();

    OutputInterface *output = display.createOutput();
    QCOMPARE(display.outputs().size(), 1);
    QCOMPARE(display.outputs().first(), output);
    // create a second output
    OutputInterface *output2 = display.createOutput();
    QCOMPARE(display.outputs().size(), 2);
    QCOMPARE(display.outputs().first(), output);
    QCOMPARE(display.outputs().last(), output2);
    // remove the first output
    display.removeOutput(output);
    QCOMPARE(display.outputs().size(), 1);
    QCOMPARE(display.outputs().first(), output2);
    // and delete the second
    delete output2;
    QVERIFY(display.outputs().isEmpty());
}

QTEST_MAIN(TestWaylandServerDisplay)
#include "test_display.moc"
