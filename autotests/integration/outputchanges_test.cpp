/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "core/outputconfiguration.h"
#include "pointer_input.h"
#include "tiles/tilemanager.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>

using namespace std::chrono_literals;

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_output_changes-0");

class OutputChangesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testWindowSticksToOutputAfterOutputIsDisabled();
    void testWindowSticksToOutputAfterAnotherOutputIsDisabled();
    void testWindowSticksToOutputAfterOutputIsMoved();
    void testWindowSticksToOutputAfterOutputsAreSwappedLeftToRight();
    void testWindowSticksToOutputAfterOutputsAreSwappedRightToLeft();

    void testWindowRestoredAfterEnablingOutput();
    void testMaximizedWindowRestoredAfterEnablingOutput();
    void testFullScreenWindowRestoredAfterEnablingOutput();
    void testQuickTiledWindowRestoredAfterEnablingOutput();
    void testCustomTiledWindowRestoredAfterEnablingOutput_data();
    void testCustomTiledWindowRestoredAfterEnablingOutput();
    void testWindowRestoredAfterChangingScale();
    void testMaximizeStateRestoredAfterEnablingOutput_data();
    void testMaximizeStateRestoredAfterEnablingOutput();
    void testInvalidGeometryRestoreAfterEnablingOutput();
    void testMaximizedWindowDoesntDisappear_data();
    void testMaximizedWindowDoesntDisappear();

    void testWindowNotRestoredAfterMovingWindowAndEnablingOutput();
    void testLaptopLidClosed();
};

void OutputChangesTest::initTestCase()
{
    qRegisterMetaType<Window *>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void OutputChangesTest::init()
{
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });
    QVERIFY(Test::setupWaylandConnection());

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
}

void OutputChangesTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void OutputChangesTest::testWindowSticksToOutputAfterOutputIsDisabled()
{
    auto outputs = kwinApp()->outputBackend()->outputs();

    // Create a window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // Move the window to some predefined position so the test is more robust.
    window->move(QPoint(42, 67));
    QCOMPARE(window->frameGeometry(), QRect(42, 67, 100, 50));

    // Disable the output where the window is on.
    OutputConfiguration config;
    {
        auto changeSet = config.changeSet(outputs[0]);
        changeSet->enabled = false;
    }
    workspace()->applyOutputConfiguration(config);

    // The window will be sent to the second output, which is at (1280, 0).
    QCOMPARE(window->frameGeometry(), QRect(1280 + 42, 0 + 67, 100, 50));
}

void OutputChangesTest::testWindowSticksToOutputAfterAnotherOutputIsDisabled()
{
    auto outputs = kwinApp()->outputBackend()->outputs();

    // Create a window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // Move the window to the second output.
    window->move(QPoint(1280 + 42, 67));
    QCOMPARE(window->frameGeometry(), QRect(1280 + 42, 67, 100, 50));

    // Disable the first output.
    OutputConfiguration config;
    {
        auto changeSet = config.changeSet(outputs[0]);
        changeSet->enabled = false;
    }
    {
        auto changeSet = config.changeSet(outputs[1]);
        changeSet->pos = QPoint(0, 0);
    }
    workspace()->applyOutputConfiguration(config);

    // The position of the window relative to its output should remain the same.
    QCOMPARE(window->frameGeometry(), QRect(42, 67, 100, 50));
}

void OutputChangesTest::testWindowSticksToOutputAfterOutputIsMoved()
{
    auto outputs = kwinApp()->outputBackend()->outputs();

    // Create a window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // Move the window to some predefined position so the test is more robust.
    window->move(QPoint(42, 67));
    QCOMPARE(window->frameGeometry(), QRect(42, 67, 100, 50));

    // Disable the first output.
    OutputConfiguration config;
    {
        auto changeSet = config.changeSet(outputs[0]);
        changeSet->pos = QPoint(-10, 20);
    }
    workspace()->applyOutputConfiguration(config);

    // The position of the window relative to its output should remain the same.
    QCOMPARE(window->frameGeometry(), QRect(-10 + 42, 20 + 67, 100, 50));
}

void OutputChangesTest::testWindowSticksToOutputAfterOutputsAreSwappedLeftToRight()
{
    // This test verifies that a window placed on the left monitor sticks
    // to that monitor even after the monitors are swapped horizontally.

    const auto outputs = kwinApp()->outputBackend()->outputs();

    // Create a window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // Move the window to the left output.
    window->move(QPointF(0, 0));
    QCOMPARE(window->output(), outputs[0]);
    QCOMPARE(window->frameGeometry(), QRectF(0, 0, 100, 50));

    // Swap outputs.
    OutputConfiguration config;
    {
        auto changeSet1 = config.changeSet(outputs[0]);
        changeSet1->pos = QPoint(1280, 0);
        auto changeSet2 = config.changeSet(outputs[1]);
        changeSet2->pos = QPoint(0, 0);
    }
    workspace()->applyOutputConfiguration(config);

    // The window should be still on its original output.
    QCOMPARE(window->output(), outputs[0]);
    QCOMPARE(window->frameGeometry(), QRectF(1280, 0, 100, 50));
}

void OutputChangesTest::testWindowSticksToOutputAfterOutputsAreSwappedRightToLeft()
{
    // This test verifies that a window placed on the right monitor sticks
    // to that monitor even after the monitors are swapped horizontally.

    const auto outputs = kwinApp()->outputBackend()->outputs();

    // Create a window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // Move the window to the right output.
    window->move(QPointF(1280, 0));
    QCOMPARE(window->output(), outputs[1]);
    QCOMPARE(window->frameGeometry(), QRectF(1280, 0, 100, 50));

    // Swap outputs.
    OutputConfiguration config;
    {
        auto changeSet1 = config.changeSet(outputs[0]);
        changeSet1->pos = QPoint(1280, 0);
        auto changeSet2 = config.changeSet(outputs[1]);
        changeSet2->pos = QPoint(0, 0);
    }
    workspace()->applyOutputConfiguration(config);

    // The window should be still on its original output.
    QCOMPARE(window->output(), outputs[1]);
    QCOMPARE(window->frameGeometry(), QRectF(0, 0, 100, 50));
}

void OutputChangesTest::testWindowRestoredAfterEnablingOutput()
{
    // This test verifies that a window will be moved back to its original output when it's hotplugged.

    const auto outputs = kwinApp()->outputBackend()->outputs();

    // Create a window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // Move the window to the right output.
    window->move(QPointF(1280 + 50, 100));
    QCOMPARE(window->output(), outputs[1]);
    QCOMPARE(window->frameGeometry(), QRectF(1280 + 50, 100, 100, 50));

    // Disable the right output.
    OutputConfiguration config1;
    {
        auto changeSet = config1.changeSet(outputs[1]);
        changeSet->enabled = false;
    }
    workspace()->applyOutputConfiguration(config1);

    // The window will be moved to the left monitor.
    QCOMPARE(window->output(), outputs[0]);
    QCOMPARE(window->frameGeometry(), QRectF(50, 100, 100, 50));

    // Enable the right monitor.
    OutputConfiguration config2;
    {
        auto changeSet = config2.changeSet(outputs[1]);
        changeSet->enabled = true;
    }
    workspace()->applyOutputConfiguration(config2);

    // The window will be moved back to the right monitor.
    QCOMPARE(window->output(), outputs[1]);
    QCOMPARE(window->frameGeometry(), QRectF(1280 + 50, 100, 100, 50));
}

void OutputChangesTest::testWindowNotRestoredAfterMovingWindowAndEnablingOutput()
{
    // This test verifies that a window won't be moved to its original output when it's
    // hotplugged because the window was moved manually by the user.

    const auto outputs = kwinApp()->outputBackend()->outputs();

    // Create a window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // Move the window to the right output.
    window->move(QPointF(1280 + 50, 100));
    QCOMPARE(window->output(), outputs[1]);
    QCOMPARE(window->frameGeometry(), QRectF(1280 + 50, 100, 100, 50));

    // Disable the right output.
    OutputConfiguration config1;
    {
        auto changeSet = config1.changeSet(outputs[1]);
        changeSet->enabled = false;
    }
    workspace()->applyOutputConfiguration(config1);

    // The window will be moved to the left monitor.
    QCOMPARE(window->output(), outputs[0]);
    QCOMPARE(window->frameGeometry(), QRectF(50, 100, 100, 50));

    // Pretend that the user moved the window.
    workspace()->slotWindowMove();
    QVERIFY(window->isInteractiveMove());
    window->keyPressEvent(Qt::Key_Right);
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(window->frameGeometry(), QRectF(58, 100, 100, 50));

    // Enable the right monitor.
    OutputConfiguration config2;
    {
        auto changeSet = config2.changeSet(outputs[1]);
        changeSet->enabled = true;
    }
    workspace()->applyOutputConfiguration(config2);

    // The window is still on the left monitor because user manually moved it.
    QCOMPARE(window->output(), outputs[0]);
    QCOMPARE(window->frameGeometry(), QRectF(58, 100, 100, 50));
}

void OutputChangesTest::testMaximizedWindowRestoredAfterEnablingOutput()
{
    // This test verifies that a maximized window will be moved to its original
    // output when it's re-enabled.

    const auto outputs = kwinApp()->outputBackend()->outputs();

    // Create a window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // kwin will send a configure event with the actived state.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    // Move the window to the right monitor and make it maximized.
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    window->move(QPointF(1280 + 50, 100));
    window->maximize(MaximizeFull);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), QSize(1280, 1024));
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(1280, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRectF(1280, 0, 1280, 1024));
    QCOMPARE(window->moveResizeGeometry(), QRectF(1280, 0, 1280, 1024));
    QCOMPARE(window->output(), outputs[1]);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);
    QCOMPARE(window->geometryRestore(), QRectF(1280 + 50, 100, 100, 50));

    // Disable the right output.
    OutputConfiguration config1;
    {
        auto changeSet = config1.changeSet(outputs[1]);
        changeSet->enabled = false;
    }
    workspace()->applyOutputConfiguration(config1);

    // The window will be moved to the left monitor, the geometry restore will be updated too.
    QCOMPARE(window->frameGeometry(), QRectF(0, 0, 1280, 1024));
    QCOMPARE(window->moveResizeGeometry(), QRectF(0, 0, 1280, 1024));
    QCOMPARE(window->output(), outputs[0]);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);
    QCOMPARE(window->geometryRestore(), QRectF(50, 100, 100, 50));

    // Enable the right monitor.
    OutputConfiguration config2;
    {
        auto changeSet = config2.changeSet(outputs[1]);
        changeSet->enabled = true;
    }
    workspace()->applyOutputConfiguration(config2);

    // The window will be moved back to the right monitor, the geometry restore will be updated too.
    QCOMPARE(window->frameGeometry(), QRectF(1280, 0, 1280, 1024));
    QCOMPARE(window->moveResizeGeometry(), QRectF(1280, 0, 1280, 1024));
    QCOMPARE(window->output(), outputs[1]);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);
    QCOMPARE(window->geometryRestore(), QRectF(1280 + 50, 100, 100, 50));
}

void OutputChangesTest::testFullScreenWindowRestoredAfterEnablingOutput()
{
    // This test verifies that a fullscreen window will be moved to its original
    // output when it's re-enabled.

    const auto outputs = kwinApp()->outputBackend()->outputs();

    // Create a window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // kwin will send a configure event with the actived state.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    // Move the window to the right monitor and make it fullscreen.
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    window->move(QPointF(1280 + 50, 100));
    window->setFullScreen(true);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), QSize(1280, 1024));
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(1280, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRectF(1280, 0, 1280, 1024));
    QCOMPARE(window->moveResizeGeometry(), QRectF(1280, 0, 1280, 1024));
    QCOMPARE(window->output(), outputs[1]);
    QCOMPARE(window->isFullScreen(), true);
    QCOMPARE(window->isRequestedFullScreen(), true);
    QCOMPARE(window->fullscreenGeometryRestore(), QRectF(1280 + 50, 100, 100, 50));

    // Disable the right output.
    OutputConfiguration config1;
    {
        auto changeSet = config1.changeSet(outputs[1]);
        changeSet->enabled = false;
    }
    workspace()->applyOutputConfiguration(config1);

    // The window will be moved to the left monitor, the geometry restore will be updated too.
    QCOMPARE(window->frameGeometry(), QRectF(0, 0, 1280, 1024));
    QCOMPARE(window->moveResizeGeometry(), QRectF(0, 0, 1280, 1024));
    QCOMPARE(window->output(), outputs[0]);
    QCOMPARE(window->isFullScreen(), true);
    QCOMPARE(window->isRequestedFullScreen(), true);
    QCOMPARE(window->fullscreenGeometryRestore(), QRectF(50, 100, 100, 50));

    // Enable the right monitor.
    OutputConfiguration config2;
    {
        auto changeSet = config2.changeSet(outputs[1]);
        changeSet->enabled = true;
    }
    workspace()->applyOutputConfiguration(config2);

    // The window will be moved back to the right monitor, the geometry restore will be updated too.
    QCOMPARE(window->frameGeometry(), QRectF(1280, 0, 1280, 1024));
    QCOMPARE(window->moveResizeGeometry(), QRectF(1280, 0, 1280, 1024));
    QCOMPARE(window->output(), outputs[1]);
    QCOMPARE(window->isFullScreen(), true);
    QCOMPARE(window->isRequestedFullScreen(), true);
    QCOMPARE(window->fullscreenGeometryRestore(), QRectF(1280 + 50, 100, 100, 50));
}

void OutputChangesTest::testQuickTiledWindowRestoredAfterEnablingOutput()
{
    // This test verifies that a quick tiled window will be moved to
    // its original output and tile when the output is re-enabled

    const auto outputs = kwinApp()->outputBackend()->outputs();

    // Create a window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // kwin will send a configure event with the actived state.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    // Move the window to the right monitor and tile it to the right.
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    window->move(QPointF(1280 + 50, 100));
    window->setQuickTileMode(QuickTileFlag::Right, true);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), QSize(1280 / 2, 1024));
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(1280 / 2, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    const QRectF rightQuickTileGeom = QRectF(1280 + 1280 / 2, 0, 1280 / 2, 1024);
    QCOMPARE(window->frameGeometry(), rightQuickTileGeom);
    QCOMPARE(window->moveResizeGeometry(), rightQuickTileGeom);
    QCOMPARE(window->output(), outputs[1]);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->geometryRestore(), QRectF(1280 + 50, 100, 100, 50));

    // Disable the right output.
    OutputConfiguration config1;
    {
        auto changeSet = config1.changeSet(outputs[1]);
        changeSet->enabled = false;
    }
    workspace()->applyOutputConfiguration(config1);

    // The window will be moved to the left monitor
    QCOMPARE(window->output(), outputs[0]);

    // Enable the right monitor again
    OutputConfiguration config2;
    {
        auto changeSet = config2.changeSet(outputs[1]);
        changeSet->enabled = true;
    }
    workspace()->applyOutputConfiguration(config2);

    // The window will be moved back to the right monitor, and put in the correct tile
    QCOMPARE(window->frameGeometry(), rightQuickTileGeom);
    QCOMPARE(window->moveResizeGeometry(), rightQuickTileGeom);
    QCOMPARE(window->output(), outputs[1]);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->geometryRestore(), QRectF(1280 + 50, 100, 100, 50));
}

void OutputChangesTest::testCustomTiledWindowRestoredAfterEnablingOutput_data()
{
    const auto outputs = kwinApp()->outputBackend()->outputs();
    const size_t tileCount = workspace()->tileManager(outputs[1])->rootTile()->childTiles().size();

    QTest::addColumn<size_t>("tileIndex");
    for (size_t i = 0; i < tileCount; i++) {
        QTest::addRow("tile %lu", i) << i;
    }
}

void OutputChangesTest::testCustomTiledWindowRestoredAfterEnablingOutput()
{
    // This test verifies that a custom tiled window will be moved to
    // its original output and tile when the output is re-enabled

    const auto outputs = kwinApp()->outputBackend()->outputs();

    // start with only one output
    {
        OutputConfiguration config;
        auto changeSet = config.changeSet(outputs[1]);
        changeSet->enabled = false;
        workspace()->applyOutputConfiguration(config);
    }

    // Create a window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    const auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // kwin will send a configure event with the actived state.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    const QRectF originalGeometry = window->moveResizeGeometry();

    // Enable the right output
    {
        OutputConfiguration config;
        auto changeSet = config.changeSet(outputs[1]);
        changeSet->enabled = true;
        workspace()->applyOutputConfiguration(config);
    }

    QFETCH(size_t, tileIndex);
    const QRectF customTileGeom = workspace()->tileManager(outputs[1])->rootTile()->childTiles()[tileIndex]->windowGeometry();

    // Move the window to the right monitor and put it in the middle tile.
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    window->move(customTileGeom.topLeft() + QPointF(50, 50));
    const auto geomBeforeTiling = window->moveResizeGeometry();
    window->setQuickTileMode(QuickTileFlag::Custom, true);

    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), customTileGeom.size().toSize());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), customTileGeom.size().toSize(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());

    QCOMPARE(window->frameGeometry(), customTileGeom);
    QCOMPARE(window->moveResizeGeometry(), customTileGeom);
    QCOMPARE(window->output(), outputs[1]);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Custom);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Custom);
    QCOMPARE(window->geometryRestore(), geomBeforeTiling);

    // Disable the right output.
    {
        OutputConfiguration config;
        auto changeSet = config.changeSet(outputs[1]);
        changeSet->enabled = false;
        workspace()->applyOutputConfiguration(config);
    }

    // The window will be moved to the left monitor, and the original geometry restored
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), originalGeometry.size().toSize());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), originalGeometry.size().toSize(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());

    QCOMPARE(window->frameGeometry(), originalGeometry);
    QCOMPARE(window->moveResizeGeometry(), originalGeometry);
    QCOMPARE(window->output(), outputs[0]);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);

    // Enable the right monitor again
    {
        OutputConfiguration config;
        auto changeSet = config.changeSet(outputs[1]);
        changeSet->enabled = true;
        workspace()->applyOutputConfiguration(config);
    }

    // The window will be moved back to the right monitor, and put in the correct tile
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), customTileGeom.size().toSize());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), customTileGeom.size().toSize(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());

    QCOMPARE(window->frameGeometry(), customTileGeom);
    QCOMPARE(window->moveResizeGeometry(), customTileGeom);
    QCOMPARE(window->output(), outputs[1]);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Custom);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Custom);
    QCOMPARE(window->geometryRestore(), geomBeforeTiling);
}

void OutputChangesTest::testWindowRestoredAfterChangingScale()
{
    // This test verifies that a window will be moved to its original position after changing the scale of an output

    const auto output = kwinApp()->outputBackend()->outputs().front();

    // Create a window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // Move the window to the bottom right
    const QPointF originalPosition(output->geometry().width() - window->width(), output->geometry().height() - window->height());
    window->move(originalPosition);
    QCOMPARE(window->pos(), originalPosition);
    QCOMPARE(window->output(), output);

    // change the scale of the output
    OutputConfiguration config1;
    {
        auto changeSet = config1.changeSet(output);
        changeSet->scale = 2;
    }
    workspace()->applyOutputConfiguration(config1);

    // The window will be moved to still be in the monitor
    QCOMPARE(window->pos(), QPointF(output->geometry().width() - window->width(), output->geometry().height() - window->height()));
    QCOMPARE(window->output(), output);

    // Change scale back
    OutputConfiguration config2;
    {
        auto changeSet = config2.changeSet(output);
        changeSet->scale = 1;
    }
    workspace()->applyOutputConfiguration(config2);

    // The window will be moved back to where it was before
    QCOMPARE(window->pos(), originalPosition);
    QCOMPARE(window->output(), output);
}

void OutputChangesTest::testMaximizeStateRestoredAfterEnablingOutput_data()
{
    QTest::addColumn<MaximizeMode>("maximizeMode");
    QTest::addRow("Vertical Maximization") << MaximizeMode::MaximizeVertical;
    QTest::addRow("Horizontal Maximization") << MaximizeMode::MaximizeHorizontal;
    QTest::addRow("Full Maximization") << MaximizeMode::MaximizeFull;
}

void OutputChangesTest::testMaximizeStateRestoredAfterEnablingOutput()
{
    // This test verifies that the window state will get restored after disabling and enabling an output,
    // even if its maximize state changed in the process

    QFETCH(MaximizeMode, maximizeMode);

    const auto outputs = kwinApp()->outputBackend()->outputs();

    // Disable the right output
    {
        OutputConfiguration config;
        auto changeSet = config.changeSet(outputs[1]);
        changeSet->enabled = false;
        workspace()->applyOutputConfiguration(config);
    }

    // Create a window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // kwin will send a configure event with the actived state.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    const QRectF originalGeometry = window->moveResizeGeometry();

    // Enable the right output
    {
        OutputConfiguration config;
        auto changeSet = config.changeSet(outputs[1]);
        changeSet->enabled = true;
        workspace()->applyOutputConfiguration(config);
    }

    // Move the window to the right monitor and make it maximized.
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    window->move(QPointF(1280 + 50, 100));
    window->maximize(maximizeMode);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    const auto maximizedGeometry = window->moveResizeGeometry();
    QCOMPARE(window->frameGeometry(), maximizedGeometry);
    QCOMPARE(window->output(), outputs[1]);
    QCOMPARE(window->maximizeMode(), maximizeMode);
    QCOMPARE(window->requestedMaximizeMode(), maximizeMode);
    QCOMPARE(window->geometryRestore(), QRectF(1280 + 50, 100, 100, 50));

    // Disable the right output
    {
        OutputConfiguration config;
        auto changeSet = config.changeSet(outputs[1]);
        changeSet->enabled = false;
        workspace()->applyOutputConfiguration(config);
    }

    // The window will be moved to its prior position on the left monitor and unmaximized
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), originalGeometry.size().toSize());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), originalGeometry.size().toSize(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), originalGeometry);
    QCOMPARE(window->moveResizeGeometry(), originalGeometry);
    QCOMPARE(window->output(), outputs[0]);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);

    // Enable the right output again
    {
        OutputConfiguration config;
        auto changeSet = config.changeSet(outputs[1]);
        changeSet->enabled = true;
        workspace()->applyOutputConfiguration(config);
    }

    // The window will be moved back to the right monitor, maximized and the geometry restore will be updated
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), maximizedGeometry.size());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), maximizedGeometry);
    QCOMPARE(window->moveResizeGeometry(), maximizedGeometry);
    QCOMPARE(window->output(), outputs[1]);
    QCOMPARE(window->maximizeMode(), maximizeMode);
    QCOMPARE(window->requestedMaximizeMode(), maximizeMode);
    QCOMPARE(window->geometryRestore(), QRectF(1280 + 50, 100, 100, 50));
}

void OutputChangesTest::testInvalidGeometryRestoreAfterEnablingOutput()
{
    // This test verifies that the geometry restore gets restore correctly, even if it's invalid

    const auto outputs = kwinApp()->outputBackend()->outputs();

    // Disable the right output
    {
        OutputConfiguration config;
        auto changeSet = config.changeSet(outputs[1]);
        changeSet->enabled = false;
        workspace()->applyOutputConfiguration(config);
    }

    // Create a window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    shellSurface->set_maximized();
    {
        QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
        shellSurface->xdgSurface()->surface()->commit(KWayland::Client::Surface::CommitFlag::None);
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().first().toUInt());
    }
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(1280, 1024), Qt::blue);
    QVERIFY(window);
    QCOMPARE(window->maximizeMode(), MaximizeFull);

    const QRectF originalGeometry = window->moveResizeGeometry();
    const QRectF originalGeometryRestore = window->geometryRestore();

    // Enable the right output
    {
        OutputConfiguration config;
        auto changeSet = config.changeSet(outputs[1]);
        changeSet->enabled = true;
        workspace()->applyOutputConfiguration(config);
    }

    // Move the window to the right monitor
    window->sendToOutput(outputs[1]);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QCOMPARE(window->frameGeometry(), QRectF(1280, 0, 1280, 1024));
    QCOMPARE(window->moveResizeGeometry(), QRectF(1280, 0, 1280, 1024));
    QCOMPARE(window->output(), outputs[1]);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);
    QVERIFY(outputs[1]->geometry().contains(window->geometryRestore().topLeft().toPoint()));
    QCOMPARE(window->geometryRestore().size(), QSizeF(0, 0));

    const QRectF rightGeometryRestore = window->geometryRestore();

    // Disable the right output
    {
        OutputConfiguration config;
        auto changeSet = config.changeSet(outputs[1]);
        changeSet->enabled = false;
        workspace()->applyOutputConfiguration(config);
    }

    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);

    // The window will be moved to its prior position on the left monitor, and still maximized
    QCOMPARE(window->frameGeometry(), originalGeometry);
    QCOMPARE(window->moveResizeGeometry(), originalGeometry);
    QCOMPARE(window->output(), outputs[0]);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);
    QVERIFY(outputs[0]->geometry().contains(window->geometryRestore().topLeft().toPoint()));
    QCOMPARE(window->geometryRestore(), originalGeometryRestore);

    // Enable the right output again
    {
        OutputConfiguration config;
        auto changeSet = config.changeSet(outputs[1]);
        changeSet->enabled = true;
        workspace()->applyOutputConfiguration(config);
    }

    // The window will be moved back to the right monitor, maximized and the geometry restore will be updated
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), outputs[1]->geometry().size());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), outputs[1]->geometry().size(), Qt::blue);
    QCOMPARE(window->frameGeometry(), QRectF(1280, 0, 1280, 1024));
    QCOMPARE(window->moveResizeGeometry(), QRectF(1280, 0, 1280, 1024));
    QCOMPARE(window->output(), outputs[1]);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);
    QCOMPARE(window->geometryRestore(), rightGeometryRestore);
}

void OutputChangesTest::testMaximizedWindowDoesntDisappear_data()
{
    QTest::addColumn<MaximizeMode>("maximizeMode");
    QTest::addRow("Vertical Maximization") << MaximizeMode::MaximizeVertical;
    QTest::addRow("Horizontal Maximization") << MaximizeMode::MaximizeHorizontal;
    QTest::addRow("Full Maximization") << MaximizeMode::MaximizeFull;
}

void OutputChangesTest::testMaximizedWindowDoesntDisappear()
{
    // This test verifies that (vertically, horizontally) maximized windows don't get placed out of the screen
    // when the output they're on gets disabled or removed

    Test::setOutputConfig({
        Test::OutputInfo{
            .geometry = QRect(5120 / 3, 1440, 2256 / 1.3, 1504 / 1.3),
            .scale = 1.3,
            .internal = true,
        },
        Test::OutputInfo{
            .geometry = QRect(0, 0, 5120, 1440),
            .scale = 1,
            .internal = false,
        },
    });
    const auto outputs = kwinApp()->outputBackend()->outputs();
    QFETCH(MaximizeMode, maximizeMode);

    workspace()->setActiveOutput(outputs[1]);

    // Create a window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(500, 300), Qt::blue);
    QVERIFY(window);

    // kwin will send a configure event with the actived state.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    window->move(outputs[1]->geometry().topLeft() + QPoint(3500, 500));
    const QRectF originalGeometry = window->frameGeometry();
    QVERIFY(outputs[1]->geometryF().contains(originalGeometry));

    // vertically maximize the window
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    window->maximize(maximizeMode);

    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());

    QCOMPARE(window->output(), outputs[1]);
    const auto maximizedGeometry = window->moveResizeGeometry();
    QCOMPARE(window->frameGeometry(), maximizedGeometry);
    QCOMPARE(window->maximizeMode(), maximizeMode);
    QCOMPARE(window->requestedMaximizeMode(), maximizeMode);
    QCOMPARE(window->geometryRestore(), originalGeometry);

    // Disable the top output
    {
        OutputConfiguration config;
        auto changeSet0 = config.changeSet(outputs[0]);
        changeSet0->pos = QPoint(0, 0);
        auto changeSet = config.changeSet(outputs[1]);
        changeSet->enabled = false;
        workspace()->applyOutputConfiguration(config);
    }

    // The window should be moved to the left output
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());

    QCOMPARE(window->output(), outputs[0]);
    QVERIFY(outputs[0]->geometryF().contains(window->frameGeometry()));
    QVERIFY(outputs[0]->geometryF().contains(window->moveResizeGeometry()));
    QCOMPARE(window->maximizeMode(), maximizeMode);
    QCOMPARE(window->requestedMaximizeMode(), maximizeMode);
}

void OutputChangesTest::testLaptopLidClosed()
{
    Test::setOutputConfig({
        Test::OutputInfo{
            .geometry = QRect(0, 0, 1280, 1024),
            .internal = true,
        },
        Test::OutputInfo{
            .geometry = QRect(1280, 0, 1280, 1024),
            .internal = false,
        },
    });
    const auto outputs = kwinApp()->outputBackend()->outputs();
    const auto internal = outputs.front();
    QVERIFY(internal->isInternal());
    const auto external = outputs.back();
    QVERIFY(!external->isInternal());

    auto lidSwitch = std::make_unique<Test::VirtualInputDevice>();
    lidSwitch->setLidSwitch(true);
    lidSwitch->setName("virtual lid switch");
    input()->addInputDevice(lidSwitch.get());

    auto timestamp = 1ms;
    Q_EMIT lidSwitch->switchToggledOff(timestamp++, lidSwitch.get());
    QVERIFY(internal->isEnabled());
    QVERIFY(external->isEnabled());

    Q_EMIT lidSwitch->switchToggledOn(timestamp++, lidSwitch.get());
    QVERIFY(!internal->isEnabled());
    QVERIFY(external->isEnabled());

    Q_EMIT lidSwitch->switchToggledOff(timestamp++, lidSwitch.get());
    QVERIFY(internal->isEnabled());
    QVERIFY(external->isEnabled());

    input()->removeInputDevice(lidSwitch.get());
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::OutputChangesTest)
#include "outputchanges_test.moc"
