/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "platform.h"
#include "abstract_client.h"
#include "cursor.h"
#include "screenedge.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include <kwineffects.h>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/touch.h>
#include <KWayland/Client/xdgshell.h>
#include <KWaylandServer/seat_interface.h>
#include <KWaylandServer/surface_interface.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_transient_placement-0");

class TransientPlacementTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testXdgPopup_data();
    void testXdgPopup();
    void testXdgPopupWithPanel();
};

void TransientPlacementTest::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient*>();
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
    setenv("QT_QPA_PLATFORM", "wayland", true);
    waylandServer()->initWorkspace();
}

void TransientPlacementTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Decoration | Test::AdditionalWaylandInterface::PlasmaShell));

    screens()->setCurrent(0);
    Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void TransientPlacementTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void TransientPlacementTest::testXdgPopup_data()
{
    using namespace KWayland::Client;

    QTest::addColumn<QSize>("parentSize");
    QTest::addColumn<QPoint>("parentPosition");
    QTest::addColumn<XdgPositioner>("positioner");
    QTest::addColumn<QRect>("expectedGeometry");

    // window in the middle, plenty of room either side: Changing anchor

    // parent window is 500,500, starting at 300,300, anchorRect is therefore between 350->750 in both dirs
    XdgPositioner positioner(QSize(200,200), QRect(50,50, 400,400));
    positioner.setGravity(Qt::BottomEdge | Qt::RightEdge);

    positioner.setAnchorEdge(Qt::Edges());
    QTest::newRow("anchorCentre") << QSize(500, 500) << QPoint(300,300) << positioner << QRect(550, 550, 200, 200);
    positioner.setAnchorEdge(Qt::TopEdge | Qt::LeftEdge);
    QTest::newRow("anchorTopLeft") << QSize(500, 500) << QPoint(300,300) << positioner << QRect(350,350, 200, 200);
    positioner.setAnchorEdge(Qt::TopEdge);
    QTest::newRow("anchorTop") << QSize(500, 500) << QPoint(300,300) << positioner << QRect(550, 350, 200, 200);
    positioner.setAnchorEdge(Qt::TopEdge | Qt::RightEdge);
    QTest::newRow("anchorTopRight") << QSize(500, 500) << QPoint(300,300) << positioner << QRect(750, 350, 200, 200);
    positioner.setAnchorEdge(Qt::RightEdge);
    QTest::newRow("anchorRight") << QSize(500, 500) << QPoint(300,300) << positioner << QRect(750, 550, 200, 200);
    positioner.setAnchorEdge(Qt::BottomEdge | Qt::RightEdge);
    QTest::newRow("anchorBottomRight") << QSize(500,500) << QPoint(300,300) << positioner << QRect(750, 750, 200, 200);
    positioner.setAnchorEdge(Qt::BottomEdge);
    QTest::newRow("anchorBottom") << QSize(500, 500) << QPoint(300,300) << positioner << QRect(550, 750, 200, 200);
    positioner.setAnchorEdge(Qt::BottomEdge | Qt::LeftEdge);
    QTest::newRow("anchorBottomLeft") << QSize(500, 500) << QPoint(300,300) << positioner << QRect(350, 750, 200, 200);
    positioner.setAnchorEdge(Qt::LeftEdge);
    QTest::newRow("anchorLeft") << QSize(500, 500) << QPoint(300,300) << positioner << QRect(350, 550, 200, 200);

    // ----------------------------------------------------------------
    // window in the middle, plenty of room either side: Changing gravity around the bottom right anchor
    positioner.setAnchorEdge(Qt::BottomEdge | Qt::RightEdge);
    positioner.setGravity(Qt::Edges());
    QTest::newRow("gravityCentre") << QSize(500, 500) << QPoint(300, 300) << positioner << QRect(650, 650, 200, 200);
    positioner.setGravity(Qt::TopEdge | Qt::LeftEdge);
    QTest::newRow("gravityTopLeft") << QSize(500, 500) << QPoint(300, 300) << positioner << QRect(550, 550, 200, 200);
    positioner.setGravity(Qt::TopEdge);
    QTest::newRow("gravityTop") << QSize(500, 500) << QPoint(300, 300) << positioner << QRect(650, 550, 200, 200);
    positioner.setGravity(Qt::TopEdge | Qt::RightEdge);
    QTest::newRow("gravityTopRight") << QSize(500, 500) << QPoint(300, 300) << positioner << QRect(750, 550, 200, 200);
    positioner.setGravity(Qt::RightEdge);
    QTest::newRow("gravityRight") << QSize(500, 500) << QPoint(300, 300) << positioner << QRect(750, 650, 200, 200);
    positioner.setGravity(Qt::BottomEdge | Qt::RightEdge);
    QTest::newRow("gravityBottomRight") << QSize(500, 500) << QPoint(300, 300) << positioner << QRect(750, 750, 200, 200);
    positioner.setGravity(Qt::BottomEdge);
    QTest::newRow("gravityBottom") << QSize(500, 500) << QPoint(300, 300) << positioner << QRect(650, 750, 200, 200);
    positioner.setGravity(Qt::BottomEdge | Qt::LeftEdge);
    QTest::newRow("gravityBottomLeft") << QSize(500, 500) << QPoint(300, 300) << positioner << QRect(550, 750, 200, 200);
    positioner.setGravity(Qt::LeftEdge);
    QTest::newRow("gravityLeft") << QSize(500, 500) << QPoint(300, 300) << positioner << QRect(550, 650, 200, 200);

    // ----------------------------------------------------------------
    //constrain and slide
    //popup is still 200,200. window moved near edge of screen, popup always comes out towards the screen edge
    positioner.setConstraints(XdgPositioner::Constraint::SlideX | XdgPositioner::Constraint::SlideY);

    positioner.setAnchorEdge(Qt::TopEdge);
    positioner.setGravity(Qt::TopEdge);
    QTest::newRow("constraintSlideTop") << QSize(500, 500) << QPoint(80, 80) << positioner << QRect(80 + 250 - 100, 0, 200, 200);

    positioner.setAnchorEdge(Qt::LeftEdge);
    positioner.setGravity(Qt::LeftEdge);
    QTest::newRow("constraintSlideLeft") << QSize(500, 500) << QPoint(80, 80) << positioner << QRect(0, 80 + 250 - 100, 200, 200);

    positioner.setAnchorEdge(Qt::RightEdge);
    positioner.setGravity(Qt::RightEdge);
    QTest::newRow("constraintSlideRight") << QSize(500, 500) << QPoint(700, 80) << positioner << QRect(1280 - 200, 80 + 250 - 100, 200, 200);

    positioner.setAnchorEdge(Qt::BottomEdge);
    positioner.setGravity(Qt::BottomEdge);
    QTest::newRow("constraintSlideBottom") << QSize(500, 500) << QPoint(80, 500) << positioner << QRect(80 + 250 - 100, 1024 - 200, 200, 200);

    positioner.setAnchorEdge(Qt::BottomEdge | Qt::RightEdge);
    positioner.setGravity(Qt::BottomEdge| Qt::RightEdge);
    QTest::newRow("constraintSlideBottomRight") << QSize(500, 500) << QPoint(700, 1000) << positioner << QRect(1280 - 200, 1024 - 200, 200, 200);


    // ----------------------------------------------------------------
    // constrain and flip
    positioner.setConstraints(XdgPositioner::Constraint::FlipX | XdgPositioner::Constraint::FlipY);

    positioner.setAnchorEdge(Qt::TopEdge);
    positioner.setGravity(Qt::TopEdge);
    QTest::newRow("constraintFlipTop") << QSize(500, 500) << QPoint(80, 80) << positioner << QRect(230, 80 + 500 - 50, 200, 200);

    positioner.setAnchorEdge(Qt::LeftEdge);
    positioner.setGravity(Qt::LeftEdge);
    QTest::newRow("constraintFlipLeft") << QSize(500, 500) << QPoint(80, 80) << positioner << QRect(80 + 500 - 50, 230, 200, 200);

    positioner.setAnchorEdge(Qt::RightEdge);
    positioner.setGravity(Qt::RightEdge);
    QTest::newRow("constraintFlipRight") << QSize(500, 500) << QPoint(700, 80) << positioner << QRect(700 + 50 - 200, 230, 200, 200);

    positioner.setAnchorEdge(Qt::BottomEdge);
    positioner.setGravity(Qt::BottomEdge);
    QTest::newRow("constraintFlipBottom") << QSize(500, 500) << QPoint(80, 500) << positioner << QRect(230, 500 + 50 - 200, 200, 200);

    positioner.setAnchorEdge(Qt::BottomEdge | Qt::RightEdge);
    positioner.setGravity(Qt::BottomEdge| Qt::RightEdge);
    QTest::newRow("constraintFlipBottomRight") << QSize(500, 500) << QPoint(700, 500) << positioner << QRect(700 + 50 - 200, 500 + 50 - 200, 200, 200);

    positioner.setAnchorEdge(Qt::TopEdge);
    positioner.setGravity(Qt::RightEdge);
    //as popup is positioned in the middle of the parent we need a massive popup to be able to overflow
    positioner.setInitialSize(QSize(400, 400));
    QTest::newRow("constraintFlipRightNoAnchor") << QSize(500, 500) << QPoint(700, 80) << positioner << QRect(700 + 250 - 400, 330, 400, 400);

    positioner.setAnchorEdge(Qt::RightEdge);
    positioner.setGravity(Qt::TopEdge);
    positioner.setInitialSize(QSize(300, 200));
    QTest::newRow("constraintFlipRightNoGravity") << QSize(500, 500) << QPoint(700, 80) << positioner << QRect(700 + 50 - 150, 130, 300, 200);

    // ----------------------------------------------------------------
    // resize
    positioner.setConstraints(XdgPositioner::Constraint::ResizeX | XdgPositioner::Constraint::ResizeY);
    positioner.setInitialSize(QSize(200, 200));

    positioner.setAnchorEdge(Qt::TopEdge);
    positioner.setGravity(Qt::TopEdge);
    QTest::newRow("resizeTop") << QSize(500, 500) << QPoint(80, 80) << positioner << QRect(80 + 250 - 100, 0, 200, 130);

    positioner.setAnchorEdge(Qt::LeftEdge);
    positioner.setGravity(Qt::LeftEdge);
    QTest::newRow("resizeLeft") << QSize(500, 500) << QPoint(80, 80) << positioner << QRect(0, 80 + 250 - 100, 130, 200);

    positioner.setAnchorEdge(Qt::RightEdge);
    positioner.setGravity(Qt::RightEdge);
    QTest::newRow("resizeRight") << QSize(500, 500) << QPoint(700, 80) << positioner << QRect(700 + 50 + 400, 80 + 250 - 100, 130, 200);

    positioner.setAnchorEdge(Qt::BottomEdge);
    positioner.setGravity(Qt::BottomEdge);
    QTest::newRow("resizeBottom") << QSize(500, 500) << QPoint(80, 500) << positioner << QRect(80 + 250 - 100, 500 + 50 + 400, 200, 74);
}

void TransientPlacementTest::testXdgPopup()
{
    using namespace KWayland::Client;

    // this test verifies that the position of a transient window is taken from the passed position
    // there are no further constraints like window too large to fit screen, cascading transients, etc
    // some test cases also verify that the transient fits on the screen
    QFETCH(QSize, parentSize);
    QFETCH(QPoint, parentPosition);
    QFETCH(QRect, expectedGeometry);
    const QRect expectedRelativeGeometry = expectedGeometry.translated(-parentPosition);

    Surface *surface = Test::createSurface(Test::waylandCompositor());
    QVERIFY(surface);
    auto parentShellSurface = Test::createXdgShellStableSurface(surface, Test::waylandCompositor());
    QVERIFY(parentShellSurface);
    auto parent = Test::renderAndWaitForShown(surface, parentSize, Qt::blue);
    QVERIFY(parent);

    QVERIFY(!parent->isDecorated());
    parent->move(parentPosition);
    QCOMPARE(parent->frameGeometry(), QRect(parentPosition, parentSize));

    //create popup
    QFETCH(XdgPositioner, positioner);

    Surface *transientSurface = Test::createSurface(Test::waylandCompositor());
    QVERIFY(transientSurface);

    auto popup = Test::createXdgShellStablePopup(transientSurface, parentShellSurface, positioner, Test::waylandCompositor(), Test::CreationSetup::CreateOnly);
    QSignalSpy configureRequestedSpy(popup, &XdgShellPopup::configureRequested);
    transientSurface->commit(Surface::CommitFlag::None);

    configureRequestedSpy.wait();
    QCOMPARE(configureRequestedSpy.count(), 1);
    QCOMPARE(configureRequestedSpy.first()[0].value<QRect>(), expectedRelativeGeometry);
    popup->ackConfigure(configureRequestedSpy.first()[1].toUInt());

    auto transient = Test::renderAndWaitForShown(transientSurface, expectedRelativeGeometry.size(), Qt::red);
    QVERIFY(transient);

    QVERIFY(!transient->isDecorated());
    QVERIFY(transient->hasTransientPlacementHint());
    QCOMPARE(transient->frameGeometry(), expectedGeometry);

    QCOMPARE(configureRequestedSpy.count(), 1); // check that we did not get reconfigured
}

void TransientPlacementTest::testXdgPopupWithPanel()
{
    using namespace KWayland::Client;

    QScopedPointer<Surface> surface{Test::createSurface()};
    QVERIFY(!surface.isNull());
    QScopedPointer<XdgShellSurface> dockShellSurface{Test::createXdgShellStableSurface(surface.data(), surface.data())};
    QVERIFY(!dockShellSurface.isNull());
    QScopedPointer<PlasmaShellSurface> plasmaSurface(Test::waylandPlasmaShell()->createSurface(surface.data()));
    QVERIFY(!plasmaSurface.isNull());
    plasmaSurface->setRole(PlasmaShellSurface::Role::Panel);
    plasmaSurface->setPosition(QPoint(0, screens()->geometry(0).height() - 50));
    plasmaSurface->setPanelBehavior(PlasmaShellSurface::PanelBehavior::AlwaysVisible);

    // now render and map the window
    QVERIFY(workspace()->clientArea(PlacementArea, 0, 1) == workspace()->clientArea(FullScreenArea, 0, 1));
    auto dock = Test::renderAndWaitForShown(surface.data(), QSize(1280, 50), Qt::blue);
    QVERIFY(dock);
    QCOMPARE(dock->windowType(), NET::Dock);
    QVERIFY(dock->isDock());
    QCOMPARE(dock->frameGeometry(), QRect(0, screens()->geometry(0).height() - 50, 1280, 50));
    QCOMPARE(dock->hasStrut(), true);
    QVERIFY(workspace()->clientArea(PlacementArea, 0, 1) != workspace()->clientArea(FullScreenArea, 0, 1));

    //create parent
    Surface *parentSurface = Test::createSurface(Test::waylandCompositor());
    QVERIFY(parentSurface);
    auto parentShellSurface = Test::createXdgShellStableSurface(parentSurface, Test::waylandCompositor());
    QVERIFY(parentShellSurface);
    auto parent = Test::renderAndWaitForShown(parentSurface, {800, 600}, Qt::blue);
    QVERIFY(parent);

    QVERIFY(!parent->isDecorated());
    parent->move({0, screens()->geometry(0).height() - 600});
    parent->keepInArea(workspace()->clientArea(PlacementArea, parent));
    QCOMPARE(parent->frameGeometry(), QRect(0, screens()->geometry(0).height() - 600 - 50, 800, 600));

    Surface *transientSurface = Test::createSurface(Test::waylandCompositor());
    QVERIFY(transientSurface);
    XdgPositioner positioner(QSize(200,200), QRect(50,500, 200,200));

    auto transientShellSurface = Test::createXdgShellStablePopup(transientSurface, parentShellSurface, positioner, Test::waylandCompositor());
    auto transient = Test::renderAndWaitForShown(transientSurface, positioner.initialSize(), Qt::red);
    QVERIFY(transient);

    QVERIFY(!transient->isDecorated());
    QVERIFY(transient->hasTransientPlacementHint());

    QCOMPARE(transient->frameGeometry(), QRect(50, screens()->geometry(0).height() - 200 - 50, 200, 200));

    transientShellSurface->deleteLater();
    transientSurface->deleteLater();
    QVERIFY(Test::waitForWindowDestroyed(transient));

    // now parent to fullscreen - on fullscreen the panel is ignored
    QSignalSpy fullscreenSpy{parentShellSurface, &XdgShellSurface::configureRequested};
    QVERIFY(fullscreenSpy.isValid());
    parent->setFullScreen(true);
    QVERIFY(fullscreenSpy.wait());
    parentShellSurface->ackConfigure(fullscreenSpy.first().at(2).value<quint32>());
    QSignalSpy frameGeometryChangedSpy{parent, &AbstractClient::frameGeometryChanged};
    QVERIFY(frameGeometryChangedSpy.isValid());
    Test::render(parentSurface, fullscreenSpy.first().at(0).toSize(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(parent->frameGeometry(), screens()->geometry(0));
    QVERIFY(parent->isFullScreen());

    // another transient, with same hints as before from bottom of window
    transientSurface = Test::createSurface(Test::waylandCompositor());
    QVERIFY(transientSurface);

    XdgPositioner positioner2(QSize(200,200), QRect(50,screens()->geometry(0).height()-100, 200,200));
    transientShellSurface = Test::createXdgShellStablePopup(transientSurface, parentShellSurface, positioner2, Test::waylandCompositor());
    transient = Test::renderAndWaitForShown(transientSurface, positioner2.initialSize(), Qt::red);
    QVERIFY(transient);

    QVERIFY(!transient->isDecorated());
    QVERIFY(transient->hasTransientPlacementHint());

    QCOMPARE(transient->frameGeometry(), QRect(50, screens()->geometry(0).height() - 200, 200, 200));
}

}

WAYLANDTEST_MAIN(KWin::TransientPlacementTest)
#include "transient_placement.moc"
