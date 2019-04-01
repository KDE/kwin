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
#include "platform.h"
#include "shell_client.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/xdgdecoration.h>

#include <KWayland/Server/shell_interface.h>
#include <KWayland/Server/xdgdecoration_interface.h>

#include <KDecoration2/Decoration>
#include <KDecoration2/DecoratedClient>

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
    void testBorderlessMaximizedWindow();
    void testBorderlessMaximizedWindowNoClientSideDecoration();
};

void TestMaximized::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void TestMaximized::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Decoration |
                                         Test::AdditionalWaylandInterface::XdgDecoration));

    screens()->setCurrent(0);
    KWin::Cursor::setPos(QPoint(1280, 512));
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
    // this test verifies that when a ShellClient gets maximized the Decoration receives the signal
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
    QScopedPointer<ServerSideDecoration> ssd(Test::waylandServerSideDecoration()->create(surface.data()));

    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QSignalSpy sizeChangedSpy(shellSurface.data(), &ShellSurface::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());

    QVERIFY(client);
    QVERIFY(client->isDecorated());
    auto decoration = client->decoration();
    QVERIFY(decoration);
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);

    // now maximize
    QVERIFY(sizeChangedSpy.isEmpty());
    QSignalSpy bordersChangedSpy(decoration, &KDecoration2::Decoration::bordersChanged);
    QVERIFY(bordersChangedSpy.isValid());
    QSignalSpy maximizedChangedSpy(decoration->client().data(), &KDecoration2::DecoratedClient::maximizedChanged);
    QVERIFY(maximizedChangedSpy.isValid());
    QSignalSpy geometryShapeChangedSpy(client, &AbstractClient::geometryShapeChanged);
    QVERIFY(geometryShapeChangedSpy.isValid());

    workspace()->slotWindowMaximize();
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.first().first().toSize(), QSize(1280, 1024 - decoration->borderTop()));
    Test::render(surface.data(), sizeChangedSpy.first().first().toSize(), Qt::red);
    QVERIFY(geometryShapeChangedSpy.wait());
    QCOMPARE(geometryShapeChangedSpy.count(), 2);
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(maximizedChangedSpy.count(), 1);
    QCOMPARE(maximizedChangedSpy.last().first().toBool(), true);
    QCOMPARE(bordersChangedSpy.count(), 1);
    QCOMPARE(decoration->borderLeft(), 0);
    QCOMPARE(decoration->borderBottom(), 0);
    QCOMPARE(decoration->borderRight(), 0);
    QVERIFY(decoration->borderTop() != 0);

    // now unmaximize again
    workspace()->slotWindowMaximize();

    Test::render(surface.data(), QSize(100, 50), Qt::red);
    QVERIFY(geometryShapeChangedSpy.wait());
    QCOMPARE(geometryShapeChangedSpy.count(), 4);
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(maximizedChangedSpy.count(), 2);
    QCOMPARE(maximizedChangedSpy.last().first().toBool(), false);
    QCOMPARE(bordersChangedSpy.count(), 2);
    QVERIFY(decoration->borderTop() != 0);
    QVERIFY(decoration->borderLeft() != 0);
    QVERIFY(decoration->borderRight() != 0);
    QVERIFY(decoration->borderBottom() != 0);

    QCOMPARE(sizeChangedSpy.count(), 2);
    QCOMPARE(sizeChangedSpy.last().first().toSize(), QSize(100, 50));
}

void TestMaximized::testInitiallyMaximized()
{
    // this test verifies that a window created as maximized, will be maximized
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));

    QSignalSpy sizeChangedSpy(shellSurface.data(), &ShellSurface::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());

    shellSurface->setMaximized();
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(shellSurface->size(), QSize(1280, 1024));

    // now let's render in an incorrect size
    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QCOMPARE(client->geometry(), QRect(0, 0, 100, 50));
    QEXPECT_FAIL("", "Should go out of maximzied", Continue);
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QVERIFY(client->shellSurface()->isMaximized());
}

void TestMaximized::testBorderlessMaximizedWindow()
{
    // test case verifies that borderless maximized window works
    // see BUG 370982

    // adjust config
    auto group = kwinApp()->config()->group("Windows");
    group.writeEntry("BorderlessMaximizedWindows", true);
    group.sync();
    Workspace::self()->slotReconfigure();
    QCOMPARE(options->borderlessMaximizedWindows(), true);
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
    QScopedPointer<ServerSideDecoration> ssd(Test::waylandServerSideDecoration()->create(surface.data()));

    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client->isDecorated());
    const QRect origGeo = client->geometry();

    QSignalSpy sizeChangedSpy(shellSurface.data(), &ShellSurface::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());

    // go to maximized
    shellSurface->setMaximized();
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(shellSurface->size(), QSize(1280, 1024));
    QSignalSpy geometryChangedSpy(client, &ShellClient::geometryChanged);
    QVERIFY(geometryChangedSpy.isValid());
    Test::render(surface.data(), shellSurface->size(), Qt::red);
    QVERIFY(geometryChangedSpy.wait());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(client->geometryRestore(), origGeo);
    QCOMPARE(client->isDecorated(), false);

    // go back to normal
    shellSurface->setToplevel();
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(shellSurface->size(), QSize(100, 50));
    Test::render(surface.data(), QSize(100, 50), Qt::red);
    QVERIFY(geometryChangedSpy.wait());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->geometry(), origGeo);
    QCOMPARE(client->geometryRestore(), origGeo);
    QCOMPARE(client->isDecorated(), true);
}

void TestMaximized::testBorderlessMaximizedWindowNoClientSideDecoration()
{
    // test case verifies that borderless maximized windows doesn't cause
    // clients to render client-side decorations instead (BUG 405385)

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

    QSignalSpy geometryChangedSpy(client, &ShellClient::geometryChanged);
    QVERIFY(geometryChangedSpy.isValid());
    QSignalSpy sizeChangeRequestedSpy(xdgShellSurface.data(), &XdgShellSurface::sizeChanged);
    QVERIFY(sizeChangeRequestedSpy.isValid());
    QSignalSpy configureRequestedSpy(xdgShellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());

    QVERIFY(client->isDecorated());
    QVERIFY(!client->noBorder());
    configureRequestedSpy.wait();
    QCOMPARE(decorationConfiguredSpy.count(), 1);
    QCOMPARE(deco->mode(), XdgDecoration::Mode::ServerSide);

    // go to maximized
    xdgShellSurface->setMaximized(true);
    QVERIFY(sizeChangeRequestedSpy.wait());
    QCOMPARE(sizeChangeRequestedSpy.count(), 1);

    for (const auto &it: configureRequestedSpy) {
        xdgShellSurface->ackConfigure(it[2].toInt());
    }
    Test::render(surface.data(), sizeChangeRequestedSpy.last().first().toSize(), Qt::red);
    QVERIFY(geometryChangedSpy.wait());

    // no deco
    QVERIFY(!client->isDecorated());
    QVERIFY(client->noBorder());
    // but still server-side
    QCOMPARE(deco->mode(), XdgDecoration::Mode::ServerSide);

    // go back to normal
    xdgShellSurface->setMaximized(false);
    QVERIFY(sizeChangeRequestedSpy.wait());
    QCOMPARE(sizeChangeRequestedSpy.count(), 2);

    for (const auto &it: configureRequestedSpy) {
        xdgShellSurface->ackConfigure(it[2].toInt());
    }
    Test::render(surface.data(), sizeChangeRequestedSpy.last().first().toSize(), Qt::red);
    QVERIFY(geometryChangedSpy.wait());

    QVERIFY(client->isDecorated());
    QVERIFY(!client->noBorder());
    QCOMPARE(deco->mode(), XdgDecoration::Mode::ServerSide);
}

WAYLANDTEST_MAIN(TestMaximized)
#include "maximize_test.moc"
