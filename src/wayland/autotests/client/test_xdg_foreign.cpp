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

    KWaylandServer::Display *m_display;
    QPointer<KWaylandServer::CompositorInterface> m_compositorInterface;
    KWaylandServer::XdgForeignV2Interface *m_foreignInterface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::EventQueue *m_queue;
    KWayland::Client::XdgExporter *m_exporter;
    KWayland::Client::XdgImporter *m_importer;

    QPointer<KWayland::Client::Surface> m_exportedSurface;
    QPointer<KWaylandServer::SurfaceInterface> m_exportedSurfaceInterface;

    QPointer<KWayland::Client::XdgExported> m_exported;
    QPointer<KWayland::Client::XdgImported> m_imported;

    QPointer<KWayland::Client::Surface> m_childSurface;
    QPointer<KWaylandServer::SurfaceInterface> m_childSurfaceInterface;

    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwayland-test-xdg-foreign-0");

TestForeign::TestForeign(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_exporter(nullptr)
    , m_importer(nullptr)
    , m_thread(nullptr)
{
}

void TestForeign::init()
{
    using namespace KWaylandServer;
    delete m_display;
    m_display = new KWaylandServer::Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    qRegisterMetaType<SurfaceInterface *>();
    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new KWayland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    KWayland::Client::Registry registry;
    QSignalSpy compositorSpy(&registry, &KWayland::Client::Registry::compositorAnnounced);

    QSignalSpy exporterSpy(&registry, &KWayland::Client::Registry::exporterUnstableV2Announced);

    QSignalSpy importerSpy(&registry, &KWayland::Client::Registry::importerUnstableV2Announced);

    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    m_foreignInterface = new XdgForeignV2Interface(m_display, m_display);

    QVERIFY(exporterSpy.wait());
    // Both importer and exporter should have been triggered by now
    QCOMPARE(exporterSpy.count(), 1);
    QCOMPARE(importerSpy.count(), 1);

    m_exporter = registry.createXdgExporter(exporterSpy.first().first().value<quint32>(), exporterSpy.first().last().value<quint32>(), this);
    m_importer = registry.createXdgImporter(importerSpy.first().first().value<quint32>(), importerSpy.first().last().value<quint32>(), this);
}

void TestForeign::cleanup()
{
#define CLEANUP(variable)   \
    if (variable) {         \
        delete variable;    \
        variable = nullptr; \
    }

    CLEANUP(m_compositor)
    CLEANUP(m_exporter)
    CLEANUP(m_importer)
    CLEANUP(m_queue)
    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    delete m_display;
    m_display = nullptr;
#undef CLEANUP

    // these are the children of the display
    m_foreignInterface = nullptr;
}

void TestForeign::doExport()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface.data(), &KWaylandServer::CompositorInterface::surfaceCreated);

    m_exportedSurface = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());

    m_exportedSurfaceInterface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface *>();

    // Export a window
    m_exported = m_exporter->exportTopLevel(m_exportedSurface);
    QVERIFY(m_exported->handle().isEmpty());
    QSignalSpy doneSpy(m_exported.data(), &KWayland::Client::XdgExported::done);
    QVERIFY(doneSpy.wait());
    QVERIFY(!m_exported->handle().isEmpty());

    QSignalSpy transientSpy(m_foreignInterface, &KWaylandServer::XdgForeignV2Interface::transientChanged);

    // Import the just exported window
    m_imported = m_importer->importTopLevel(m_exported->handle());
    QVERIFY(m_imported->isValid());

    QSignalSpy childSurfaceInterfaceCreated(m_compositorInterface.data(), &KWaylandServer::CompositorInterface::surfaceCreated);
    m_childSurface = m_compositor->createSurface();
    QVERIFY(childSurfaceInterfaceCreated.wait());
    m_childSurfaceInterface = childSurfaceInterfaceCreated.first().first().value<KWaylandServer::SurfaceInterface *>();
    m_childSurface->commit(KWayland::Client::Surface::CommitFlag::None);

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

    QSignalSpy transientSpy(m_foreignInterface, &KWaylandServer::XdgForeignV2Interface::transientChanged);

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

    QSignalSpy transientSpy(m_foreignInterface, &KWaylandServer::XdgForeignV2Interface::transientChanged);

    m_childSurface->deleteLater();

    QVERIFY(transientSpy.wait());

    QVERIFY(!transientSpy.first().at(0).value<KWaylandServer::SurfaceInterface *>());
    QCOMPARE(transientSpy.first().at(1).value<KWaylandServer::SurfaceInterface *>(), m_exportedSurfaceInterface.data());
}

void TestForeign::testDeleteParentSurface()
{
    doExport();

    QSignalSpy transientSpy(m_foreignInterface, &KWaylandServer::XdgForeignV2Interface::transientChanged);
    m_exportedSurface->deleteLater();
    QVERIFY(transientSpy.wait());

    QCOMPARE(transientSpy.first().first().value<KWaylandServer::SurfaceInterface *>(), m_childSurfaceInterface.data());
    QVERIFY(!transientSpy.first().at(1).value<KWaylandServer::SurfaceInterface *>());
    QVERIFY(!m_foreignInterface->transientFor(m_childSurfaceInterface));
}

void TestForeign::testDeleteExported()
{
    doExport();

    QSignalSpy transientSpy(m_foreignInterface, &KWaylandServer::XdgForeignV2Interface::transientChanged);
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
    QSignalSpy doneSpy(exported2, &KWayland::Client::XdgExported::done);
    QVERIFY(doneSpy.wait());
    QVERIFY(!exported2->handle().isEmpty());

    QSignalSpy transientSpy(m_foreignInterface, &KWaylandServer::XdgForeignV2Interface::transientChanged);

    // Import the just exported window
    KWayland::Client::XdgImported *imported2 = m_importer->importTopLevel(exported2->handle());
    QVERIFY(imported2->isValid());

    // create a second child surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface.data(), &KWaylandServer::CompositorInterface::surfaceCreated);

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

    QSignalSpy transientSpy(m_foreignInterface, &KWaylandServer::XdgForeignV2Interface::transientChanged);

    // Import another time the exported window
    KWayland::Client::XdgImported *imported2 = m_importer->importTopLevel(m_exported->handle());
    QVERIFY(imported2->isValid());

    // create a second child surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface.data(), &KWaylandServer::CompositorInterface::surfaceCreated);

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
