/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "main.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/output.h>
#include <KWayland/Client/surface.h>

Q_DECLARE_METATYPE(QMargins)
Q_DECLARE_METATYPE(KWin::Layer)

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_layershellv1window-0");

class LayerShellV1WindowTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testOutput_data();
    void testOutput();
    void testAnchor_data();
    void testAnchor();
    void testMargins_data();
    void testMargins();
    void testLayer_data();
    void testLayer();
    void testPlacementArea_data();
    void testPlacementArea();
    void testFill_data();
    void testFill();
    void testStack();
    void testFocus();
    void testActivate_data();
    void testActivate();
    void testUnmap();
};

void LayerShellV1WindowTest::initTestCase()
{
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void LayerShellV1WindowTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::LayerShellV1));

    workspace()->setActiveOutput(QPoint(640, 512));
    Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void LayerShellV1WindowTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void LayerShellV1WindowTest::testOutput_data()
{
    QTest::addColumn<int>("screenId");

    QTest::addRow("first output") << 0;
    QTest::addRow("second output") << 1;
}

void LayerShellV1WindowTest::testOutput()
{
    // Fetch the wl_output object.
    QFETCH(int, screenId);
    KWayland::Client::Output *output = Test::waylandOutputs().value(screenId);
    QVERIFY(output);

    // Create a layer shell surface.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.get(), QStringLiteral("test"), output));

    // Set the initial state of the layer surface.
    shellSurface->set_size(280, 124);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surface.
    QSignalSpy configureRequestedSpy(shellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.wait());
    const QSize requestedSize = configureRequestedSpy.last().at(1).toSize();

    // Map the layer surface.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    Window *window = Test::renderAndWaitForShown(surface.get(), requestedSize, Qt::red);
    QVERIFY(window);

    // Verify that the window is on the requested screen.
    QVERIFY(output->geometry().contains(window->frameGeometry().toRect()));

    // Destroy the window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void LayerShellV1WindowTest::testAnchor_data()
{
    QTest::addColumn<int>("anchor");
    QTest::addColumn<QRectF>("expectedGeometry");

    QTest::addRow("left") << int(Test::LayerSurfaceV1::anchor_left)
                          << QRectF(0, 450, 280, 124);

    QTest::addRow("top left") << (Test::LayerSurfaceV1::anchor_top | Test::LayerSurfaceV1::anchor_left)
                              << QRectF(0, 0, 280, 124);

    QTest::addRow("top") << int(Test::LayerSurfaceV1::anchor_top)
                         << QRectF(500, 0, 280, 124);

    QTest::addRow("top right") << (Test::LayerSurfaceV1::anchor_top | Test::LayerSurfaceV1::anchor_right)
                               << QRectF(1000, 0, 280, 124);

    QTest::addRow("right") << int(Test::LayerSurfaceV1::anchor_right)
                           << QRectF(1000, 450, 280, 124);

    QTest::addRow("bottom right") << (Test::LayerSurfaceV1::anchor_bottom | Test::LayerSurfaceV1::anchor_right)
                                  << QRectF(1000, 900, 280, 124);

    QTest::addRow("bottom") << int(Test::LayerSurfaceV1::anchor_bottom)
                            << QRectF(500, 900, 280, 124);

    QTest::addRow("bottom left") << (Test::LayerSurfaceV1::anchor_bottom | Test::LayerSurfaceV1::anchor_left)
                                 << QRectF(0, 900, 280, 124);
}

void LayerShellV1WindowTest::testAnchor()
{
    // Create a layer shell surface.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.get(), QStringLiteral("test")));

    // Set the initial state of the layer surface.
    QFETCH(int, anchor);
    shellSurface->set_anchor(anchor);
    shellSurface->set_size(280, 124);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surface.
    QSignalSpy configureRequestedSpy(shellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.wait());
    const QSize requestedSize = configureRequestedSpy.last().at(1).toSize();
    QCOMPARE(requestedSize, QSize(280, 124));

    // Map the layer surface.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(280, 124), Qt::red);
    QVERIFY(window);

    // Verify that the window is placed at expected location.
    QTEST(window->frameGeometry(), "expectedGeometry");

    // Destroy the window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void LayerShellV1WindowTest::testMargins_data()
{
    QTest::addColumn<int>("anchor");
    QTest::addColumn<QMargins>("margins");
    QTest::addColumn<QRectF>("expectedGeometry");

    QTest::addRow("left") << int(Test::LayerSurfaceV1::anchor_left)
                          << QMargins(100, 0, 0, 0)
                          << QRectF(100, 450, 280, 124);

    QTest::addRow("top left") << (Test::LayerSurfaceV1::anchor_top | Test::LayerSurfaceV1::anchor_left)
                              << QMargins(100, 200, 0, 0)
                              << QRectF(100, 200, 280, 124);

    QTest::addRow("top") << int(Test::LayerSurfaceV1::anchor_top)
                         << QMargins(0, 200, 0, 0)
                         << QRectF(500, 200, 280, 124);

    QTest::addRow("top right") << (Test::LayerSurfaceV1::anchor_top | Test::LayerSurfaceV1::anchor_right)
                               << QMargins(0, 200, 300, 0)
                               << QRectF(700, 200, 280, 124);

    QTest::addRow("right") << int(Test::LayerSurfaceV1::anchor_right)
                           << QMargins(0, 0, 300, 0)
                           << QRectF(700, 450, 280, 124);

    QTest::addRow("bottom right") << (Test::LayerSurfaceV1::anchor_bottom | Test::LayerSurfaceV1::anchor_right)
                                  << QMargins(0, 0, 300, 400)
                                  << QRectF(700, 500, 280, 124);

    QTest::addRow("bottom") << int(Test::LayerSurfaceV1::anchor_bottom)
                            << QMargins(0, 0, 0, 400)
                            << QRectF(500, 500, 280, 124);

    QTest::addRow("bottom left") << (Test::LayerSurfaceV1::anchor_bottom | Test::LayerSurfaceV1::anchor_left)
                                 << QMargins(100, 0, 0, 400)
                                 << QRectF(100, 500, 280, 124);
}

void LayerShellV1WindowTest::testMargins()
{
    // Create a layer shell surface.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.get(), QStringLiteral("test")));

    // Set the initial state of the layer surface.
    QFETCH(QMargins, margins);
    QFETCH(int, anchor);
    shellSurface->set_anchor(anchor);
    shellSurface->set_margin(margins.top(), margins.right(), margins.bottom(), margins.left());
    shellSurface->set_size(280, 124);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surface.
    QSignalSpy configureRequestedSpy(shellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.wait());
    const QSize requestedSize = configureRequestedSpy.last().at(1).toSize();

    // Map the layer surface.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    Window *window = Test::renderAndWaitForShown(surface.get(), requestedSize, Qt::red);
    QVERIFY(window);

    // Verify that the window is placed at expected location.
    QTEST(window->frameGeometry(), "expectedGeometry");

    // Destroy the window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void LayerShellV1WindowTest::testLayer_data()
{
    QTest::addColumn<int>("protocolLayer");
    QTest::addColumn<Layer>("compositorLayer");

    QTest::addRow("overlay") << int(Test::LayerShellV1::layer_overlay) << UnmanagedLayer;
    QTest::addRow("top") << int(Test::LayerShellV1::layer_top) << AboveLayer;
    QTest::addRow("bottom") << int(Test::LayerShellV1::layer_bottom) << BelowLayer;
    QTest::addRow("background") << int(Test::LayerShellV1::layer_background) << DesktopLayer;
}

void LayerShellV1WindowTest::testLayer()
{
    // Create a layer shell surface.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.get(), QStringLiteral("test")));

    // Set the initial state of the layer surface.
    QFETCH(int, protocolLayer);
    shellSurface->set_layer(protocolLayer);
    shellSurface->set_size(280, 124);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surface.
    QSignalSpy configureRequestedSpy(shellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.wait());
    const QSize requestedSize = configureRequestedSpy.last().at(1).toSize();

    // Map the layer surface.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    Window *window = Test::renderAndWaitForShown(surface.get(), requestedSize, Qt::red);
    QVERIFY(window);

    // Verify that the window is placed at expected location.
    QTEST(window->layer(), "compositorLayer");

    // Destroy the window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void LayerShellV1WindowTest::testPlacementArea_data()
{
    QTest::addColumn<int>("anchor");
    QTest::addColumn<int>("exclusiveZone");
    QTest::addColumn<QRectF>("placementArea");

    QTest::addRow("left") << int(Test::LayerSurfaceV1::anchor_left) << 300 << QRectF(300, 0, 980, 1024);
    QTest::addRow("top") << int(Test::LayerSurfaceV1::anchor_top) << 300 << QRectF(0, 300, 1280, 724);
    QTest::addRow("right") << int(Test::LayerSurfaceV1::anchor_right) << 300 << QRectF(0, 0, 980, 1024);
    QTest::addRow("bottom") << int(Test::LayerSurfaceV1::anchor_bottom) << 300 << QRectF(0, 0, 1280, 724);
}

void LayerShellV1WindowTest::testPlacementArea()
{
    // Create a layer shell surface.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.get(), QStringLiteral("test")));

    // Set the initial state of the layer surface.
    QFETCH(int, anchor);
    QFETCH(int, exclusiveZone);
    shellSurface->set_anchor(anchor);
    shellSurface->set_exclusive_zone(exclusiveZone);
    shellSurface->set_size(280, 124);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surface.
    QSignalSpy configureRequestedSpy(shellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.wait());
    const QSize requestedSize = configureRequestedSpy.last().at(1).toSize();

    // Map the layer surface.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    Window *window = Test::renderAndWaitForShown(surface.get(), requestedSize, Qt::red);
    QVERIFY(window);

    // Verify that the work area has been adjusted.
    QTEST(workspace()->clientArea(PlacementArea, window), "placementArea");

    // Destroy the window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void LayerShellV1WindowTest::testFill_data()
{
    QTest::addColumn<int>("anchor");
    QTest::addColumn<QSize>("desiredSize");
    QTest::addColumn<QRectF>("expectedGeometry");

    QTest::addRow("horizontal") << (Test::LayerSurfaceV1::anchor_left | Test::LayerSurfaceV1::anchor_right)
                                << QSize(0, 124)
                                << QRectF(0, 450, 1280, 124);

    QTest::addRow("vertical") << (Test::LayerSurfaceV1::anchor_top | Test::LayerSurfaceV1::anchor_bottom)
                              << QSize(280, 0)
                              << QRectF(500, 0, 280, 1024);

    QTest::addRow("all") << (Test::LayerSurfaceV1::anchor_left | Test::LayerSurfaceV1::anchor_top | Test::LayerSurfaceV1::anchor_right | Test::LayerSurfaceV1::anchor_bottom)
                         << QSize(0, 0)
                         << QRectF(0, 0, 1280, 1024);
}

void LayerShellV1WindowTest::testFill()
{
    // Create a layer shell surface.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.get(), QStringLiteral("test")));

    // Set the initial state of the layer surface.
    QFETCH(int, anchor);
    QFETCH(QSize, desiredSize);
    shellSurface->set_anchor(anchor);
    shellSurface->set_size(desiredSize.width(), desiredSize.height());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surface.
    QSignalSpy configureRequestedSpy(shellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.wait());
    const QSize requestedSize = configureRequestedSpy.last().at(1).toSize();

    // Map the layer surface.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    Window *window = Test::renderAndWaitForShown(surface.get(), requestedSize, Qt::red);
    QVERIFY(window);

    // Verify that the window is placed at expected location.
    QTEST(window->frameGeometry(), "expectedGeometry");

    // Destroy the window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void LayerShellV1WindowTest::testStack()
{
    // Create a layer shell surface.
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::LayerSurfaceV1> shellSurface1(Test::createLayerSurfaceV1(surface1.get(), QStringLiteral("test")));

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::LayerSurfaceV1> shellSurface2(Test::createLayerSurfaceV1(surface2.get(), QStringLiteral("test")));

    // Set the initial state of the layer surface.
    shellSurface1->set_anchor(Test::LayerSurfaceV1::anchor_left);
    shellSurface1->set_size(80, 124);
    shellSurface1->set_exclusive_zone(80);
    surface1->commit(KWayland::Client::Surface::CommitFlag::None);

    shellSurface2->set_anchor(Test::LayerSurfaceV1::anchor_left);
    shellSurface2->set_size(200, 124);
    shellSurface2->set_exclusive_zone(200);
    surface2->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surfaces.
    QSignalSpy configureRequestedSpy1(shellSurface1.get(), &Test::LayerSurfaceV1::configureRequested);
    QSignalSpy configureRequestedSpy2(shellSurface2.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy2.wait());
    const QSize requestedSize1 = configureRequestedSpy1.last().at(1).toSize();
    const QSize requestedSize2 = configureRequestedSpy2.last().at(1).toSize();

    // Map the layer surface.
    shellSurface1->ack_configure(configureRequestedSpy1.last().at(0).toUInt());
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), requestedSize1, Qt::red);
    QVERIFY(window1);

    shellSurface2->ack_configure(configureRequestedSpy2.last().at(0).toUInt());
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), requestedSize2, Qt::red);
    QVERIFY(window2);

    // Check that the second layer surface is placed next to the first.
    QCOMPARE(window1->frameGeometry(), QRect(0, 450, 80, 124));
    QCOMPARE(window2->frameGeometry(), QRect(80, 450, 200, 124));

    // Check that the work area has been adjusted accordingly.
    QCOMPARE(workspace()->clientArea(PlacementArea, window1), QRect(280, 0, 1000, 1024));
    QCOMPARE(workspace()->clientArea(PlacementArea, window2), QRect(280, 0, 1000, 1024));

    // Destroy the window.
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowDestroyed(window1));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(window2));
}

void LayerShellV1WindowTest::testFocus()
{
    // Create a layer shell surface.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.get(), QStringLiteral("test")));

    // Set the initial state of the layer surface.
    shellSurface->set_keyboard_interactivity(1);
    shellSurface->set_size(280, 124);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surface.
    QSignalSpy configureRequestedSpy(shellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.wait());
    const QSize requestedSize = configureRequestedSpy.last().at(1).toSize();

    // Map the layer surface.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    Window *window = Test::renderAndWaitForShown(surface.get(), requestedSize, Qt::red);
    QVERIFY(window);

    // The layer surface must be focused when it's mapped.
    QVERIFY(window->isActive());

    // Destroy the window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void LayerShellV1WindowTest::testActivate_data()
{
    QTest::addColumn<int>("layer");
    QTest::addColumn<bool>("active");

    QTest::addRow("overlay") << int(Test::LayerShellV1::layer_overlay) << true;
    QTest::addRow("top") << int(Test::LayerShellV1::layer_top) << true;
    QTest::addRow("bottom") << int(Test::LayerShellV1::layer_bottom) << false;
    QTest::addRow("background") << int(Test::LayerShellV1::layer_background) << false;
}

void LayerShellV1WindowTest::testActivate()
{
    // Create a layer shell surface.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.get(), QStringLiteral("test")));

    // Set the initial state of the layer surface.
    QFETCH(int, layer);
    shellSurface->set_layer(layer);
    shellSurface->set_size(280, 124);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surface.
    QSignalSpy configureRequestedSpy(shellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.wait());
    const QSize requestedSize = configureRequestedSpy.last().at(1).toSize();

    // Map the layer surface.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    Window *window = Test::renderAndWaitForShown(surface.get(), requestedSize, Qt::red);
    QVERIFY(window);
    QVERIFY(!window->isActive());

    // Try to activate the layer surface.
    shellSurface->set_keyboard_interactivity(1);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QSignalSpy activeChangedSpy(window, &Window::activeChanged);
    QTEST(activeChangedSpy.wait(1000), "active");

    // Destroy the window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void LayerShellV1WindowTest::testUnmap()
{
    // Create a layer shell surface.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.get(), QStringLiteral("test")));

    // Set the initial state of the layer surface.
    shellSurface->set_size(280, 124);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surface.
    QSignalSpy configureRequestedSpy(shellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.wait());

    // Map the layer surface.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(280, 124), Qt::red);
    QVERIFY(window);

    // Unmap the layer surface.
    surface->attachBuffer(KWayland::Client::Buffer::Ptr());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(Test::waitForWindowDestroyed(window));

    // Notify the compositor that we want to map the layer surface.
    shellSurface->set_size(280, 124);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the configure event.
    QVERIFY(configureRequestedSpy.wait());

    // Map the layer surface back.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    window = Test::renderAndWaitForShown(surface.get(), QSize(280, 124), Qt::red);
    QVERIFY(window);

    // Destroy the window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::LayerShellV1WindowTest)
#include "layershellv1window_test.moc"
