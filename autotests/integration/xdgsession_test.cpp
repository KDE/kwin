/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "pointer_input.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/surface.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_xdgsession-0");

class TestXdgSession : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void addToplevelSession();
    void addMappedToplevelSession();
    void restoreToplevelSession();
    void restoreMappedToplevelSession();
    void replaceSession();
};

void TestXdgSession::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWayland::Client::Output *>();

    qputenv("KWIN_WAYLAND_SUPPORT_XX_SESSION_MANAGER", "1");

    QVERIFY(waylandServer()->init(s_socketName));

    kwinApp()->start();
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void TestXdgSession::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::XdgSessionV1));
    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
}

void TestXdgSession::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestXdgSession::addToplevelSession()
{
    std::unique_ptr<Test::XdgSessionV1> session(Test::createXdgSessionV1(Test::XdgSessionManagerV1::reason_launch));
    QSignalSpy sessionCreatedSpy(session.get(), &Test::XdgSessionV1::created);
    QVERIFY(sessionCreatedSpy.wait());

    // If the toplevel is added to the session for the first time, we should receive a created event, but no restored event.
    {
        std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
        std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
        std::unique_ptr<Test::XdgToplevelSessionV1> toplevelSession(session->add(shellSurface.get(), QStringLiteral("foo")));

        QSignalSpy toplevelSessionRestoredSpy(toplevelSession.get(), &Test::XdgToplevelSessionV1::restored);
        QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
        QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        QCOMPARE(sessionCreatedSpy.count(), 1);
        QCOMPARE(toplevelSessionRestoredSpy.count(), 0);

        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.first()[0].toUInt());
        auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
        shellSurface.reset();
        QVERIFY(Test::waitForWindowClosed(window));
    }

    // If the toplevel is added to the session again, no created and restored events should be sent.
    {
        std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
        std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
        std::unique_ptr<Test::XdgToplevelSessionV1> toplevelSession(session->add(shellSurface.get(), QStringLiteral("foo")));

        QSignalSpy toplevelSessionRestoredSpy(toplevelSession.get(), &Test::XdgToplevelSessionV1::restored);
        QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
        QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        QCOMPARE(sessionCreatedSpy.count(), 1);
        QCOMPARE(toplevelSessionRestoredSpy.count(), 0);

        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.first()[0].toUInt());
        auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
        shellSurface.reset();
        QVERIFY(Test::waitForWindowClosed(window));
    }
}

void TestXdgSession::addMappedToplevelSession()
{
    // This test verifies that no protocol error will be posted if a mapped toplevel is added to the session.

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    std::unique_ptr<Test::XdgSessionV1> session(Test::createXdgSessionV1(Test::XdgSessionManagerV1::reason_launch));
    std::unique_ptr<Test::XdgToplevelSessionV1> toplevelSession(session->add(shellSurface.get(), QStringLiteral("foo")));

    QSignalSpy sessionCreatedSpy(session.get(), &Test::XdgSessionV1::created);
    QSignalSpy toplevelSessionRestoredSpy(toplevelSession.get(), &Test::XdgToplevelSessionV1::restored);
    QVERIFY(sessionCreatedSpy.wait());
    QCOMPARE(sessionCreatedSpy.count(), 1);
    QCOMPARE(toplevelSessionRestoredSpy.count(), 0);
}

void TestXdgSession::restoreToplevelSession()
{
    std::unique_ptr<Test::XdgSessionV1> session(Test::createXdgSessionV1(Test::XdgSessionManagerV1::reason_launch));
    QSignalSpy sessionCreatedSpy(session.get(), &Test::XdgSessionV1::created);
    QVERIFY(sessionCreatedSpy.wait());

    // Add the toplevel to the session.
    {
        std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
        std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
        std::unique_ptr<Test::XdgToplevelSessionV1> toplevelSession(session->add(shellSurface.get(), QStringLiteral("foo")));

        QSignalSpy toplevelSessionRestoredSpy(toplevelSession.get(), &Test::XdgToplevelSessionV1::restored);
        QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
        QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        QCOMPARE(toplevelSessionRestoredSpy.count(), 0);

        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.first()[0].toUInt());
        auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
        shellSurface.reset();
        QVERIFY(Test::waitForWindowClosed(window));
    }

    // Restore the toplevel session.
    {
        std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
        std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
        std::unique_ptr<Test::XdgToplevelSessionV1> toplevelSession(session->restore(shellSurface.get(), QStringLiteral("foo")));

        QSignalSpy toplevelSessionRestoredSpy(toplevelSession.get(), &Test::XdgToplevelSessionV1::restored);
        QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
        QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        QCOMPARE(toplevelSessionRestoredSpy.count(), 1);

        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.first()[0].toUInt());
        auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
        shellSurface.reset();
        QVERIFY(Test::waitForWindowClosed(window));
    }

    // Restore a toplevel session that does not exist in the session storage.
    {
        std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
        std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
        std::unique_ptr<Test::XdgToplevelSessionV1> toplevelSession(session->restore(shellSurface.get(), QStringLiteral("bar")));

        QSignalSpy toplevelSessionRestoredSpy(toplevelSession.get(), &Test::XdgToplevelSessionV1::restored);
        QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
        QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        QCOMPARE(toplevelSessionRestoredSpy.count(), 0);

        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.first()[0].toUInt());
        auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
        shellSurface.reset();
        QVERIFY(Test::waitForWindowClosed(window));
    }
}

void TestXdgSession::restoreMappedToplevelSession()
{
    // This test verifies that a protocol error will be posted if a client attempts to restore a mapped toplevel.

    std::unique_ptr<Test::XdgSessionV1> session(Test::createXdgSessionV1(Test::XdgSessionManagerV1::reason_launch));
    QSignalSpy sessionCreatedSpy(session.get(), &Test::XdgSessionV1::created);
    QVERIFY(sessionCreatedSpy.wait());

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    std::unique_ptr<Test::XdgToplevelSessionV1> toplevelSession(session->restore(shellSurface.get(), QStringLiteral("foo")));

    QSignalSpy connectionErrorSpy(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
    QVERIFY(connectionErrorSpy.wait());
}

void TestXdgSession::replaceSession()
{
    // A naughty client attempts to create a session twice -> a protocol error and lots of spanking.
    {
        auto connection = Test::Connection::setup(Test::AdditionalWaylandInterface::XdgSessionV1);

        std::unique_ptr<Test::XdgSessionV1> firstSession = Test::createXdgSessionV1(connection->sessionManager.get(), Test::XdgSessionManagerV1::reason_launch);
        QSignalSpy sessionCreatedSpy(firstSession.get(), &Test::XdgSessionV1::created);
        QVERIFY(sessionCreatedSpy.wait());

        const QString sessionId = sessionCreatedSpy.last().at(0).toString();
        std::unique_ptr<Test::XdgSessionV1> secondSession = Test::createXdgSessionV1(connection->sessionManager.get(), Test::XdgSessionManagerV1::reason_launch, sessionId);
        QSignalSpy connectionErrorSpy(connection->connection, &KWayland::Client::ConnectionThread::errorOccurred);
        QVERIFY(connectionErrorSpy.wait());
    }

    // A client creates a session that's in use by another client -> send the replaced event to the other client.
    {
        auto firstConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::XdgSessionV1);
        auto secondConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::XdgSessionV1);

        std::unique_ptr<Test::XdgSessionV1> firstSession = Test::createXdgSessionV1(firstConnection->sessionManager.get(), Test::XdgSessionManagerV1::reason_launch);
        QSignalSpy firstSessionCreatedSpy(firstSession.get(), &Test::XdgSessionV1::created);
        QSignalSpy firstSessionReplacedSpy(firstSession.get(), &Test::XdgSessionV1::replaced);
        QVERIFY(firstSessionCreatedSpy.wait());
        QCOMPARE(firstSessionReplacedSpy.count(), 0);
        QCOMPARE(firstSessionReplacedSpy.count(), 0);

        const QString sessionId = firstSessionCreatedSpy.last().at(0).toString();
        std::unique_ptr<Test::XdgSessionV1> secondSession = Test::createXdgSessionV1(secondConnection->sessionManager.get(), Test::XdgSessionManagerV1::reason_launch, sessionId);
        QSignalSpy secondSessionCreatedSpy(secondSession.get(), &Test::XdgSessionV1::created);
        QSignalSpy secondSessionReplacedSpy(secondSession.get(), &Test::XdgSessionV1::replaced);
        QVERIFY(secondSessionCreatedSpy.wait());
        QCOMPARE(secondSessionReplacedSpy.count(), 0);

        // The first and the second connection can process events in different order, the sync is needed
        // to guarantee that the first connection has processed all events before checking the signal spies.
        // Without the sync, the test will be flaky.
        QVERIFY(firstConnection->sync());
        QCOMPARE(firstSessionCreatedSpy.count(), 1);
        QCOMPARE(firstSessionReplacedSpy.count(), 1);
    }
}

WAYLANDTEST_MAIN(TestXdgSession)
#include "xdgsession_test.moc"
