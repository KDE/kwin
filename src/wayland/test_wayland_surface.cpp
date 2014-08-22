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
#include "../../wayland_client/surface.h"
#include "../../wayland_client/registry.h"
// Wayland
#include <wayland-client-protocol.h>

class TestWaylandSurface : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandSurface(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testStaticAccessor();

private:
    QProcess *m_westonProcess;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-surface-0");

TestWaylandSurface::TestWaylandSurface(QObject *parent)
    : QObject(parent)
    , m_westonProcess(nullptr)
{
}

void TestWaylandSurface::init()
{
    QVERIFY(!m_westonProcess);
    // starts weston
    m_westonProcess = new QProcess(this);
    m_westonProcess->setProgram(QStringLiteral("weston"));

    m_westonProcess->setArguments(QStringList({QStringLiteral("--socket=%1").arg(s_socketName),
                                               QStringLiteral("--use-pixman")}));
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

void TestWaylandSurface::cleanup()
{
    // terminates weston
    m_westonProcess->terminate();
    QVERIFY(m_westonProcess->waitForFinished());
    delete m_westonProcess;
    m_westonProcess = nullptr;
}

void TestWaylandSurface::testStaticAccessor()
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
    registry.create(connection.display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(connection.display());
    QVERIFY(compositorSpy.wait());

    KWin::Wayland::Compositor compositor;
    compositor.setup(registry.bindCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>()));
    QVERIFY(compositor.isValid());

    QVERIFY(KWin::Wayland::Surface::all().isEmpty());
    KWin::Wayland::Surface *s1 = compositor.createSurface();
    QCOMPARE(KWin::Wayland::Surface::all().count(), 1);
    QCOMPARE(KWin::Wayland::Surface::all().first(), s1);
    QCOMPARE(KWin::Wayland::Surface::get(*s1), s1);

    // add another surface
    KWin::Wayland::Surface *s2 = compositor.createSurface();
    QCOMPARE(KWin::Wayland::Surface::all().count(), 2);
    QCOMPARE(KWin::Wayland::Surface::all().first(), s1);
    QCOMPARE(KWin::Wayland::Surface::all().last(), s2);
    QCOMPARE(KWin::Wayland::Surface::get(*s1), s1);
    QCOMPARE(KWin::Wayland::Surface::get(*s2), s2);

    // delete s2 again
    delete s2;
    QCOMPARE(KWin::Wayland::Surface::all().count(), 1);
    QCOMPARE(KWin::Wayland::Surface::all().first(), s1);
    QCOMPARE(KWin::Wayland::Surface::get(*s1), s1);

    // and finally delete the last one
    delete s1;
    QVERIFY(KWin::Wayland::Surface::all().isEmpty());
    QVERIFY(!KWin::Wayland::Surface::get(nullptr));
}

QTEST_MAIN(TestWaylandSurface)
#include "test_wayland_surface.moc"
