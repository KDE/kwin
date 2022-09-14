/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// WaylandServer
#include "wayland/clientconnection.h"
#include "wayland/display.h"
// Wayland
#include <wayland-server.h>
// system
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace KWaylandServer;

class TestWaylandServerDisplay : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testSocketName();
    void testStartStop();
    void testClientConnection();
    void testConnectNoSocket();
    void testAutoSocketName();
};

void TestWaylandServerDisplay::testSocketName()
{
    KWaylandServer::Display display;
    QSignalSpy changedSpy(&display, &KWaylandServer::Display::socketNamesChanged);
    QCOMPARE(display.socketNames(), QStringList());
    const QString testSName = QStringLiteral("fooBar");
    display.addSocketName(testSName);
    QCOMPARE(display.socketNames(), QStringList{testSName});
    QCOMPARE(changedSpy.count(), 1);

    // changing to same name again should not emit signal
    display.addSocketName(testSName);
    QCOMPARE(changedSpy.count(), 1);
}

void TestWaylandServerDisplay::testStartStop()
{
    const QString testSocketName = QStringLiteral("kwin-wayland-server-display-test-0");
    QDir runtimeDir(qgetenv("XDG_RUNTIME_DIR"));
    QVERIFY(runtimeDir.exists());
    QVERIFY(!runtimeDir.exists(testSocketName));

    std::unique_ptr<KWaylandServer::Display> display(new KWaylandServer::Display);
    QSignalSpy runningSpy(display.get(), &KWaylandServer::Display::runningChanged);
    display->addSocketName(testSocketName);
    QVERIFY(!display->isRunning());
    display->start();
    //     QVERIFY(runningSpy.wait());
    QCOMPARE(runningSpy.count(), 1);
    QVERIFY(runningSpy.first().first().toBool());
    QVERIFY(display->isRunning());
    QVERIFY(runtimeDir.exists(testSocketName));

    display.reset();
    QVERIFY(!runtimeDir.exists(testSocketName));
}

void TestWaylandServerDisplay::testClientConnection()
{
    KWaylandServer::Display display;
    display.addSocketName(QStringLiteral("kwin-wayland-server-display-test-client-connection"));
    display.start();
    QSignalSpy connectedSpy(&display, &KWaylandServer::Display::clientConnected);
    QSignalSpy disconnectedSpy(&display, &KWaylandServer::Display::clientDisconnected);

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
    QCOMPARE((wl_client *)*connection, client);
    const ClientConnection &constRef = *connection;
    QCOMPARE((wl_client *)constRef, client);
    QCOMPARE(connectedSpy.count(), 1);
    QCOMPARE(connectedSpy.first().first().value<ClientConnection *>(), connection);
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
    QCOMPARE(connectedSpy.first().first().value<ClientConnection *>(), connection);
    QCOMPARE(connectedSpy.last().first().value<ClientConnection *>(), connection2);
    QCOMPARE(connectedSpy.last().first().value<ClientConnection *>(), client2);
    QCOMPARE(display.connections().count(), 2);
    QCOMPARE(display.connections().first(), connection);
    QCOMPARE(display.connections().last(), connection2);
    QCOMPARE(display.connections().last(), client2);

    // and destroy
    QVERIFY(disconnectedSpy.isEmpty());
    wl_client_destroy(client);
    QCOMPARE(disconnectedSpy.count(), 1);
    QSignalSpy clientDestroyedSpy(client2, &QObject::destroyed);
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
    KWaylandServer::Display display;
    display.start();
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

void TestWaylandServerDisplay::testAutoSocketName()
{
    QTemporaryDir runtimeDir;
    QVERIFY(runtimeDir.isValid());
    QVERIFY(qputenv("XDG_RUNTIME_DIR", runtimeDir.path().toUtf8()));

    KWaylandServer::Display display0;
    QSignalSpy socketNameChangedSpy0(&display0, &KWaylandServer::Display::socketNamesChanged);
    QVERIFY(socketNameChangedSpy0.isValid());
    QVERIFY(display0.addSocketName());
    display0.start();
    QVERIFY(display0.isRunning());
    QCOMPARE(socketNameChangedSpy0.count(), 1);

    KWaylandServer::Display display1;
    QSignalSpy socketNameChangedSpy1(&display1, &KWaylandServer::Display::socketNamesChanged);
    QVERIFY(socketNameChangedSpy1.isValid());
    QVERIFY(display1.addSocketName());
    display1.start();
    QVERIFY(display1.isRunning());
    QCOMPARE(socketNameChangedSpy1.count(), 1);
}

QTEST_GUILESS_MAIN(TestWaylandServerDisplay)
#include "test_display.moc"
