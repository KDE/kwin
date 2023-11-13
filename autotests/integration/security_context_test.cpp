/*
    KWin - the KDE window manager
    This file is part of the KDE project.

 SPDX-FileCopyrightText: 2023 David Edmundson <davidedmundson@kde.org>

 SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "wayland/clientconnection.h"
#include "wayland/display.h"
#include "wayland_server.h"

#include <QTemporaryFile>

#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/registry.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_security_context-0");

class SecurityContextTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testSecurityContext();
    void testClosedCloseFdOnStartup();
};

void SecurityContextTest::initTestCase()
{
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
}

void SecurityContextTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::SecurityContextManagerV1));
}

void SecurityContextTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void SecurityContextTest::testSecurityContext()
{
    // This tests a mock flatpak server creating a Security Context
    // connecting a client to the newly created server
    // and making sure everything is torn down after the closeFd is closed
    auto securityContextManager = Test::waylandSecurityContextManagerV1();
    QVERIFY(securityContextManager);

    int listenFd = socket(AF_UNIX, SOCK_STREAM, 0);
    QVERIFY(listenFd != 0);

    QTemporaryDir tempDir;

    sockaddr_un sockaddr;
    sockaddr.sun_family = AF_UNIX;
    snprintf(sockaddr.sun_path, sizeof(sockaddr.sun_path), "%s", tempDir.filePath("socket").toUtf8().constData());
    qDebug() << "listening socket:" << sockaddr.sun_path;
    QVERIFY(bind(listenFd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) == 0);
    QVERIFY(listen(listenFd, 0) == 0);

    int syncFds[2];
    QVERIFY(pipe(syncFds) >= 0);
    int closeFdForClientToKeep = syncFds[0];
    int closeFdToGiveToKwin = syncFds[1];

    auto securityContext = new QtWayland::wp_security_context_v1(securityContextManager->create_listener(listenFd, closeFdToGiveToKwin));
    close(closeFdToGiveToKwin);
    close(listenFd);
    securityContext->set_instance_id("kde.unitest.instance_id");
    securityContext->set_app_id("kde.unittest.app_id");
    securityContext->set_sandbox_engine("test_sandbox_engine");
    securityContext->commit();
    securityContext->destroy();
    delete securityContext;

    qputenv("WAYLAND_DISPLAY", tempDir.filePath("socket").toUtf8());
    QSignalSpy clientConnectedspy(waylandServer()->display(), &Display::clientConnected);

    // connect a client using the newly created listening socket
    KWayland::Client::ConnectionThread restrictedClientConnection;
    QSignalSpy connectedSpy(&restrictedClientConnection, &KWayland::Client::ConnectionThread::connected);
    QThread restictedClientThread;
    auto restictedClientThreadQuitter = qScopeGuard([&restictedClientThread]() {
        restictedClientThread.quit();
        restictedClientThread.wait();
    });
    restrictedClientConnection.moveToThread(&restictedClientThread);
    restictedClientThread.start();
    restrictedClientConnection.initConnection();
    QVERIFY(connectedSpy.wait());

    // verify that our new restricted client is seen by kwin with the right security context
    QVERIFY(clientConnectedspy.count());
    QCOMPARE(clientConnectedspy.first().first().value<KWin::ClientConnection *>()->securityContextAppId(), "kde.unittest.app_id");

    // verify that the globals for the restricted client does not contain the security context
    KWayland::Client::Registry registry;
    registry.create(&restrictedClientConnection);
    QSignalSpy interfaceAnnounced(&registry, &KWayland::Client::Registry::interfaceAnnounced);
    QSignalSpy allAnnouncedSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    registry.setup();
    QVERIFY(allAnnouncedSpy.wait());
    for (auto interfaceSignal : interfaceAnnounced) {
        QVERIFY(interfaceSignal.first().toString() != "wp_security_context_manager_v1");
    }

    // close the mock flatpak closeFDs
    close(closeFdForClientToKeep);

    // security context properties should have not changed after close-fd is closed
    QVERIFY(Test::waylandSync());
    QCOMPARE(clientConnectedspy.first().first().value<KWin::ClientConnection *>()->securityContextAppId(), "kde.unittest.app_id");

    // new clients can't connect anymore
    KWayland::Client::ConnectionThread restrictedClientConnection2;
    QSignalSpy connectedSpy2(&restrictedClientConnection2, &KWayland::Client::ConnectionThread::connected);
    QSignalSpy failedSpy2(&restrictedClientConnection2, &KWayland::Client::ConnectionThread::failed);
    QThread restictedClientThread2;
    auto restictedClientThreadQuitter2 = qScopeGuard([&restictedClientThread2]() {
        restictedClientThread2.quit();
        restictedClientThread2.wait();
    });
    restrictedClientConnection2.moveToThread(&restictedClientThread2);
    restictedClientThread2.start();
    restrictedClientConnection2.initConnection();
    QVERIFY(failedSpy2.wait());
    QVERIFY(connectedSpy2.isEmpty());
}

void SecurityContextTest::testClosedCloseFdOnStartup()
{
    // This tests what would happen if the closeFd is already closed when kwin processes the security context
    auto securityContextManager = Test::waylandSecurityContextManagerV1();
    QVERIFY(securityContextManager);

    int listenFd = socket(AF_UNIX, SOCK_STREAM, 0);
    QVERIFY(listenFd != 0);

    QTemporaryDir tempDir;

    sockaddr_un sockaddr;
    sockaddr.sun_family = AF_UNIX;
    snprintf(sockaddr.sun_path, sizeof(sockaddr.sun_path), "%s", tempDir.filePath("socket").toUtf8().constData());
    qDebug() << "listening socket:" << sockaddr.sun_path;
    QVERIFY(bind(listenFd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) == 0);
    QVERIFY(listen(listenFd, 0) == 0);

    int syncFds[2];
    QVERIFY(pipe(syncFds) >= 0);
    int closeFdForClientToKeep = syncFds[0];
    int closeFdToGiveToKwin = syncFds[1];

    close(closeFdForClientToKeep); // closes the connection

    auto securityContext = new QtWayland::wp_security_context_v1(securityContextManager->create_listener(listenFd, closeFdToGiveToKwin));
    close(closeFdToGiveToKwin);
    close(listenFd);
    securityContext->set_instance_id("kde.unitest.instance_id");
    securityContext->set_app_id("kde.unittest.app_id");
    securityContext->set_sandbox_engine("test_sandbox_engine");
    securityContext->commit();
    securityContext->destroy();
    delete securityContext;

    QVERIFY(Test::waylandSync());

    qputenv("WAYLAND_DISPLAY", tempDir.filePath("socket").toUtf8());
    QSignalSpy clientConnectedspy(waylandServer()->display(), &Display::clientConnected);

    // new clients can't connect anymore
    KWayland::Client::ConnectionThread restrictedClientConnection;
    QSignalSpy connectedSpy(&restrictedClientConnection, &KWayland::Client::ConnectionThread::connected);
    QSignalSpy failedSpy(&restrictedClientConnection, &KWayland::Client::ConnectionThread::failed);
    QThread restictedClientThread;
    auto restictedClientThreadQuitter = qScopeGuard([&restictedClientThread]() {
        restictedClientThread.quit();
        restictedClientThread.wait();
    });
    restrictedClientConnection.moveToThread(&restictedClientThread);
    restictedClientThread.start();
    restrictedClientConnection.initConnection();
    QVERIFY(failedSpy.wait());
    QVERIFY(connectedSpy.isEmpty());
    QVERIFY(clientConnectedspy.isEmpty());
}
}

WAYLANDTEST_MAIN(KWin::SecurityContextTest)
#include "security_context_test.moc"
