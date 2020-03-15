/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
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
    void testRegistryCreate();

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
                                               QStringLiteral("--backend=headless-backend.so"),
                                               QStringLiteral("--shell=fullscreen-shell.so")}));
    m_westonProcess->start();
    QVERIFY(m_westonProcess->waitForStarted());
    QTest::qWait(500);

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
    KWayland::Client::ConnectionThread connection;
    QSignalSpy connectedSpy(&connection, SIGNAL(connected()));
    connection.setSocketName(s_socketName);
    connection.initConnection();
    QVERIFY(connectedSpy.wait());

    KWayland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfaceAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, SIGNAL(fullscreenShellAnnounced(quint32,quint32)));
    registry.create(connection.display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(connection.display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    if (!registry.hasInterface(KWayland::Client::Registry::Interface::FullscreenShell)) {
        QSKIP("Weston does not have fullscreen shell support");
    }
    QCOMPARE(announced.count(), 1);

    KWayland::Client::FullscreenShell fullscreenShell;
    QVERIFY(!fullscreenShell.isValid());
    QVERIFY(!fullscreenShell.hasCapabilityArbitraryModes());
    QVERIFY(!fullscreenShell.hasCapabilityCursorPlane());

    fullscreenShell.setup(registry.bindFullscreenShell(announced.first().first().value<quint32>(), 1));
    QVERIFY(fullscreenShell.isValid());
}

void TestWaylandFullscreenShell::testRegistryCreate()
{    if (m_westonProcess->state() != QProcess::Running) {
        QSKIP("This test requires a running wayland server");
    }
    KWayland::Client::ConnectionThread connection;
    QSignalSpy connectedSpy(&connection, SIGNAL(connected()));
    connection.setSocketName(s_socketName);
    connection.initConnection();
    QVERIFY(connectedSpy.wait());

    KWayland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfaceAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, SIGNAL(fullscreenShellAnnounced(quint32,quint32)));
    registry.create(connection.display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(connection.display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    if (!registry.hasInterface(KWayland::Client::Registry::Interface::FullscreenShell)) {
        QSKIP("Weston does not have fullscreen shell support");
    }
    QCOMPARE(announced.count(), 1);

    KWayland::Client::FullscreenShell *fullscreenShell = registry.createFullscreenShell(announced.first().first().value<quint32>(), 1, &registry);
    QVERIFY(fullscreenShell->isValid());
}

QTEST_GUILESS_MAIN(TestWaylandFullscreenShell)
#include "test_wayland_fullscreen_shell.moc"
