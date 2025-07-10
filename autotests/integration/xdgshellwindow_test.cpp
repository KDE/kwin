/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "decorations/decorationbridge.h"
#include "decorations/settings.h"
#include "pointer_input.h"
#include "virtualdesktops.h"
#include "wayland/clientconnection.h"
#include "wayland/display.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationSettings>

#include <KWayland/Client/appmenu.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>

#include <QDBusConnection>

// system
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <csignal>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_xdgshellwindow-0");

class TestXdgShellWindow : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testMapUnmap();
    void testWindowOutputs();
    void testMinimizeActiveWindow();
    void testFullscreen_data();
    void testFullscreen();
    void testUserCanSetFullscreen();
    void testSendFullScreenWindowToAnotherOutput();

    void testMaximizeHorizontal();
    void testMaximizeVertical();
    void testMaximizeFull();
    void testMaximizedToFullscreen_data();
    void testMaximizedToFullscreen();
    void testSendMaximizedWindowToAnotherOutput();
    void testInteractiveMoveUnmaximizeFull();
    void testInteractiveMoveUnmaximizeInitiallyFull();
    void testInteractiveMoveUnmaximizeHorizontal();
    void testInteractiveMoveUnmaximizeVertical();
    void testFullscreenMultipleOutputs();
    void testHidden();
    void testDesktopFileName();
    void testCaptionSimplified();
    void testUnresponsiveWindow_data();
    void testUnresponsiveWindow();
    void testAppMenu();
    void testSendClientWithTransientToDesktop();
    void testMinimizeWindowWithTransients();
    void testXdgDecoration_data();
    void testXdgDecoration();
    void testXdgNeverCommitted();
    void testXdgInitialState();
    void testXdgInitiallyMaximised();
    void testXdgInitiallyFullscreen();
    void testXdgInitiallyMinimized();
    void testXdgWindowGeometryIsntSet();
    void testXdgWindowGeometryAttachBuffer();
    void testXdgWindowGeometryAttachSubSurface();
    void testXdgWindowGeometryInteractiveResize();
    void testXdgWindowGeometryFullScreen();
    void testXdgWindowGeometryMaximize();
    void testXdgPopupReactive_data();
    void testXdgPopupReactive();
    void testXdgPopupReposition();
    void testPointerInputTransform();
    void testReentrantSetFrameGeometry();
    void testDoubleMaximize();
    void testDoubleFullscreenSeparatedByCommit();
    void testMaximizeAndChangeDecorationModeAfterInitialCommit();
    void testFullScreenAndChangeDecorationModeAfterInitialCommit();
    void testChangeDecorationModeAfterInitialCommit();
    void testModal();
    void testCloseModal();
    void testCloseModalPreSetup();
    void testCloseInactiveModal();
    void testClosePopupOnParentUnmapped();
    void testPopupWithDismissedParent();
    void testMinimumSize();
    void testNoMinimumSize();
    void testMaximumSize();
    void testNoMaximumSize();
    void testUnconfiguredBufferToplevel();
    void testUnconfiguredBufferPopup();
};

void TestXdgShellWindow::testXdgPopupReactive_data()
{
    QTest::addColumn<bool>("reactive");
    QTest::addColumn<QPointF>("parentPos");
    QTest::addColumn<QPointF>("popupPos");

    QTest::addRow("reactive") << true << QPointF(0, 1024) << QPointF(50, 1024 - 10);
    QTest::addRow("not reactive") << false << QPointF(0, 1024) << QPointF(50, 1024 + 40);
}

void TestXdgShellWindow::testXdgPopupReactive()
{
    // This test verifies the behavior of reactive popups. If a popup is not reactive,
    // it only has to move together with its parent. If a popup is reactive, it moves
    // with its parent and it's reconstrained as needed.

    std::unique_ptr<Test::XdgPositioner> positioner(Test::createXdgPositioner());
    positioner->set_size(10, 10);
    positioner->set_anchor_rect(10, 10, 10, 10);
    positioner->set_anchor_rect(0, 0, 50, 40);
    positioner->set_anchor(Test::XdgPositioner::anchor_bottom_right);
    positioner->set_gravity(Test::XdgPositioner::gravity_bottom_right);
    positioner->set_constraint_adjustment(Test::XdgPositioner::constraint_adjustment_slide_y);

    QFETCH(bool, reactive);
    if (reactive) {
        positioner->set_reactive();
    }

    std::unique_ptr<KWayland::Client::Surface> rootSurface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> root(Test::createXdgToplevelSurface(rootSurface.get()));
    auto rootWindow = Test::renderAndWaitForShown(rootSurface.get(), QSize(100, 100), Qt::cyan);
    QVERIFY(rootWindow);

    std::unique_ptr<KWayland::Client::Surface> childSurface(Test::createSurface());
    std::unique_ptr<Test::XdgPopup> popup(Test::createXdgPopupSurface(childSurface.get(), root->xdgSurface(), positioner.get()));
    auto childWindow = Test::renderAndWaitForShown(childSurface.get(), QSize(10, 10), Qt::cyan);
    QVERIFY(childWindow);

    QFETCH(QPointF, parentPos);
    QFETCH(QPointF, popupPos);

    QSignalSpy popupConfigureRequested(popup.get(), &Test::XdgPopup::configureRequested);
    rootWindow->move(parentPos);
    QVERIFY(popupConfigureRequested.wait());
    QCOMPARE(popupConfigureRequested.count(), 1);

    QCOMPARE(childWindow->pos(), popupPos);
}

void TestXdgShellWindow::testXdgPopupReposition()
{
    std::unique_ptr<Test::XdgPositioner> positioner(Test::createXdgPositioner());
    positioner->set_size(10, 10);
    positioner->set_anchor_rect(10, 10, 10, 10);

    std::unique_ptr<Test::XdgPositioner> otherPositioner(Test::createXdgPositioner());
    otherPositioner->set_size(50, 50);
    otherPositioner->set_anchor_rect(10, 10, 10, 10);

    std::unique_ptr<KWayland::Client::Surface> rootSurface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> root(Test::createXdgToplevelSurface(rootSurface.get()));
    auto rootWindow = Test::renderAndWaitForShown(rootSurface.get(), QSize(100, 100), Qt::cyan);
    QVERIFY(rootWindow);

    std::unique_ptr<KWayland::Client::Surface> childSurface(Test::createSurface());
    std::unique_ptr<Test::XdgPopup> popup(Test::createXdgPopupSurface(childSurface.get(), root->xdgSurface(), positioner.get()));
    auto childWindow = Test::renderAndWaitForShown(childSurface.get(), QSize(10, 10), Qt::cyan);
    QVERIFY(childWindow);

    QSignalSpy reconfigureSpy(popup.get(), &Test::XdgPopup::configureRequested);

    popup->reposition(otherPositioner->object(), 500000);

    QVERIFY(reconfigureSpy.wait());
    QCOMPARE(reconfigureSpy.count(), 1);
}

void TestXdgShellWindow::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWayland::Client::Output *>();

    QVERIFY(waylandServer()->init(s_socketName));

    kwinApp()->start();
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void TestXdgShellWindow::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::XdgDecorationV1 | Test::AdditionalWaylandInterface::AppMenu | Test::AdditionalWaylandInterface::XdgDialogV1));
    QVERIFY(Test::waitForWaylandPointer());

    workspace()->setActiveOutput(QPoint(640, 512));
    // put mouse in the middle of screen one
    KWin::input()->pointer()->warp(QPoint(640, 512));
}

void TestXdgShellWindow::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestXdgShellWindow::testMapUnmap()
{
    // This test verifies that the compositor destroys XdgToplevelWindow when the
    // associated xdg_toplevel surface is unmapped.

    // Create a wl_surface and an xdg_toplevel, but don't commit them yet!
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));

    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);

    QSignalSpy configureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);

    // Tell the compositor that we want to map the surface.
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // The compositor will respond with a configure event.
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);

    // Now we can attach a buffer with actual data to the surface.
    Test::render(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(windowAddedSpy.wait());
    QCOMPARE(windowAddedSpy.count(), 1);
    Window *window = windowAddedSpy.last().first().value<Window *>();
    QVERIFY(window);
    QCOMPARE(window->readyForPainting(), true);

    // When the window becomes active, the compositor will send another configure event.
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 2);

    // Unmap the xdg_toplevel surface by committing a null buffer.
    surface->attachBuffer(KWayland::Client::Buffer::Ptr());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(Test::waitForWindowClosed(window));

    // Tell the compositor that we want to re-map the xdg_toplevel surface.
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // The compositor will respond with a configure event.
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 3);

    // Now we can attach a buffer with actual data to the surface.
    Test::render(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(windowAddedSpy.wait());
    QCOMPARE(windowAddedSpy.count(), 2);
    window = windowAddedSpy.last().first().value<Window *>();
    QVERIFY(window);
    QCOMPARE(window->readyForPainting(), true);

    // The compositor will respond with a configure event.
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 4);

    // Destroy the test window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void TestXdgShellWindow::testWindowOutputs()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto size = QSize(200, 200);

    QSignalSpy outputEnteredSpy(surface.get(), &KWayland::Client::Surface::outputEntered);
    QSignalSpy outputLeftSpy(surface.get(), &KWayland::Client::Surface::outputLeft);

    auto window = Test::renderAndWaitForShown(surface.get(), size, Qt::blue);
    // assumption: window is initially placed on first screen
    QVERIFY(outputEnteredSpy.wait());
    QCOMPARE(outputEnteredSpy.count(), 1);
    QCOMPARE(surface->outputs().count(), 1);
    QCOMPARE(surface->outputs().first()->globalPosition(), QPoint(0, 0));

    // move to overlapping both first and second screen
    window->moveResize(QRect(QPoint(1250, 100), size));
    QVERIFY(outputEnteredSpy.wait());
    QCOMPARE(outputEnteredSpy.count(), 2);
    QCOMPARE(outputLeftSpy.count(), 0);
    QCOMPARE(surface->outputs().count(), 2);
    QVERIFY(surface->outputs()[0] != surface->outputs()[1]);

    // move entirely into second screen
    window->moveResize(QRect(QPoint(1400, 100), size));
    QVERIFY(outputLeftSpy.wait());
    QCOMPARE(outputEnteredSpy.count(), 2);
    QCOMPARE(outputLeftSpy.count(), 1);
    QCOMPARE(surface->outputs().count(), 1);
    QCOMPARE(surface->outputs().first()->globalPosition(), QPoint(1280, 0));
}

void TestXdgShellWindow::testMinimizeActiveWindow()
{
    // this test verifies that when minimizing the active window it gets deactivated
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<QObject> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QCOMPARE(workspace()->activeWindow(), window);
    QVERIFY(window->wantsInput());
    QVERIFY(window->wantsTabFocus());
    QVERIFY(window->isShown());

    workspace()->slotWindowMinimize();
    QVERIFY(!window->isShown());
    QVERIFY(window->wantsInput());
    QVERIFY(window->wantsTabFocus());
    QVERIFY(!window->isActive());
    QVERIFY(!workspace()->activeWindow());
    QVERIFY(window->isMinimized());

    // unminimize again
    window->setMinimized(false);
    QVERIFY(!window->isMinimized());
    QVERIFY(!window->isActive());
    QVERIFY(window->wantsInput());
    QVERIFY(window->wantsTabFocus());
    QVERIFY(window->isShown());
    QCOMPARE(workspace()->activeWindow(), nullptr);
}

void TestXdgShellWindow::testFullscreen_data()
{
    QTest::addColumn<Test::XdgToplevelDecorationV1::mode>("decoMode");

    QTest::newRow("client-side deco") << Test::XdgToplevelDecorationV1::mode_client_side;
    QTest::newRow("server-side deco") << Test::XdgToplevelDecorationV1::mode_server_side;
}

void TestXdgShellWindow::testFullscreen()
{
    // this test verifies that a window can be properly fullscreened

    Test::XdgToplevel::States states;

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    std::unique_ptr<Test::XdgToplevelDecorationV1> decoration(Test::createXdgToplevelDecorationV1(shellSurface.get()));
    QSignalSpy decorationConfigureRequestedSpy(decoration.get(), &Test::XdgToplevelDecorationV1::configureRequested);
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);

    // Initialize the xdg-toplevel surface.
    QFETCH(Test::XdgToplevelDecorationV1::mode, decoMode);
    decoration->set_mode(decoMode);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(500, 250), Qt::blue);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QCOMPARE(window->layer(), NormalLayer);
    QVERIFY(!window->isFullScreen());
    QCOMPARE(window->clientSize(), QSize(500, 250));
    QCOMPARE(window->isDecorated(), decoMode == Test::XdgToplevelDecorationV1::mode_server_side);
    QCOMPARE(window->clientSizeToFrameSize(window->clientSize()), window->size());

    QSignalSpy fullScreenChangedSpy(window, &Window::fullScreenChanged);
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);

    // Wait for the compositor to send a configure event with the Activated state.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states & Test::XdgToplevel::State::Activated);

    // Ask the compositor to show the window in full screen mode.
    shellSurface->set_fullscreen(nullptr);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states & Test::XdgToplevel::State::Fullscreen);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), window->output()->geometry().size());

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::red);

    QVERIFY(fullScreenChangedSpy.wait());
    QCOMPARE(fullScreenChangedSpy.count(), 1);
    QVERIFY(window->isFullScreen());
    QVERIFY(!window->isDecorated());
    QCOMPARE(window->layer(), NormalLayer);
    QCOMPARE(window->frameGeometry(), QRect(QPoint(0, 0), window->output()->geometry().size()));

    // Ask the compositor to show the window in normal mode.
    shellSurface->unset_fullscreen();
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 4);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!(states & Test::XdgToplevel::State::Fullscreen));
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), QSize(500, 250));

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::blue);

    QVERIFY(fullScreenChangedSpy.wait());
    QCOMPARE(fullScreenChangedSpy.count(), 2);
    QCOMPARE(window->clientSize(), QSize(500, 250));
    QVERIFY(!window->isFullScreen());
    QCOMPARE(window->isDecorated(), decoMode == Test::XdgToplevelDecorationV1::mode_server_side);
    QCOMPARE(window->layer(), NormalLayer);

    // Destroy the window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void TestXdgShellWindow::testUserCanSetFullscreen()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QVERIFY(!window->isFullScreen());
    QVERIFY(window->isFullScreenable());
}

void TestXdgShellWindow::testSendFullScreenWindowToAnotherOutput()
{
    // This test verifies that the fullscreen window will have correct geometry restore
    // after it's sent to another output.

    const auto outputs = workspace()->outputs();

    // Create the window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // Wait for the compositor to send a configure event with the activated state.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    // Move the window to the left monitor.
    window->move(QPointF(10, 20));
    QCOMPARE(window->frameGeometry(), QRectF(10, 20, 100, 50));
    QCOMPARE(window->output(), outputs[0]);

    // Make the window fullscreen.
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    shellSurface->set_fullscreen(nullptr);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->isFullScreen(), true);
    QCOMPARE(window->frameGeometry(), QRectF(0, 0, 1280, 1024));
    QCOMPARE(window->fullscreenGeometryRestore(), QRectF(10, 20, 100, 50));
    QCOMPARE(window->output(), outputs[0]);

    // Send the window to another output.
    window->sendToOutput(outputs[1]);
    QCOMPARE(window->isFullScreen(), true);
    QCOMPARE(window->frameGeometry(), QRectF(1280, 0, 1280, 1024));
    QCOMPARE(window->fullscreenGeometryRestore(), QRectF(1280 + 10, 20, 100, 50));
    QCOMPARE(window->output(), outputs[1]);
}

void TestXdgShellWindow::testMaximizedToFullscreen_data()
{
    QTest::addColumn<Test::XdgToplevelDecorationV1::mode>("decoMode");

    QTest::newRow("client-side deco") << Test::XdgToplevelDecorationV1::mode_client_side;
    QTest::newRow("server-side deco") << Test::XdgToplevelDecorationV1::mode_server_side;
}

void TestXdgShellWindow::testMaximizedToFullscreen()
{
    // this test verifies that a window can be properly fullscreened after maximizing

    Test::XdgToplevel::States states;

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    std::unique_ptr<Test::XdgToplevelDecorationV1> decoration(Test::createXdgToplevelDecorationV1(shellSurface.get()));
    QSignalSpy decorationConfigureRequestedSpy(decoration.get(), &Test::XdgToplevelDecorationV1::configureRequested);
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);

    // Initialize the xdg-toplevel surface.
    QFETCH(Test::XdgToplevelDecorationV1::mode, decoMode);
    decoration->set_mode(decoMode);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QVERIFY(!window->isFullScreen());
    QCOMPARE(window->clientSize(), QSize(100, 50));
    QCOMPARE(window->isDecorated(), decoMode == Test::XdgToplevelDecorationV1::mode_server_side);

    QSignalSpy fullscreenChangedSpy(window, &Window::fullScreenChanged);
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);

    // Wait for the compositor to send a configure event with the Activated state.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states & Test::XdgToplevel::State::Activated);

    // Ask the compositor to maximize the window.
    shellSurface->set_maximized();
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states & Test::XdgToplevel::State::Maximized);

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->maximizeMode(), MaximizeFull);

    // Ask the compositor to show the window in full screen mode.
    shellSurface->set_fullscreen(nullptr);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 4);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), window->output()->geometry().size());
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states & Test::XdgToplevel::State::Maximized);
    QVERIFY(states & Test::XdgToplevel::State::Fullscreen);

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::red);

    QVERIFY(fullscreenChangedSpy.wait());
    QCOMPARE(fullscreenChangedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QVERIFY(window->isFullScreen());
    QVERIFY(!window->isDecorated());

    // Switch back to normal mode.
    shellSurface->unset_fullscreen();
    shellSurface->unset_maximized();
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 5);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), QSize(100, 50));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!(states & Test::XdgToplevel::State::Maximized));
    QVERIFY(!(states & Test::XdgToplevel::State::Fullscreen));

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::red);

    QVERIFY(frameGeometryChangedSpy.wait());
    QVERIFY(!window->isFullScreen());
    QCOMPARE(window->isDecorated(), decoMode == Test::XdgToplevelDecorationV1::mode_server_side);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);

    // Destroy the window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void TestXdgShellWindow::testFullscreenMultipleOutputs()
{
    // this test verifies that kwin will place fullscreen windows in the outputs its instructed to

    const auto outputs = workspace()->outputs();
    for (KWin::Output *output : outputs) {
        Test::XdgToplevel::States states;

        std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
        QVERIFY(surface);
        std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
        QVERIFY(shellSurface);

        auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
        QVERIFY(window);
        QVERIFY(window->isActive());
        QVERIFY(!window->isFullScreen());
        QCOMPARE(window->clientSize(), QSize(100, 50));
        QVERIFY(!window->isDecorated());

        QSignalSpy fullscreenChangedSpy(window, &Window::fullScreenChanged);
        QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
        QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
        QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);

        // Wait for the compositor to send a configure event with the Activated state.
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);
        states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
        QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));

        // Ask the compositor to show the window in full screen mode.
        shellSurface->set_fullscreen(*Test::waylandOutput(output->name()));
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
        QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), output->geometry().size());

        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
        Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::red);

        QVERIFY(!fullscreenChangedSpy.isEmpty() || fullscreenChangedSpy.wait());
        QCOMPARE(fullscreenChangedSpy.count(), 1);

        QVERIFY(!frameGeometryChangedSpy.isEmpty() || frameGeometryChangedSpy.wait());

        QVERIFY(window->isFullScreen());

        QCOMPARE(window->frameGeometry(), output->geometry());
    }
}

void TestXdgShellWindow::testHidden()
{
    // this test verifies that when hiding window it doesn't get shown
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<QObject> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QCOMPARE(workspace()->activeWindow(), window);
    QVERIFY(window->wantsInput());
    QVERIFY(window->wantsTabFocus());
    QVERIFY(window->isShown());

    window->setHidden(true);
    QVERIFY(!window->isShown());
    QVERIFY(!window->isActive());
    QVERIFY(window->wantsInput());
    QVERIFY(window->wantsTabFocus());

    // unhide again
    window->setHidden(false);
    QVERIFY(window->isShown());
    QVERIFY(window->wantsInput());
    QVERIFY(window->wantsTabFocus());

    // QCOMPARE(workspace()->activeClient(), c);
}

void TestXdgShellWindow::testDesktopFileName()
{
    QIcon::setThemeName(QStringLiteral("breeze"));
    // this test verifies that desktop file name is passed correctly to the window
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    // only xdg-shell as ShellSurface misses the setter
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QCOMPARE(window->desktopFileName(), QStringLiteral("org.kde.foo"));
    QCOMPARE(window->resourceClass(), QStringLiteral("org.kde.foo"));
    QVERIFY(window->resourceName().startsWith("testXdgShellWindow"));
    // the desktop file does not exist, so icon should be generic Wayland
    QCOMPARE(window->icon().name(), QStringLiteral("wayland"));

    QSignalSpy desktopFileNameChangedSpy(window, &Window::desktopFileNameChanged);
    QSignalSpy iconChangedSpy(window, &Window::iconChanged);
    shellSurface->set_app_id(QStringLiteral("org.kde.bar"));
    QVERIFY(desktopFileNameChangedSpy.wait());
    QCOMPARE(window->desktopFileName(), QStringLiteral("org.kde.bar"));
    QCOMPARE(window->resourceClass(), QStringLiteral("org.kde.bar"));
    QVERIFY(window->resourceName().startsWith("testXdgShellWindow"));
    // icon should still be wayland
    QCOMPARE(window->icon().name(), QStringLiteral("wayland"));
    QVERIFY(iconChangedSpy.isEmpty());

    const QString dfPath = QFINDTESTDATA("data/example.desktop");
    shellSurface->set_app_id(dfPath.toUtf8());
    QVERIFY(desktopFileNameChangedSpy.wait());
    QCOMPARE(iconChangedSpy.count(), 1);
    QCOMPARE(window->desktopFileName(), dfPath);
    QCOMPARE(window->icon().name(), QStringLiteral("kwin"));
}

void TestXdgShellWindow::testCaptionSimplified()
{
    // this test verifies that caption is properly trimmed
    // see BUG 323798 comment #12
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    // only done for xdg-shell as ShellSurface misses the setter
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    const QString origTitle = QString::fromUtf8(QByteArrayLiteral("Was tun, wenn Schüler Autismus haben?\342\200\250\342\200\250\342\200\250 – Marlies Hübner - Mozilla Firefox"));
    shellSurface->set_title(origTitle);
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QVERIFY(window->caption() != origTitle);
    QCOMPARE(window->caption(), origTitle.simplified());
}

void TestXdgShellWindow::testUnresponsiveWindow_data()
{
    QTest::addColumn<QString>("shellInterface"); // see env selection in qwaylandintegration.cpp
    QTest::addColumn<bool>("socketMode");

    QTest::newRow("xdg display") << "xdg-shell" << false;
    QTest::newRow("xdg socket") << "xdg-shell" << true;
}

void TestXdgShellWindow::testUnresponsiveWindow()
{
    // this test verifies that killWindow properly terminates a process
    // for this an external binary is launched
    const QString kill = QFINDTESTDATA(QStringLiteral("kill"));
    QVERIFY(!kill.isEmpty());
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);

    std::unique_ptr<QProcess> process(new QProcess);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    QFETCH(QString, shellInterface);
    QFETCH(bool, socketMode);
    env.insert("QT_WAYLAND_SHELL_INTEGRATION", shellInterface);
    if (socketMode) {
        int sx[2];
        QVERIFY(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) >= 0);
        waylandServer()->display()->createClient(sx[0]);
        int socket = dup(sx[1]);
        QVERIFY(socket != -1);
        env.insert(QStringLiteral("WAYLAND_SOCKET"), QByteArray::number(socket));
        env.remove("WAYLAND_DISPLAY");
    } else {
        env.insert("WAYLAND_DISPLAY", s_socketName);
    }
    process->setProcessEnvironment(env);
    process->setProcessChannelMode(QProcess::ForwardedChannels);
    process->setProgram(kill);
    QSignalSpy processStartedSpy{process.get(), &QProcess::started};
    process->start();
    QVERIFY(processStartedSpy.wait());

    Window *killWindow = nullptr;
    if (windowAddedSpy.isEmpty()) {
        QVERIFY(windowAddedSpy.wait());
    }
    ::kill(process->processId(), SIGUSR1); // send a signal to freeze the process

    killWindow = windowAddedSpy.first().first().value<Window *>();
    QVERIFY(killWindow);
    QSignalSpy unresponsiveSpy(killWindow, &Window::unresponsiveChanged);
    QSignalSpy killedSpy(process.get(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished));
    QSignalSpy deletedSpy(killWindow, &QObject::destroyed);

    qint64 startTime = QDateTime::currentMSecsSinceEpoch();

    // wait for the process to be frozen
    QTest::qWait(10);

    // pretend the user clicked the close button
    killWindow->closeWindow();

    // window should not yet be marked unresponsive nor killed
    QVERIFY(!killWindow->unresponsive());
    QVERIFY(killedSpy.isEmpty());

    QVERIFY(unresponsiveSpy.wait());
    // window should be marked unresponsive but not killed
    auto elapsed1 = QDateTime::currentMSecsSinceEpoch() - startTime;
    const int timeout = options->killPingTimeout() / 2; // first timeout at half the time is for "unresponsive".
    QVERIFY(elapsed1 > timeout - 200 && elapsed1 < timeout + 200); // coarse timers on a test across two processes means we need a fuzzy compare
    QVERIFY(killWindow->unresponsive());
    QVERIFY(killedSpy.isEmpty());

    // TODO verify that kill prompt works.
    killWindow->killWindow();
    process->kill();

    QVERIFY(killedSpy.wait());

    if (deletedSpy.isEmpty()) {
        QVERIFY(deletedSpy.wait());
    }
}

void TestXdgShellWindow::testAppMenu()
{
    // register a faux appmenu client
    QVERIFY(QDBusConnection::sessionBus().registerService("org.kde.kappmenu"));

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    std::unique_ptr<KWayland::Client::AppMenu> menu(Test::waylandAppMenuManager()->create(surface.get()));
    QSignalSpy spy(window, &Window::hasApplicationMenuChanged);
    menu->setAddress("service.name", "object/path");
    spy.wait();
    QCOMPARE(window->hasApplicationMenu(), true);
    QCOMPARE(window->applicationMenuServiceName(), QString("service.name"));
    QCOMPARE(window->applicationMenuObjectPath(), QString("object/path"));

    QVERIFY(QDBusConnection::sessionBus().unregisterService("org.kde.kappmenu"));
}

void TestXdgShellWindow::testSendClientWithTransientToDesktop()
{
    // this test verifies that when sending a window to a desktop all transients are also send to that desktop

    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    vds->setCount(2);
    const QList<VirtualDesktop *> desktops = vds->desktops();

    std::unique_ptr<KWayland::Client::Surface> surface{Test::createSurface()};
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));

    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // let's create a transient window
    std::unique_ptr<KWayland::Client::Surface> transientSurface{Test::createSurface()};
    std::unique_ptr<Test::XdgToplevel> transientShellSurface(Test::createXdgToplevelSurface(transientSurface.get()));
    transientShellSurface->set_parent(shellSurface->object());

    auto transient = Test::renderAndWaitForShown(transientSurface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(transient);
    QCOMPARE(workspace()->activeWindow(), transient);
    QCOMPARE(transient->transientFor(), window);
    QVERIFY(window->transients().contains(transient));

    // initially, the parent and the transient are on the first virtual desktop
    QCOMPARE(window->desktops(), QList<VirtualDesktop *>{desktops[0]});
    QVERIFY(!window->isOnAllDesktops());
    QCOMPARE(transient->desktops(), QList<VirtualDesktop *>{desktops[0]});
    QVERIFY(!transient->isOnAllDesktops());

    // send the transient to the second virtual desktop
    workspace()->slotWindowToDesktop(desktops[1]);
    QCOMPARE(window->desktops(), QList<VirtualDesktop *>{desktops[0]});
    QCOMPARE(transient->desktops(), QList<VirtualDesktop *>{desktops[1]});

    // activate c
    workspace()->activateWindow(window);
    QCOMPARE(workspace()->activeWindow(), window);
    QVERIFY(window->isActive());

    // and send it to the desktop it's already on
    QCOMPARE(window->desktops(), QList<VirtualDesktop *>{desktops[0]});
    QCOMPARE(transient->desktops(), QList<VirtualDesktop *>{desktops[1]});
    workspace()->slotWindowToDesktop(desktops[0]);

    // which should move the transient back to the desktop
    QCOMPARE(window->desktops(), QList<VirtualDesktop *>{desktops[0]});
    QCOMPARE(transient->desktops(), QList<VirtualDesktop *>{desktops[0]});
}

void TestXdgShellWindow::testMinimizeWindowWithTransients()
{
    // this test verifies that when minimizing/unminimizing a window all its
    // transients will be minimized/unminimized as well

    // create the main window
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QVERIFY(!window->isMinimized());

    // create a transient window
    std::unique_ptr<KWayland::Client::Surface> transientSurface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> transientShellSurface(Test::createXdgToplevelSurface(transientSurface.get()));
    transientShellSurface->set_parent(shellSurface->object());
    auto transient = Test::renderAndWaitForShown(transientSurface.get(), QSize(100, 50), Qt::red);
    QVERIFY(transient);
    QVERIFY(!transient->isMinimized());
    QCOMPARE(transient->transientFor(), window);
    QVERIFY(window->hasTransient(transient, false));

    // minimize the main window, the transient should be minimized as well
    window->setMinimized(true);
    QVERIFY(window->isMinimized());
    QVERIFY(transient->isMinimized());

    // unminimize the main window, the transient should be unminimized as well
    window->setMinimized(false);
    QVERIFY(!window->isMinimized());
    QVERIFY(!transient->isMinimized());
}

void TestXdgShellWindow::testXdgDecoration_data()
{
    QTest::addColumn<Test::XdgToplevelDecorationV1::mode>("requestedMode");
    QTest::addColumn<Test::XdgToplevelDecorationV1::mode>("expectedMode");

    QTest::newRow("client side requested") << Test::XdgToplevelDecorationV1::mode_client_side << Test::XdgToplevelDecorationV1::mode_client_side;
    QTest::newRow("server side requested") << Test::XdgToplevelDecorationV1::mode_server_side << Test::XdgToplevelDecorationV1::mode_server_side;
}

void TestXdgShellWindow::testXdgDecoration()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    std::unique_ptr<Test::XdgToplevelDecorationV1> deco(Test::createXdgToplevelDecorationV1(shellSurface.get()));

    QSignalSpy decorationConfigureRequestedSpy(deco.get(), &Test::XdgToplevelDecorationV1::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);

    QFETCH(Test::XdgToplevelDecorationV1::mode, requestedMode);
    QFETCH(Test::XdgToplevelDecorationV1::mode, expectedMode);

    // request a mode
    deco->set_mode(requestedMode);

    // kwin will send a configure
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    QCOMPARE(decorationConfigureRequestedSpy.count(), 1);
    QCOMPARE(decorationConfigureRequestedSpy.last()[0].value<Test::XdgToplevelDecorationV1::mode>(), expectedMode);
    QVERIFY(decorationConfigureRequestedSpy.count() > 0);

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last()[0].toInt());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QCOMPARE(window->isDecorated(), expectedMode == Test::XdgToplevelDecorationV1::mode_server_side);
}

void TestXdgShellWindow::testXdgNeverCommitted()
{
    // check we don't crash if we create a shell object but delete the XdgShellClient before committing it
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
}

void TestXdgShellWindow::testXdgInitialState()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    surfaceConfigureRequestedSpy.wait();
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    const auto size = toplevelConfigureRequestedSpy.first()[0].value<QSize>();

    QCOMPARE(size, QSize(0, 0)); // window should chose it's preferred size

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.first()[0].toUInt());

    auto window = Test::renderAndWaitForShown(surface.get(), QSize(200, 100), Qt::blue);
    QCOMPARE(window->size(), QSize(200, 100));
}

void TestXdgShellWindow::testXdgInitiallyMaximised()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);

    shellSurface->set_maximized();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    surfaceConfigureRequestedSpy.wait();

    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    const auto size = toplevelConfigureRequestedSpy.first()[0].value<QSize>();
    const auto state = toplevelConfigureRequestedSpy.first()[1].value<Test::XdgToplevel::States>();

    QCOMPARE(size, QSize(1280, 1024));
    QVERIFY(state & Test::XdgToplevel::State::Maximized);

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.first()[0].toUInt());

    auto window = Test::renderAndWaitForShown(surface.get(), size, Qt::blue);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->size(), QSize(1280, 1024));
}

void TestXdgShellWindow::testXdgInitiallyFullscreen()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);

    shellSurface->set_fullscreen(nullptr);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    surfaceConfigureRequestedSpy.wait();

    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    const auto size = toplevelConfigureRequestedSpy.first()[0].value<QSize>();
    const auto state = toplevelConfigureRequestedSpy.first()[1].value<Test::XdgToplevel::States>();

    QCOMPARE(size, QSize(1280, 1024));
    QVERIFY(state & Test::XdgToplevel::State::Fullscreen);

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.first()[0].toUInt());

    auto window = Test::renderAndWaitForShown(surface.get(), size, Qt::blue);
    QCOMPARE(window->isFullScreen(), true);
    QCOMPARE(window->size(), QSize(1280, 1024));
}

void TestXdgShellWindow::testXdgInitiallyMinimized()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    shellSurface->set_minimized();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    surfaceConfigureRequestedSpy.wait();
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    const auto size = toplevelConfigureRequestedSpy.first()[0].value<QSize>();
    const auto state = toplevelConfigureRequestedSpy.first()[1].value<Test::XdgToplevel::States>();

    QCOMPARE(size, QSize(0, 0));
    QCOMPARE(state, 0);

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.first()[0].toUInt());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QVERIFY(window->isMinimized());
}

void TestXdgShellWindow::testXdgWindowGeometryIsntSet()
{
    // This test verifies that the effective window geometry corresponds to the
    // bounding rectangle of the main surface and its sub-surfaces if no window
    // geometry is set by the window.

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(200, 100), Qt::red);
    QVERIFY(window);
    QCOMPARE(window->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(window->frameGeometry().size(), QSize(200, 100));

    const QPointF oldPosition = window->pos();

    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    Test::render(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry().topLeft(), oldPosition);
    QCOMPARE(window->frameGeometry().size(), QSize(100, 50));
    QCOMPARE(window->bufferGeometry().topLeft(), oldPosition);
    QCOMPARE(window->bufferGeometry().size(), QSize(100, 50));

    std::unique_ptr<KWayland::Client::Surface> childSurface(Test::createSurface());
    std::unique_ptr<KWayland::Client::SubSurface> subSurface(Test::createSubSurface(childSurface.get(), surface.get()));
    QVERIFY(subSurface);
    subSurface->setPosition(QPoint(-20, -10));
    Test::render(childSurface.get(), QSize(100, 50), Qt::blue);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry().topLeft(), oldPosition);
    QCOMPARE(window->frameGeometry().size(), QSize(120, 60));
    QCOMPARE(window->bufferGeometry().topLeft(), oldPosition + QPoint(20, 10));
    QCOMPARE(window->bufferGeometry().size(), QSize(100, 50));
}

void TestXdgShellWindow::testXdgWindowGeometryAttachBuffer()
{
    // This test verifies that the effective window geometry remains the same when
    // a new buffer is attached and xdg_surface.set_window_geometry is not called
    // again. Notice that the window geometry must remain the same even if the new
    // buffer is smaller.

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(200, 100), Qt::red);
    QVERIFY(window);
    QCOMPARE(window->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(window->frameGeometry().size(), QSize(200, 100));

    const QPointF oldPosition = window->pos();

    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    shellSurface->xdgSurface()->set_window_geometry(10, 10, 180, 80);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(frameGeometryChangedSpy.count(), 1);
    QCOMPARE(window->frameGeometry().topLeft(), oldPosition);
    QCOMPARE(window->frameGeometry().size(), QSize(180, 80));
    QCOMPARE(window->bufferGeometry().topLeft(), oldPosition - QPoint(10, 10));
    QCOMPARE(window->bufferGeometry().size(), QSize(200, 100));

    Test::render(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(frameGeometryChangedSpy.count(), 2);
    QCOMPARE(window->frameGeometry().topLeft(), oldPosition);
    QCOMPARE(window->frameGeometry().size(), QSize(90, 40));
    QCOMPARE(window->bufferGeometry().topLeft(), oldPosition - QPoint(10, 10));
    QCOMPARE(window->bufferGeometry().size(), QSize(100, 50));

    shellSurface->xdgSurface()->set_window_geometry(0, 0, 100, 50);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(frameGeometryChangedSpy.count(), 3);
    QCOMPARE(window->frameGeometry().topLeft(), oldPosition);
    QCOMPARE(window->frameGeometry().size(), QSize(100, 50));
    QCOMPARE(window->bufferGeometry().topLeft(), oldPosition);
    QCOMPARE(window->bufferGeometry().size(), QSize(100, 50));

    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void TestXdgShellWindow::testXdgWindowGeometryAttachSubSurface()
{
    // This test verifies that the effective window geometry remains the same
    // when a new sub-surface is added and xdg_surface.set_window_geometry is
    // not called again.

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(200, 100), Qt::red);
    QVERIFY(window);
    QCOMPARE(window->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(window->frameGeometry().size(), QSize(200, 100));

    const QPointF oldPosition = window->pos();

    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    shellSurface->xdgSurface()->set_window_geometry(10, 10, 180, 80);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry().topLeft(), oldPosition);
    QCOMPARE(window->frameGeometry().size(), QSize(180, 80));
    QCOMPARE(window->bufferGeometry().topLeft(), oldPosition - QPoint(10, 10));
    QCOMPARE(window->bufferGeometry().size(), QSize(200, 100));

    std::unique_ptr<KWayland::Client::Surface> childSurface(Test::createSurface());
    std::unique_ptr<KWayland::Client::SubSurface> subSurface(Test::createSubSurface(childSurface.get(), surface.get()));
    QVERIFY(subSurface);
    subSurface->setPosition(QPoint(-20, -20));
    Test::render(childSurface.get(), QSize(100, 50), Qt::blue);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QCOMPARE(window->frameGeometry().topLeft(), oldPosition);
    QCOMPARE(window->frameGeometry().size(), QSize(180, 80));
    QCOMPARE(window->bufferGeometry().topLeft(), oldPosition - QPoint(10, 10));
    QCOMPARE(window->bufferGeometry().size(), QSize(200, 100));

    shellSurface->xdgSurface()->set_window_geometry(-15, -15, 50, 40);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry().topLeft(), oldPosition);
    QCOMPARE(window->frameGeometry().size(), QSize(50, 40));
    QCOMPARE(window->bufferGeometry().topLeft(), oldPosition - QPoint(-15, -15));
    QCOMPARE(window->bufferGeometry().size(), QSize(200, 100));
}

void TestXdgShellWindow::testXdgWindowGeometryInteractiveResize()
{
    // This test verifies that correct window geometry is provided along each
    // configure event when an xdg-shell is being interactively resized.

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(200, 100), Qt::red);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QCOMPARE(window->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(window->frameGeometry().size(), QSize(200, 100));

    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    shellSurface->xdgSurface()->set_window_geometry(10, 10, 180, 80);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(window->frameGeometry().size(), QSize(180, 80));

    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &Window::interactiveMoveResizeStepped);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);

    // Start interactively resizing the window.
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    Test::XdgToplevel::States states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));

    // Go right.
    QPointF cursorPos = KWin::Cursors::self()->mouse()->pos();
    window->keyPressEvent(Qt::Key_Right);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(188, 80));
    shellSurface->xdgSurface()->set_window_geometry(10, 10, 188, 80);
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(208, 100), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->bufferGeometry().size(), QSize(208, 100));
    QCOMPARE(window->frameGeometry().size(), QSize(188, 80));

    // Go down.
    cursorPos = KWin::Cursors::self()->mouse()->pos();
    window->keyPressEvent(Qt::Key_Down);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(0, 8));
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 4);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(188, 88));
    shellSurface->xdgSurface()->set_window_geometry(10, 10, 188, 88);
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(208, 108), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 2);
    QCOMPARE(window->bufferGeometry().size(), QSize(208, 108));
    QCOMPARE(window->frameGeometry().size(), QSize(188, 88));

    // Finish resizing the window.
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 5);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Resizing));

    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void TestXdgShellWindow::testXdgWindowGeometryFullScreen()
{
    // This test verifies that an xdg-shell receives correct window geometry when
    // its fullscreen state gets changed.

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(200, 100), Qt::red);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QCOMPARE(window->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(window->frameGeometry().size(), QSize(200, 100));

    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    shellSurface->xdgSurface()->set_window_geometry(10, 10, 180, 80);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(window->frameGeometry().size(), QSize(180, 80));

    workspace()->slotWindowFullScreen();
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(1280, 1024));
    Test::XdgToplevel::States states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Fullscreen));
    shellSurface->xdgSurface()->set_window_geometry(0, 0, 1280, 1024);
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(1280, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->bufferGeometry().size(), QSize(1280, 1024));
    QCOMPARE(window->frameGeometry().size(), QSize(1280, 1024));

    workspace()->slotWindowFullScreen();
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(180, 80));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Fullscreen));
    shellSurface->xdgSurface()->set_window_geometry(10, 10, 180, 80);
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(200, 100), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(window->frameGeometry().size(), QSize(180, 80));

    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void TestXdgShellWindow::testXdgWindowGeometryMaximize()
{
    // This test verifies that an xdg-shell receives correct window geometry when
    // its maximized state gets changed.

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(200, 100), Qt::red);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QCOMPARE(window->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(window->frameGeometry().size(), QSize(200, 100));

    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    shellSurface->xdgSurface()->set_window_geometry(10, 10, 180, 80);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(window->frameGeometry().size(), QSize(180, 80));

    workspace()->slotWindowMaximize();
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(1280, 1024));
    Test::XdgToplevel::States states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));
    shellSurface->xdgSurface()->set_window_geometry(0, 0, 1280, 1024);
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(1280, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->bufferGeometry().size(), QSize(1280, 1024));
    QCOMPARE(window->frameGeometry().size(), QSize(1280, 1024));

    workspace()->slotWindowMaximize();
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(180, 80));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));
    shellSurface->xdgSurface()->set_window_geometry(10, 10, 180, 80);
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(200, 100), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(window->frameGeometry().size(), QSize(180, 80));

    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void TestXdgShellWindow::testPointerInputTransform()
{
    // This test verifies that XdgToplevelWindow provides correct input transform matrix.
    // The input transform matrix is used by seat to map pointer events from the global
    // screen coordinates to the surface-local coordinates.

    // Get a wl_pointer object on the client side.
    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy pointerEnteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy pointerMotionSpy(pointer.get(), &KWayland::Client::Pointer::motion);

    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(200, 100), Qt::red);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QCOMPARE(window->bufferGeometry().size(), QSize(200, 100));
    QCOMPARE(window->frameGeometry().size(), QSize(200, 100));

    // Enter the surface.
    quint32 timestamp = 0;
    Test::pointerMotion(window->pos(), timestamp++);
    QVERIFY(pointerEnteredSpy.wait());

    // Move the pointer to (10, 5) relative to the upper left frame corner, which is located
    // at (0, 0) in the surface-local coordinates.
    Test::pointerMotion(window->pos() + QPointF(10, 5), timestamp++);
    QVERIFY(pointerMotionSpy.wait());
    QCOMPARE(pointerMotionSpy.last().first().toPointF(), QPointF(10, 5));

    // Let's pretend that the window has changed the extents of the client-side drop-shadow
    // but the frame geometry didn't change.
    QSignalSpy bufferGeometryChangedSpy(window, &Window::bufferGeometryChanged);
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    shellSurface->xdgSurface()->set_window_geometry(10, 20, 200, 100);
    Test::render(surface.get(), QSize(220, 140), Qt::blue);
    QVERIFY(bufferGeometryChangedSpy.wait());
    QCOMPARE(frameGeometryChangedSpy.count(), 0);
    QCOMPARE(window->frameGeometry().size(), QSize(200, 100));
    QCOMPARE(window->bufferGeometry().size(), QSize(220, 140));

    // Move the pointer to (20, 50) relative to the upper left frame corner, which is located
    // at (10, 20) in the surface-local coordinates.
    Test::pointerMotion(window->pos() + QPointF(20, 50), timestamp++);
    QVERIFY(pointerMotionSpy.wait());
    QCOMPARE(pointerMotionSpy.last().first().toPointF(), QPointF(10, 20) + QPointF(20, 50));

    // Destroy the xdg-toplevel surface.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void TestXdgShellWindow::testReentrantSetFrameGeometry()
{
    // This test verifies that calling moveResize() from a slot connected directly
    // to the frameGeometryChanged() signal won't cause an infinite recursion.

    // Create an xdg-toplevel surface and wait for the compositor to catch up.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(200, 100), Qt::red);
    QVERIFY(window);
    QCOMPARE(window->pos(), QPoint(0, 0));

    // Let's pretend that there is a script that really wants the window to be at (100, 100).
    connect(window, &Window::frameGeometryChanged, this, [window]() {
        window->moveResize(QRectF(QPointF(100, 100), window->size()));
    });

    // Trigger the lambda above.
    window->move(QPoint(40, 50));

    // Eventually, the window will end up at (100, 100).
    QCOMPARE(window->pos(), QPoint(100, 100));

    // Destroy the xdg-toplevel surface.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void TestXdgShellWindow::testDoubleMaximize()
{
    // This test verifies that the case where a window issues two set_maximized() requests
    // separated by the initial commit is handled properly.

    // Create the test surface.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    shellSurface->set_maximized();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to respond with a configure event.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    QSize size = toplevelConfigureRequestedSpy.last().at(0).toSize();
    QCOMPARE(size, QSize(1280, 1024));
    Test::XdgToplevel::States states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Send another set_maximized() request, but do not attach any buffer yet.
    shellSurface->set_maximized();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // The compositor must respond with another configure event even if the state hasn't changed.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    size = toplevelConfigureRequestedSpy.last().at(0).toSize();
    QCOMPARE(size, QSize(1280, 1024));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));
}

void TestXdgShellWindow::testDoubleFullscreenSeparatedByCommit()
{
    // Some applications do weird things at startup and this is one of them. This test verifies
    // that the window will have good frame geometry if the window has issued several
    // xdg_toplevel.set_fullscreen requests and they are separated by a surface commit with
    // no attached buffer.

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);

    // Tell the compositor that we want the window to be shown in fullscreen mode.
    shellSurface->set_fullscreen(nullptr);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), QSize(1280, 1024));
    QVERIFY(toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>() & Test::XdgToplevel::State::Fullscreen);

    // Ask again.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    shellSurface->set_fullscreen(nullptr);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), QSize(1280, 1024));
    QVERIFY(toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>() & Test::XdgToplevel::State::Fullscreen);

    // Map the window.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(1280, 1024), Qt::blue);
    QVERIFY(window->isFullScreen());
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 1280, 1024));
}

void TestXdgShellWindow::testMaximizeHorizontal()
{
    // Create the test window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));

    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(0, 0));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Map the window.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(800, 600), Qt::blue);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QVERIFY(window->isMaximizable());
    QCOMPARE(window->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(window->size(), QSize(800, 600));

    // We should receive a configure event when the window becomes active.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Maximize the test window in horizontal direction.
    workspace()->slotWindowMaximizeHorizontal();
    QCOMPARE(window->requestedMaximizeMode(), MaximizeHorizontal);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(1280, 600));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Draw contents of the maximized window.
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(1280, 600), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->size(), QSize(1280, 600));
    QCOMPARE(window->requestedMaximizeMode(), MaximizeHorizontal);
    QCOMPARE(window->maximizeMode(), MaximizeHorizontal);

    // Restore the window.
    workspace()->slotWindowMaximizeHorizontal();
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->maximizeMode(), MaximizeHorizontal);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 4);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(800, 600));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Draw contents of the restored window.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(800, 600), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->size(), QSize(800, 600));
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);

    // Destroy the window.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void TestXdgShellWindow::testMaximizeVertical()
{
    // Create the test window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));

    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(0, 0));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Map the window.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(800, 600), Qt::blue);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QVERIFY(window->isMaximizable());
    QCOMPARE(window->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(window->size(), QSize(800, 600));

    // We should receive a configure event when the window becomes active.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Maximize the test window in vertical direction.
    workspace()->slotWindowMaximizeVertical();
    QCOMPARE(window->requestedMaximizeMode(), MaximizeVertical);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(800, 1024));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Draw contents of the maximized window.
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(800, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->size(), QSize(800, 1024));
    QCOMPARE(window->requestedMaximizeMode(), MaximizeVertical);
    QCOMPARE(window->maximizeMode(), MaximizeVertical);

    // Restore the window.
    workspace()->slotWindowMaximizeVertical();
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->maximizeMode(), MaximizeVertical);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 4);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(800, 600));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Draw contents of the restored window.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(800, 600), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->size(), QSize(800, 600));
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);

    // Destroy the window.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void TestXdgShellWindow::testMaximizeFull()
{
    // Create the test window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));

    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(0, 0));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Map the window.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(800, 600), Qt::blue);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QVERIFY(window->isMaximizable());
    QCOMPARE(window->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(window->size(), QSize(800, 600));

    // We should receive a configure event when the window becomes active.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Maximize the test window.
    workspace()->slotWindowMaximize();
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(1280, 1024));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Draw contents of the maximized window.
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(1280, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->size(), QSize(1280, 1024));
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);
    QCOMPARE(window->maximizeMode(), MaximizeFull);

    // Restore the window.
    workspace()->slotWindowMaximize();
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 4);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(800, 600));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Draw contents of the restored window.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(800, 600), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->size(), QSize(800, 600));
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);

    // Destroy the window.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void TestXdgShellWindow::testSendMaximizedWindowToAnotherOutput()
{
    // This test verifies that the maximized window will have correct geometry restore
    // after it's sent to another output.

    const auto outputs = workspace()->outputs();

    // Create the window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // Wait for the compositor to send a configure event with the activated state.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    // Move the window to the left monitor.
    window->move(QPointF(10, 20));
    QCOMPARE(window->frameGeometry(), QRectF(10, 20, 100, 50));
    QCOMPARE(window->output(), outputs[0]);

    // Make the window maximized.
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    shellSurface->set_maximized();
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->frameGeometry(), QRectF(0, 0, 1280, 1024));
    QCOMPARE(window->geometryRestore(), QRectF(10, 20, 100, 50));
    QCOMPARE(window->output(), outputs[0]);

    // Send the window to another output.
    window->sendToOutput(outputs[1]);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->frameGeometry(), QRectF(1280, 0, 1280, 1024));
    QCOMPARE(window->geometryRestore(), QRectF(1280 + 10, 20, 100, 50));
    QCOMPARE(window->output(), outputs[1]);
}

void TestXdgShellWindow::testInteractiveMoveUnmaximizeFull()
{
    // This test verifies that a maximized xdg-toplevel is going to be properly unmaximized when it's dragged.

    // Create the window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    // Wait for the compositor to send a configure event with the activated state.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    // Make the window maximized.
    const QRectF originalGeometry = window->frameGeometry();
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    window->maximize(MaximizeFull);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);

    // Start interactive move.
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &Window::interactiveMoveResizeStepped);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);
    const qreal xOffset = 0.25;
    const qreal yOffset = 0.5;
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(window->x() + window->width() * xOffset, window->y() + window->height() * yOffset), timestamp++);
    window->performMousePressCommand(Options::MouseMove, input()->pointer()->pos());
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);

    // Move the window to unmaximize it.
    const QRectF maximizedGeometry = window->frameGeometry();
    Test::pointerMotionRelative(QPointF(0, 100), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), maximizedGeometry);

    // Move the window a tiny bit more.
    Test::pointerMotionRelative(QPointF(0, 10), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), maximizedGeometry);

    // Render the window at the new size.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), QRectF(input()->pointer()->pos() - QPointF(originalGeometry.width() * xOffset, originalGeometry.height() * yOffset), originalGeometry.size()));

    // Move the window again.
    const QRectF normalGeometry = window->frameGeometry();
    Test::pointerMotionRelative(QPointF(0, 10), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), normalGeometry.translated(0, 10));

    // Finish interactive move.
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
}

void TestXdgShellWindow::testInteractiveMoveUnmaximizeInitiallyFull()
{
    // This test verifies that an initially maximized xdg-toplevel will be properly unmaximized when it's dragged.

    // Create the window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), [](Test::XdgToplevel *toplevel) {
        toplevel->set_maximized();
    }));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    // Wait for the compositor to send a configure event with the activated state.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    // Start interactive move.
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &Window::interactiveMoveResizeStepped);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);
    const qreal xOffset = 0.25;
    const qreal yOffset = 0.5;
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(window->x() + window->width() * xOffset, window->y() + window->height() * yOffset), timestamp++);
    window->performMousePressCommand(Options::MouseMove, input()->pointer()->pos());
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);

    // Move the window to unmaximize it.
    const QRectF maximizedGeometry = window->frameGeometry();
    Test::pointerMotionRelative(QPointF(0, 100), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), maximizedGeometry);

    // Move the window a tiny bit more.
    Test::pointerMotionRelative(QPointF(0, 10), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 0);
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), maximizedGeometry);

    // Render the window at the new size.
    const QSize restoredSize(100, 50);
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), QSize(0, 0));
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), restoredSize, Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), QRectF(input()->pointer()->pos() - QPointF(restoredSize.width() * xOffset, restoredSize.height() * yOffset), restoredSize));

    // Move the window again.
    const QRectF normalGeometry = window->frameGeometry();
    Test::pointerMotionRelative(QPointF(0, 10), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), normalGeometry.translated(0, 10));

    // Finish interactive move.
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
}

void TestXdgShellWindow::testInteractiveMoveUnmaximizeHorizontal()
{
    // This test verifies that a maximized horizontally xdg-toplevel is going to be properly unmaximized when it's dragged horizontally.

    // Create the window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    // Wait for the compositor to send a configure event with the activated state.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    // Make the window maximized.
    const QRectF originalGeometry = window->frameGeometry();
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    window->maximize(MaximizeHorizontal);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->maximizeMode(), MaximizeHorizontal);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeHorizontal);

    // Start interactive move.
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &Window::interactiveMoveResizeStepped);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);
    const qreal xOffset = 0.25;
    const qreal yOffset = 0.5;
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(window->x() + window->width() * xOffset, window->y() + window->height() * yOffset), timestamp++);
    window->performMousePressCommand(Options::MouseMove, input()->pointer()->pos());
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeHorizontal);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeHorizontal);

    // Move the window vertically, it's not going to be unmaximized.
    const QRectF maximizedGeometry = window->frameGeometry();
    Test::pointerMotionRelative(QPointF(0, 100), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeHorizontal);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeHorizontal);
    QCOMPARE(window->frameGeometry(), maximizedGeometry.translated(0, 100));

    // Move the window horizontally.
    Test::pointerMotionRelative(QPointF(100, 0), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeHorizontal);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), maximizedGeometry.translated(0, 100));

    // Move the window to the right a bit more.
    Test::pointerMotionRelative(QPointF(10, 0), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeHorizontal);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), maximizedGeometry.translated(0, 100));

    // Render the window at the new size.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), QRectF(input()->pointer()->pos() - QPointF(originalGeometry.width() * xOffset, originalGeometry.height() * yOffset), originalGeometry.size()));

    // Move the window again.
    const QRectF normalGeometry = window->frameGeometry();
    Test::pointerMotionRelative(QPointF(10, 0), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 2);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), normalGeometry.translated(10, 0));

    // Finish interactive move.
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
}

void TestXdgShellWindow::testInteractiveMoveUnmaximizeVertical()
{
    // This test verifies that a maximized vertically xdg-toplevel is going to be properly unmaximized when it's dragged vertically.

    // Create the window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    // Wait for the compositor to send a configure event with the activated state.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    // Make the window maximized.
    const QRectF originalGeometry = window->frameGeometry();
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    window->maximize(MaximizeVertical);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->maximizeMode(), MaximizeVertical);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeVertical);

    // Start interactive move.
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &Window::interactiveMoveResizeStepped);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);
    const qreal xOffset = 0.25;
    const qreal yOffset = 0.5;
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(window->x() + window->width() * xOffset, window->y() + window->height() * yOffset), timestamp++);
    window->performMousePressCommand(Options::MouseMove, input()->pointer()->pos());
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeVertical);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeVertical);

    // Move the window to the right, it's not going to be unmaximized.
    const QRectF maximizedGeometry = window->frameGeometry();
    Test::pointerMotionRelative(QPointF(100, 0), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeVertical);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeVertical);
    QCOMPARE(window->frameGeometry(), maximizedGeometry.translated(100, 0));

    // Move the window vertically.
    Test::pointerMotionRelative(QPointF(0, 100), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeVertical);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), maximizedGeometry.translated(100, 0));

    // Move the window down a bit more.
    Test::pointerMotionRelative(QPointF(0, 10), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(window->maximizeMode(), MaximizeVertical);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), maximizedGeometry.translated(100, 0));

    // Render the window at the new size.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), QRectF(input()->pointer()->pos() - QPointF(originalGeometry.width() * xOffset, originalGeometry.height() * yOffset), originalGeometry.size()));

    // Move the window again.
    const QRectF normalGeometry = window->frameGeometry();
    Test::pointerMotionRelative(QPointF(0, 10), timestamp++);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 2);
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
    QCOMPARE(window->frameGeometry(), normalGeometry.translated(0, 10));

    // Finish interactive move.
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
}

void TestXdgShellWindow::testMaximizeAndChangeDecorationModeAfterInitialCommit()
{
    // Ideally, the app would initialize the xdg-toplevel surface before the initial commit, but
    // many don't do it. They initialize the surface after the first commit.
    // This test verifies that the window will receive a configure event with correct size
    // if an xdg-toplevel surface is set maximized and decoration mode changes after initial commit.

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    std::unique_ptr<Test::XdgToplevelDecorationV1> decoration(Test::createXdgToplevelDecorationV1(shellSurface.get()));
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);

    // Commit the initial state.
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), QSize(0, 0));

    // Request maximized mode and set decoration mode, i.e. perform late initialization.
    shellSurface->set_maximized();
    decoration->set_mode(Test::XdgToplevelDecorationV1::mode_client_side);

    // The compositor will respond with a new configure event, which should contain maximized state.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), QSize(1280, 1024));
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>(), Test::XdgToplevel::State::Maximized);
}

void TestXdgShellWindow::testFullScreenAndChangeDecorationModeAfterInitialCommit()
{
    // Ideally, the app would initialize the xdg-toplevel surface before the initial commit, but
    // many don't do it. They initialize the surface after the first commit.
    // This test verifies that the window will receive a configure event with correct size
    // if an xdg-toplevel surface is set fullscreen and decoration mode changes after initial commit.

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    std::unique_ptr<Test::XdgToplevelDecorationV1> decoration(Test::createXdgToplevelDecorationV1(shellSurface.get()));
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);

    // Commit the initial state.
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), QSize(0, 0));

    // Request fullscreen mode and set decoration mode, i.e. perform late initialization.
    shellSurface->set_fullscreen(nullptr);
    decoration->set_mode(Test::XdgToplevelDecorationV1::mode_client_side);

    // The compositor will respond with a new configure event, which should contain fullscreen state.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), QSize(1280, 1024));
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>(), Test::XdgToplevel::State::Fullscreen);
}

void TestXdgShellWindow::testChangeDecorationModeAfterInitialCommit()
{
    // This test verifies that the compositor will respond with a good configure event when
    // the decoration mode changes after the first surface commit but before the surface is mapped.

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    std::unique_ptr<Test::XdgToplevelDecorationV1> decoration(Test::createXdgToplevelDecorationV1(shellSurface.get()));
    QSignalSpy decorationConfigureRequestedSpy(decoration.get(), &Test::XdgToplevelDecorationV1::configureRequested);
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);

    // Perform the initial commit.
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), QSize(0, 0));
    QCOMPARE(decorationConfigureRequestedSpy.last().at(0).value<Test::XdgToplevelDecorationV1::mode>(), Test::XdgToplevelDecorationV1::mode_server_side);

    // Change decoration mode.
    decoration->set_mode(Test::XdgToplevelDecorationV1::mode_client_side);

    // The configure event should still have 0x0 size.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), QSize(0, 0));
    QCOMPARE(decorationConfigureRequestedSpy.last().at(0).value<Test::XdgToplevelDecorationV1::mode>(), Test::XdgToplevelDecorationV1::mode_client_side);
}

void TestXdgShellWindow::testModal()
{
    auto parentSurface = Test::createSurface();
    auto parentToplevel = Test::createXdgToplevelSurface(parentSurface.get());
    auto parentWindow = Test::renderAndWaitForShown(parentSurface.get(), {200, 200}, Qt::cyan);
    QVERIFY(parentWindow);

    auto childSurface = Test::createSurface();
    auto childToplevel = Test::createXdgToplevelSurface(childSurface.get(), [&parentToplevel](Test::XdgToplevel *toplevel) {
        toplevel->set_parent(parentToplevel->object());
    });
    auto childWindow = Test::renderAndWaitForShown(childSurface.get(), {200, 200}, Qt::yellow);
    QVERIFY(childWindow);
    QVERIFY(!childWindow->isModal());
    QCOMPARE(childWindow->transientFor(), parentWindow);

    auto dialog = Test::createXdgDialogV1(childToplevel.get());
    QVERIFY(Test::waylandSync());
    QVERIFY(dialog);
    QVERIFY(!childWindow->isModal());

    QSignalSpy modalChangedSpy(childWindow, &Window::modalChanged);

    dialog->set_modal();
    Test::flushWaylandConnection();
    QVERIFY(modalChangedSpy.wait());
    QVERIFY(childWindow->isModal());

    dialog->unset_modal();
    Test::flushWaylandConnection();
    QVERIFY(modalChangedSpy.wait());
    QVERIFY(!childWindow->isModal());

    dialog->set_modal();
    Test::flushWaylandConnection();
    QVERIFY(modalChangedSpy.wait());
    Workspace::self()->activateWindow(parentWindow);
    QCOMPARE(Workspace::self()->activeWindow(), childWindow);

    dialog.reset();
    Test::flushWaylandConnection();
    QVERIFY(modalChangedSpy.wait());
    QVERIFY(!childWindow->isModal());
}

void TestXdgShellWindow::testCloseModal()
{
    // This test verifies that the parent window will be activated when an active modal dialog is closed.

    // Create a parent and a child windows.
    auto parentSurface = Test::createSurface();
    auto parentToplevel = Test::createXdgToplevelSurface(parentSurface.get());
    auto parent = Test::renderAndWaitForShown(parentSurface.get(), {200, 200}, Qt::cyan);
    QVERIFY(parent);

    auto childSurface = Test::createSurface();
    auto childToplevel = Test::createXdgToplevelSurface(childSurface.get(), [&parentToplevel](Test::XdgToplevel *toplevel) {
        toplevel->set_parent(parentToplevel->object());
    });
    auto child = Test::renderAndWaitForShown(childSurface.get(), {200, 200}, Qt::yellow);
    QVERIFY(child);
    QVERIFY(!child->isModal());
    QCOMPARE(child->transientFor(), parent);

    // Set modal state.
    auto dialog = Test::createXdgDialogV1(childToplevel.get());
    QSignalSpy modalChangedSpy(child, &Window::modalChanged);
    dialog->set_modal();
    Test::flushWaylandConnection();
    QVERIFY(modalChangedSpy.wait());
    QVERIFY(child->isModal());
    QCOMPARE(workspace()->activeWindow(), child);

    // Close the child.
    QSignalSpy childClosedSpy(child, &Window::closed);
    childToplevel.reset();
    childSurface.reset();
    dialog.reset();
    Test::flushWaylandConnection();
    QVERIFY(childClosedSpy.wait());
    QCOMPARE(workspace()->activeWindow(), parent);
}

void TestXdgShellWindow::testCloseModalPreSetup()
{
    // This test verifies that the parent window will be activated when an active modal dialog is closed
    // even if the modality existed before mapping the parent

    // Create a parent and a child windows.
    auto parentSurface = Test::createSurface();
    auto parentToplevel = Test::createXdgToplevelSurface(parentSurface.get());

    auto childSurface = Test::createSurface();
    auto childToplevel = Test::createXdgToplevelSurface(childSurface.get(), [&parentToplevel](Test::XdgToplevel *toplevel) {
        toplevel->set_parent(parentToplevel->object());
    });
    auto dialog = Test::createXdgDialogV1(childToplevel.get());
    dialog->set_modal();

    auto parent = Test::renderAndWaitForShown(parentSurface.get(), {200, 200}, Qt::cyan);
    QVERIFY(parent);
    auto child = Test::renderAndWaitForShown(childSurface.get(), {200, 200}, Qt::yellow);
    QVERIFY(child);
    QCOMPARE(child->transientFor(), parent);

    Test::flushWaylandConnection();
    QVERIFY(child->isModal());
    QCOMPARE(workspace()->activeWindow(), child);

    // Close the child.
    QSignalSpy childClosedSpy(child, &Window::closed);
    childToplevel.reset();
    childSurface.reset();
    Test::flushWaylandConnection();
    QVERIFY(childClosedSpy.wait());
    QCOMPARE(workspace()->activeWindow(), parent);
}

void TestXdgShellWindow::testCloseInactiveModal()
{
    // This test verifies that the parent window will not be activated when an inactive modal dialog is closed.

    // Create a parent and a child windows.
    auto parentSurface = Test::createSurface();
    auto parentToplevel = Test::createXdgToplevelSurface(parentSurface.get());
    auto parent = Test::renderAndWaitForShown(parentSurface.get(), {200, 200}, Qt::cyan);
    QVERIFY(parent);

    auto childSurface = Test::createSurface();
    auto childToplevel = Test::createXdgToplevelSurface(childSurface.get(), [&parentToplevel](Test::XdgToplevel *toplevel) {
        toplevel->set_parent(parentToplevel->object());
    });
    auto child = Test::renderAndWaitForShown(childSurface.get(), {200, 200}, Qt::yellow);
    QVERIFY(child);
    QVERIFY(!child->isModal());
    QCOMPARE(child->transientFor(), parent);

    // Set modal state.
    auto dialog = Test::createXdgDialogV1(childToplevel.get());
    QSignalSpy modalChangedSpy(child, &Window::modalChanged);
    dialog->set_modal();
    Test::flushWaylandConnection();
    QVERIFY(modalChangedSpy.wait());
    QVERIFY(child->isModal());
    QCOMPARE(workspace()->activeWindow(), child);

    // Show another window.
    auto otherSurface = Test::createSurface();
    auto otherToplevel = Test::createXdgToplevelSurface(otherSurface.get());
    auto otherWindow = Test::renderAndWaitForShown(otherSurface.get(), {200, 200}, Qt::magenta);
    QVERIFY(otherWindow);
    workspace()->setActiveWindow(otherWindow);
    QCOMPARE(workspace()->activeWindow(), otherWindow);

    // Close the child.
    QSignalSpy childClosedSpy(child, &Window::closed);
    childToplevel.reset();
    childSurface.reset();
    dialog.reset();
    Test::flushWaylandConnection();
    QVERIFY(childClosedSpy.wait());
    QCOMPARE(workspace()->activeWindow(), otherWindow);
}

void TestXdgShellWindow::testClosePopupOnParentUnmapped()
{
    // This test verifies that a popup window will be closed when the parent window is closed.

    std::unique_ptr<KWayland::Client::Surface> parentSurface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> parentToplevel = Test::createXdgToplevelSurface(parentSurface.get());
    Window *parent = Test::renderAndWaitForShown(parentSurface.get(), QSize(200, 200), Qt::cyan);
    QVERIFY(parent);

    std::unique_ptr<Test::XdgPositioner> positioner = Test::createXdgPositioner();
    positioner->set_size(10, 10);
    positioner->set_anchor_rect(10, 10, 10, 10);

    std::unique_ptr<KWayland::Client::Surface> childSurface = Test::createSurface();
    std::unique_ptr<Test::XdgPopup> popup = Test::createXdgPopupSurface(childSurface.get(), parentToplevel->xdgSurface(), positioner.get());
    Window *child = Test::renderAndWaitForShown(childSurface.get(), QSize(10, 10), Qt::cyan);
    QVERIFY(child);

    QSignalSpy childClosedSpy(child, &Window::closed);
    parentToplevel.reset();
    parentSurface.reset();
    QVERIFY(childClosedSpy.wait());
}

void TestXdgShellWindow::testPopupWithDismissedParent()
{
    // This test verifies that a popup window will be closed when the parent window is already dismissed by the compositor.

    std::unique_ptr<KWayland::Client::Surface> parentSurface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> parentToplevel = Test::createXdgToplevelSurface(parentSurface.get());
    Window *parent = Test::renderAndWaitForShown(parentSurface.get(), QSize(200, 200), Qt::cyan);
    QVERIFY(parent);

    std::unique_ptr<Test::XdgPositioner> positioner = Test::createXdgPositioner();
    positioner->set_size(10, 10);
    positioner->set_anchor_rect(10, 10, 10, 10);

    std::unique_ptr<KWayland::Client::Surface> childSurface = Test::createSurface();
    std::unique_ptr<Test::XdgPopup> popup = Test::createXdgPopupSurface(childSurface.get(), parentToplevel->xdgSurface(), positioner.get());
    Window *child = Test::renderAndWaitForShown(childSurface.get(), QSize(10, 10), Qt::cyan);
    QVERIFY(child);

    QSignalSpy childDoneSpy(popup.get(), &Test::XdgPopup::doneReceived);
    child->popupDone();
    QVERIFY(childDoneSpy.wait());

    // A nested popup will be dismissed immediately if its parent popup is already dismissed.
    std::unique_ptr<KWayland::Client::Surface> grandChildSurface = Test::createSurface();
    std::unique_ptr<Test::XdgPopup> grandChildPopup = Test::createXdgPopupSurface(grandChildSurface.get(), popup->xdgSurface(), positioner.get(), Test::CreationSetup::CreateOnly);
    QSignalSpy grandChildDoneSpy(grandChildPopup.get(), &Test::XdgPopup::doneReceived);
    grandChildPopup->xdgSurface()->surface()->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(grandChildDoneSpy.wait());
}

void TestXdgShellWindow::testMinimumSize()
{
    // NOTE: a minimum size of 20px is forced by the compositor

    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get(), [](Test::XdgToplevel *toplevel) {
        toplevel->set_min_size(200, 250);
    });
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(300, 300), Qt::cyan);
    QCOMPARE(window->minSize(), QSizeF(200, 250));

    QSignalSpy committedSpy(window->surface(), &SurfaceInterface::committed);

    shellSurface->set_min_size(100, 100);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QCOMPARE(window->minSize(), QSizeF(100, 100));

    shellSurface->set_min_size(0, 100);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QCOMPARE(window->minSize(), QSizeF(20, 100));

    shellSurface->set_min_size(100, 0);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QCOMPARE(window->minSize(), QSizeF(100, 20));

    shellSurface->set_min_size(0, 0);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QCOMPARE(window->minSize(), QSizeF(20, 20));
}

void TestXdgShellWindow::testNoMinimumSize()
{
    // NOTE: a minimum size of 20px is forced by the compositor

    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(300, 300), Qt::cyan);
    QCOMPARE(window->minSize(), QSizeF(20, 20));
}

void TestXdgShellWindow::testMaximumSize()
{
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get(), [](Test::XdgToplevel *toplevel) {
        toplevel->set_max_size(300, 350);
    });
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(200, 200), Qt::cyan);
    QCOMPARE(window->maxSize(), QSizeF(300, 350));

    QSignalSpy committedSpy(window->surface(), &SurfaceInterface::committed);

    shellSurface->set_max_size(400, 400);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QCOMPARE(window->maxSize(), QSizeF(400, 400));

    shellSurface->set_max_size(0, 400);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QCOMPARE(window->maxSize(), QSizeF(INT_MAX, 400));

    shellSurface->set_max_size(400, 0);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QCOMPARE(window->maxSize(), QSizeF(400, INT_MAX));

    shellSurface->set_max_size(0, 0);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QCOMPARE(window->maxSize(), QSizeF(INT_MAX, INT_MAX));
}

void TestXdgShellWindow::testNoMaximumSize()
{
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(300, 300), Qt::cyan);
    QCOMPARE(window->maxSize(), QSizeF(INT_MAX, INT_MAX));
}

void TestXdgShellWindow::testUnconfiguredBufferToplevel()
{
    // This test verifies that a protocol error is posted when a client attaches a buffer to
    // the initial xdg-toplevel commit.

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    Test::render(surface.get(), QSize(100, 50), Qt::blue);

    QSignalSpy connectionErrorSpy(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
    QVERIFY(connectionErrorSpy.wait());
}

void TestXdgShellWindow::testUnconfiguredBufferPopup()
{
    // This test verifies that a protocol error is posted when a client attaches a buffer to
    // the initial xdg-popup commit.

    std::unique_ptr<KWayland::Client::Surface> parentSurface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> parentToplevel = Test::createXdgToplevelSurface(parentSurface.get());
    Test::renderAndWaitForShown(parentSurface.get(), QSize(200, 200), Qt::cyan);

    std::unique_ptr<Test::XdgPositioner> positioner = Test::createXdgPositioner();
    positioner->set_size(10, 10);
    positioner->set_anchor_rect(10, 10, 10, 10);

    std::unique_ptr<KWayland::Client::Surface> childSurface = Test::createSurface();
    std::unique_ptr<Test::XdgPopup> popup = Test::createXdgPopupSurface(childSurface.get(), parentToplevel->xdgSurface(), positioner.get(), Test::CreationSetup::CreateOnly);
    Test::render(childSurface.get(), QSize(100, 50), Qt::blue);

    QSignalSpy connectionErrorSpy(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
    QVERIFY(connectionErrorSpy.wait());
}

WAYLANDTEST_MAIN(TestXdgShellWindow)
#include "xdgshellwindow_test.moc"
