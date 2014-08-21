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
#include "../../wayland_client/compositor.h"
#include "../../wayland_client/connection_thread.h"
#include "../../wayland_client/shell.h"
#include "../../wayland_client/surface.h"
#include "../../wayland_client/registry.h"
// Wayland
#include <wayland-client-protocol.h>

class TestWaylandShell : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandShell(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testRegistry();
    void testShell();

    // TODO: add tests for removal - requires more control over the compositor

private:
    QProcess *m_westonProcess;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-shell-0");

TestWaylandShell::TestWaylandShell(QObject *parent)
    : QObject(parent)
    , m_westonProcess(nullptr)
{
}

void TestWaylandShell::init()
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

void TestWaylandShell::cleanup()
{
    // terminates weston
    m_westonProcess->terminate();
    QVERIFY(m_westonProcess->waitForFinished());
    delete m_westonProcess;
    m_westonProcess = nullptr;
}

void TestWaylandShell::testRegistry()
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
    QSignalSpy announced(&registry, SIGNAL(shellAnnounced(quint32,quint32)));
    registry.create(connection.display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(connection.display());
    QVERIFY(announced.wait());

    KWin::Wayland::Shell shell;
    QVERIFY(!shell.isValid());

    shell.setup(registry.bindShell(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(connection.display());
    QVERIFY(shell.isValid());

    shell.release();
    QVERIFY(!shell.isValid());
}

void TestWaylandShell::testShell()
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
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    QSignalSpy announced(&registry, SIGNAL(shellAnnounced(quint32,quint32)));
    registry.create(connection.display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(connection.display());
    QVERIFY(announced.wait());

    if (compositorSpy.isEmpty()) {
        QVERIFY(compositorSpy.wait());
    }
    KWin::Wayland::Compositor compositor;
    compositor.setup(registry.bindCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>()));

    KWin::Wayland::Shell shell;
    shell.setup(registry.bindShell(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(connection.display());
    QScopedPointer<KWin::Wayland::Surface> s(compositor.createSurface());
    QVERIFY(!s.isNull());
    QVERIFY(s->isValid());
    KWin::Wayland::ShellSurface *surface = shell.createSurface(s.data(), &shell);
    QSignalSpy sizeSpy(surface, SIGNAL(sizeChanged(QSize)));
    QVERIFY(sizeSpy.isValid());
    QCOMPARE(surface->size(), QSize());

    surface->setFullscreen();
    wl_display_flush(connection.display());
    QVERIFY(sizeSpy.wait());
    QCOMPARE(sizeSpy.count(), 1);
    QCOMPARE(sizeSpy.first().first().toSize(), QSize(1024, 768));
    QCOMPARE(surface->size(), QSize(1024, 768));

    QVERIFY(surface->isValid());
    shell.release();
    QVERIFY(!surface->isValid());

    compositor.release();
}

QTEST_MAIN(TestWaylandShell)
#include "test_wayland_shell.moc"
