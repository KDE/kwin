/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "main.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#if KWIN_BUILD_X11
#include "utils/xcbutils.h"
#endif

#include <KWayland/Client/surface.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_virtualdesktop-0");

class VirtualDesktopTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
#if KWIN_BUILD_X11
    void testNetCurrentDesktop();
#endif
    void testLastDesktopRemoved();
    void testWindowOnMultipleDesktops();
    void testRemoveDesktopWithWindow();
};

void VirtualDesktopTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));
    qputenv("KWIN_XKB_DEFAULT_KEYMAP", "1");
    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();

#if KWIN_BUILD_X11
    if (kwinApp()->x11Connection()) {
        // verify the current desktop x11 property on startup, see BUG: 391034
        Xcb::Atom currentDesktopAtom("_NET_CURRENT_DESKTOP");
        QVERIFY(currentDesktopAtom.isValid());
        Xcb::Property currentDesktop(0, kwinApp()->x11RootWindow(), currentDesktopAtom, XCB_ATOM_CARDINAL, 0, 1);
        bool ok = true;
        QCOMPARE(currentDesktop.value(0, &ok), 0);
        QVERIFY(ok);
    }
#endif
}

void VirtualDesktopTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
    workspace()->setActiveOutput(QPoint(640, 512));
    VirtualDesktopManager::self()->setCount(1);
}

void VirtualDesktopTest::cleanup()
{
    Test::destroyWaylandConnection();
}

#if KWIN_BUILD_X11
void VirtualDesktopTest::testNetCurrentDesktop()
{
    if (!kwinApp()->x11Connection()) {
        QSKIP("Skipped on Wayland only");
    }
    QCOMPARE(VirtualDesktopManager::self()->count(), 1u);
    VirtualDesktopManager::self()->setCount(4);
    QCOMPARE(VirtualDesktopManager::self()->count(), 4u);

    Xcb::Atom currentDesktopAtom("_NET_CURRENT_DESKTOP");
    QVERIFY(currentDesktopAtom.isValid());
    Xcb::Property currentDesktop(0, kwinApp()->x11RootWindow(), currentDesktopAtom, XCB_ATOM_CARDINAL, 0, 1);
    bool ok = true;
    QCOMPARE(currentDesktop.value(0, &ok), 0);
    QVERIFY(ok);

    // go to desktop 2
    VirtualDesktopManager::self()->setCurrent(2);
    currentDesktop = Xcb::Property(0, kwinApp()->x11RootWindow(), currentDesktopAtom, XCB_ATOM_CARDINAL, 0, 1);
    QCOMPARE(currentDesktop.value(0, &ok), 1);
    QVERIFY(ok);

    // go to desktop 3
    VirtualDesktopManager::self()->setCurrent(3);
    currentDesktop = Xcb::Property(0, kwinApp()->x11RootWindow(), currentDesktopAtom, XCB_ATOM_CARDINAL, 0, 1);
    QCOMPARE(currentDesktop.value(0, &ok), 2);
    QVERIFY(ok);

    // go to desktop 4
    VirtualDesktopManager::self()->setCurrent(4);
    currentDesktop = Xcb::Property(0, kwinApp()->x11RootWindow(), currentDesktopAtom, XCB_ATOM_CARDINAL, 0, 1);
    QCOMPARE(currentDesktop.value(0, &ok), 3);
    QVERIFY(ok);

    // and back to first
    VirtualDesktopManager::self()->setCurrent(1);
    currentDesktop = Xcb::Property(0, kwinApp()->x11RootWindow(), currentDesktopAtom, XCB_ATOM_CARDINAL, 0, 1);
    QCOMPARE(currentDesktop.value(0, &ok), 0);
    QVERIFY(ok);
}
#endif

void VirtualDesktopTest::testLastDesktopRemoved()
{
    // first create a new desktop
    QCOMPARE(VirtualDesktopManager::self()->count(), 1u);
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);

    // switch to last desktop
    VirtualDesktopManager::self()->setCurrent(VirtualDesktopManager::self()->desktops().last());
    QCOMPARE(VirtualDesktopManager::self()->current(), 2u);

    // now create a window on this desktop
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    QVERIFY(window);
    QCOMPARE(window->desktops().count(), 1u);
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), window->desktops().first());

    // and remove last desktop
    VirtualDesktopManager::self()->setCount(1);
    QCOMPARE(VirtualDesktopManager::self()->count(), 1u);
    // now the window should be moved as well
    QCOMPARE(window->desktops().count(), 1u);
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), window->desktops().first());
}

void VirtualDesktopTest::testWindowOnMultipleDesktops()
{
    // first create two new desktops
    QCOMPARE(VirtualDesktopManager::self()->count(), 1u);
    VirtualDesktopManager::self()->setCount(3);
    QCOMPARE(VirtualDesktopManager::self()->count(), 3u);

    // switch to last desktop
    const auto desktops = VirtualDesktopManager::self()->desktops();
    VirtualDesktopManager::self()->setCurrent(desktops.at(2));

    // now create a window on this desktop
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QCOMPARE(window->desktops(), (QList<VirtualDesktop *>{desktops.at(2)}));

    // Set the window on desktop 2 as well
    window->enterDesktop(VirtualDesktopManager::self()->desktopForX11Id(2));
    QCOMPARE(window->desktops().count(), 2u);
    QCOMPARE(window->desktops()[0], desktops.at(2));
    QCOMPARE(window->desktops()[1], desktops.at(1));

    // leave desktop 3
    window->leaveDesktop(desktops.at(2));
    QCOMPARE(window->desktops(), (QList<VirtualDesktop *>{desktops.at(1)}));
    // leave desktop 2
    window->leaveDesktop(desktops.at(1));
    QCOMPARE(window->desktops(), QList<VirtualDesktop *>{});
    // we should be on all desktops now
    QVERIFY(window->isOnAllDesktops());
    // put on desktop 1
    window->enterDesktop(desktops.at(0));
    QVERIFY(window->isOnDesktop(desktops.at(0)));
    QVERIFY(!window->isOnDesktop(desktops.at(1)));
    QVERIFY(!window->isOnDesktop(desktops.at(2)));
    QCOMPARE(window->desktops().count(), 1u);
    // put on desktop 2
    window->enterDesktop(desktops.at(1));
    QVERIFY(window->isOnDesktop(desktops.at(0)));
    QVERIFY(window->isOnDesktop(desktops.at(1)));
    QVERIFY(!window->isOnDesktop(desktops.at(2)));
    QCOMPARE(window->desktops().count(), 2u);
    // put on desktop 3
    window->enterDesktop(desktops.at(2));
    QVERIFY(window->isOnDesktop(desktops.at(0)));
    QVERIFY(window->isOnDesktop(desktops.at(1)));
    QVERIFY(window->isOnDesktop(desktops.at(2)));
    QCOMPARE(window->desktops().count(), 3u);

    // entering twice dooes nothing
    window->enterDesktop(desktops.at(2));
    QCOMPARE(window->desktops().count(), 3u);

    // adding to "all desktops" results in just that one desktop
    window->setOnAllDesktops(true);
    QCOMPARE(window->desktops().count(), 0u);
    window->enterDesktop(desktops.at(2));
    QVERIFY(window->isOnDesktop(desktops.at(2)));
    QCOMPARE(window->desktops().count(), 1u);

    // leaving a desktop on "all desktops" puts on everything else
    window->setOnAllDesktops(true);
    QCOMPARE(window->desktops().count(), 0u);
    window->leaveDesktop(desktops.at(2));
    QVERIFY(window->isOnDesktop(desktops.at(0)));
    QVERIFY(window->isOnDesktop(desktops.at(1)));
    QCOMPARE(window->desktops().count(), 2u);
}

void VirtualDesktopTest::testRemoveDesktopWithWindow()
{
    // first create two new desktops
    QCOMPARE(VirtualDesktopManager::self()->count(), 1u);
    VirtualDesktopManager::self()->setCount(3);
    QCOMPARE(VirtualDesktopManager::self()->count(), 3u);

    // switch to last desktop
    VirtualDesktopManager::self()->setCurrent(VirtualDesktopManager::self()->desktops().last());
    QCOMPARE(VirtualDesktopManager::self()->current(), 3u);

    // now create a window on this desktop
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    QVERIFY(window);

    QCOMPARE(window->desktops().count(), 1u);
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), window->desktops().first());

    // Set the window on desktop 2 as well
    window->enterDesktop(VirtualDesktopManager::self()->desktops()[1]);
    QCOMPARE(window->desktops().count(), 2u);
    QCOMPARE(VirtualDesktopManager::self()->desktops()[2], window->desktops()[0]);
    QCOMPARE(VirtualDesktopManager::self()->desktops()[1], window->desktops()[1]);

    // remove desktop 3
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(window->desktops().count(), 1u);
    // window is only on desktop 2
    QCOMPARE(VirtualDesktopManager::self()->desktops()[1], window->desktops()[0]);

    // Again 3 desktops
    VirtualDesktopManager::self()->setCount(3);
    // move window to be only on desktop 3
    window->enterDesktop(VirtualDesktopManager::self()->desktops()[2]);
    window->leaveDesktop(VirtualDesktopManager::self()->desktops()[1]);
    QCOMPARE(window->desktops().count(), 1u);
    // window is only on desktop 3
    QCOMPARE(VirtualDesktopManager::self()->desktops()[2], window->desktops()[0]);

    // remove desktop 3
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(window->desktops().count(), 1u);
    // window is only on desktop 2
    QCOMPARE(VirtualDesktopManager::self()->desktops()[1], window->desktops()[0]);
}

WAYLANDTEST_MAIN(VirtualDesktopTest)
#include "virtual_desktop_test.moc"
