/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
// KWin
#include "../../src/client/connection_thread.h"
#include "../../src/client/registry.h"
#include "../../src/client/fullscreen_shell.h"
// Wayland
#include <wayland-client-protocol.h>

class TestWaylandFullscreenShell : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandFullscreenShell(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testRegistry();

    // TODO: add tests for removal - requires more control over the compositor

private:
    QProcess *m_westonProcess;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-fullscreen-shell-0");

TestWaylandFullscreenShell::TestWaylandFullscreenShell(QObject *parent)
    : QObject(parent)
    , m_westonProcess(nullptr)
{
}

void TestWaylandFullscreenShell::init()
{
    QVERIFY(!m_westonProcess);
    // starts weston
    m_westonProcess = new QProcess(this);
    m_westonProcess->setProgram(QStringLiteral("weston"));

    m_westonProcess->setArguments(QStringList({QStringLiteral("--socket=%1").arg(s_socketName),
                                               QStringLiteral("--use-pixman"),
                                               QStringLiteral("--shell=fullscreen-shell.so")}));
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

void TestWaylandFullscreenShell::cleanup()
{
    // terminates weston
    m_westonProcess->terminate();
    QVERIFY(m_westonProcess->waitForFinished());
    delete m_westonProcess;
    m_westonProcess = nullptr;
}

void TestWaylandFullscreenShell::testRegistry()
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
    QSignalSpy announced(&registry, SIGNAL(fullscreenShellAnnounced(quint32,quint32)));
    registry.create(connection.display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(connection.display());
    QVERIFY(announced.wait());

    KWin::Wayland::FullscreenShell fullscreenShell;
    QVERIFY(!fullscreenShell.isValid());
    QVERIFY(!fullscreenShell.hasCapabilityArbitraryModes());
    QVERIFY(!fullscreenShell.hasCapabilityCursorPlane());

    fullscreenShell.setup(registry.bindFullscreenShell(announced.first().first().value<quint32>(), 1));
    QVERIFY(fullscreenShell.isValid());
}

QTEST_MAIN(TestWaylandFullscreenShell)
#include "test_wayland_fullscreen_shell.moc"
