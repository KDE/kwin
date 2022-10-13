/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

// Qt
#include <QHash>
#include <QThread>
#include <QtTest>

// WaylandServer
#include "wayland/compositor_interface.h"
#include "wayland/datacontroldevice_v1_interface.h"
#include "wayland/datacontroldevicemanager_v1_interface.h"
#include "wayland/datacontrolsource_v1_interface.h"
#include "wayland/display.h"
#include "wayland/seat_interface.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/seat.h>

#include "qwayland-wlr-data-control-unstable-v1.h"

using namespace KWaylandServer;

// Faux-client API for tests

Q_DECLARE_OPAQUE_POINTER(::zwlr_data_control_offer_v1 *)
Q_DECLARE_METATYPE(::zwlr_data_control_offer_v1 *)

class DataControlDeviceManager : public QObject, public QtWayland::zwlr_data_control_manager_v1
{
    Q_OBJECT
};

class DataControlOffer : public QObject, public QtWayland::zwlr_data_control_offer_v1
{
    Q_OBJECT
public:
    ~DataControlOffer()
    {
        destroy();
    }
    QStringList receivedOffers()
    {
        return m_receivedOffers;
    }

protected:
    virtual void zwlr_data_control_offer_v1_offer(const QString &mime_type) override
    {
        m_receivedOffers << mime_type;
    }

private:
    QStringList m_receivedOffers;
};

class DataControlDevice : public QObject, public QtWayland::zwlr_data_control_device_v1
{
    Q_OBJECT
public:
    ~DataControlDevice()
    {
        destroy();
    }
Q_SIGNALS:
    void dataControlOffer(DataControlOffer *offer); // our event receives a new ID, so we make a new object
    void selection(struct ::zwlr_data_control_offer_v1 *id);
    void primary_selection(struct ::zwlr_data_control_offer_v1 *id);

protected:
    void zwlr_data_control_device_v1_data_offer(struct ::zwlr_data_control_offer_v1 *id) override
    {
        auto offer = new DataControlOffer;
        offer->init(id);
        Q_EMIT dataControlOffer(offer);
    }

    void zwlr_data_control_device_v1_selection(struct ::zwlr_data_control_offer_v1 *id) override
    {
        Q_EMIT selection(id);
    }

    void zwlr_data_control_device_v1_primary_selection(struct ::zwlr_data_control_offer_v1 *id) override
    {
        Q_EMIT primary_selection(id);
    }
};

class DataControlSource : public QObject, public QtWayland::zwlr_data_control_source_v1
{
    Q_OBJECT
public:
    ~DataControlSource()
    {
        destroy();
    }

public:
};

class TestDataSource : public AbstractDataSource
{
    Q_OBJECT
public:
    TestDataSource()
        : AbstractDataSource(nullptr)
    {
    }
    ~TestDataSource()
    {
        Q_EMIT aboutToBeDestroyed();
    }
    void requestData(const QString &mimeType, qint32 fd) override
    {
        Q_UNUSED(mimeType);
        Q_UNUSED(fd);
    };
    void cancel() override
    {
        Q_EMIT cancelled();
    };
    QStringList mimeTypes() const override
    {
        return {"text/test1", "text/test2"};
    }
Q_SIGNALS:
    void cancelled();
};

// The test itself

class DataControlInterfaceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void testCopyToControl();
    void testCopyToControlPrimarySelection();
    void testCopyFromControl();
    void testCopyFromControlPrimarySelection();
    void testKlipperCase();
    void testPrimarySelectionDisabled();

private:
    std::unique_ptr<KWayland::Client::ConnectionThread> m_connection;
    std::unique_ptr<KWayland::Client::EventQueue> m_queue;
    std::unique_ptr<KWayland::Client::Compositor> m_clientCompositor;
    std::unique_ptr<KWayland::Client::Seat> m_clientSeat;

    std::unique_ptr<QThread> m_thread;
    std::unique_ptr<KWaylandServer::Display> m_display;
    std::unique_ptr<SeatInterface> m_seat;
    std::unique_ptr<CompositorInterface> m_serverCompositor;

    std::unique_ptr<DataControlDeviceManagerV1Interface> m_dataControlDeviceManagerInterface;

    std::unique_ptr<DataControlDeviceManager> m_dataControlDeviceManager;

    QVector<SurfaceInterface *> m_surfaces;
};

static const QString s_socketName = QStringLiteral("kwin-wayland-datacontrol-test-0");

void DataControlInterfaceTest::init()
{
    m_display = std::make_unique<KWaylandServer::Display>();
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    m_seat = std::make_unique<SeatInterface>(m_display.get());
    m_serverCompositor = std::make_unique<CompositorInterface>(m_display.get());
    m_dataControlDeviceManagerInterface = std::make_unique<DataControlDeviceManagerV1Interface>(m_display.get());

    // setup connection
    m_connection = std::make_unique<KWayland::Client::ConnectionThread>();
    QSignalSpy connectedSpy(m_connection.get(), &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = std::make_unique<QThread>();
    m_connection->moveToThread(m_thread.get());
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());
    QVERIFY(!m_connection->connections().isEmpty());

    m_queue = std::make_unique<KWayland::Client::EventQueue>();
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection.get());
    QVERIFY(m_queue->isValid());

    KWayland::Client::Registry registry;
    connect(&registry, &KWayland::Client::Registry::interfaceAnnounced, this, [this, &registry](const QByteArray &interface, quint32 name, quint32 version) {
        if (interface == "zwlr_data_control_manager_v1") {
            m_dataControlDeviceManager = std::make_unique<DataControlDeviceManager>();
            m_dataControlDeviceManager->init(registry.registry(), name, version);
        }
    });
    connect(&registry, &KWayland::Client::Registry::seatAnnounced, this, [this, &registry](quint32 name, quint32 version) {
        m_clientSeat.reset(registry.createSeat(name, version));
    });
    registry.setEventQueue(m_queue.get());
    QSignalSpy compositorSpy(&registry, &KWayland::Client::Registry::compositorAnnounced);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());

    QVERIFY(compositorSpy.wait());
    m_clientCompositor.reset(registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>()));
    QVERIFY(m_clientCompositor->isValid());

    QVERIFY(m_dataControlDeviceManager);
}

void DataControlInterfaceTest::cleanup()
{
    m_dataControlDeviceManager.reset();
    m_clientSeat.reset();
    m_clientCompositor.reset();
    m_queue.reset();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        m_thread.reset();
    }
    m_connection.reset();
    m_display.reset();
    m_seat.reset();
    m_serverCompositor.reset();
}

void DataControlInterfaceTest::testCopyToControl()
{
    // we set a dummy data source on the seat using abstract client directly
    // then confirm we receive the offer despite not having a surface

    std::unique_ptr<DataControlDevice> dataControlDevice(new DataControlDevice);
    dataControlDevice->init(m_dataControlDeviceManager->get_data_device(*m_clientSeat));

    QSignalSpy newOfferSpy(dataControlDevice.get(), &DataControlDevice::dataControlOffer);
    QSignalSpy selectionSpy(dataControlDevice.get(), &DataControlDevice::selection);

    std::unique_ptr<TestDataSource> testSelection(new TestDataSource);
    m_seat->setSelection(testSelection.get());

    // selection will be sent after we've been sent a new offer object and the mimes have been sent to that object
    selectionSpy.wait();

    QCOMPARE(newOfferSpy.count(), 1);
    std::unique_ptr<DataControlOffer> offer(newOfferSpy.first().first().value<DataControlOffer *>());
    QCOMPARE(selectionSpy.first().first().value<struct ::zwlr_data_control_offer_v1 *>(), offer->object());

    QCOMPARE(offer->receivedOffers().count(), 2);
    QCOMPARE(offer->receivedOffers()[0], "text/test1");
    QCOMPARE(offer->receivedOffers()[1], "text/test2");
}

void DataControlInterfaceTest::testCopyToControlPrimarySelection()
{
    // we set a dummy data source on the seat using abstract client directly
    // then confirm we receive the offer despite not having a surface

    std::unique_ptr<DataControlDevice> dataControlDevice(new DataControlDevice);
    dataControlDevice->init(m_dataControlDeviceManager->get_data_device(*m_clientSeat));

    QSignalSpy newOfferSpy(dataControlDevice.get(), &DataControlDevice::dataControlOffer);
    QSignalSpy selectionSpy(dataControlDevice.get(), &DataControlDevice::primary_selection);

    std::unique_ptr<TestDataSource> testSelection(new TestDataSource);
    m_seat->setPrimarySelection(testSelection.get());

    // selection will be sent after we've been sent a new offer object and the mimes have been sent to that object
    selectionSpy.wait();

    QCOMPARE(newOfferSpy.count(), 1);
    std::unique_ptr<DataControlOffer> offer(newOfferSpy.first().first().value<DataControlOffer *>());
    QCOMPARE(selectionSpy.first().first().value<struct ::zwlr_data_control_offer_v1 *>(), offer->object());

    QCOMPARE(offer->receivedOffers().count(), 2);
    QCOMPARE(offer->receivedOffers()[0], "text/test1");
    QCOMPARE(offer->receivedOffers()[1], "text/test2");
}

void DataControlInterfaceTest::testPrimarySelectionDisabled()
{
    // we set a dummy data source on the seat using abstract client directly
    // then confirm we receive the offer despite not having a surface

    // disable primary selection
    m_seat->setPrimarySelectionEnabled(false);

    std::unique_ptr<DataControlDevice> dataControlDevice(new DataControlDevice);
    dataControlDevice->init(m_dataControlDeviceManager->get_data_device(*m_clientSeat));

    QSignalSpy newOfferSpy(dataControlDevice.get(), &DataControlDevice::dataControlOffer);
    QSignalSpy selectionSpy(dataControlDevice.get(), &DataControlDevice::primary_selection);

    std::unique_ptr<TestDataSource> testSelection(new TestDataSource);
    QSignalSpy cancelSpy(testSelection.get(), &TestDataSource::cancelled);

    m_seat->setPrimarySelection(testSelection.get());

    // selection will be sent after we've been sent a new offer object and the mimes have been sent to that object
    cancelSpy.wait();

    QCOMPARE(newOfferSpy.count(), 0);
    QCOMPARE(cancelSpy.count(), 1);
}

void DataControlInterfaceTest::testCopyFromControl()
{
    // we create a data device and set a selection
    // then confirm the server sees the new selection
    QSignalSpy serverSelectionChangedSpy(m_seat.get(), &SeatInterface::selectionChanged);

    std::unique_ptr<DataControlDevice> dataControlDevice(new DataControlDevice);
    dataControlDevice->init(m_dataControlDeviceManager->get_data_device(*m_clientSeat));

    std::unique_ptr<DataControlSource> source(new DataControlSource);
    source->init(m_dataControlDeviceManager->create_data_source());
    source->offer("cheese/test1");
    source->offer("cheese/test2");

    dataControlDevice->set_selection(source->object());

    serverSelectionChangedSpy.wait();
    QVERIFY(m_seat->selection());
    QCOMPARE(m_seat->selection()->mimeTypes(), QStringList({"cheese/test1", "cheese/test2"}));
}

void DataControlInterfaceTest::testCopyFromControlPrimarySelection()
{
    // we create a data device and set a selection
    // then confirm the server sees the new selection
    QSignalSpy serverSelectionChangedSpy(m_seat.get(), &SeatInterface::primarySelectionChanged);

    std::unique_ptr<DataControlDevice> dataControlDevice(new DataControlDevice);
    dataControlDevice->init(m_dataControlDeviceManager->get_data_device(*m_clientSeat));

    std::unique_ptr<DataControlSource> source(new DataControlSource);
    source->init(m_dataControlDeviceManager->create_data_source());
    source->offer("cheese/test1");
    source->offer("cheese/test2");

    dataControlDevice->set_primary_selection(source->object());

    serverSelectionChangedSpy.wait();
    QVERIFY(m_seat->primarySelection());
    QCOMPARE(m_seat->primarySelection()->mimeTypes(), QStringList({"cheese/test1", "cheese/test2"}));
}

void DataControlInterfaceTest::testKlipperCase()
{
    // This tests the setup of klipper's real world operation and a race with a common pattern seen between clients and klipper
    // The client's behaviour is faked with direct access to the seat

    std::unique_ptr<DataControlDevice> dataControlDevice(new DataControlDevice);
    dataControlDevice->init(m_dataControlDeviceManager->get_data_device(*m_clientSeat));

    QSignalSpy newOfferSpy(dataControlDevice.get(), &DataControlDevice::dataControlOffer);
    QSignalSpy selectionSpy(dataControlDevice.get(), &DataControlDevice::selection);
    QSignalSpy serverSelectionChangedSpy(m_seat.get(), &SeatInterface::selectionChanged);

    // Client A has a data source
    std::unique_ptr<TestDataSource> testSelection(new TestDataSource);
    m_seat->setSelection(testSelection.get());

    // klipper gets it
    selectionSpy.wait();

    // Client A deletes it
    testSelection.reset();

    // klipper gets told
    selectionSpy.wait();

    // Client A sets something else
    std::unique_ptr<TestDataSource> testSelection2(new TestDataSource);
    m_seat->setSelection(testSelection2.get());

    // Meanwhile klipper updates with the old content
    std::unique_ptr<DataControlSource> source(new DataControlSource);
    source->init(m_dataControlDeviceManager->create_data_source());
    source->offer("fromKlipper/test1");
    source->offer("application/x-kde-onlyReplaceEmpty");

    dataControlDevice->set_selection(source->object());

    QVERIFY(!serverSelectionChangedSpy.wait(10));
    QCOMPARE(m_seat->selection(), testSelection2.get());
}

QTEST_GUILESS_MAIN(DataControlInterfaceTest)

#include "test_datacontrol_interface.moc"
