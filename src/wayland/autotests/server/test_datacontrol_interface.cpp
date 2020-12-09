/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

// Qt
#include <QHash>
#include <QThread>
#include <QtTest>

// WaylandServer
#include "../../src/server/compositor_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/seat_interface.h"
#include "../../src/server/datacontroldevice_v1_interface.h"
#include "../../src/server/datacontroldevicemanager_v1_interface.h"
#include "../../src/server/datacontrolsource_v1_interface.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/seat.h>

#include "qwayland-wlr-data-control-unstable-v1.h"

using namespace KWaylandServer;

// Faux-client API for tests

Q_DECLARE_OPAQUE_POINTER(::zwlr_data_control_offer_v1*)
Q_DECLARE_METATYPE(::zwlr_data_control_offer_v1*)

class DataControlDeviceManager : public QObject, public QtWayland::zwlr_data_control_manager_v1
{
    Q_OBJECT
};

class DataControlOffer: public QObject, public QtWayland::zwlr_data_control_offer_v1
{
    Q_OBJECT
public:
    ~DataControlOffer() {
        destroy();
    }
    QStringList receivedOffers() {
        return m_receivedOffers;
    }
protected:
    virtual void zwlr_data_control_offer_v1_offer(const QString &mime_type) override {
        m_receivedOffers << mime_type;
    }
private:
    QStringList m_receivedOffers;
};

class DataControlDevice : public QObject, public QtWayland::zwlr_data_control_device_v1
{
    Q_OBJECT
public:
    ~DataControlDevice() {
        destroy();
    }
Q_SIGNALS:
    void dataControlOffer(DataControlOffer *offer); //our event receives a new ID, so we make a new object
    void selection(struct ::zwlr_data_control_offer_v1 *id);
protected:
    void zwlr_data_control_device_v1_data_offer(struct ::zwlr_data_control_offer_v1 *id) override {
        auto offer = new DataControlOffer;
        offer->init(id);
        Q_EMIT dataControlOffer(offer);
    }

    void zwlr_data_control_device_v1_selection(struct ::zwlr_data_control_offer_v1 *id) override {
        Q_EMIT selection(id);
    }
};

class DataControlSource: public QObject, public QtWayland::zwlr_data_control_source_v1
{
    Q_OBJECT
public:
    ~DataControlSource() {
        destroy();
    }
public:
};


class TestDataSource : public AbstractDataSource
{
    Q_OBJECT
public:
    TestDataSource() :
        AbstractDataSource(nullptr)
    {}
    ~TestDataSource() {
        emit aboutToBeDestroyed();
    }
    void requestData(const QString &mimeType, qint32 fd) override {
        Q_UNUSED(mimeType);
        Q_UNUSED(fd);
    };
    void cancel() override {};
    QStringList mimeTypes() const override {
        return {"text/test1", "text/test2"};
    }
};


// The test itself

class DataControlInterfaceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void testCopyToControl();
    void testCopyFromControl();
    void testKlipperCase();

private:
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue;
    KWayland::Client::Compositor *m_clientCompositor;
    KWayland::Client::Seat *m_clientSeat = nullptr;

    QThread *m_thread;
    Display *m_display;
    SeatInterface *m_seat;
    CompositorInterface *m_serverCompositor;

    DataControlDeviceManagerV1Interface *m_dataControlDeviceManagerInterface;

    DataControlDeviceManager *m_dataControlDeviceManager;

    QVector<SurfaceInterface *> m_surfaces;
};

static const QString s_socketName = QStringLiteral("kwin-wayland-datacontrol-test-0");

void DataControlInterfaceTest::init()
{
    m_display = new Display();
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    m_seat = new SeatInterface(m_display, this);
    m_seat->create();
    m_serverCompositor = new CompositorInterface(m_display, this);
    m_dataControlDeviceManagerInterface = new DataControlDeviceManagerV1Interface(m_display, this);

    // setup connection
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

    KWayland::Client::Registry registry;
    connect(&registry, &KWayland::Client::Registry::interfaceAnnounced, this, [this, &registry](const QByteArray &interface, quint32 name, quint32 version) {
        if (interface == "zwlr_data_control_manager_v1") {
            m_dataControlDeviceManager = new DataControlDeviceManager;
            m_dataControlDeviceManager->init(registry.registry(), name, version);
        }
    });
    connect(&registry, &KWayland::Client::Registry::seatAnnounced, this, [this, &registry](quint32 name, quint32 version) {
        m_clientSeat = registry.createSeat(name, version);
    });
    registry.setEventQueue(m_queue);
    QSignalSpy compositorSpy(&registry, &KWayland::Client::Registry::compositorAnnounced);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());

    QVERIFY(compositorSpy.wait());
    m_clientCompositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);
    QVERIFY(m_clientCompositor->isValid());

    QVERIFY(m_dataControlDeviceManager);
}

void DataControlInterfaceTest::cleanup()
{
#define CLEANUP(variable) \
    if (variable) { \
        delete variable; \
        variable = nullptr; \
    }
    CLEANUP(m_dataControlDeviceManager)
    CLEANUP(m_clientSeat)
    CLEANUP(m_clientCompositor)
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
    CLEANUP(m_display)
#undef CLEANUP

    // these are the children of the display
    m_seat = nullptr;
    m_serverCompositor = nullptr;
}

void DataControlInterfaceTest::testCopyToControl()
{
    // we set a dummy data source on the seat using abstract client directly
    // then confirm we receive the offer despite not having a surface

    QScopedPointer<DataControlDevice> dataControlDevice(new DataControlDevice);
    dataControlDevice->init(m_dataControlDeviceManager->get_data_device(*m_clientSeat));

    QSignalSpy newOfferSpy(dataControlDevice.data(), &DataControlDevice::dataControlOffer);
    QSignalSpy selectionSpy(dataControlDevice.data(), &DataControlDevice::selection);

    QScopedPointer<TestDataSource> testSelection(new TestDataSource);
    m_seat->setSelection(testSelection.data());

    // selection will be sent after we've been sent a new offer object and the mimes have been sent to that object
    selectionSpy.wait();

    QCOMPARE(newOfferSpy.count(), 1);
    QScopedPointer<DataControlOffer> offer(newOfferSpy.first().first().value<DataControlOffer*>());
    QCOMPARE(selectionSpy.first().first().value<struct ::zwlr_data_control_offer_v1*>(), offer->object());

    QCOMPARE(offer->receivedOffers().count(), 2);
    QCOMPARE(offer->receivedOffers()[0], "text/test1");
    QCOMPARE(offer->receivedOffers()[1], "text/test2");
}

void DataControlInterfaceTest::testCopyFromControl()
{
    // we create a data device and set a selection
    // then confirm the server sees the new selection
    QSignalSpy serverSelectionChangedSpy(m_seat, &SeatInterface::selectionChanged);

    QScopedPointer<DataControlDevice> dataControlDevice(new DataControlDevice);
    dataControlDevice->init(m_dataControlDeviceManager->get_data_device(*m_clientSeat));

    QScopedPointer<DataControlSource> source(new DataControlSource);
    source->init(m_dataControlDeviceManager->create_data_source());
    source->offer("cheese/test1");
    source->offer("cheese/test2");

    dataControlDevice->set_selection(source->object());

    serverSelectionChangedSpy.wait();
    QVERIFY(m_seat->selection());
    QCOMPARE(m_seat->selection()->mimeTypes(), QStringList({"cheese/test1", "cheese/test2"}));
}

void DataControlInterfaceTest::testKlipperCase()
{
    // This tests the setup of klipper's real world operation and a race with a common pattern seen between clients and klipper
    // The client's behaviour is faked with direct access to the seat

    QScopedPointer<DataControlDevice> dataControlDevice(new DataControlDevice);
    dataControlDevice->init(m_dataControlDeviceManager->get_data_device(*m_clientSeat));

    QSignalSpy newOfferSpy(dataControlDevice.data(), &DataControlDevice::dataControlOffer);
    QSignalSpy selectionSpy(dataControlDevice.data(), &DataControlDevice::selection);
    QSignalSpy serverSelectionChangedSpy(m_seat, &SeatInterface::selectionChanged);

    // Client A has a data source
    QScopedPointer<TestDataSource> testSelection(new TestDataSource);
    m_seat->setSelection(testSelection.data());

    // klipper gets it
    selectionSpy.wait();

    // Client A deletes it
    testSelection.reset();

    //klipper gets told
    selectionSpy.wait();

    // Client A sets something else
    QScopedPointer<TestDataSource> testSelection2(new TestDataSource);
    m_seat->setSelection(testSelection2.data());

    // Meanwhile klipper updates with the old content
    QScopedPointer<DataControlSource> source(new DataControlSource);
    source->init(m_dataControlDeviceManager->create_data_source());
    source->offer("fromKlipper/test1");
    source->offer("application/x-kde-onlyReplaceEmpty");

    dataControlDevice->set_selection(source->object());

    QVERIFY(!serverSelectionChangedSpy.wait(10));
    QCOMPARE(m_seat->selection(), testSelection2.data());
}


QTEST_GUILESS_MAIN(DataControlInterfaceTest)

#include "test_datacontrol_interface.moc"
