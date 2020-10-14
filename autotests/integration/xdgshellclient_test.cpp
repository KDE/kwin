/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_client.h"
#include "cursor.h"
#include "decorations/decorationbridge.h"
#include "decorations/settings.h"
#include "effects.h"
#include "deleted.h"
#include "platform.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KDecoration2/DecoratedClient>
#include <KDecoration2/Decoration>
#include <KDecoration2/DecorationSettings>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/xdgshell.h>
#include <KWayland/Client/xdgdecoration.h>
#include <KWayland/Client/appmenu.h>

#include <KWaylandServer/clientconnection.h>
#include <KWaylandServer/display.h>

#include <QDBusConnection>

// system
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <csignal>

using namespace KWin;
using namespace KWayland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_xdgshellclient-0");

class TestXdgShellClient : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testMapUnmap();
    void testDesktopPresenceChanged();
    void testWindowOutputs();
    void testMinimizeActiveWindow();
    void testFullscreen_data();
    void testFullscreen();

    void testFullscreenRestore();
    void testUserCanSetFullscreen();
    void testUserSetFullscreen();

    void testMaximizedToFullscreen_data();
    void testMaximizedToFullscreen();
    void testWindowOpensLargerThanScreen();
    void testHidden();
    void testDesktopFileName();
    void testCaptionSimplified();
    void testCaptionMultipleWindows();
    void testUnresponsiveWindow_data();
    void testUnresponsiveWindow();
    void testX11WindowId();
    void testAppMenu();
    void testNoDecorationModeRequested();
    void testSendClientWithTransientToDesktop();
    void testMinimizeWindowWithTransients();
    void testXdgDecoration_data();
    void testXdgDecoration();
    void testXdgNeverCommitted();
    void testXdgInitialState();
    void testXdgInitiallyMaximised();
    void testXdgInitiallyFullscreen();
    void testXdgInitiallyMinimized();
    void testXdgWindowGeometryIsntSet();
    void testXdgWindowGeometryAttachBuffer();
    void testXdgWindowGeometryAttachSubSurface();
    void testXdgWindowGeometryInteractiveResize();
    void testXdgWindowGeometryFullScreen();
    void testXdgWindowGeometryMaximize();
    void testPointerInputTransform();
    void testReentrantSetFrameGeometry();
    void testDoubleMaximize();
};

void TestXdgShellClient::initTestCase()
{
    qRegisterMetaType<KWin::Deleted*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    qRegisterMetaType<KWayland::Client::Output*>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void TestXdgShellClient::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Decoration |
                                         Test::AdditionalWaylandInterface::Seat |
                                         Test::AdditionalWaylandInterface::XdgDecoration |
                                         Test::AdditionalWaylandInterface::AppMenu));
    QVERIFY(Test::waitForWaylandPointer());

    screens()->setCurrent(0);
    KWin::Cursors::self()->mouse()->setPos(QPoint(1280, 512));
}

void TestXdgShellClient::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestXdgShellClient::testMapUnmap()
{
    // This test verifies that the compositor destroys XdgToplevelClient when the
    // associated xdg_toplevel surface is unmapped.

    // Create a wl_surface and an xdg_toplevel, but don't commit them yet!
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(
        Test::createXdgShellStableSurface(surface.data(), nullptr, Test::CreationSetup::CreateOnly));

    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());

    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());

    // Tell the compositor that we want to map the surface.
    surface->commit(Surface::CommitFlag::None);

    // The compositor will respond with a configure event.
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);

    // Now we can attach a buffer with actual data to the surface.
    Test::render(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(clientAddedSpy.wait());
    QCOMPARE(clientAddedSpy.count(), 1);
    AbstractClient *client = clientAddedSpy.last().first().value<AbstractClient *>();
    QVERIFY(client);
    QCOMPARE(client->readyForPainting(), true);

    // When the client becomes active, the compositor will send another configure event.
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 2);

    // Unmap the xdg_toplevel surface by committing a null buffer.
    surface->attachBuffer(Buffer::Ptr());
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(Test::waitForWindowDestroyed(client));

    // Tell the compositor that we want to re-map the xdg_toplevel surface.
    surface->commit(Surface::CommitFlag::None);

    // The compositor will respond with a configure event.
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 3);

    // Now we can attach a buffer with actual data to the surface.
    Test::render(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(clientAddedSpy.wait());
    QCOMPARE(clientAddedSpy.count(), 2);
    client = clientAddedSpy.last().first().value<AbstractClient *>();
    QVERIFY(client);
    QCOMPARE(client->readyForPainting(), true);

    // The compositor will respond with a configure event.
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 4);

    // Destroy the test client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClient::testDesktopPresenceChanged()
{
    // this test verifies that the desktop presence changed signals are properly emitted
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(c->desktop(), 1);
    effects->setNumberOfDesktops(4);
    QSignalSpy desktopPresenceChangedClientSpy(c, &AbstractClient::desktopPresenceChanged);
    QVERIFY(desktopPresenceChangedClientSpy.isValid());
    QSignalSpy desktopPresenceChangedWorkspaceSpy(workspace(), &Workspace::desktopPresenceChanged);
    QVERIFY(desktopPresenceChangedWorkspaceSpy.isValid());
    QSignalSpy desktopPresenceChangedEffectsSpy(effects, &EffectsHandler::desktopPresenceChanged);
    QVERIFY(desktopPresenceChangedEffectsSpy.isValid());

    // let's change the desktop
    workspace()->sendClientToDesktop(c, 2, false);
    QCOMPARE(c->desktop(), 2);
    QCOMPARE(desktopPresenceChangedClientSpy.count(), 1);
    QCOMPARE(desktopPresenceChangedWorkspaceSpy.count(), 1);
    QCOMPARE(desktopPresenceChangedEffectsSpy.count(), 1);

    // verify the arguments
    QCOMPARE(desktopPresenceChangedClientSpy.first().at(0).value<AbstractClient*>(), c);
    QCOMPARE(desktopPresenceChangedClientSpy.first().at(1).toInt(), 1);
    QCOMPARE(desktopPresenceChangedWorkspaceSpy.first().at(0).value<AbstractClient*>(), c);
    QCOMPARE(desktopPresenceChangedWorkspaceSpy.first().at(1).toInt(), 1);
    QCOMPARE(desktopPresenceChangedEffectsSpy.first().at(0).value<EffectWindow*>(), c->effectWindow());
    QCOMPARE(desktopPresenceChangedEffectsSpy.first().at(1).toInt(), 1);
    QCOMPARE(desktopPresenceChangedEffectsSpy.first().at(2).toInt(), 2);
}

void TestXdgShellClient::testWindowOutputs()
{
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    auto size = QSize(200,200);

    QSignalSpy outputEnteredSpy(surface.data(), &Surface::outputEntered);
    QSignalSpy outputLeftSpy(surface.data(), &Surface::outputLeft);

    auto c = Test::renderAndWaitForShown(surface.data(), size, Qt::blue);
    //move to be in the first screen
    c->setFrameGeometry(QRect(QPoint(100,100), size));
    //we don't don't know where the compositor first placed this window,
    //this might fire, it might not
    outputEnteredSpy.wait(5);
    outputEnteredSpy.clear();

    QCOMPARE(surface->outputs().count(), 1);
    QCOMPARE(surface->outputs().first()->globalPosition(), QPoint(0,0));

    //move to overlapping both first and second screen
    c->setFrameGeometry(QRect(QPoint(1250,100), size));
    QVERIFY(outputEnteredSpy.wait());
    QCOMPARE(outputEnteredSpy.count(), 1);
    QCOMPARE(outputLeftSpy.count(), 0);
    QCOMPARE(surface->outputs().count(), 2);
    QVERIFY(surface->outputs()[0] != surface->outputs()[1]);

    //move entirely into second screen
    c->setFrameGeometry(QRect(QPoint(1400,100), size));
    QVERIFY(outputLeftSpy.wait());
    QCOMPARE(outputEnteredSpy.count(), 1);
    QCOMPARE(outputLeftSpy.count(), 1);
    QCOMPARE(surface->outputs().count(), 1);
    QCOMPARE(surface->outputs().first()->globalPosition(), QPoint(1280,0));
}

void TestXdgShellClient::testMinimizeActiveWindow()
{
    // this test verifies that when minimizing the active window it gets deactivated
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<QObject> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QVERIFY(c->isActive());
    QCOMPARE(workspace()->activeClient(), c);
    QVERIFY(c->wantsInput());
    QVERIFY(c->wantsTabFocus());
    QVERIFY(c->isShown(true));

    workspace()->slotWindowMinimize();
    QVERIFY(!c->isShown(true));
    QVERIFY(c->wantsInput());
    QVERIFY(c->wantsTabFocus());
    QVERIFY(!c->isActive());
    QVERIFY(!workspace()->activeClient());
    QVERIFY(c->isMinimized());

    // unminimize again
    c->unminimize();
    QVERIFY(!c->isMinimized());
    QVERIFY(c->isActive());
    QVERIFY(c->wantsInput());
    QVERIFY(c->wantsTabFocus());
    QVERIFY(c->isShown(true));
    QCOMPARE(workspace()->activeClient(), c);
}

void TestXdgShellClient::testFullscreen_data()
{
    QTest::addColumn<ServerSideDecoration::Mode>("decoMode");

    QTest::newRow("client-side deco") << ServerSideDecoration::Mode::Client;
    QTest::newRow("server-side deco") << ServerSideDecoration::Mode::Server;
}

void TestXdgShellClient::testFullscreen()
{
    // this test verifies that a window can be properly fullscreened

    XdgShellSurface::States states;

    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(shellSurface);

    // create deco
    QScopedPointer<ServerSideDecoration> deco(Test::waylandServerSideDecoration()->create(surface.data()));
    QSignalSpy decoSpy(deco.data(), &ServerSideDecoration::modeChanged);
    QVERIFY(decoSpy.isValid());
    QVERIFY(decoSpy.wait());
    QFETCH(ServerSideDecoration::Mode, decoMode);
    deco->requestMode(decoMode);
    QVERIFY(decoSpy.wait());
    QCOMPARE(deco->mode(), decoMode);

    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->layer(), NormalLayer);
    QVERIFY(!client->isFullScreen());
    QCOMPARE(client->clientSize(), QSize(100, 50));
    QCOMPARE(client->isDecorated(), decoMode == ServerSideDecoration::Mode::Server);
    QCOMPARE(client->clientSizeToFrameSize(client->clientSize()), client->size());

    QSignalSpy fullScreenChangedSpy(client, &AbstractClient::fullScreenChanged);
    QVERIFY(fullScreenChangedSpy.isValid());
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());

    // Wait for the compositor to send a configure event with the Activated state.
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states & XdgShellSurface::State::Activated);

    // Ask the compositor to show the window in full screen mode.
    shellSurface->setFullscreen(true);
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 2);
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states & XdgShellSurface::State::Fullscreen);
    QCOMPARE(configureRequestedSpy.last().at(0).value<QSize>(), screens()->size(0));

    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), configureRequestedSpy.last().at(0).value<QSize>(), Qt::red);

#if 0 // TODO: Uncomment when full screen state updates are truly asynchronous.
    QVERIFY(fullScreenChangedSpy.wait());
    QCOMPARE(fullScreenChangedSpy.count(), 1);
#else
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(fullScreenChangedSpy.count(), 1);
#endif
    QVERIFY(client->isFullScreen());
    QVERIFY(!client->isDecorated());
    QCOMPARE(client->layer(), ActiveLayer);
    QCOMPARE(client->frameGeometry(), QRect(QPoint(0, 0), screens()->size(0)));

    // Ask the compositor to show the window in normal mode.
    shellSurface->setFullscreen(false);
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 3);
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!(states & XdgShellSurface::State::Fullscreen));
    QCOMPARE(configureRequestedSpy.last().at(0).value<QSize>(), QSize(100, 50));

    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), configureRequestedSpy.last().at(0).value<QSize>(), Qt::blue);

#if 0 // TODO: Uncomment when full screen state updates are truly asynchronous.
    QVERIFY(fullScreenChangedSpy.wait());
    QCOMPARE(fullScreenChangedSpy.count(), 2);
#else
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(fullScreenChangedSpy.count(), 2);
#endif
    QCOMPARE(client->clientSize(), QSize(100, 50));
    QVERIFY(!client->isFullScreen());
    QCOMPARE(client->isDecorated(), decoMode == ServerSideDecoration::Mode::Server);
    QCOMPARE(client->layer(), NormalLayer);

    // Destroy the client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClient::testFullscreenRestore()
{
    // this test verifies that windows created fullscreen can be later properly restored
    QScopedPointer<Surface> surface(Test::createSurface());
    XdgShellSurface *xdgShellSurface = Test::createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly);
    QSignalSpy configureRequestedSpy(xdgShellSurface, &XdgShellSurface::configureRequested);

    // fullscreen the window
    xdgShellSurface->setFullscreen(true);
    surface->commit(Surface::CommitFlag::None);

    configureRequestedSpy.wait();
    QCOMPARE(configureRequestedSpy.count(), 1);

    const auto size = configureRequestedSpy.first()[0].value<QSize>();
    const auto state = configureRequestedSpy.first()[1].value<KWayland::Client::XdgShellSurface::States>();

    QCOMPARE(size, screens()->size(0));
    QVERIFY(state & KWayland::Client::XdgShellSurface::State::Fullscreen);
    xdgShellSurface->ackConfigure(configureRequestedSpy.first()[2].toUInt());

    auto c = Test::renderAndWaitForShown(surface.data(), size, Qt::blue);
    QVERIFY(c);
    QVERIFY(c->isFullScreen());

    configureRequestedSpy.wait(100);

    QSignalSpy fullscreenChangedSpy(c, &AbstractClient::fullScreenChanged);
    QVERIFY(fullscreenChangedSpy.isValid());
    QSignalSpy frameGeometryChangedSpy(c, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());

    // swap back to normal
    configureRequestedSpy.clear();
    xdgShellSurface->setFullscreen(false);

    QVERIFY(fullscreenChangedSpy.wait());
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.last().first().toSize(), QSize(0, 0));
    QVERIFY(!c->isFullScreen());

    for (const auto &it: configureRequestedSpy) {
        xdgShellSurface->ackConfigure(it[2].toUInt());
    }

    Test::render(surface.data(), QSize(100, 50), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(frameGeometryChangedSpy.count(), 1);
    QVERIFY(!c->isFullScreen());
    QCOMPARE(c->frameGeometry().size(), QSize(100, 50));
}

void TestXdgShellClient::testUserCanSetFullscreen()
{
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QVERIFY(c->isActive());
    QVERIFY(!c->isFullScreen());
    QVERIFY(c->userCanSetFullScreen());
}

void TestXdgShellClient::testUserSetFullscreen()
{
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(
        surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    QVERIFY(!shellSurface.isNull());

    // wait for the initial configure event
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);

    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QVERIFY(c->isActive());
    QVERIFY(!c->isFullScreen());

    // The client gets activated, which gets another configure event. Though that's not relevant to the test
    configureRequestedSpy.wait(10);

    QSignalSpy fullscreenChangedSpy(c, &AbstractClient::fullScreenChanged);
    QVERIFY(fullscreenChangedSpy.isValid());
    c->setFullScreen(true);
    QCOMPARE(c->isFullScreen(), true);
    configureRequestedSpy.clear();
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);
    QCOMPARE(configureRequestedSpy.first().at(0).toSize(), screens()->size(0));
    const auto states = configureRequestedSpy.first().at(1).value<KWayland::Client::XdgShellSurface::States>();
    QVERIFY(states.testFlag(KWayland::Client::XdgShellSurface::State::Fullscreen));
    QVERIFY(states.testFlag(KWayland::Client::XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(KWayland::Client::XdgShellSurface::State::Maximized));
    QVERIFY(!states.testFlag(KWayland::Client::XdgShellSurface::State::Resizing));
    QCOMPARE(fullscreenChangedSpy.count(), 1);
    QVERIFY(c->isFullScreen());

    shellSurface->ackConfigure(configureRequestedSpy.first().at(2).value<quint32>());

    // unset fullscreen again
    c->setFullScreen(false);
    QCOMPARE(c->isFullScreen(), false);
    configureRequestedSpy.clear();
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);
    QCOMPARE(configureRequestedSpy.first().at(0).toSize(), QSize(100, 50));
    QVERIFY(!configureRequestedSpy.first().at(1).value<KWayland::Client::XdgShellSurface::States>().testFlag(KWayland::Client::XdgShellSurface::State::Fullscreen));
    QCOMPARE(fullscreenChangedSpy.count(), 2);
    QVERIFY(!c->isFullScreen());
}

void TestXdgShellClient::testMaximizedToFullscreen_data()
{
    QTest::addColumn<ServerSideDecoration::Mode>("decoMode");

    QTest::newRow("client-side deco") << ServerSideDecoration::Mode::Client;
    QTest::newRow("server-side deco") << ServerSideDecoration::Mode::Server;
}

void TestXdgShellClient::testMaximizedToFullscreen()
{
    // this test verifies that a window can be properly fullscreened after maximizing

    XdgShellSurface::States states;

    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(shellSurface);

    // create deco
    QScopedPointer<ServerSideDecoration> deco(Test::waylandServerSideDecoration()->create(surface.data()));
    QSignalSpy decoSpy(deco.data(), &ServerSideDecoration::modeChanged);
    QVERIFY(decoSpy.isValid());
    QVERIFY(decoSpy.wait());
    QFETCH(ServerSideDecoration::Mode, decoMode);
    deco->requestMode(decoMode);
    QVERIFY(decoSpy.wait());
    QCOMPARE(deco->mode(), decoMode);

    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(!client->isFullScreen());
    QCOMPARE(client->clientSize(), QSize(100, 50));
    QCOMPARE(client->isDecorated(), decoMode == ServerSideDecoration::Mode::Server);

    QSignalSpy fullscreenChangedSpy(client, &AbstractClient::fullScreenChanged);
    QVERIFY(fullscreenChangedSpy.isValid());
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());

    // Wait for the compositor to send a configure event with the Activated state.
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states & XdgShellSurface::State::Activated);

    // Ask the compositor to maximize the window.
    shellSurface->setMaximized(true);
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 2);
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states & XdgShellSurface::State::Maximized);

    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), configureRequestedSpy.last().at(0).value<QSize>(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->maximizeMode(), MaximizeFull);

    // Ask the compositor to show the window in full screen mode.
    shellSurface->setFullscreen(true);
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 3);
    QCOMPARE(configureRequestedSpy.last().at(0).value<QSize>(), screens()->size(0));
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states & XdgShellSurface::State::Maximized);
    QVERIFY(states & XdgShellSurface::State::Fullscreen);

    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), configureRequestedSpy.last().at(0).value<QSize>(), Qt::red);

#if 0 // TODO: Uncomment when full screen changes are truly asynchronous.
    QVERIFY(fullScreenChangedSpy.wait());
    QCOMPARE(fullScreenChangedSpy.count(), 1);
#else
    QTRY_COMPARE(fullscreenChangedSpy.count(), 1);
#endif
    QCOMPARE(client->maximizeMode(), MaximizeFull);
    QVERIFY(client->isFullScreen());
    QVERIFY(!client->isDecorated());

    // Switch back to normal mode.
    shellSurface->setFullscreen(false);
    shellSurface->setMaximized(false);
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 4);
    QCOMPARE(configureRequestedSpy.last().at(0).value<QSize>(), QSize(100, 50));
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!(states & XdgShellSurface::State::Maximized));
    QVERIFY(!(states & XdgShellSurface::State::Fullscreen));

    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), configureRequestedSpy.last().at(0).value<QSize>(), Qt::red);

    QVERIFY(frameGeometryChangedSpy.wait());
    QVERIFY(!client->isFullScreen());
    QCOMPARE(client->isDecorated(), decoMode == ServerSideDecoration::Mode::Server);
    QCOMPARE(client->maximizeMode(), MaximizeRestore);

    // Destroy the client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClient::testWindowOpensLargerThanScreen()
{
    // this test creates a window which is as large as the screen, but is decorated
    // the window should get resized to fit into the screen, BUG: 366632
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QSignalSpy sizeChangeRequestedSpy(shellSurface.data(), SIGNAL(sizeChanged(QSize)));
    QVERIFY(sizeChangeRequestedSpy.isValid());

    // create deco
    QScopedPointer<ServerSideDecoration> deco(Test::waylandServerSideDecoration()->create(surface.data()));
    QSignalSpy decoSpy(deco.data(), &ServerSideDecoration::modeChanged);
    QVERIFY(decoSpy.isValid());
    QVERIFY(decoSpy.wait());
    deco->requestMode(ServerSideDecoration::Mode::Server);
    QVERIFY(decoSpy.wait());
    QCOMPARE(deco->mode(), ServerSideDecoration::Mode::Server);

    auto c = Test::renderAndWaitForShown(surface.data(), screens()->size(0), Qt::blue);
    QVERIFY(c);
    QVERIFY(c->isActive());
    QVERIFY(c->isDecorated());
    QEXPECT_FAIL("", "BUG 366632", Continue);
    QCOMPARE(c->frameGeometry(), QRect(QPoint(0, 0), screens()->size(0)));
}

void TestXdgShellClient::testHidden()
{
    // this test verifies that when hiding window it doesn't get shown
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<QObject> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QVERIFY(c->isActive());
    QCOMPARE(workspace()->activeClient(), c);
    QVERIFY(c->wantsInput());
    QVERIFY(c->wantsTabFocus());
    QVERIFY(c->isShown(true));

    c->hideClient(true);
    QVERIFY(!c->isShown(true));
    QVERIFY(!c->isActive());
    QVERIFY(c->wantsInput());
    QVERIFY(c->wantsTabFocus());

    // unhide again
    c->hideClient(false);
    QVERIFY(c->isShown(true));
    QVERIFY(c->wantsInput());
    QVERIFY(c->wantsTabFocus());

    //QCOMPARE(workspace()->activeClient(), c);
}

void TestXdgShellClient::testDesktopFileName()
{
    QIcon::setThemeName(QStringLiteral("breeze"));
    // this test verifies that desktop file name is passed correctly to the window
    QScopedPointer<Surface> surface(Test::createSurface());
    // only xdg-shell as ShellSurface misses the setter
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    shellSurface->setAppId(QByteArrayLiteral("org.kde.foo"));
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(c->desktopFileName(), QByteArrayLiteral("org.kde.foo"));
    QCOMPARE(c->resourceClass(), QByteArrayLiteral("org.kde.foo"));
    QVERIFY(c->resourceName().startsWith("testXdgShellClient"));
    // the desktop file does not exist, so icon should be generic Wayland
    QCOMPARE(c->icon().name(), QStringLiteral("wayland"));

    QSignalSpy desktopFileNameChangedSpy(c, &AbstractClient::desktopFileNameChanged);
    QVERIFY(desktopFileNameChangedSpy.isValid());
    QSignalSpy iconChangedSpy(c, &AbstractClient::iconChanged);
    QVERIFY(iconChangedSpy.isValid());
    shellSurface->setAppId(QByteArrayLiteral("org.kde.bar"));
    QVERIFY(desktopFileNameChangedSpy.wait());
    QCOMPARE(c->desktopFileName(), QByteArrayLiteral("org.kde.bar"));
    QCOMPARE(c->resourceClass(), QByteArrayLiteral("org.kde.bar"));
    QVERIFY(c->resourceName().startsWith("testXdgShellClient"));
    // icon should still be wayland
    QCOMPARE(c->icon().name(), QStringLiteral("wayland"));
    QVERIFY(iconChangedSpy.isEmpty());

    const QString dfPath = QFINDTESTDATA("data/example.desktop");
    shellSurface->setAppId(dfPath.toUtf8());
    QVERIFY(desktopFileNameChangedSpy.wait());
    QCOMPARE(iconChangedSpy.count(), 1);
    QCOMPARE(QString::fromUtf8(c->desktopFileName()), dfPath);
    QCOMPARE(c->icon().name(), QStringLiteral("kwin"));
}

void TestXdgShellClient::testCaptionSimplified()
{
    // this test verifies that caption is properly trimmed
    // see BUG 323798 comment #12
    QScopedPointer<Surface> surface(Test::createSurface());
    // only done for xdg-shell as ShellSurface misses the setter
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    const QString origTitle = QString::fromUtf8(QByteArrayLiteral("Was tun, wenn Schüler Autismus haben?\342\200\250\342\200\250\342\200\250 – Marlies Hübner - Mozilla Firefox"));
    shellSurface->setTitle(origTitle);
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QVERIFY(c->caption() != origTitle);
    QCOMPARE(c->caption(), origTitle.simplified());
}

void TestXdgShellClient::testCaptionMultipleWindows()
{
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    shellSurface->setTitle(QStringLiteral("foo"));
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(c->caption(), QStringLiteral("foo"));
    QCOMPARE(c->captionNormal(), QStringLiteral("foo"));
    QCOMPARE(c->captionSuffix(), QString());

    QScopedPointer<Surface> surface2(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface2(Test::createXdgShellStableSurface(surface2.data()));
    shellSurface2->setTitle(QStringLiteral("foo"));
    auto c2 = Test::renderAndWaitForShown(surface2.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c2);
    QCOMPARE(c2->caption(), QStringLiteral("foo <2>"));
    QCOMPARE(c2->captionNormal(), QStringLiteral("foo"));
    QCOMPARE(c2->captionSuffix(), QStringLiteral(" <2>"));

    QScopedPointer<Surface> surface3(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface3(Test::createXdgShellStableSurface(surface3.data()));
    shellSurface3->setTitle(QStringLiteral("foo"));
    auto c3 = Test::renderAndWaitForShown(surface3.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c3);
    QCOMPARE(c3->caption(), QStringLiteral("foo <3>"));
    QCOMPARE(c3->captionNormal(), QStringLiteral("foo"));
    QCOMPARE(c3->captionSuffix(), QStringLiteral(" <3>"));

    QScopedPointer<Surface> surface4(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface4(Test::createXdgShellStableSurface(surface4.data()));
    shellSurface4->setTitle(QStringLiteral("bar"));
    auto c4 = Test::renderAndWaitForShown(surface4.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c4);
    QCOMPARE(c4->caption(), QStringLiteral("bar"));
    QCOMPARE(c4->captionNormal(), QStringLiteral("bar"));
    QCOMPARE(c4->captionSuffix(), QString());
    QSignalSpy captionChangedSpy(c4, &AbstractClient::captionChanged);
    QVERIFY(captionChangedSpy.isValid());
    shellSurface4->setTitle(QStringLiteral("foo"));
    QVERIFY(captionChangedSpy.wait());
    QCOMPARE(captionChangedSpy.count(), 1);
    QCOMPARE(c4->caption(), QStringLiteral("foo <4>"));
    QCOMPARE(c4->captionNormal(), QStringLiteral("foo"));
    QCOMPARE(c4->captionSuffix(), QStringLiteral(" <4>"));
}

void TestXdgShellClient::testUnresponsiveWindow_data()
{
    QTest::addColumn<QString>("shellInterface");//see env selection in qwaylandintegration.cpp
    QTest::addColumn<bool>("socketMode");

    QTest::newRow("xdg display") << "xdg-shell" << false;
    QTest::newRow("xdg socket") << "xdg-shell" << true;
}

void TestXdgShellClient::testUnresponsiveWindow()
{
    // this test verifies that killWindow properly terminates a process
    // for this an external binary is launched
    const QString kill = QFINDTESTDATA(QStringLiteral("kill"));
    QVERIFY(!kill.isEmpty());
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());

    QScopedPointer<QProcess> process(new QProcess);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    QFETCH(QString, shellInterface);
    QFETCH(bool, socketMode);
    env.insert("QT_WAYLAND_SHELL_INTEGRATION", shellInterface);
    if (socketMode) {
        int sx[2];
        QVERIFY(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) >= 0);
        waylandServer()->display()->createClient(sx[0]);
        int socket = dup(sx[1]);
        QVERIFY(socket != -1);
        env.insert(QStringLiteral("WAYLAND_SOCKET"), QByteArray::number(socket));
        env.remove("WAYLAND_DISPLAY");
    } else {
        env.insert("WAYLAND_DISPLAY", s_socketName);
    }
    process->setProcessEnvironment(env);
    process->setProcessChannelMode(QProcess::ForwardedChannels);
    process->setProgram(kill);
    QSignalSpy processStartedSpy{process.data(), &QProcess::started};
    QVERIFY(processStartedSpy.isValid());
    process->start();
    QVERIFY(processStartedSpy.wait());

    AbstractClient *killClient = nullptr;
    if (clientAddedSpy.isEmpty()) {
        QVERIFY(clientAddedSpy.wait());
    }
    ::kill(process->processId(), SIGUSR1); // send a signal to freeze the process

    killClient = clientAddedSpy.first().first().value<AbstractClient*>();
    QVERIFY(killClient);
    QSignalSpy unresponsiveSpy(killClient, &AbstractClient::unresponsiveChanged);
    QSignalSpy killedSpy(process.data(), static_cast<void(QProcess::*)(int,QProcess::ExitStatus)>(&QProcess::finished));
    QSignalSpy deletedSpy(killClient, &QObject::destroyed);

    qint64 startTime = QDateTime::currentMSecsSinceEpoch();

    //wait for the process to be frozen
    QTest::qWait(10);

    //pretend the user clicked the close button
    killClient->closeWindow();

    //client should not yet be marked unresponsive nor killed
    QVERIFY(!killClient->unresponsive());
    QVERIFY(killedSpy.isEmpty());

    QVERIFY(unresponsiveSpy.wait());
    //client should be marked unresponsive but not killed
    auto elapsed1 = QDateTime::currentMSecsSinceEpoch() - startTime;
    QVERIFY(elapsed1 > 900  && elapsed1 < 1200); //ping timer is 1s, but coarse timers on a test across two processes means we need a fuzzy compare
    QVERIFY(killClient->unresponsive());
    QVERIFY(killedSpy.isEmpty());

    QVERIFY(deletedSpy.wait());
    if (!socketMode) {
        //process was killed - because we're across process this could happen in either order
        QVERIFY(killedSpy.count() || killedSpy.wait());
    }

    auto elapsed2 = QDateTime::currentMSecsSinceEpoch() - startTime;
    QVERIFY(elapsed2 > 1800); //second ping comes in a second later
}

void TestXdgShellClient::testX11WindowId()
{
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QVERIFY(c->windowId() != 0);
    QCOMPARE(c->window(), 0u);
}

void TestXdgShellClient::testAppMenu()
{
    //register a faux appmenu client
    QVERIFY (QDBusConnection::sessionBus().registerService("org.kde.kappmenu"));

    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QScopedPointer<AppMenu> menu(Test::waylandAppMenuManager()->create(surface.data()));
    QSignalSpy spy(c, &AbstractClient::hasApplicationMenuChanged);
    menu->setAddress("service.name", "object/path");
    spy.wait();
    QCOMPARE(c->hasApplicationMenu(), true);
    QCOMPARE(c->applicationMenuServiceName(), QString("service.name"));
    QCOMPARE(c->applicationMenuObjectPath(), QString("object/path"));

    QVERIFY (QDBusConnection::sessionBus().unregisterService("org.kde.kappmenu"));
}

void TestXdgShellClient::testNoDecorationModeRequested()
{
    // this test verifies that the decoration follows the default mode if no mode is explicitly requested
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QScopedPointer<ServerSideDecoration> deco(Test::waylandServerSideDecoration()->create(surface.data()));
    QSignalSpy decoSpy(deco.data(), &ServerSideDecoration::modeChanged);
    QVERIFY(decoSpy.isValid());
    if (deco->mode() != ServerSideDecoration::Mode::Server) {
        QVERIFY(decoSpy.wait());
    }
    QCOMPARE(deco->mode(), ServerSideDecoration::Mode::Server);

    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(c->noBorder(), false);
    QCOMPARE(c->isDecorated(), true);
}

void TestXdgShellClient::testSendClientWithTransientToDesktop()
{
    // this test verifies that when sending a client to a desktop all transients are also send to that desktop

    VirtualDesktopManager::self()->setCount(2);
    QScopedPointer<Surface> surface{Test::createSurface()};
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));

    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);

    // let's create a transient window
    QScopedPointer<Surface> transientSurface{Test::createSurface()};
    QScopedPointer<XdgShellSurface> transientShellSurface(Test::createXdgShellStableSurface(transientSurface.data()));
    transientShellSurface->setTransientFor(shellSurface.data());

    auto transient = Test::renderAndWaitForShown(transientSurface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(transient);
    QCOMPARE(workspace()->activeClient(), transient);
    QCOMPARE(transient->transientFor(), c);
    QVERIFY(c->transients().contains(transient));

    QCOMPARE(c->desktop(), 1);
    QVERIFY(!c->isOnAllDesktops());
    QCOMPARE(transient->desktop(), 1);
    QVERIFY(!transient->isOnAllDesktops());
    workspace()->slotWindowToDesktop(2);

    QCOMPARE(c->desktop(), 1);
    QCOMPARE(transient->desktop(), 2);

    // activate c
    workspace()->activateClient(c);
    QCOMPARE(workspace()->activeClient(), c);
    QVERIFY(c->isActive());

    // and send it to the desktop it's already on
    QCOMPARE(c->desktop(), 1);
    QCOMPARE(transient->desktop(), 2);
    workspace()->slotWindowToDesktop(1);

    // which should move the transient back to the desktop
    QCOMPARE(c->desktop(), 1);
    QCOMPARE(transient->desktop(), 1);
}

void TestXdgShellClient::testMinimizeWindowWithTransients()
{
    // this test verifies that when minimizing/unminimizing a window all its
    // transients will be minimized/unminimized as well

    // create the main window
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QVERIFY(!c->isMinimized());

    // create a transient window
    QScopedPointer<Surface> transientSurface(Test::createSurface());
    QScopedPointer<XdgShellSurface> transientShellSurface(Test::createXdgShellStableSurface(transientSurface.data()));
    transientShellSurface->setTransientFor(shellSurface.data());
    auto transient = Test::renderAndWaitForShown(transientSurface.data(), QSize(100, 50), Qt::red);
    QVERIFY(transient);
    QVERIFY(!transient->isMinimized());
    QCOMPARE(transient->transientFor(), c);
    QVERIFY(c->hasTransient(transient, false));

    // minimize the main window, the transient should be minimized as well
    c->minimize();
    QVERIFY(c->isMinimized());
    QVERIFY(transient->isMinimized());

    // unminimize the main window, the transient should be unminimized as well
    c->unminimize();
    QVERIFY(!c->isMinimized());
    QVERIFY(!transient->isMinimized());
}

void TestXdgShellClient::testXdgDecoration_data()
{
    QTest::addColumn<KWayland::Client::XdgDecoration::Mode>("requestedMode");
    QTest::addColumn<KWayland::Client::XdgDecoration::Mode>("expectedMode");

    QTest::newRow("client side requested") << XdgDecoration::Mode::ClientSide << XdgDecoration::Mode::ClientSide;
    QTest::newRow("server side requested") << XdgDecoration::Mode::ServerSide << XdgDecoration::Mode::ServerSide;
}

void TestXdgShellClient::testXdgDecoration()
{
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QScopedPointer<XdgDecoration> deco(Test::xdgDecorationManager()->getToplevelDecoration(shellSurface.data()));

    QSignalSpy decorationConfiguredSpy(deco.data(), &XdgDecoration::modeChanged);
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);

    QFETCH(KWayland::Client::XdgDecoration::Mode, requestedMode);
    QFETCH(KWayland::Client::XdgDecoration::Mode, expectedMode);

    //request a mode
    deco->setMode(requestedMode);

    //kwin will send a configure
    decorationConfiguredSpy.wait();
    configureRequestedSpy.wait();

    QCOMPARE(decorationConfiguredSpy.count(), 1);
    QCOMPARE(decorationConfiguredSpy.first()[0].value<KWayland::Client::XdgDecoration::Mode>(), expectedMode);
    QVERIFY(configureRequestedSpy.count() > 0);

    shellSurface->ackConfigure(configureRequestedSpy.last()[2].toInt());

    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QCOMPARE(c->userCanSetNoBorder(), expectedMode == XdgDecoration::Mode::ServerSide);
    QCOMPARE(c->isDecorated(), expectedMode == XdgDecoration::Mode::ServerSide);
}

void TestXdgShellClient::testXdgNeverCommitted()
{
    //check we don't crash if we create a shell object but delete the XdgShellClient before committing it
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data(), nullptr, Test::CreationSetup::CreateOnly));
}

void TestXdgShellClient::testXdgInitialState()
{
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data(), nullptr, Test::CreationSetup::CreateOnly));
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    surface->commit(Surface::CommitFlag::None);

    configureRequestedSpy.wait();

    QCOMPARE(configureRequestedSpy.count(), 1);

    const auto size = configureRequestedSpy.first()[0].value<QSize>();

    QCOMPARE(size, QSize(0, 0)); //client should chose it's preferred size

    shellSurface->ackConfigure(configureRequestedSpy.first()[2].toUInt());

    auto c = Test::renderAndWaitForShown(surface.data(), QSize(200,100), Qt::blue);
    QCOMPARE(c->size(), QSize(200, 100));
}

void TestXdgShellClient::testXdgInitiallyMaximised()
{
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data(), nullptr, Test::CreationSetup::CreateOnly));
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);

    shellSurface->setMaximized(true);
    surface->commit(Surface::CommitFlag::None);

    configureRequestedSpy.wait();

    QCOMPARE(configureRequestedSpy.count(), 1);

    const auto size = configureRequestedSpy.first()[0].value<QSize>();
    const auto state = configureRequestedSpy.first()[1].value<KWayland::Client::XdgShellSurface::States>();

    QCOMPARE(size, QSize(1280, 1024));
    QVERIFY(state & KWayland::Client::XdgShellSurface::State::Maximized);

    shellSurface->ackConfigure(configureRequestedSpy.first()[2].toUInt());

    auto c = Test::renderAndWaitForShown(surface.data(), size, Qt::blue);
    QCOMPARE(c->maximizeMode(), MaximizeFull);
    QCOMPARE(c->size(), QSize(1280, 1024));
}

void TestXdgShellClient::testXdgInitiallyFullscreen()
{
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data(), nullptr, Test::CreationSetup::CreateOnly));
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);

    shellSurface->setFullscreen(true);
    surface->commit(Surface::CommitFlag::None);

    configureRequestedSpy.wait();

    QCOMPARE(configureRequestedSpy.count(), 1);

    const auto size = configureRequestedSpy.first()[0].value<QSize>();
    const auto state = configureRequestedSpy.first()[1].value<KWayland::Client::XdgShellSurface::States>();

    QCOMPARE(size, QSize(1280, 1024));
    QVERIFY(state & KWayland::Client::XdgShellSurface::State::Fullscreen);

    shellSurface->ackConfigure(configureRequestedSpy.first()[2].toUInt());

    auto c = Test::renderAndWaitForShown(surface.data(), size, Qt::blue);
    QCOMPARE(c->isFullScreen(), true);
    QCOMPARE(c->size(), QSize(1280, 1024));
}

void TestXdgShellClient::testXdgInitiallyMinimized()
{
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data(), nullptr, Test::CreationSetup::CreateOnly));
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);

    shellSurface->requestMinimize();
    surface->commit(Surface::CommitFlag::None);

    configureRequestedSpy.wait();

    QCOMPARE(configureRequestedSpy.count(), 1);

    const auto size = configureRequestedSpy.first()[0].value<QSize>();
    const auto state = configureRequestedSpy.first()[1].value<KWayland::Client::XdgShellSurface::States>();

    QCOMPARE(size, QSize(0, 0));
    QCOMPARE(state, 0);

    shellSurface->ackConfigure(configureRequestedSpy.first()[2].toUInt());

    QEXPECT_FAIL("", "Client created in a minimised state is not exposed to kwin bug 404838", Abort);
    auto c = Test::renderAndWaitForShown(surface.data(), size, Qt::blue, QImage::Format_ARGB32, 10);
    QVERIFY(c);
    QVERIFY(c->isMinimized());
}

void TestXdgShellClient::testXdgWindowGeometryIsntSet()
{
    // This test verifies that the effective window geometry corresponds to the
    // bounding rectangle of the main surface and its sub-surfaces if no window
    // geometry is set by the client.

    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(200, 100), Qt::red);
    QVERIFY(client);
    QCOMPARE(client->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(client->frameGeometry().size(), QSize(200, 100));

    const QPoint oldPosition = client->pos();

    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    Test::render(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->frameGeometry().topLeft(), oldPosition);
    QCOMPARE(client->frameGeometry().size(), QSize(100, 50));
    QCOMPARE(client->bufferGeometry().topLeft(), oldPosition);
    QCOMPARE(client->bufferGeometry().size(), QSize(100, 50));

    QScopedPointer<Surface> childSurface(Test::createSurface());
    QScopedPointer<SubSurface> subSurface(Test::createSubSurface(childSurface.data(), surface.data()));
    QVERIFY(subSurface);
    subSurface->setPosition(QPoint(-20, -10));
    Test::render(childSurface.data(), QSize(100, 50), Qt::blue);
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->frameGeometry().topLeft(), oldPosition);
    QCOMPARE(client->frameGeometry().size(), QSize(120, 60));
    QCOMPARE(client->bufferGeometry().topLeft(), oldPosition + QPoint(20, 10));
    QCOMPARE(client->bufferGeometry().size(), QSize(100, 50));
}

void TestXdgShellClient::testXdgWindowGeometryAttachBuffer()
{
    // This test verifies that the effective window geometry remains the same when
    // a new buffer is attached and xdg_surface.set_window_geometry is not called
    // again. Notice that the window geometry must remain the same even if the new
    // buffer is smaller.

    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(200, 100), Qt::red);
    QVERIFY(client);
    QCOMPARE(client->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(client->frameGeometry().size(), QSize(200, 100));

    const QPoint oldPosition = client->pos();

    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    shellSurface->setWindowGeometry(QRect(10, 10, 180, 80));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(frameGeometryChangedSpy.count(), 1);
    QCOMPARE(client->frameGeometry().topLeft(), oldPosition);
    QCOMPARE(client->frameGeometry().size(), QSize(180, 80));
    QCOMPARE(client->bufferGeometry().topLeft(), oldPosition - QPoint(10, 10));
    QCOMPARE(client->bufferGeometry().size(), QSize(200, 100));

    Test::render(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(frameGeometryChangedSpy.count(), 2);
    QCOMPARE(client->frameGeometry().topLeft(), oldPosition);
    QCOMPARE(client->frameGeometry().size(), QSize(90, 40));
    QCOMPARE(client->bufferGeometry().topLeft(), oldPosition - QPoint(10, 10));
    QCOMPARE(client->bufferGeometry().size(), QSize(100, 50));

    shellSurface->setWindowGeometry(QRect(0, 0, 100, 50));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(frameGeometryChangedSpy.count(), 3);
    QCOMPARE(client->frameGeometry().topLeft(), oldPosition);
    QCOMPARE(client->frameGeometry().size(), QSize(100, 50));
    QCOMPARE(client->bufferGeometry().topLeft(), oldPosition);
    QCOMPARE(client->bufferGeometry().size(), QSize(100, 50));

    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClient::testXdgWindowGeometryAttachSubSurface()
{
    // This test verifies that the effective window geometry remains the same
    // when a new sub-surface is added and xdg_surface.set_window_geometry is
    // not called again.

    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(200, 100), Qt::red);
    QVERIFY(client);
    QCOMPARE(client->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(client->frameGeometry().size(), QSize(200, 100));

    const QPoint oldPosition = client->pos();

    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    shellSurface->setWindowGeometry(QRect(10, 10, 180, 80));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->frameGeometry().topLeft(), oldPosition);
    QCOMPARE(client->frameGeometry().size(), QSize(180, 80));
    QCOMPARE(client->bufferGeometry().topLeft(), oldPosition - QPoint(10, 10));
    QCOMPARE(client->bufferGeometry().size(), QSize(200, 100));

    QScopedPointer<Surface> childSurface(Test::createSurface());
    QScopedPointer<SubSurface> subSurface(Test::createSubSurface(childSurface.data(), surface.data()));
    QVERIFY(subSurface);
    subSurface->setPosition(QPoint(-20, -20));
    Test::render(childSurface.data(), QSize(100, 50), Qt::blue);
    surface->commit(Surface::CommitFlag::None);
    QCOMPARE(client->frameGeometry().topLeft(), oldPosition);
    QCOMPARE(client->frameGeometry().size(), QSize(180, 80));
    QCOMPARE(client->bufferGeometry().topLeft(), oldPosition - QPoint(10, 10));
    QCOMPARE(client->bufferGeometry().size(), QSize(200, 100));

    shellSurface->setWindowGeometry(QRect(-15, -15, 50, 40));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->frameGeometry().topLeft(), oldPosition);
    QCOMPARE(client->frameGeometry().size(), QSize(50, 40));
    QCOMPARE(client->bufferGeometry().topLeft(), oldPosition - QPoint(-15, -15));
    QCOMPARE(client->bufferGeometry().size(), QSize(200, 100));
}

void TestXdgShellClient::testXdgWindowGeometryInteractiveResize()
{
    // This test verifies that correct window geometry is provided along each
    // configure event when an xdg-shell is being interactively resized.

    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(200, 100), Qt::red);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(client->frameGeometry().size(), QSize(200, 100));

    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);

    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    shellSurface->setWindowGeometry(QRect(10, 10, 180, 80));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(client->frameGeometry().size(), QSize(180, 80));

    QSignalSpy clientStartMoveResizedSpy(client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QSignalSpy clientStepUserMovedResizedSpy(client, &AbstractClient::clientStepUserMovedResized);
    QVERIFY(clientStepUserMovedResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(client, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    // Start interactively resizing the client.
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeClient(), client);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 2);
    XdgShellSurface::States states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Resizing));

    // Go right.
    QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    client->keyPressEvent(Qt::Key_Right);
    client->updateMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 3);
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Resizing));
    QCOMPARE(configureRequestedSpy.last().at(0).toSize(), QSize(188, 80));
    shellSurface->setWindowGeometry(QRect(10, 10, 188, 80));
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), QSize(208, 100), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QCOMPARE(client->bufferGeometry().size(), QSize(208, 100));
    QCOMPARE(client->frameGeometry().size(), QSize(188, 80));

    // Go down.
    cursorPos = KWin::Cursors::self()->mouse()->pos();
    client->keyPressEvent(Qt::Key_Down);
    client->updateMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(0, 8));
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 4);
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Resizing));
    QCOMPARE(configureRequestedSpy.last().at(0).toSize(), QSize(188, 88));
    shellSurface->setWindowGeometry(QRect(10, 10, 188, 88));
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), QSize(208, 108), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 2);
    QCOMPARE(client->bufferGeometry().size(), QSize(208, 108));
    QCOMPARE(client->frameGeometry().size(), QSize(188, 88));

    // Finish resizing the client.
    client->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 5);
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Resizing));

    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClient::testXdgWindowGeometryFullScreen()
{
    // This test verifies that an xdg-shell receives correct window geometry when
    // its fullscreen state gets changed.

    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(200, 100), Qt::red);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(client->frameGeometry().size(), QSize(200, 100));

    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);

    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    shellSurface->setWindowGeometry(QRect(10, 10, 180, 80));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(client->frameGeometry().size(), QSize(180, 80));

    workspace()->slotWindowFullScreen();
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 2);
    QCOMPARE(configureRequestedSpy.last().at(0).toSize(), QSize(1280, 1024));
    XdgShellSurface::States states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Fullscreen));
    shellSurface->setWindowGeometry(QRect(0, 0, 1280, 1024));
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->bufferGeometry().size(), QSize(1280, 1024));
    QCOMPARE(client->frameGeometry().size(), QSize(1280, 1024));

    workspace()->slotWindowFullScreen();
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 3);
    QCOMPARE(configureRequestedSpy.last().at(0).toSize(), QSize(180, 80));
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Fullscreen));
    shellSurface->setWindowGeometry(QRect(10, 10, 180, 80));
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), QSize(200, 100), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(client->frameGeometry().size(), QSize(180, 80));

    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClient::testXdgWindowGeometryMaximize()
{
    // This test verifies that an xdg-shell receives correct window geometry when
    // its maximized state gets changed.

    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(200, 100), Qt::red);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(client->frameGeometry().size(), QSize(200, 100));

    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);

    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    shellSurface->setWindowGeometry(QRect(10, 10, 180, 80));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(client->frameGeometry().size(), QSize(180, 80));

    workspace()->slotWindowMaximize();
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 2);
    QCOMPARE(configureRequestedSpy.last().at(0).toSize(), QSize(1280, 1024));
    XdgShellSurface::States states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));
    shellSurface->setWindowGeometry(QRect(0, 0, 1280, 1024));
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->bufferGeometry().size(), QSize(1280, 1024));
    QCOMPARE(client->frameGeometry().size(), QSize(1280, 1024));

    workspace()->slotWindowMaximize();
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 3);
    QCOMPARE(configureRequestedSpy.last().at(0).toSize(), QSize(180, 80));
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Maximized));
    shellSurface->setWindowGeometry(QRect(10, 10, 180, 80));
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), QSize(200, 100), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(client->frameGeometry().size(), QSize(180, 80));

    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClient::testPointerInputTransform()
{
    // This test verifies that XdgToplevelClient provides correct input transform matrix.
    // The input transform matrix is used by seat to map pointer events from the global
    // screen coordinates to the surface-local coordinates.

    // Get a wl_pointer object on the client side.
    QScopedPointer<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy pointerEnteredSpy(pointer.data(), &KWayland::Client::Pointer::entered);
    QVERIFY(pointerEnteredSpy.isValid());
    QSignalSpy pointerMotionSpy(pointer.data(), &KWayland::Client::Pointer::motion);
    QVERIFY(pointerMotionSpy.isValid());

    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(200, 100), Qt::red);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(client->frameGeometry().size(), QSize(200, 100));

    // Enter the surface.
    quint32 timestamp = 0;
    kwinApp()->platform()->pointerMotion(client->pos(), timestamp++);
    QVERIFY(pointerEnteredSpy.wait());

    // Move the pointer to (10, 5) relative to the upper left frame corner, which is located
    // at (0, 0) in the surface-local coordinates.
    kwinApp()->platform()->pointerMotion(client->pos() + QPoint(10, 5), timestamp++);
    QVERIFY(pointerMotionSpy.wait());
    QCOMPARE(pointerMotionSpy.last().first(), QPoint(10, 5));

    // Let's pretend that the client has changed the extents of the client-side drop-shadow
    // but the frame geometry didn't change.
    QSignalSpy bufferGeometryChangedSpy(client, &AbstractClient::bufferGeometryChanged);
    QVERIFY(bufferGeometryChangedSpy.isValid());
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    shellSurface->setWindowGeometry(QRect(10, 20, 200, 100));
    Test::render(surface.data(), QSize(220, 140), Qt::blue);
    QVERIFY(bufferGeometryChangedSpy.wait());
    QCOMPARE(frameGeometryChangedSpy.count(), 0);
    QCOMPARE(client->frameGeometry().size(), QSize(200, 100));
    QCOMPARE(client->bufferGeometry().size(), QSize(220, 140));

    // Move the pointer to (20, 50) relative to the upper left frame corner, which is located
    // at (10, 20) in the surface-local coordinates.
    kwinApp()->platform()->pointerMotion(client->pos() + QPoint(20, 50), timestamp++);
    QVERIFY(pointerMotionSpy.wait());
    QCOMPARE(pointerMotionSpy.last().first(), QPoint(10, 20) + QPoint(20, 50));

    // Destroy the xdg-toplevel surface.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClient::testReentrantSetFrameGeometry()
{
    // This test verifies that calling setFrameGeometry() from a slot connected directly
    // to the frameGeometryChanged() signal won't cause an infinite recursion.

    // Create an xdg-toplevel surface and wait for the compositor to catch up.
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(200, 100), Qt::red);
    QVERIFY(client);
    QCOMPARE(client->pos(), QPoint(0, 0));

    // Let's pretend that there is a script that really wants the client to be at (100, 100).
    connect(client, &AbstractClient::frameGeometryChanged, this, [client]() {
        client->setFrameGeometry(QRect(QPoint(100, 100), client->size()));
    });

    // Trigger the lambda above.
    client->move(QPoint(40, 50));

    // Eventually, the client will end up at (100, 100).
    QCOMPARE(client->pos(), QPoint(100, 100));

    // Destroy the xdg-toplevel surface.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClient::testDoubleMaximize()
{
    // This test verifies that the case where a client issues two set_maximized() requests
    // separated by the initial commit is handled properly.

    // Create the test surface.
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data(), nullptr, Test::CreationSetup::CreateOnly));
    shellSurface->setMaximized(true);
    surface->commit(Surface::CommitFlag::None);

    // Wait for the compositor to respond with a configure event.
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);
    QSize size = configureRequestedSpy.last().at(0).value<QSize>();
    QCOMPARE(size, QSize(1280, 1024));
    XdgShellSurface::States states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));

    // Send another set_maximized() request, but do not attach any buffer yet.
    shellSurface->setMaximized(true);
    surface->commit(Surface::CommitFlag::None);

    // The compositor must respond with another configure event even if the state hasn't changed.
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 2);
    size = configureRequestedSpy.last().at(0).value<QSize>();
    QCOMPARE(size, QSize(1280, 1024));
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));
}

WAYLANDTEST_MAIN(TestXdgShellClient)
#include "xdgshellclient_test.moc"
