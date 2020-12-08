/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"
#include "abstract_client.h"
#include "abstract_output.h"
#include "main.h"
#include "platform.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/output.h>
#include <KWayland/Client/surface.h>

Q_DECLARE_METATYPE(QMargins)
Q_DECLARE_METATYPE(KWin::Layer)

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_layershellv1client-0");

class LayerShellV1ClientTest : public QObject
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

void LayerShellV1ClientTest::initTestCase()
{
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void LayerShellV1ClientTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::LayerShellV1));

    screens()->setCurrent(0);
    Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void LayerShellV1ClientTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void LayerShellV1ClientTest::testOutput_data()
{
    QTest::addColumn<int>("screenId");

    QTest::addRow("first output")  << 0;
    QTest::addRow("second output") << 1;
}

void LayerShellV1ClientTest::testOutput()
{
    // Fetch the wl_output object.
    QFETCH(int, screenId);
    KWayland::Client::Output *output = Test::waylandOutputs().value(screenId);
    QVERIFY(output);

    // Create a layer shell surface.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.data(), QStringLiteral("test"), output));

    // Set the initial state of the layer surface.
    shellSurface->set_size(280, 124);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surface.
    QSignalSpy configureRequestedSpy(shellSurface.data(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    QVERIFY(configureRequestedSpy.wait());
    const QSize requestedSize = configureRequestedSpy.last().at(1).toSize();

    // Map the layer surface.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), requestedSize, Qt::red);
    QVERIFY(client);

    // Verify that the client is on the requested screen.
    QVERIFY(output->geometry().contains(client->frameGeometry()));

    // Destroy the client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void LayerShellV1ClientTest::testAnchor_data()
{
    QTest::addColumn<int>("anchor");
    QTest::addColumn<QRect>("expectedGeometry");

    QTest::addRow("left")         << int(Test::LayerSurfaceV1::anchor_left)
                                  << QRect(0, 450, 280, 124);

    QTest::addRow("top left")     << (Test::LayerSurfaceV1::anchor_top |
                                      Test::LayerSurfaceV1::anchor_left)
                                  << QRect(0, 0, 280, 124);

    QTest::addRow("top")          << int(Test::LayerSurfaceV1::anchor_top)
                                  << QRect(500, 0, 280, 124);

    QTest::addRow("top right")    << (Test::LayerSurfaceV1::anchor_top |
                                      Test::LayerSurfaceV1::anchor_right)
                                  << QRect(1000, 0, 280, 124);

    QTest::addRow("right")        << int(Test::LayerSurfaceV1::anchor_right)
                                  << QRect(1000, 450, 280, 124);

    QTest::addRow("bottom right") << (Test::LayerSurfaceV1::anchor_bottom |
                                      Test::LayerSurfaceV1::anchor_right)
                                  << QRect(1000, 900, 280, 124);

    QTest::addRow("bottom")       << int(Test::LayerSurfaceV1::anchor_bottom)
                                  << QRect(500, 900, 280, 124);

    QTest::addRow("bottom left")  << (Test::LayerSurfaceV1::anchor_bottom |
                                      Test::LayerSurfaceV1::anchor_left)
                                  << QRect(0, 900, 280, 124);
}

void LayerShellV1ClientTest::testAnchor()
{
    // Create a layer shell surface.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.data(), QStringLiteral("test")));

    // Set the initial state of the layer surface.
    QFETCH(int, anchor);
    shellSurface->set_anchor(anchor);
    shellSurface->set_size(280, 124);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surface.
    QSignalSpy configureRequestedSpy(shellSurface.data(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    QVERIFY(configureRequestedSpy.wait());
    const QSize requestedSize = configureRequestedSpy.last().at(1).toSize();
    QCOMPARE(requestedSize, QSize(280, 124));

    // Map the layer surface.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(280, 124), Qt::red);
    QVERIFY(client);

    // Verify that the client is placed at expected location.
    QTEST(client->frameGeometry(), "expectedGeometry");

    // Destroy the client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void LayerShellV1ClientTest::testMargins_data()
{
    QTest::addColumn<int>("anchor");
    QTest::addColumn<QMargins>("margins");
    QTest::addColumn<QRect>("expectedGeometry");

    QTest::addRow("left")         << int(Test::LayerSurfaceV1::anchor_left)
                                  << QMargins(100, 0, 0, 0)
                                  << QRect(100, 450, 280, 124);

    QTest::addRow("top left")     << (Test::LayerSurfaceV1::anchor_top |
                                      Test::LayerSurfaceV1::anchor_left)
                                  << QMargins(100, 200, 0, 0)
                                  << QRect(100, 200, 280, 124);

    QTest::addRow("top")          << int(Test::LayerSurfaceV1::anchor_top)
                                  << QMargins(0, 200, 0, 0)
                                  << QRect(500, 200, 280, 124);

    QTest::addRow("top right")    << (Test::LayerSurfaceV1::anchor_top |
                                      Test::LayerSurfaceV1::anchor_right)
                                  << QMargins(0, 200, 300, 0)
                                  << QRect(700, 200, 280, 124);

    QTest::addRow("right")        << int(Test::LayerSurfaceV1::anchor_right)
                                  << QMargins(0, 0, 300, 0)
                                  << QRect(700, 450, 280, 124);

    QTest::addRow("bottom right") << (Test::LayerSurfaceV1::anchor_bottom |
                                      Test::LayerSurfaceV1::anchor_right)
                                  << QMargins(0, 0, 300, 400)
                                  << QRect(700, 500, 280, 124);

    QTest::addRow("bottom")       << int(Test::LayerSurfaceV1::anchor_bottom)
                                  << QMargins(0, 0, 0, 400)
                                  << QRect(500, 500, 280, 124);

    QTest::addRow("bottom left")  << (Test::LayerSurfaceV1::anchor_bottom |
                                      Test::LayerSurfaceV1::anchor_left)
                                  << QMargins(100, 0, 0, 400)
                                  << QRect(100, 500, 280, 124);
}

void LayerShellV1ClientTest::testMargins()
{
    // Create a layer shell surface.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.data(), QStringLiteral("test")));

    // Set the initial state of the layer surface.
    QFETCH(QMargins, margins);
    QFETCH(int, anchor);
    shellSurface->set_anchor(anchor);
    shellSurface->set_margin(margins.top(), margins.right(), margins.bottom(), margins.left());
    shellSurface->set_size(280, 124);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surface.
    QSignalSpy configureRequestedSpy(shellSurface.data(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    QVERIFY(configureRequestedSpy.wait());
    const QSize requestedSize = configureRequestedSpy.last().at(1).toSize();

    // Map the layer surface.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), requestedSize, Qt::red);
    QVERIFY(client);

    // Verify that the client is placed at expected location.
    QTEST(client->frameGeometry(), "expectedGeometry");

    // Destroy the client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void LayerShellV1ClientTest::testLayer_data()
{
    QTest::addColumn<int>("protocolLayer");
    QTest::addColumn<Layer>("compositorLayer");

    QTest::addRow("overlay")    << int(Test::LayerShellV1::layer_overlay)    << UnmanagedLayer;
    QTest::addRow("top")        << int(Test::LayerShellV1::layer_top)        << AboveLayer;
    QTest::addRow("bottom")     << int(Test::LayerShellV1::layer_bottom)     << BelowLayer;
    QTest::addRow("background") << int(Test::LayerShellV1::layer_background) << DesktopLayer;
}

void LayerShellV1ClientTest::testLayer()
{
    // Create a layer shell surface.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.data(), QStringLiteral("test")));

    // Set the initial state of the layer surface.
    QFETCH(int, protocolLayer);
    shellSurface->set_layer(protocolLayer);
    shellSurface->set_size(280, 124);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surface.
    QSignalSpy configureRequestedSpy(shellSurface.data(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    QVERIFY(configureRequestedSpy.wait());
    const QSize requestedSize = configureRequestedSpy.last().at(1).toSize();

    // Map the layer surface.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), requestedSize, Qt::red);
    QVERIFY(client);

    // Verify that the client is placed at expected location.
    QTEST(client->layer(), "compositorLayer");

    // Destroy the client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void LayerShellV1ClientTest::testPlacementArea_data()
{
    QTest::addColumn<int>("anchor");
    QTest::addColumn<int>("exclusiveZone");
    QTest::addColumn<QRect>("placementArea");

    QTest::addRow("left")   << int(Test::LayerSurfaceV1::anchor_left)   << 300 << QRect(300, 0, 980, 1024);
    QTest::addRow("top")    << int(Test::LayerSurfaceV1::anchor_top)    << 300 << QRect(0, 300, 1280, 724);
    QTest::addRow("right")  << int(Test::LayerSurfaceV1::anchor_right)  << 300 << QRect(0, 0, 980, 1024);
    QTest::addRow("bottom") << int(Test::LayerSurfaceV1::anchor_bottom) << 300 << QRect(0, 0, 1280, 724);
}

void LayerShellV1ClientTest::testPlacementArea()
{
    // Create a layer shell surface.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.data(), QStringLiteral("test")));

    // Set the initial state of the layer surface.
    QFETCH(int, anchor);
    QFETCH(int, exclusiveZone);
    shellSurface->set_anchor(anchor);
    shellSurface->set_exclusive_zone(exclusiveZone);
    shellSurface->set_size(280, 124);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surface.
    QSignalSpy configureRequestedSpy(shellSurface.data(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    QVERIFY(configureRequestedSpy.wait());
    const QSize requestedSize = configureRequestedSpy.last().at(1).toSize();

    // Map the layer surface.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), requestedSize, Qt::red);
    QVERIFY(client);

    // Verify that the work area has been adjusted.
    QTEST(workspace()->clientArea(PlacementArea, client), "placementArea");

    // Destroy the client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void LayerShellV1ClientTest::testFill_data()
{
    QTest::addColumn<int>("anchor");
    QTest::addColumn<QSize>("desiredSize");
    QTest::addColumn<QRect>("expectedGeometry");

    QTest::addRow("horizontal") << (Test::LayerSurfaceV1::anchor_left |
                                    Test::LayerSurfaceV1::anchor_right)
                                << QSize(0, 124)
                                << QRect(0, 450, 1280, 124);

    QTest::addRow("vertical")   << (Test::LayerSurfaceV1::anchor_top |
                                    Test::LayerSurfaceV1::anchor_bottom)
                                << QSize(280, 0)
                                << QRect(500, 0, 280, 1024);

    QTest::addRow("all")        << (Test::LayerSurfaceV1::anchor_left |
                                    Test::LayerSurfaceV1::anchor_top |
                                    Test::LayerSurfaceV1::anchor_right |
                                    Test::LayerSurfaceV1::anchor_bottom)
                                << QSize(0, 0)
                                << QRect(0, 0, 1280, 1024);
}

void LayerShellV1ClientTest::testFill()
{
    // Create a layer shell surface.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.data(), QStringLiteral("test")));

    // Set the initial state of the layer surface.
    QFETCH(int, anchor);
    QFETCH(QSize, desiredSize);
    shellSurface->set_anchor(anchor);
    shellSurface->set_size(desiredSize.width(), desiredSize.height());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surface.
    QSignalSpy configureRequestedSpy(shellSurface.data(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    QVERIFY(configureRequestedSpy.wait());
    const QSize requestedSize = configureRequestedSpy.last().at(1).toSize();

    // Map the layer surface.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), requestedSize, Qt::red);
    QVERIFY(client);

    // Verify that the client is placed at expected location.
    QTEST(client->frameGeometry(), "expectedGeometry");

    // Destroy the client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void LayerShellV1ClientTest::testStack()
{
    // Create a layer shell surface.
    QScopedPointer<KWayland::Client::Surface> surface1(Test::createSurface());
    QScopedPointer<Test::LayerSurfaceV1> shellSurface1(Test::createLayerSurfaceV1(surface1.data(), QStringLiteral("test")));

    QScopedPointer<KWayland::Client::Surface> surface2(Test::createSurface());
    QScopedPointer<Test::LayerSurfaceV1> shellSurface2(Test::createLayerSurfaceV1(surface2.data(), QStringLiteral("test")));

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
    QSignalSpy configureRequestedSpy1(shellSurface1.data(), &Test::LayerSurfaceV1::configureRequested);
    QSignalSpy configureRequestedSpy2(shellSurface2.data(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy2.wait());
    const QSize requestedSize1 = configureRequestedSpy1.last().at(1).toSize();
    const QSize requestedSize2 = configureRequestedSpy2.last().at(1).toSize();

    // Map the layer surface.
    shellSurface1->ack_configure(configureRequestedSpy1.last().at(0).toUInt());
    AbstractClient *client1 = Test::renderAndWaitForShown(surface1.data(), requestedSize1, Qt::red);
    QVERIFY(client1);

    shellSurface2->ack_configure(configureRequestedSpy2.last().at(0).toUInt());
    AbstractClient *client2 = Test::renderAndWaitForShown(surface2.data(), requestedSize2, Qt::red);
    QVERIFY(client2);

    // Check that the second layer surface is placed next to the first.
    QCOMPARE(client1->frameGeometry(), QRect(0, 450, 80, 124));
    QCOMPARE(client2->frameGeometry(), QRect(80, 450, 200, 124));

    // Check that the work area has been adjusted accordingly.
    QCOMPARE(workspace()->clientArea(PlacementArea, client1), QRect(280, 0, 1000, 1024));
    QCOMPARE(workspace()->clientArea(PlacementArea, client2), QRect(280, 0, 1000, 1024));

    // Destroy the client.
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowDestroyed(client1));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(client2));
}

void LayerShellV1ClientTest::testFocus()
{
    // Create a layer shell surface.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.data(), QStringLiteral("test")));

    // Set the initial state of the layer surface.
    shellSurface->set_keyboard_interactivity(1);
    shellSurface->set_size(280, 124);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surface.
    QSignalSpy configureRequestedSpy(shellSurface.data(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    QVERIFY(configureRequestedSpy.wait());
    const QSize requestedSize = configureRequestedSpy.last().at(1).toSize();

    // Map the layer surface.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), requestedSize, Qt::red);
    QVERIFY(client);

    // The layer surface must be focused when it's mapped.
    QVERIFY(client->isActive());

    // Destroy the client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void LayerShellV1ClientTest::testActivate_data()
{
    QTest::addColumn<int>("layer");
    QTest::addColumn<bool>("active");

    QTest::addRow("overlay")    << int(Test::LayerShellV1::layer_overlay)    << true;
    QTest::addRow("top")        << int(Test::LayerShellV1::layer_top)        << true;
    QTest::addRow("bottom")     << int(Test::LayerShellV1::layer_bottom)     << false;
    QTest::addRow("background") << int(Test::LayerShellV1::layer_background) << false;
}

void LayerShellV1ClientTest::testActivate()
{
    // Create a layer shell surface.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.data(), QStringLiteral("test")));

    // Set the initial state of the layer surface.
    QFETCH(int, layer);
    shellSurface->set_layer(layer);
    shellSurface->set_size(280, 124);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surface.
    QSignalSpy configureRequestedSpy(shellSurface.data(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    QVERIFY(configureRequestedSpy.wait());
    const QSize requestedSize = configureRequestedSpy.last().at(1).toSize();

    // Map the layer surface.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), requestedSize, Qt::red);
    QVERIFY(client);
    QVERIFY(!client->isActive());

    // Try to activate the layer surface.
    shellSurface->set_keyboard_interactivity(1);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QSignalSpy activeChangedSpy(client, &AbstractClient::activeChanged);
    QVERIFY(activeChangedSpy.isValid());
    QTEST(activeChangedSpy.wait(1000), "active");

    // Destroy the client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void LayerShellV1ClientTest::testUnmap()
{
    // Create a layer shell surface.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::LayerSurfaceV1> shellSurface(Test::createLayerSurfaceV1(surface.data(), QStringLiteral("test")));

    // Set the initial state of the layer surface.
    shellSurface->set_size(280, 124);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the compositor to position the surface.
    QSignalSpy configureRequestedSpy(shellSurface.data(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    QVERIFY(configureRequestedSpy.wait());

    // Map the layer surface.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(280, 124), Qt::red);
    QVERIFY(client);

    // Unmap the layer surface.
    surface->attachBuffer(KWayland::Client::Buffer::Ptr());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(Test::waitForWindowDestroyed(client));

    // Notify the compositor that we want to map the layer surface.
    shellSurface->set_size(280, 124);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the configure event.
    QVERIFY(configureRequestedSpy.wait());

    // Map the layer surface back.
    shellSurface->ack_configure(configureRequestedSpy.last().at(0).toUInt());
    client = Test::renderAndWaitForShown(surface.data(), QSize(280, 124), Qt::red);
    QVERIFY(client);

    // Destroy the client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::LayerShellV1ClientTest)
#include "layershellv1client_test.moc"
