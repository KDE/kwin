/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "pointer_input.h"
#include "wayland/seat.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/touch.h>

#include <QSignalSpy>

struct PopupLayout
{
    QRect anchorRect;
    QSize size;
    quint32 anchor = 0;
    quint32 gravity = 0;
    quint32 constraint = 0;
};
Q_DECLARE_METATYPE(PopupLayout)

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
    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    kwinApp()->start();
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
}

void TransientPlacementTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::LayerShellV1));

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
}

void TransientPlacementTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void TransientPlacementTest::testXdgPopup_data()
{
    QTest::addColumn<QSize>("parentSize");
    QTest::addColumn<QPoint>("parentPosition");
    QTest::addColumn<PopupLayout>("layout");
    QTest::addColumn<QRect>("expectedGeometry");

    // parent window is 500,500, starting at 300,300, anchorRect is therefore between 350->750 in both dirs

    // ----------------------------------------------------------------
    // window in the middle, plenty of room either side: Changing anchor

    const PopupLayout layoutNone{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_top_left,
        .gravity = Test::XdgPositioner::gravity_top_left,
    };
    QTest::newRow("no constraint adjustments") << QSize(500, 500) << QPoint(0, 0) << layoutNone << QRect(50 - 200, 50 - 200, 200, 200);

    const PopupLayout layoutAnchorCenter{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_none,
        .gravity = Test::XdgPositioner::gravity_bottom_right,
    };
    QTest::newRow("anchorCenter") << QSize(500, 500) << QPoint(300, 300) << layoutAnchorCenter << QRect(550, 550, 200, 200);

    const PopupLayout layoutAnchorTopLeft{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_top_left,
        .gravity = Test::XdgPositioner::gravity_bottom_right,
    };
    QTest::newRow("anchorTopLeft") << QSize(500, 500) << QPoint(300, 300) << layoutAnchorTopLeft << QRect(350, 350, 200, 200);

    const PopupLayout layoutAnchorTop{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_top,
        .gravity = Test::XdgPositioner::gravity_bottom_right,
    };
    QTest::newRow("anchorTop") << QSize(500, 500) << QPoint(300, 300) << layoutAnchorTop << QRect(550, 350, 200, 200);

    const PopupLayout layoutAnchorTopRight{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_top_right,
        .gravity = Test::XdgPositioner::gravity_bottom_right,
    };
    QTest::newRow("anchorTopRight") << QSize(500, 500) << QPoint(300, 300) << layoutAnchorTopRight << QRect(750, 350, 200, 200);

    const PopupLayout layoutAnchorRight{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_right,
        .gravity = Test::XdgPositioner::gravity_bottom_right,
    };
    QTest::newRow("anchorRight") << QSize(500, 500) << QPoint(300, 300) << layoutAnchorRight << QRect(750, 550, 200, 200);

    const PopupLayout layoutAnchorBottomRight{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_bottom_right,
        .gravity = Test::XdgPositioner::gravity_bottom_right,
    };
    QTest::newRow("anchorBottomRight") << QSize(500, 500) << QPoint(300, 300) << layoutAnchorBottomRight << QRect(750, 750, 200, 200);

    const PopupLayout layoutAnchorBottom{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_bottom,
        .gravity = Test::XdgPositioner::gravity_bottom_right,
    };
    QTest::newRow("anchorBottom") << QSize(500, 500) << QPoint(300, 300) << layoutAnchorBottom << QRect(550, 750, 200, 200);

    const PopupLayout layoutAnchorBottomLeft{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_bottom_left,
        .gravity = Test::XdgPositioner::gravity_bottom_right,
    };
    QTest::newRow("anchorBottomLeft") << QSize(500, 500) << QPoint(300, 300) << layoutAnchorBottomLeft << QRect(350, 750, 200, 200);

    const PopupLayout layoutAnchorLeft{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_left,
        .gravity = Test::XdgPositioner::gravity_bottom_right,
    };
    QTest::newRow("anchorLeft") << QSize(500, 500) << QPoint(300, 300) << layoutAnchorLeft << QRect(350, 550, 200, 200);

    // ----------------------------------------------------------------
    // window in the middle, plenty of room either side: Changing gravity around the bottom right anchor

    const PopupLayout layoutGravityCenter{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_bottom_right,
        .gravity = Test::XdgPositioner::gravity_none,
    };
    QTest::newRow("gravityCenter") << QSize(500, 500) << QPoint(300, 300) << layoutGravityCenter << QRect(650, 650, 200, 200);

    const PopupLayout layoutGravityTopLeft{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_bottom_right,
        .gravity = Test::XdgPositioner::gravity_top_left,
    };
    QTest::newRow("gravityTopLeft") << QSize(500, 500) << QPoint(300, 300) << layoutGravityTopLeft << QRect(550, 550, 200, 200);

    const PopupLayout layoutGravityTop{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_bottom_right,
        .gravity = Test::XdgPositioner::gravity_top,
    };
    QTest::newRow("gravityTop") << QSize(500, 500) << QPoint(300, 300) << layoutGravityTop << QRect(650, 550, 200, 200);

    const PopupLayout layoutGravityTopRight{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_bottom_right,
        .gravity = Test::XdgPositioner::gravity_top_right,
    };
    QTest::newRow("gravityTopRight") << QSize(500, 500) << QPoint(300, 300) << layoutGravityTopRight << QRect(750, 550, 200, 200);

    const PopupLayout layoutGravityRight{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_bottom_right,
        .gravity = Test::XdgPositioner::gravity_right,
    };
    QTest::newRow("gravityRight") << QSize(500, 500) << QPoint(300, 300) << layoutGravityRight << QRect(750, 650, 200, 200);

    const PopupLayout layoutGravityBottomRight{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_bottom_right,
        .gravity = Test::XdgPositioner::gravity_bottom_right,
    };
    QTest::newRow("gravityBottomRight") << QSize(500, 500) << QPoint(300, 300) << layoutGravityBottomRight << QRect(750, 750, 200, 200);

    const PopupLayout layoutGravityBottom{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_bottom_right,
        .gravity = Test::XdgPositioner::gravity_bottom,
    };
    QTest::newRow("gravityBottom") << QSize(500, 500) << QPoint(300, 300) << layoutGravityBottom << QRect(650, 750, 200, 200);

    const PopupLayout layoutGravityBottomLeft{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_bottom_right,
        .gravity = Test::XdgPositioner::gravity_bottom_left,
    };
    QTest::newRow("gravityBottomLeft") << QSize(500, 500) << QPoint(300, 300) << layoutGravityBottomLeft << QRect(550, 750, 200, 200);

    const PopupLayout layoutGravityLeft{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_bottom_right,
        .gravity = Test::XdgPositioner::gravity_left,
    };
    QTest::newRow("gravityLeft") << QSize(500, 500) << QPoint(300, 300) << layoutGravityLeft << QRect(550, 650, 200, 200);

    // ----------------------------------------------------------------
    // constrain and slide
    // popup is still 200,200. window moved near edge of screen, popup always comes out towards the screen edge

    const PopupLayout layoutSlideTop{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_top,
        .gravity = Test::XdgPositioner::gravity_top,
        .constraint = Test::XdgPositioner::constraint_adjustment_slide_x | Test::XdgPositioner::constraint_adjustment_slide_y,
    };
    QTest::newRow("constraintSlideTop") << QSize(500, 500) << QPoint(80, 80) << layoutSlideTop << QRect(80 + 250 - 100, 0, 200, 200);

    const PopupLayout layoutSlideLeft{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_left,
        .gravity = Test::XdgPositioner::gravity_left,
        .constraint = Test::XdgPositioner::constraint_adjustment_slide_x | Test::XdgPositioner::constraint_adjustment_slide_y,
    };
    QTest::newRow("constraintSlideLeft") << QSize(500, 500) << QPoint(80, 80) << layoutSlideLeft << QRect(0, 80 + 250 - 100, 200, 200);

    const PopupLayout layoutSlideRight{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_right,
        .gravity = Test::XdgPositioner::gravity_right,
        .constraint = Test::XdgPositioner::constraint_adjustment_slide_x | Test::XdgPositioner::constraint_adjustment_slide_y,
    };
    QTest::newRow("constraintSlideRight") << QSize(500, 500) << QPoint(700, 80) << layoutSlideRight << QRect(1280 - 200, 80 + 250 - 100, 200, 200);

    const PopupLayout layoutSlideBottom{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_bottom,
        .gravity = Test::XdgPositioner::gravity_bottom,
        .constraint = Test::XdgPositioner::constraint_adjustment_slide_x | Test::XdgPositioner::constraint_adjustment_slide_y,
    };
    QTest::newRow("constraintSlideBottom") << QSize(500, 500) << QPoint(80, 500) << layoutSlideBottom << QRect(80 + 250 - 100, 1024 - 200, 200, 200);

    const PopupLayout layoutSlideBottomRight{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_bottom_right,
        .gravity = Test::XdgPositioner::gravity_bottom_right,
        .constraint = Test::XdgPositioner::constraint_adjustment_slide_x | Test::XdgPositioner::constraint_adjustment_slide_y,
    };
    QTest::newRow("constraintSlideBottomRight") << QSize(500, 500) << QPoint(700, 1000) << layoutSlideBottomRight << QRect(1280 - 200, 1024 - 200, 200, 200);

    // ----------------------------------------------------------------
    // constrain and flip

    const PopupLayout layoutFlipTop{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_top,
        .gravity = Test::XdgPositioner::gravity_top,
        .constraint = Test::XdgPositioner::constraint_adjustment_flip_x | Test::XdgPositioner::constraint_adjustment_flip_y,
    };
    QTest::newRow("constraintFlipTop") << QSize(500, 500) << QPoint(80, 80) << layoutFlipTop << QRect(230, 80 + 500 - 50, 200, 200);

    const PopupLayout layoutFlipLeft{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_left,
        .gravity = Test::XdgPositioner::gravity_left,
        .constraint = Test::XdgPositioner::constraint_adjustment_flip_x | Test::XdgPositioner::constraint_adjustment_flip_y,
    };
    QTest::newRow("constraintFlipLeft") << QSize(500, 500) << QPoint(80, 80) << layoutFlipLeft << QRect(80 + 500 - 50, 230, 200, 200);

    const PopupLayout layoutFlipRight{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_right,
        .gravity = Test::XdgPositioner::gravity_right,
        .constraint = Test::XdgPositioner::constraint_adjustment_flip_x | Test::XdgPositioner::constraint_adjustment_flip_y,
    };
    QTest::newRow("constraintFlipRight") << QSize(500, 500) << QPoint(700, 80) << layoutFlipRight << QRect(700 + 50 - 200, 230, 200, 200);

    const PopupLayout layoutFlipBottom{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_bottom,
        .gravity = Test::XdgPositioner::gravity_bottom,
        .constraint = Test::XdgPositioner::constraint_adjustment_flip_x | Test::XdgPositioner::constraint_adjustment_flip_y,
    };
    QTest::newRow("constraintFlipBottom") << QSize(500, 500) << QPoint(80, 500) << layoutFlipBottom << QRect(230, 500 + 50 - 200, 200, 200);

    const PopupLayout layoutFlipBottomRight{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_bottom_right,
        .gravity = Test::XdgPositioner::gravity_bottom_right,
        .constraint = Test::XdgPositioner::constraint_adjustment_flip_x | Test::XdgPositioner::constraint_adjustment_flip_y,
    };
    QTest::newRow("constraintFlipBottomRight") << QSize(500, 500) << QPoint(700, 500) << layoutFlipBottomRight << QRect(700 + 50 - 200, 500 + 50 - 200, 200, 200);

    const PopupLayout layoutFlipRightNoAnchor{
        .anchorRect = QRect(50, 50, 400, 400),
        // as popup is positioned in the middle of the parent we need a massive popup to be able to overflow
        .size = QSize(400, 400),
        .anchor = Test::XdgPositioner::anchor_top,
        .gravity = Test::XdgPositioner::gravity_right,
        .constraint = Test::XdgPositioner::constraint_adjustment_flip_x | Test::XdgPositioner::constraint_adjustment_flip_y,
    };
    QTest::newRow("constraintFlipRightNoAnchor") << QSize(500, 500) << QPoint(700, 80) << layoutFlipRightNoAnchor << QRect(700 + 250 - 400, 330, 400, 400);

    const PopupLayout layoutFlipRightNoGravity{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(300, 200),
        .anchor = Test::XdgPositioner::anchor_right,
        .gravity = Test::XdgPositioner::gravity_top,
        .constraint = Test::XdgPositioner::constraint_adjustment_flip_x | Test::XdgPositioner::constraint_adjustment_flip_y,
    };
    QTest::newRow("constraintFlipRightNoGravity") << QSize(500, 500) << QPoint(700, 80) << layoutFlipRightNoGravity << QRect(700 + 50 - 150, 130, 300, 200);

    // ----------------------------------------------------------------
    // resize

    const PopupLayout layoutResizeTop{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_top,
        .gravity = Test::XdgPositioner::gravity_top,
        .constraint = Test::XdgPositioner::constraint_adjustment_resize_x | Test::XdgPositioner::constraint_adjustment_resize_y,
    };
    QTest::newRow("resizeTop") << QSize(500, 500) << QPoint(80, 80) << layoutResizeTop << QRect(80 + 250 - 100, 0, 200, 130);

    const PopupLayout layoutResizeLeft{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_left,
        .gravity = Test::XdgPositioner::gravity_left,
        .constraint = Test::XdgPositioner::constraint_adjustment_resize_x | Test::XdgPositioner::constraint_adjustment_resize_y,
    };
    QTest::newRow("resizeLeft") << QSize(500, 500) << QPoint(80, 80) << layoutResizeLeft << QRect(0, 80 + 250 - 100, 130, 200);

    const PopupLayout layoutResizeRight{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_right,
        .gravity = Test::XdgPositioner::gravity_right,
        .constraint = Test::XdgPositioner::constraint_adjustment_resize_x | Test::XdgPositioner::constraint_adjustment_resize_y,
    };
    QTest::newRow("resizeRight") << QSize(500, 500) << QPoint(700, 80) << layoutResizeRight << QRect(700 + 50 + 400, 80 + 250 - 100, 130, 200);

    const PopupLayout layoutResizeBottom{
        .anchorRect = QRect(50, 50, 400, 400),
        .size = QSize(200, 200),
        .anchor = Test::XdgPositioner::anchor_bottom,
        .gravity = Test::XdgPositioner::gravity_bottom,
        .constraint = Test::XdgPositioner::constraint_adjustment_resize_x | Test::XdgPositioner::constraint_adjustment_resize_y,
    };
    QTest::newRow("resizeBottom") << QSize(500, 500) << QPoint(80, 500) << layoutResizeBottom << QRect(80 + 250 - 100, 500 + 50 + 400, 200, 74);
}

void TransientPlacementTest::testXdgPopup()
{
    // this test verifies that the position of a transient window is taken from the passed position
    // there are no further constraints like window too large to fit screen, cascading transients, etc
    // some test cases also verify that the transient fits on the screen
    QFETCH(QSize, parentSize);
    QFETCH(QPoint, parentPosition);
    QFETCH(QRect, expectedGeometry);
    const QRect expectedRelativeGeometry = expectedGeometry.translated(-parentPosition);

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    std::unique_ptr<Test::XdgToplevel> parentShellSurface = Test::createXdgToplevelSurface(surface.get());
    QVERIFY(parentShellSurface);
    auto parent = Test::renderAndWaitForShown(surface.get(), parentSize, Qt::blue);
    QVERIFY(parent);

    QVERIFY(!parent->isDecorated());
    parent->move(parentPosition);
    QCOMPARE(parent->frameGeometry(), QRect(parentPosition, parentSize));

    // create popup
    QFETCH(PopupLayout, layout);

    std::unique_ptr<KWayland::Client::Surface> transientSurface = Test::createSurface();
    QVERIFY(transientSurface);

    std::unique_ptr<Test::XdgPositioner> positioner(Test::createXdgPositioner());
    positioner->set_anchor_rect(layout.anchorRect.x(), layout.anchorRect.y(), layout.anchorRect.width(), layout.anchorRect.height());
    positioner->set_size(layout.size.width(), layout.size.height());
    positioner->set_anchor(layout.anchor);
    positioner->set_gravity(layout.gravity);
    positioner->set_constraint_adjustment(layout.constraint);
    std::unique_ptr<Test::XdgPopup> popup(Test::createXdgPopupSurface(transientSurface.get(), parentShellSurface->xdgSurface(), positioner.get(), Test::CreationSetup::CreateOnly));
    QSignalSpy popupConfigureRequestedSpy(popup.get(), &Test::XdgPopup::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(popup->xdgSurface(), &Test::XdgSurface::configureRequested);
    transientSurface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);
    QCOMPARE(popupConfigureRequestedSpy.last()[0].value<QRect>(), expectedRelativeGeometry);
    popup->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last()[0].toUInt());

    auto transient = Test::renderAndWaitForShown(transientSurface.get(), expectedRelativeGeometry.size(), Qt::red);
    QVERIFY(transient);

    QVERIFY(!transient->isDecorated());
    QVERIFY(transient->hasTransientPlacementHint());
    QCOMPARE(transient->frameGeometry(), expectedGeometry);

    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1); // check that we did not get reconfigured
}

void TransientPlacementTest::testXdgPopupWithPanel()
{
    const Output *output = workspace()->activeOutput();

    std::unique_ptr<KWayland::Client::Surface> dockSurface{Test::createSurface()};
    std::unique_ptr<Test::LayerSurfaceV1> dockShellSurface{Test::createLayerSurfaceV1(dockSurface.get(), QStringLiteral("dock"))};
    dockShellSurface->set_size(1280, 50);
    dockShellSurface->set_anchor(Test::LayerSurfaceV1::anchor_bottom);
    dockShellSurface->set_exclusive_zone(50);
    dockSurface->commit(KWayland::Client::Surface::CommitFlag::None);

    // now render and map the window
    QSignalSpy dockConfigureRequestedSpy(dockShellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(dockConfigureRequestedSpy.wait());
    auto dock = Test::renderAndWaitForShown(dockSurface.get(), dockConfigureRequestedSpy.last().at(1).toSize(), Qt::blue);
    QVERIFY(dock);
    QCOMPARE(dock->windowType(), WindowType::Dock);
    QVERIFY(dock->isDock());
    QCOMPARE(dock->frameGeometry(), QRect(0, output->geometry().height() - 50, 1280, 50));
    QCOMPARE(dock->hasStrut(), true);
    QCOMPARE(workspace()->clientArea(PlacementArea, dock), QRect(0, 0, 1280, 1024 - 50));
    QCOMPARE(workspace()->clientArea(FullScreenArea, dock), QRect(0, 0, 1280, 1024));

    // create parent
    std::unique_ptr<KWayland::Client::Surface> parentSurface(Test::createSurface());
    QVERIFY(parentSurface);
    auto parentShellSurface = Test::createXdgToplevelSurface(parentSurface.get());
    QVERIFY(parentShellSurface);
    auto parent = Test::renderAndWaitForShown(parentSurface.get(), {800, 600}, Qt::blue);
    QVERIFY(parent);

    QVERIFY(!parent->isDecorated());
    parent->move(QPointF(0, output->geometry().height() - 600));
    parent->moveResize(parent->keepInArea(parent->moveResizeGeometry(), workspace()->clientArea(PlacementArea, parent)));
    QCOMPARE(parent->frameGeometry(), QRect(0, output->geometry().height() - 600 - 50, 800, 600));

    std::unique_ptr<KWayland::Client::Surface> transientSurface(Test::createSurface());
    QVERIFY(transientSurface);

    std::unique_ptr<Test::XdgPositioner> positioner(Test::createXdgPositioner());
    positioner->set_size(200, 200);
    positioner->set_anchor_rect(50, 500, 200, 200);
    positioner->set_constraint_adjustment(Test::XdgPositioner::constraint_adjustment_slide_x | Test::XdgPositioner::constraint_adjustment_slide_y);

    std::unique_ptr<Test::XdgPopup> transientShellSurface(Test::createXdgPopupSurface(transientSurface.get(), parentShellSurface->xdgSurface(), positioner.get()));
    auto transient = Test::renderAndWaitForShown(transientSurface.get(), QSize(200, 200), Qt::red);
    QVERIFY(transient);

    QVERIFY(!transient->isDecorated());
    QVERIFY(transient->hasTransientPlacementHint());

    QCOMPARE(transient->frameGeometry(), QRect(50, output->geometry().height() - 200 - 50, 200, 200));

    transientShellSurface.reset();
    transientSurface.reset();
    QVERIFY(Test::waitForWindowClosed(transient));

    // now parent to fullscreen - on fullscreen the panel is ignored
    QSignalSpy toplevelConfigureRequestedSpy(parentShellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(parentShellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    parent->setFullScreen(true);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    parentShellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    QSignalSpy frameGeometryChangedSpy{parent, &Window::frameGeometryChanged};
    Test::render(parentSurface.get(), toplevelConfigureRequestedSpy.last().at(0).toSize(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(parent->frameGeometry(), output->geometry());
    QVERIFY(parent->isFullScreen());

    // another transient, with same hints as before from bottom of window
    transientSurface = Test::createSurface();
    QVERIFY(transientSurface);

    const QRect anchorRect2(50, output->geometry().height() - 100, 200, 200);
    std::unique_ptr<Test::XdgPositioner> positioner2(Test::createXdgPositioner());
    positioner2->set_size(200, 200);
    positioner2->set_anchor_rect(anchorRect2.x(), anchorRect2.y(), anchorRect2.width(), anchorRect2.height());
    positioner2->set_constraint_adjustment(Test::XdgPositioner::constraint_adjustment_slide_x | Test::XdgPositioner::constraint_adjustment_slide_y);
    transientShellSurface = Test::createXdgPopupSurface(transientSurface.get(), parentShellSurface->xdgSurface(), positioner2.get());
    transient = Test::renderAndWaitForShown(transientSurface.get(), QSize(200, 200), Qt::red);
    QVERIFY(transient);

    QVERIFY(!transient->isDecorated());
    QVERIFY(transient->hasTransientPlacementHint());

    QCOMPARE(transient->frameGeometry(), QRect(50, output->geometry().height() - 200, 200, 200));
}

}

WAYLANDTEST_MAIN(KWin::TransientPlacementTest)
#include "transient_placement.moc"
