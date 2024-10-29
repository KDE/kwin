/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "pointer_input.h"
#include "screenedge.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/surface.h>

#include <KDecoration3/Decoration>

#include <netwm.h>
#include <xcb/xcb_icccm.h>

Q_DECLARE_METATYPE(KWin::StrutRects)

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_struts-0");

class StrutsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testX11Struts_data();
    void testX11Struts();
    void test363804();
    void testLeftScreenSmallerBottomAligned();
    void testWindowMoveWithPanelBetweenScreens();

private:
    KWayland::Client::Compositor *m_compositor = nullptr;
};

void StrutsTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    // set custom config which disables the Outline
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup group = config->group(QStringLiteral("Outline"));
    group.writeEntry(QStringLiteral("QmlPath"), QString("/does/not/exist.qml"));
    group.sync();

    kwinApp()->setConfig(config);

    kwinApp()->start();
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
}

void StrutsTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
    m_compositor = Test::waylandCompositor();

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
    QVERIFY(waylandServer()->windows().isEmpty());
}

void StrutsTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void StrutsTest::testX11Struts_data()
{
    QTest::addColumn<QRect>("windowGeometry");
    QTest::addColumn<int>("leftStrut");
    QTest::addColumn<int>("rightStrut");
    QTest::addColumn<int>("topStrut");
    QTest::addColumn<int>("bottomStrut");
    QTest::addColumn<int>("leftStrutStart");
    QTest::addColumn<int>("leftStrutEnd");
    QTest::addColumn<int>("rightStrutStart");
    QTest::addColumn<int>("rightStrutEnd");
    QTest::addColumn<int>("topStrutStart");
    QTest::addColumn<int>("topStrutEnd");
    QTest::addColumn<int>("bottomStrutStart");
    QTest::addColumn<int>("bottomStrutEnd");
    QTest::addColumn<QRectF>("screen0Maximized");
    QTest::addColumn<QRectF>("screen1Maximized");
    QTest::addColumn<QRectF>("workArea");
    QTest::addColumn<StrutRects>("restrictedMoveArea");

    QTest::newRow("bottom panel/no strut") << QRect(0, 980, 1280, 44)
                                           << 0 << 0 << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << QRectF(0, 0, 1280, 1024)
                                           << QRectF(1280, 0, 1280, 1024)
                                           << QRectF(0, 0, 2560, 1024)
                                           << StrutRects();
    QTest::newRow("bottom panel/strut") << QRect(0, 980, 1280, 44)
                                        << 0 << 0 << 0 << 44
                                        << 0 << 0
                                        << 0 << 0
                                        << 0 << 0
                                        << 0 << 1279
                                        << QRectF(0, 0, 1280, 980)
                                        << QRectF(1280, 0, 1280, 1024)
                                        << QRectF(0, 0, 2560, 980)
                                        << StrutRects{StrutRect(0, 980, 1279, 44)};
    QTest::newRow("top panel/no strut") << QRect(0, 0, 1280, 44)
                                        << 0 << 0 << 0 << 0
                                        << 0 << 0
                                        << 0 << 0
                                        << 0 << 0
                                        << 0 << 0
                                        << QRectF(0, 0, 1280, 1024)
                                        << QRectF(1280, 0, 1280, 1024)
                                        << QRectF(0, 0, 2560, 1024)
                                        << StrutRects();
    QTest::newRow("top panel/strut") << QRect(0, 0, 1280, 44)
                                     << 0 << 0 << 44 << 0
                                     << 0 << 0
                                     << 0 << 0
                                     << 0 << 1279
                                     << 0 << 0
                                     << QRectF(0, 44, 1280, 980)
                                     << QRectF(1280, 0, 1280, 1024)
                                     << QRectF(0, 44, 2560, 980)
                                     << StrutRects{StrutRect(0, 0, 1279, 44)};
    QTest::newRow("left panel/no strut") << QRect(0, 0, 60, 1024)
                                         << 0 << 0 << 0 << 0
                                         << 0 << 0
                                         << 0 << 0
                                         << 0 << 0
                                         << 0 << 0
                                         << QRectF(0, 0, 1280, 1024)
                                         << QRectF(1280, 0, 1280, 1024)
                                         << QRectF(0, 0, 2560, 1024)
                                         << StrutRects();
    QTest::newRow("left panel/strut") << QRect(0, 0, 60, 1024)
                                      << 60 << 0 << 0 << 0
                                      << 0 << 1023
                                      << 0 << 0
                                      << 0 << 0
                                      << 0 << 0
                                      << QRectF(60, 0, 1220, 1024)
                                      << QRectF(1280, 0, 1280, 1024)
                                      << QRectF(60, 0, 2500, 1024)
                                      << StrutRects{StrutRect(0, 0, 60, 1023)};
    QTest::newRow("right panel/no strut") << QRect(1220, 0, 60, 1024)
                                          << 0 << 0 << 0 << 0
                                          << 0 << 0
                                          << 0 << 0
                                          << 0 << 0
                                          << 0 << 0
                                          << QRectF(0, 0, 1280, 1024)
                                          << QRectF(1280, 0, 1280, 1024)
                                          << QRectF(0, 0, 2560, 1024)
                                          << StrutRects();
    QTest::newRow("right panel/strut") << QRect(1220, 0, 60, 1024)
                                       << 0 << 1340 << 0 << 0
                                       << 0 << 0
                                       << 0 << 1023
                                       << 0 << 0
                                       << 0 << 0
                                       << QRectF(0, 0, 1220, 1024)
                                       << QRectF(1280, 0, 1280, 1024)
                                       << QRectF(0, 0, 2560, 1024)
                                       << StrutRects{StrutRect(1220, 0, 60, 1023)};
    // second screen
    QTest::newRow("bottom panel 1/no strut") << QRect(1280, 980, 1280, 44)
                                             << 0 << 0 << 0 << 0
                                             << 0 << 0
                                             << 0 << 0
                                             << 0 << 0
                                             << 0 << 0
                                             << QRectF(0, 0, 1280, 1024)
                                             << QRectF(1280, 0, 1280, 1024)
                                             << QRectF(0, 0, 2560, 1024)
                                             << StrutRects();
    QTest::newRow("bottom panel 1/strut") << QRect(1280, 980, 1280, 44)
                                          << 0 << 0 << 0 << 44
                                          << 0 << 0
                                          << 0 << 0
                                          << 0 << 0
                                          << 1280 << 2559
                                          << QRectF(0, 0, 1280, 1024)
                                          << QRectF(1280, 0, 1280, 980)
                                          << QRectF(0, 0, 2560, 980)
                                          << StrutRects{StrutRect(1280, 980, 1279, 44)};
    QTest::newRow("top panel 1/no strut") << QRect(1280, 0, 1280, 44)
                                          << 0 << 0 << 0 << 0
                                          << 0 << 0
                                          << 0 << 0
                                          << 0 << 0
                                          << 0 << 0
                                          << QRectF(0, 0, 1280, 1024)
                                          << QRectF(1280, 0, 1280, 1024)
                                          << QRectF(0, 0, 2560, 1024)
                                          << StrutRects();
    QTest::newRow("top panel 1 /strut") << QRect(1280, 0, 1280, 44)
                                        << 0 << 0 << 44 << 0
                                        << 0 << 0
                                        << 0 << 0
                                        << 1280 << 2559
                                        << 0 << 0
                                        << QRectF(0, 0, 1280, 1024)
                                        << QRectF(1280, 44, 1280, 980)
                                        << QRectF(0, 44, 2560, 980)
                                        << StrutRects{StrutRect(1280, 0, 1279, 44)};
    QTest::newRow("left panel 1/no strut") << QRect(1280, 0, 60, 1024)
                                           << 0 << 0 << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << 0 << 0
                                           << QRectF(0, 0, 1280, 1024)
                                           << QRectF(1280, 0, 1280, 1024)
                                           << QRectF(0, 0, 2560, 1024)
                                           << StrutRects();
    QTest::newRow("left panel 1/strut") << QRect(1280, 0, 60, 1024)
                                        << 1340 << 0 << 0 << 0
                                        << 0 << 1023
                                        << 0 << 0
                                        << 0 << 0
                                        << 0 << 0
                                        << QRectF(0, 0, 1280, 1024)
                                        << QRectF(1340, 0, 1220, 1024)
                                        << QRectF(0, 0, 2560, 1024)
                                        << StrutRects{StrutRect(1280, 0, 60, 1023)};
    // invalid struts
    QTest::newRow("bottom panel/ invalid strut") << QRect(0, 980, 1280, 44)
                                                 << 1280 << 0 << 0 << 44
                                                 << 980 << 1024
                                                 << 0 << 0
                                                 << 0 << 0
                                                 << 0 << 1279
                                                 << QRectF(0, 0, 1280, 1024)
                                                 << QRectF(1280, 0, 1280, 1024)
                                                 << QRectF(0, 0, 2560, 1024)
                                                 << StrutRects{StrutRect(0, 980, 1279, 44), StrutRect(0, 980, 1280, 44)};
    QTest::newRow("top panel/ invalid strut") << QRect(0, 0, 1280, 44)
                                              << 1280 << 0 << 44 << 0
                                              << 0 << 44
                                              << 0 << 0
                                              << 0 << 1279
                                              << 0 << 0
                                              << QRectF(0, 0, 1280, 1024)
                                              << QRectF(1280, 0, 1280, 1024)
                                              << QRectF(0, 0, 2560, 1024)
                                              << StrutRects{StrutRect(0, 0, 1279, 44), StrutRect(0, 0, 1280, 44)};
    QTest::newRow("top panel/invalid strut 2") << QRect(0, 0, 1280, 44)
                                               << 0 << 0 << 1024 << 0
                                               << 0 << 0
                                               << 0 << 0
                                               << 0 << 1279
                                               << 0 << 0
                                               << QRectF(0, 0, 1280, 1024)
                                               << QRectF(1280, 0, 1280, 1024)
                                               << QRectF(0, 0, 2560, 1024)
                                               << StrutRects();
}

void StrutsTest::testX11Struts()
{
    // this test verifies that struts are applied correctly for X11 windows

    VirtualDesktop *desktop = VirtualDesktopManager::self()->currentDesktop();
    const QList<Output *> outputs = workspace()->outputs();

    // no, struts yet
    // first screen
    QCOMPARE(workspace()->clientArea(PlacementArea, outputs[0], desktop), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MovementArea, outputs[0], desktop), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeArea, outputs[0], desktop), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeFullArea, outputs[0], desktop), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(FullScreenArea, outputs[0], desktop), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(ScreenArea, outputs[0], desktop), QRect(0, 0, 1280, 1024));
    // second screen
    QCOMPARE(workspace()->clientArea(PlacementArea, outputs[1], desktop), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MovementArea, outputs[1], desktop), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeArea, outputs[1], desktop), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeFullArea, outputs[1], desktop), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(FullScreenArea, outputs[1], desktop), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(ScreenArea, outputs[1], desktop), QRect(1280, 0, 1280, 1024));
    // combined
    QCOMPARE(workspace()->clientArea(WorkArea, outputs[0], desktop), QRect(0, 0, 2560, 1024));
    QCOMPARE(workspace()->clientArea(FullArea, outputs[0], desktop), QRect(0, 0, 2560, 1024));
    QCOMPARE(workspace()->restrictedMoveArea(desktop), StrutRects());

    // create an xcb window
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));

    xcb_window_t windowId = xcb_generate_id(c.get());
    QFETCH(QRect, windowGeometry);
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    NETWinInfo info(c.get(), windowId, rootWindow(), NET::WMAllProperties, NET::WM2AllProperties);
    info.setWindowType(NET::Dock);
    // set the extended strut
    QFETCH(int, leftStrut);
    QFETCH(int, rightStrut);
    QFETCH(int, topStrut);
    QFETCH(int, bottomStrut);
    QFETCH(int, leftStrutStart);
    QFETCH(int, leftStrutEnd);
    QFETCH(int, rightStrutStart);
    QFETCH(int, rightStrutEnd);
    QFETCH(int, topStrutStart);
    QFETCH(int, topStrutEnd);
    QFETCH(int, bottomStrutStart);
    QFETCH(int, bottomStrutEnd);
    NETExtendedStrut strut;
    strut.left_start = leftStrutStart;
    strut.left_end = leftStrutEnd;
    strut.left_width = leftStrut;
    strut.right_start = rightStrutStart;
    strut.right_end = rightStrutEnd;
    strut.right_width = rightStrut;
    strut.top_start = topStrutStart;
    strut.top_end = topStrutEnd;
    strut.top_width = topStrut;
    strut.bottom_start = bottomStrutStart;
    strut.bottom_end = bottomStrutEnd;
    strut.bottom_width = bottomStrut;
    info.setExtendedStrut(strut);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QVERIFY(!window->isDecorated());
    QCOMPARE(window->windowType(), WindowType::Dock);
    QCOMPARE(window->frameGeometry(), windowGeometry);

    // this should have affected the client area
    // some props are independent of struts - those first
    // screen 0
    QCOMPARE(workspace()->clientArea(MovementArea, outputs[0], desktop), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeFullArea, outputs[0], desktop), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(FullScreenArea, outputs[0], desktop), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(ScreenArea, outputs[0], desktop), QRect(0, 0, 1280, 1024));
    // screen 1
    QCOMPARE(workspace()->clientArea(MovementArea, outputs[1], desktop), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeFullArea, outputs[1], desktop), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(FullScreenArea, outputs[1], desktop), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(ScreenArea, outputs[1], desktop), QRect(1280, 0, 1280, 1024));
    // combined
    QCOMPARE(workspace()->clientArea(FullArea, outputs[0], desktop), QRect(0, 0, 2560, 1024));

    // now verify the actual updated client areas
    QTEST(workspace()->clientArea(PlacementArea, outputs[0], desktop), "screen0Maximized");
    QTEST(workspace()->clientArea(MaximizeArea, outputs[0], desktop), "screen0Maximized");
    QTEST(workspace()->clientArea(PlacementArea, outputs[1], desktop), "screen1Maximized");
    QTEST(workspace()->clientArea(MaximizeArea, outputs[1], desktop), "screen1Maximized");
    QTEST(workspace()->clientArea(WorkArea, outputs[0], desktop), "workArea");
    QTEST(workspace()->restrictedMoveArea(desktop), "restrictedMoveArea");

    // and destroy the window again
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    c.reset();

    QSignalSpy windowClosedSpy(window, &X11Window::closed);
    QVERIFY(windowClosedSpy.wait());

    // now struts should be removed again
    QCOMPARE(workspace()->clientArea(PlacementArea, outputs[0], desktop), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MovementArea, outputs[0], desktop), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeArea, outputs[0], desktop), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeFullArea, outputs[0], desktop), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(FullScreenArea, outputs[0], desktop), QRect(0, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(ScreenArea, outputs[0], desktop), QRect(0, 0, 1280, 1024));
    // second screen
    QCOMPARE(workspace()->clientArea(PlacementArea, outputs[1], desktop), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MovementArea, outputs[1], desktop), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeArea, outputs[1], desktop), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(MaximizeFullArea, outputs[1], desktop), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(FullScreenArea, outputs[1], desktop), QRect(1280, 0, 1280, 1024));
    QCOMPARE(workspace()->clientArea(ScreenArea, outputs[1], desktop), QRect(1280, 0, 1280, 1024));
    // combined
    QCOMPARE(workspace()->clientArea(WorkArea, outputs[0], desktop), QRect(0, 0, 2560, 1024));
    QCOMPARE(workspace()->clientArea(FullArea, outputs[0], desktop), QRect(0, 0, 2560, 1024));
    QCOMPARE(workspace()->restrictedMoveArea(desktop), StrutRects());
}

void StrutsTest::test363804()
{
    // this test verifies the condition described in BUG 363804
    // two screens in a vertical setup, aligned to right border with panel on the bottom screen
    const QList<QRect> geometries{QRect(0, 0, 1920, 1080), QRect(554, 1080, 1366, 768)};
    Test::setOutputConfig(geometries);
    QCOMPARE(workspace()->geometry(), QRect(0, 0, 1920, 1848));

    VirtualDesktop *desktop = VirtualDesktopManager::self()->currentDesktop();
    const QList<Output *> outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), geometries[0]);
    QCOMPARE(outputs[1]->geometry(), geometries[1]);

    // create an xcb window
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));

    xcb_window_t windowId = xcb_generate_id(c.get());
    const QRect windowGeometry(554, 1812, 1366, 36);
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    NETWinInfo info(c.get(), windowId, rootWindow(), NET::WMAllProperties, NET::WM2AllProperties);
    info.setWindowType(NET::Dock);
    NETExtendedStrut strut;
    strut.left_start = 0;
    strut.left_end = 0;
    strut.left_width = 0;
    strut.right_start = 0;
    strut.right_end = 0;
    strut.right_width = 0;
    strut.top_start = 0;
    strut.top_end = 0;
    strut.top_width = 0;
    strut.bottom_start = 554;
    strut.bottom_end = 1919;
    strut.bottom_width = 36;
    info.setExtendedStrut(strut);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QVERIFY(!window->isDecorated());
    QCOMPARE(window->windowType(), WindowType::Dock);
    QCOMPARE(window->frameGeometry(), windowGeometry);

    // now verify the actual updated client areas
    QCOMPARE(workspace()->clientArea(PlacementArea, outputs[0], desktop), geometries.at(0));
    QCOMPARE(workspace()->clientArea(MaximizeArea, outputs[0], desktop), geometries.at(0));
    QCOMPARE(workspace()->clientArea(PlacementArea, outputs[1], desktop), QRect(554, 1080, 1366, 732));
    QCOMPARE(workspace()->clientArea(MaximizeArea, outputs[1], desktop), QRect(554, 1080, 1366, 732));
    QCOMPARE(workspace()->clientArea(WorkArea, outputs[0], desktop), QRect(0, 0, 1920, 1812));

    // and destroy the window again
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    c.reset();

    QSignalSpy windowClosedSpy(window, &X11Window::closed);
    QVERIFY(windowClosedSpy.wait());
}

void StrutsTest::testLeftScreenSmallerBottomAligned()
{
    // this test verifies a two screen setup with the left screen smaller than the right and bottom aligned
    // the panel is on the top of the left screen, thus not at 0/0
    const QList<QRect> geometries{QRect(0, 282, 1366, 768), QRect(1366, 0, 1680, 1050)};
    Test::setOutputConfig(geometries);
    QCOMPARE(workspace()->geometry(), QRect(0, 0, 3046, 1050));

    const QList<Output *> outputs = workspace()->outputs();
    QCOMPARE(outputs[0]->geometry(), geometries.at(0));
    QCOMPARE(outputs[1]->geometry(), geometries.at(1));

    // the test window will be on the current desktop
    VirtualDesktop *desktop = VirtualDesktopManager::self()->currentDesktop();

    // create the panel
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));

    xcb_window_t windowId = xcb_generate_id(c.get());
    const QRect windowGeometry(0, 282, 1366, 24);
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    NETWinInfo info(c.get(), windowId, rootWindow(), NET::WMAllProperties, NET::WM2AllProperties);
    info.setWindowType(NET::Dock);
    NETExtendedStrut strut;
    strut.left_start = 0;
    strut.left_end = 0;
    strut.left_width = 0;
    strut.right_start = 0;
    strut.right_end = 0;
    strut.right_width = 0;
    strut.top_start = 0;
    strut.top_end = 1365;
    strut.top_width = 306;
    strut.bottom_start = 0;
    strut.bottom_end = 0;
    strut.bottom_width = 0;
    info.setExtendedStrut(strut);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QVERIFY(!window->isDecorated());
    QCOMPARE(window->windowType(), WindowType::Dock);
    QCOMPARE(window->frameGeometry(), windowGeometry);

    // now verify the actual updated client areas
    QCOMPARE(workspace()->clientArea(PlacementArea, outputs[0], desktop), QRect(0, 306, 1366, 744));
    QCOMPARE(workspace()->clientArea(MaximizeArea, outputs[0], desktop), QRect(0, 306, 1366, 744));
    QCOMPARE(workspace()->clientArea(PlacementArea, outputs[1], desktop), geometries.at(1));
    QCOMPARE(workspace()->clientArea(MaximizeArea, outputs[1], desktop), geometries.at(1));
    QCOMPARE(workspace()->clientArea(WorkArea, outputs[0], desktop), QRect(0, 0, 3046, 1050));

    // and destroy the window again
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    c.reset();

    QSignalSpy windowClosedSpy(window, &X11Window::closed);
    QVERIFY(windowClosedSpy.wait());
}

void StrutsTest::testWindowMoveWithPanelBetweenScreens()
{
    // this test verifies the condition of BUG
    // when moving a window with decorations in a restricted way it should pass from one screen
    // to the other even if there is a panel in between.

    // left screen must be smaller than right screen
    const QList<QRect> geometries{QRect(0, 282, 1366, 768), QRect(1366, 0, 1680, 1050)};
    Test::setOutputConfig(geometries);
    QCOMPARE(workspace()->geometry(), QRect(0, 0, 3046, 1050));

    const QList<Output *> outputs = workspace()->outputs();
    QCOMPARE(outputs[0]->geometry(), geometries.at(0));
    QCOMPARE(outputs[1]->geometry(), geometries.at(1));

    // all windows will be placed on the current desktop
    VirtualDesktop *desktop = VirtualDesktopManager::self()->currentDesktop();

    // create the panel on the right screen, left edge
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));

    xcb_window_t windowId = xcb_generate_id(c.get());
    const QRect windowGeometry(1366, 0, 24, 1050);
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    NETWinInfo info(c.get(), windowId, rootWindow(), NET::WMAllProperties, NET::WM2AllProperties);
    info.setWindowType(NET::Dock);
    NETExtendedStrut strut;
    strut.left_start = 0;
    strut.left_end = 1050;
    strut.left_width = 1366 + 24;
    strut.right_start = 0;
    strut.right_end = 0;
    strut.right_width = 0;
    strut.top_start = 0;
    strut.top_end = 0;
    strut.top_width = 0;
    strut.bottom_start = 0;
    strut.bottom_end = 0;
    strut.bottom_width = 0;
    info.setExtendedStrut(strut);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QVERIFY(!window->isDecorated());
    QCOMPARE(window->windowType(), WindowType::Dock);
    QCOMPARE(window->frameGeometry(), windowGeometry);

    // now verify the actual updated client areas
    QCOMPARE(workspace()->clientArea(PlacementArea, outputs[0], desktop), QRect(0, 282, 1366, 768));
    QCOMPARE(workspace()->clientArea(MaximizeArea, outputs[0], desktop), QRect(0, 282, 1366, 768));
    QCOMPARE(workspace()->clientArea(PlacementArea, outputs[1], desktop), QRect(1390, 0, 1656, 1050));
    QCOMPARE(workspace()->clientArea(MaximizeArea, outputs[1], desktop), QRect(1390, 0, 1656, 1050));
    QCOMPARE(workspace()->clientArea(WorkArea, outputs[0], desktop), QRect(0, 0, 3046, 1050));
    QCOMPARE(workspace()->restrictedMoveArea(desktop), StrutRects{StrutRect(1366, 0, 24, 1050)});

    // create another window and try to move it

    xcb_window_t w2 = xcb_generate_id(c.get());
    const QRect windowGeometry2(1500, 400, 200, 300);
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, w2, rootWindow(),
                      windowGeometry2.x(),
                      windowGeometry2.y(),
                      windowGeometry2.width(),
                      windowGeometry2.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints2;
    memset(&hints2, 0, sizeof(hints2));
    xcb_icccm_size_hints_set_position(&hints2, 1, windowGeometry2.x(), windowGeometry2.y());
    xcb_icccm_size_hints_set_min_size(&hints2, 200, 300);
    xcb_icccm_set_wm_normal_hints(c.get(), w2, &hints2);
    xcb_map_window(c.get(), w2);
    xcb_flush(c.get());
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window2 = windowCreatedSpy.last().first().value<X11Window *>();
    QVERIFY(window2);
    QVERIFY(window2 != window);
    QVERIFY(window2->isDecorated());
    QCOMPARE(window2->clientSize(), QSize(200, 300));
    QCOMPARE(window2->pos(), QPoint(1500, 400));

    const QRectF origGeo = window2->frameGeometry();
    input()->pointer()->warp(origGeo.center());
    workspace()->performWindowOperation(window2, Options::MoveOp);
    QTRY_COMPARE(workspace()->moveResizeWindow(), window2);
    QVERIFY(window2->isInteractiveMove());
    // move to next screen - step is 8 pixel, so 800 pixel
    for (int i = 0; i < 100; i++) {
        window2->keyPressEvent(Qt::Key_Left);
    }
    window2->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(window2->isInteractiveMove(), false);
    QVERIFY(workspace()->moveResizeWindow() == nullptr);
    QCOMPARE(window2->frameGeometry(), QRectF(origGeo.translated(-800, 0)));
}

}

WAYLANDTEST_MAIN(KWin::StrutsTest)
#include "struts_test.moc"
