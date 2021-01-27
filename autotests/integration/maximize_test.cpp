/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_client.h"
#include "abstract_output.h"
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
    void testInitiallyMaximizedBorderless();
    void testBorderlessMaximizedWindow();
    void testMaximizedGainFocusAndBeActivated();
};

void TestMaximized::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = kwinApp()->platform()->enabledOutputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    Test::initWaylandWorkspace();
}

void TestMaximized::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::XdgDecorationV1 |
                                         Test::AdditionalWaylandInterface::PlasmaShell));

    workspace()->setActiveOutput(QPoint(640, 512));
    KWin::Cursors::self()->mouse()->setPos(QPoint(640, 512));
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
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<Test::XdgToplevelDecorationV1> xdgDecoration(Test::createXdgToplevelDecorationV1(shellSurface.data()));

    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isDecorated());

    auto decoration = client->decoration();
    QVERIFY(decoration);
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);

    // Wait for configure event that signals the client is active now.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);

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
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(1280, 1024 - decoration->borderTop()));
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.data(), toplevelConfigureRequestedSpy.last().at(0).toSize(), Qt::red);
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
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 4);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(100, 50));

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
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
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<Test::XdgToplevelDecorationV1> decoration(Test::createXdgToplevelDecorationV1(shellSurface.data()));

    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    shellSurface->set_maximized();
    QSignalSpy decorationConfigureRequestedSpy(decoration.data(), &Test::XdgToplevelDecorationV1::configureRequested);
    decoration->set_mode(Test::XdgToplevelDecorationV1::mode_server_side);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(1280, 1024));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(client);
    QVERIFY(!client->isDecorated());
    QVERIFY(client->isActive());
    QVERIFY(client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->frameGeometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(decorationConfigureRequestedSpy.last().at(0).value<Test::XdgToplevelDecorationV1::mode>(),
             Test::XdgToplevelDecorationV1::mode_server_side);

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
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<Test::XdgToplevelDecorationV1> decoration(Test::createXdgToplevelDecorationV1(shellSurface.data()));

    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QSignalSpy decorationConfigureRequestedSpy(decoration.data(), &Test::XdgToplevelDecorationV1::configureRequested);
    decoration->set_mode(Test::XdgToplevelDecorationV1::mode_server_side);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(0, 0));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Map the client.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->isDecorated(), true);

    // We should receive a configure event when the client becomes active.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Maximize the client.
    const QRect maximizeRestoreGeometry = client->frameGeometry();
    workspace()->slotWindowMaximize();
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(1280, 1024));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->frameGeometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->isDecorated(), false);

    // Restore the client.
    workspace()->slotWindowMaximize();
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 4);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(100, 50));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
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

void TestMaximized::testMaximizedGainFocusAndBeActivated()
{
    // This test verifies that a window will be raised and gain focus  when it's maximized
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> xdgShellSurface(Test::createXdgToplevelSurface(surface.data()));
    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QScopedPointer<KWayland::Client::Surface> surface2(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> xdgShellSurface2(Test::createXdgToplevelSurface(surface2.data()));
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
