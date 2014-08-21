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
#include "../../wayland_client/registry.h"
// Wayland
#include <wayland-client-protocol.h>

class TestWaylandRegistry : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandRegistry(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testBindCompositor();
    void testBindShell();
    void testBindOutput();
    void testBindShm();
    void testBindSeat();

    // TODO: add tests for removal - requires more control over the compositor

private:
    QProcess *m_westonProcess;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-registry-0");

TestWaylandRegistry::TestWaylandRegistry(QObject *parent)
    : QObject(parent)
    , m_westonProcess(nullptr)
{
}

void TestWaylandRegistry::init()
{
    QVERIFY(!m_westonProcess);
    // starts weston
    m_westonProcess = new QProcess(this);
    m_westonProcess->setProgram(QStringLiteral("weston"));

    m_westonProcess->setArguments(QStringList({QStringLiteral("--socket=%1").arg(s_socketName), QStringLiteral("--use-pixman")}));
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

void TestWaylandRegistry::cleanup()
{
    // terminates weston
    m_westonProcess->terminate();
    QVERIFY(m_westonProcess->waitForFinished());
    delete m_westonProcess;
    m_westonProcess = nullptr;
}

void TestWaylandRegistry::testCreate()
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
    QVERIFY(!registry.isValid());
    registry.create(connection.display());
    QVERIFY(registry.isValid());
    registry.release();
    QVERIFY(!registry.isValid());
}

#define TEST_BIND(interface, signalName, bindMethod, destroyFunction) \
    if (m_westonProcess->state() != QProcess::Running) { \
        QSKIP("This test requires a running wayland server"); \
    } \
    KWin::Wayland::ConnectionThread connection; \
    QSignalSpy connectedSpy(&connection, SIGNAL(connected())); \
    connection.setSocketName(s_socketName); \
    connection.initConnection(); \
    QVERIFY(connectedSpy.wait()); \
    \
    KWin::Wayland::Registry registry; \
    /* before registry is created, we cannot bind the interface*/ \
    QVERIFY(!registry.bindMethod(0, 0)); \
    \
    QVERIFY(!registry.isValid()); \
    registry.create(connection.display()); \
    QVERIFY(registry.isValid()); \
    /* created but not yet connected still results in no bind */ \
    QVERIFY(!registry.bindMethod(0, 0)); \
    \
    /* now lets register */ \
    QSignalSpy announced(&registry, signalName); \
    QVERIFY(announced.isValid()); \
    registry.setup(); \
    wl_display_flush(connection.display()); \
    QVERIFY(announced.wait()); \
    const quint32 name = announced.first().first().value<quint32>(); \
    const quint32 version = announced.first().last().value<quint32>(); \
    \
    /* registry should now about the interface now */ \
    QVERIFY(registry.hasInterface(interface)); \
    QVERIFY(!registry.bindMethod(name+1, version)); \
    QVERIFY(!registry.bindMethod(name, version+1)); \
    auto *c = registry.bindMethod(name, version); \
    QVERIFY(c); \
    destroyFunction(c); \

void TestWaylandRegistry::testBindCompositor()
{
    TEST_BIND(KWin::Wayland::Registry::Interface::Compositor, SIGNAL(compositorAnnounced(quint32,quint32)), bindCompositor, wl_compositor_destroy)
}

void TestWaylandRegistry::testBindShell()
{
    TEST_BIND(KWin::Wayland::Registry::Interface::Shell, SIGNAL(shellAnnounced(quint32,quint32)), bindShell, free)
}

void TestWaylandRegistry::testBindOutput()
{
    TEST_BIND(KWin::Wayland::Registry::Interface::Output, SIGNAL(outputAnnounced(quint32,quint32)), bindOutput, wl_output_destroy)
}

void TestWaylandRegistry::testBindSeat()
{
    TEST_BIND(KWin::Wayland::Registry::Interface::Seat, SIGNAL(seatAnnounced(quint32,quint32)), bindSeat, wl_seat_destroy)
}

void TestWaylandRegistry::testBindShm()
{
    TEST_BIND(KWin::Wayland::Registry::Interface::Shm, SIGNAL(shmAnnounced(quint32,quint32)), bindShm, wl_shm_destroy)
}

#undef TEST_BIND

QTEST_MAIN(TestWaylandRegistry)
#include "test_wayland_registry.moc"
