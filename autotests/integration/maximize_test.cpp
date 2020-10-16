/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_client.h"
#include "cursor.h"
#include "decorations/decorationbridge.h"
#include "decorations/settings.h"
#include "platform.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/xdgdecoration.h>
#include <KWayland/Client/plasmashell.h>

#include <KDecoration2/DecoratedClient>
#include <KDecoration2/Decoration>
#include <KDecoration2/DecorationSettings>

using namespace KWin;
using namespace KWayland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_maximized-0");

class TestMaximized : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testMaximizedPassedToDeco();
    void testInitiallyMaximized();
    void testInitiallyMaximizedBorderless();
    void testBorderlessMaximizedWindow();
    void testBorderlessMaximizedWindowNoClientSideDecoration();
    void testMaximizedGainFocusAndBeActivated();
};

void TestMaximized::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void TestMaximized::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Decoration |
                                         Test::AdditionalWaylandInterface::XdgDecoration |
                                         Test::AdditionalWaylandInterface::PlasmaShell));

    screens()->setCurrent(0);
    KWin::Cursors::self()->mouse()->setPos(QPoint(1280, 512));
}

void TestMaximized::cleanup()
{
    Test::destroyWaylandConnection();

    // adjust config
    auto group = kwinApp()->config()->group("Windows");
    group.writeEntry("BorderlessMaximizedWindows", false);
    group.sync();
    Workspace::self()->slotReconfigure();
    QCOMPARE(options->borderlessMaximizedWindows(), false);
}

void TestMaximized::testMaximizedPassedToDeco()
{
    // this test verifies that when a XdgShellClient gets maximized the Decoration receives the signal

    // Create the test client.
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QScopedPointer<ServerSideDecoration> ssd(Test::waylandServerSideDecoration()->create(surface.data()));

    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isDecorated());

    auto decoration = client->decoration();
    QVERIFY(decoration);
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);

    // Wait for configure event that signals the client is active now.
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);

    // When there are no borders, there is no change to them when maximizing.
    // TODO: we should test both cases with fixed fake decoration for autotests.
    const bool hasBorders = Decoration::DecorationBridge::self()->settings()->borderSize() != KDecoration2::BorderSize::None;

    // now maximize
    QSignalSpy bordersChangedSpy(decoration, &KDecoration2::Decoration::bordersChanged);
    QVERIFY(bordersChangedSpy.isValid());
    QSignalSpy maximizedChangedSpy(decoration->client().toStrongRef().data(), &KDecoration2::DecoratedClient::maximizedChanged);
    QVERIFY(maximizedChangedSpy.isValid());
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());

    workspace()->slotWindowMaximize();
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 2);
    QCOMPARE(configureRequestedSpy.last().at(0).toSize(), QSize(1280, 1024 - decoration->borderTop()));
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), configureRequestedSpy.last().at(0).toSize(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());

    // If no borders, there is only the initial geometry shape change, but none through border resizing.
    QCOMPARE(frameGeometryChangedSpy.count(), hasBorders ? 2 : 1);
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(maximizedChangedSpy.count(), 1);
    QCOMPARE(maximizedChangedSpy.last().first().toBool(), true);
    QCOMPARE(bordersChangedSpy.count(), hasBorders ? 1 : 0);
    QCOMPARE(decoration->borderLeft(), 0);
    QCOMPARE(decoration->borderBottom(), 0);
    QCOMPARE(decoration->borderRight(), 0);
    QVERIFY(decoration->borderTop() != 0);

    // now unmaximize again
    workspace()->slotWindowMaximize();
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 3);
    QCOMPARE(configureRequestedSpy.last().at(0).toSize(), QSize(100, 50));

    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), QSize(100, 50), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(frameGeometryChangedSpy.count(), hasBorders ? 4 : 2);
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(maximizedChangedSpy.count(), 2);
    QCOMPARE(maximizedChangedSpy.last().first().toBool(), false);
    QCOMPARE(bordersChangedSpy.count(), hasBorders ? 2 : 0);
    QVERIFY(decoration->borderTop() != 0);
    QVERIFY(decoration->borderLeft() != !hasBorders);
    QVERIFY(decoration->borderRight() != !hasBorders);
    QVERIFY(decoration->borderBottom() != !hasBorders);

    // Destroy the test client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestMaximized::testInitiallyMaximized()
{
    // This test verifies that a window created as maximized, will be maximized.

    // Create the test client.
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(
        Test::createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    shellSurface->setMaximized(true);
    surface->commit(Surface::CommitFlag::None);

    // Wait for the initial configure event.
    XdgShellSurface::States states;
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));

    // Now let's render in an incorrect size.
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QCOMPARE(client->frameGeometry(), QRect(0, 0, 100, 50));
    QEXPECT_FAIL("", "Should go out of maximzied", Continue);
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);

    // Destroy the client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestMaximized::testInitiallyMaximizedBorderless()
{
    // This test verifies that a window created as maximized, will be maximized and without Border with BorderlessMaximizedWindows

    // adjust config
    auto group = kwinApp()->config()->group("Windows");
    group.writeEntry("BorderlessMaximizedWindows", true);
    group.sync();
    Workspace::self()->slotReconfigure();
    QCOMPARE(options->borderlessMaximizedWindows(), true);

    // Create the test client.
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(
        Test::createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<XdgDecoration> decoration(
        Test::xdgDecorationManager()->getToplevelDecoration(shellSurface.data()));

    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    shellSurface->setMaximized(true);
    QSignalSpy decorationConfiguredSpy(decoration.data(), &XdgDecoration::modeChanged);
    QVERIFY(decorationConfiguredSpy.isValid());
    decoration->setMode(XdgDecoration::Mode::ServerSide);
    surface->commit(Surface::CommitFlag::None);

    // Wait for the initial configure event.
    XdgShellSurface::States states;
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);
    QCOMPARE(configureRequestedSpy.last().at(0).toSize(), QSize(1280, 1024));
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));

    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(client);
    QVERIFY(!client->isDecorated());
    QVERIFY(client->isActive());
    QVERIFY(client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->frameGeometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(decoration->mode(), XdgDecoration::Mode::ServerSide);

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}
void TestMaximized::testBorderlessMaximizedWindow()
{
    // This test verifies that a maximized client looses it's server-side
    // decoration when the borderless maximized option is on.

    // Enable the borderless maximized windows option.
    auto group = kwinApp()->config()->group("Windows");
    group.writeEntry("BorderlessMaximizedWindows", true);
    group.sync();
    Workspace::self()->slotReconfigure();
    QCOMPARE(options->borderlessMaximizedWindows(), true);

    // Create the test client.
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(
        Test::createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<XdgDecoration> decoration(
        Test::xdgDecorationManager()->getToplevelDecoration(shellSurface.data()));
    QSignalSpy decorationConfiguredSpy(decoration.data(), &XdgDecoration::modeChanged);
    QVERIFY(decorationConfiguredSpy.isValid());
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    decoration->setMode(XdgDecoration::Mode::ServerSide);
    surface->commit(Surface::CommitFlag::None);

    // Wait for the initial configure event.
    XdgShellSurface::States states;
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);
    QCOMPARE(configureRequestedSpy.last().at(0).toSize(), QSize(0, 0));
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Maximized));

    // Map the client.
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->isDecorated(), true);

    // We should receive a configure event when the client becomes active.
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 2);
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Maximized));

    // Maximize the client.
    const QRect maximizeRestoreGeometry = client->frameGeometry();
    workspace()->slotWindowMaximize();
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 3);
    QCOMPARE(configureRequestedSpy.last().at(0).toSize(), QSize(1280, 1024));
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));

    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->frameGeometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->isDecorated(), false);

    // Restore the client.
    workspace()->slotWindowMaximize();
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 4);
    QCOMPARE(configureRequestedSpy.last().at(0).toSize(), QSize(100, 50));
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Maximized));

    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), QSize(100, 50), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->frameGeometry(), maximizeRestoreGeometry);
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->isDecorated(), true);

    // Destroy the client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestMaximized::testBorderlessMaximizedWindowNoClientSideDecoration()
{
    // test case verifies that borderless maximized windows doesn't cause
    // clients to render client-side decorations instead (BUG 405385)

    XdgShellSurface::States states;

    // adjust config
    auto group = kwinApp()->config()->group("Windows");
    group.writeEntry("BorderlessMaximizedWindows", true);
    group.sync();
    Workspace::self()->slotReconfigure();
    QCOMPARE(options->borderlessMaximizedWindows(), true);

    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> xdgShellSurface(Test::createXdgShellStableSurface(surface.data()));
    QScopedPointer<XdgDecoration> deco(Test::xdgDecorationManager()->getToplevelDecoration(xdgShellSurface.data()));

    QSignalSpy decorationConfiguredSpy(deco.data(), &XdgDecoration::modeChanged);
    QVERIFY(decorationConfiguredSpy.isValid());

    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client->isDecorated());
    QVERIFY(!client->noBorder());

    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    QSignalSpy configureRequestedSpy(xdgShellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());

    // Wait for the compositor to send a configure event with the Activated state.
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states & XdgShellSurface::State::Activated);

    QCOMPARE(decorationConfiguredSpy.count(), 1);
    QCOMPARE(deco->mode(), XdgDecoration::Mode::ServerSide);

    // go to maximized
    xdgShellSurface->setMaximized(true);
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 2);
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states & XdgShellSurface::State::Maximized);

    xdgShellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), configureRequestedSpy.last().first().toSize(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());

    // no deco
    QVERIFY(!client->isDecorated());
    QVERIFY(client->noBorder());
    // but still server-side
    QCOMPARE(deco->mode(), XdgDecoration::Mode::ServerSide);

    // go back to normal
    xdgShellSurface->setMaximized(false);
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 3);
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!(states & XdgShellSurface::State::Maximized));

    xdgShellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), configureRequestedSpy.last().first().toSize(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());

    QVERIFY(client->isDecorated());
    QVERIFY(!client->noBorder());
    QCOMPARE(deco->mode(), XdgDecoration::Mode::ServerSide);
}

void TestMaximized::testMaximizedGainFocusAndBeActivated()
{
    // This test verifies that a window will be raised and gain focus  when it's maximized
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> xdgShellSurface(Test::createXdgShellStableSurface(surface.data()));
    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QScopedPointer<Surface> surface2(Test::createSurface());
    QScopedPointer<XdgShellSurface> xdgShellSurface2(Test::createXdgShellStableSurface(surface2.data()));
    auto client2 = Test::renderAndWaitForShown(surface2.data(), QSize(100, 50), Qt::blue);

    QVERIFY(!client->isActive());
    QVERIFY(client2->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{client, client2}));

    workspace()->performWindowOperation(client, Options::MaximizeOp);

    QVERIFY(client->isActive());
    QVERIFY(!client2->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList<Toplevel *>{client2, client}));

    xdgShellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
    xdgShellSurface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(client2));
}

WAYLANDTEST_MAIN(TestMaximized)
#include "maximize_test.moc"
