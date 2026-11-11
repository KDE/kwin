/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "pointer_input.h"
#include "wayland/clientconnection.h"
#include "wayland/display.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/surface.h>

#include <QDBusConnection>

// system
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <csignal>

using namespace KWin;

class TestFractionalScale : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testToplevel();
    void testPopup();
    void testRecreatedLayerSurface();
    void testRemappedLayerSurface();
    void testRecreatedInputPanelSurface();
};

void TestFractionalScale::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWayland::Client::Output *>();

    QVERIFY(waylandServer()->init(qAppName()));
    static_cast<WaylandTestApplication *>(kwinApp())->setInputMethodServerToStart("internal");
    kwinApp()->start();
    Test::setOutputConfig({
        Test::OutputInfo{
            .geometry = Rect(0, 0, 1280 / 1.25, 1024 / 1.25),
            .scale = 1.25,
        },
        Test::OutputInfo{
            .geometry = Rect(1280, 0, 1280 / 2, 1024 / 2),
            .scale = 2.0,
        },
        Test::OutputInfo{
            .geometry = Rect(1920, 0, 1280, 1024),
            .scale = 1.0,
        },
    });
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 3);
    QCOMPARE(outputs[0]->geometry(), Rect(0, 0, 1024, 819));
    QCOMPARE(outputs[1]->geometry(), Rect(1280, 0, 640, 512));
    QCOMPARE(outputs[0]->scale(), 1.25);
    QCOMPARE(outputs[1]->scale(), 2.0);
    QCOMPARE(outputs[2]->geometry(), Rect(1920, 0, 1280, 1024));
    QCOMPARE(outputs[2]->scale(), 1.0);
}

void TestFractionalScale::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::FractionalScaleManagerV1 | Test::AdditionalWaylandInterface::LayerShellV1));

    workspace()->setActiveOutput(QPoint(640, 512));
    // put mouse in the middle of screen one
    KWin::input()->pointer()->warp(QPoint(640, 512));
}

void TestFractionalScale::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestFractionalScale::testToplevel()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::FractionalScaleV1> fractionalScale(Test::createFractionalScaleV1(surface.get()));
    QSignalSpy fractionalScaleChanged(fractionalScale.get(), &Test::FractionalScaleV1::preferredScaleChanged);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));

    // above call commits the surface and blocks for the configure event. We should have received the scale already
    // We are sent the value in 120ths
    QCOMPARE(fractionalScaleChanged.count(), 1);
    QCOMPARE(fractionalScale->preferredScale(), std::round(1.25 * 120));

    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QCOMPARE(fractionalScaleChanged.count(), 1);
    QCOMPARE(fractionalScale->preferredScale(), std::round(1.25 * 120));

    // move to screen 2
    window->move(QPoint(1280, 0));

    QVERIFY(Test::waylandSync());
    QCOMPARE(fractionalScaleChanged.count(), 2);
    QCOMPARE(fractionalScale->preferredScale(), std::round(2.0 * 120));
}

void TestFractionalScale::testPopup()
{
    std::unique_ptr<KWayland::Client::Surface> toplevelSurface(Test::createSurface());
    std::unique_ptr<Test::FractionalScaleV1> toplevelFractionalScale(Test::createFractionalScaleV1(toplevelSurface.get()));
    QSignalSpy toplevelFractionalScaleChanged(toplevelFractionalScale.get(), &Test::FractionalScaleV1::preferredScaleChanged);
    std::unique_ptr<Test::XdgToplevel> toplevel(Test::createXdgToplevelSurface(toplevelSurface.get()));

    // above call commits the surface and blocks for the configure event. We should have received the scale already
    // We are sent the value in 120ths
    QCOMPARE(toplevelFractionalScaleChanged.count(), 1);
    QCOMPARE(toplevelFractionalScale->preferredScale(), std::round(1.25 * 120));

    auto toplevelWindow = Test::renderAndWaitForShown(toplevelSurface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(toplevelWindow);
    QCOMPARE(toplevelFractionalScaleChanged.count(), 1);
    QCOMPARE(toplevelFractionalScale->preferredScale(), std::round(1.25 * 120));

    std::unique_ptr<KWayland::Client::Surface> popupSurface(Test::createSurface());
    std::unique_ptr<Test::FractionalScaleV1> popupFractionalScale(Test::createFractionalScaleV1(popupSurface.get()));
    QSignalSpy popupFractionalScaleChanged(popupFractionalScale.get(), &Test::FractionalScaleV1::preferredScaleChanged);
    std::unique_ptr<Test::XdgPositioner> positioner(Test::createXdgPositioner());
    positioner->set_size(10, 10);
    positioner->set_anchor_rect(10, 10, 10, 10);
    std::unique_ptr<Test::XdgPopup> popup(Test::createXdgPopupSurface(popupSurface.get(), toplevel->xdgSurface(), positioner.get()));

    // above call commits the surface and blocks for the configure event. We should have received the scale already
    // We are sent the value in 120ths
    QCOMPARE(popupFractionalScaleChanged.count(), 1);
    QCOMPARE(popupFractionalScale->preferredScale(), std::round(1.25 * 120));

    auto popupWindow = Test::renderAndWaitForShown(popupSurface.get(), QSize(10, 10), Qt::cyan);
    QVERIFY(popupWindow);
    QCOMPARE(popupFractionalScaleChanged.count(), 1);
    QCOMPARE(popupFractionalScale->preferredScale(), std::round(1.25 * 120));

    // move the parent to screen 2
    toplevelWindow->move(QPoint(1280, 0));

    QVERIFY(Test::waylandSync());
    QCOMPARE(toplevelFractionalScaleChanged.count(), 2);
    QCOMPARE(toplevelFractionalScale->preferredScale(), std::round(2.0 * 120));
    QCOMPARE(popupFractionalScaleChanged.count(), 2);
    QCOMPARE(popupFractionalScale->preferredScale(), std::round(2.0 * 120));
}

void TestFractionalScale::testRecreatedLayerSurface()
{
    QVERIFY(Test::waylandSync());
    const auto outputs = workspace()->outputs();
    KWayland::Client::Output *hiDpiOutput = Test::waylandOutput(outputs[1]->name());
    KWayland::Client::Output *loDpiOutput = Test::waylandOutput(outputs[2]->name());
    QVERIFY(hiDpiOutput);
    QVERIFY(loDpiOutput);

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::FractionalScaleV1> fractionalScale(Test::createFractionalScaleV1(surface.get()));

    {
        std::unique_ptr<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.get(), QStringLiteral("test"), hiDpiOutput));
        shellSurface->set_size(100, 50);
        surface->commit(KWayland::Client::Surface::CommitFlag::None);

        QSignalSpy configureRequestedSpy(shellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(fractionalScale->preferredScale(), std::round(2.0 * 120));

        shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
        Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::red);
        QVERIFY(window);
        QCOMPARE(window->output(), outputs[1]);
        QCOMPARE(fractionalScale->preferredScale(), std::round(2.0 * 120));

        // destroy the layer surface, but keep the wl_surface
        shellSurface.reset();
        QVERIFY(Test::waitForWindowClosed(window));
        surface->attachBuffer(KWayland::Client::Buffer::Ptr());
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }

    // create a new layer surface for the same wl_surface on screen 3
    std::unique_ptr<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.get(), QStringLiteral("test"), loDpiOutput));
    shellSurface->set_size(100, 50);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QSignalSpy configureRequestedSpy(shellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(fractionalScale->preferredScale(), std::round(1.0 * 120));

    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::red);
    QVERIFY(window);
    QCOMPARE(window->output(), outputs[2]);
    QCOMPARE(fractionalScale->preferredScale(), std::round(1.0 * 120));

    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void TestFractionalScale::testRemappedLayerSurface()
{
    const auto outputs = workspace()->outputs();
    workspace()->setActiveOutput(outputs[1]);

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::FractionalScaleV1> fractionalScale(Test::createFractionalScaleV1(surface.get()));
    std::unique_ptr<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.get(), QStringLiteral("test")));
    shellSurface->set_size(100, 50);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QSignalSpy configureRequestedSpy(shellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(fractionalScale->preferredScale(), std::round(2.0 * 120));

    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::red);
    QVERIFY(window);
    QCOMPARE(window->output(), outputs[1]);

    // unmap while screen 3 is active, the layer surface follows the active output
    workspace()->setActiveOutput(outputs[2]);
    surface->attachBuffer(KWayland::Client::Buffer::Ptr());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(Test::waitForWindowClosed(window));

    shellSurface->set_size(100, 50);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(fractionalScale->preferredScale(), std::round(1.0 * 120));

    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::red);
    QVERIFY(window);
    QCOMPARE(window->output(), outputs[2]);
    QCOMPARE(fractionalScale->preferredScale(), std::round(1.0 * 120));

    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void TestFractionalScale::testRecreatedInputPanelSurface()
{
    Test::destroyWaylandConnection();
    QVERIFY(Test::setupWaylandConnection(waylandServer()->createInputMethodConnection(),
                                         Test::AdditionalWaylandInterface::FractionalScaleManagerV1 | Test::AdditionalWaylandInterface::InputMethodV1));
    QVERIFY(Test::waylandSync());
    const auto outputs = workspace()->outputs();
    KWayland::Client::Output *output = Test::waylandOutput(outputs[0]->name());
    QVERIFY(output);

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::FractionalScaleV1> fractionalScale(Test::createFractionalScaleV1(surface.get()));
    QSignalSpy preferredScaleChangedSpy(fractionalScale.get(), &Test::FractionalScaleV1::preferredScaleChanged);

    workspace()->setActiveOutput(outputs[1]);
    auto panelSurface = Test::createInputPanelSurfaceV1(surface.get(), output, Test::MockInputMethod::Mode::TopLevel);
    QVERIFY(panelSurface);
    QVERIFY(Test::waylandSync());
    QCOMPARE(preferredScaleChangedSpy.count(), 1);
    QCOMPARE(fractionalScale->preferredScale(), std::round(2.0 * 120));

    // new panel surface on the same screen, the scale must not change in between
    auto samePanelSurface = Test::createInputPanelSurfaceV1(surface.get(), output, Test::MockInputMethod::Mode::TopLevel);
    QVERIFY(samePanelSurface);
    QVERIFY(Test::waylandSync());
    QCOMPARE(preferredScaleChangedSpy.count(), 1);
    QCOMPARE(fractionalScale->preferredScale(), std::round(2.0 * 120));

    // new panel surface for the same wl_surface on screen 3
    workspace()->setActiveOutput(outputs[2]);
    auto newPanelSurface = Test::createInputPanelSurfaceV1(surface.get(), output, Test::MockInputMethod::Mode::TopLevel);
    QVERIFY(newPanelSurface);
    QVERIFY(Test::waylandSync());
    QCOMPARE(preferredScaleChangedSpy.count(), 2);
    QCOMPARE(fractionalScale->preferredScale(), std::round(1.0 * 120));
}

WAYLANDTEST_MAIN(TestFractionalScale)
#include "fractional_scaling_test.moc"
