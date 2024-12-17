/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "pointer_input.h"
#include "tiles/tilemanager.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#if KWIN_BUILD_X11
#include "x11window.h"
#include <netwm.h>
#include <xcb/xcb_icccm.h>
#endif

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
    void shortcuts();
    void testPerDesktopTiles();
    void mixQuickAndCustomTilesOnDesktops();
    void mixQuickAndCustomTilesOnDesktopsX11();

private:
    void createSampleLayout();

    Output *m_output;
    TileManager *m_tileManager;
    CustomTile *m_rootTile;
};

void TilesTest::initTestCase()
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

void TilesTest::init()
{
    QVERIFY(Test::setupWaylandConnection());

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
    m_output = workspace()->activeOutput();
    m_tileManager = workspace()->tileManager(m_output);
    m_rootTile = static_cast<RootTile *>(workspace()->rootTile(m_output));
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

    // TODO: add tests for the tile flags
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

    leftTile->addWindow(rootWindow);
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

    middleBottomTile->addWindow(rootWindow);
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

    topLeftTile->addWindow(window);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 2);

    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), topLeftTile->windowGeometry().toRect().size());
    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(4, 4, 506, 506));

    QCOMPARE(workspace()->activeWindow(), window);
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QVERIFY(interactiveMoveResizeStartedSpy.isValid());
    QSignalSpy moveResizedChangedSpy(window, &Window::moveResizedChanged);
    QVERIFY(moveResizedChangedSpy.isValid());
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);
    QVERIFY(interactiveMoveResizeFinishedSpy.isValid());

    // begin resize
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QCOMPARE(window->isInteractiveMove(), false);
    QCOMPARE(window->isInteractiveResize(), false);
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QCOMPARE(moveResizedChangedSpy.count(), 1);
    QCOMPARE(window->isInteractiveResize(), true);
    QCOMPARE(window->geometryRestore(), QRect(0, 0, 100, 100));
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 3);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    // Trigger a change.
    QPoint cursorPos = window->frameGeometry().bottomRight().toPoint();
    input()->pointer()->warp(cursorPos + QPoint(8, 0));
    window->updateInteractiveMoveResize(Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());
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
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 2);
    QCOMPARE(moveResizedChangedSpy.count(), 3);
    QCOMPARE(window->isInteractiveResize(), true);
    QCOMPARE(window->geometryRestore(), QRect(0, 0, 100, 100));
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 5);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 5);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));

    // Trigger a change.
    cursorPos = window->frameGeometry().bottomRight().toPoint();
    input()->pointer()->warp(cursorPos + QPoint(0, 8));
    window->updateInteractiveMoveResize(Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());
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

void TilesTest::shortcuts()
{
    // Our tile layout
    // | | | |
    // | |-| |
    // | | | |
    auto leftTile = qobject_cast<CustomTile *>(m_rootTile->childTiles()[0]);
    auto centerTile = qobject_cast<CustomTile *>(m_rootTile->childTiles()[1]);
    auto rightTile = qobject_cast<CustomTile *>(m_rootTile->childTiles()[2]);
    auto topCenterTile = qobject_cast<CustomTile *>(centerTile->childTiles()[0]);
    auto bottomCenterTile = qobject_cast<CustomTile *>(centerTile->childTiles()[1]);

    // Create a window, don't tile yet
    std::unique_ptr<KWayland::Client::Surface> rootSurface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> root(Test::createXdgToplevelSurface(rootSurface.get()));

    auto window = Test::renderAndWaitForShown(rootSurface.get(), QSize(100, 100), Qt::cyan);
    QVERIFY(window);

    // Trigger the shortcut, window should be tiled now
    // |w| | |
    // |w|-| |
    // |w| | |
    window->handleCustomQuickTileShortcut(QuickTileFlag::Left);
    QCOMPARE(window->requestedTile(), leftTile);
    QVERIFY(leftTile->windows().contains(window));

    // Make the window move around the grid
    // | |w| |
    // | |-| |
    // | | | |
    window->handleCustomQuickTileShortcut(QuickTileFlag::Right);
    QCOMPARE(window->requestedTile(), topCenterTile);
    QVERIFY(!leftTile->windows().contains(window));
    QVERIFY(topCenterTile->windows().contains(window));

    // | | |w|
    // | |-|w|
    // | | |w|
    window->handleCustomQuickTileShortcut(QuickTileFlag::Right);
    QCOMPARE(window->requestedTile(), rightTile);
    QVERIFY(!topCenterTile->windows().contains(window));
    QVERIFY(rightTile->windows().contains(window));

    // Right doesn't do anything now
    // | | |w|
    // | |-|w|
    // | | |w|
    window->handleCustomQuickTileShortcut(QuickTileFlag::Right);
    QCOMPARE(window->requestedTile(), rightTile);
    QVERIFY(!topCenterTile->windows().contains(window));
    QVERIFY(rightTile->windows().contains(window));

    // | |w| |
    // | |-| |
    // | | | |
    window->handleCustomQuickTileShortcut(QuickTileFlag::Left);
    QCOMPARE(window->requestedTile(), topCenterTile);
    QVERIFY(!rightTile->windows().contains(window));
    QVERIFY(topCenterTile->windows().contains(window));

    // | | | |
    // | |-| |
    // | |w| |
    window->handleCustomQuickTileShortcut(QuickTileFlag::Bottom);
    QCOMPARE(window->requestedTile(), bottomCenterTile);
    QVERIFY(!topCenterTile->windows().contains(window));
    QVERIFY(bottomCenterTile->windows().contains(window));

    // |w| | |
    // |w|-| |
    // |w| | |
    window->handleCustomQuickTileShortcut(QuickTileFlag::Left);
    QCOMPARE(window->requestedTile(), leftTile);
    QVERIFY(!bottomCenterTile->windows().contains(window));
    QVERIFY(leftTile->windows().contains(window));
}

void TilesTest::testPerDesktopTiles()
{
    VirtualDesktopManager::self()->setCount(2);
    VirtualDesktopManager::self()->setCurrent(1);

    QSignalSpy virtualDesktopChangeSpy(VirtualDesktopManager::self(), &KWin::VirtualDesktopManager::currentChanged);

    auto rootTileD1 = m_tileManager->rootTile(VirtualDesktopManager::self()->desktops()[0]);
    auto centerTileD1 = qobject_cast<CustomTile *>(rootTileD1->childTiles()[1]);
    auto bottomCenterTileD1 = qobject_cast<CustomTile *>(centerTileD1->childTiles()[1]);

    auto rootTileD2 = m_tileManager->rootTile(VirtualDesktopManager::self()->desktops()[1]);
    auto leftTileD2 = qobject_cast<CustomTile *>(rootTileD2->childTiles()[0]);

    std::unique_ptr<KWayland::Client::Surface> rootSurface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> root(Test::createXdgToplevelSurface(rootSurface.get()));

    QSignalSpy surfaceConfigureRequestedSpy(root->xdgSurface(), &Test::XdgSurface::configureRequested);
    QSignalSpy toplevelConfigureRequestedSpy(root.get(), &Test::XdgToplevel::configureRequested);

    Test::XdgToplevel::States states;
    auto window = Test::renderAndWaitForShown(rootSurface.get(), QSize(100, 100), Qt::cyan);
    // Set the window on all desktops
    window->setDesktops({});
    QVERIFY(window);
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    QSignalSpy tileChangedSpy(window, &Window::tileChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 1);
    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    // Add the window to a tile in desktop 1
    bottomCenterTileD1->addWindow(window);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 2);

    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), bottomCenterTileD1->windowGeometry().toRect().size());
    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(642, 514, 316, 506));

    // Set current Desktop 2
    VirtualDesktopManager::self()->setCurrent(2);
    QCOMPARE(virtualDesktopChangeSpy.count(), 1);

    //  The window will lose its tile and be resized
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 3);
    QCOMPARE(tileChangedSpy.count(), 1);

    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), QSize(100, 100));
    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 100, 100));

    // Set a new tile for Desktop 2
    leftTileD2->addWindow(window);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 4);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 4);
    QCOMPARE(tileChangedSpy.count(), 2);

    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), leftTileD2->windowGeometry().toRect().size());
    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(4, 4, 314, 1016));

    // Go back to desktop 1
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(virtualDesktopChangeSpy.count(), 2);

    // The window got back into bottomCenterTileD1
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 5);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 5);
    QCOMPARE(tileChangedSpy.count(), 3);
    QCOMPARE(window->requestedTile(), bottomCenterTileD1);

    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), bottomCenterTileD1->windowGeometry().toRect().size());
    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);

    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(642, 514, 316, 506));

    QCOMPARE(window->tile(), bottomCenterTileD1);
}

void TilesTest::mixQuickAndCustomTilesOnDesktops()
{
    VirtualDesktopManager::self()->setCount(3);
    VirtualDesktopManager::self()->setCurrent(1);

    QSignalSpy virtualDesktopChangeSpy(VirtualDesktopManager::self(), &KWin::VirtualDesktopManager::currentChanged);

    auto rootTileD1 = m_tileManager->rootTile(VirtualDesktopManager::self()->desktops()[0]);
    auto centerTileD1 = qobject_cast<CustomTile *>(rootTileD1->childTiles()[1]);
    auto bottomCenterTileD1 = qobject_cast<CustomTile *>(centerTileD1->childTiles()[1]);

    auto rootTileD2 = m_tileManager->rootTile(VirtualDesktopManager::self()->desktops()[1]);
    auto centerTileD2 = qobject_cast<CustomTile *>(rootTileD2->childTiles()[1]);

    auto leftQuickTileD1 = m_tileManager->quickTile(QuickTileFlag::Left);

    std::unique_ptr<KWayland::Client::Surface> rootSurface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> root(Test::createXdgToplevelSurface(rootSurface.get()));

    QSignalSpy surfaceConfigureRequestedSpy(root->xdgSurface(), &Test::XdgSurface::configureRequested);
    QSignalSpy toplevelConfigureRequestedSpy(root.get(), &Test::XdgToplevel::configureRequested);

    Test::XdgToplevel::States states;
    auto window = Test::renderAndWaitForShown(rootSurface.get(), QSize(100, 100), Qt::cyan);
    // Set the window on all desktops
    window->setDesktops({});
    QVERIFY(window);
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    QSignalSpy tileChangedSpy(window, &Window::tileChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 1);
    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    // Add the window to a quick tile in desktop 1
    leftQuickTileD1->addWindow(window);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 2);

    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), leftQuickTileD1->windowGeometry().toRect().size());
    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 640, 1024));

    // Set current Desktop 2
    VirtualDesktopManager::self()->setCurrent(2);
    QCOMPARE(virtualDesktopChangeSpy.count(), 1);

    //  The window will lose its tile and be resized
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 3);
    QCOMPARE(tileChangedSpy.count(), 1);

    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), QSize(100, 100));
    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 100, 100));

    // Set a new tile for Desktop 2, this one is a custom tile
    centerTileD2->addWindow(window);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 4);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 4);
    QCOMPARE(tileChangedSpy.count(), 2);

    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), centerTileD2->windowGeometry().toRect().size());
    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(322, 4, 636, 1016));

    // Go back to the desktop 1, we should be in the quick tile again
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(virtualDesktopChangeSpy.count(), 2);

    // The window got back into leftQuickTileD1
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 5);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 5);
    QCOMPARE(tileChangedSpy.count(), 3);
    QCOMPARE(window->requestedTile(), leftQuickTileD1);

    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), leftQuickTileD1->windowGeometry().toRect().size());
    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);

    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 640, 1024));

    QCOMPARE(window->tile(), leftQuickTileD1);

    // Go back to desktop 2, we should be back in centerTileD2
    VirtualDesktopManager::self()->setCurrent(2);
    QCOMPARE(virtualDesktopChangeSpy.count(), 3);

    // The window got back into centerTileD2
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 6);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 6);
    QCOMPARE(tileChangedSpy.count(), 4);
    QCOMPARE(window->requestedTile(), centerTileD2);

    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), centerTileD2->windowGeometry().toRect().size());
    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);

    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(322, 4, 636, 1016));

    QCOMPARE(window->tile(), centerTileD2);

    // Go to desktop 3, we should be untiled
    VirtualDesktopManager::self()->setCurrent(3);
    QCOMPARE(virtualDesktopChangeSpy.count(), 4);

    // The window got untiled
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 7);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 7);
    QCOMPARE(tileChangedSpy.count(), 5);
    QCOMPARE(window->requestedTile(), nullptr);

    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), QSize(100, 100));
    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);

    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 100, 100));

    QCOMPARE(window->tile(), nullptr);

    // Move the window to output 2 and assign a tile there
    const QList<Output *> outputs = workspace()->outputs();
    window->sendToOutput(outputs[1]);
    TileManager *out2TileMan = workspace()->tileManager(outputs[1]);

    auto leftQuickTileD1O2 = out2TileMan->quickTile(QuickTileFlag::Left);
    leftQuickTileD1O2->addWindow(window);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 8);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 8);
    QCOMPARE(tileChangedSpy.count(), 6);
    QCOMPARE(window->requestedTile(), leftQuickTileD1O2);

    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), leftQuickTileD1O2->windowGeometry().toRect().size());
    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);

    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), leftQuickTileD1O2->windowGeometry());

    QCOMPARE(window->tile(), leftQuickTileD1O2);

    // Go back to desktop 2, the window will change output and get back to centerTileD2
    VirtualDesktopManager::self()->setCurrent(2);
    QCOMPARE(virtualDesktopChangeSpy.count(), 5);

    // The window got back into centerTileD2
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 9);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 9);
    QCOMPARE(tileChangedSpy.count(), 7);
    QCOMPARE(window->requestedTile(), centerTileD2);

    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), centerTileD2->windowGeometry().toRect().size());
    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);

    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), centerTileD2->windowGeometry());

    QCOMPARE(window->tile(), centerTileD2);

    // Go back to desktop 3, the window will change output and get back to leftQuickTileD1O2
    VirtualDesktopManager::self()->setCurrent(3);
    QCOMPARE(virtualDesktopChangeSpy.count(), 6);

    // The window got back into leftQuickTileD1O2
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 10);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 10);
    QCOMPARE(tileChangedSpy.count(), 8);
    QCOMPARE(window->requestedTile(), leftQuickTileD1O2);

    root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());

    QCOMPARE(toplevelConfigureRequestedSpy.last().first().value<QSize>(), leftQuickTileD1O2->windowGeometry().toRect().size());
    Test::render(rootSurface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);

    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), leftQuickTileD1O2->windowGeometry());

    QCOMPARE(window->tile(), leftQuickTileD1O2);
}

#if KWIN_BUILD_X11
static X11Window *createWindow(xcb_connection_t *connection, const QRect &geometry, std::function<void(xcb_window_t)> setup = {})
{
    xcb_window_t windowId = xcb_generate_id(connection);
    xcb_create_window(connection, XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      geometry.x(),
                      geometry.y(),
                      geometry.width(),
                      geometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);

    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, geometry.x(), geometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, geometry.width(), geometry.height());
    xcb_icccm_set_wm_normal_hints(connection, windowId, &hints);

    if (setup) {
        setup(windowId);
    }

    xcb_map_window(connection, windowId);
    xcb_flush(connection);

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    if (!windowCreatedSpy.wait()) {
        return nullptr;
    }
    return windowCreatedSpy.last().first().value<X11Window *>();
}

void TilesTest::mixQuickAndCustomTilesOnDesktopsX11()
{
    TileManager *tileManager = workspace()->tileManager(workspace()->outputs().first());
    VirtualDesktopManager::self()->setCount(3);
    VirtualDesktopManager::self()->setCurrent(1);

    QSignalSpy virtualDesktopChangeSpy(VirtualDesktopManager::self(), &KWin::VirtualDesktopManager::currentChanged);

    auto rootTileD2 = tileManager->rootTile(VirtualDesktopManager::self()->desktops()[1]);
    auto centerTileD2 = qobject_cast<CustomTile *>(rootTileD2->childTiles()[1]);

    auto leftQuickTileD1 = tileManager->quickTile(QuickTileFlag::Left);

    std::unique_ptr<KWayland::Client::Surface> rootSurface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> root(Test::createXdgToplevelSurface(rootSurface.get()));

    // Create an xcb window.
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createWindow(c.get(), QRect(0, 0, 100, 200));
    window->setDesktops({});
    const QRectF originalGeometry = window->frameGeometry();

    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    QSignalSpy tileChangedSpy(window, &Window::tileChanged);

    // Add the window to a quick tile in desktop 1
    leftQuickTileD1->addWindow(window);
    QCOMPARE(window->requestedTile(), leftQuickTileD1);
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 640, 1024));

    // Set current Desktop 2
    VirtualDesktopManager::self()->setCurrent(2);
    QCOMPARE(virtualDesktopChangeSpy.count(), 1);
    //  The window will lose its tile and be resized
    QCOMPARE(tileChangedSpy.count(), 2);
    QCOMPARE(window->requestedTile(), nullptr);
    QCOMPARE(window->frameGeometry(), originalGeometry);

    // Set a new tile for Desktop 2, this one is a custom tile
    centerTileD2->addWindow(window);
    QCOMPARE(tileChangedSpy.count(), 3);
    QCOMPARE(window->requestedTile(), centerTileD2);
    QCOMPARE(window->frameGeometry(), QRect(322, 4, 636, 1016));

    // Go back to the desktop 1, we should be in the quick tile again
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(virtualDesktopChangeSpy.count(), 2);
    // The window got back into leftQuickTileD1
    QCOMPARE(tileChangedSpy.count(), 4);
    QCOMPARE(window->requestedTile(), leftQuickTileD1);
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 640, 1024));

    QCOMPARE(window->tile(), leftQuickTileD1);

    // Go back to desktop 2, we should be back in centerTileD2
    VirtualDesktopManager::self()->setCurrent(2);
    QCOMPARE(virtualDesktopChangeSpy.count(), 3);

    // The window got back into centerTileD2
    QCOMPARE(tileChangedSpy.count(), 5);
    QCOMPARE(window->requestedTile(), centerTileD2);
    QCOMPARE(window->frameGeometry(), QRect(322, 4, 636, 1016));
    QCOMPARE(window->tile(), centerTileD2);

    // Go to desktop 3, we should be untiled
    VirtualDesktopManager::self()->setCurrent(3);
    QCOMPARE(virtualDesktopChangeSpy.count(), 4);

    // The window got untiled
    QCOMPARE(tileChangedSpy.count(), 6);
    QCOMPARE(window->requestedTile(), nullptr);
    QCOMPARE(window->frameGeometry(), originalGeometry);
    QCOMPARE(window->tile(), nullptr);

    // Move the window to output 2 and assign a tile there
    const QList<Output *> outputs = workspace()->outputs();
    window->sendToOutput(outputs[1]);
    TileManager *out2TileMan = workspace()->tileManager(outputs[1]);

    auto leftQuickTileD1O2 = out2TileMan->quickTile(QuickTileFlag::Left);
    leftQuickTileD1O2->addWindow(window);
    QCOMPARE(tileChangedSpy.count(), 7);
    QCOMPARE(window->requestedTile(), leftQuickTileD1O2);
    QCOMPARE(window->frameGeometry(), leftQuickTileD1O2->windowGeometry());
    QCOMPARE(window->tile(), leftQuickTileD1O2);

    // Go back to desktop 2, the window will change output and get back to centerTileD2
    VirtualDesktopManager::self()->setCurrent(2);
    QCOMPARE(virtualDesktopChangeSpy.count(), 5);

    // The window got back into centerTileD2
    QCOMPARE(tileChangedSpy.count(), 8);
    QCOMPARE(window->requestedTile(), centerTileD2);
    QCOMPARE(window->frameGeometry(), centerTileD2->windowGeometry());
    QCOMPARE(window->tile(), centerTileD2);

    // Go back to desktop 3, the window will change output and get back to leftQuickTileD1O2
    VirtualDesktopManager::self()->setCurrent(3);
    QCOMPARE(virtualDesktopChangeSpy.count(), 6);

    // The window got back into leftQuickTileD1O2
    QCOMPARE(tileChangedSpy.count(), 9);
    QCOMPARE(window->requestedTile(), leftQuickTileD1O2);
    QCOMPARE(window->frameGeometry(), leftQuickTileD1O2->windowGeometry());
    QCOMPARE(window->tile(), leftQuickTileD1O2);
}
#endif
}

WAYLANDTEST_MAIN(KWin::TilesTest)
#include "tiles_test.moc"
