/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QMimeDatabase>
#include <QtTest>

#include "wayland/datadevicemanager_interface.h"
#include "wayland/datasource_interface.h"
#include "wayland/display.h"

// KWayland
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/datadevicemanager.h"
#include "KWayland/Client/datasource.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"

// Wayland
#include <wayland-client.h>

class TestDataSource : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testOffer();
    void testTargetAccepts_data();
    void testTargetAccepts();
    void testRequestSend();
    void testCancel();
    void testServerGet();

private:
    KWaylandServer::Display *m_display = nullptr;
    KWaylandServer::DataDeviceManagerInterface *m_dataDeviceManagerInterface = nullptr;
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::DataDeviceManager *m_dataDeviceManager = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    QThread *m_thread = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-datasource-0");

void TestDataSource::init()
{
    using namespace KWaylandServer;
    delete m_display;
    m_display = new KWaylandServer::Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

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
    QSignalSpy dataDeviceManagerSpy(&registry, &KWayland::Client::Registry::dataDeviceManagerAnnounced);
    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_dataDeviceManagerInterface = new DataDeviceManagerInterface(m_display, m_display);

    QVERIFY(dataDeviceManagerSpy.wait());
    m_dataDeviceManager =
        registry.createDataDeviceManager(dataDeviceManagerSpy.first().first().value<quint32>(), dataDeviceManagerSpy.first().last().value<quint32>(), this);
}

void TestDataSource::cleanup()
{
    if (m_dataDeviceManager) {
        delete m_dataDeviceManager;
        m_dataDeviceManager = nullptr;
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
    delete m_connection;
    m_connection = nullptr;

    delete m_display;
    m_display = nullptr;
}

void TestDataSource::testOffer()
{
    using namespace KWaylandServer;

    qRegisterMetaType<KWaylandServer::DataSourceInterface *>();
    QSignalSpy dataSourceCreatedSpy(m_dataDeviceManagerInterface, &KWaylandServer::DataDeviceManagerInterface::dataSourceCreated);

    std::unique_ptr<KWayland::Client::DataSource> dataSource(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());

    QVERIFY(dataSourceCreatedSpy.wait());
    QCOMPARE(dataSourceCreatedSpy.count(), 1);

    QPointer<DataSourceInterface> serverDataSource = dataSourceCreatedSpy.first().first().value<DataSourceInterface *>();
    QVERIFY(!serverDataSource.isNull());
    QCOMPARE(serverDataSource->mimeTypes().count(), 0);

    QSignalSpy offeredSpy(serverDataSource.data(), &KWaylandServer::AbstractDataSource::mimeTypeOffered);

    const QString plain = QStringLiteral("text/plain");
    QMimeDatabase db;
    dataSource->offer(db.mimeTypeForName(plain));

    QVERIFY(offeredSpy.wait());
    QCOMPARE(offeredSpy.count(), 1);
    QCOMPARE(offeredSpy.last().first().toString(), plain);
    QCOMPARE(serverDataSource->mimeTypes().count(), 1);
    QCOMPARE(serverDataSource->mimeTypes().first(), plain);

    const QString html = QStringLiteral("text/html");
    dataSource->offer(db.mimeTypeForName(html));

    QVERIFY(offeredSpy.wait());
    QCOMPARE(offeredSpy.count(), 2);
    QCOMPARE(offeredSpy.first().first().toString(), plain);
    QCOMPARE(offeredSpy.last().first().toString(), html);
    QCOMPARE(serverDataSource->mimeTypes().count(), 2);
    QCOMPARE(serverDataSource->mimeTypes().first(), plain);
    QCOMPARE(serverDataSource->mimeTypes().last(), html);

    // try destroying the client side, should trigger a destroy of server side
    dataSource.reset();
    QVERIFY(!serverDataSource.isNull());
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QVERIFY(serverDataSource.isNull());
}

void TestDataSource::testTargetAccepts_data()
{
    QTest::addColumn<QString>("mimeType");

    QTest::newRow("empty") << QString();
    QTest::newRow("text/plain") << QStringLiteral("text/plain");
}

void TestDataSource::testTargetAccepts()
{
    using namespace KWaylandServer;

    QSignalSpy dataSourceCreatedSpy(m_dataDeviceManagerInterface, &KWaylandServer::DataDeviceManagerInterface::dataSourceCreated);

    std::unique_ptr<KWayland::Client::DataSource> dataSource(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());

    QSignalSpy targetAcceptsSpy(dataSource.get(), &KWayland::Client::DataSource::targetAccepts);

    QVERIFY(dataSourceCreatedSpy.wait());
    QCOMPARE(dataSourceCreatedSpy.count(), 1);

    QFETCH(QString, mimeType);
    dataSourceCreatedSpy.first().first().value<DataSourceInterface *>()->accept(mimeType);

    QVERIFY(targetAcceptsSpy.wait());
    QCOMPARE(targetAcceptsSpy.count(), 1);
    QCOMPARE(targetAcceptsSpy.first().first().toString(), mimeType);
}

void TestDataSource::testRequestSend()
{
    using namespace KWaylandServer;

    QSignalSpy dataSourceCreatedSpy(m_dataDeviceManagerInterface, &KWaylandServer::DataDeviceManagerInterface::dataSourceCreated);

    std::unique_ptr<KWayland::Client::DataSource> dataSource(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());
    QSignalSpy sendRequestedSpy(dataSource.get(), &KWayland::Client::DataSource::sendDataRequested);

    const QString plain = QStringLiteral("text/plain");
    QVERIFY(dataSourceCreatedSpy.wait());
    QCOMPARE(dataSourceCreatedSpy.count(), 1);
    QTemporaryFile file;
    QVERIFY(file.open());
    dataSourceCreatedSpy.first().first().value<DataSourceInterface *>()->requestData(plain, file.handle());

    QVERIFY(sendRequestedSpy.wait());
    QCOMPARE(sendRequestedSpy.count(), 1);
    QCOMPARE(sendRequestedSpy.first().first().toString(), plain);
    QCOMPARE(sendRequestedSpy.first().last().value<qint32>(), file.handle());
    QVERIFY(sendRequestedSpy.first().last().value<qint32>() != -1);

    QFile writeFile;
    QVERIFY(writeFile.open(sendRequestedSpy.first().last().value<qint32>(), QFile::WriteOnly, QFileDevice::AutoCloseHandle));
    writeFile.close();
}

void TestDataSource::testCancel()
{
    using namespace KWaylandServer;

    QSignalSpy dataSourceCreatedSpy(m_dataDeviceManagerInterface, &KWaylandServer::DataDeviceManagerInterface::dataSourceCreated);

    std::unique_ptr<KWayland::Client::DataSource> dataSource(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());
    QSignalSpy cancelledSpy(dataSource.get(), &KWayland::Client::DataSource::cancelled);

    QVERIFY(dataSourceCreatedSpy.wait());

    QCOMPARE(cancelledSpy.count(), 0);
    dataSourceCreatedSpy.first().first().value<DataSourceInterface *>()->cancel();

    QVERIFY(cancelledSpy.wait());
    QCOMPARE(cancelledSpy.count(), 1);
}

void TestDataSource::testServerGet()
{
    using namespace KWaylandServer;

    QSignalSpy dataSourceCreatedSpy(m_dataDeviceManagerInterface, &KWaylandServer::DataDeviceManagerInterface::dataSourceCreated);

    std::unique_ptr<KWayland::Client::DataSource> dataSource(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());

    QVERIFY(!DataSourceInterface::get(nullptr));
    QVERIFY(dataSourceCreatedSpy.wait());
    auto d = dataSourceCreatedSpy.first().first().value<DataSourceInterface *>();

    QCOMPARE(DataSourceInterface::get(d->resource()), d);
    QVERIFY(!DataSourceInterface::get(nullptr));
}

QTEST_GUILESS_MAIN(TestDataSource)
#include "test_datasource.moc"
