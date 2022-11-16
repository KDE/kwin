/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/outputbackend.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/surface.h>

using namespace KWin;

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
    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
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
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    auto window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    auto window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window1 != window2);

    QCOMPARE(workspace()->activeWindow(), window2);
    workspace()->slotToggleShowDesktop();
    QVERIFY(workspace()->showingDesktop());
    workspace()->slotToggleShowDesktop();
    QVERIFY(!workspace()->showingDesktop());

    QVERIFY(workspace()->activeWindow());
    QCOMPARE(workspace()->activeWindow(), window2);
}

void ShowingDesktopTest::testRestoreFocusWithDesktopWindow()
{
    // first create a desktop window

    std::unique_ptr<KWayland::Client::Surface> desktopSurface(Test::createSurface());
    QVERIFY(desktopSurface != nullptr);
    std::unique_ptr<Test::XdgToplevel> desktopShellSurface(Test::createXdgToplevelSurface(desktopSurface.get()));
    QVERIFY(desktopSurface != nullptr);
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> plasmaSurface(Test::waylandPlasmaShell()->createSurface(desktopSurface.get()));
    QVERIFY(plasmaSurface != nullptr);
    plasmaSurface->setRole(KWayland::Client::PlasmaShellSurface::Role::Desktop);

    auto desktop = Test::renderAndWaitForShown(desktopSurface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(desktop);
    QVERIFY(desktop->isDesktop());

    // now create some windows
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    auto window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    auto window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window1 != window2);

    QCOMPARE(workspace()->activeWindow(), window2);
    workspace()->slotToggleShowDesktop();
    QVERIFY(workspace()->showingDesktop());
    QCOMPARE(workspace()->activeWindow(), desktop);
    workspace()->slotToggleShowDesktop();
    QVERIFY(!workspace()->showingDesktop());

    QVERIFY(workspace()->activeWindow());
    QCOMPARE(workspace()->activeWindow(), window2);
}

WAYLANDTEST_MAIN(ShowingDesktopTest)
#include "showing_desktop_test.moc"
