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
// KWin
#include "../../wayland_client/connection_thread.h"
#include "../../wayland_client/output.h"
#include "../../wayland_client/registry.h"
// Wayland
#include <wayland-client-protocol.h>

class TestWaylandOutput : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandOutput(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testRegistry();

    // TODO: add tests for removal - requires more control over the compositor

private:
    QProcess *m_westonProcess;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-output-0");

TestWaylandOutput::TestWaylandOutput(QObject *parent)
    : QObject(parent)
    , m_westonProcess(nullptr)
{
}

void TestWaylandOutput::init()
{
    QVERIFY(!m_westonProcess);
    // starts weston
    m_westonProcess = new QProcess(this);
    m_westonProcess->setProgram(QStringLiteral("weston"));

    m_westonProcess->setArguments(QStringList({QStringLiteral("--socket=%1").arg(s_socketName),
                                               QStringLiteral("--use-pixman"),
                                               QStringLiteral("--width=1024"),
                                               QStringLiteral("--height=768")}));
    m_westonProcess->start();
    QVERIFY(m_westonProcess->waitForStarted());

    // wait for the socket to appear
    QDir runtimeDir(qgetenv("XDG_RUNTIME_DIR"));
    if (runtimeDir.exists(s_socketName)) {
        return;
    }
    QFileSystemWatcher *socketWatcher = new QFileSystemWatcher(QStringList({runtimeDir.absolutePath()}), this);
    QSignalSpy socketSpy(socketWatcher, SIGNAL(directoryChanged(QString)));

    // limit to maximum of 10 waits
    for (int i = 0; i < 10; ++i) {
        QVERIFY(socketSpy.wait());
        if (runtimeDir.exists(s_socketName)) {
            delete socketWatcher;
            return;
        }
    }
}

void TestWaylandOutput::cleanup()
{
    // terminates weston
    m_westonProcess->terminate();
    QVERIFY(m_westonProcess->waitForFinished());
    delete m_westonProcess;
    m_westonProcess = nullptr;
}

void TestWaylandOutput::testRegistry()
{
    if (m_westonProcess->state() != QProcess::Running) {
        QSKIP("This test requires a running wayland server");
    }
    KWin::Wayland::ConnectionThread connection;
    QSignalSpy connectedSpy(&connection, SIGNAL(connected()));
    connection.setSocketName(s_socketName);
    connection.initConnection();
    QVERIFY(connectedSpy.wait());

    KWin::Wayland::Registry registry;
    QSignalSpy announced(&registry, SIGNAL(outputAnnounced(quint32,quint32)));
    registry.create(connection.display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(connection.display());
    QVERIFY(announced.wait());

    KWin::Wayland::Output output;
    QVERIFY(!output.isValid());
    QCOMPARE(output.geometry(), QRect());
    QCOMPARE(output.globalPosition(), QPoint());
    QCOMPARE(output.manufacturer(), QString());
    QCOMPARE(output.model(), QString());
    QCOMPARE(output.physicalSize(), QSize());
    QCOMPARE(output.pixelSize(), QSize());
    QCOMPARE(output.refreshRate(), 0);
    QCOMPARE(output.scale(), 1);
    QCOMPARE(output.subPixel(), KWin::Wayland::Output::SubPixel::Unknown);
    QCOMPARE(output.transform(), KWin::Wayland::Output::Transform::Normal);

    QSignalSpy outputChanged(&output, SIGNAL(changed()));
    QVERIFY(outputChanged.isValid());

    output.setup(registry.bindOutput(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(connection.display());
    QVERIFY(outputChanged.wait());

    QCOMPARE(output.geometry(), QRect(0, 0, 1024, 768));
    QCOMPARE(output.globalPosition(), QPoint(0, 0));
    QCOMPARE(output.manufacturer(), QStringLiteral("xwayland"));
    QCOMPARE(output.model(), QStringLiteral("none"));
    // TODO: add test for physicalSize
    QCOMPARE(output.pixelSize(), QSize(1024, 768));
    QCOMPARE(output.refreshRate(), 60000);
    QCOMPARE(output.scale(), 1);
    // for xwayland output it's unknown
    QCOMPARE(output.subPixel(), KWin::Wayland::Output::SubPixel::Unknown);
    // for xwayland transform is normal
    QCOMPARE(output.transform(), KWin::Wayland::Output::Transform::Normal);
}

QTEST_MAIN(TestWaylandOutput)
#include "test_wayland_output.moc"
