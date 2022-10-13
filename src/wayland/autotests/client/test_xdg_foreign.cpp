/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2017 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/surface_interface.h"
#include "wayland/xdgforeign_v2_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/region.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/surface.h"
#include "KWayland/Client/xdgforeign.h"

using namespace KWayland::Client;

class TestForeign : public QObject
{
    Q_OBJECT
public:
    explicit TestForeign(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testExport();
    void testDeleteImported();
    void testDeleteChildSurface();
    void testDeleteParentSurface();
    void testDeleteExported();
    void testExportTwoTimes();
    void testImportTwoTimes();
    void testImportInvalidToplevel();

private:
    void doExport();

    std::unique_ptr<KWaylandServer::Display> m_display;
    std::unique_ptr<KWaylandServer::CompositorInterface> m_compositorInterface;
    std::unique_ptr<KWaylandServer::XdgForeignV2Interface> m_foreignInterface;
    std::unique_ptr<KWayland::Client::ConnectionThread> m_connection;
    std::unique_ptr<KWayland::Client::Compositor> m_compositor;
    std::unique_ptr<KWayland::Client::EventQueue> m_queue;
    std::unique_ptr<KWayland::Client::XdgExporter> m_exporter;
    std::unique_ptr<KWayland::Client::XdgImporter> m_importer;

    QPointer<KWayland::Client::Surface> m_exportedSurface;
    QPointer<KWaylandServer::SurfaceInterface> m_exportedSurfaceInterface;

    QPointer<KWayland::Client::XdgExported> m_exported;
    QPointer<KWayland::Client::XdgImported> m_imported;

    QPointer<KWayland::Client::Surface> m_childSurface;
    QPointer<KWaylandServer::SurfaceInterface> m_childSurfaceInterface;

    std::unique_ptr<QThread> m_thread;
};

static const QString s_socketName = QStringLiteral("kwayland-test-xdg-foreign-0");

TestForeign::TestForeign(QObject *parent)
    : QObject(parent)
{
}

void TestForeign::init()
{
    using namespace KWaylandServer;
    m_display = std::make_unique<KWaylandServer::Display>();
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    qRegisterMetaType<SurfaceInterface *>();
    // setup connection
    m_connection = std::make_unique<KWayland::Client::ConnectionThread>();
    QSignalSpy connectedSpy(m_connection.get(), &ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = std::make_unique<QThread>();
    m_connection->moveToThread(m_thread.get());
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = std::make_unique<KWayland::Client::EventQueue>();
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection.get());
    QVERIFY(m_queue->isValid());

    Registry registry;
    QSignalSpy compositorSpy(&registry, &Registry::compositorAnnounced);

    QSignalSpy exporterSpy(&registry, &Registry::exporterUnstableV2Announced);

    QSignalSpy importerSpy(&registry, &Registry::importerUnstableV2Announced);

    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue.get());
    QCOMPARE(registry.eventQueue(), m_queue.get());
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_compositorInterface = std::make_unique<CompositorInterface>(m_display.get());
    QVERIFY(compositorSpy.wait());
    m_compositor.reset(registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>()));

    m_foreignInterface = std::make_unique<XdgForeignV2Interface>(m_display.get());

    QVERIFY(exporterSpy.wait());
    // Both importer and exporter should have been triggered by now
    QCOMPARE(exporterSpy.count(), 1);
    QCOMPARE(importerSpy.count(), 1);

    m_exporter.reset(registry.createXdgExporter(exporterSpy.first().first().value<quint32>(), exporterSpy.first().last().value<quint32>()));
    m_importer.reset(registry.createXdgImporter(importerSpy.first().first().value<quint32>(), importerSpy.first().last().value<quint32>()));
}

void TestForeign::cleanup()
{
    m_compositor.reset();
    m_exporter.reset();
    m_importer.reset();
    m_queue.reset();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        m_thread.reset();
    }
    m_connection.reset();

    m_display.reset();
}

void TestForeign::doExport()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface.get(), &KWaylandServer::CompositorInterface::surfaceCreated);

    m_exportedSurface = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());

    m_exportedSurfaceInterface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface *>();

    // Export a window
    m_exported = m_exporter->exportTopLevel(m_exportedSurface);
    QVERIFY(m_exported->handle().isEmpty());
    QSignalSpy doneSpy(m_exported.data(), &XdgExported::done);
    QVERIFY(doneSpy.wait());
    QVERIFY(!m_exported->handle().isEmpty());

    QSignalSpy transientSpy(m_foreignInterface.get(), &KWaylandServer::XdgForeignV2Interface::transientChanged);

    // Import the just exported window
    m_imported = m_importer->importTopLevel(m_exported->handle());
    QVERIFY(m_imported->isValid());

    QSignalSpy childSurfaceInterfaceCreated(m_compositorInterface.get(), &KWaylandServer::CompositorInterface::surfaceCreated);
    m_childSurface = m_compositor->createSurface();
    QVERIFY(childSurfaceInterfaceCreated.wait());
    m_childSurfaceInterface = childSurfaceInterfaceCreated.first().first().value<KWaylandServer::SurfaceInterface *>();
    m_childSurface->commit(Surface::CommitFlag::None);

    m_imported->setParentOf(m_childSurface);
    QVERIFY(transientSpy.wait());

    QCOMPARE(transientSpy.first().first().value<KWaylandServer::SurfaceInterface *>(), m_childSurfaceInterface.data());
    QCOMPARE(transientSpy.first().at(1).value<KWaylandServer::SurfaceInterface *>(), m_exportedSurfaceInterface.data());

    // transientFor api
    QCOMPARE(m_foreignInterface->transientFor(m_childSurfaceInterface), m_exportedSurfaceInterface.data());
}

void TestForeign::testExport()
{
    doExport();
}

void TestForeign::testDeleteImported()
{
    doExport();

    QSignalSpy transientSpy(m_foreignInterface.get(), &KWaylandServer::XdgForeignV2Interface::transientChanged);

    m_imported->deleteLater();
    m_imported = nullptr;

    QVERIFY(transientSpy.wait());

    QCOMPARE(transientSpy.first().first().value<KWaylandServer::SurfaceInterface *>(), m_childSurfaceInterface.data());
    QVERIFY(!transientSpy.first().at(1).value<KWaylandServer::SurfaceInterface *>());
    QVERIFY(!m_foreignInterface->transientFor(m_childSurfaceInterface));
}

void TestForeign::testDeleteChildSurface()
{
    doExport();

    QSignalSpy transientSpy(m_foreignInterface.get(), &KWaylandServer::XdgForeignV2Interface::transientChanged);

    m_childSurface->deleteLater();

    QVERIFY(transientSpy.wait());

    QVERIFY(!transientSpy.first().at(0).value<KWaylandServer::SurfaceInterface *>());
    QCOMPARE(transientSpy.first().at(1).value<KWaylandServer::SurfaceInterface *>(), m_exportedSurfaceInterface.data());
}

void TestForeign::testDeleteParentSurface()
{
    doExport();

    QSignalSpy transientSpy(m_foreignInterface.get(), &KWaylandServer::XdgForeignV2Interface::transientChanged);
    m_exportedSurface->deleteLater();
    QVERIFY(transientSpy.wait());

    QCOMPARE(transientSpy.first().first().value<KWaylandServer::SurfaceInterface *>(), m_childSurfaceInterface.data());
    QVERIFY(!transientSpy.first().at(1).value<KWaylandServer::SurfaceInterface *>());
    QVERIFY(!m_foreignInterface->transientFor(m_childSurfaceInterface));
}

void TestForeign::testDeleteExported()
{
    doExport();

    QSignalSpy transientSpy(m_foreignInterface.get(), &KWaylandServer::XdgForeignV2Interface::transientChanged);
    QSignalSpy destroyedSpy(m_imported.data(), &KWayland::Client::XdgImported::importedDestroyed);

    m_exported->deleteLater();
    m_exported = nullptr;

    QVERIFY(transientSpy.wait());
    QVERIFY(destroyedSpy.wait());

    QCOMPARE(transientSpy.first().first().value<KWaylandServer::SurfaceInterface *>(), m_childSurfaceInterface.data());
    QVERIFY(!transientSpy.first().at(1).value<KWaylandServer::SurfaceInterface *>());
    QVERIFY(!m_foreignInterface->transientFor(m_childSurfaceInterface));

    QVERIFY(!m_imported->isValid());
}

void TestForeign::testExportTwoTimes()
{
    doExport();

    // Export second window
    KWayland::Client::XdgExported *exported2 = m_exporter->exportTopLevel(m_exportedSurface);
    QVERIFY(exported2->handle().isEmpty());
    QSignalSpy doneSpy(exported2, &XdgExported::done);
    QVERIFY(doneSpy.wait());
    QVERIFY(!exported2->handle().isEmpty());

    QSignalSpy transientSpy(m_foreignInterface.get(), &KWaylandServer::XdgForeignV2Interface::transientChanged);

    // Import the just exported window
    KWayland::Client::XdgImported *imported2 = m_importer->importTopLevel(exported2->handle());
    QVERIFY(imported2->isValid());

    // create a second child surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface.get(), &KWaylandServer::CompositorInterface::surfaceCreated);

    KWayland::Client::Surface *childSurface2 = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());

    KWaylandServer::SurfaceInterface *childSurface2Interface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface *>();

    imported2->setParentOf(childSurface2);
    QVERIFY(transientSpy.wait());

    QCOMPARE(transientSpy.first().first().value<KWaylandServer::SurfaceInterface *>(), childSurface2Interface);
    QCOMPARE(transientSpy.first().at(1).value<KWaylandServer::SurfaceInterface *>(), m_exportedSurfaceInterface.data());

    // transientFor api
    // check the old relationship is still here
    QCOMPARE(m_foreignInterface->transientFor(m_childSurfaceInterface), m_exportedSurfaceInterface.data());
    // check the new relationship
    QCOMPARE(m_foreignInterface->transientFor(childSurface2Interface), m_exportedSurfaceInterface.data());
}

void TestForeign::testImportTwoTimes()
{
    doExport();

    QSignalSpy transientSpy(m_foreignInterface.get(), &KWaylandServer::XdgForeignV2Interface::transientChanged);

    // Import another time the exported window
    KWayland::Client::XdgImported *imported2 = m_importer->importTopLevel(m_exported->handle());
    QVERIFY(imported2->isValid());

    // create a second child surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface.get(), &KWaylandServer::CompositorInterface::surfaceCreated);

    KWayland::Client::Surface *childSurface2 = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());

    KWaylandServer::SurfaceInterface *childSurface2Interface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface *>();

    imported2->setParentOf(childSurface2);
    QVERIFY(transientSpy.wait());

    QCOMPARE(transientSpy.first().first().value<KWaylandServer::SurfaceInterface *>(), childSurface2Interface);
    QCOMPARE(transientSpy.first().at(1).value<KWaylandServer::SurfaceInterface *>(), m_exportedSurfaceInterface.data());

    // transientFor api
    // check the old relationship is still here
    QCOMPARE(m_foreignInterface->transientFor(m_childSurfaceInterface), m_exportedSurfaceInterface.data());
    // check the new relationship
    QCOMPARE(m_foreignInterface->transientFor(childSurface2Interface), m_exportedSurfaceInterface.data());
}

void TestForeign::testImportInvalidToplevel()
{
    // This test verifies that the compositor properly handles the case where a client
    // attempts to import a toplevel with an invalid handle.

    KWayland::Client::XdgImported *imported = m_importer->importTopLevel(QStringLiteral("foobar"));
    QVERIFY(imported->isValid());

    QSignalSpy importedDestroySpy(imported, &KWayland::Client::XdgImported::importedDestroyed);
    QVERIFY(importedDestroySpy.wait());
}

QTEST_GUILESS_MAIN(TestForeign)
#include "test_xdg_foreign.moc"
