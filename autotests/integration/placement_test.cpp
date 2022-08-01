/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "cursor.h"
#include "output.h"
#include "platform.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_placement-0");

struct PlaceWindowResult
{
    QSizeF initiallyConfiguredSize;
    Test::XdgToplevel::States initiallyConfiguredStates;
    QRectF finalGeometry;
};

class TestPlacement : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void initTestCase();

    void testPlaceSmart();
    void testPlaceZeroCornered();
    void testPlaceMaximized();
    void testPlaceMaximizedLeavesFullscreen();
    void testPlaceCentered();
    void testPlaceUnderMouse();
    void testPlaceCascaded();
    void testPlaceRandom();
    void testFullscreen();

private:
    void setPlacementPolicy(Placement::Policy policy);
    /*
     * Create a window with the lifespan of parent and return relevant results for testing
     * defaultSize is the buffer size to use if the compositor returns an empty size in the first configure
     * event.
     */
    PlaceWindowResult createAndPlaceWindow(const QSize &defaultSize, QObject *parent);
};

void TestPlacement::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::PlasmaShell));

    workspace()->setActiveOutput(QPoint(640, 512));
    KWin::Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void TestPlacement::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestPlacement::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    Test::initWaylandWorkspace();
}

void TestPlacement::setPlacementPolicy(Placement::Policy policy)
{
    auto group = kwinApp()->config()->group("Windows");
    group.writeEntry("Placement", Placement::policyToString(policy));
    group.sync();
    Workspace::self()->slotReconfigure();
}

PlaceWindowResult TestPlacement::createAndPlaceWindow(const QSize &defaultSize, QObject *parent)
{
    PlaceWindowResult rc;

    // create a new window
    auto surface = Test::createSurface(parent);
    auto shellSurface = Test::createXdgToplevelSurface(surface, Test::CreationSetup::CreateOnly, surface);

    QSignalSpy toplevelConfigureRequestedSpy(shellSurface, &Test::XdgToplevel::configureRequested);
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

    auto window = Test::renderAndWaitForShown(surface, size.toSize(), Qt::red);

    rc.finalGeometry = window->frameGeometry();
    return rc;
}

void TestPlacement::testPlaceSmart()
{
    setPlacementPolicy(Placement::Smart);

    std::unique_ptr<QObject> testParent(new QObject); // dumb QObject just for scoping surfaces to the test

    QRegion usedArea;

    for (int i = 0; i < 4; i++) {
        PlaceWindowResult windowPlacement = createAndPlaceWindow(QSize(600, 500), testParent.get());
        // smart placement shouldn't define a size on windows
        QCOMPARE(windowPlacement.initiallyConfiguredSize, QSize(0, 0));
        QCOMPARE(windowPlacement.finalGeometry.size(), QSize(600, 500));

        // exact placement isn't a defined concept that should be tested
        // but the goal of smart placement is to make sure windows don't overlap until they need to
        // 4 windows of 600, 500 should fit without overlap
        QVERIFY(!usedArea.intersects(windowPlacement.finalGeometry.toRect()));
        usedArea += windowPlacement.finalGeometry.toRect();
    }
}

void TestPlacement::testPlaceZeroCornered()
{
    setPlacementPolicy(Placement::ZeroCornered);

    std::unique_ptr<QObject> testParent(new QObject);

    for (int i = 0; i < 4; i++) {
        PlaceWindowResult windowPlacement = createAndPlaceWindow(QSize(600, 500), testParent.get());
        // smart placement shouldn't define a size on windows
        QCOMPARE(windowPlacement.initiallyConfiguredSize, QSize(0, 0));
        // size should match our buffer
        QCOMPARE(windowPlacement.finalGeometry.size(), QSize(600, 500));
        // and it should be in the corner
        QCOMPARE(windowPlacement.finalGeometry.topLeft(), QPoint(0, 0));
    }
}

void TestPlacement::testPlaceMaximized()
{
    setPlacementPolicy(Placement::Maximizing);

    // add a top panel
    std::unique_ptr<KWayland::Client::Surface> panelSurface(Test::createSurface());
    std::unique_ptr<QObject> panelShellSurface(Test::createXdgToplevelSurface(panelSurface.get()));
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> plasmaSurface(Test::waylandPlasmaShell()->createSurface(panelSurface.get()));
    plasmaSurface->setRole(KWayland::Client::PlasmaShellSurface::Role::Panel);
    plasmaSurface->setPosition(QPoint(0, 0));
    Test::renderAndWaitForShown(panelSurface.get(), QSize(1280, 20), Qt::blue);

    std::unique_ptr<QObject> testParent(new QObject);

    // all windows should be initially maximized with an initial configure size sent
    for (int i = 0; i < 4; i++) {
        PlaceWindowResult windowPlacement = createAndPlaceWindow(QSize(600, 500), testParent.get());
        QVERIFY(windowPlacement.initiallyConfiguredStates & Test::XdgToplevel::State::Maximized);
        QCOMPARE(windowPlacement.initiallyConfiguredSize, QSize(1280, 1024 - 20));
        QCOMPARE(windowPlacement.finalGeometry, QRect(0, 20, 1280, 1024 - 20)); // under the panel
    }
}

void TestPlacement::testPlaceMaximizedLeavesFullscreen()
{
    setPlacementPolicy(Placement::Maximizing);

    // add a top panel
    std::unique_ptr<KWayland::Client::Surface> panelSurface(Test::createSurface());
    std::unique_ptr<QObject> panelShellSurface(Test::createXdgToplevelSurface(panelSurface.get()));
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> plasmaSurface(Test::waylandPlasmaShell()->createSurface(panelSurface.get()));
    plasmaSurface->setRole(KWayland::Client::PlasmaShellSurface::Role::Panel);
    plasmaSurface->setPosition(QPoint(0, 0));
    Test::renderAndWaitForShown(panelSurface.get(), QSize(1280, 20), Qt::blue);

    std::unique_ptr<QObject> testParent(new QObject);

    // all windows should be initially fullscreen with an initial configure size sent, despite the policy
    for (int i = 0; i < 4; i++) {
        auto surface = Test::createSurface(testParent.get());
        auto shellSurface = Test::createXdgToplevelSurface(surface, Test::CreationSetup::CreateOnly, surface);
        shellSurface->set_fullscreen(nullptr);
        QSignalSpy toplevelConfigureRequestedSpy(shellSurface, &Test::XdgToplevel::configureRequested);
        QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
        QVERIFY(surfaceConfigureRequestedSpy.wait());

        auto initiallyConfiguredSize = toplevelConfigureRequestedSpy[0][0].toSize();
        auto initiallyConfiguredStates = toplevelConfigureRequestedSpy[0][1].value<Test::XdgToplevel::States>();
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy[0][0].toUInt());

        auto window = Test::renderAndWaitForShown(surface, initiallyConfiguredSize, Qt::red);

        QVERIFY(initiallyConfiguredStates & Test::XdgToplevel::State::Fullscreen);
        QCOMPARE(initiallyConfiguredSize, QSize(1280, 1024));
        QCOMPARE(window->frameGeometry(), QRect(0, 0, 1280, 1024));
    }
}

void TestPlacement::testPlaceCentered()
{
    // This test verifies that Centered placement policy works.

    KConfigGroup group = kwinApp()->config()->group("Windows");
    group.writeEntry("Placement", Placement::policyToString(Placement::Centered));
    group.sync();
    workspace()->slotReconfigure();

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::red);
    QVERIFY(window);
    QCOMPARE(window->frameGeometry(), QRect(590, 487, 100, 50));

    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void TestPlacement::testPlaceUnderMouse()
{
    // This test verifies that Under Mouse placement policy works.

    KConfigGroup group = kwinApp()->config()->group("Windows");
    group.writeEntry("Placement", Placement::policyToString(Placement::UnderMouse));
    group.sync();
    workspace()->slotReconfigure();

    KWin::Cursors::self()->mouse()->setPos(QPoint(200, 300));
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), QPoint(200, 300));

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::red);
    QVERIFY(window);
    QCOMPARE(window->frameGeometry(), QRect(150, 275, 100, 50));

    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void TestPlacement::testPlaceCascaded()
{
    // This test verifies that Cascaded placement policy works.

    KConfigGroup group = kwinApp()->config()->group("Windows");
    group.writeEntry("Placement", Placement::policyToString(Placement::Cascade));
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
    QCOMPARE(window2->pos(), window1->pos() + workspace()->cascadeOffset(window2));
    QCOMPARE(window2->size(), QSize(100, 50));

    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    Window *window3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::green);
    QVERIFY(window3);
    QCOMPARE(window3->pos(), window2->pos() + workspace()->cascadeOffset(window3));
    QCOMPARE(window3->size(), QSize(100, 50));

    shellSurface3.reset();
    QVERIFY(Test::waitForWindowDestroyed(window3));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(window2));
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowDestroyed(window1));
}

void TestPlacement::testPlaceRandom()
{
    // This test verifies that Random placement policy works.

    KConfigGroup group = kwinApp()->config()->group("Windows");
    group.writeEntry("Placement", Placement::policyToString(Placement::Random));
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
    QVERIFY(Test::waitForWindowDestroyed(window3));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(window2));
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowDestroyed(window1));
}

void TestPlacement::testFullscreen()
{
    const QList<Output *> outputs = workspace()->outputs();

    setPlacementPolicy(Placement::Smart);
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

WAYLANDTEST_MAIN(TestPlacement)
#include "placement_test.moc"
