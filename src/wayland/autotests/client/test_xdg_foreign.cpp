/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
Copyright 2017  Marco Martin <mart@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
// Qt
#include <QtTest/QtTest>
// KWin
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/region.h"
#include "../../src/client/registry.h"
#include "../../src/client/surface.h"
#include "../../src/client/xdgforeign.h"
#include "../../src/server/display.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/surface_interface.h"
#include "../../src/server/xdgforeign_interface.h"

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

private:
    void doExport();

    KWayland::Server::Display *m_display;
    KWayland::Server::CompositorInterface *m_compositorInterface;
    KWayland::Server::XdgForeignInterface *m_foreignInterface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::EventQueue *m_queue;
    KWayland::Client::XdgExporter *m_exporter;
    KWayland::Client::XdgImporter *m_importer;

    QPointer<KWayland::Client::Surface> m_exportedSurface;
    QPointer<KWayland::Server::SurfaceInterface> m_exportedSurfaceInterface;

    QPointer<KWayland::Client::XdgExported> m_exported;
    QPointer<KWayland::Client::XdgImported> m_imported;

    QPointer<KWayland::Client::Surface> m_childSurface;
    QPointer<KWayland::Server::SurfaceInterface> m_childSurfaceInterface;

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
    using namespace KWayland::Server;
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    qRegisterMetaType<KWayland::Server::SurfaceInterface*>("KWayland::Server::SurfaceInterface");

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::connected);
    QVERIFY(connectedSpy.isValid());
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

    Registry registry;
    QSignalSpy compositorSpy(&registry, &Registry::compositorAnnounced);
    QVERIFY(compositorSpy.isValid());

    QSignalSpy exporterSpy(&registry, &Registry::exporterUnstableV2Announced);
    QVERIFY(exporterSpy.isValid());

    QSignalSpy importerSpy(&registry, &Registry::importerUnstableV2Announced);
    QVERIFY(importerSpy.isValid());

    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_compositorInterface = m_display->createCompositor(m_display);
    m_compositorInterface->create();
    QVERIFY(m_compositorInterface->isValid());

    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    m_foreignInterface = m_display->createXdgForeignInterface(m_display);
    m_foreignInterface->create();
    QVERIFY(m_foreignInterface->isValid());
    
    QVERIFY(exporterSpy.wait());
    //Both importer and exporter should have been triggered by now
    QCOMPARE(exporterSpy.count(), 1);
    QCOMPARE(importerSpy.count(), 1);

    m_exporter = registry.createXdgExporter(exporterSpy.first().first().value<quint32>(), exporterSpy.first().last().value<quint32>(), this);
    m_importer = registry.createXdgImporter(importerSpy.first().first().value<quint32>(), importerSpy.first().last().value<quint32>(), this);
}

void TestForeign::cleanup()
{
#define CLEANUP(variable) \
    if (variable) { \
        delete variable; \
        variable = nullptr; \
    }

    //some tests delete it beforehand
    if (m_exportedSurfaceInterface) {
        QSignalSpy exportedSurfaceDestroyedSpy(m_exportedSurfaceInterface.data(), &QObject::destroyed);
        QVERIFY(exportedSurfaceDestroyedSpy.isValid());
        CLEANUP(m_exportedSurface)
        exportedSurfaceDestroyedSpy.wait();
    }

    if (m_childSurfaceInterface) {
        QSignalSpy childSurfaceDestroyedSpy(m_childSurfaceInterface.data(), &QObject::destroyed);
        QVERIFY(childSurfaceDestroyedSpy.isValid());
        CLEANUP(m_childSurface)
        childSurfaceDestroyedSpy.wait();
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
    CLEANUP(m_compositorInterface)
    CLEANUP(m_foreignInterface)
    CLEANUP(m_display)
#undef CLEANUP
}

void TestForeign::doExport()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    m_exportedSurface = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());

    m_exportedSurfaceInterface = serverSurfaceCreated.first().first().value<KWayland::Server::SurfaceInterface*>();

    //Export a window
    m_exported = m_exporter->exportTopLevel(m_exportedSurface, this);
    QVERIFY(m_exported->handle().isEmpty());
    QSignalSpy doneSpy(m_exported.data(), &XdgExported::done);
    QVERIFY(doneSpy.wait());
    QVERIFY(!m_exported->handle().isEmpty());

    QSignalSpy transientSpy(m_foreignInterface, &KWayland::Server::XdgForeignInterface::transientChanged);
    QVERIFY(transientSpy.isValid());

    //Import the just exported window
    m_imported = m_importer->importTopLevel(m_exported->handle(), this);
    QVERIFY(m_imported->isValid());

    QSignalSpy childSurfaceInterfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());
    m_childSurface = m_compositor->createSurface();
    QVERIFY(childSurfaceInterfaceCreated.wait());
    m_childSurfaceInterface = childSurfaceInterfaceCreated.first().first().value<KWayland::Server::SurfaceInterface*>();
    m_childSurface->commit(Surface::CommitFlag::None);

    m_imported->setParentOf(m_childSurface);
    QVERIFY(transientSpy.wait());

    QCOMPARE(transientSpy.first().first().value<KWayland::Server::SurfaceInterface *>(), m_childSurfaceInterface.data());
    QCOMPARE(transientSpy.first().at(1).value<KWayland::Server::SurfaceInterface *>(), m_exportedSurfaceInterface.data());

    //transientFor api
    QCOMPARE(m_foreignInterface->transientFor(m_childSurfaceInterface), m_exportedSurfaceInterface.data());
}

void TestForeign::testExport()
{
    doExport();
}

void TestForeign::testDeleteImported()
{
    doExport();

    QSignalSpy transientSpy(m_foreignInterface, &KWayland::Server::XdgForeignInterface::transientChanged);
 
    QVERIFY(transientSpy.isValid());
    m_imported->deleteLater();
    m_imported = nullptr;

    QVERIFY(transientSpy.wait());

    QCOMPARE(transientSpy.first().first().value<KWayland::Server::SurfaceInterface *>(), m_childSurfaceInterface.data());
    QCOMPARE(transientSpy.first().at(1).value<KWayland::Server::SurfaceInterface *>(), nullptr);
    QCOMPARE(m_foreignInterface->transientFor(m_childSurfaceInterface), nullptr);
}

void TestForeign::testDeleteChildSurface()
{
    doExport();

    QSignalSpy transientSpy(m_foreignInterface, &KWayland::Server::XdgForeignInterface::transientChanged);
 
    QVERIFY(transientSpy.isValid());
    m_childSurface->deleteLater();

    QVERIFY(transientSpy.wait());

    //when the client surface dies, the server one will eventually die too
    QSignalSpy surfaceDestroyedSpy(m_childSurfaceInterface, SIGNAL(destroyed()));
    QVERIFY(surfaceDestroyedSpy.wait());

    QCOMPARE(transientSpy.first().at(0).value<KWayland::Server::SurfaceInterface *>(), nullptr);
    QCOMPARE(transientSpy.first().at(1).value<KWayland::Server::SurfaceInterface *>(), m_exportedSurfaceInterface.data());    
}

void TestForeign::testDeleteParentSurface()
{
    doExport();

    QSignalSpy transientSpy(m_foreignInterface, &KWayland::Server::XdgForeignInterface::transientChanged);
 
    QVERIFY(transientSpy.isValid());
    m_exportedSurface->deleteLater();

    QSignalSpy exportedSurfaceDestroyedSpy(m_exportedSurfaceInterface.data(), &QObject::destroyed);
    QVERIFY(exportedSurfaceDestroyedSpy.isValid());
    exportedSurfaceDestroyedSpy.wait();

    QVERIFY(transientSpy.wait());

    QCOMPARE(transientSpy.first().first().value<KWayland::Server::SurfaceInterface *>(), m_childSurfaceInterface.data());
    QCOMPARE(transientSpy.first().at(1).value<KWayland::Server::SurfaceInterface *>(), nullptr);
    QCOMPARE(m_foreignInterface->transientFor(m_childSurfaceInterface), nullptr);   
}

void TestForeign::testDeleteExported()
{
    doExport();

    QSignalSpy transientSpy(m_foreignInterface, &KWayland::Server::XdgForeignInterface::transientChanged);
    QSignalSpy destroyedSpy(m_imported.data(), &KWayland::Client::XdgImported::importedDestroyed);
 
    QVERIFY(transientSpy.isValid());
    m_exported->deleteLater();
    m_exported = nullptr;

    QVERIFY(transientSpy.wait());
    QVERIFY(destroyedSpy.wait());

    QCOMPARE(transientSpy.first().first().value<KWayland::Server::SurfaceInterface *>(), m_childSurfaceInterface.data());
    QCOMPARE(transientSpy.first().at(1).value<KWayland::Server::SurfaceInterface *>(), nullptr);
    QCOMPARE(m_foreignInterface->transientFor(m_childSurfaceInterface), nullptr);

    QVERIFY(!m_imported->isValid());
}

void TestForeign::testExportTwoTimes()
{
    doExport();

    //Export second window
    KWayland::Client::XdgExported *exported2 = m_exporter->exportTopLevel(m_exportedSurface, this);
    QVERIFY(exported2->handle().isEmpty());
    QSignalSpy doneSpy(exported2, &XdgExported::done);
    QVERIFY(doneSpy.wait());
    QVERIFY(!exported2->handle().isEmpty());

    QSignalSpy transientSpy(m_foreignInterface, &KWayland::Server::XdgForeignInterface::transientChanged);
    QVERIFY(transientSpy.isValid());

    //Import the just exported window
    KWayland::Client::XdgImported *imported2 = m_importer->importTopLevel(exported2->handle(), this);
    QVERIFY(imported2->isValid());

    //create a second child surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    KWayland::Client::Surface *childSurface2 = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());

    KWayland::Server::SurfaceInterface *childSurface2Interface = serverSurfaceCreated.first().first().value<KWayland::Server::SurfaceInterface*>();

    imported2->setParentOf(childSurface2);
    QVERIFY(transientSpy.wait());
    
    QCOMPARE(transientSpy.first().first().value<KWayland::Server::SurfaceInterface *>(), childSurface2Interface);
    QCOMPARE(transientSpy.first().at(1).value<KWayland::Server::SurfaceInterface *>(), m_exportedSurfaceInterface.data());

    //transientFor api
    //check the old relationship is still here
    QCOMPARE(m_foreignInterface->transientFor(m_childSurfaceInterface), m_exportedSurfaceInterface.data());
    //check the new relationship
    QCOMPARE(m_foreignInterface->transientFor(childSurface2Interface), m_exportedSurfaceInterface.data());
}

void TestForeign::testImportTwoTimes()
{
    doExport();

    QSignalSpy transientSpy(m_foreignInterface, &KWayland::Server::XdgForeignInterface::transientChanged);
    QVERIFY(transientSpy.isValid());

    //Import another time the exported window
    KWayland::Client::XdgImported *imported2 = m_importer->importTopLevel(m_exported->handle(), this);
    QVERIFY(imported2->isValid());

    //create a second child surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    KWayland::Client::Surface *childSurface2 = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());

    KWayland::Server::SurfaceInterface *childSurface2Interface = serverSurfaceCreated.first().first().value<KWayland::Server::SurfaceInterface*>();

    imported2->setParentOf(childSurface2);
    QVERIFY(transientSpy.wait());
    
    QCOMPARE(transientSpy.first().first().value<KWayland::Server::SurfaceInterface *>(), childSurface2Interface);
    QCOMPARE(transientSpy.first().at(1).value<KWayland::Server::SurfaceInterface *>(), m_exportedSurfaceInterface.data());

    //transientFor api
    //check the old relationship is still here
    QCOMPARE(m_foreignInterface->transientFor(m_childSurfaceInterface), m_exportedSurfaceInterface.data());
    //check the new relationship
    QCOMPARE(m_foreignInterface->transientFor(childSurface2Interface), m_exportedSurfaceInterface.data());
}

QTEST_GUILESS_MAIN(TestForeign)
#include "test_xdg_foreign.moc"
