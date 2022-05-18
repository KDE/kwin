/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "wayland/display.h"
#include "wayland/dpms_interface.h"
#include "wayland/output_interface.h"

#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/dpms.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/output.h"
#include "KWayland/Client/registry.h"

// Wayland
#include <wayland-client-protocol.h>

class TestWaylandOutput : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandOutput(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testRegistry();
    void testModeChange();
    void testScaleChange();

    void testSubPixel_data();
    void testSubPixel();

    void testTransform_data();
    void testTransform();

    void testDpms_data();
    void testDpms();

    void testDpmsRequestMode_data();
    void testDpmsRequestMode();

private:
    KWaylandServer::Display *m_display;
    KWaylandServer::OutputInterface *m_serverOutput;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-output-0");

TestWaylandOutput::TestWaylandOutput(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_serverOutput(nullptr)
    , m_connection(nullptr)
    , m_thread(nullptr)
{
}

void TestWaylandOutput::init()
{
    using namespace KWaylandServer;
    delete m_display;
    m_display = new KWaylandServer::Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    m_serverOutput = new OutputInterface(m_display, this);
    QCOMPARE(m_serverOutput->pixelSize(), QSize());
    QCOMPARE(m_serverOutput->refreshRate(), 60000);
    m_serverOutput->setMode(QSize(1024, 768));
    QCOMPARE(m_serverOutput->pixelSize(), QSize(1024, 768));
    QCOMPARE(m_serverOutput->refreshRate(), 60000);
    QCOMPARE(m_serverOutput->isDpmsSupported(), false);
    QCOMPARE(m_serverOutput->dpmsMode(), KWin::Output::DpmsMode::Off);

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

void TestWaylandOutput::cleanup()
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

    delete m_display;
    m_display = nullptr;

    // these are the children of the display
    m_serverOutput = nullptr;
}

void TestWaylandOutput::testRegistry()
{
    QSignalSpy globalPositionChangedSpy(m_serverOutput, &KWaylandServer::OutputInterface::globalPositionChanged);
    QVERIFY(globalPositionChangedSpy.isValid());
    QCOMPARE(m_serverOutput->globalPosition(), QPoint(0, 0));
    m_serverOutput->setGlobalPosition(QPoint(100, 50));
    QCOMPARE(m_serverOutput->globalPosition(), QPoint(100, 50));
    QCOMPARE(globalPositionChangedSpy.count(), 1);
    // changing again should not trigger signal
    m_serverOutput->setGlobalPosition(QPoint(100, 50));
    QCOMPARE(globalPositionChangedSpy.count(), 1);

    QSignalSpy physicalSizeChangedSpy(m_serverOutput, &KWaylandServer::OutputInterface::physicalSizeChanged);
    QVERIFY(physicalSizeChangedSpy.isValid());
    QCOMPARE(m_serverOutput->physicalSize(), QSize());
    m_serverOutput->setPhysicalSize(QSize(200, 100));
    QCOMPARE(m_serverOutput->physicalSize(), QSize(200, 100));
    QCOMPARE(physicalSizeChangedSpy.count(), 1);
    // changing again should not trigger signal
    m_serverOutput->setPhysicalSize(QSize(200, 100));
    QCOMPARE(physicalSizeChangedSpy.count(), 1);
    m_serverOutput->done();

    KWayland::Client::Registry registry;
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputAnnounced);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    KWayland::Client::Output output;
    QVERIFY(!output.isValid());
    QCOMPARE(output.geometry(), QRect());
    QCOMPARE(output.globalPosition(), QPoint());
    QCOMPARE(output.manufacturer(), QString());
    QCOMPARE(output.model(), QString());
    QCOMPARE(output.physicalSize(), QSize());
    QCOMPARE(output.pixelSize(), QSize());
    QCOMPARE(output.refreshRate(), 0);
    QCOMPARE(output.scale(), 1);
    QCOMPARE(output.subPixel(), KWayland::Client::Output::SubPixel::Unknown);
    QCOMPARE(output.transform(), KWayland::Client::Output::Transform::Normal);

    QSignalSpy outputChanged(&output, &KWayland::Client::Output::changed);
    QVERIFY(outputChanged.isValid());

    auto o = registry.bindOutput(announced.first().first().value<quint32>(), announced.first().last().value<quint32>());
    QVERIFY(!KWayland::Client::Output::get(o));
    output.setup(o);
    QCOMPARE(KWayland::Client::Output::get(o), &output);
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());

    QCOMPARE(output.geometry(), QRect(100, 50, 1024, 768));
    QCOMPARE(output.globalPosition(), QPoint(100, 50));
    QCOMPARE(output.manufacturer(), QStringLiteral("org.kde.kwin"));
    QCOMPARE(output.model(), QStringLiteral("none"));
    QCOMPARE(output.physicalSize(), QSize(200, 100));
    QCOMPARE(output.pixelSize(), QSize(1024, 768));
    QCOMPARE(output.refreshRate(), 60000);
    QCOMPARE(output.scale(), 1);
    // for xwayland output it's unknown
    QCOMPARE(output.subPixel(), KWayland::Client::Output::SubPixel::Unknown);
    // for xwayland transform is normal
    QCOMPARE(output.transform(), KWayland::Client::Output::Transform::Normal);
}

void TestWaylandOutput::testModeChange()
{
    using namespace KWayland::Client;
    KWayland::Client::Registry registry;
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    KWayland::Client::Output output;
    QSignalSpy outputChanged(&output, &KWayland::Client::Output::changed);
    QVERIFY(outputChanged.isValid());
    QSignalSpy modeAddedSpy(&output, &KWayland::Client::Output::modeAdded);
    QVERIFY(modeAddedSpy.isValid());
    output.setup(registry.bindOutput(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());
    QCOMPARE(modeAddedSpy.count(), 1);
    QCOMPARE(modeAddedSpy.at(0).first().value<Output::Mode>().size, QSize(1024, 768));
    QCOMPARE(modeAddedSpy.at(0).first().value<Output::Mode>().refreshRate, 60000);
    QCOMPARE(modeAddedSpy.at(0).first().value<Output::Mode>().flags, Output::Mode::Flags(Output::Mode::Flag::Current));
    QCOMPARE(modeAddedSpy.at(0).first().value<Output::Mode>().output, QPointer<Output>(&output));
    QCOMPARE(output.pixelSize(), QSize(1024, 768));
    QCOMPARE(output.refreshRate(), 60000);

    // change once more
    m_serverOutput->setMode(QSize(1280, 1024), 90000);
    QCOMPARE(m_serverOutput->refreshRate(), 90000);
    m_serverOutput->done();
    QVERIFY(outputChanged.wait());
    QCOMPARE(modeAddedSpy.count(), 2);
    QCOMPARE(modeAddedSpy.at(1).first().value<Output::Mode>().size, QSize(1280, 1024));
    QCOMPARE(modeAddedSpy.at(1).first().value<Output::Mode>().refreshRate, 90000);
    QCOMPARE(modeAddedSpy.at(1).first().value<Output::Mode>().flags, Output::Mode::Flags(Output::Mode::Flag::Current));
    QCOMPARE(modeAddedSpy.at(1).first().value<Output::Mode>().output, QPointer<Output>(&output));
    QCOMPARE(output.pixelSize(), QSize(1280, 1024));
    QCOMPARE(output.refreshRate(), 90000);
}

void TestWaylandOutput::testScaleChange()
{
    KWayland::Client::Registry registry;
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputAnnounced);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    KWayland::Client::Output output;
    QSignalSpy outputChanged(&output, &KWayland::Client::Output::changed);
    QVERIFY(outputChanged.isValid());
    output.setup(registry.bindOutput(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scale(), 1);

    // change the scale
    outputChanged.clear();
    QCOMPARE(m_serverOutput->scale(), 1);
    QSignalSpy serverScaleChanged(m_serverOutput, &KWaylandServer::OutputInterface::scaleChanged);
    QVERIFY(serverScaleChanged.isValid());
    m_serverOutput->setScale(2);
    QCOMPARE(m_serverOutput->scale(), 2);
    m_serverOutput->done();
    QCOMPARE(serverScaleChanged.count(), 1);
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scale(), 2);
    // changing to same value should not trigger
    m_serverOutput->setScale(2);
    QCOMPARE(serverScaleChanged.count(), 1);
    QVERIFY(!outputChanged.wait(100));

    // change once more
    outputChanged.clear();
    m_serverOutput->setScale(4);
    m_serverOutput->done();
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scale(), 4);
}

void TestWaylandOutput::testSubPixel_data()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QTest::addColumn<KWayland::Client::Output::SubPixel>("expected");
    QTest::addColumn<KWin::Output::SubPixel>("actual");

    QTest::newRow("none") << Output::SubPixel::None << KWin::Output::SubPixel::None;
    QTest::newRow("horizontal/rgb") << Output::SubPixel::HorizontalRGB << KWin::Output::SubPixel::Horizontal_RGB;
    QTest::newRow("horizontal/bgr") << Output::SubPixel::HorizontalBGR << KWin::Output::SubPixel::Horizontal_BGR;
    QTest::newRow("vertical/rgb") << Output::SubPixel::VerticalRGB << KWin::Output::SubPixel::Vertical_RGB;
    QTest::newRow("vertical/bgr") << Output::SubPixel::VerticalBGR << KWin::Output::SubPixel::Vertical_BGR;
}

void TestWaylandOutput::testSubPixel()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QFETCH(KWin::Output::SubPixel, actual);
    QCOMPARE(m_serverOutput->subPixel(), KWin::Output::SubPixel::Unknown);
    QSignalSpy serverSubPixelChangedSpy(m_serverOutput, &KWaylandServer::OutputInterface::subPixelChanged);
    QVERIFY(serverSubPixelChangedSpy.isValid());
    m_serverOutput->setSubPixel(actual);
    QCOMPARE(m_serverOutput->subPixel(), actual);
    QCOMPARE(serverSubPixelChangedSpy.count(), 1);
    // changing to same value should not trigger the signal
    m_serverOutput->setSubPixel(actual);
    QCOMPARE(serverSubPixelChangedSpy.count(), 1);

    KWayland::Client::Registry registry;
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputAnnounced);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    KWayland::Client::Output output;
    QSignalSpy outputChanged(&output, &KWayland::Client::Output::changed);
    QVERIFY(outputChanged.isValid());
    output.setup(registry.bindOutput(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    if (outputChanged.isEmpty()) {
        QVERIFY(outputChanged.wait());
    }

    QTEST(output.subPixel(), "expected");

    // change back to unknown
    outputChanged.clear();
    m_serverOutput->setSubPixel(KWin::Output::SubPixel::Unknown);
    QCOMPARE(m_serverOutput->subPixel(), KWin::Output::SubPixel::Unknown);
    m_serverOutput->done();
    QCOMPARE(serverSubPixelChangedSpy.count(), 2);
    if (outputChanged.isEmpty()) {
        QVERIFY(outputChanged.wait());
    }
    QCOMPARE(output.subPixel(), Output::SubPixel::Unknown);
}

void TestWaylandOutput::testTransform_data()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QTest::addColumn<KWayland::Client::Output::Transform>("expected");
    QTest::addColumn<KWin::Output::Transform>("actual");

    QTest::newRow("90") << Output::Transform::Rotated90 << KWin::Output::Transform::Rotated90;
    QTest::newRow("180") << Output::Transform::Rotated180 << KWin::Output::Transform::Rotated180;
    QTest::newRow("270") << Output::Transform::Rotated270 << KWin::Output::Transform::Rotated270;
    QTest::newRow("Flipped") << Output::Transform::Flipped << KWin::Output::Transform::Flipped;
    QTest::newRow("Flipped 90") << Output::Transform::Flipped90 << KWin::Output::Transform::Flipped90;
    QTest::newRow("Flipped 180") << Output::Transform::Flipped180 << KWin::Output::Transform::Flipped180;
    QTest::newRow("Flipped 280") << Output::Transform::Flipped270 << KWin::Output::Transform::Flipped270;
}

void TestWaylandOutput::testTransform()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QFETCH(KWin::Output::Transform, actual);
    QCOMPARE(m_serverOutput->transform(), KWin::Output::Transform::Normal);
    QSignalSpy serverTransformChangedSpy(m_serverOutput, &KWaylandServer::OutputInterface::transformChanged);
    QVERIFY(serverTransformChangedSpy.isValid());
    m_serverOutput->setTransform(actual);
    QCOMPARE(m_serverOutput->transform(), actual);
    QCOMPARE(serverTransformChangedSpy.count(), 1);
    // changing to same should not trigger signal
    m_serverOutput->setTransform(actual);
    QCOMPARE(serverTransformChangedSpy.count(), 1);

    KWayland::Client::Registry registry;
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputAnnounced);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    KWayland::Client::Output *output = registry.createOutput(announced.first().first().value<quint32>(), announced.first().last().value<quint32>(), &registry);
    QSignalSpy outputChanged(output, &KWayland::Client::Output::changed);
    QVERIFY(outputChanged.isValid());
    wl_display_flush(m_connection->display());
    if (outputChanged.isEmpty()) {
        QVERIFY(outputChanged.wait());
    }

    QTEST(output->transform(), "expected");

    // change back to normal
    outputChanged.clear();
    m_serverOutput->setTransform(KWin::Output::Transform::Normal);
    QCOMPARE(m_serverOutput->transform(), KWin::Output::Transform::Normal);
    m_serverOutput->done();
    QCOMPARE(serverTransformChangedSpy.count(), 2);
    if (outputChanged.isEmpty()) {
        QVERIFY(outputChanged.wait());
    }
    QCOMPARE(output->transform(), Output::Transform::Normal);
}

void TestWaylandOutput::testDpms_data()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;

    QTest::addColumn<KWayland::Client::Dpms::Mode>("client");
    QTest::addColumn<KWin::Output::DpmsMode>("server");

    QTest::newRow("Standby") << Dpms::Mode::Standby << KWin::Output::DpmsMode::Standby;
    QTest::newRow("Suspend") << Dpms::Mode::Suspend << KWin::Output::DpmsMode::Suspend;
    QTest::newRow("On") << Dpms::Mode::On << KWin::Output::DpmsMode::On;
}

void TestWaylandOutput::testDpms()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;

    DpmsManagerInterface iface(m_display);

    // set Dpms on the Output
    QSignalSpy serverDpmsSupportedChangedSpy(m_serverOutput, &OutputInterface::dpmsSupportedChanged);
    QVERIFY(serverDpmsSupportedChangedSpy.isValid());
    QCOMPARE(m_serverOutput->isDpmsSupported(), false);
    m_serverOutput->setDpmsSupported(true);
    QCOMPARE(serverDpmsSupportedChangedSpy.count(), 1);
    QCOMPARE(m_serverOutput->isDpmsSupported(), true);

    KWayland::Client::Registry registry;
    registry.setEventQueue(m_queue);
    QSignalSpy announced(&registry, &Registry::interfacesAnnounced);
    QVERIFY(announced.isValid());
    QSignalSpy dpmsAnnouncedSpy(&registry, &Registry::dpmsAnnounced);
    QVERIFY(dpmsAnnouncedSpy.isValid());
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    m_connection->flush();
    QVERIFY(announced.wait());
    QCOMPARE(dpmsAnnouncedSpy.count(), 1);

    Output *output =
        registry.createOutput(registry.interface(Registry::Interface::Output).name, registry.interface(Registry::Interface::Output).version, &registry);

    DpmsManager *dpmsManager =
        registry.createDpmsManager(dpmsAnnouncedSpy.first().first().value<quint32>(), dpmsAnnouncedSpy.first().last().value<quint32>(), &registry);
    QVERIFY(dpmsManager->isValid());

    Dpms *dpms = dpmsManager->getDpms(output, &registry);
    QSignalSpy clientDpmsSupportedChangedSpy(dpms, &Dpms::supportedChanged);
    QVERIFY(clientDpmsSupportedChangedSpy.isValid());
    QVERIFY(dpms->isValid());
    QCOMPARE(dpms->isSupported(), false);
    QCOMPARE(dpms->mode(), Dpms::Mode::On);
    m_connection->flush();
    QVERIFY(clientDpmsSupportedChangedSpy.wait());
    QCOMPARE(clientDpmsSupportedChangedSpy.count(), 1);
    QCOMPARE(dpms->isSupported(), true);

    // and let's change to suspend
    QSignalSpy serverDpmsModeChangedSpy(m_serverOutput, &KWaylandServer::OutputInterface::dpmsModeChanged);
    QVERIFY(serverDpmsModeChangedSpy.isValid());
    QSignalSpy clientDpmsModeChangedSpy(dpms, &Dpms::modeChanged);
    QVERIFY(clientDpmsModeChangedSpy.isValid());

    QCOMPARE(m_serverOutput->dpmsMode(), KWin::Output::DpmsMode::Off);
    QFETCH(KWin::Output::DpmsMode, server);
    m_serverOutput->setDpmsMode(server);
    QCOMPARE(m_serverOutput->dpmsMode(), server);
    QCOMPARE(serverDpmsModeChangedSpy.count(), 1);

    QVERIFY(clientDpmsModeChangedSpy.wait());
    QCOMPARE(clientDpmsModeChangedSpy.count(), 1);
    QTEST(dpms->mode(), "client");

    // test supported changed
    QSignalSpy supportedChangedSpy(dpms, &Dpms::supportedChanged);
    QVERIFY(supportedChangedSpy.isValid());
    m_serverOutput->setDpmsSupported(false);
    QVERIFY(supportedChangedSpy.wait());
    QCOMPARE(supportedChangedSpy.count(), 1);
    QVERIFY(!dpms->isSupported());
    m_serverOutput->setDpmsSupported(true);
    QVERIFY(supportedChangedSpy.wait());
    QCOMPARE(supportedChangedSpy.count(), 2);
    QVERIFY(dpms->isSupported());

    // and switch back to off
    m_serverOutput->setDpmsMode(KWin::Output::DpmsMode::Off);
    QVERIFY(clientDpmsModeChangedSpy.wait());
    QCOMPARE(clientDpmsModeChangedSpy.count(), 2);
    QCOMPARE(dpms->mode(), Dpms::Mode::Off);
}

void TestWaylandOutput::testDpmsRequestMode_data()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;

    QTest::addColumn<KWayland::Client::Dpms::Mode>("client");
    QTest::addColumn<KWin::Output::DpmsMode>("server");

    QTest::newRow("Standby") << Dpms::Mode::Standby << KWin::Output::DpmsMode::Standby;
    QTest::newRow("Suspend") << Dpms::Mode::Suspend << KWin::Output::DpmsMode::Suspend;
    QTest::newRow("Off") << Dpms::Mode::Off << KWin::Output::DpmsMode::Off;
    QTest::newRow("On") << Dpms::Mode::On << KWin::Output::DpmsMode::On;
}

void TestWaylandOutput::testDpmsRequestMode()
{
    // this test verifies that requesting a dpms change from client side emits the signal on server side
    using namespace KWayland::Client;
    using namespace KWaylandServer;

    // setup code
    DpmsManagerInterface iface(m_display);

    // set Dpms on the Output
    QSignalSpy serverDpmsSupportedChangedSpy(m_serverOutput, &OutputInterface::dpmsSupportedChanged);
    QVERIFY(serverDpmsSupportedChangedSpy.isValid());
    QCOMPARE(m_serverOutput->isDpmsSupported(), false);
    m_serverOutput->setDpmsSupported(true);
    QCOMPARE(serverDpmsSupportedChangedSpy.count(), 1);
    QCOMPARE(m_serverOutput->isDpmsSupported(), true);

    KWayland::Client::Registry registry;
    registry.setEventQueue(m_queue);
    QSignalSpy announced(&registry, &Registry::interfacesAnnounced);
    QVERIFY(announced.isValid());
    QSignalSpy dpmsAnnouncedSpy(&registry, &Registry::dpmsAnnounced);
    QVERIFY(dpmsAnnouncedSpy.isValid());
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    m_connection->flush();
    QVERIFY(announced.wait());
    QCOMPARE(dpmsAnnouncedSpy.count(), 1);

    Output *output =
        registry.createOutput(registry.interface(Registry::Interface::Output).name, registry.interface(Registry::Interface::Output).version, &registry);

    DpmsManager *dpmsManager =
        registry.createDpmsManager(dpmsAnnouncedSpy.first().first().value<quint32>(), dpmsAnnouncedSpy.first().last().value<quint32>(), &registry);
    QVERIFY(dpmsManager->isValid());

    Dpms *dpms = dpmsManager->getDpms(output, &registry);
    // and test request mode
    QSignalSpy modeRequestedSpy(m_serverOutput, &KWaylandServer::OutputInterface::dpmsModeRequested);
    QVERIFY(modeRequestedSpy.isValid());

    QFETCH(Dpms::Mode, client);
    dpms->requestMode(client);
    QVERIFY(modeRequestedSpy.wait());
    QTEST(modeRequestedSpy.last().first().value<KWin::Output::DpmsMode>(), "server");
}

QTEST_GUILESS_MAIN(TestWaylandOutput)
#include "test_wayland_output.moc"
