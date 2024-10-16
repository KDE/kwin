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

WAYLANDTEST_MAIN(WorkspaceTest)
#include "workspace_test.moc"
