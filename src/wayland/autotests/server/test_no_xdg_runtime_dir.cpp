/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// WaylandServer
#include "../../src/server/display.h"

using namespace KWaylandServer;

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
    QVERIFY(!display.addSocketName(testSocketName));
    display.start();

    // call into dispatchEvents should not crash
    display.dispatchEvents();
}

QTEST_GUILESS_MAIN(NoXdgRuntimeDirTest)
#include "test_no_xdg_runtime_dir.moc"
