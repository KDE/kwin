/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2023 Natalie Clarius <natalie_clarius@yahoo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "placement.h"
#include "pointer_input.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_placement-0");

class TestPlacement : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void initTestCase();

    void testPlaceSmart();
    void testPlaceMaximized();
    void testPlaceMaximizedLeavesFullscreen();
    void testPlaceCentered();
    void testPlaceUnderMouse();
    void testPlaceZeroCornered();
    void testPlaceRandom();
    void testFullscreen();
    void testCascadeIfCovering();
    void testCascadeIfCoveringIgnoreNonCovering();
    void testCascadeIfCoveringIgnoreOutOfArea();
    void testCascadeIfCoveringIgnoreAlreadyCovered();
    void testTitlebarOnScreen_data();
    void testTitlebarOnScreen();

private:
    void setPlacementPolicy(PlacementPolicy policy);
    struct WindowHandle
    {
        Window *window;
        std::unique_ptr<KWayland::Client::Surface> surface;
        std::unique_ptr<Test::XdgToplevel> shellSurface;
    };
    struct PlaceWindowResult
    {
        QSizeF initiallyConfiguredSize;
        Test::XdgToplevel::States initiallyConfiguredStates;
        QRectF finalGeometry;
    };
    /*
     * Create a window and return relevant results for testing
     * defaultSize is the buffer size to use if the compositor returns an empty size in the first configure
     * event.
     */
    std::tuple<PlaceWindowResult, WindowHandle> createAndPlaceWindow(const QSize &defaultSize);
};

void TestPlacement::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::LayerShellV1));

    workspace()->setActiveOutput(QPoint(640, 512));
    KWin::input()->pointer()->warp(QPoint(640, 512));
}

void TestPlacement::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestPlacement::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void TestPlacement::setPlacementPolicy(PlacementPolicy policy)
{
    auto group = kwinApp()->config()->group(QStringLiteral("Windows"));
    group.writeEntry("Placement", Placement::policyToString(policy));
    group.sync();
    Workspace::self()->slotReconfigure();
}

std::tuple<TestPlacement::PlaceWindowResult, TestPlacement::WindowHandle> TestPlacement::createAndPlaceWindow(const QSize &defaultSize)
{
    PlaceWindowResult rc;

    // create a new window
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly);

    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    surfaceConfigureRequestedSpy.wait();

    rc.initiallyConfiguredSize = toplevelConfigureRequestedSpy[0][0].toSize();
    rc.initiallyConfiguredStates = toplevelConfigureRequestedSpy[0][1].value<Test::XdgToplevel::States>();
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy[0][0].toUInt());

    QSizeF size = rc.initiallyConfiguredSize;

    if (size.isEmpty()) {
        size = defaultSize;
    }

    auto window = Test::renderAndWaitForShown(surface.get(), size.toSize(), Qt::red);

    rc.finalGeometry = window->frameGeometry();
    return {rc, WindowHandle{
                    .window = window,
                    .surface = std::move(surface),
                    .shellSurface = std::move(shellSurface),
                }};
}

void TestPlacement::testPlaceSmart()
{
    const auto outputs = workspace()->outputs();
    const QList<QRect> desiredGeometries{
        QRect(0, 0, 600, 500),
        QRect(600, 0, 600, 500),
        QRect(0, 500, 600, 500),
        QRect(600, 500, 600, 500),
        QRect(680, 524, 600, 500),
        QRect(680, 0, 600, 500),
        QRect(0, 524, 600, 500),
        QRect(0, 0, 600, 500),
    };

    setPlacementPolicy(PlacementSmart);

    std::vector<WindowHandle> handles;

    for (const QRect &desiredGeometry : desiredGeometries) {
        auto [windowPlacement, handle] = createAndPlaceWindow(QSize(600, 500));
        handles.push_back(std::move(handle));

        // smart placement shouldn't define a size on windows
        QCOMPARE(windowPlacement.initiallyConfiguredSize, QSize(0, 0));
        QCOMPARE(windowPlacement.finalGeometry.size(), QSize(600, 500));

        QVERIFY(outputs[0]->geometry().contains(windowPlacement.finalGeometry.toRect()));

        QCOMPARE(windowPlacement.finalGeometry.toRect(), desiredGeometry);
    }
}

void TestPlacement::testPlaceMaximized()
{
    setPlacementPolicy(PlacementMaximizing);

    // add a top panel
    std::unique_ptr<KWayland::Client::Surface> panelSurface{Test::createSurface()};
    std::unique_ptr<Test::LayerSurfaceV1> panelShellSurface{Test::createLayerSurfaceV1(panelSurface.get(), QStringLiteral("dock"))};
    panelShellSurface->set_size(1280, 20);
    panelShellSurface->set_anchor(Test::LayerSurfaceV1::anchor_top);
    panelShellSurface->set_exclusive_zone(20);
    panelSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy panelConfigureRequestedSpy(panelShellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(panelConfigureRequestedSpy.wait());
    Test::renderAndWaitForShown(panelSurface.get(), panelConfigureRequestedSpy.last().at(1).toSize(), Qt::blue);

    std::vector<WindowHandle> handles;

    // all windows should be initially maximized with an initial configure size sent
    for (int i = 0; i < 4; i++) {
        auto [windowPlacement, handle] = createAndPlaceWindow(QSize(600, 500));
        QVERIFY(windowPlacement.initiallyConfiguredStates & Test::XdgToplevel::State::Maximized);
        QCOMPARE(windowPlacement.initiallyConfiguredSize, QSize(1280, 1024 - 20));
        QCOMPARE(windowPlacement.finalGeometry, QRect(0, 20, 1280, 1024 - 20)); // under the panel
        handles.push_back(std::move(handle));
    }
}

void TestPlacement::testPlaceMaximizedLeavesFullscreen()
{
    setPlacementPolicy(PlacementMaximizing);

    // add a top panel
    std::unique_ptr<KWayland::Client::Surface> panelSurface{Test::createSurface()};
    std::unique_ptr<Test::LayerSurfaceV1> panelShellSurface{Test::createLayerSurfaceV1(panelSurface.get(), QStringLiteral("dock"))};
    panelShellSurface->set_size(1280, 20);
    panelShellSurface->set_anchor(Test::LayerSurfaceV1::anchor_top);
    panelShellSurface->set_exclusive_zone(20);
    panelSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy panelConfigureRequestedSpy(panelShellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(panelConfigureRequestedSpy.wait());
    Test::renderAndWaitForShown(panelSurface.get(), panelConfigureRequestedSpy.last().at(1).toSize(), Qt::blue);

    std::vector<WindowHandle> handles;

    // all windows should be initially fullscreen with an initial configure size sent, despite the policy
    for (int i = 0; i < 4; i++) {
        std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
        auto shellSurface = Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly);
        shellSurface->set_fullscreen(nullptr);
        QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
        QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
        QVERIFY(surfaceConfigureRequestedSpy.wait());

        auto initiallyConfiguredSize = toplevelConfigureRequestedSpy[0][0].toSize();
        auto initiallyConfiguredStates = toplevelConfigureRequestedSpy[0][1].value<Test::XdgToplevel::States>();
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy[0][0].toUInt());

        auto window = Test::renderAndWaitForShown(surface.get(), initiallyConfiguredSize, Qt::red);

        QVERIFY(initiallyConfiguredStates & Test::XdgToplevel::State::Fullscreen);
        QCOMPARE(initiallyConfiguredSize, QSize(1280, 1024));
        QCOMPARE(window->frameGeometry(), QRect(0, 0, 1280, 1024));

        handles.emplace_back(WindowHandle{
            .window = window,
            .surface = std::move(surface),
            .shellSurface = std::move(shellSurface),
        });
    }
}

void TestPlacement::testPlaceCentered()
{
    // This test verifies that Centered placement policy works.

    KConfigGroup group = kwinApp()->config()->group(QStringLiteral("Windows"));
    group.writeEntry("Placement", Placement::policyToString(PlacementCentered));
    group.sync();
    workspace()->slotReconfigure();

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::red);
    QVERIFY(window);
    QCOMPARE(window->frameGeometry(), QRect(590, 487, 100, 50));

    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void TestPlacement::testPlaceUnderMouse()
{
    // This test verifies that Under Mouse placement policy works.

    KConfigGroup group = kwinApp()->config()->group(QStringLiteral("Windows"));
    group.writeEntry("Placement", Placement::policyToString(PlacementUnderMouse));
    group.sync();
    workspace()->slotReconfigure();

    KWin::input()->pointer()->warp(QPoint(200, 300));
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), QPoint(200, 300));

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::red);
    QVERIFY(window);
    QCOMPARE(window->frameGeometry(), QRect(150, 275, 100, 50));

    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void TestPlacement::testPlaceZeroCornered()
{
    // This test verifies that the Zero-Cornered placement policy works.

    KConfigGroup group = kwinApp()->config()->group(QStringLiteral("Windows"));
    group.writeEntry("Placement", Placement::policyToString(PlacementZeroCornered));
    group.sync();
    workspace()->slotReconfigure();

    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::red);
    QVERIFY(window1);
    QCOMPARE(window1->pos(), QPoint(0, 0));
    QCOMPARE(window1->size(), QSize(100, 50));

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window2);
    QCOMPARE(window2->pos(), window1->pos() + workspace()->cascadeOffset(workspace()->clientArea(PlacementArea, window2)));
    QCOMPARE(window2->size(), QSize(100, 50));

    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    Window *window3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::green);
    QVERIFY(window3);
    QCOMPARE(window3->pos(), window2->pos() + workspace()->cascadeOffset(workspace()->clientArea(PlacementArea, window3)));
    QCOMPARE(window3->size(), QSize(100, 50));

    shellSurface3.reset();
    QVERIFY(Test::waitForWindowClosed(window3));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowClosed(window2));
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowClosed(window1));
}

void TestPlacement::testPlaceRandom()
{
    // This test verifies that Random placement policy works.

    KConfigGroup group = kwinApp()->config()->group(QStringLiteral("Windows"));
    group.writeEntry("Placement", Placement::policyToString(PlacementRandom));
    group.sync();
    workspace()->slotReconfigure();

    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::red);
    QVERIFY(window1);
    QCOMPARE(window1->size(), QSize(100, 50));

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window2);
    QVERIFY(window2->pos() != window1->pos());
    QCOMPARE(window2->size(), QSize(100, 50));

    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    Window *window3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::green);
    QVERIFY(window3);
    QVERIFY(window3->pos() != window1->pos());
    QVERIFY(window3->pos() != window2->pos());
    QCOMPARE(window3->size(), QSize(100, 50));

    shellSurface3.reset();
    QVERIFY(Test::waitForWindowClosed(window3));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowClosed(window2));
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowClosed(window1));
}

void TestPlacement::testFullscreen()
{
    const QList<Output *> outputs = workspace()->outputs();

    setPlacementPolicy(PlacementSmart);
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));

    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::red);
    QVERIFY(window);
    window->sendToOutput(outputs[0]);

    // Wait for the configure event with the activated state.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    window->setFullScreen(true);

    QSignalSpy geometryChangedSpy(window, &Window::frameGeometryChanged);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).toSize(), Qt::red);
    QVERIFY(geometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), outputs[0]->geometry());

    // this doesn't require a round trip, so should be immediate
    window->sendToOutput(outputs[1]);
    QCOMPARE(window->frameGeometry(), outputs[1]->geometry());
    QCOMPARE(geometryChangedSpy.count(), 2);
}

void TestPlacement::testCascadeIfCovering()
{
    // This test verifies that the cascade-if-covering adjustment works for the Centered placement
    // policy.

    KConfigGroup group = kwinApp()->config()->group(QStringLiteral("Windows"));
    group.writeEntry("Placement", Placement::policyToString(PlacementCentered));
    group.sync();
    workspace()->slotReconfigure();

    // window should be in center
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::red);
    QVERIFY(window1);
    QCOMPARE(window1->pos(), QPoint(590, 487));
    QCOMPARE(window1->size(), QSize(100, 50));

    // window should be cascaded to avoid overlapping window 1
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window2);
    QCOMPARE(window2->pos(), window1->pos() + workspace()->cascadeOffset(workspace()->clientArea(PlacementArea, window2)));
    QCOMPARE(window2->size(), QSize(100, 50));

    // window should be cascaded to avoid overlapping window 1 and 2
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    Window *window3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::green);
    QVERIFY(window3);
    QCOMPARE(window3->pos(), window2->pos() + workspace()->cascadeOffset(workspace()->clientArea(PlacementArea, window3)));
    QCOMPARE(window3->size(), QSize(100, 50));

    shellSurface3.reset();
    QVERIFY(Test::waitForWindowClosed(window3));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowClosed(window2));
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowClosed(window1));
}

void TestPlacement::testCascadeIfCoveringIgnoreNonCovering()
{
    // This test verifies that the cascade-if-covering adjustment doesn't take effect when the
    // other window wouldn't be fully covered.

    KConfigGroup group = kwinApp()->config()->group(QStringLiteral("Windows"));
    group.writeEntry("Placement", Placement::policyToString(PlacementCentered));
    group.sync();
    workspace()->slotReconfigure();

    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::red);
    QVERIFY(window1);

    // window should not be cascaded since it wouldn't fully overlap
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(50, 50), Qt::blue);
    QVERIFY(window2);
    QCOMPARE(window2->pos(), QPoint(615, 487));
    QCOMPARE(window2->size(), QSize(50, 50));

    shellSurface2.reset();
    QVERIFY(Test::waitForWindowClosed(window2));
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowClosed(window1));
}

void TestPlacement::testCascadeIfCoveringIgnoreOutOfArea()
{
    // This test verifies that the cascade-if-covering adjustment doesn't take effect when there is
    // not enough space on the placement area to cascade.

    KConfigGroup group = kwinApp()->config()->group(QStringLiteral("Windows"));
    group.writeEntry("Placement", Placement::policyToString(PlacementCentered));
    group.sync();
    workspace()->slotReconfigure();

    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::red);
    QVERIFY(window1);

    // window should not be cascaded since it would be out of bounds of work area
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(1280, 1024), Qt::blue);
    QVERIFY(window2);
    QCOMPARE(window2->pos(), QPoint(0, 0));
    QCOMPARE(window2->size(), QSize(1280, 1024));

    shellSurface2.reset();
    QVERIFY(Test::waitForWindowClosed(window2));
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowClosed(window1));
}

void TestPlacement::testCascadeIfCoveringIgnoreAlreadyCovered()
{
    // This test verifies that the cascade-if-covering adjustment doesn't take effect when the
    // other window is already fully covered by other windows anyway.

    KConfigGroup group = kwinApp()->config()->group(QStringLiteral("Windows"));
    group.writeEntry("Placement", Placement::policyToString(PlacementCentered));
    group.sync();
    workspace()->slotReconfigure();

    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::red);
    QVERIFY(window1);

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(1280, 1024), Qt::blue);
    QVERIFY(window2);

    // window should not be cascaded since the small window is already fully covered by the
    // large window anyway
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    Window *window3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::green);
    QVERIFY(window3);
    QCOMPARE(window3->pos(), QPoint(590, 487));
    QCOMPARE(window3->size(), QSize(100, 50));

    shellSurface3.reset();
    QVERIFY(Test::waitForWindowClosed(window3));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowClosed(window2));
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowClosed(window1));
}

void TestPlacement::testTitlebarOnScreen_data()
{
    QTest::addColumn<PlacementPolicy>("placementMode");
    QTest::addRow("PlacementRandom") << PlacementPolicy::PlacementRandom;
    QTest::addRow("PlacementSmart") << PlacementPolicy::PlacementSmart;
    QTest::addRow("PlacementCentered") << PlacementPolicy::PlacementCentered;
    QTest::addRow("PlacementZeroCornered") << PlacementPolicy::PlacementZeroCornered;
    QTest::addRow("PlacementUnderMouse") << PlacementPolicy::PlacementUnderMouse;
    QTest::addRow("PlacementMaximizing") << PlacementPolicy::PlacementMaximizing;
}

void TestPlacement::testTitlebarOnScreen()
{
    // this test verifies that windows that are bigger than the screen
    // still get placed with their title bar on the screen

    QFETCH(PlacementPolicy, placementMode);
    setPlacementPolicy(PlacementPolicy(placementMode));

    KWin::input()->pointer()->warp(QPoint(200, 0));
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), QPoint(200, 0));

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, workspace()->outputs().front()->geometry().height() + 100), Qt::red);
    QVERIFY(window);
    QCOMPARE(window->frameGeometry().y(), 0);

    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

WAYLANDTEST_MAIN(TestPlacement)
#include "placement_test.moc"
