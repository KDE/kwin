/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "core/outputconfiguration.h"
#include "pointer_input.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_workspace-0");

class WorkspaceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void evacuateMappedWindowFromRemovedOutput();
    void evacuateUnmappedWindowFromRemovedOutput();
    void activeOutputFollowsPointer();
    void activeOutputFollowsTouch();
    void activeOutputFollowsTablet();
    void activeOutputFollowsActiveWindow();
    void activeOutputDoesntFollowInactiveWindow();
    void disableActiveOutput();
    void activeOutputAfterActivateNextWindowOnOutputAdded();
    void activeOutputAfterActivateNextWindowOnOutputRemoved_data();
    void activeOutputAfterActivateNextWindowOnOutputRemoved();
};

void WorkspaceTest::initTestCase()
{
    QVERIFY(waylandServer()->init(s_socketName));
    kwinApp()->start();
}

void WorkspaceTest::init()
{
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    QVERIFY(Test::setupWaylandConnection());

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
}

void WorkspaceTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void WorkspaceTest::evacuateMappedWindowFromRemovedOutput()
{
    // This test verifies that a window will be evacuated to another output if the output it is
    // currently on has been either removed or disabled.

    const auto firstOutput = workspace()->outputs()[0];
    const auto secondOutput = workspace()->outputs()[1];

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QCOMPARE(window->output(), firstOutput);
    QCOMPARE(window->moveResizeOutput(), firstOutput);

    QSignalSpy outputChangedSpy(window, &Window::outputChanged);
    {
        OutputConfiguration config;
        config.changeSet(firstOutput)->enabled = false;
        workspace()->applyOutputConfiguration(config);
    }
    QCOMPARE(outputChangedSpy.count(), 1);
    QCOMPARE(window->output(), secondOutput);
    QCOMPARE(window->moveResizeOutput(), secondOutput);
}

void WorkspaceTest::evacuateUnmappedWindowFromRemovedOutput()
{
    // This test verifies that a window, which is not fully managed by the Workspace yet, will be
    // evacuated to another output if the output it is currently on has been withdrawn.

    const auto firstOutput = workspace()->outputs()[0];
    const auto secondOutput = workspace()->outputs()[1];

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));

    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(Test::waylandSync());
    QCOMPARE(waylandServer()->windows().count(), 1);
    Window *window = waylandServer()->windows().constFirst();
    QVERIFY(!window->readyForPainting());
    QCOMPARE(window->output(), firstOutput);
    QCOMPARE(window->moveResizeOutput(), firstOutput);

    QSignalSpy outputChangedSpy(window, &Window::outputChanged);
    {
        OutputConfiguration config;
        config.changeSet(firstOutput)->enabled = false;
        workspace()->applyOutputConfiguration(config);
    }
    QCOMPARE(outputChangedSpy.count(), 1);
    QCOMPARE(window->output(), secondOutput);
    QCOMPARE(window->moveResizeOutput(), secondOutput);
}

void WorkspaceTest::activeOutputFollowsPointer()
{
    // This test verifies that the active output follows pointer input.

    const auto firstOutput = workspace()->outputs()[0];
    const auto secondOutput = workspace()->outputs()[1];

    quint32 timestamp = 0;
    Test::pointerMotion(firstOutput->geometry().center(), timestamp++);
    QCOMPARE(workspace()->activeOutput(), firstOutput);

    Test::pointerMotion(secondOutput->geometry().center(), timestamp++);
    QCOMPARE(workspace()->activeOutput(), secondOutput);

    Test::pointerMotion(firstOutput->geometry().center(), timestamp++);
    QCOMPARE(workspace()->activeOutput(), firstOutput);
}

void WorkspaceTest::activeOutputFollowsTouch()
{
    // This test verifies that the active output follows touch input.

    const auto firstOutput = workspace()->outputs()[0];
    const auto secondOutput = workspace()->outputs()[1];

    quint32 timestamp = 0;

    {
        Test::touchDown(0, firstOutput->geometry().center(), timestamp++);
        QCOMPARE(workspace()->activeOutput(), firstOutput);

        Test::touchMotion(0, firstOutput->geometry().center() + QPointF(10, 10), timestamp++);
        QCOMPARE(workspace()->activeOutput(), firstOutput);

        Test::touchUp(0, timestamp++);
        QCOMPARE(workspace()->activeOutput(), firstOutput);
    }

    {
        Test::touchDown(0, secondOutput->geometry().center(), timestamp++);
        QCOMPARE(workspace()->activeOutput(), secondOutput);

        Test::touchMotion(0, secondOutput->geometry().center() + QPointF(10, 10), timestamp++);
        QCOMPARE(workspace()->activeOutput(), secondOutput);

        Test::touchUp(0, timestamp++);
        QCOMPARE(workspace()->activeOutput(), secondOutput);
    }
}

void WorkspaceTest::activeOutputFollowsTablet()
{
    // This test verifies that the active output follows tablet input.

    const auto firstOutput = workspace()->outputs()[0];
    const auto secondOutput = workspace()->outputs()[1];

    quint32 timestamp = 0;
    Test::tabletToolProximityEvent(firstOutput->geometry().center(), 0, 0, 0, 0, true, 0, timestamp++);
    QCOMPARE(workspace()->activeOutput(), firstOutput);

    Test::tabletToolTipEvent(firstOutput->geometry().center(), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QCOMPARE(workspace()->activeOutput(), firstOutput);

    Test::tabletToolAxisEvent(firstOutput->geometry().center(), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QCOMPARE(workspace()->activeOutput(), firstOutput);

    Test::tabletToolAxisEvent(secondOutput->geometry().center(), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QCOMPARE(workspace()->activeOutput(), secondOutput);

    Test::tabletToolAxisEvent(firstOutput->geometry().center(), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QCOMPARE(workspace()->activeOutput(), firstOutput);

    Test::tabletToolTipEvent(firstOutput->geometry().center(), 0, 0, 0, 0, 0, false, 0, timestamp++);
    QCOMPARE(workspace()->activeOutput(), firstOutput);

    Test::tabletToolProximityEvent(firstOutput->geometry().center(), 0, 0, 0, 0, false, 0, timestamp++);
    QCOMPARE(workspace()->activeOutput(), firstOutput);
}

void WorkspaceTest::activeOutputFollowsActiveWindow()
{
    // This test verifies that the active output follows the active window.

    const auto firstOutput = workspace()->outputs()[0];
    const auto secondOutput = workspace()->outputs()[1];

    std::unique_ptr<KWayland::Client::Surface> firstSurface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> firstShellSurface(Test::createXdgToplevelSurface(firstSurface.get()));
    auto firstWindow = Test::renderAndWaitForShown(firstSurface.get(), QSize(100, 50), Qt::blue);
    firstWindow->sendToOutput(firstOutput);
    QCOMPARE(workspace()->activeOutput(), firstOutput);

    std::unique_ptr<KWayland::Client::Surface> secondSurface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> secondShellSurface(Test::createXdgToplevelSurface(secondSurface.get()));
    auto secondWindow = Test::renderAndWaitForShown(secondSurface.get(), QSize(100, 50), Qt::red);
    secondWindow->sendToOutput(secondOutput);
    QCOMPARE(workspace()->activeOutput(), secondOutput);

    workspace()->activateWindow(firstWindow);
    QCOMPARE(workspace()->activeOutput(), firstOutput);

    workspace()->activateWindow(secondWindow);
    QCOMPARE(workspace()->activeOutput(), secondOutput);

    workspace()->activateWindow(firstWindow);
    QCOMPARE(workspace()->activeOutput(), firstOutput);

    quint32 timestamp = 0;
    Test::pointerMotion(secondOutput->geometry().center(), timestamp++);
    QCOMPARE(workspace()->activeOutput(), secondOutput);
}

void WorkspaceTest::activeOutputDoesntFollowInactiveWindow()
{
    // This test verifies that the active output doesn't follow inactive windows.

    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
        QRect(2560, 0, 1280, 1024),
    });

    const auto firstOutput = workspace()->outputs()[0];
    const auto secondOutput = workspace()->outputs()[1];
    const auto thirdOutput = workspace()->outputs()[2];

    std::unique_ptr<KWayland::Client::Surface> firstSurface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> firstShellSurface(Test::createXdgToplevelSurface(firstSurface.get()));
    auto firstWindow = Test::renderAndWaitForShown(firstSurface.get(), QSize(100, 50), Qt::blue);
    firstWindow->sendToOutput(firstOutput);
    QCOMPARE(workspace()->activeOutput(), firstOutput);

    std::unique_ptr<KWayland::Client::Surface> secondSurface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> secondShellSurface(Test::createXdgToplevelSurface(secondSurface.get()));
    auto secondWindow = Test::renderAndWaitForShown(secondSurface.get(), QSize(100, 50), Qt::red);
    secondWindow->sendToOutput(secondOutput);
    QCOMPARE(workspace()->activeOutput(), secondOutput);

    firstWindow->sendToOutput(thirdOutput);
    QCOMPARE(workspace()->activeOutput(), secondOutput);

    firstWindow->sendToOutput(firstOutput);
    QCOMPARE(workspace()->activeOutput(), secondOutput);
}

void WorkspaceTest::disableActiveOutput()
{
    // This test verifies that the active output property will be reset when the corresponding output is disabled.

    const auto firstOutput = workspace()->outputs()[0];
    const auto secondOutput = workspace()->outputs()[1];
    QCOMPARE(workspace()->activeOutput(), firstOutput);

    OutputConfiguration config;
    {
        auto changeSet = config.changeSet(workspace()->activeOutput());
        changeSet->enabled = false;
    }
    workspace()->applyOutputConfiguration(config);
    QCOMPARE(workspace()->activeOutput(), secondOutput);
}

void WorkspaceTest::activeOutputAfterActivateNextWindowOnOutputAdded()
{
    // This test verifies that the workspace doesn't end up with corrupted state when the Workspace::outputAdded() signal is emitted.
    // activateNextWindow() is interesting because it changes the active output.

    const auto firstOutput = workspace()->outputs()[0];
    const auto secondOutput = workspace()->outputs()[1];

    {
        OutputConfiguration config;
        {
            auto changeSet = config.changeSet(firstOutput);
            changeSet->enabled = false;
        }
        {
            auto changeSet = config.changeSet(secondOutput);
            changeSet->enabled = false;
        }
        workspace()->applyOutputConfiguration(config);
    }

    std::unique_ptr<KWayland::Client::Surface> firstSurface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> firstShellSurface(Test::createXdgToplevelSurface(firstSurface.get()));
    auto firstWindow = Test::renderAndWaitForShown(firstSurface.get(), QSize(100, 50), Qt::blue);

    std::unique_ptr<KWayland::Client::Surface> secondSurface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> secondShellSurface(Test::createXdgToplevelSurface(secondSurface.get()));
    auto secondWindow = Test::renderAndWaitForShown(secondSurface.get(), QSize(100, 50), Qt::red);
    QCOMPARE(workspace()->activeWindow(), secondWindow);

    connect(workspace(), &Workspace::outputAdded, secondWindow, [secondWindow]() {
        workspace()->activateNextWindow(secondWindow);
    });

    {
        OutputConfiguration config;
        {
            auto changeSet = config.changeSet(firstOutput);
            changeSet->enabled = true;
        }
        workspace()->applyOutputConfiguration(config);
    }

    QCOMPARE(workspace()->activeWindow(), firstWindow);
    QCOMPARE(workspace()->activeOutput(), firstOutput);
}

void WorkspaceTest::activeOutputAfterActivateNextWindowOnOutputRemoved_data()
{
    QTest::addColumn<bool>("separateScreenFocus");

    QTest::addRow("split screen focus") << true;
    QTest::addRow("unified screen focus") << false;
}

void WorkspaceTest::activeOutputAfterActivateNextWindowOnOutputRemoved()
{
    // This test verifies that the workspace doesn't end up with corrupted state when the Workspace::outputAdded() signal is emitted.
    // activateNextWindow() is interesting because it changes the active output.

    QFETCH(bool, separateScreenFocus);
    options->setSeparateScreenFocus(separateScreenFocus);

    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
        QRect(2560, 0, 1280, 1024),
    });

    const auto firstOutput = workspace()->outputs()[0];
    const auto secondOutput = workspace()->outputs()[1];
    const auto thirdOutput = workspace()->outputs()[2];

    std::unique_ptr<KWayland::Client::Surface> firstSurface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> firstShellSurface(Test::createXdgToplevelSurface(firstSurface.get()));
    auto firstWindow = Test::renderAndWaitForShown(firstSurface.get(), QSize(100, 50), Qt::blue);
    firstWindow->sendToOutput(firstOutput);

    std::unique_ptr<KWayland::Client::Surface> secondSurface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> secondShellSurface(Test::createXdgToplevelSurface(secondSurface.get()));
    auto secondWindow = Test::renderAndWaitForShown(secondSurface.get(), QSize(100, 50), Qt::red);
    secondWindow->sendToOutput(secondOutput);

    workspace()->activateWindow(firstWindow);
    QCOMPARE(workspace()->activeWindow(), firstWindow);

    connect(workspace(), &Workspace::outputRemoved, firstWindow, [firstOutput, firstWindow](Output *output) {
        if (output == firstOutput) {
            workspace()->activateNextWindow(firstWindow);
        }
    });

    {
        OutputConfiguration config;
        {
            auto changeSet = config.changeSet(firstOutput);
            changeSet->enabled = false;
        }
        {
            auto changeSet = config.changeSet(secondOutput);
            changeSet->enabled = false;
        }
        {
            auto changeSet = config.changeSet(thirdOutput);
            changeSet->pos = QPoint(0, 0);
        }
        workspace()->applyOutputConfiguration(config);
    }

    QCOMPARE(workspace()->activeWindow(), separateScreenFocus ? nullptr : secondWindow);
    QCOMPARE(workspace()->activeOutput(), thirdOutput);
}

WAYLANDTEST_MAIN(WorkspaceTest)
#include "workspace_test.moc"
