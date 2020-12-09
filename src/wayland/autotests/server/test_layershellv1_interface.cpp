/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QThread>
#include <QtTest>

#include "../../src/server/compositor_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/layershell_v1_interface.h"
#include "../../src/server/surface_interface.h"
#include "../../src/server/xdgshell_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/surface.h"

#include "qwayland-wlr-layer-shell-unstable-v1.h"
#include "qwayland-xdg-shell.h"

Q_DECLARE_METATYPE(KWaylandServer::LayerSurfaceV1Interface::Layer)
Q_DECLARE_METATYPE(KWaylandServer::LayerSurfaceV1Interface *)

using namespace KWaylandServer;

class LayerShellV1 : public QtWayland::zwlr_layer_shell_v1
{
public:
    ~LayerShellV1() override { destroy(); }
};

class LayerSurfaceV1 : public QtWayland::zwlr_layer_surface_v1
{
public:
    ~LayerSurfaceV1() override { destroy(); }
};

class XdgShell : public QtWayland::xdg_wm_base
{
public:
    ~XdgShell() { destroy(); }
};

class XdgSurface : public QtWayland::xdg_surface
{
public:
    ~XdgSurface() { destroy(); }
};

class XdgPositioner : public QtWayland::xdg_positioner
{
public:
    ~XdgPositioner() { destroy(); }
};

class XdgPopup : public QtWayland::xdg_popup
{
public:
    ~XdgPopup() { destroy(); }
};

class TestLayerShellV1Interface : public QObject
{
    Q_OBJECT

public:
    ~TestLayerShellV1Interface() override;

private Q_SLOTS:
    void initTestCase();
    void testDesiredSize();
    void testScope();
    void testAnchor_data();
    void testAnchor();
    void testMargins();
    void testExclusiveZone();
    void testExclusiveEdge_data();
    void testExclusiveEdge();
    void testLayer_data();
    void testLayer();
    void testPopup();

private:
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue;
    KWayland::Client::Compositor *m_clientCompositor;

    QThread *m_thread;
    Display m_display;
    CompositorInterface *m_serverCompositor;
    LayerShellV1 *m_clientLayerShell = nullptr;
    LayerShellV1Interface *m_serverLayerShell = nullptr;
    XdgShell *m_clientXdgShell = nullptr;
    XdgShellInterface *m_serverXdgShell = nullptr;
};

static const QString s_socketName = QStringLiteral("kwin-wayland-server-layer-shell-v1-test-0");

void TestLayerShellV1Interface::initTestCase()
{
    m_display.addSocketName(s_socketName);
    m_display.start();
    QVERIFY(m_display.isRunning());

    m_serverLayerShell = new LayerShellV1Interface(&m_display, this);
    m_serverXdgShell = new XdgShellInterface(&m_display, this);
    m_serverCompositor = new CompositorInterface(&m_display, this);

    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());
    QVERIFY(!m_connection->connections().isEmpty());

    m_queue = new KWayland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    auto registry = new KWayland::Client::Registry(this);
    connect(registry, &KWayland::Client::Registry::interfaceAnnounced, this, [this, registry](const QByteArray &interface, quint32 id, quint32 version) {
        if (interface == QByteArrayLiteral("zwlr_layer_shell_v1")) {
            m_clientLayerShell = new LayerShellV1();
            m_clientLayerShell->init(*registry, id, version);
        }
        if (interface == QByteArrayLiteral("xdg_wm_base")) {
            m_clientXdgShell = new XdgShell();
            m_clientXdgShell->init(*registry, id, version);
        }
    });
    QSignalSpy allAnnouncedSpy(registry, &KWayland::Client::Registry::interfaceAnnounced);
    QSignalSpy compositorSpy(registry, &KWayland::Client::Registry::compositorAnnounced);
    QSignalSpy shmSpy(registry, &KWayland::Client::Registry::shmAnnounced);
    registry->setEventQueue(m_queue);
    registry->create(m_connection->display());
    QVERIFY(registry->isValid());
    registry->setup();
    QVERIFY(allAnnouncedSpy.wait());

    m_clientCompositor = registry->createCompositor(compositorSpy.first().first().value<quint32>(),
                                                    compositorSpy.first().last().value<quint32>(), this);
    QVERIFY(m_clientCompositor->isValid());
}

TestLayerShellV1Interface::~TestLayerShellV1Interface()
{
    if (m_clientXdgShell) {
        delete m_clientXdgShell;
        m_clientXdgShell = nullptr;
    }
    if (m_clientLayerShell) {
        delete m_clientLayerShell;
        m_clientLayerShell = nullptr;
    }
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    m_connection->deleteLater();
    m_connection = nullptr;
}

void TestLayerShellV1Interface::testDesiredSize()
{
    // Create a test wl_surface object.
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreatedSpy.isValid());
    QScopedPointer<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    // Create a test wlr_layer_surface_v1 object.
    QScopedPointer<LayerSurfaceV1> clientShellSurface(new LayerSurfaceV1);
    clientShellSurface->init(m_clientLayerShell->get_layer_surface(*clientSurface, nullptr,
                                                                   LayerShellV1::layer_top,
                                                                   QStringLiteral("test")));
    QSignalSpy layerSurfaceCreatedSpy(m_serverLayerShell, &LayerShellV1Interface::surfaceCreated);
    QVERIFY(layerSurfaceCreatedSpy.isValid());
    QVERIFY(layerSurfaceCreatedSpy.wait());
    auto serverShellSurface = layerSurfaceCreatedSpy.last().first().value<LayerSurfaceV1Interface *>();
    QVERIFY(serverShellSurface);

    clientShellSurface->set_size(10, 20);
    clientSurface->commit(KWayland::Client::Surface::CommitFlag::None);

    QSignalSpy desiredSizeChangedSpy(serverShellSurface, &LayerSurfaceV1Interface::desiredSizeChanged);
    QVERIFY(desiredSizeChangedSpy.isValid());
    QVERIFY(desiredSizeChangedSpy.wait());

    QCOMPARE(serverShellSurface->desiredSize(), QSize(10, 20));
}

void TestLayerShellV1Interface::testScope()
{
    // Create a test wl_surface object.
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreatedSpy.isValid());
    QScopedPointer<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    // Create a test wlr_layer_surface_v1 object.
    QScopedPointer<LayerSurfaceV1> clientShellSurface(new LayerSurfaceV1);
    clientShellSurface->init(m_clientLayerShell->get_layer_surface(*clientSurface, nullptr,
                                                                   LayerShellV1::layer_top,
                                                                   QStringLiteral("foobar")));
    clientShellSurface->set_size(100, 50);
    QSignalSpy layerSurfaceCreatedSpy(m_serverLayerShell, &LayerShellV1Interface::surfaceCreated);
    QVERIFY(layerSurfaceCreatedSpy.isValid());
    QVERIFY(layerSurfaceCreatedSpy.wait());
    auto serverShellSurface = layerSurfaceCreatedSpy.last().first().value<LayerSurfaceV1Interface *>();
    QVERIFY(serverShellSurface);

    QCOMPARE(serverShellSurface->scope(), QStringLiteral("foobar"));
}

void TestLayerShellV1Interface::testAnchor_data()
{
    QTest::addColumn<int>("anchor");
    QTest::addColumn<Qt::Edge>("expected");

    QTest::addRow("left")   << int(QtWayland::zwlr_layer_surface_v1::anchor_left)   << Qt::LeftEdge;
    QTest::addRow("right")  << int(QtWayland::zwlr_layer_surface_v1::anchor_right)  << Qt::RightEdge;
    QTest::addRow("top")    << int(QtWayland::zwlr_layer_surface_v1::anchor_top)    << Qt::TopEdge;
    QTest::addRow("bottom") << int(QtWayland::zwlr_layer_surface_v1::anchor_bottom) << Qt::BottomEdge;
}

void TestLayerShellV1Interface::testAnchor()
{
    // Create a test wl_surface object.
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreatedSpy.isValid());
    QScopedPointer<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    // Create a test wlr_layer_surface_v1 object.
    QScopedPointer<LayerSurfaceV1> clientShellSurface(new LayerSurfaceV1);
    clientShellSurface->init(m_clientLayerShell->get_layer_surface(*clientSurface, nullptr,
                                                                   LayerShellV1::layer_top,
                                                                   QStringLiteral("test")));
    QSignalSpy layerSurfaceCreatedSpy(m_serverLayerShell, &LayerShellV1Interface::surfaceCreated);
    QVERIFY(layerSurfaceCreatedSpy.isValid());
    QVERIFY(layerSurfaceCreatedSpy.wait());
    auto serverShellSurface = layerSurfaceCreatedSpy.last().first().value<LayerSurfaceV1Interface *>();
    QVERIFY(serverShellSurface);

    QFETCH(int, anchor);
    QFETCH(Qt::Edge, expected);

    clientShellSurface->set_anchor(anchor);
    clientShellSurface->set_size(100, 50);
    clientSurface->commit(KWayland::Client::Surface::CommitFlag::None);

    QSignalSpy anchorChangedSpy(serverShellSurface, &LayerSurfaceV1Interface::anchorChanged);
    QVERIFY(anchorChangedSpy.isValid());
    QVERIFY(anchorChangedSpy.wait());

    QCOMPARE(serverShellSurface->anchor(), expected);
}

void TestLayerShellV1Interface::testMargins()
{
    // Create a test wl_surface object.
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreatedSpy.isValid());
    QScopedPointer<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    // Create a test wlr_layer_surface_v1 object.
    QScopedPointer<LayerSurfaceV1> clientShellSurface(new LayerSurfaceV1);
    clientShellSurface->init(m_clientLayerShell->get_layer_surface(*clientSurface, nullptr,
                                                                   LayerShellV1::layer_top,
                                                                   QStringLiteral("test")));
    QSignalSpy layerSurfaceCreatedSpy(m_serverLayerShell, &LayerShellV1Interface::surfaceCreated);
    QVERIFY(layerSurfaceCreatedSpy.isValid());
    QVERIFY(layerSurfaceCreatedSpy.wait());
    auto serverShellSurface = layerSurfaceCreatedSpy.last().first().value<LayerSurfaceV1Interface *>();
    QVERIFY(serverShellSurface);

    clientShellSurface->set_margin(10, 20, 30, 40);
    clientShellSurface->set_size(100, 50);
    clientSurface->commit(KWayland::Client::Surface::CommitFlag::None);

    QSignalSpy marginsChangedSpy(serverShellSurface, &LayerSurfaceV1Interface::marginsChanged);
    QVERIFY(marginsChangedSpy.isValid());
    QVERIFY(marginsChangedSpy.wait());

    QCOMPARE(serverShellSurface->margins(), QMargins(40, 10, 20, 30));
}

void TestLayerShellV1Interface::testExclusiveZone()
{
    // Create a test wl_surface object.
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreatedSpy.isValid());
    QScopedPointer<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    // Create a test wlr_layer_surface_v1 object.
    QScopedPointer<LayerSurfaceV1> clientShellSurface(new LayerSurfaceV1);
    clientShellSurface->init(m_clientLayerShell->get_layer_surface(*clientSurface, nullptr,
                                                                   LayerShellV1::layer_top,
                                                                   QStringLiteral("test")));
    QSignalSpy layerSurfaceCreatedSpy(m_serverLayerShell, &LayerShellV1Interface::surfaceCreated);
    QVERIFY(layerSurfaceCreatedSpy.isValid());
    QVERIFY(layerSurfaceCreatedSpy.wait());
    auto serverShellSurface = layerSurfaceCreatedSpy.last().first().value<LayerSurfaceV1Interface *>();
    QVERIFY(serverShellSurface);

    clientShellSurface->set_exclusive_zone(10);
    clientShellSurface->set_size(100, 50);
    clientSurface->commit(KWayland::Client::Surface::CommitFlag::None);

    QSignalSpy exclusiveZoneChangedSpy(serverShellSurface, &LayerSurfaceV1Interface::exclusiveZoneChanged);
    QVERIFY(exclusiveZoneChangedSpy.isValid());
    QVERIFY(exclusiveZoneChangedSpy.wait());

    QCOMPARE(serverShellSurface->exclusiveZone(), 10);
}

void TestLayerShellV1Interface::testExclusiveEdge_data()
{
    QTest::addColumn<int>("anchor");
    QTest::addColumn<Qt::Edge>("expected");

    QTest::addRow("left (singular)")   << int(QtWayland::zwlr_layer_surface_v1::anchor_left)
                                       << Qt::LeftEdge;

    QTest::addRow("left (triplet)")    << (QtWayland::zwlr_layer_surface_v1::anchor_bottom |
                                           QtWayland::zwlr_layer_surface_v1::anchor_left |
                                           QtWayland::zwlr_layer_surface_v1::anchor_top )
                                       << Qt::LeftEdge;

    QTest::addRow("right (singular)")  << int(QtWayland::zwlr_layer_surface_v1::anchor_right)
                                       << Qt::RightEdge;

    QTest::addRow("right (triplet)")   << (QtWayland::zwlr_layer_surface_v1::anchor_top |
                                           QtWayland::zwlr_layer_surface_v1::anchor_right |
                                           QtWayland::zwlr_layer_surface_v1::anchor_bottom)
                                       << Qt::RightEdge;

    QTest::addRow("top (singular)")    << int(QtWayland::zwlr_layer_surface_v1::anchor_top)
                                       << Qt::TopEdge;

    QTest::addRow("top (triplet)")     << (QtWayland::zwlr_layer_surface_v1::anchor_left |
                                           QtWayland::zwlr_layer_surface_v1::anchor_top |
                                           QtWayland::zwlr_layer_surface_v1::anchor_right)
                                       << Qt::TopEdge;

    QTest::addRow("bottom (singular)") << int(QtWayland::zwlr_layer_surface_v1::anchor_bottom)
                                       << Qt::BottomEdge;

    QTest::addRow("bottom (triplet)")  << (QtWayland::zwlr_layer_surface_v1::anchor_right |
                                           QtWayland::zwlr_layer_surface_v1::anchor_bottom |
                                           QtWayland::zwlr_layer_surface_v1::anchor_left)
                                       << Qt::BottomEdge;

    QTest::addRow("all")               << (QtWayland::zwlr_layer_surface_v1::anchor_left |
                                           QtWayland::zwlr_layer_surface_v1::anchor_right |
                                           QtWayland::zwlr_layer_surface_v1::anchor_top |
                                           QtWayland::zwlr_layer_surface_v1::anchor_bottom)
                                       << Qt::Edge();
}

void TestLayerShellV1Interface::testExclusiveEdge()
{
    // Create a test wl_surface object.
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreatedSpy.isValid());
    QScopedPointer<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    // Create a test wlr_layer_surface_v1 object.
    QScopedPointer<LayerSurfaceV1> clientShellSurface(new LayerSurfaceV1);
    clientShellSurface->init(m_clientLayerShell->get_layer_surface(*clientSurface, nullptr,
                                                                   LayerShellV1::layer_top,
                                                                   QStringLiteral("test")));
    QSignalSpy layerSurfaceCreatedSpy(m_serverLayerShell, &LayerShellV1Interface::surfaceCreated);
    QVERIFY(layerSurfaceCreatedSpy.isValid());
    QVERIFY(layerSurfaceCreatedSpy.wait());
    auto serverShellSurface = layerSurfaceCreatedSpy.last().first().value<LayerSurfaceV1Interface *>();
    QVERIFY(serverShellSurface);

    QFETCH(int, anchor);
    QFETCH(Qt::Edge, expected);

    clientShellSurface->set_exclusive_zone(10);
    clientShellSurface->set_size(100, 50);
    clientShellSurface->set_anchor(anchor);
    clientSurface->commit(KWayland::Client::Surface::CommitFlag::None);

    QSignalSpy anchorChangedSpy(serverShellSurface, &LayerSurfaceV1Interface::anchorChanged);
    QVERIFY(anchorChangedSpy.isValid());
    QVERIFY(anchorChangedSpy.wait());

    QCOMPARE(serverShellSurface->exclusiveEdge(), expected);
}

void TestLayerShellV1Interface::testLayer_data()
{
    QTest::addColumn<int>("layer");
    QTest::addColumn<LayerSurfaceV1Interface::Layer>("expected");

    QTest::addRow("overlay")    << int(QtWayland::zwlr_layer_shell_v1::layer_overlay)
                                << LayerSurfaceV1Interface::OverlayLayer;
    QTest::addRow("top")        << int(QtWayland::zwlr_layer_shell_v1::layer_top)
                                << LayerSurfaceV1Interface::TopLayer;
    QTest::addRow("bottom")     << int(QtWayland::zwlr_layer_shell_v1::layer_bottom)
                                << LayerSurfaceV1Interface::BottomLayer;
    QTest::addRow("background") << int(QtWayland::zwlr_layer_shell_v1::layer_background)
                                << LayerSurfaceV1Interface::BackgroundLayer;
}

void TestLayerShellV1Interface::testLayer()
{
    // Create a test wl_surface object.
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreatedSpy.isValid());
    QScopedPointer<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    // Create a test wlr_layer_surface_v1 object.
    QScopedPointer<LayerSurfaceV1> clientShellSurface(new LayerSurfaceV1);
    clientShellSurface->init(m_clientLayerShell->get_layer_surface(*clientSurface, nullptr,
                                                                   LayerShellV1::layer_top,
                                                                   QStringLiteral("test")));
    QSignalSpy layerSurfaceCreatedSpy(m_serverLayerShell, &LayerShellV1Interface::surfaceCreated);
    QVERIFY(layerSurfaceCreatedSpy.isValid());
    QVERIFY(layerSurfaceCreatedSpy.wait());
    auto serverShellSurface = layerSurfaceCreatedSpy.last().first().value<LayerSurfaceV1Interface *>();
    QVERIFY(serverShellSurface);

    QFETCH(int, layer);
    QFETCH(LayerSurfaceV1Interface::Layer, expected);

    clientShellSurface->set_layer(layer);
    clientShellSurface->set_size(100, 50);
    clientSurface->commit(KWayland::Client::Surface::CommitFlag::None);

    QSignalSpy committedSpy(serverSurface, &SurfaceInterface::committed);
    QVERIFY(committedSpy.isValid());
    QVERIFY(committedSpy.wait());

    QCOMPARE(serverShellSurface->layer(), expected);
}

void TestLayerShellV1Interface::testPopup()
{
    // Create a test wl_surface object for the panel.
    QSignalSpy serverPanelSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    QVERIFY(serverPanelSurfaceCreatedSpy.isValid());
    QScopedPointer<KWayland::Client::Surface> clientPanelSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverPanelSurfaceCreatedSpy.wait());
    SurfaceInterface *serverPanelSurface = serverPanelSurfaceCreatedSpy.last().first().value<SurfaceInterface *>();
    QVERIFY(serverPanelSurface);

    // Create a test wlr_layer_surface_v1 object for the panel..
    QScopedPointer<LayerSurfaceV1> clientPanelShellSurface(new LayerSurfaceV1);
    clientPanelShellSurface->init(m_clientLayerShell->get_layer_surface(*clientPanelSurface, nullptr,
                                                                        LayerShellV1::layer_top,
                                                                        QStringLiteral("panel")));
    clientPanelShellSurface->set_size(100, 50);
    QSignalSpy layerSurfaceCreatedSpy(m_serverLayerShell, &LayerShellV1Interface::surfaceCreated);
    QVERIFY(layerSurfaceCreatedSpy.isValid());
    QVERIFY(layerSurfaceCreatedSpy.wait());
    auto serverPanelShellSurface = layerSurfaceCreatedSpy.last().first().value<LayerSurfaceV1Interface *>();
    QVERIFY(serverPanelShellSurface);

    // Create a wl_surface object for the popup.
    QScopedPointer<KWayland::Client::Surface> clientPopupSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverPanelSurfaceCreatedSpy.wait());
    SurfaceInterface *serverPopupSurface = serverPanelSurfaceCreatedSpy.last().first().value<SurfaceInterface *>();
    QVERIFY(serverPopupSurface);

    // Create an xdg_surface object for the popup.
    QScopedPointer<XdgSurface> clientXdgSurface(new XdgSurface);
    clientXdgSurface->init(m_clientXdgShell->get_xdg_surface(*clientPopupSurface));

    // Create an xdg_positioner object for the popup.
    QScopedPointer<::XdgPositioner> positioner(new ::XdgPositioner);
    positioner->init(m_clientXdgShell->create_positioner());
    positioner->set_size(100, 100);
    positioner->set_anchor_rect(0, 0, 10, 10);

    // Create an xdg_popup surface.
    QScopedPointer<XdgPopup> clientXdgPopup(new XdgPopup);
    clientXdgPopup->init(clientXdgSurface->get_popup(nullptr, positioner->object()));

    // Wait for the server side to catch up.
    QSignalSpy popupCreatedSpy(m_serverXdgShell, &XdgShellInterface::popupCreated);
    QVERIFY(popupCreatedSpy.wait());
    XdgPopupInterface *serverPopupShellSurface = popupCreatedSpy.last().first().value<XdgPopupInterface *>();
    QVERIFY(serverPopupShellSurface);
    QCOMPARE(serverPopupShellSurface->parentSurface(), nullptr);

    // Make the xdg_popup surface a child of the panel.
    clientPanelShellSurface->get_popup(clientXdgPopup->object());

    // Commit the initial state of the xdg_popup surface.
    clientPopupSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy initializeRequestedSpy(serverPopupShellSurface, &XdgPopupInterface::initializeRequested);
    QVERIFY(initializeRequestedSpy.isValid());
    QVERIFY(initializeRequestedSpy.wait());

    // The popup should be a transient for the panel.
    QCOMPARE(serverPopupShellSurface->parentSurface(), serverPanelSurface);
}

QTEST_GUILESS_MAIN(TestLayerShellV1Interface)

#include "test_layershellv1_interface.moc"
