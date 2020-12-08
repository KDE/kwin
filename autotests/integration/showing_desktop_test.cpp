/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_client.h"
#include "platform.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/surface.h>

using namespace KWin;
using namespace KWayland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_showing_desktop-0");

class ShowingDesktopTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testRestoreFocus();
    void testRestoreFocusWithDesktopWindow();
};

void ShowingDesktopTest::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    waylandServer()->initWorkspace();
}

void ShowingDesktopTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::PlasmaShell));
}

void ShowingDesktopTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void ShowingDesktopTest::testRestoreFocus()
{
    QScopedPointer<Surface> surface1(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface1(Test::createXdgShellStableSurface(surface1.data()));
    auto client1 = Test::renderAndWaitForShown(surface1.data(), QSize(100, 50), Qt::blue);
    QScopedPointer<Surface> surface2(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface2(Test::createXdgShellStableSurface(surface2.data()));
    auto client2 = Test::renderAndWaitForShown(surface2.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client1 != client2);

    QCOMPARE(workspace()->activeClient(), client2);
    workspace()->slotToggleShowDesktop();
    QVERIFY(workspace()->showingDesktop());
    workspace()->slotToggleShowDesktop();
    QVERIFY(!workspace()->showingDesktop());

    QVERIFY(workspace()->activeClient());
    QCOMPARE(workspace()->activeClient(), client2);
}

void ShowingDesktopTest::testRestoreFocusWithDesktopWindow()
{
    // first create a desktop window

    QScopedPointer<Surface> desktopSurface(Test::createSurface());
    QVERIFY(!desktopSurface.isNull());
    QScopedPointer<XdgShellSurface> desktopShellSurface(Test::createXdgShellStableSurface(desktopSurface.data()));
    QVERIFY(!desktopSurface.isNull());
    QScopedPointer<PlasmaShellSurface> plasmaSurface(Test::waylandPlasmaShell()->createSurface(desktopSurface.data()));
    QVERIFY(!plasmaSurface.isNull());
    plasmaSurface->setRole(PlasmaShellSurface::Role::Desktop);

    auto desktop = Test::renderAndWaitForShown(desktopSurface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(desktop);
    QVERIFY(desktop->isDesktop());

    // now create some windows
    QScopedPointer<Surface> surface1(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface1(Test::createXdgShellStableSurface(surface1.data()));
    auto client1 = Test::renderAndWaitForShown(surface1.data(), QSize(100, 50), Qt::blue);
    QScopedPointer<Surface> surface2(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface2(Test::createXdgShellStableSurface(surface2.data()));
    auto client2 = Test::renderAndWaitForShown(surface2.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client1 != client2);

    QCOMPARE(workspace()->activeClient(), client2);
    workspace()->slotToggleShowDesktop();
    QVERIFY(workspace()->showingDesktop());
    QCOMPARE(workspace()->activeClient(), desktop);
    workspace()->slotToggleShowDesktop();
    QVERIFY(!workspace()->showingDesktop());

    QVERIFY(workspace()->activeClient());
    QCOMPARE(workspace()->activeClient(), client2);
}

WAYLANDTEST_MAIN(ShowingDesktopTest)
#include "showing_desktop_test.moc"
