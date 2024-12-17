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

#include <QAbstractItemModelTester>

Q_DECLARE_METATYPE(KWin::QuickTileMode)
Q_DECLARE_METATYPE(KWin::MaximizeMode)

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

private:
    void createSimpleLayout();
    void createComplexLayout();

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
    m_rootTile = workspace()->rootTile(m_output);
    QAbstractItemModelTester(m_tileManager->model(), QAbstractItemModelTester::FailureReportingMode::QtTest);

    VirtualDesktopManager::self()->setCount(3);
    VirtualDesktopManager::self()->setCurrent(1);
    createSimpleLayout();
}

void TilesTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void TilesTest::createSimpleLayout()
{
    std::vector<qreal> leftTileWidths = {0.5, 0.45, 0.4, 0.35, 0.3, 0.25};
    int i = 0;
    for (VirtualDesktop *desk : VirtualDesktopManager::self()->desktops()) {
        for (Output *out : workspace()->outputs()) {
            qreal leftTileWidth = leftTileWidths[i++];
            CustomTile *rootTile = workspace()->rootTile(out, desk);
            while (rootTile->childCount() > 0) {
                static_cast<CustomTile *>(rootTile->childTile(0))->remove();
            }

            QCOMPARE(rootTile->childCount(), 0);
            rootTile->split(CustomTile::LayoutDirection::Horizontal);
            QCOMPARE(rootTile->childCount(), 2);

            auto leftTile = qobject_cast<CustomTile *>(rootTile->childTiles().first());
            auto rightTile = qobject_cast<CustomTile *>(rootTile->childTiles().last());
            QVERIFY(leftTile);
            QVERIFY(rightTile);

            leftTile->setRelativeGeometry(QRectF(0, 0, leftTileWidth, 1));

            QCOMPARE(leftTile->relativeGeometry(), QRectF(0, 0, leftTileWidth, 1));
            QCOMPARE(rightTile->relativeGeometry(), QRectF(leftTileWidth, 0, 1 - leftTileWidth, 1));
        }
    }
}

void TilesTest::createComplexLayout()
{
    while (m_rootTile->childCount() > 0) {
        static_cast<CustomTile *>(m_rootTile->childTile(0))->remove();
    }

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
    createComplexLayout();
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
    createComplexLayout();

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
    createComplexLayout();

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
    createComplexLayout();

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
    auto rootTileD1 = m_tileManager->rootTile(VirtualDesktopManager::self()->desktops()[0]);
    auto rightTileD1 = rootTileD1->childTiles()[1];

    auto rootTileD2 = m_tileManager->rootTile(VirtualDesktopManager::self()->desktops()[1]);
    auto leftTileD2 = rootTileD2->childTiles()[0];

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> root(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::cyan);
    window->setOnAllDesktops(true);

    QSignalSpy tileChangedSpy(window, &Window::tileChanged);
    QSignalSpy surfaceConfigureRequestedSpy(root->xdgSurface(), &Test::XdgSurface::configureRequested);
    QSignalSpy toplevelConfigureRequestedSpy(root.get(), &Test::XdgToplevel::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    auto ackConfigure = [&]() {
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        root->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
        Test::render(surface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::blue);
    };

    // Add the window to a tile in desktop 1
    {
        rightTileD1->addWindow(window);
        ackConfigure();
        QVERIFY(tileChangedSpy.wait());
        QCOMPARE(window->tile(), rightTileD1);
        QCOMPARE(window->frameGeometry(), rightTileD1->windowGeometry());
    }

    // Set current Desktop 2
    {
        VirtualDesktopManager::self()->setCurrent(2);
        ackConfigure();
        QVERIFY(tileChangedSpy.wait());
        QCOMPARE(window->tile(), nullptr);
        QCOMPARE(window->frameGeometry(), QRectF(0, 0, 100, 100));
    }

    // Set a new tile for Desktop 2
    {
        leftTileD2->addWindow(window);
        ackConfigure();
        QVERIFY(tileChangedSpy.wait());
        QCOMPARE(window->tile(), leftTileD2);
        QCOMPARE(window->frameGeometry(), leftTileD2->windowGeometry());
    }

    // Go back to desktop 1, we go back to rightTileD1
    {
        VirtualDesktopManager::self()->setCurrent(1);
        ackConfigure();
        QVERIFY(tileChangedSpy.wait());
        QCOMPARE(window->tile(), rightTileD1);
        QCOMPARE(window->frameGeometry(), rightTileD1->windowGeometry());
    }

    // Switch to desktop 2
    {
        VirtualDesktopManager::self()->setCurrent(2);
        ackConfigure();
        QVERIFY(tileChangedSpy.wait());
        QCOMPARE(window->tile(), leftTileD2);
        QCOMPARE(window->frameGeometry(), leftTileD2->windowGeometry());
    }
}
}

WAYLANDTEST_MAIN(KWin::TilesTest)
#include "tiles_test.moc"
