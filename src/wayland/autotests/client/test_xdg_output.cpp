/*
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "wayland/display.h"
#include "wayland/output_interface.h"
#include "wayland/xdgoutput_v1_interface.h"

#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/output.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/xdgoutput.h"

#include "../../tests/fakeoutput.h"

class TestXdgOutput : public QObject
{
    Q_OBJECT
public:
    explicit TestXdgOutput(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();
    void testChanges();

private:
    KWaylandServer::Display *m_display;
    std::unique_ptr<FakeOutput> m_outputHandle;
    KWaylandServer::OutputInterface *m_serverOutput;
    KWaylandServer::XdgOutputManagerV1Interface *m_serverXdgOutputManager;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwin-test-xdg-output-0");

TestXdgOutput::TestXdgOutput(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_serverOutput(nullptr)
    , m_connection(nullptr)
    , m_thread(nullptr)
{
}

void TestXdgOutput::init()
{
    using namespace KWaylandServer;
    delete m_display;
    m_display = new KWaylandServer::Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    m_outputHandle = std::make_unique<FakeOutput>();
    m_outputHandle->setMode(QSize(1920, 1080), 60000);
    m_outputHandle->moveTo(QPoint(11, 12)); // not a sensible value for one monitor, but works for this test
    m_outputHandle->setScale(1.5);
    m_outputHandle->setName("testName");
    m_outputHandle->setManufacturer("foo");
    m_outputHandle->setModel("bar");

    m_serverOutput = new OutputInterface(m_display, m_outputHandle.get(), this);

    m_serverXdgOutputManager = new XdgOutputManagerV1Interface(m_display, this);
    m_serverXdgOutputManager->offer(m_serverOutput);

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
}

void TestXdgOutput::cleanup()
{
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

    delete m_serverOutput;
    m_serverOutput = nullptr;
    m_outputHandle.reset();

    delete m_display;
    m_display = nullptr;
}

void TestXdgOutput::testChanges()
{
    // verify the server modes
    using namespace KWaylandServer;
    KWayland::Client::Registry registry;
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputAnnounced);
    QSignalSpy xdgOutputAnnounced(&registry, &KWayland::Client::Registry::xdgOutputAnnounced);

    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(announced.wait());
    if (xdgOutputAnnounced.count() != 1) {
        QVERIFY(xdgOutputAnnounced.wait());
    }

    KWayland::Client::Output output;
    QSignalSpy outputChanged(&output, &KWayland::Client::Output::changed);

    output.setup(registry.bindOutput(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    QVERIFY(outputChanged.wait());

    std::unique_ptr<KWayland::Client::XdgOutputManager> xdgOutputManager(
        registry.createXdgOutputManager(xdgOutputAnnounced.first().first().value<quint32>(), xdgOutputAnnounced.first().last().value<quint32>(), this));

    std::unique_ptr<KWayland::Client::XdgOutput> xdgOutput(xdgOutputManager->getXdgOutput(&output, this));
    QSignalSpy xdgOutputChanged(xdgOutput.get(), &KWayland::Client::XdgOutput::changed);

    // check details are sent on client bind
    QVERIFY(xdgOutputChanged.wait());
    xdgOutputChanged.clear();
    QCOMPARE(xdgOutput->logicalPosition(), QPoint(11, 12));
    QCOMPARE(xdgOutput->logicalSize(), QSize(1280, 720));
    QCOMPARE(xdgOutput->name(), "testName");
    QCOMPARE(xdgOutput->description(), "foo bar");

    // change the logical position
    m_outputHandle->moveTo(QPoint(1000, 2000));
    QVERIFY(xdgOutputChanged.wait());
    QCOMPARE(xdgOutputChanged.count(), 1);
    QCOMPARE(xdgOutput->logicalPosition(), QPoint(1000, 2000));
    QEXPECT_FAIL("", "KWayland::Client::XdgOutput incorrectly handles partial updates", Continue);
    QCOMPARE(xdgOutput->logicalSize(), QSize(1280, 720));

    // change the logical size
    m_outputHandle->setScale(2);
    QVERIFY(xdgOutputChanged.wait());
    QCOMPARE(xdgOutputChanged.count(), 2);
    QEXPECT_FAIL("", "KWayland::Client::XdgOutput incorrectly handles partial updates", Continue);
    QCOMPARE(xdgOutput->logicalPosition(), QPoint(1000, 2000));
    QCOMPARE(xdgOutput->logicalSize(), QSize(960, 540));
}

QTEST_GUILESS_MAIN(TestXdgOutput)
#include "test_xdg_output.moc"
