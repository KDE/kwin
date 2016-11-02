/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#include "kwin_wayland_test.h"
#include "cursor.h"
#include "effects.h"
#include "platform.h"
#include "shell_client.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/xdgshell.h>

#include <KWayland/Server/shell_interface.h>

using namespace KWin;
using namespace KWayland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_shell_client-0");

class TestShellClient : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testMapUnmapMap_data();
    void testMapUnmapMap();
    void testDesktopPresenceChanged();
    void testTransientPositionAfterRemap();
    void testMinimizeActiveWindow_data();
    void testMinimizeActiveWindow();
    void testFullscreen_data();
    void testFullscreen();
    void testMaximizedToFullscreen_data();
    void testMaximizedToFullscreen();
    void testWindowOpensLargerThanScreen_data();
    void testWindowOpensLargerThanScreen();
    void testHidden_data();
    void testHidden();
    void testDesktopFileName();
    void testCaptionSimplified();
};

void TestShellClient::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setOutputCount", Qt::DirectConnection, Q_ARG(int, 2));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void TestShellClient::init()
{
    QVERIFY(Test::setupWaylandConnection(s_socketName, Test::AdditionalWaylandInterface::Decoration));

    screens()->setCurrent(0);
    KWin::Cursor::setPos(QPoint(1280, 512));
}

void TestShellClient::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestShellClient::testMapUnmapMap_data()
{
    QTest::addColumn<Test::ShellSurfaceType>("type");

    QTest::newRow("wlShell") << Test::ShellSurfaceType::WlShell;
    QTest::newRow("xdgShellV5") << Test::ShellSurfaceType::XdgShellV5;
}

void TestShellClient::testMapUnmapMap()
{
    // this test verifies that mapping a previously mapped window works correctly
    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    QSignalSpy effectsWindowShownSpy(effects, &EffectsHandler::windowShown);
    QVERIFY(effectsWindowShownSpy.isValid());
    QSignalSpy effectsWindowHiddenSpy(effects, &EffectsHandler::windowHidden);
    QVERIFY(effectsWindowHiddenSpy.isValid());

    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));

    // now let's render
    Test::render(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(clientAddedSpy.isEmpty());
    QVERIFY(clientAddedSpy.wait());
    auto client = clientAddedSpy.first().first().value<ShellClient*>();
    QVERIFY(client);
    QVERIFY(client->isShown(true));
    QCOMPARE(client->isHiddenInternal(), false);
    QCOMPARE(client->readyForPainting(), true);
    QCOMPARE(client->depth(), 32);
    QVERIFY(client->hasAlpha());
    QCOMPARE(workspace()->activeClient(), client);
    QVERIFY(effectsWindowShownSpy.isEmpty());
    QVERIFY(client->isMaximizable());
    QVERIFY(client->isMovable());
    QVERIFY(client->isMovableAcrossScreens());
    QVERIFY(client->isResizable());
    QVERIFY(client->property("maximizable").toBool());
    QVERIFY(client->property("moveable").toBool());
    QVERIFY(client->property("moveableAcrossScreens").toBool());
    QVERIFY(client->property("resizeable").toBool());

    // now unmap
    QSignalSpy hiddenSpy(client, &ShellClient::windowHidden);
    QVERIFY(hiddenSpy.isValid());
    QSignalSpy windowClosedSpy(client, &ShellClient::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    surface->attachBuffer(Buffer::Ptr());
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(hiddenSpy.wait());
    QCOMPARE(client->readyForPainting(), true);
    QCOMPARE(client->isHiddenInternal(), true);
    QVERIFY(windowClosedSpy.isEmpty());
    QVERIFY(!workspace()->activeClient());
    QCOMPARE(effectsWindowHiddenSpy.count(), 1);
    QCOMPARE(effectsWindowHiddenSpy.first().first().value<EffectWindow*>(), client->effectWindow());

    QSignalSpy windowShownSpy(client, &ShellClient::windowShown);
    QVERIFY(windowShownSpy.isValid());
    Test::render(surface.data(), QSize(100, 50), Qt::blue, QImage::Format_RGB32);
    QCOMPARE(clientAddedSpy.count(), 1);
    QVERIFY(windowShownSpy.wait());
    QCOMPARE(windowShownSpy.count(), 1);
    QCOMPARE(clientAddedSpy.count(), 1);
    QCOMPARE(client->readyForPainting(), true);
    QCOMPARE(client->isHiddenInternal(), false);
    QCOMPARE(client->depth(), 24);
    QVERIFY(!client->hasAlpha());
    QCOMPARE(workspace()->activeClient(), client);
    QCOMPARE(effectsWindowShownSpy.count(), 1);
    QCOMPARE(effectsWindowShownSpy.first().first().value<EffectWindow*>(), client->effectWindow());

    // let's unmap again
    surface->attachBuffer(Buffer::Ptr());
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(hiddenSpy.wait());
    QCOMPARE(hiddenSpy.count(), 2);
    QCOMPARE(client->readyForPainting(), true);
    QCOMPARE(client->isHiddenInternal(), true);
    QVERIFY(windowClosedSpy.isEmpty());
    QCOMPARE(effectsWindowHiddenSpy.count(), 2);
    QCOMPARE(effectsWindowHiddenSpy.last().first().value<EffectWindow*>(), client->effectWindow());

    shellSurface.reset();
    surface.reset();
    QVERIFY(windowClosedSpy.wait());
    QCOMPARE(windowClosedSpy.count(), 1);
    QCOMPARE(effectsWindowHiddenSpy.count(), 2);
}

void TestShellClient::testDesktopPresenceChanged()
{
    // this test verifies that the desktop presence changed signals are properly emitted
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(c->desktop(), 1);
    effects->setNumberOfDesktops(4);
    QSignalSpy desktopPresenceChangedClientSpy(c, &ShellClient::desktopPresenceChanged);
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
    // effects is delayed by one cycle
    QCOMPARE(desktopPresenceChangedEffectsSpy.count(), 0);
    QVERIFY(desktopPresenceChangedEffectsSpy.wait());
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

void TestShellClient::testTransientPositionAfterRemap()
{
    // this test simulates the situation that a transient window gets reused and the parent window
    // moved between the two usages
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);

    // create the Transient window
    QScopedPointer<Surface> transientSurface(Test::createSurface());
    QScopedPointer<ShellSurface> transientShellSurface(Test::createShellSurface(transientSurface.data()));
    transientShellSurface->setTransient(surface.data(), QPoint(5, 10));
    auto transient = Test::renderAndWaitForShown(transientSurface.data(), QSize(50, 40), Qt::blue);
    QVERIFY(transient);
    QCOMPARE(transient->geometry(), QRect(c->geometry().topLeft() + QPoint(5, 10), QSize(50, 40)));

    // unmap the transient
    QSignalSpy windowHiddenSpy(transient, &ShellClient::windowHidden);
    QVERIFY(windowHiddenSpy.isValid());
    transientSurface->attachBuffer(Buffer::Ptr());
    transientSurface->commit(Surface::CommitFlag::None);
    QVERIFY(windowHiddenSpy.wait());

    // now move the parent surface
    c->setGeometry(c->geometry().translated(5, 10));

    // now map the transient again
    QSignalSpy windowShownSpy(transient, &ShellClient::windowShown);
    QVERIFY(windowShownSpy.isValid());
    Test::render(transientSurface.data(), QSize(50, 40), Qt::blue);
    QVERIFY(windowShownSpy.wait());
    QCOMPARE(transient->geometry(), QRect(c->geometry().topLeft() + QPoint(5, 10), QSize(50, 40)));
}

void TestShellClient::testMinimizeActiveWindow_data()
{
    QTest::addColumn<Test::ShellSurfaceType>("type");

    QTest::newRow("wlShell") << Test::ShellSurfaceType::WlShell;
    QTest::newRow("xdgShellV5") << Test::ShellSurfaceType::XdgShellV5;
}

void TestShellClient::testMinimizeActiveWindow()
{
    // this test verifies that when minimizing the active window it gets deactivated
    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));
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

void TestShellClient::testFullscreen_data()
{
    QTest::addColumn<Test::ShellSurfaceType>("type");
    QTest::addColumn<ServerSideDecoration::Mode>("decoMode");

    QTest::newRow("wlShell") << Test::ShellSurfaceType::WlShell << ServerSideDecoration::Mode::Client;
    QTest::newRow("xdgShellV5") << Test::ShellSurfaceType::XdgShellV5 << ServerSideDecoration::Mode::Client;

    QTest::newRow("wlShell - deco") << Test::ShellSurfaceType::WlShell << ServerSideDecoration::Mode::Server;
    QTest::newRow("xdgShellV5 - deco") << Test::ShellSurfaceType::XdgShellV5 << ServerSideDecoration::Mode::Server;
}

void TestShellClient::testFullscreen()
{
    // this test verifies that a window can be properly fullscreened
    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));

    // create deco
    QScopedPointer<ServerSideDecoration> deco(Test::waylandServerSideDecoration()->create(surface.data()));
    QSignalSpy decoSpy(deco.data(), &ServerSideDecoration::modeChanged);
    QVERIFY(decoSpy.isValid());
    QVERIFY(decoSpy.wait());
    QFETCH(ServerSideDecoration::Mode, decoMode);
    deco->requestMode(decoMode);
    QVERIFY(decoSpy.wait());
    QCOMPARE(deco->mode(), decoMode);

    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QVERIFY(c->isActive());
    QCOMPARE(c->layer(), NormalLayer);
    QVERIFY(!c->isFullScreen());
    QCOMPARE(c->clientSize(), QSize(100, 50));
    QCOMPARE(c->isDecorated(), decoMode == ServerSideDecoration::Mode::Server);
    QSignalSpy fullscreenChangedSpy(c, &ShellClient::fullScreenChanged);
    QVERIFY(fullscreenChangedSpy.isValid());
    QSignalSpy geometryChangedSpy(c, &ShellClient::geometryChanged);
    QVERIFY(geometryChangedSpy.isValid());
    QSignalSpy sizeChangeRequestedSpy(shellSurface.data(), SIGNAL(sizeChanged(QSize)));
    QVERIFY(sizeChangeRequestedSpy.isValid());

    // fullscreen the window
    switch (type) {
    case Test::ShellSurfaceType::WlShell:
        qobject_cast<ShellSurface*>(shellSurface.data())->setFullscreen();
        break;
    case Test::ShellSurfaceType::XdgShellV5:
        qobject_cast<XdgShellSurface*>(shellSurface.data())->setFullscreen(true);
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    QVERIFY(fullscreenChangedSpy.wait());
    QVERIFY(sizeChangeRequestedSpy.wait());
    QCOMPARE(sizeChangeRequestedSpy.count(), 1);
    QCOMPARE(sizeChangeRequestedSpy.first().first().toSize(), QSize(screens()->size(0)));
    // TODO: should switch to fullscreen once it's updated
    QVERIFY(c->isFullScreen());
    QCOMPARE(c->clientSize(), QSize(100, 50));
    QVERIFY(geometryChangedSpy.isEmpty());

    // render at the new size
    Test::render(surface.data(), sizeChangeRequestedSpy.first().first().toSize(), Qt::red);
    QVERIFY(geometryChangedSpy.wait());
    QCOMPARE(geometryChangedSpy.count(), 1);
    QVERIFY(c->isFullScreen());
    QVERIFY(!c->isDecorated());
    QCOMPARE(c->geometry(), QRect(QPoint(0, 0), sizeChangeRequestedSpy.first().first().toSize()));
    QCOMPARE(c->layer(), ActiveLayer);

    // swap back to normal
    switch (type) {
    case Test::ShellSurfaceType::WlShell:
        qobject_cast<ShellSurface*>(shellSurface.data())->setToplevel();
        break;
    case Test::ShellSurfaceType::XdgShellV5:
        qobject_cast<XdgShellSurface*>(shellSurface.data())->setFullscreen(false);
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    QVERIFY(fullscreenChangedSpy.wait());
    QVERIFY(sizeChangeRequestedSpy.wait());
    QCOMPARE(sizeChangeRequestedSpy.count(), 2);
    QCOMPARE(sizeChangeRequestedSpy.last().first().toSize(), QSize(100, 50));
    // TODO: should switch to fullscreen once it's updated
    QVERIFY(!c->isFullScreen());
    QCOMPARE(c->layer(), NormalLayer);
    QCOMPARE(c->isDecorated(), decoMode == ServerSideDecoration::Mode::Server);
}


void TestShellClient::testMaximizedToFullscreen_data()
{
    QTest::addColumn<Test::ShellSurfaceType>("type");
    QTest::addColumn<ServerSideDecoration::Mode>("decoMode");

    QTest::newRow("wlShell") << Test::ShellSurfaceType::WlShell << ServerSideDecoration::Mode::Client;
    QTest::newRow("xdgShellV5") << Test::ShellSurfaceType::XdgShellV5 << ServerSideDecoration::Mode::Client;

    QTest::newRow("wlShell - deco") << Test::ShellSurfaceType::WlShell << ServerSideDecoration::Mode::Server;
    QTest::newRow("xdgShellV5 - deco") << Test::ShellSurfaceType::XdgShellV5 << ServerSideDecoration::Mode::Server;
}

void TestShellClient::testMaximizedToFullscreen()
{
    // this test verifies that a window can be properly fullscreened after maximizing
    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));

    // create deco
    QScopedPointer<ServerSideDecoration> deco(Test::waylandServerSideDecoration()->create(surface.data()));
    QSignalSpy decoSpy(deco.data(), &ServerSideDecoration::modeChanged);
    QVERIFY(decoSpy.isValid());
    QVERIFY(decoSpy.wait());
    QFETCH(ServerSideDecoration::Mode, decoMode);
    deco->requestMode(decoMode);
    QVERIFY(decoSpy.wait());
    QCOMPARE(deco->mode(), decoMode);

    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QVERIFY(c->isActive());
    QVERIFY(!c->isFullScreen());
    QCOMPARE(c->clientSize(), QSize(100, 50));
    QCOMPARE(c->isDecorated(), decoMode == ServerSideDecoration::Mode::Server);
    QSignalSpy fullscreenChangedSpy(c, &ShellClient::fullScreenChanged);
    QVERIFY(fullscreenChangedSpy.isValid());
    QSignalSpy geometryChangedSpy(c, &ShellClient::geometryChanged);
    QVERIFY(geometryChangedSpy.isValid());
    QSignalSpy sizeChangeRequestedSpy(shellSurface.data(), SIGNAL(sizeChanged(QSize)));
    QVERIFY(sizeChangeRequestedSpy.isValid());

    // change to maximize
    switch (type) {
    case Test::ShellSurfaceType::WlShell:
        qobject_cast<ShellSurface*>(shellSurface.data())->setMaximized();
        break;
    case Test::ShellSurfaceType::XdgShellV5:
        qobject_cast<XdgShellSurface*>(shellSurface.data())->setMaximized(true);
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    QVERIFY(sizeChangeRequestedSpy.wait());
    QCOMPARE(sizeChangeRequestedSpy.count(), 1);
    QCOMPARE(c->maximizeMode(), MaximizeFull);
    QCOMPARE(geometryChangedSpy.isEmpty(), false);
    geometryChangedSpy.clear();

    // fullscreen the window
    switch (type) {
    case Test::ShellSurfaceType::WlShell:
        qobject_cast<ShellSurface*>(shellSurface.data())->setFullscreen();
        break;
    case Test::ShellSurfaceType::XdgShellV5:
        qobject_cast<XdgShellSurface*>(shellSurface.data())->setFullscreen(true);
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    QVERIFY(fullscreenChangedSpy.wait());
    if (decoMode == ServerSideDecoration::Mode::Server) {
        QVERIFY(sizeChangeRequestedSpy.wait());
        QCOMPARE(sizeChangeRequestedSpy.count(), 2);
    }
    QCOMPARE(sizeChangeRequestedSpy.last().first().toSize(), QSize(screens()->size(0)));
    // TODO: should switch to fullscreen once it's updated
    QVERIFY(c->isFullScreen());
    QCOMPARE(c->clientSize(), QSize(100, 50));
    QVERIFY(geometryChangedSpy.isEmpty());

    // render at the new size
    Test::render(surface.data(), sizeChangeRequestedSpy.last().first().toSize(), Qt::red);
    QVERIFY(geometryChangedSpy.wait());
    QCOMPARE(geometryChangedSpy.count(), 1);
    QVERIFY(c->isFullScreen());
    QVERIFY(!c->isDecorated());
    QCOMPARE(c->geometry(), QRect(QPoint(0, 0), sizeChangeRequestedSpy.last().first().toSize()));
    sizeChangeRequestedSpy.clear();

    // swap back to normal
    switch (type) {
    case Test::ShellSurfaceType::WlShell:
        qobject_cast<ShellSurface*>(shellSurface.data())->setToplevel();
        break;
    case Test::ShellSurfaceType::XdgShellV5:
        qobject_cast<XdgShellSurface*>(shellSurface.data())->setFullscreen(false);
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    QVERIFY(fullscreenChangedSpy.wait());
    QVERIFY(sizeChangeRequestedSpy.wait());
    QCOMPARE(sizeChangeRequestedSpy.count(), 1);
    QEXPECT_FAIL("wlShell - deco", "With decoration incorrect geometry requested", Continue);
    QEXPECT_FAIL("xdgShellV5 - deco", "With decoration incorrect geometry requested", Continue);
    QCOMPARE(sizeChangeRequestedSpy.last().first().toSize(), QSize(100, 50));
    // TODO: should switch to fullscreen once it's updated
    QVERIFY(!c->isFullScreen());
    QCOMPARE(c->isDecorated(), decoMode == ServerSideDecoration::Mode::Server);
}

void TestShellClient::testWindowOpensLargerThanScreen_data()
{
    QTest::addColumn<Test::ShellSurfaceType>("type");

    QTest::newRow("wlShell") << Test::ShellSurfaceType::WlShell;
    QTest::newRow("xdgShellV5") << Test::ShellSurfaceType::XdgShellV5;
}

void TestShellClient::testWindowOpensLargerThanScreen()
{
    // this test creates a window which is as large as the screen, but is decorated
    // the window should get resized to fit into the screen, BUG: 366632
    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));
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
    QCOMPARE(c->clientSize(), screens()->size(0));
    QVERIFY(c->isDecorated());
    QEXPECT_FAIL("", "BUG 366632", Continue);
    QVERIFY(sizeChangeRequestedSpy.wait());
}

void TestShellClient::testHidden_data()
{
    QTest::addColumn<Test::ShellSurfaceType>("type");

    QTest::newRow("wlShell") << Test::ShellSurfaceType::WlShell;
    QTest::newRow("xdgShellV5") << Test::ShellSurfaceType::XdgShellV5;
}

void TestShellClient::testHidden()
{
    // this test verifies that when hiding window it doesn't get shown
    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));
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

void TestShellClient::testDesktopFileName()
{
    QIcon::setThemeName(QStringLiteral("breeze"));
    // this test verifies that desktop file name is passed correctly to the window
    QScopedPointer<Surface> surface(Test::createSurface());
    // only xdg-shell as ShellSurface misses the setter
    QScopedPointer<XdgShellSurface> shellSurface(qobject_cast<XdgShellSurface*>(Test::createShellSurface(Test::ShellSurfaceType::XdgShellV5, surface.data())));
    shellSurface->setAppId(QByteArrayLiteral("org.kde.foo"));
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(c->desktopFileName(), QByteArrayLiteral("org.kde.foo"));
    // the desktop file does not exist, so icon should be generic Wayland
    QCOMPARE(c->icon().name(), QStringLiteral("wayland"));

    QSignalSpy desktopFileNameChangedSpy(c, &AbstractClient::desktopFileNameChanged);
    QVERIFY(desktopFileNameChangedSpy.isValid());
    QSignalSpy iconChangedSpy(c, &ShellClient::iconChanged);
    QVERIFY(iconChangedSpy.isValid());
    shellSurface->setAppId(QByteArrayLiteral("org.kde.bar"));
    QVERIFY(desktopFileNameChangedSpy.wait());
    QCOMPARE(c->desktopFileName(), QByteArrayLiteral("org.kde.bar"));
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

void TestShellClient::testCaptionSimplified()
{
    // this test verifies that caption is properly trimmed
    // see BUG 323798 comment #12
    QScopedPointer<Surface> surface(Test::createSurface());
    // only done for xdg-shell as ShellSurface misses the setter
    QScopedPointer<XdgShellSurface> shellSurface(qobject_cast<XdgShellSurface*>(Test::createShellSurface(Test::ShellSurfaceType::XdgShellV5, surface.data())));
    const QString origTitle = QString::fromUtf8(QByteArrayLiteral("Was tun, wenn Schüler Autismus haben?\342\200\250\342\200\250\342\200\250 – Marlies Hübner - Mozilla Firefox"));
    shellSurface->setTitle(origTitle);
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QEXPECT_FAIL("", "BUG 323798", Continue);
    QVERIFY(c->caption() != origTitle);
    QEXPECT_FAIL("", "BUG 323798", Continue);
    QCOMPARE(c->caption(), origTitle.simplified());
}

WAYLANDTEST_MAIN(TestShellClient)
#include "shell_client_test.moc"
