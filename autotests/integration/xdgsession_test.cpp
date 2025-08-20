/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/outputconfiguration.h"
#include "pointer_input.h"
#include "virtualdesktops.h"
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
    void restorePosition();
    void restoreOffscreenPosition();
    void restoreSize();
    void restoreKeepAbove();
    void restoreKeepBelow();
    void restoreSkipSwitcher();
    void restoreSkipPager();
    void restoreSkipTaskbar();
    void restoreMaximized_data();
    void restoreMaximized();
    void restoreFullScreen();
    void restoreMinimized();
    void restoreNoBorder();
    void restoreShortcut();
    void restoreDesktops();
    void restoreUnknownDesktops();

private:
    struct RestoreFuncs
    {
        std::function<void(Window *window)> setup;
        std::function<void()> between;
        std::function<void(Window *window)> restore;
    };
    void restoreTemplate(RestoreFuncs funcs);
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
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::XdgSessionV1));
    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));

    VirtualDesktopManager::self()->setCount(2);
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

void TestXdgSession::restoreTemplate(RestoreFuncs funcs)
{
    std::unique_ptr<Test::XdgSessionV1> session(Test::createXdgSessionV1(Test::XdgSessionManagerV1::reason_launch));
    QSignalSpy sessionCreatedSpy(session.get(), &Test::XdgSessionV1::created);
    QVERIFY(sessionCreatedSpy.wait());

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

        const QSize surfaceSize = toplevelConfigureRequestedSpy.last().at(0).toSize();
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.first()[0].toUInt());
        auto window = Test::renderAndWaitForShown(surface.get(), surfaceSize.isEmpty() ? QSize(100, 50) : surfaceSize, Qt::blue);

        funcs.setup(window);

        shellSurface.reset();
        QVERIFY(Test::waitForWindowClosed(window));
    }

    if (funcs.between) {
        funcs.between();
    }

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

        const QSize surfaceSize = toplevelConfigureRequestedSpy.last().at(0).toSize();
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.first()[0].toUInt());
        auto window = Test::renderAndWaitForShown(surface.get(), surfaceSize.isEmpty() ? QSize(100, 50) : surfaceSize, Qt::blue);

        funcs.restore(window);

        shellSurface.reset();
        QVERIFY(Test::waitForWindowClosed(window));
    }
}

void TestXdgSession::restorePosition()
{
    restoreTemplate({
        .setup = [](Window *window) {
            window->move(QPointF(42, 84));
        },
        .restore = [](Window *window) {
            QCOMPARE(window->pos(), QPointF(42, 84));
        },
    });
}

void TestXdgSession::restoreOffscreenPosition()
{
    const auto outputs = workspace()->outputs();

    restoreTemplate({
        .setup = [outputs](Window *window) {
            QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
            window->move(QPointF(1280 + 42, 42));
            QCOMPARE(window->output(), outputs[1]);
        },
        .between = [outputs]() {
            OutputConfiguration configuration;
            {
                auto changeSet = configuration.changeSet(outputs[1]);
                changeSet->enabled = false;
            }
            workspace()->applyOutputConfiguration(configuration);
        },
        .restore = [outputs](Window *window) {
            QCOMPARE(window->pos(), QPointF(0, 0));
            QCOMPARE(window->output(), outputs[0]);
        },
    });
}

void TestXdgSession::restoreSize()
{
    restoreTemplate({
        .setup = [](Window *window) {
            window->resize(QSizeF(300, 250));
        },
        .restore = [](Window *window) {
            QCOMPARE(window->size(), QSizeF(300, 250));
        },
    });
}

void TestXdgSession::restoreKeepAbove()
{
    restoreTemplate({
        .setup = [](Window *window) {
            window->setKeepAbove(true);
        },
        .restore = [](Window *window) {
            QVERIFY(window->keepAbove());
        },
    });
}

void TestXdgSession::restoreKeepBelow()
{
    restoreTemplate({
        .setup = [](Window *window) {
            window->setKeepBelow(true);
        },
        .restore = [](Window *window) {
            QVERIFY(window->keepBelow());
        },
    });
}

void TestXdgSession::restoreSkipSwitcher()
{
    restoreTemplate({
        .setup = [](Window *window) {
            window->setSkipSwitcher(true);
        },
        .restore = [](Window *window) {
            QVERIFY(window->skipSwitcher());
        },
    });
}

void TestXdgSession::restoreSkipPager()
{
    restoreTemplate({
        .setup = [](Window *window) {
            window->setSkipPager(true);
        },
        .restore = [](Window *window) {
            QVERIFY(window->skipPager());
        },
    });
}

void TestXdgSession::restoreSkipTaskbar()
{
    restoreTemplate({
        .setup = [](Window *window) {
            window->setSkipTaskbar(true);
        },
        .restore = [](Window *window) {
            QVERIFY(window->skipTaskbar());
        },
    });
}

void TestXdgSession::restoreMaximized_data()
{
    QTest::addColumn<MaximizeMode>("maximizeMode");

    QTest::addRow("none") << MaximizeMode::MaximizeRestore;
    QTest::addRow("horizontal") << MaximizeMode::MaximizeHorizontal;
    QTest::addRow("vertical") << MaximizeMode::MaximizeVertical;
    QTest::addRow("full") << MaximizeMode::MaximizeFull;
}

void TestXdgSession::restoreMaximized()
{
    QFETCH(MaximizeMode, maximizeMode);
    restoreTemplate({
        .setup = [maximizeMode](Window *window) {
            window->maximize(maximizeMode);
        },
        .restore = [maximizeMode](Window *window) {
            QCOMPARE(window->maximizeMode(), maximizeMode);
            QCOMPARE(window->requestedMaximizeMode(), maximizeMode);
        },
    });
}

void TestXdgSession::restoreFullScreen()
{
    restoreTemplate({
        .setup = [](Window *window) {
            window->setFullScreen(true);
        },
        .restore = [](Window *window) {
            QVERIFY(window->isFullScreen());
            QVERIFY(window->isRequestedFullScreen());
        },
    });
}

void TestXdgSession::restoreMinimized()
{
    restoreTemplate({
        .setup = [](Window *window) {
            window->setMinimized(true);
        },
        .restore = [](Window *window) {
            QVERIFY(window->isMinimized());
        },
    });
}

void TestXdgSession::restoreNoBorder()
{
    restoreTemplate({
        .setup = [](Window *window) {
            window->setNoBorder(true);
        },
        .restore = [](Window *window) {
            QVERIFY(window->noBorder());
        },
    });
}

void TestXdgSession::restoreShortcut()
{
    restoreTemplate({
        .setup = [](Window *window) {
            window->setShortcut(QStringLiteral("Ctrl+Meta+T"));
        },
        .restore = [](Window *window) {
            QCOMPARE(window->shortcut(), QKeySequence(Qt::ControlModifier | Qt::MetaModifier | Qt::Key_T));
        },
    });
}

void TestXdgSession::restoreDesktops()
{
    const auto desktops = VirtualDesktopManager::self()->desktops();
    restoreTemplate({
        .setup = [desktops](Window *window) {
            QVERIFY(window->isOnDesktop(desktops[0]));
            QVERIFY(!window->isOnDesktop(desktops[1]));
            window->setDesktops({desktops[1]});
        },
        .restore = [desktops](Window *window) {
            QVERIFY(!window->isOnDesktop(desktops[0]));
            QVERIFY(window->isOnDesktop(desktops[1]));
        },
    });
}

void TestXdgSession::restoreUnknownDesktops()
{
    auto vds = VirtualDesktopManager::self();
    vds->setCount(3);

    // The window is on desktops 2 and 3. If desktop 3 is removed, the window should be restored only on desktop 2.
    const auto desktops = VirtualDesktopManager::self()->desktops();
    restoreTemplate({
        .setup = [desktops](Window *window) {
            window->setDesktops({desktops[1], desktops[2]});
        },
        .between = [vds, desktops]() {
            vds->removeVirtualDesktop(desktops[2]);
        },
        .restore = [desktops](Window *window) {
            QCOMPARE(window->desktops(), (QList<VirtualDesktop *>{desktops[1]}));
        },
    });

    // The window is on desktop 2. If desktop 2 is removed, the window should be restored only on desktop 1,
    // it should not be displayed on all desktops.
    restoreTemplate({
        .setup = [desktops](Window *window) {
            window->setDesktops({desktops[1]});
        },
        .between = [vds, desktops]() {
            vds->removeVirtualDesktop(desktops[1]);
        },
        .restore = [desktops](Window *window) {
            QCOMPARE(window->desktops(), (QList<VirtualDesktop *>{desktops[0]}));
        },
    });
}

WAYLANDTEST_MAIN(TestXdgSession)
#include "xdgsession_test.moc"
