/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "core/outputconfiguration.h"
#include "outputconfigurationstore.h"
#include "pointer_input.h"
#include "tiles/tilemanager.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include "x11window.h"

#include <KWayland/Client/surface.h>
#include <QOrientationSensor>
#include <netwm.h>
#include <xcb/xcb_icccm.h>

using namespace std::chrono_literals;

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_output_changes-0");

enum class DeviceType {
    Desktop,
    Laptop,
    Phone,
};

class LidSwitch : public InputDevice
{
    Q_OBJECT

public:
    explicit LidSwitch(QObject *parent = nullptr)
        : InputDevice(parent)
    {
    }

    QString name() const override
    {
        return QStringLiteral("lid switch");
    }

    bool isEnabled() const override
    {
        return true;
    }
    void setEnabled(bool enabled) override
    {
    }

    bool isKeyboard() const override
    {
        return false;
    }
    bool isPointer() const override
    {
        return false;
    }
    bool isTouchpad() const override
    {
        return false;
    }
    bool isTouch() const override
    {
        return false;
    }
    bool isTabletTool() const override
    {
        return false;
    }
    bool isTabletPad() const override
    {
        return false;
    }
    bool isTabletModeSwitch() const override
    {
        return false;
    }
    bool isLidSwitch() const override
    {
        return true;
    }
};

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
    void testQuickTileUntileWindowRestoredAfterEnablingOutput();
    void testCustomTiledWindowRestoredAfterEnablingOutput_data();
    void testCustomTiledWindowRestoredAfterEnablingOutput();
    void testWindowRestoredAfterChangingScale();
    void testMaximizeStateRestoredAfterEnablingOutput_data();
    void testMaximizeStateRestoredAfterEnablingOutput();
    void testInvalidGeometryRestoreAfterEnablingOutput();
    void testMaximizedWindowDoesntDisappear_data();
    void testMaximizedWindowDoesntDisappear();
    void testXwaylandScaleChange();

    void testWindowNotRestoredAfterMovingWindowAndEnablingOutput();
    void testLaptopLidClosed();
    void testGenerateConfigs_data();
    void testGenerateConfigs();
    void testAutorotate_data();
    void testAutorotate();
    void testSettingRestoration_data();
    void testSettingRestoration();
    void testSettingRestoration_initialParsingFailure();
};

void OutputChangesTest::initTestCase()
{
    qRegisterMetaType<Window *>();

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
    QSignalSpy quickTileChangedSpy(window, &Window::quickTileModeChanged);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    // Move the window to the right monitor and tile it to the right.
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    window->move(QPointF(1280 + 50, 100));
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::Right);
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
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(1280 / 2, 1024), Qt::blue);
    QVERIFY(quickTileChangedSpy.wait());

    // The window will be moved back to the right monitor, and put in the correct tile
    QCOMPARE(window->frameGeometry(), rightQuickTileGeom);
    QCOMPARE(window->moveResizeGeometry(), rightQuickTileGeom);
    QCOMPARE(window->output(), outputs[1]);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->geometryRestore(), QRectF(1280 + 50, 100, 100, 50));
}

void OutputChangesTest::testQuickTileUntileWindowRestoredAfterEnablingOutput()
{
    // This test verifies that a quick tiled window will be moved to
    // its original output and tile when the output is re-enabled
    // even if the window changed quick tile state
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
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // kwin will send a configure event with the current state.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), QSize(100, 50));
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(100, 50), Qt::blue);

    const QRectF originalGeometry = window->frameGeometry();

    // add a second output
    {
        OutputConfiguration config;
        auto changeSet = config.changeSet(outputs[1]);
        changeSet->enabled = true;
        workspace()->applyOutputConfiguration(config);
    }

    // Move the window to the second output and tile it to the right.
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    window->move(QPointF(1280 + 50, 100));
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::Right);
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

    // remove the second output again
    {
        OutputConfiguration config;
        auto changeSet = config.changeSet(outputs[1]);
        changeSet->enabled = false;
        workspace()->applyOutputConfiguration(config);
    }

    // the window should now be untiled, and put back in its original position
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), QSize(100, 50));
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), originalGeometry);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);

    // add the second output again
    {
        OutputConfiguration config;
        auto changeSet = config.changeSet(outputs[1]);
        changeSet->enabled = true;
        workspace()->applyOutputConfiguration(config);
    }

    // the window should now be put back into the tile on the second output
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), QSize(1280 / 2, 1024));
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(1280 / 2, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
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
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::Custom);

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
    Q_EMIT lidSwitch->switchToggle(SwitchState::Off, timestamp++, lidSwitch.get());
    QVERIFY(internal->isEnabled());
    QVERIFY(external->isEnabled());

    Q_EMIT lidSwitch->switchToggle(SwitchState::On, timestamp++, lidSwitch.get());
    QVERIFY(!internal->isEnabled());
    QVERIFY(external->isEnabled());

    Q_EMIT lidSwitch->switchToggle(SwitchState::Off, timestamp++, lidSwitch.get());
    QVERIFY(internal->isEnabled());
    QVERIFY(external->isEnabled());

    input()->removeInputDevice(lidSwitch.get());
}

static X11Window *createX11Window(xcb_connection_t *connection, const QRect &geometry, std::function<void(xcb_window_t)> setup = {})
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

void OutputChangesTest::testXwaylandScaleChange()
{
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });
    const auto outputs = workspace()->outputs();

    {
        OutputConfiguration config;
        config.changeSet(outputs[0])->scale = 2;
        config.changeSet(outputs[1])->scale = 1;
        workspace()->applyOutputConfiguration(config);
    }
    QCOMPARE(kwinApp()->xwaylandScale(), 2);

    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    X11Window *window = createX11Window(c.get(), QRect(0, 0, 100, 200));
    const QRectF originalGeometry = window->frameGeometry();

    // disable the left output -> window gets moved to the right output
    {
        OutputConfiguration config;
        config.changeSet(outputs[0])->enabled = false;
        workspace()->applyOutputConfiguration(config);
    }

    // the window should still have logical size of 100, 200
    QCOMPARE(kwinApp()->xwaylandScale(), 1);
    QCOMPARE(window->frameGeometry().size(), originalGeometry.size());

    // enable the left output again
    {
        OutputConfiguration config;
        config.changeSet(outputs[0])->enabled = true;
        workspace()->applyOutputConfiguration(config);
    }

    // the window should be back in its original geometry
    QCOMPARE(kwinApp()->xwaylandScale(), 2);
    QCOMPARE(window->frameGeometry(), originalGeometry);
}

using ModeInfo = std::tuple<QSize, uint64_t, OutputMode::Flags>;

void OutputChangesTest::testGenerateConfigs_data()
{
    QTest::addColumn<DeviceType>("deviceType");
    QTest::addColumn<Test::OutputInfo>("outputInfo");
    QTest::addColumn<std::tuple<QSize, uint64_t, OutputMode::Flags>>("defaultMode");
    QTest::addColumn<double>("defaultScale");

    QTest::addRow("1080p 27\"")
        << DeviceType::Desktop
        << Test::OutputInfo{
               .geometry = QRect(0, 0, 1920, 1080),
               .internal = false,
               .physicalSizeInMM = QSize(598, 336),
               .modes = {ModeInfo(QSize(1920, 1080), 60000, OutputMode::Flag::Preferred)},
           }
        << ModeInfo(QSize(1920, 1080), 60000ul, OutputMode::Flag::Preferred) << 1.0;

    QTest::addRow("2160p 27\"")
        << DeviceType::Desktop
        << Test::OutputInfo{
               .geometry = QRect(0, 0, 3840, 2160),
               .internal = false,
               .physicalSizeInMM = QSize(598, 336),
               .modes = {ModeInfo(QSize(3840, 2160), 60000, OutputMode::Flag::Preferred)},
           }
        << ModeInfo(QSize(3840, 2160), 60000ul, OutputMode::Flag::Preferred) << 1.70;

    QTest::addRow("2160p invalid size")
        << DeviceType::Desktop
        << Test::OutputInfo{
               .geometry = QRect(0, 0, 3840, 2160),
               .internal = false,
               .physicalSizeInMM = QSize(),
               .modes = {ModeInfo(QSize(3840, 2160), 60000, OutputMode::Flag::Preferred)},
           }
        << ModeInfo(QSize(3840, 2160), 60000ul, OutputMode::Flag::Preferred) << 1.0;

    QTest::addRow("2160p impossibly tiny size")
        << DeviceType::Desktop
        << Test::OutputInfo{
               .geometry = QRect(0, 0, 3840, 2160),
               .internal = false,
               .physicalSizeInMM = QSize(1, 1),
               .modes = {ModeInfo(QSize(3840, 2160), 60000, OutputMode::Flag::Preferred)},
           }
        << ModeInfo(QSize(3840, 2160), 60000ul, OutputMode::Flag::Preferred) << 1.0;

    QTest::addRow("1080p 27\" with non-preferred high refresh option")
        << DeviceType::Desktop
        << Test::OutputInfo{
               .geometry = QRect(0, 0, 1920, 1080),
               .internal = false,
               .physicalSizeInMM = QSize(598, 336),
               .modes = {ModeInfo(QSize(1920, 1080), 60000, OutputMode::Flag::Preferred), ModeInfo(QSize(1920, 1080), 120000, OutputMode::Flags{})},
           }
        << ModeInfo(QSize(1920, 1080), 120000ul, OutputMode::Flags{}) << 1.0;

    QTest::addRow("2160p 27\" with 30Hz preferred mode")
        << DeviceType::Desktop
        << Test::OutputInfo{
               .geometry = QRect(0, 0, 3840, 2160),
               .internal = false,
               .physicalSizeInMM = QSize(598, 336),
               .modes = {ModeInfo(QSize(3840, 2160), 30000, OutputMode::Flag::Preferred), ModeInfo(QSize(2560, 1440), 60000, OutputMode::Flags{})},
           }
        << ModeInfo(QSize(2560, 1440), 60000ul, OutputMode::Flags{}) << 1.15;

    QTest::addRow("2160p 27\" with 30Hz preferred and a generated 60Hz mode")
        << DeviceType::Desktop
        << Test::OutputInfo{
               .geometry = QRect(0, 0, 3840, 2160),
               .internal = false,
               .physicalSizeInMM = QSize(598, 336),
               .modes = {ModeInfo(QSize(3840, 2160), 30000, OutputMode::Flag::Preferred), ModeInfo(QSize(2560, 1440), 60000, OutputMode::Flag::Generated)},
           }
        << ModeInfo(QSize(3840, 2160), 30000ul, OutputMode::Flag::Preferred) << 1.70;

    QTest::addRow("1440p 32:9 49\" with two preferred modes")
        << DeviceType::Desktop
        << Test::OutputInfo{
               .geometry = QRect(0, 0, 5120, 1440),
               .internal = false,
               .physicalSizeInMM = QSize(1190, 340),
               .modes = {ModeInfo(QSize(3840, 1080), 120000, OutputMode::Flag::Preferred), ModeInfo(QSize(5120, 1440), 120000, OutputMode::Flag::Preferred)},
           }
        << ModeInfo(QSize(5120, 1440), 120000ul, OutputMode::Flag::Preferred) << 1.10;

    QTest::addRow("2160p 32:9 57\" with non-native preferred mode")
        << DeviceType::Desktop
        << Test::OutputInfo{
               .geometry = QRect(0, 0, 7680, 2160),
               .internal = false,
               .physicalSizeInMM = QSize(1400, 400),
               .modes = {ModeInfo(QSize(3840, 1080), 60000, OutputMode::Flag::Preferred), ModeInfo(QSize(7680, 2160), 120000, OutputMode::Flags{})},
           }
        << ModeInfo(QSize(7680, 2160), 120000ul, OutputMode::Flags{}) << 1.45;

    QTest::addRow("Framework 1920p 13.5\"")
        << DeviceType::Laptop
        << Test::OutputInfo{
               .geometry = QRect(0, 0, 2880, 1920),
               .internal = true,
               .physicalSizeInMM = QSize(285, 190),
               .modes = {ModeInfo(QSize(2880, 1920), 120000, OutputMode::Flag::Preferred)},
           }
        << ModeInfo(QSize(2880, 1920), 120000, OutputMode::Flag::Preferred) << 2.05;

    QTest::addRow("DELL XPS 13 1080p 13\"")
        << DeviceType::Laptop
        << Test::OutputInfo{
               .geometry = QRect(0, 0, 1920, 1080),
               .internal = true,
               .physicalSizeInMM = QSize(293, 162),
               .modes = {ModeInfo(QSize(1920, 1080), 60000, OutputMode::Flag::Preferred)},
           }
        << ModeInfo(QSize(1920, 1080), 60000, OutputMode::Flag::Preferred) << 1.35;

    QTest::addRow("DELL XPS 13 2160p 13\"")
        << DeviceType::Laptop
        << Test::OutputInfo{
               .geometry = QRect(0, 0, 3840, 2160),
               .internal = true,
               .physicalSizeInMM = QSize(294, 165),
               .modes = {ModeInfo(QSize(3840, 2160), 60000, OutputMode::Flag::Preferred)},
           }
        << ModeInfo(QSize(3840, 2160), 60000, OutputMode::Flag::Preferred) << 2.65;

    QTest::addRow("ThinkPad T14 2400p 14\"")
        << DeviceType::Laptop
        << Test::OutputInfo{
               .geometry = QRect(0, 0, 3840, 2400),
               .internal = true,
               .physicalSizeInMM = QSize(301, 188),
               .modes = {ModeInfo(QSize(3840, 2400), 60000, OutputMode::Flag::Preferred)},
           }
        << ModeInfo(QSize(3840, 2400), 60000, OutputMode::Flag::Preferred) << 2.60;

    QTest::addRow("SteamDeck OLED")
        << DeviceType::Laptop
        << Test::OutputInfo{
               .geometry = QRect(0, 0, 800, 1280),
               .internal = true,
               .physicalSizeInMM = QSize(100, 160),
               .modes = {ModeInfo(QSize(800, 1280), 90000, OutputMode::Flag::Preferred)},
               .panelOrientation = OutputTransform::Kind::Rotate90,
           }
        << ModeInfo(QSize(800, 1280), 90000ul, OutputMode::Flag::Preferred) << 1.0;

    QTest::addRow("Pixel 3a")
        << DeviceType::Phone
        << Test::OutputInfo{
               .geometry = QRect(0, 0, 1080, 2220),
               .internal = true,
               .physicalSizeInMM = QSize(62, 128),
               .modes = {ModeInfo(QSize(1080, 2220), 60000, OutputMode::Flags{}), ModeInfo(QSize(1080, 2220), 120000, OutputMode::Flag::Preferred)},
           }
        << ModeInfo(QSize(1080, 2220), 120000ul, OutputMode::Flag::Preferred) << 2.95;

    QTest::addRow("OnePlus 6")
        << DeviceType::Phone
        << Test::OutputInfo{
               .geometry = QRect(0, 0, 1080, 2280),
               .internal = true,
               .physicalSizeInMM = QSize(68, 145),
               .modes = {ModeInfo(QSize(1080, 2280), 60000, OutputMode::Flag::Preferred)},
           }
        << ModeInfo(QSize(1080, 2280), 60000ul, OutputMode::Flag::Preferred) << 2.65;
}

void OutputChangesTest::testGenerateConfigs()
{
    // delete the previous config to avoid clashes between test runs
    QFile(QStandardPaths::locate(QStandardPaths::ConfigLocation, QStringLiteral("kwinoutputconfig.json"))).remove();

    // Whether there is a lid switch input device is not a totally reliable way to determine if it's
    // a laptop, but on the other hand, we don't have any other better hints.
    QFETCH(DeviceType, deviceType);
    LidSwitch lidSwitch;
    if (deviceType == DeviceType::Laptop) {
        input()->addInputDevice(&lidSwitch);
    }
    const auto lidSwitchGuard = qScopeGuard([&]() {
        if (deviceType == DeviceType::Laptop) {
            input()->removeInputDevice(&lidSwitch);
        }
    });

    QFETCH(Test::OutputInfo, outputInfo);
    Test::setOutputConfig({outputInfo});
    const auto outputs = kwinApp()->outputBackend()->outputs();
    OutputConfigurationStore configs;
    auto cfg = configs.queryConfig(outputs, false, nullptr, false);
    QVERIFY(cfg.has_value());
    const auto [config, order, type] = *cfg;
    const auto outputConfig = config.constChangeSet(outputs.front());

    QFETCH(ModeInfo, defaultMode);
    const auto &[modeSize, modeRefresh, modeFlags] = defaultMode;

    const auto mode = outputConfig->mode->lock();
    QVERIFY(mode);
    QCOMPARE(mode->size(), modeSize);
    QCOMPARE(mode->refreshRate(), modeRefresh);
    QCOMPARE(mode->flags(), modeFlags);

    QFETCH(double, defaultScale);
    QVERIFY(outputConfig->scale);
    QCOMPARE(*outputConfig->scale, defaultScale);
}

void OutputChangesTest::testAutorotate_data()
{
    QTest::addColumn<OutputTransform::Kind>("panelOrientation");
    QTest::addColumn<QOrientationReading::Orientation>("orientation");
    QTest::addColumn<OutputTransform::Kind>("expectedRotation");

    QTest::addRow("panel orientation normal, no rotation") << OutputTransform::Kind::Normal << QOrientationReading::Orientation::TopUp << OutputTransform::Kind::Normal;
    QTest::addRow("panel orientation normal, rotated 90° right") << OutputTransform::Kind::Normal << QOrientationReading::Orientation::LeftUp << OutputTransform::Kind::Rotate90;
    QTest::addRow("panel orientation normal, rotated 180°") << OutputTransform::Kind::Normal << QOrientationReading::Orientation::TopDown << OutputTransform::Kind::Rotate180;
    QTest::addRow("panel orientation normal, rotated 90° left") << OutputTransform::Kind::Normal << QOrientationReading::Orientation::RightUp << OutputTransform::Kind::Rotate270;

    QTest::addRow("panel orientation left up, no rotation") << OutputTransform::Kind::Rotate90 << QOrientationReading::Orientation::TopUp << OutputTransform::Kind::Rotate90;
    QTest::addRow("panel orientation left up, rotated 90° right") << OutputTransform::Kind::Rotate90 << QOrientationReading::Orientation::LeftUp << OutputTransform::Kind::Rotate180;
    QTest::addRow("panel orientation left up, rotated 180°") << OutputTransform::Kind::Rotate90 << QOrientationReading::Orientation::TopDown << OutputTransform::Kind::Rotate270;
    QTest::addRow("panel orientation left up, rotated 90° left") << OutputTransform::Kind::Rotate90 << QOrientationReading::Orientation::RightUp << OutputTransform::Kind::Normal;

    QTest::addRow("panel orientation upside down, no rotation") << OutputTransform::Kind::Rotate180 << QOrientationReading::Orientation::TopUp << OutputTransform::Kind::Rotate180;
    QTest::addRow("panel orientation upside down, rotated 90° right") << OutputTransform::Kind::Rotate180 << QOrientationReading::Orientation::LeftUp << OutputTransform::Kind::Rotate270;
    QTest::addRow("panel orientation upside down, rotated 180°") << OutputTransform::Kind::Rotate180 << QOrientationReading::Orientation::TopDown << OutputTransform::Kind::Normal;
    QTest::addRow("panel orientation upside down, rotated 90° left") << OutputTransform::Kind::Rotate180 << QOrientationReading::Orientation::RightUp << OutputTransform::Kind::Rotate90;

    QTest::addRow("panel orientation right up, no rotation") << OutputTransform::Kind::Rotate270 << QOrientationReading::Orientation::TopUp << OutputTransform::Kind::Rotate270;
    QTest::addRow("panel orientation right up, rotated 90° right") << OutputTransform::Kind::Rotate270 << QOrientationReading::Orientation::LeftUp << OutputTransform::Kind::Normal;
    QTest::addRow("panel orientation right up, rotated 180°") << OutputTransform::Kind::Rotate270 << QOrientationReading::Orientation::TopDown << OutputTransform::Kind::Rotate90;
    QTest::addRow("panel orientation right up, rotated 90° left") << OutputTransform::Kind::Rotate270 << QOrientationReading::Orientation::RightUp << OutputTransform::Kind::Rotate180;
}

void OutputChangesTest::testAutorotate()
{
    // delete the previous config to avoid clashes between test runs
    QFile(QStandardPaths::locate(QStandardPaths::ConfigLocation, QStringLiteral("kwinoutputconfig.json"))).remove();

    QFETCH(OutputTransform::Kind, panelOrientation);
    Test::setOutputConfig({Test::OutputInfo{
        .geometry = QRect(0, 0, 1280, 1024),
        .internal = true,
        .physicalSizeInMM = QSize(598, 336),
        .modes = {ModeInfo(QSize(1280, 1024), 60000, OutputMode::Flag::Preferred)},
        .panelOrientation = panelOrientation,
    }});

    QFETCH(QOrientationReading::Orientation, orientation);
    QOrientationReading sensorReading;
    sensorReading.setOrientation(orientation);

    const auto outputs = kwinApp()->outputBackend()->outputs();
    OutputConfigurationStore configs;
    auto cfg = configs.queryConfig(outputs, false, &sensorReading, true);
    QVERIFY(cfg.has_value());
    const auto [config, order, type] = *cfg;
    const auto outputConfig = config.constChangeSet(outputs.front());

    QCOMPARE(outputConfig->autoRotationPolicy, Output::AutoRotationPolicy::InTabletMode);

    QFETCH(OutputTransform::Kind, expectedRotation);
    QVERIFY(outputConfig->transform.has_value());
    QCOMPARE(outputConfig->transform->kind(), expectedRotation);
}

struct IdentificationData
{
    std::optional<QString> connectorName;
    QByteArray edid;
    std::optional<QByteArray> mstPath;
};

void OutputChangesTest::testSettingRestoration_data()
{
    QTest::addColumn<QList<IdentificationData>>("outputData");

    const auto readEdid = [](const QString &path) {
        QFile file(path);
        file.open(QIODeviceBase::OpenModeFlag::ReadOnly);
        return file.readAll();
    };

    QTest::addRow("Same EDID ID, different hash") << QList{
        IdentificationData{
            .connectorName = std::nullopt,
            .edid = readEdid(QFINDTESTDATA("data/same serial number/edid.bin")),
            .mstPath = std::nullopt,
        },
        IdentificationData{
            .connectorName = std::nullopt,
            .edid = readEdid(QFINDTESTDATA("data/same serial number/edid2.bin")),
            .mstPath = std::nullopt,
        },
    };

    QTest::addRow("Same EDID") << QList{
        IdentificationData{
            .connectorName = QStringLiteral("connector1"),
            .edid = readEdid(QFINDTESTDATA("data/same serial number/edid.bin")),
            .mstPath = std::nullopt,
        },
        IdentificationData{
            .connectorName = QStringLiteral("connector2"),
            .edid = readEdid(QFINDTESTDATA("data/same serial number/edid.bin")),
            .mstPath = std::nullopt,
        },
    };

    QTest::addRow("No EDID") << QList{
        IdentificationData{
            .connectorName = QStringLiteral("connector1"),
            .edid = QByteArray{},
            .mstPath = std::nullopt,
        },
        IdentificationData{
            .connectorName = QStringLiteral("connector2"),
            .edid = QByteArray{},
            .mstPath = std::nullopt,
        },
    };

    QTest::addRow("One has EDID, the other doesn't") << QList{
        IdentificationData{
            .connectorName = QStringLiteral("connector1"),
            .edid = readEdid(QFINDTESTDATA("data/same serial number/edid.bin")),
            .mstPath = std::nullopt,
        },
        IdentificationData{
            .connectorName = QStringLiteral("connector2"),
            .edid = QByteArray{},
            .mstPath = std::nullopt,
        },
    };

    QTest::addRow("Same EDID, no connector names, different MST paths") << QList{
        IdentificationData{
            .connectorName = std::nullopt,
            .edid = readEdid(QFINDTESTDATA("data/same serial number/edid.bin")),
            .mstPath = QByteArrayLiteral("MST-1-1"),
        },
        IdentificationData{
            .connectorName = std::nullopt,
            .edid = readEdid(QFINDTESTDATA("data/same serial number/edid.bin")),
            .mstPath = QByteArrayLiteral("MST-1-2"),
        },
    };

    QTest::addRow("Same EDID ID, different hash, no connector names, different MST paths") << QList{
        IdentificationData{
            .connectorName = std::nullopt,
            .edid = readEdid(QFINDTESTDATA("data/same serial number/edid.bin")),
            .mstPath = QByteArrayLiteral("MST-1-1"),
        },
        IdentificationData{
            .connectorName = std::nullopt,
            .edid = readEdid(QFINDTESTDATA("data/same serial number/edid2.bin")),
            .mstPath = QByteArrayLiteral("MST-1-2"),
        },
    };

    QTest::addRow("No EDID, no connector names, different MST paths") << QList{
        IdentificationData{
            .connectorName = std::nullopt,
            .edid = QByteArray{},
            .mstPath = QByteArrayLiteral("MST-1-1"),
        },
        IdentificationData{
            .connectorName = std::nullopt,
            .edid = QByteArray{},
            .mstPath = QByteArrayLiteral("MST-1-2"),
        },
    };

    QTest::addRow("One EDID, the other not, no connector names, different MST paths") << QList{
        IdentificationData{
            .connectorName = std::nullopt,
            .edid = readEdid(QFINDTESTDATA("data/same serial number/edid.bin")),
            .mstPath = QByteArrayLiteral("MST-1-1"),
        },
        IdentificationData{
            .connectorName = std::nullopt,
            .edid = QByteArray{},
            .mstPath = QByteArrayLiteral("MST-1-2"),
        },
    };

    QTest::addRow("Only EDID hash, no connector names, no MST path") << QList{
        IdentificationData{
            .connectorName = std::nullopt,
            .edid = QByteArrayLiteral("bbbbbbbbbbbbbbbb"),
            .mstPath = std::nullopt,
        },
        IdentificationData{
            .connectorName = std::nullopt,
            .edid = QByteArrayLiteral("aaaaaaaaaaaaaaaa"),
            .mstPath = std::nullopt,
        },
    };

    QTest::addRow("One EDID ID, other only EDID hash") << QList{
        IdentificationData{
            .connectorName = std::nullopt,
            .edid = readEdid(QFINDTESTDATA("data/same serial number/edid.bin")),
            .mstPath = std::nullopt,
        },
        IdentificationData{
            .connectorName = std::nullopt,
            .edid = QByteArrayLiteral("aaaaaaaaaaaaaaaa"),
            .mstPath = std::nullopt,
        },
    };

    QTest::addRow("three outputs, two with the same EDID, with overlapping MST paths") << QList{
        IdentificationData{
            .connectorName = std::nullopt,
            .edid = readEdid(QFINDTESTDATA("data/same serial number/edid.bin")),
            .mstPath = QByteArrayLiteral("MST-1-1"),
        },
        IdentificationData{
            .connectorName = std::nullopt,
            .edid = readEdid(QFINDTESTDATA("data/same serial number/edid.bin")),
            .mstPath = QByteArrayLiteral("MST-1-2"),
        },
        IdentificationData{
            .connectorName = std::nullopt,
            .edid = readEdid(QFINDTESTDATA("data/same serial number/edid2.bin")),
            .mstPath = QByteArrayLiteral("MST-1-1"),
        },
    };
}

void OutputChangesTest::testSettingRestoration()
{
    // this test verifies that we restore configs correctly,
    // even if there's no unique EDID ID to match them with

    // delete the previous config to avoid clashes between test runs
    QFile(QStandardPaths::locate(QStandardPaths::ConfigLocation, QStringLiteral("kwinoutputconfig.json"))).remove();

    QFETCH(QList<IdentificationData>, outputData);

    Test::setOutputConfig(outputData | std::views::transform([](const IdentificationData &data) {
        return Test::OutputInfo{
            .geometry = QRect(0, 0, 1280, 1024),
            .internal = false,
            .physicalSizeInMM = QSize(598, 336),
            .modes = {ModeInfo(QSize(1280, 1024), 60000, OutputMode::Flag::Preferred)},
            .edid = data.edid,
            .edidIdentifierOverride = std::nullopt,
            .connectorName = data.connectorName,
            .mstPath = data.mstPath,
        };
    }) | std::ranges::to<QList>());

    auto outputs = kwinApp()->outputBackend()->outputs();
    OutputConfigurationStore configs;

    QList<std::optional<QPoint>> outputPositions;
    {
        auto cfg = configs.queryConfig(outputs, false, nullptr, false);
        QVERIFY(cfg.has_value());
        const auto [config, order, type] = *cfg;
        for (const auto output : outputs) {
            output->applyChanges(config);
            outputPositions.push_back(config.constChangeSet(output)->pos);
        }
    }

    // the positions must be independent of the order of outputs in the list
    std::ranges::reverse(outputs);
    {
        auto cfg = configs.queryConfig(outputs, false, nullptr, false);
        QVERIFY(cfg.has_value());
        const auto [config, order, type] = *cfg;
        auto revertedPositions = outputPositions | std::views::reverse;
        for (int i = 0; i < outputs.size(); i++) {
            QCOMPARE(revertedPositions[i], config.constChangeSet(outputs[i])->pos);
        }
    }

    // this must work if one of the outputs is removed in between as well
    Test::setOutputConfig({
        Test::OutputInfo{
            .geometry = QRect(1280, 0, 1280, 1024),
            .internal = false,
            .physicalSizeInMM = QSize(598, 336),
            .modes = {ModeInfo(QSize(1280, 1024), 60000, OutputMode::Flag::Preferred)},
            .edid = outputData.back().edid,
            .edidIdentifierOverride = std::nullopt,
            .connectorName = outputData.back().connectorName,
            .mstPath = outputData.back().mstPath,
        },
    });
    outputs = kwinApp()->outputBackend()->outputs();
    {
        auto cfg = configs.queryConfig(outputs, false, nullptr, false);
        const auto [config, order, type] = *cfg;
        outputs.front()->applyChanges(config);
    }

    // and add it again, with the inverted order
    Test::setOutputConfig(outputData | std::views::reverse | std::views::transform([](const IdentificationData &data) {
        return Test::OutputInfo{
            .geometry = QRect(0, 0, 1280, 1024),
            .internal = false,
            .physicalSizeInMM = QSize(598, 336),
            .modes = {ModeInfo(QSize(1280, 1024), 60000, OutputMode::Flag::Preferred)},
            .edid = data.edid,
            .edidIdentifierOverride = std::nullopt,
            .connectorName = data.connectorName,
            .mstPath = data.mstPath,
        };
    }) | std::ranges::to<QList>());
    outputs = kwinApp()->outputBackend()->outputs();

    {
        auto cfg = configs.queryConfig(outputs, false, nullptr, false);
        QVERIFY(cfg.has_value());
        const auto [config, order, type] = *cfg;
        auto revertedPositions = outputPositions | std::views::reverse;
        for (int i = 0; i < outputs.size(); i++) {
            QCOMPARE(revertedPositions[i], config.constChangeSet(outputs[i])->pos);
        }
    }
}

void OutputChangesTest::testSettingRestoration_initialParsingFailure()
{
    // this test checks that when libdisplay-info fails to parse an EDID
    // and gets fixed later, we still pick the same settings as before

    // delete the previous config to avoid clashes between test runs
    QFile(QStandardPaths::locate(QStandardPaths::ConfigLocation, QStringLiteral("kwinoutputconfig.json"))).remove();

    QFile file(QFINDTESTDATA("data/same serial number/edid.bin"));
    file.open(QIODeviceBase::OpenModeFlag::ReadOnly);
    const auto edid = file.readAll();

    // first, libdisplay-info failed to parse the EDID and we don't have an EDID ID
    // note that this uses two displays with the same EDID,
    // to additionally test the case when EDID ID isn't unique when this happens
    Test::setOutputConfig({
        Test::OutputInfo{
            .geometry = QRect(0, 0, 1280, 1024),
            .internal = false,
            .physicalSizeInMM = QSize(598, 336),
            .modes = {
                ModeInfo(QSize(1280, 1024), 60000, OutputMode::Flag::Preferred),
                ModeInfo(QSize(640, 480), 60000, OutputMode::Flags{}),
            },
            .edid = edid,
            .edidIdentifierOverride = QByteArray(),
            .connectorName = std::nullopt,
            .mstPath = QByteArrayLiteral("MST-1-1"),
        },
        Test::OutputInfo{
            .geometry = QRect(0, 0, 1280, 1024),
            .internal = false,
            .physicalSizeInMM = QSize(598, 336),
            .modes = {
                ModeInfo(QSize(1280, 1024), 60000, OutputMode::Flag::Preferred),
                ModeInfo(QSize(640, 480), 60000, OutputMode::Flags{}),
            },
            .edid = edid,
            .edidIdentifierOverride = QByteArray(),
            .connectorName = std::nullopt,
            .mstPath = QByteArrayLiteral("MST-1-2"),
        },
    });

    auto outputs = kwinApp()->outputBackend()->outputs();
    OutputConfigurationStore configs;

    {
        // query the generated config, like KWin normally would
        auto cfg = configs.queryConfig(outputs, false, nullptr, false);
        QVERIFY(cfg.has_value());
        const auto [config, order, type] = *cfg;
        outputs.front()->applyChanges(config);
        QCOMPARE(config.constChangeSet(outputs[0])->desiredModeSize.value(), QSize(1280, 1024));
    }
    {
        // change the mode, so that we know if a new config entry was generated
        OutputConfiguration config;
        const auto changeSet = config.changeSet(outputs[0]);
        changeSet->mode = outputs[0]->modes()[1];
        changeSet->desiredModeSize = QSize(640, 480);
        changeSet->desiredModeRefreshRate = 60000;
        outputs.front()->applyChanges(config);
        configs.storeConfig(outputs, false, config, outputs);
    }
    {
        // verify that querying the config also shows the changed mode
        // things could already go wrong here
        auto cfg = configs.queryConfig(outputs, false, nullptr, false);
        QVERIFY(cfg.has_value());
        const auto [config, order, type] = *cfg;
        QCOMPARE(type, OutputConfigurationStore::ConfigType::Preexisting);
        outputs.front()->applyChanges(config);
        QCOMPARE(config.constChangeSet(outputs[0])->desiredModeSize.value(), QSize(640, 480));
    }

    // now libdisplay-info was updated, and we have an EDID ID for the same hash
    Test::setOutputConfig({
        Test::OutputInfo{
            .geometry = QRect(0, 0, 1280, 1024),
            .internal = false,
            .physicalSizeInMM = QSize(598, 336),
            .modes = {
                ModeInfo(QSize(1280, 1024), 60000, OutputMode::Flag::Preferred),
                ModeInfo(QSize(640, 480), 60000, OutputMode::Flags{}),
            },
            .edid = edid,
            .edidIdentifierOverride = std::nullopt,
            .connectorName = std::nullopt,
            .mstPath = QByteArrayLiteral("MST-1-1"),
        },
        Test::OutputInfo{
            .geometry = QRect(0, 0, 1280, 1024),
            .internal = false,
            .physicalSizeInMM = QSize(598, 336),
            .modes = {
                ModeInfo(QSize(1280, 1024), 60000, OutputMode::Flag::Preferred),
                ModeInfo(QSize(640, 480), 60000, OutputMode::Flags{}),
            },
            .edid = edid,
            .edidIdentifierOverride = std::nullopt,
            .connectorName = std::nullopt,
            .mstPath = QByteArrayLiteral("MST-1-2"),
        },
    });
    outputs = kwinApp()->outputBackend()->outputs();

    {
        auto cfg = configs.queryConfig(outputs, false, nullptr, false);
        QVERIFY(cfg.has_value());
        const auto [config, order, type] = *cfg;
        QCOMPARE(config.constChangeSet(outputs[0])->desiredModeSize.value(), QSize(640, 480));
    }
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::OutputChangesTest)
#include "outputchanges_test.moc"
