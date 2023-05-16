/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "cursor.h"
#include "tiles/tilemanager.h"
#include "wayland/seat_interface.h"
#include "wayland/surface_interface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include <kwineffects.h>

#include <QAbstractItemModelTester>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_transient_placement-0");

class TilesTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testWindowInteraction();
    void testAssignedTileDeletion();
    void resizeTileFromWindow();

private:
    void createSampleLayout();

    Output *m_output;
    TileManager *m_tileManager;
    CustomTile *m_rootTile;
};

void TilesTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
}

void TilesTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Decoration | Test::AdditionalWaylandInterface::PlasmaShell | Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandPointer());

    workspace()->setActiveOutput(QPoint(640, 512));
    Cursors::self()->mouse()->setPos(QPoint(640, 512));
    m_output = workspace()->activeOutput();
    m_tileManager = workspace()->tileManager(m_output);
    m_rootTile = m_tileManager->rootTile();
    QAbstractItemModelTester(m_tileManager->model(), QAbstractItemModelTester::FailureReportingMode::QtTest);
    while (m_rootTile->childCount() > 0) {
        static_cast<CustomTile *>(m_rootTile->childTile(0))->remove();
    }
    createSampleLayout();
}

void TilesTest::cleanup()
{
    while (m_rootTile->childCount() > 0) {
        static_cast<CustomTile *>(m_rootTile->childTile(0))->remove();
    }
    Test::destroyWaylandConnection();
}

void TilesTest::createSampleLayout()
{
    QCOMPARE(m_rootTile->childCount(), 0);
    m_rootTile->split(CustomTile::LayoutDirection::Horizontal);
    QCOMPARE(m_rootTile->childCount(), 2);

    auto leftTile = qobject_cast<CustomTile *>(m_rootTile->childTiles().first());
    auto rightTile = qobject_cast<CustomTile *>(m_rootTile->childTiles().last());
    QVERIFY(leftTile);
    QVERIFY(rightTile);

    QCOMPARE(leftTile->relativeGeometry(), QRectF(0, 0, 0.5, 1));
    QCOMPARE(rightTile->relativeGeometry(), QRectF(0.5, 0, 0.5, 1));

    // Splitting with the same layout direction creates a sibling, not 2 children
    rightTile->split(CustomTile::LayoutDirection::Horizontal);
    auto newRightTile = qobject_cast<CustomTile *>(m_rootTile->childTiles().last());

    QCOMPARE(m_rootTile->childCount(), 3);
    QCOMPARE(m_rootTile->relativeGeometry(), QRectF(0, 0, 1, 1));
    QCOMPARE(leftTile->relativeGeometry(), QRectF(0, 0, 0.5, 1));
    QCOMPARE(rightTile->relativeGeometry(), QRectF(0.5, 0, 0.25, 1));
    QCOMPARE(newRightTile->relativeGeometry(), QRectF(0.75, 0, 0.25, 1));

    QCOMPARE(m_rootTile->windowGeometry(), QRectF(4, 4, 1272, 1016));
    QCOMPARE(leftTile->windowGeometry(), QRectF(4, 4, 634, 1016));
    QCOMPARE(rightTile->windowGeometry(), QRectF(642, 4, 316, 1016));
    QCOMPARE(newRightTile->windowGeometry(), QRectF(962, 4, 314, 1016));

    // Splitting with a different layout direction creates 2 children in the tile
    QVERIFY(!rightTile->isLayout());
    QCOMPARE(rightTile->childCount(), 0);
    rightTile->split(CustomTile::LayoutDirection::Vertical);
    QVERIFY(rightTile->isLayout());
    QCOMPARE(rightTile->childCount(), 2);
    auto verticalTopTile = qobject_cast<CustomTile *>(rightTile->childTiles().first());
    auto verticalBottomTile = qobject_cast<CustomTile *>(rightTile->childTiles().last());

    // geometry of rightTile should be the same
    QCOMPARE(m_rootTile->childCount(), 3);
    QCOMPARE(rightTile->relativeGeometry(), QRectF(0.5, 0, 0.25, 1));
    QCOMPARE(rightTile->windowGeometry(), QRectF(642, 4, 316, 1016));

    QCOMPARE(verticalTopTile->relativeGeometry(), QRectF(0.5, 0, 0.25, 0.5));
    QCOMPARE(verticalBottomTile->relativeGeometry(), QRectF(0.5, 0.5, 0.25, 0.5));
    QCOMPARE(verticalTopTile->windowGeometry(), QRectF(642, 4, 316, 506));
    QCOMPARE(verticalBottomTile->windowGeometry(), QRectF(642, 514, 316, 506));
}

void TilesTest::testWindowInteraction()
{
    // Test that resizing a tile resizes the contained window and resizes the neighboring tiles as well
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());

    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));

    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);

    auto rootWindow = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::cyan);
    QVERIFY(rootWindow);
    QSignalSpy frameGeometryChangedSpy(rootWindow, &Window::frameGeometryChanged);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 1);

    auto leftTile = qobject_cast<CustomTile *>(m_rootTile->childTiles().first());
    QVERIFY(leftTile);

    rootWindow->setTile(leftTile);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 2);

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(rootWindow->frameGeometry(), leftTile->windowGeometry().toRect());

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), leftTile->windowGeometry().toRect().size());

    // Resize owning tile
    leftTile->setRelativeGeometry({0, 0, 0.4, 1});

    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 3);

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), leftTile->windowGeometry().toRect().size());

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), leftTile->windowGeometry().toRect().size());

    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(rootWindow->frameGeometry(), leftTile->windowGeometry().toRect());

    auto middleTile = qobject_cast<CustomTile *>(m_rootTile->childTiles()[1]);
    QVERIFY(middleTile);
    auto rightTile = qobject_cast<CustomTile *>(m_rootTile->childTiles()[2]);
    QVERIFY(rightTile);
    auto verticalTopTile = qobject_cast<CustomTile *>(middleTile->childTiles().first());
    QVERIFY(verticalTopTile);
    auto verticalBottomTile = qobject_cast<CustomTile *>(middleTile->childTiles().last());
    QVERIFY(verticalBottomTile);

    QCOMPARE(leftTile->relativeGeometry(), QRectF(0, 0, 0.4, 1));
    QCOMPARE(middleTile->relativeGeometry(), QRectF(0.4, 0, 0.35, 1));
    QCOMPARE(rightTile->relativeGeometry(), QRectF(0.75, 0, 0.25, 1));
    QCOMPARE(verticalTopTile->relativeGeometry(), QRectF(0.4, 0, 0.35, 0.5));
    QCOMPARE(verticalBottomTile->relativeGeometry(), QRectF(0.4, 0.5, 0.35, 0.5));
}

void TilesTest::testAssignedTileDeletion()
{
    auto leftTile = qobject_cast<CustomTile *>(m_rootTile->childTiles().first());
    QVERIFY(leftTile);
    leftTile->setRelativeGeometry({0, 0, 0.4, 1});

    std::unique_ptr<KWayland::Client::Surface> rootSurface(Test::createSurface());

    std::unique_ptr<Test::XdgToplevel> root(Test::createXdgToplevelSurface(rootSurface.get()));

    QSignalSpy surfaceConfigureRequestedSpy(root->xdgSurface(), &Test::XdgSurface::configureRequested);
    QSignalSpy toplevelConfigureRequestedSpy(root.get(), &Test::XdgToplevel::configureRequested);

    auto rootWindow = Test::renderAndWaitForShown(rootSurface.get(), QSize(100, 100), Qt::cyan);
    QVERIFY(rootWindow);
    QSignalSpy frameGeometryChangedSpy(rootWindow, &Window::frameGeometryChanged);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 1);
    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    auto middleTile = qobject_cast<CustomTile *>(m_rootTile->childTiles()[1]);
    QVERIFY(middleTile);
    auto middleBottomTile = qobject_cast<CustomTile *>(m_rootTile->childTiles()[1]->childTiles()[1]);
    QVERIFY(middleBottomTile);

    rootWindow->setTile(middleBottomTile);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 2);

    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(rootWindow->frameGeometry(), middleBottomTile->windowGeometry().toRect());

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), middleBottomTile->windowGeometry().toRect().size());

    QCOMPARE(middleBottomTile->windowGeometry().toRect(), QRect(514, 514, 444, 506));

    middleBottomTile->remove();

    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 3);

    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    // The window has been reassigned to middleTile after deletion of the children
    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), middleTile->windowGeometry().toRect().size());

    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(rootWindow->frameGeometry(), middleTile->windowGeometry().toRect());

    // Both children have been deleted as the system avoids tiles with ha single child
    QCOMPARE(middleTile->isLayout(), false);
    QCOMPARE(middleTile->childCount(), 0);
    QCOMPARE(rootWindow->tile(), middleTile);
}

void TilesTest::resizeTileFromWindow()
{
    auto middleBottomTile = qobject_cast<CustomTile *>(m_rootTile->childTiles()[1]->childTiles()[1]);
    QVERIFY(middleBottomTile);
    middleBottomTile->remove();

    std::unique_ptr<KWayland::Client::Surface> rootSurface(Test::createSurface());

    std::unique_ptr<Test::XdgToplevel> root(Test::createXdgToplevelSurface(rootSurface.get()));

    QSignalSpy surfaceConfigureRequestedSpy(root->xdgSurface(), &Test::XdgSurface::configureRequested);
    QSignalSpy toplevelConfigureRequestedSpy(root.get(), &Test::XdgToplevel::configureRequested);

    Test::XdgToplevel::States states;
    auto window = Test::renderAndWaitForShown(rootSurface.get(), QSize(100, 100), Qt::cyan);
    QVERIFY(window);
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 1);
    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    auto leftTile = qobject_cast<CustomTile *>(m_rootTile->childTiles().first());
    QVERIFY(leftTile);
    leftTile->setRelativeGeometry({0, 0, 0.4, 1});
    QCOMPARE(leftTile->windowGeometry(), QRectF(4, 4, 506, 1016));

    auto middleTile = qobject_cast<CustomTile *>(m_rootTile->childTiles()[1]);
    QVERIFY(middleTile);
    QCOMPARE(middleTile->windowGeometry(), QRectF(514, 4, 444, 1016));

    leftTile->split(CustomTile::LayoutDirection::Vertical);
    auto topLeftTile = qobject_cast<CustomTile *>(leftTile->childTiles().first());
    QVERIFY(topLeftTile);
    QCOMPARE(topLeftTile->windowGeometry(), QRectF(4, 4, 506, 506));
    QSignalSpy tileGeometryChangedSpy(topLeftTile, &Tile::windowGeometryChanged);
    auto bottomLeftTile = qobject_cast<CustomTile *>(leftTile->childTiles().last());
    QVERIFY(bottomLeftTile);
    QCOMPARE(bottomLeftTile->windowGeometry(), QRectF(4, 514, 506, 506));

    window->setTile(topLeftTile);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 2);

    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), topLeftTile->windowGeometry().toRect().size());
    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(4, 4, 506, 506));

    QCOMPARE(workspace()->activeWindow(), window);
    QSignalSpy startMoveResizedSpy(window, &Window::clientStartUserMovedResized);
    QVERIFY(startMoveResizedSpy.isValid());
    QSignalSpy moveResizedChangedSpy(window, &Window::moveResizedChanged);
    QVERIFY(moveResizedChangedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(window, &Window::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    // begin resize
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QCOMPARE(window->isInteractiveMove(), false);
    QCOMPARE(window->isInteractiveResize(), false);
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QCOMPARE(startMoveResizedSpy.count(), 1);
    QCOMPARE(moveResizedChangedSpy.count(), 1);
    QCOMPARE(window->isInteractiveResize(), true);
    QCOMPARE(window->geometryRestore(), QRect());
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 3);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    // Trigger a change.
    QPoint cursorPos = window->frameGeometry().bottomRight().toPoint();
    Cursors::self()->mouse()->setPos(cursorPos + QPoint(8, 0));
    window->updateInteractiveMoveResize(Cursors::self()->mouse()->pos());
    QCOMPARE(Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));

    // The client should receive a configure event with the new size.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 4);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 4);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(516, 508));

    // Now render new size.
    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(4, 4, 516, 508));

    QTRY_COMPARE(tileGeometryChangedSpy.count(), 2);
    QCOMPARE(window->tile(), topLeftTile);
    QCOMPARE(topLeftTile->windowGeometry(), QRect(4, 4, 516, 508));
    QCOMPARE(bottomLeftTile->windowGeometry(), QRect(4, 516, 516, 504));
    QCOMPARE(leftTile->windowGeometry(), QRect(4, 4, 516, 1016));
    QCOMPARE(middleTile->windowGeometry(), QRect(524, 4, 434, 1016));

    // Resize vertically
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QCOMPARE(startMoveResizedSpy.count(), 2);
    QCOMPARE(moveResizedChangedSpy.count(), 3);
    QCOMPARE(window->isInteractiveResize(), true);
    QCOMPARE(window->geometryRestore(), QRect());
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 5);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 5);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));

    // Trigger a change.
    cursorPos = window->frameGeometry().bottomRight().toPoint();
    Cursors::self()->mouse()->setPos(cursorPos + QPoint(0, 8));
    window->updateInteractiveMoveResize(Cursors::self()->mouse()->pos());
    QCOMPARE(Cursors::self()->mouse()->pos(), cursorPos + QPoint(0, 8));

    // The client should receive a configure event with the new size.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 6);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 6);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(518, 518));

    // Now render new size.
    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(4, 4, 518, 518));

    QTRY_COMPARE(tileGeometryChangedSpy.count(), 5);
    QCOMPARE(window->tile(), topLeftTile);
    QCOMPARE(topLeftTile->windowGeometry(), QRect(4, 4, 518, 518));
    QCOMPARE(bottomLeftTile->windowGeometry(), QRect(4, 526, 518, 494));
    QCOMPARE(leftTile->windowGeometry(), QRect(4, 4, 518, 1016));
    QCOMPARE(middleTile->windowGeometry(), QRect(526, 4, 432, 1016));
}
}

WAYLANDTEST_MAIN(KWin::TilesTest)
#include "tiles_test.moc"
