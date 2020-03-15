/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// WaylandServer
#include "../../src/server/display.h"
#include "../../src/server/clientconnection.h"
#include "../../src/server/outputmanagement_interface.h"
#include "../../src/server/output_interface.h"
// Wayland
#include <wayland-server.h>
// system
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace KWayland::Server;

class TestWaylandServerDisplay : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testSocketName();
    void testStartStop();
    void testAddRemoveOutput();
    void testClientConnection();
    void testConnectNoSocket();
    void testOutputManagement();
    void testAutoSocketName();
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

void TestWaylandServerDisplay::testClientConnection()
{
    Display display;
    display.setSocketName(QStringLiteral("kwin-wayland-server-display-test-client-connection"));
    display.start();
    QSignalSpy connectedSpy(&display, SIGNAL(clientConnected(KWayland::Server::ClientConnection*)));
    QVERIFY(connectedSpy.isValid());
    QSignalSpy disconnectedSpy(&display, SIGNAL(clientDisconnected(KWayland::Server::ClientConnection*)));
    QVERIFY(disconnectedSpy.isValid());

    int sv[2];
    QVERIFY(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) >= 0);

    auto client = wl_client_create(display, sv[0]);
    QVERIFY(client);

    QVERIFY(connectedSpy.isEmpty());
    QVERIFY(display.connections().isEmpty());
    ClientConnection *connection = display.getConnection(client);
    QVERIFY(connection);
    QCOMPARE(connection->client(), client);
    if (getuid() == 0) {
        QEXPECT_FAIL("", "Please don't run test as root", Continue);
    }
    QVERIFY(connection->userId() != 0);
    if (getgid() == 0) {
        QEXPECT_FAIL("", "Please don't run test as root", Continue);
    }
    QVERIFY(connection->groupId() != 0);
    QVERIFY(connection->processId() != 0);
    QCOMPARE(connection->display(), &display);
    QCOMPARE(connection->executablePath(), QCoreApplication::applicationFilePath());
    QCOMPARE((wl_client*)*connection, client);
    const ClientConnection &constRef = *connection;
    QCOMPARE((wl_client*)constRef, client);
    QCOMPARE(connectedSpy.count(), 1);
    QCOMPARE(connectedSpy.first().first().value<ClientConnection*>(), connection);
    QCOMPARE(display.connections().count(), 1);
    QCOMPARE(display.connections().first(), connection);

    QCOMPARE(connection, display.getConnection(client));
    QCOMPARE(connectedSpy.count(), 1);

    // create a second client
    int sv2[2];
    QVERIFY(socketpair(AF_UNIX, SOCK_STREAM, 0, sv2) >= 0);
    auto client2 = display.createClient(sv2[0]);
    QVERIFY(client2);
    ClientConnection *connection2 = display.getConnection(client2->client());
    QVERIFY(connection2);
    QCOMPARE(connection2, client2);
    QCOMPARE(connectedSpy.count(), 2);
    QCOMPARE(connectedSpy.first().first().value<ClientConnection*>(), connection);
    QCOMPARE(connectedSpy.last().first().value<ClientConnection*>(), connection2);
    QCOMPARE(connectedSpy.last().first().value<ClientConnection*>(), client2);
    QCOMPARE(display.connections().count(), 2);
    QCOMPARE(display.connections().first(), connection);
    QCOMPARE(display.connections().last(), connection2);
    QCOMPARE(display.connections().last(), client2);

    // and destroy
    QVERIFY(disconnectedSpy.isEmpty());
    wl_client_destroy(client);
    QCOMPARE(disconnectedSpy.count(), 1);
    QSignalSpy clientDestroyedSpy(client2, &QObject::destroyed);
    QVERIFY(clientDestroyedSpy.isValid());
    client2->destroy();
    QVERIFY(clientDestroyedSpy.wait());
    QCOMPARE(disconnectedSpy.count(), 2);
    close(sv[0]);
    close(sv[1]);
    close(sv2[0]);
    close(sv2[1]);
    QVERIFY(display.connections().isEmpty());
}

void TestWaylandServerDisplay::testConnectNoSocket()
{
    Display display;
    display.start(Display::StartMode::ConnectClientsOnly);
    QVERIFY(display.isRunning());

    // let's try connecting a client
    int sv[2];
    QVERIFY(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) >= 0);
    auto client = display.createClient(sv[0]);
    QVERIFY(client);

    wl_client_destroy(client->client());
    close(sv[0]);
    close(sv[1]);
}

void TestWaylandServerDisplay::testOutputManagement()
{
    Display display;
    display.setSocketName("kwayland-test-0");
    display.start();
    auto kwin = display.createOutputManagement(this);
    kwin->create();
    QVERIFY(kwin->isValid());
}

void TestWaylandServerDisplay::testAutoSocketName()
{
    QTemporaryDir runtimeDir;
    QVERIFY(runtimeDir.isValid());
    QVERIFY(qputenv("XDG_RUNTIME_DIR", runtimeDir.path().toUtf8()));

    Display display0;
    display0.setAutomaticSocketNaming(true);
    QSignalSpy socketNameChangedSpy0(&display0, SIGNAL(socketNameChanged(QString)));
    display0.start();
    QVERIFY(display0.isRunning());
    QCOMPARE(socketNameChangedSpy0.count(), 0);
    QCOMPARE(display0.socketName(), QStringLiteral("wayland-0"));

    Display display1;
    display1.setAutomaticSocketNaming(true);
    QSignalSpy socketNameChangedSpy1(&display1, SIGNAL(socketNameChanged(QString)));
    display1.start();
    QVERIFY(display1.isRunning());
    QCOMPARE(socketNameChangedSpy1.count(), 1);
    QCOMPARE(display1.socketName(), QStringLiteral("wayland-1"));
}


QTEST_GUILESS_MAIN(TestWaylandServerDisplay)
#include "test_display.moc"
